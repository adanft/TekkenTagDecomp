/*
 * libmc on PC: the PS2 memory cards as directory trees on the host (T35).
 *
 * Card port 0 is `<saves>/mc0/` (a card path `/BASLUS-20001TekkenTT/xiao2d.ico` is the host file
 * `<saves>/mc0/BASLUS-20001TekkenTT/xiao2d.ico`); port 1 has no card unless TTT_MC1 says so (`<saves>/mc1/`).
 * `<saves>` is TTT_SAVES, else the repository's saves/ when the program is build/port/ttt of a checkout (the file
 * ../../port/tools/build.py exists beside it), else saves/ in the player's data folder (plat_paths.c: beside the
 * program when portable.txt is there, else $XDG_DATA_HOME/ttt or ~/.local/share/ttt; T43).
 *
 * TTT_MC0 / TTT_MC1 = formatted (port 0's default: an empty formatted 8 MB card when the directory does not exist yet)
 * | unformatted (unformatted until the game formats it, which creates the directory; an existing directory is a
 * formatted card) | none (no card; port 1's default).
 *
 * The asynchronous model of libmc: each call only registers the request (0, or -1 while another one is in flight);
 * sceMcSync(1, &cmd, &result) answers 0 (running) for MC_BUSY_POLLS polls (2; TTT_MC_POLLS=<n> changes it), then
 * carries out the request on the host files, stores the command number and the result and answers 1; with no request
 * it answers -1. The game polls once a frame (Mc_Update), so a request takes the same frames on every run: no wall
 * clock is involved. Output arguments (sceMcGetInfo's type / free / format, sceMcGetDir's table, a read's buffer)
 * are written at completion, as mcserv's reply is.
 *
 * Result codes (libmc): 0 success, -1 new formatted card (sceMcGetInfo) / busy (at call), -2 new unformatted card,
 * -3 card full, -4 no such file or directory / already exists (mkdir), -5 not permitted, -6 directory not empty,
 * -7 too many open files, -10 no card. A file opened for writing is kept in memory and written at sceMcClose to a
 * temporary file (`.ttt-tmp-<name>`, flushed with fsync), then renamed over the old one, so a crash leaves either
 * the old save or the new one. Free space: 8000 one-KB clusters on an empty card [I: the order of an 8 MB card's
 * free space after its file system; not measured on hardware], less ceil(size / 1024) per file and
 * ceil((entries + 2) / 2) per directory (512-byte entries). Formatting creates the card's directory and erases
 * nothing; unformatting is remembered for the session only (the files stay). Timestamps are the host files'
 * modification time, in JST as the console's clock keeps it.
 *
 * TTT_SELFTEST=mc checks this layer on a temporary directory.
 */
#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stddef.h>
#include "plat_state.h"
#include "port_host.h"

/* new host calls go through the GOT (port_host.h explains why the PLT must not grow) */
PORT_NOPLT(mkdir);
PORT_NOPLT(rename);
PORT_NOPLT(unlink);
PORT_NOPLT(rmdir);
PORT_NOPLT(opendir);
PORT_NOPLT(readdir);
PORT_NOPLT(closedir);
PORT_NOPLT(stat);
PORT_NOPLT(fsync);
PORT_NOPLT(fileno);
PORT_NOPLT(open);
PORT_NOPLT(close);
PORT_NOPLT(mkdtemp);
PORT_NOPLT(setenv);
PORT_NOPLT(unsetenv);
PORT_NOPLT(strncmp);
PORT_NOPLT(realloc);

#define MC_BUSY_POLLS 2 /* TTT_MC_POLLS overrides it (0: every request finishes at its first poll) */
#define MC_FREE_EMPTY 8000 /* clusters of 1 KB */
#define MC_MAX_FILE (MC_FREE_EMPTY * 1024)
#define MC_NAME 32 /* an entry name with its terminating zero */
#define MC_DEPTH 8
#define MC_FILES 4
#define MC_LIST 256

enum { /* libmc's command numbers, handed back by sceMcSync */
    CMD_GETINFO = 1, CMD_OPEN = 2, CMD_CLOSE = 3, CMD_READ = 5, CMD_WRITE = 6, CMD_MKDIR = 0xB, CMD_CHDIR = 0xC,
    CMD_GETDIR = 0xD, CMD_DELETE = 0xF, CMD_FORMAT = 0x10, CMD_UNFORMAT = 0x11
};
enum { MCR_OK = 0, MCR_FULL = -3, MCR_NOENT = -4, MCR_DENIED = -5, MCR_NOTEMPTY = -6, MCR_HANDLES = -7, MCR_NOCARD = -10 };
enum { O_MC_READ = 1, O_MC_WRITE = 2, O_MC_CREAT = 0x200, O_MC_TRUNC = 0x400 };
enum { CARD_NONE, CARD_FORMATTED, CARD_UNFORMATTED };

typedef struct {
    int kind;      /* CARD_* from TTT_MCn */
    int unformat;  /* sceMcUnformat this session */
    int reported;  /* sceMcGetInfo told the game about this card */
    char cwd[MC_DEPTH][MC_NAME];
    int depth;
    char root[1024];
} Card;

typedef struct {
    int used, port, mode;
    char host[1100];
    unsigned char *data;
    int size, pos, dirty;
    int oldClusters; /* the file's clusters on the card when opened */
} McFile;

static Card sCard[2];
static McFile sFile[MC_FILES];
static int sInit;
static int sBusyPolls = MC_BUSY_POLLS;
static char sSaves[1024];
static struct {
    int busy, polls, cmd, result;
    int a[6];
} sReq;
static struct { /* sceMcGetDir's position for its mode 1 (continue) */
    char dir[1100];
    char pat[MC_NAME];
    int next;
} sList;

/* ---- the host side ---- */

static int is_dir(const char *p) {
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static int exists(const char *p) {
    struct stat st;
    return stat(p, &st) == 0;
}

void Port_UserFolder(const char *leaf, char *out, int n); /* plat_paths.c */

static void saves_dir(void) {
    const char *e = getenv("TTT_SAVES");
    if (e != NULL && *e) {
        snprintf(sSaves, sizeof sSaves, "%s", e);
        return;
    }
    Port_UserFolder("saves", sSaves, sizeof sSaves);
}

extern int gPortNet; /* plat_net.c (set before main) */

static int card_kind(const char *env, int dflt) {
    const char *e = getenv(env);
    if (e == NULL || !*e) {
        return dflt;
    }
    if (strcmp(e, "none") == 0) {
        return CARD_NONE;
    }
    if (strcmp(e, "unformatted") == 0) {
        return CARD_UNFORMATTED;
    }
    return CARD_FORMATTED;
}

static void mc_setup(void) {
    int i;
    if (sInit) {
        return;
    }
    sInit = 1;
    if (!sSaves[0]) {
        saves_dir();
    }
    if (getenv("TTT_MC_POLLS") != NULL) {
        sBusyPolls = atoi(getenv("TTT_MC_POLLS"));
        sBusyPolls = sBusyPolls < 0 ? 0 : sBusyPolls;
    }
    for (i = 0; i < 2; i++) {
        memset(&sCard[i], 0, sizeof sCard[i]);
        sCard[i].kind = card_kind(i == 0 ? "TTT_MC0" : "TTT_MC1", i == 0 ? CARD_FORMATTED : CARD_NONE);
        if (gPortNet) { /* netplay (T45): no card on either side, so both boot the same and nothing is written */
            sCard[i].kind = CARD_NONE;
        }
        snprintf(sCard[i].root, sizeof sCard[i].root, "%s/mc%d", sSaves, i);
    }
}

static Card *card(int port) {
    mc_setup();
    if (port < 0 || port > 1 || sCard[port].kind == CARD_NONE) {
        return NULL;
    }
    return &sCard[port];
}

static int formatted(const Card *c) {
    if (c->unformat) {
        return 0;
    }
    return c->kind == CARD_FORMATTED || is_dir(c->root);
}

/* mkdir -p for the saves directory and the card's root */
static int make_root(const Card *c) {
    char p[1024];
    size_t i;
    snprintf(p, sizeof p, "%s", c->root);
    for (i = 1; p[i]; i++) {
        if (p[i] == '/') {
            p[i] = 0;
            mkdir(p, 0777);
            p[i] = '/';
        }
    }
    mkdir(p, 0777);
    return is_dir(p);
}

static int reserved(const char *n) { return strncmp(n, ".ttt-tmp-", 9) == 0; }

/* A card path (absolute, or relative to the port's current directory) -> its components; -4 when a component is
   too long or reserved. ".." stops at the root, so no path leaves the card. */
static int resolve(const Card *c, const char *path, char comp[MC_DEPTH][MC_NAME], int *depth) {
    int d = 0;
    const char *s = path;
    if (path == NULL) {
        return MCR_NOENT;
    }
    if (*s != '/') {
        for (d = 0; d < c->depth; d++) {
            memcpy(comp[d], c->cwd[d], MC_NAME);
        }
    }
    while (*s) {
        const char *e;
        size_t n;
        while (*s == '/') {
            s++;
        }
        if (!*s) {
            break;
        }
        for (e = s; *e && *e != '/'; e++) {
        }
        n = (size_t)(e - s);
        if (n == 1 && s[0] == '.') {
        } else if (n == 2 && s[0] == '.' && s[1] == '.') {
            if (d > 0) {
                d--;
            }
        } else {
            if (n >= MC_NAME || d >= MC_DEPTH) {
                return MCR_NOENT;
            }
            memcpy(comp[d], s, n);
            comp[d][n] = 0;
            if (reserved(comp[d])) {
                return MCR_NOENT;
            }
            d++;
        }
        s = e;
    }
    *depth = d;
    return MCR_OK;
}

static void host_path(const Card *c, char comp[MC_DEPTH][MC_NAME], int depth, char *out, size_t size) {
    int i;
    size_t n = (size_t)snprintf(out, size, "%s", c->root);
    for (i = 0; i < depth && n < size; i++) {
        n += (size_t)snprintf(out + n, size - n, "/%s", comp[i]);
    }
}

static int clusters(int bytes) { return (bytes + 1023) / 1024; }

/* the card names of a host directory, sorted (the listing order of a run does not depend on the host) */
static int list_names(const char *dir, char names[][MC_NAME], int max) {
    DIR *d = opendir(dir);
    struct dirent *e;
    int n = 0, i, j;
    if (d == NULL) {
        return -1;
    }
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0 || reserved(e->d_name) ||
            strlen(e->d_name) >= MC_NAME) {
            continue;
        }
        if (n < max) {
            snprintf(names[n++], MC_NAME, "%s", e->d_name);
        }
    }
    closedir(d);
    for (i = 1; i < n; i++) { /* insertion sort */
        char t[MC_NAME];
        memcpy(t, names[i], MC_NAME);
        for (j = i; j > 0 && strcmp(names[j - 1], t) > 0; j--) {
            memcpy(names[j], names[j - 1], MC_NAME);
        }
        memcpy(names[j], t, MC_NAME);
    }
    return n;
}

/* clusters used under a host directory (its own entries counted when it is not the card's root) */
static int used_clusters(const char *dir, int isRoot, int level) {
    char (*names)[MC_NAME];
    int n, i, used = 0;
    if (level > MC_DEPTH) {
        return 0;
    }
    names = malloc(sizeof(*names) * MC_LIST);
    if (names == NULL) {
        return 0;
    }
    n = list_names(dir, names, MC_LIST);
    if (n < 0) {
        free(names);
        return 0;
    }
    if (!isRoot) {
        used += (n + 2 + 1) / 2;
    }
    for (i = 0; i < n; i++) {
        char p[1400];
        struct stat st;
        snprintf(p, sizeof p, "%s/%s", dir, names[i]);
        if (stat(p, &st) != 0) {
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            used += used_clusters(p, 0, level + 1);
        } else {
            used += clusters((int)st.st_size);
        }
    }
    free(names);
    return used;
}

static int free_clusters(const Card *c) {
    int f = MC_FREE_EMPTY - used_clusters(c->root, 1, 0);
    int i;
    for (i = 0; i < MC_FILES; i++) { /* files being written count with their new size */
        if (sFile[i].used && sFile[i].dirty && &sCard[sFile[i].port] == c) {
            f -= clusters(sFile[i].size) - sFile[i].oldClusters;
        }
    }
    return f < 0 ? 0 : f;
}

/* ---- the requests, carried out at completion ---- */

static int do_getinfo(int port, int *type, int *freep, int *format) {
    Card *c = card(port);
    int r, f;
    if (c == NULL) {
        if (type != NULL) { *type = 0; }
        if (freep != NULL) { *freep = 0; }
        if (format != NULL) { *format = 0; }
        return MCR_NOCARD;
    }
    f = formatted(c);
    if (type != NULL) { *type = 2; } /* PS2 card */
    if (freep != NULL) { *freep = f ? free_clusters(c) : 0; }
    if (format != NULL) { *format = f; }
    r = c->reported ? MCR_OK : (f ? -1 : -2);
    c->reported = 1;
    return r;
}

static int ready(int port, Card **out) {
    Card *c = card(port);
    if (c == NULL) {
        return MCR_NOCARD;
    }
    if (!formatted(c)) {
        return -2;
    }
    *out = c;
    return MCR_OK;
}

static int do_open(int port, const char *name, int mode) {
    Card *c;
    char comp[MC_DEPTH][MC_NAME];
    int depth, r, i;
    McFile *f = NULL;
    struct stat st;
    if ((r = ready(port, &c)) != MCR_OK || (r = resolve(c, name, comp, &depth)) != MCR_OK) {
        return r;
    }
    if (depth == 0) {
        return MCR_DENIED;
    }
    for (i = 0; i < MC_FILES; i++) {
        if (!sFile[i].used) {
            f = &sFile[i];
            break;
        }
    }
    if (f == NULL) {
        return MCR_HANDLES;
    }
    memset(f, 0, sizeof *f);
    host_path(c, comp, depth - 1, f->host, sizeof f->host);
    if (depth > 1 && !is_dir(f->host)) {
        return MCR_NOENT;
    }
    host_path(c, comp, depth, f->host, sizeof f->host);
    if (stat(f->host, &st) == 0) {
        FILE *h;
        if (S_ISDIR(st.st_mode)) {
            return MCR_DENIED;
        }
        if (st.st_size > MC_MAX_FILE) {
            return MCR_DENIED;
        }
        f->oldClusters = clusters((int)st.st_size);
        if (!(mode & O_MC_TRUNC)) {
            f->size = (int)st.st_size;
            f->data = malloc(f->size ? (size_t)f->size : 1);
            h = fopen(f->host, "rb");
            if (f->data == NULL || h == NULL || fread(f->data, 1, (size_t)f->size, h) != (size_t)f->size) {
                if (h != NULL) {
                    fclose(h);
                }
                free(f->data);
                f->data = NULL;
                return MCR_DENIED;
            }
            fclose(h);
        } else {
            f->dirty = 1;
        }
    } else if (mode & O_MC_CREAT) {
        f->dirty = 1; /* created at close */
    } else {
        return MCR_NOENT;
    }
    f->used = 1;
    f->port = port;
    f->mode = mode;
    return (int)(f - sFile);
}

static McFile *file(int fd) {
    if (fd < 0 || fd >= MC_FILES || !sFile[fd].used) {
        return NULL;
    }
    return &sFile[fd];
}

static int do_read(int fd, unsigned char *buf, int size) {
    McFile *f = file(fd);
    int n;
    if (f == NULL || card(f->port) == NULL) {
        return f == NULL ? MCR_NOENT : MCR_NOCARD;
    }
    if (!(f->mode & O_MC_READ) || size < 0) {
        return MCR_DENIED;
    }
    n = f->size - f->pos;
    if (n > size) {
        n = size;
    }
    if (n > 0) {
        memcpy(buf, f->data + f->pos, (size_t)n);
        f->pos += n;
    }
    return n < 0 ? 0 : n;
}

static int do_write(int fd, const unsigned char *buf, int size) {
    McFile *f = file(fd);
    Card *c;
    int end, grow;
    if (f == NULL || (c = card(f->port)) == NULL) {
        return f == NULL ? MCR_NOENT : MCR_NOCARD;
    }
    if (!(f->mode & O_MC_WRITE) || size < 0) {
        return MCR_DENIED;
    }
    end = f->pos + size;
    if (end > MC_MAX_FILE) {
        return MCR_FULL;
    }
    if (end > f->size) {
        unsigned char *d;
        grow = clusters(end) - clusters(f->size);
        if (grow > 0 && grow > free_clusters(c)) {
            return MCR_FULL;
        }
        d = realloc(f->data, (size_t)end);
        if (d == NULL) {
            return MCR_FULL;
        }
        memset(d + f->size, 0, (size_t)(end - f->size));
        f->data = d;
        f->size = end;
    }
    if (size > 0) {
        memcpy(f->data + f->pos, buf, (size_t)size);
    }
    f->pos = end;
    f->dirty = 1;
    return size;
}

static void sync_dir(const char *path) {
    char d[1100];
    char *slash;
    int h;
    snprintf(d, sizeof d, "%s", path);
    slash = strrchr(d, '/');
    if (slash == NULL) {
        return;
    }
    *slash = 0;
    h = open(d, O_RDONLY | O_DIRECTORY);
    if (h >= 0) {
        fsync(h);
        close(h);
    }
}

/* temporary file beside the target, flushed, then renamed over it: the old file or the new one, never half of one */
static int write_atomic(const char *path, const unsigned char *data, int size) {
    char tmp[1200];
    const char *base = strrchr(path, '/');
    FILE *h;
    int ok;
    base = base != NULL ? base + 1 : path;
    snprintf(tmp, sizeof tmp, "%.*s.ttt-tmp-%s", (int)(base - path), path, base);
    h = fopen(tmp, "wb");
    if (h == NULL) {
        return 0;
    }
    ok = (size == 0 || fwrite(data, 1, (size_t)size, h) == (size_t)size);
    ok = fflush(h) == 0 && ok;
    ok = fsync(fileno(h)) == 0 && ok;
    ok = fclose(h) == 0 && ok;
    if (!ok || rename(tmp, path) != 0) {
        unlink(tmp);
        return 0;
    }
    sync_dir(path);
    return 1;
}

static int do_close(int fd) {
    McFile *f = file(fd);
    int r = MCR_OK;
    if (f == NULL) {
        return MCR_NOENT;
    }
    if (f->dirty && (f->mode & O_MC_WRITE)) {
        Card *c = card(f->port);
        if (c == NULL) {
            r = MCR_NOCARD;
        } else if (!make_root(c) || !write_atomic(f->host, f->data, f->size)) {
            r = MCR_FULL; /* the host refused the write: reported as a full card */
        }
    }
    free(f->data);
    memset(f, 0, sizeof *f);
    return r;
}

static int do_mkdir(int port, const char *name) {
    Card *c;
    char comp[MC_DEPTH][MC_NAME];
    char p[1100];
    int depth, r;
    if ((r = ready(port, &c)) != MCR_OK || (r = resolve(c, name, comp, &depth)) != MCR_OK) {
        return r;
    }
    if (depth == 0) {
        return MCR_NOENT;
    }
    host_path(c, comp, depth - 1, p, sizeof p);
    if (depth > 1 && !is_dir(p)) {
        return MCR_NOENT;
    }
    host_path(c, comp, depth, p, sizeof p);
    if (exists(p)) {
        return MCR_NOENT; /* libmc: already there */
    }
    if (free_clusters(c) < 1) {
        return MCR_FULL;
    }
    if (!make_root(c) || mkdir(p, 0777) != 0) {
        return MCR_FULL;
    }
    sync_dir(p);
    return MCR_OK;
}

static int do_chdir(int port, const char *name, char *old) {
    Card *c;
    char comp[MC_DEPTH][MC_NAME];
    char p[1100];
    int depth, r, i;
    if ((r = ready(port, &c)) != MCR_OK || (r = resolve(c, name, comp, &depth)) != MCR_OK) {
        return r;
    }
    if (old != NULL) {
        size_t n = 0;
        old[0] = 0;
        for (i = 0; i < c->depth; i++) {
            n += (size_t)snprintf(old + n, 1024 - n, "/%s", c->cwd[i]);
        }
        if (c->depth == 0) {
            snprintf(old, 1024, "/");
        }
    }
    host_path(c, comp, depth, p, sizeof p);
    if (depth > 0 && !is_dir(p)) {
        return MCR_NOENT;
    }
    for (i = 0; i < depth; i++) {
        memcpy(c->cwd[i], comp[i], MC_NAME);
    }
    c->depth = depth;
    return MCR_OK;
}

static int do_delete(int port, const char *name) {
    Card *c;
    char comp[MC_DEPTH][MC_NAME];
    char p[1100];
    struct stat st;
    int depth, r, i;
    if ((r = ready(port, &c)) != MCR_OK || (r = resolve(c, name, comp, &depth)) != MCR_OK) {
        return r;
    }
    if (depth == 0) {
        return MCR_DENIED;
    }
    host_path(c, comp, depth, p, sizeof p);
    if (stat(p, &st) != 0) {
        return MCR_NOENT;
    }
    for (i = 0; i < MC_FILES; i++) {
        if (sFile[i].used && strcmp(sFile[i].host, p) == 0) {
            return MCR_DENIED; /* open */
        }
    }
    if (S_ISDIR(st.st_mode)) {
        char one[1][MC_NAME];
        if (list_names(p, one, 1) > 0) {
            return MCR_NOTEMPTY;
        }
        r = rmdir(p);
    } else {
        r = unlink(p);
    }
    if (r != 0) {
        return MCR_DENIED;
    }
    sync_dir(p);
    return MCR_OK;
}

static int match(const char *pat, const char *s) {
    if (!*pat) {
        return !*s;
    }
    if (*pat == '*') {
        for (;;) {
            if (match(pat + 1, s)) {
                return 1;
            }
            if (!*s++) {
                return 0;
            }
        }
    }
    if (*s && (*pat == '?' || *pat == *s)) {
        return match(pat + 1, s + 1);
    }
    return 0;
}

/* time_t -> the card's date fields, JST (UTC+9); days to civil date after H. Hinnant's algorithm */
static void put_time(unsigned char *t, long long secs) {
    long long z, era, doe, yoe, doy, mp, y, m, d, sod;
    secs += 9 * 3600;
    z = secs >= 0 ? secs / 86400 : (secs - 86399) / 86400;
    sod = secs - z * 86400;
    z += 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = z - era * 146097;
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = yoe + era * 400;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp = (5 * doy + 2) / 153;
    d = doy - (153 * mp + 2) / 5 + 1;
    m = mp < 10 ? mp + 3 : mp - 9;
    y += m <= 2;
    t[0] = 0;
    t[1] = (unsigned char)(sod % 60);
    t[2] = (unsigned char)(sod / 60 % 60);
    t[3] = (unsigned char)(sod / 3600);
    t[4] = (unsigned char)d;
    t[5] = (unsigned char)m;
    t[6] = (unsigned char)(y & 0xFF);
    t[7] = (unsigned char)(y >> 8);
}

/* one sceMcTblGetDir entry (64 bytes): created, modified, size, attributes, ..., name at 0x20 */
static void put_entry(unsigned char *e, const char *name, int isDir, int size, long long mtime) {
    memset(e, 0, 64);
    put_time(e, mtime);
    put_time(e + 8, mtime);
    e[0x10] = (unsigned char)size;
    e[0x11] = (unsigned char)(size >> 8);
    e[0x12] = (unsigned char)(size >> 16);
    e[0x13] = (unsigned char)(size >> 24);
    e[0x14] = isDir ? 0x27 : 0x97; /* readable, writable, executable, file / subdirectory, closed, exists */
    e[0x15] = isDir ? 0x84 : 0x84;
    snprintf((char *)e + 0x20, MC_NAME, "%s", name);
}

static int do_getdir(int port, const char *name, int mode, int maxent, unsigned char *table) {
    Card *c;
    char comp[MC_DEPTH][MC_NAME];
    char (*names)[MC_NAME];
    int depth, r, n, i, idx, count = 0;
    struct stat st;
    if ((r = ready(port, &c)) != MCR_OK) {
        return r;
    }
    if (mode == 0) {
        if ((r = resolve(c, name, comp, &depth)) != MCR_OK) {
            return r;
        }
        memset(&sList, 0, sizeof sList); /* (no stale bytes after the strings: the list is part of a state) */
        if (depth == 0) {
            snprintf(sList.pat, sizeof sList.pat, "*");
            host_path(c, comp, 0, sList.dir, sizeof sList.dir);
        } else {
            snprintf(sList.pat, sizeof sList.pat, "%s", comp[depth - 1]);
            host_path(c, comp, depth - 1, sList.dir, sizeof sList.dir);
        }
        sList.next = 0;
        if (!is_dir(sList.dir)) {
            if (depth <= 1 && sList.dir[0] && !exists(c->root)) {
                return 0; /* the empty card's root, not created yet */
            }
            return MCR_NOENT;
        }
    }
    names = malloc(sizeof(*names) * (MC_LIST + 2));
    if (names == NULL) {
        return MCR_DENIED;
    }
    /* "." and ".." first in a subdirectory, as the card's own entries */
    n = 0;
    if (strcmp(sList.dir, c->root) != 0) {
        snprintf(names[n++], MC_NAME, ".");
        snprintf(names[n++], MC_NAME, "..");
    }
    r = list_names(sList.dir, names + n, MC_LIST);
    n += r < 0 ? 0 : r;
    idx = 0;
    for (i = 0; i < n && count < maxent; i++) {
        char p[1400];
        int isDir, size;
        if (!match(sList.pat, names[i])) {
            continue;
        }
        if (idx++ < sList.next) {
            continue;
        }
        if (names[i][0] == '.') {
            snprintf(p, sizeof p, "%s", sList.dir);
        } else {
            snprintf(p, sizeof p, "%s/%s", sList.dir, names[i]);
        }
        if (stat(p, &st) != 0) {
            continue;
        }
        isDir = S_ISDIR(st.st_mode);
        size = (int)st.st_size;
        if (isDir) { /* a directory's size is its number of entries */
            char (*sub)[MC_NAME] = malloc(sizeof(*sub) * MC_LIST);
            int k = sub != NULL ? list_names(p, sub, MC_LIST) : 0;
            free(sub);
            size = (k < 0 ? 0 : k) + 2;
        }
        if (table != NULL) {
            put_entry(table + count * 64, names[i], isDir, size, (long long)st.st_mtime);
        }
        count++;
    }
    sList.next += count;
    free(names);
    return count;
}

static int do_format(int port) {
    Card *c = card(port);
    if (c == NULL) {
        return MCR_NOCARD;
    }
    if (!make_root(c)) {
        return MCR_DENIED;
    }
    c->unformat = 0;
    c->depth = 0;
    return MCR_OK;
}

static int do_unformat(int port) {
    Card *c = card(port);
    if (c == NULL) {
        return MCR_NOCARD;
    }
    c->unformat = 1;
    c->depth = 0;
    return MCR_OK;
}

static int run_request(void) {
    int *a = sReq.a;
    switch (sReq.cmd) {
    case CMD_GETINFO:
        return do_getinfo(a[0], (int *)(uintptr_t)(uint32_t)a[2], (int *)(uintptr_t)(uint32_t)a[3],
                          (int *)(uintptr_t)(uint32_t)a[4]);
    case CMD_OPEN:
        return do_open(a[0], (const char *)(uintptr_t)(uint32_t)a[2], a[3]);
    case CMD_CLOSE:
        return do_close(a[0]);
    case CMD_READ:
        return do_read(a[0], (unsigned char *)(uintptr_t)(uint32_t)a[1], a[2]);
    case CMD_WRITE:
        return do_write(a[0], (const unsigned char *)(uintptr_t)(uint32_t)a[1], a[2]);
    case CMD_MKDIR:
        return do_mkdir(a[0], (const char *)(uintptr_t)(uint32_t)a[2]);
    case CMD_CHDIR:
        return do_chdir(a[0], (const char *)(uintptr_t)(uint32_t)a[2], (char *)(uintptr_t)(uint32_t)a[3]);
    case CMD_GETDIR:
        return do_getdir(a[0], (const char *)(uintptr_t)(uint32_t)a[2], a[3], a[4],
                         (unsigned char *)(uintptr_t)(uint32_t)a[5]);
    case CMD_DELETE:
        return do_delete(a[0], (const char *)(uintptr_t)(uint32_t)a[2]);
    case CMD_FORMAT:
        return do_format(a[0]);
    case CMD_UNFORMAT:
        return do_unformat(a[0]);
    }
    return MCR_DENIED;
}

/* The host side off the EE stack (Port_OffEe): the C library's frames hold addresses of this process. */
static void mc_setup_off(void *p) {
    (void)p;
    mc_setup();
}
static void run_request_off(void *p) { *(int *)p = run_request(); }

static int start(int cmd, int a0, int a1, int a2, int a3, int a4, int a5) {
    Port_OffEe(mc_setup_off, NULL);
    if (sReq.busy) {
        return -1;
    }
    sReq.busy = 1;
    sReq.polls = 0;
    sReq.cmd = cmd;
    sReq.a[0] = a0;
    sReq.a[1] = a1;
    sReq.a[2] = a2;
    sReq.a[3] = a3;
    sReq.a[4] = a4;
    sReq.a[5] = a5;
    return 0;
}

/* ---- libmc ---- */

int sceMcInit(void) {
    Port_OffEe(mc_setup_off, NULL);
    return 0;
}

/* mode 0 waits, mode 1 polls: 0 running, 1 finished (cmd / result stored), -1 nothing registered */
int sceMcSync(int mode, int *cmd, int *result) {
    if (!sReq.busy) {
        return -1;
    }
    if (mode != 0 && sReq.polls < sBusyPolls) {
        sReq.polls++;
        return 0;
    }
    Port_OffEe(run_request_off, &sReq.result);
    sReq.busy = 0;
    if (cmd != NULL) {
        *cmd = sReq.cmd;
    }
    if (result != NULL) {
        *result = sReq.result;
    }
    return 1;
}

int func_00406D08(int port, int slot, int *type, int *free_, int *format) { /* sceMcGetInfo */
    return start(CMD_GETINFO, port, slot, (int)(uintptr_t)type, (int)(uintptr_t)free_, (int)(uintptr_t)format, 0);
}
int sceMcOpen(int port, int slot, int name, int mode) { return start(CMD_OPEN, port, slot, name, mode, 0, 0); }
int sceMcClose(int fd) { return start(CMD_CLOSE, fd, 0, 0, 0, 0, 0); }
int func_004069B0(int fd, int buf, int size) { return start(CMD_READ, fd, buf, size, 0, 0, 0); }  /* sceMcRead */
int func_00406A88(int fd, int buf, int size) { return start(CMD_WRITE, fd, buf, size, 0, 0, 0); } /* sceMcWrite */
int sceMcMkdir(int port, int slot, int name) { return start(CMD_MKDIR, port, slot, name, 0, 0, 0); }
int func_00407138(int port, int slot, int name) { return start(CMD_DELETE, port, slot, name, 0, 0, 0); } /* sceMcDelete */
int func_00406E50(int port, int slot, int name, int mode, int maxent, int table) { /* sceMcGetDir */
    return start(CMD_GETDIR, port, slot, name, mode, maxent, table);
}
int func_00406FC0(int port, int slot, int name, int old) { return start(CMD_CHDIR, port, slot, name, old, 0, 0); } /* sceMcChdir */
int func_004070A8(int port, int slot) { return start(CMD_FORMAT, port, slot, 0, 0, 0, 0); }   /* sceMcFormat */
int func_004074F8(int port, int slot) { return start(CMD_UNFORMAT, port, slot, 0, 0, 0, 0); } /* sceMcUnformat */

/* ---- TTT_SELFTEST=mc ---- */

static int sFails;
#define CHECK(c)                                                                                                       \
    do {                                                                                                               \
        if (!(c)) {                                                                                                    \
            printf("ttt: mc self-test: line %d: %s\n", __LINE__, #c);                                                  \
            sFails++;                                                                                                  \
        }                                                                                                              \
    } while (0)

#define A(p) ((int)(uintptr_t)(p))

/* one request from call to completion; the polls must be exactly MC_BUSY_POLLS zeros, then 1 */
static int wait_result(int called, int expectCmd) {
    int cmd = -1, res = 0x7FFF, i, r;
    CHECK(called == 0);
    for (i = 0; i < MC_BUSY_POLLS; i++) {
        r = sceMcSync(1, &cmd, &res);
        CHECK(r == 0);
    }
    r = sceMcSync(1, &cmd, &res);
    CHECK(r == 1);
    CHECK(cmd == expectCmd);
    CHECK(sceMcSync(1, &cmd, &res) == -1);
    return res;
}

static void rm_tree(const char *dir) {
    DIR *d = opendir(dir);
    struct dirent *e;
    if (d != NULL) {
        while ((e = readdir(d)) != NULL) {
            char p[1400];
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) {
                continue;
            }
            snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
            if (is_dir(p)) {
                rm_tree(p);
            } else {
                unlink(p);
            }
        }
        closedir(d);
    }
    rmdir(dir);
}

static int host_size(const char *p) {
    struct stat st;
    return stat(p, &st) == 0 ? (int)st.st_size : -1;
}

static void mc_reset(const char *saves, const char *mc0) {
    int i;
    for (i = 0; i < MC_FILES; i++) {
        free(sFile[i].data);
    }
    memset(sFile, 0, sizeof sFile);
    memset(&sReq, 0, sizeof sReq);
    snprintf(sSaves, sizeof sSaves, "%s", saves);
    if (mc0 != NULL) {
        setenv("TTT_MC0", mc0, 1);
    } else {
        unsetenv("TTT_MC0");
    }
    unsetenv("TTT_MC1");
    unsetenv("TTT_MC_POLLS");
    sBusyPolls = MC_BUSY_POLLS;
    sInit = 0;
    mc_setup();
}

static int mc_selftest(void) {
    char tmpl[] = "/tmp/ttt-mc-XXXXXX";
    char base[1100], p[1400];
    const char *dir = "/BASLUS-20001TekkenTT";
    static unsigned char buf[0x480], back[0x480], table[24 * 64];
    int type = -9, freeC = -9, format = -9, fd, i, n, free0;
    const char *t = getenv("TMPDIR");
    if (t != NULL && *t) {
        snprintf(base, sizeof base, "%s/ttt-mc-XXXXXX", t);
    } else {
        snprintf(base, sizeof base, "%s", tmpl);
    }
    if (mkdtemp(base) == NULL) {
        printf("ttt: mc self-test: no temporary directory\n");
        return 1;
    }
    mc_reset(base, NULL);
    snprintf(p, sizeof p, "%s/mc0", base);

    /* an empty formatted card: first GetInfo -1 (new card), then 0; nothing created on the host yet */
    CHECK(sceMcSync(1, NULL, NULL) == -1);
    CHECK(wait_result(func_00406D08(0, 0, &type, &freeC, &format), CMD_GETINFO) == -1);
    CHECK(type == 2 && format == 1 && freeC == MC_FREE_EMPTY);
    CHECK(wait_result(func_00406D08(0, 0, &type, &freeC, &format), CMD_GETINFO) == 0);
    CHECK(!exists(p));
    /* a second call while one is in flight is refused */
    CHECK(func_00406D08(0, 0, &type, &freeC, &format) == 0);
    CHECK(sceMcOpen(0, 0, A(dir), 1) == -1);
    CHECK(sceMcSync(0, NULL, NULL) == 1);
    /* port 1: no card */
    type = freeC = format = -9;
    CHECK(wait_result(func_00406D08(1, 0, &type, &freeC, &format), CMD_GETINFO) == MCR_NOCARD);
    CHECK(type == 0 && freeC == 0 && format == 0);
    CHECK(wait_result(sceMcOpen(1, 0, A("/x"), 1), CMD_OPEN) == MCR_NOCARD);

    /* the game's check on an empty card: GetDir of a missing directory -4, chdir -4, open -4 */
    CHECK(wait_result(func_00406E50(0, 0, A("/BASLUS-20001TekkenTT/*"), 0, 24, A(table)), CMD_GETDIR) == MCR_NOENT);
    CHECK(wait_result(func_00406FC0(0, 0, A(dir), 0), CMD_CHDIR) == MCR_NOENT);
    CHECK(wait_result(sceMcOpen(0, 0, A("/BASLUS-20001TekkenTT/BASLUS-20001TekkenTT"), 1), CMD_OPEN) == MCR_NOENT);
    CHECK(wait_result(func_00406E50(0, 0, A("/*"), 0, 24, A(table)), CMD_GETDIR) == 0);

    /* mkdir, again (-4), chdir, a relative file written through the save's open mode 0x203 */
    CHECK(wait_result(sceMcMkdir(0, 0, A(dir)), CMD_MKDIR) == 0);
    CHECK(wait_result(sceMcMkdir(0, 0, A(dir)), CMD_MKDIR) == MCR_NOENT);
    CHECK(wait_result(sceMcMkdir(0, 0, A("/nope/sub")), CMD_MKDIR) == MCR_NOENT);
    CHECK(wait_result(func_00406FC0(0, 0, A(dir), 0), CMD_CHDIR) == 0);
    for (i = 0; i < (int)sizeof buf; i++) {
        buf[i] = (unsigned char)(i * 7 + 3);
    }
    fd = wait_result(sceMcOpen(0, 0, A("BASLUS-20001TekkenTT"), 0x203), CMD_OPEN);
    CHECK(fd >= 0);
    CHECK(wait_result(func_00406A88(fd, A(buf), 0x480), CMD_WRITE) == 0x480);
    snprintf(p, sizeof p, "%s/mc0/BASLUS-20001TekkenTT/BASLUS-20001TekkenTT", base);
    CHECK(!exists(p)); /* written at close */
    CHECK(wait_result(func_004069B0(fd, A(back), 4), CMD_READ) == 0); /* 0x203 reads too: at the end */
    CHECK(wait_result(sceMcClose(fd), CMD_CLOSE) == 0);
    CHECK(host_size(p) == 0x480);
    fd = wait_result(sceMcOpen(0, 0, A("BASLUS-20001TekkenTT"), 2), CMD_OPEN); /* write only */
    CHECK(wait_result(func_004069B0(fd, A(back), 4), CMD_READ) == MCR_DENIED);
    CHECK(wait_result(sceMcClose(fd), CMD_CLOSE) == 0);
    CHECK(wait_result(sceMcClose(fd), CMD_CLOSE) == MCR_NOENT);
    snprintf(p, sizeof p, "%s/mc0/BASLUS-20001TekkenTT/.ttt-tmp-BASLUS-20001TekkenTT", base);
    CHECK(!exists(p));

    /* read it back (absolute path), past the end */
    fd = wait_result(sceMcOpen(0, 0, A("/BASLUS-20001TekkenTT/BASLUS-20001TekkenTT"), 1), CMD_OPEN);
    CHECK(fd >= 0);
    memset(back, 0, sizeof back);
    CHECK(wait_result(func_004069B0(fd, A(back), 0x480), CMD_READ) == 0x480);
    CHECK(memcmp(back, buf, sizeof buf) == 0);
    CHECK(wait_result(func_004069B0(fd, A(back), 0x480), CMD_READ) == 0);
    CHECK(wait_result(func_00406A88(fd, A(buf), 4), CMD_WRITE) == MCR_DENIED); /* opened read-only */
    CHECK(wait_result(sceMcClose(fd), CMD_CLOSE) == 0);

    /* rewrite without truncation: the head changes, the tail stays (open mode 0x203 has no TRUNC) */
    fd = wait_result(sceMcOpen(0, 0, A("BASLUS-20001TekkenTT"), 0x203), CMD_OPEN);
    CHECK(wait_result(func_00406A88(fd, A("ABCD"), 4), CMD_WRITE) == 4);
    CHECK(wait_result(sceMcClose(fd), CMD_CLOSE) == 0);
    fd = wait_result(sceMcOpen(0, 0, A("BASLUS-20001TekkenTT"), 1), CMD_OPEN);
    CHECK(wait_result(func_004069B0(fd, A(back), 0x480), CMD_READ) == 0x480);
    CHECK(memcmp(back, "ABCD", 4) == 0 && memcmp(back + 4, buf + 4, 0x480 - 4) == 0);
    CHECK(wait_result(sceMcClose(fd), CMD_CLOSE) == 0);

    /* the directory listing the game asks for: ".", "..", then the file with its size and name at 0x20 */
    memset(table, 0xEE, sizeof table);
    n = wait_result(func_00406E50(0, 0, A("/BASLUS-20001TekkenTT/*"), 0, 24, A(table)), CMD_GETDIR);
    CHECK(n == 3);
    CHECK(strcmp((char *)table + 0x20, ".") == 0 && strcmp((char *)table + 64 + 0x20, "..") == 0);
    CHECK(strcmp((char *)table + 128 + 0x20, "BASLUS-20001TekkenTT") == 0);
    CHECK(*(int *)(table + 128 + 0x10) == 0x480);
    CHECK(table[128 + 7] == 0x07 || table[128 + 7] == 0x08); /* year 20xx, high byte */
    CHECK(table[3 * 64] == 0xEE); /* nothing past the count */
    n = wait_result(func_00406E50(0, 0, A("/BASLUS-20001TekkenTT/*"), 1, 24, A(table)), CMD_GETDIR);
    CHECK(n == 0); /* mode 1 continues after the last entry */
    n = wait_result(func_00406E50(0, 0, A("/BASLUS-20001TekkenTT/?ASLUS*"), 0, 24, A(table)), CMD_GETDIR);
    CHECK(n == 1);
    n = wait_result(func_00406E50(0, 0, A("/BASLUS-20001TekkenTT/*"), 0, 2, A(table)), CMD_GETDIR);
    CHECK(n == 2); /* maxent */

    /* free space: 2 clusters of directory entries and 2 of data */
    CHECK(wait_result(func_00406D08(0, 0, &type, &freeC, &format), CMD_GETINFO) == 0);
    CHECK(freeC == MC_FREE_EMPTY - 2 - 2);
    free0 = freeC;

    /* ".." stops at the card's root: nothing outside mc0 is reachable */
    CHECK(wait_result(sceMcOpen(0, 0, A("../../../../etc/passwd"), 1), CMD_OPEN) == MCR_NOENT);
    CHECK(wait_result(sceMcOpen(0, 0, A("../BASLUS-20001TekkenTT/BASLUS-20001TekkenTT"), 1), CMD_OPEN) >= 0);
    CHECK(wait_result(sceMcClose(0), CMD_CLOSE) == 0);
    CHECK(wait_result(sceMcOpen(0, 0, A(".ttt-tmp-x"), 0x203), CMD_OPEN) == MCR_NOENT);

    /* a full card: a write past the free space is refused (-3) and nothing is written */
    {
        static unsigned char big[64 * 1024];
        int left = free0, w = 0;
        fd = wait_result(sceMcOpen(0, 0, A("big"), 0x203), CMD_OPEN);
        CHECK(fd >= 0);
        for (i = 0; i < 200 && left >= 64; i++) {
            w = wait_result(func_00406A88(fd, A(big), (int)sizeof big), CMD_WRITE);
            if (w != (int)sizeof big) {
                break;
            }
            left -= 64;
        }
        CHECK(w == MCR_FULL || left < 64);
        CHECK(wait_result(func_00406A88(fd, A(big), (int)sizeof big), CMD_WRITE) == MCR_FULL);
        CHECK(wait_result(sceMcClose(fd), CMD_CLOSE) == 0);
        CHECK(wait_result(func_00406D08(0, 0, &type, &freeC, &format), CMD_GETINFO) == 0);
        CHECK(freeC < 64 + 2);
        CHECK(wait_result(func_00407138(0, 0, A("big")), CMD_DELETE) == 0);
        CHECK(wait_result(func_00406D08(0, 0, &type, &freeC, &format), CMD_GETINFO) == 0);
        CHECK(freeC == free0);
    }

    /* too many open files */
    for (i = 0; i < MC_FILES; i++) {
        CHECK(wait_result(sceMcOpen(0, 0, A("BASLUS-20001TekkenTT"), 1), CMD_OPEN) == i);
    }
    CHECK(wait_result(sceMcOpen(0, 0, A("BASLUS-20001TekkenTT"), 1), CMD_OPEN) == MCR_HANDLES);
    CHECK(wait_result(func_00407138(0, 0, A("BASLUS-20001TekkenTT")), CMD_DELETE) == MCR_DENIED); /* open */
    for (i = 0; i < MC_FILES; i++) {
        CHECK(wait_result(sceMcClose(i), CMD_CLOSE) == 0);
    }

    /* delete: a non-empty directory -6, the file, a missing file -4, then the empty directory */
    CHECK(wait_result(func_00407138(0, 0, A(dir)), CMD_DELETE) == MCR_NOTEMPTY);
    CHECK(wait_result(func_00407138(0, 0, A("/BASLUS-20001TekkenTT/BASLUS-20001TekkenTT")), CMD_DELETE) == 0);
    CHECK(wait_result(func_00407138(0, 0, A("/BASLUS-20001TekkenTT/BASLUS-20001TekkenTT")), CMD_DELETE) == MCR_NOENT);
    CHECK(wait_result(func_00407138(0, 0, A(dir)), CMD_DELETE) == 0);
    CHECK(wait_result(func_00406D08(0, 0, &type, &freeC, &format), CMD_GETINFO) == 0);
    CHECK(freeC == MC_FREE_EMPTY);

    /* the same card again after a restart: GetInfo says new card (-1) once */
    mc_reset(base, NULL);
    CHECK(wait_result(func_00406D08(0, 0, &type, &freeC, &format), CMD_GETINFO) == -1);

    /* no card at all */
    mc_reset(base, "none");
    CHECK(wait_result(func_00406D08(0, 0, &type, &freeC, &format), CMD_GETINFO) == MCR_NOCARD);

    /* an unformatted card (no directory yet): -2, file calls -2, format creates it, unformat for the session */
    rm_tree(base);
    mkdir(base, 0777);
    mc_reset(base, "unformatted");
    CHECK(wait_result(func_00406D08(0, 0, &type, &freeC, &format), CMD_GETINFO) == -2);
    CHECK(type == 2 && format == 0 && freeC == 0);
    CHECK(wait_result(sceMcMkdir(0, 0, A(dir)), CMD_MKDIR) == -2);
    CHECK(wait_result(func_004070A8(0, 0), CMD_FORMAT) == 0);
    snprintf(p, sizeof p, "%s/mc0", base);
    CHECK(is_dir(p));
    CHECK(wait_result(func_00406D08(0, 0, &type, &freeC, &format), CMD_GETINFO) == 0);
    CHECK(format == 1 && freeC == MC_FREE_EMPTY);
    CHECK(wait_result(func_004074F8(0, 0), CMD_UNFORMAT) == 0);
    CHECK(wait_result(func_00406D08(0, 0, &type, &freeC, &format), CMD_GETINFO) == 0);
    CHECK(format == 0);

    /* the date fields: 2001-02-03 04:05:06 JST */
    {
        unsigned char tm[8];
        put_time(tm, 981140706LL);
        CHECK(tm[1] == 6 && tm[2] == 5 && tm[3] == 4 && tm[4] == 3 && tm[5] == 2 && tm[6] + tm[7] * 256 == 2001);
    }

    rm_tree(base);
    mc_reset("", NULL);
    printf(sFails ? "ttt: mc self-test FAILED (%d)\n" : "ttt: mc self-test OK\n", sFails);
    return sFails;
}

static void mc_selftest_exit(void) { exit(mc_selftest() != 0); }
__attribute__((constructor(103))) static void Port_McSelfTestEntry(void) {
    const char *t = getenv("TTT_SELFTEST");
    if (t != NULL && strcmp(t, "mc") == 0) {
        Port_OnLowStack(mc_selftest_exit); /* (x86-64: on a stack below 4 GB, plat_state.h) */
    }
}

/* The state: the request in flight and what the game changed of each card (not the host paths, which come from
   TTT_SAVES). An open file's data is host memory: no state is taken while the game has one open. The set-up comes
   first, so that a later one does not undo a load. */
static const char *mc_busy(void) {
    int i;
    for (i = 0; i < MC_FILES; i++) {
        if (sFile[i].used) {
            return "a memory card file is open";
        }
    }
    return NULL;
}
static void mc_before(void) { mc_setup(); }
__attribute__((constructor(101))) static void mc_state(void) {
    Port_StateAdd("mc.req", &sReq, sizeof sReq, PS_HASH);
    Port_StateAdd("mc.card0", &sCard[0].unformat, offsetof(Card, root) - offsetof(Card, unformat), PS_HASH);
    Port_StateAdd("mc.card1", &sCard[1].unformat, offsetof(Card, root) - offsetof(Card, unformat), PS_HASH);
    Port_StateAdd("mc.list", &sList, sizeof sList, 0);
    Port_StateHooks(mc_busy, mc_before, NULL);
}
