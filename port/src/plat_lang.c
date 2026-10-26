/*
 * The port's own text in the player's language (T47, docs/port/translations.md; plat_lang.h).
 *
 * Catalogs: flat JSON objects of key -> text, UTF-8, one file a language (port/lang/es.json...). Keys starting with
 * "@" are about the catalog itself ("@name": the language's name in that language, for the picker). The English text
 * is in the code (Tr's second argument), so English needs no file and any missing key falls back to it.
 *
 * Where the catalogs are looked for, the first folder holding <code>.json wins: TTT_LANG_DIR; lang/ in the player's
 * data folder (plat_paths.c: user translations); lang/ beside the program (a release); port/lang/ of the repository
 * when the program is build/port/ttt of a checkout.
 *
 * The language: ui.language in the settings (TTT_LANG wins over it); "" or "auto" asks the environment (LANGUAGE,
 * LC_ALL, LC_MESSAGES, LANG: "es_AR.UTF-8" tries es_AR, then es); "C" and "POSIX" are English.
 *
 * A catalog is read once and kept until the program ends (the overlay switches between loaded ones; texts handed out
 * stay valid), and the language in use is one pointer, so the window thread, the setup's thread and the screen reader
 * may look texts up while the picker switches. Nothing here is game state. The host stack only.
 */
#include <dirent.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_host.h"
#include "plat_lang.h"
#include "plat_settings.h"

/* C library calls through the GOT: no new PLT entry (port_host.h) */
#define PORT_NOPLT(f) __typeof__(f) f __attribute__((noplt))
PORT_NOPLT(opendir);
PORT_NOPLT(readdir);
PORT_NOPLT(closedir);
PORT_NOPLT(strchr);
PORT_NOPLT(calloc);
PORT_NOPLT(realloc);
PORT_NOPLT(fread);
#undef PORT_NOPLT

const char *Port_ExeDir(void); /* plat_paths.c */
int Port_Checkout(void);
const char *Port_DataDir(void);

#define MAX_FILE (1 << 20) /* a catalog larger than 1 MB is refused */

typedef struct Catalog {
    char code[16];
    char *arena;          /* every key and text, NUL-terminated */
    const char **keys, **texts;
    uint32_t mask;        /* the hash table: mask + 1 slots, -1 empty */
    int32_t *slots;
    int n;
    struct Catalog *next; /* every catalog read */
} Catalog;

static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static Catalog *sLoaded;               /* read once, never freed */
static Catalog *volatile sCur;          /* NULL: English */
static char sCurCode[16] = "en", sDetected[16] = "en";
static char sLastError[300];

static uint32_t fnv(const char *s) {
    uint32_t h = 2166136261u;
    while (*s) {
        h = (h ^ (uint8_t)*s++) * 16777619u;
    }
    return h;
}

/* ---- a small JSON reader: one object of string values ---- */
typedef struct {
    const char *p, *end;
    char *out; /* the arena's next byte */
    int err;
} Json;

static void skip_ws(Json *j) {
    while (j->p < j->end && (*j->p == ' ' || *j->p == '\t' || *j->p == '\n' || *j->p == '\r')) {
        j->p++;
    }
}

static int hexval(char c) {
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

static int read_u16(Json *j, uint32_t *v) {
    int i, d;
    *v = 0;
    if (j->end - j->p < 4) {
        return 0;
    }
    for (i = 0; i < 4; i++) {
        if ((d = hexval(j->p[i])) < 0) {
            return 0;
        }
        *v = *v << 4 | (uint32_t)d;
    }
    j->p += 4;
    return 1;
}

static void put_utf8(Json *j, uint32_t c) {
    if (c < 0x80) {
        *j->out++ = (char)c;
    } else if (c < 0x800) {
        *j->out++ = (char)(0xC0 | c >> 6);
        *j->out++ = (char)(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
        *j->out++ = (char)(0xE0 | c >> 12);
        *j->out++ = (char)(0x80 | (c >> 6 & 0x3F));
        *j->out++ = (char)(0x80 | (c & 0x3F));
    } else {
        *j->out++ = (char)(0xF0 | c >> 18);
        *j->out++ = (char)(0x80 | (c >> 12 & 0x3F));
        *j->out++ = (char)(0x80 | (c >> 6 & 0x3F));
        *j->out++ = (char)(0x80 | (c & 0x3F));
    }
}

/* A string (the opening quote at p) into the arena; its start, NULL on an error. The arena is as large as the file
   plus one byte a string, and an escape never writes more bytes than it reads. */
static const char *read_string(Json *j) {
    char *start = j->out;
    if (j->p >= j->end || *j->p != '"') {
        return NULL;
    }
    j->p++;
    while (j->p < j->end && *j->p != '"') {
        uint8_t c = (uint8_t)*j->p++;
        if (c < 0x20) {
            return NULL; /* control characters must be escaped */
        }
        if (c != '\\') {
            *j->out++ = (char)c;
            continue;
        }
        if (j->p >= j->end) {
            return NULL;
        }
        c = (uint8_t)*j->p++;
        switch (c) {
        case '"': case '\\': case '/': *j->out++ = (char)c; break;
        case 'b': *j->out++ = '\b'; break;
        case 'f': *j->out++ = '\f'; break;
        case 'n': *j->out++ = '\n'; break;
        case 'r': *j->out++ = '\r'; break;
        case 't': *j->out++ = '\t'; break;
        case 'u': {
            uint32_t u, lo;
            if (!read_u16(j, &u) || u == 0) {
                return NULL;
            }
            if (u >= 0xD800 && u < 0xDC00) { /* a surrogate pair */
                if (j->end - j->p < 6 || j->p[0] != '\\' || j->p[1] != 'u') {
                    return NULL;
                }
                j->p += 2;
                if (!read_u16(j, &lo) || lo < 0xDC00 || lo >= 0xE000) {
                    return NULL;
                }
                u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00);
            } else if (u >= 0xDC00 && u < 0xE000) {
                return NULL;
            }
            put_utf8(j, u);
            break;
        }
        default:
            return NULL;
        }
    }
    if (j->p >= j->end) {
        return NULL;
    }
    j->p++;
    *j->out++ = 0;
    return start;
}

static void insert(Catalog *c, int i) {
    uint32_t s = fnv(c->keys[i]) & c->mask;
    while (c->slots[s] >= 0) {
        if (strcmp(c->keys[c->slots[s]], c->keys[i]) == 0) {
            c->slots[s] = i; /* a repeated key: the last one wins, as in most JSON readers */
            return;
        }
        s = (s + 1) & c->mask;
    }
    c->slots[s] = i;
}

/* Parses `text` (n bytes) into a catalog: NULL with the reason in err on a format error. */
static Catalog *parse(const char *text, long n, const char *code, char *err, int errn) {
    Json j;
    Catalog *c = calloc(1, sizeof *c);
    int cap = 0, i;
    if (c == NULL || (c->arena = malloc((size_t)n + 1)) == NULL) {
        free(c);
        snprintf(err, (size_t)errn, "out of memory");
        return NULL;
    }
    snprintf(c->code, sizeof c->code, "%s", code);
    j.p = text;
    j.end = text + n;
    j.out = c->arena;
    j.err = 0;
    if (n >= 3 && (uint8_t)text[0] == 0xEF && (uint8_t)text[1] == 0xBB && (uint8_t)text[2] == 0xBF) {
        j.p += 3; /* a UTF-8 byte order mark */
    }
    skip_ws(&j);
    if (j.p >= j.end || *j.p++ != '{') {
        snprintf(err, (size_t)errn, "not a JSON object");
        goto fail;
    }
    skip_ws(&j);
    if (j.p < j.end && *j.p == '}') {
        j.p++;
    } else {
        for (;;) {
            const char *k, *v;
            skip_ws(&j);
            if ((k = read_string(&j)) == NULL) {
                snprintf(err, (size_t)errn, "a key is not a valid string (byte %ld)", (long)(j.p - text));
                goto fail;
            }
            skip_ws(&j);
            if (j.p >= j.end || *j.p++ != ':') {
                snprintf(err, (size_t)errn, "no ':' after \"%s\"", k);
                goto fail;
            }
            skip_ws(&j);
            if ((v = read_string(&j)) == NULL) {
                snprintf(err, (size_t)errn, "the value of \"%s\" is not a valid string", k);
                goto fail;
            }
            if (c->n == cap) {
                const char **nk, **nt;
                cap = cap ? cap * 2 : 256;
                nk = realloc(c->keys, sizeof *nk * (size_t)cap);
                if (nk != NULL) {
                    c->keys = nk;
                }
                nt = realloc(c->texts, sizeof *nt * (size_t)cap);
                if (nt != NULL) {
                    c->texts = nt;
                }
                if (nk == NULL || nt == NULL) {
                    snprintf(err, (size_t)errn, "out of memory");
                    goto fail;
                }
            }
            c->keys[c->n] = k;
            c->texts[c->n] = v;
            c->n++;
            skip_ws(&j);
            if (j.p < j.end && *j.p == ',') {
                j.p++;
                continue;
            }
            if (j.p < j.end && *j.p == '}') {
                j.p++;
                break;
            }
            snprintf(err, (size_t)errn, "',' or '}' expected after \"%s\"", k);
            goto fail;
        }
    }
    skip_ws(&j);
    if (j.p != j.end) {
        snprintf(err, (size_t)errn, "text after the object");
        goto fail;
    }
    for (c->mask = 15; c->mask + 1 < (uint32_t)c->n * 2; c->mask = c->mask * 2 + 1) {
    }
    if ((c->slots = malloc(sizeof *c->slots * (c->mask + 1))) == NULL) {
        snprintf(err, (size_t)errn, "out of memory");
        goto fail;
    }
    for (i = 0; i <= (int)c->mask; i++) {
        c->slots[i] = -1;
    }
    for (i = 0; i < c->n; i++) {
        insert(c, i);
    }
    return c;
fail:
    free(c->keys);
    free(c->texts);
    free(c->arena);
    free(c);
    return NULL;
}

static const char *lookup(const Catalog *c, const char *key) {
    uint32_t s;
    if (c == NULL || key == NULL) {
        return NULL;
    }
    for (s = fnv(key) & c->mask; c->slots[s] >= 0; s = (s + 1) & c->mask) {
        if (strcmp(c->keys[c->slots[s]], key) == 0) {
            return c->texts[c->slots[s]];
        }
    }
    return NULL;
}

/* ---- the files ---- */
/* A code is a file name part: letters, digits, '_' and '-' only (no paths). */
static int code_ok(const char *code) {
    int n = 0;
    for (; code[n]; n++) {
        char c = code[n];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) {
            return 0;
        }
    }
    return n > 0 && n < 16;
}

/* The catalog folders in order (see the top): how many were written to dirs. */
static int lang_dirs(char dirs[4][1100]) {
    const char *e = getenv("TTT_LANG_DIR"), *x = Port_ExeDir(), *d = Port_DataDir();
    int n = 0;
    if (e != NULL && *e) {
        snprintf(dirs[n++], 1100, "%s", e);
    }
    if (d != NULL && *d) {
        snprintf(dirs[n++], 1100, "%s/lang", d);
    }
    if (x != NULL && *x) {
        snprintf(dirs[n++], 1100, "%s/lang", x);
        if (Port_Checkout()) {
            snprintf(dirs[n++], 1100, "%s/../../port/lang", x);
        }
    }
    return n;
}

static char *read_file(const char *path, long *n) {
    FILE *f = fopen(path, "rb");
    char *buf;
    if (f == NULL) {
        return NULL;
    }
    if ((buf = malloc(MAX_FILE + 1)) == NULL) {
        fclose(f);
        return NULL;
    }
    *n = (long)fread(buf, 1, MAX_FILE + 1, f);
    fclose(f);
    if (*n > MAX_FILE) {
        free(buf);
        *n = -1;
        return NULL;
    }
    return buf;
}

/* The catalog of `code`: one read before, else read now. NULL when there is none (or it is broken: said once on
   stderr). Lock held. */
static Catalog *load(const char *code) {
    char dirs[4][1100], path[1200], err[256];
    Catalog *c;
    int i, nd;
    for (c = sLoaded; c != NULL; c = c->next) {
        if (strcmp(c->code, code) == 0) {
            return c;
        }
    }
    if (!code_ok(code)) {
        return NULL;
    }
    nd = lang_dirs(dirs);
    for (i = 0; i < nd; i++) {
        long n = 0;
        char *text;
        snprintf(path, sizeof path, "%s/%s.json", dirs[i], code);
        if ((text = read_file(path, &n)) == NULL) {
            if (n < 0) {
                snprintf(sLastError, sizeof sLastError, "%s: larger than 1 MB", path);
                fprintf(stderr, "ttt: lang: %s\n", sLastError);
            }
            continue;
        }
        c = parse(text, n, code, err, sizeof err);
        free(text);
        if (c == NULL) {
            snprintf(sLastError, sizeof sLastError, "%s: %s", path, err);
            fprintf(stderr, "ttt: lang: %s: not used\n", sLastError);
            return NULL;
        }
        c->next = sLoaded;
        sLoaded = c;
        return c;
    }
    return NULL;
}

/* "es_AR.UTF-8@euro" -> "es_AR"; the language part in lower case. "" for C / POSIX / nothing usable. */
static void normalise(const char *in, char *out, int n) {
    int k = 0, lang = 1;
    out[0] = 0;
    if (in == NULL) {
        return;
    }
    for (; *in && *in != '.' && *in != '@' && *in != ':' && k < n - 1; in++) {
        char c = *in;
        if (c == '-') {
            c = '_';
        }
        if (c == '_') {
            lang = 0;
        } else if (lang && c >= 'A' && c <= 'Z') {
            c = (char)(c + 32);
        }
        out[k++] = c;
    }
    out[k] = 0;
    if (strcmp(out, "c") == 0 || strcmp(out, "posix") == 0 || !code_ok(out)) {
        out[0] = 0;
    }
}

const char *Lang_Detect(void) {
    static const char *const kVars[4] = {"LANGUAGE", "LC_ALL", "LC_MESSAGES", "LANG"};
    int i;
    for (i = 0; i < 4; i++) {
        const char *v = getenv(kVars[i]);
        if (v != NULL && *v) {
            normalise(v, sDetected, sizeof sDetected);
            if (sDetected[0]) {
                return sDetected;
            }
            if (i > 0) {
                break; /* LC_ALL=C and the like: English (LANGUAGE is only a preference list) */
            }
        }
    }
    snprintf(sDetected, sizeof sDetected, "en");
    return sDetected;
}

int Lang_Set(const char *code) {
    char want[16], base[16];
    Catalog *c = NULL;
    int exact = 1;
    char *u;
    if (code == NULL || *code == 0 || strcmp(code, "auto") == 0) {
        code = Lang_Detect();
    }
    normalise(code, want, sizeof want);
    if (want[0] == 0) {
        snprintf(want, sizeof want, "en");
    }
    snprintf(base, sizeof base, "%s", want);
    if ((u = strchr(base, '_')) != NULL) {
        *u = 0;
    }
    pthread_mutex_lock(&sLock);
    if (strcmp(base, "en") != 0) {
        c = load(want);
        if (c == NULL && strcmp(base, want) != 0) {
            c = load(base);
        }
        exact = c != NULL;
    }
    sCur = c;
    snprintf(sCurCode, sizeof sCurCode, "%s", c != NULL ? c->code : "en");
    pthread_mutex_unlock(&sLock);
    return exact;
}

void Lang_Init(void) {
    char v[32];
    Settings_GetStr("ui.language", v, sizeof v);
    if (!Lang_Set(v) && v[0] && strcmp(v, "auto") != 0) {
        fprintf(stderr, "ttt: lang: no catalog for '%s': English\n", v);
    }
}

const char *Lang_Current(void) { return sCurCode; }

/* ---- lookups ---- */
const char *Tr(const char *key, const char *en) {
    const char *t = lookup(sCur, key);
    return t != NULL && *t ? t : en;
}

/* The printf conversions of a format in order ("%d%s"), %% left out; 0 when it holds a malformed one. */
static int conversions(const char *f, char *out, int n) {
    int k = 0;
    for (; *f; f++) {
        if (*f != '%') {
            continue;
        }
        f++;
        if (*f == '%') {
            continue;
        }
        while (*f && strchr("-+ #0123456789.lhzjt*'", *f) != NULL) {
            if (k < n - 1) {
                out[k++] = *f;
            }
            f++;
        }
        if (*f == 0 || strchr("diouxXcspfFeEgGaA", *f) == NULL) {
            return 0;
        }
        if (k < n - 2) {
            out[k++] = *f;
            out[k++] = ',';
        }
    }
    out[k] = 0;
    return 1;
}

const char *TrFmt(const char *key, const char *en) {
    const char *t = lookup(sCur, key);
    char a[64], b[64];
    if (t == NULL || *t == 0 || !conversions(t, a, sizeof a) || !conversions(en, b, sizeof b) || strcmp(a, b) != 0) {
        return en;
    }
    return t;
}

/* How many times each of {0}..{9} appears. */
static void placeholders(const char *s, int count[10]) {
    int i;
    for (i = 0; i < 10; i++) {
        count[i] = 0;
    }
    for (; *s; s++) {
        if (s[0] == '{' && s[1] >= '0' && s[1] <= '9' && s[2] == '}') {
            count[s[1] - '0']++;
        }
    }
}

static int same_placeholders(const char *a, const char *b) {
    int x[10], y[10], i;
    placeholders(a, x);
    placeholders(b, y);
    for (i = 0; i < 10; i++) {
        if ((x[i] != 0) != (y[i] != 0)) {
            return 0;
        }
    }
    return 1;
}

static int format(char *out, int n, const char *t, const char *args[10], int nargs) {
    int k = 0;
    if (n <= 0) {
        return 0;
    }
    for (; *t; t++) {
        if (t[0] == '{' && t[1] >= '0' && t[1] <= '9' && t[2] == '}') {
            int a = t[1] - '0';
            const char *s = a < nargs && args[a] != NULL ? args[a] : "";
            for (; *s && k < n - 1; s++) {
                out[k++] = *s;
            }
            t += 2;
        } else if (k < n - 1) {
            out[k++] = *t;
        }
    }
    out[k] = 0;
    return k;
}

static int vformat(char *out, int n, const char *t, va_list ap) {
    const char *args[10];
    int nargs = 0;
    const char *a;
    while (nargs < 10 && (a = va_arg(ap, const char *)) != NULL) {
        args[nargs++] = a;
    }
    return format(out, n, t, args, nargs);
}

int Lang_Format(char *out, int n, const char *key, const char *en, ...) {
    const char *t = lookup(sCur, key);
    va_list ap;
    int k;
    if (t == NULL || *t == 0 || !same_placeholders(t, en)) {
        t = en;
    }
    va_start(ap, en);
    k = vformat(out, n, t, ap);
    va_end(ap);
    return k;
}

int Lang_FormatEn(char *out, int n, const char *en, ...) {
    va_list ap;
    int k;
    va_start(ap, en);
    k = vformat(out, n, en, ap);
    va_end(ap);
    return k;
}

/* ---- the picker's list ---- */
int Lang_List(LangInfo *out, int max) {
    char dirs[4][1100];
    int nd, i, n = 0;
    if (max <= 0) {
        return 0;
    }
    snprintf(out[0].code, sizeof out[0].code, "en");
    snprintf(out[0].name, sizeof out[0].name, "English");
    n = 1;
    nd = lang_dirs(dirs);
    for (i = 0; i < nd && n < max; i++) {
        DIR *d = opendir(dirs[i]);
        struct dirent *e;
        if (d == NULL) {
            continue;
        }
        while (n < max && (e = readdir(d)) != NULL) {
            char code[16];
            size_t len = strlen(e->d_name);
            const Catalog *c;
            const char *name;
            int k, dup = 0;
            if (len < 6 || len - 5 >= sizeof code || strcmp(e->d_name + len - 5, ".json") != 0) {
                continue;
            }
            memcpy(code, e->d_name, len - 5);
            code[len - 5] = 0;
            for (k = 0; k < n; k++) {
                dup |= strcmp(out[k].code, code) == 0;
            }
            if (dup || !code_ok(code) || strcmp(code, "en") == 0) {
                continue;
            }
            pthread_mutex_lock(&sLock);
            c = load(code);
            pthread_mutex_unlock(&sLock);
            if (c == NULL) {
                continue;
            }
            name = lookup(c, "@name");
            snprintf(out[n].code, sizeof out[n].code, "%s", code);
            snprintf(out[n].name, sizeof out[n].name, "%s", name != NULL && *name ? name : code);
            n++;
        }
        closedir(d);
    }
    for (i = 2; i < n; i++) { /* by code, English first */
        int k;
        for (k = i; k > 1 && strcmp(out[k - 1].code, out[k].code) > 0; k--) {
            LangInfo t = out[k];
            out[k] = out[k - 1];
            out[k - 1] = t;
        }
    }
    return n;
}

/* ---- TTT_SELFTEST=settings: the reader, the fallbacks, the placeholders ---- */
static int sFails;
static void expect(int ok, const char *what) {
    if (!ok) {
        printf("settings: FAIL lang: %s\n", what);
        sFails++;
    }
}

int Lang_SelfTest(void) {
    static const char kGood[] = "\xEF\xBB\xBF{ \"@name\": \"Test\", \"a.b\" : \"caf\\u00e9 \\\"{0}\\\"\",\n"
                                "\"pair\": \"\\ud83d\\ude00\", \"fmt\": \"%d de %s\", \"bad.fmt\": \"%s %d\","
                                "\"miss\": \"{1}\", \"a.b\": \"\\u00bfqu\\u00e9 {0}?\" }";
    static const char *const kBad[] = {"", "[]", "{\"a\": 1}", "{\"a\": \"b\"", "{\"a\" \"b\"}", "{\"a\": \"b\",}",
                                       "{\"a\": \"\\x\"}", "{\"a\": \"b\"} x", "{\"a\": \"\\ud800\"}",
                                       "{\"a\": \"\x01\"}"};
    char err[256], out[64];
    Catalog *c = parse(kGood, (long)strlen(kGood), "zz", err, sizeof err);
    Catalog *saved = sCur;
    unsigned i;
    sFails = 0;
    expect(c != NULL, "a valid catalog is read");
    if (c != NULL) {
        expect(c->n == 7 && strcmp(lookup(c, "a.b"), "\xC2\xBFqu\xC3\xA9 {0}?") == 0, "escapes, UTF-8, the last of a repeated key");
        expect(strcmp(lookup(c, "pair"), "\xF0\x9F\x98\x80") == 0, "a surrogate pair");
        expect(strcmp(lookup(c, "@name"), "Test") == 0 && lookup(c, "nope") == NULL, "lookups");
        sCur = c;
        expect(strcmp(Tr("a.b", "x"), "\xC2\xBFqu\xC3\xA9 {0}?") == 0 && strcmp(Tr("none", "English"), "English") == 0,
               "Tr: the catalog, else English");
        expect(strcmp(TrFmt("fmt", "%d of %s"), "%d de %s") == 0 && strcmp(TrFmt("bad.fmt", "%d %s"), "%d %s") == 0,
               "TrFmt: other conversions fall back to English");
        Lang_Format(out, sizeof out, "a.b", "what {0}?", "X", NULL);
        expect(strcmp(out, "\xC2\xBFqu\xC3\xA9 X?") == 0, "Lang_Format with the catalog");
        Lang_Format(out, sizeof out, "miss", "{0} and {1}", "A", "B", NULL);
        expect(strcmp(out, "A and B") == 0, "Lang_Format: other placeholders fall back to English");
        Lang_Format(out, 6, "none", "{0}{0}{0}", "abc", NULL);
        expect(strcmp(out, "abcab") == 0, "Lang_Format cuts at the buffer");
        sCur = saved;
    }
    for (i = 0; i < sizeof kBad / sizeof kBad[0]; i++) {
        Catalog *b = parse(kBad[i], (long)strlen(kBad[i]), "zz", err, sizeof err);
        if (b != NULL) {
            printf("settings: FAIL lang: accepted broken catalog %u\n", i);
            sFails++;
        }
    }
    normalise("es_AR.UTF-8@euro", out, sizeof out);
    expect(strcmp(out, "es_AR") == 0, "es_AR.UTF-8@euro -> es_AR");
    normalise("C", out, sizeof out);
    expect(out[0] == 0, "C is no language");
    normalise("../x", out, sizeof out);
    expect(out[0] == 0, "no paths in a code");
    return sFails;
}
