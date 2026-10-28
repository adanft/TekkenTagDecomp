/*
 * ttt-server: the matchmaking server of the PC port's netplay (T46, docs/port/matchmaking.md). Standalone, one
 * process, one UDP socket, no threads, no files written, nothing remembered past a game's session: anyone can run one
 * for their friends or their community. It is never part of the game, and the game has no server built in.
 *
 * What it does (the protocol: ttt_match_proto.h):
 *   - rooms: a game asks for a room and gets a code (6 letters and digits); another game joins with that code;
 *   - quick match: games waiting in a queue are paired two by two, the same build with the same build;
 *   - the version check: only games of the same build id and netplay protocol are paired (a join to a room of
 *     another build is refused);
 *   - hole punching: each game of a pair is told the other's address as the server sees it (and its local address,
 *     for two players behind one NAT); the games then try to reach each other directly;
 *   - a relay: when they cannot (symmetric NATs, firewalls), each sends its packets here and the server forwards
 *     them to the other, within a rate limit (TURN's idea, without TURN's protocol);
 *   - the update manifest: the file given with --manifest (signed offline with ttt-sign), sent as it is to a game
 *     that asks; the game checks the signature itself.
 *
 * Build: python3 port/tools/build_server.py (any C99 compiler on Linux, macOS or the BSDs: cc -O2 -o ttt-server
 * ttt-server.c; Windows: build_server.py --win64 cross-builds ttt-server.exe with mingw-w64). Run: ttt-server
 * [--listen 127.0.0.1:7100] (--help for the rest).
 */
#define _GNU_SOURCE
#ifdef _WIN32
/* Windows (T42 b7): Winsock 2 (WSAPoll for poll; the socket made non-blocking, Winsock having no MSG_DONTWAIT), the
   system's random generator, the performance counter for the clock. */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <windows.h>
#include <bcrypt.h>
typedef SOCKET Sock;
#define SOCK_NONE INVALID_SOCKET
#define poll WSAPoll
#define MSG_DONTWAIT 0
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int Sock;
#define SOCK_NONE (-1)
#endif
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "ttt_match_proto.h"

typedef struct {
    struct sockaddr_storage a;
    socklen_t len;
} Addr;

typedef struct {
    int used, mode; /* MS_REQ_CREATE (a room) or MS_REQ_QUICK */
    Addr addr;
    uint32_t cookie, proto;
    uint64_t build, since, seen;
    uint8_t local[MS_EP];
    char name[MS_NAME], code[MS_CODE];
} Waiter;

typedef struct {
    int used, relay;
    Addr addr[2];
    uint32_t cookie[2], session;
    uint8_t pub[2][MS_EP], local[2][MS_EP];
    char name[2][MS_NAME], code[MS_CODE];
    uint64_t seen, refill[2], relayed[2], dropped[2];
    int64_t tokens[2]; /* the relay's token buckets, bytes, one per direction */
} Pair;

static struct {
    const char *listen, *manifestPath;
    int relay, verbose, maxWaiting, maxPairs;
    uint32_t waitS, pairS, rate;
} sOpt = {"127.0.0.1:7100", NULL, 1, 0, 256, 256, 10, 30, 65536};

static Waiter *sWait;
static Pair *sPair;
static Sock sSock = SOCK_NONE;
static uint8_t sManifest[MS_MANIFEST_MAX];
static uint32_t sManifestLen;
static volatile sig_atomic_t sStop;
static uint64_t sStatPairs, sStatRelayed, sStatRefused;

/* ---------------------------------------------------------------------------------------------- little helpers */

static uint64_t now_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER c, f;
    QueryPerformanceCounter(&c);
    QueryPerformanceFrequency(&f);
    return (uint64_t)(c.QuadPart / f.QuadPart) * 1000000000ull +
           (uint64_t)(c.QuadPart % f.QuadPart) * 1000000000ull / (uint64_t)f.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
#endif
}

static void put16(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}
static void put32(uint8_t *p, uint32_t v) {
    put16(p, v);
    put16(p + 2, v >> 16);
}
static uint32_t get32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint64_t get64(const uint8_t *p) { return get32(p) | (uint64_t)get32(p + 4) << 32; }

/* Random bytes from the system (codes and session ids: not guessable from outside). */
static void random_bytes(void *p, size_t n) {
#ifdef _WIN32
    if (BCryptGenRandom(NULL, (PUCHAR)p, (ULONG)n, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        fprintf(stderr, "ttt-server: no randomness (BCryptGenRandom)\n");
        exit(1);
    }
#else
    static int fd = -1;
    size_t got = 0;
    if (fd < 0) {
        fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    }
    while (fd >= 0 && got < n) {
        ssize_t r = read(fd, (uint8_t *)p + got, n - got);
        if (r <= 0) {
            break;
        }
        got += (size_t)r;
    }
    if (got < n) {
        fprintf(stderr, "ttt-server: no randomness (/dev/urandom)\n");
        exit(1);
    }
#endif
}

static int addr_eq(const Addr *x, const struct sockaddr_storage *a, socklen_t len) {
    if (x->a.ss_family != a->ss_family) {
        return 0;
    }
    if (a->ss_family == AF_INET) {
        const struct sockaddr_in *p = (const struct sockaddr_in *)&x->a, *q = (const struct sockaddr_in *)a;
        return p->sin_port == q->sin_port && p->sin_addr.s_addr == q->sin_addr.s_addr;
    }
    if (a->ss_family == AF_INET6) {
        const struct sockaddr_in6 *p = (const struct sockaddr_in6 *)&x->a, *q = (const struct sockaddr_in6 *)a;
        return p->sin6_port == q->sin6_port && memcmp(&p->sin6_addr, &q->sin6_addr, 16) == 0;
    }
    return x->len == len && memcmp(&x->a, a, len) == 0;
}

/* A socket address as the protocol's 20-byte endpoint. */
static void put_ep(uint8_t *ep, const struct sockaddr_storage *a) {
    memset(ep, 0, MS_EP);
    if (a->ss_family == AF_INET) {
        const struct sockaddr_in *s = (const struct sockaddr_in *)a;
        ep[0] = 4;
        put16(ep + 2, ntohs(s->sin_port));
        memcpy(ep + 4, &s->sin_addr, 4);
    } else if (a->ss_family == AF_INET6) {
        const struct sockaddr_in6 *s = (const struct sockaddr_in6 *)a;
        ep[0] = 6;
        put16(ep + 2, ntohs(s->sin6_port));
        memcpy(ep + 4, &s->sin6_addr, 16);
    }
}

static void addr_text(const struct sockaddr_storage *a, char *out, size_t n) {
    char ip[INET6_ADDRSTRLEN] = "?";
    unsigned port = 0;
    if (a->ss_family == AF_INET) {
        inet_ntop(AF_INET, &((const struct sockaddr_in *)a)->sin_addr, ip, sizeof ip);
        port = ntohs(((const struct sockaddr_in *)a)->sin_port);
    } else if (a->ss_family == AF_INET6) {
        inet_ntop(AF_INET6, &((const struct sockaddr_in6 *)a)->sin6_addr, ip, sizeof ip);
        port = ntohs(((const struct sockaddr_in6 *)a)->sin6_port);
    }
    snprintf(out, n, a->ss_family == AF_INET6 ? "[%s]:%u" : "%s:%u", ip, port);
}

/* A name as received: at most MS_NAME - 1 bytes, control characters replaced, never empty. */
static void clean_name(char *out, const uint8_t *in) {
    int i;
    for (i = 0; i < MS_NAME - 1 && in[i] != 0; i++) {
        out[i] = in[i] < 0x20 || in[i] == 0x7F ? '?' : (char)in[i];
    }
    out[i] = 0;
    if (i == 0) {
        snprintf(out, MS_NAME, "Player");
    }
}

static void sendto_addr(const Addr *to, const uint8_t *p, size_t n) {
    if (sendto(sSock, (const char *)p, n, 0, (const struct sockaddr *)&to->a, to->len) < 0 && sOpt.verbose) {
        fprintf(stderr, "ttt-server: send: %s\n", strerror(errno));
    }
}

static void header(uint8_t *p, int type, uint32_t cookie) {
    put32(p, MS_MAGIC);
    p[4] = (uint8_t)type;
    p[5] = MS_VERSION;
    p[6] = p[7] = 0;
    put32(p + 8, cookie);
}

static void send_error(const Addr *to, uint32_t cookie, uint32_t err, const char *why) {
    uint8_t p[MS_E_SIZE];
    memset(p, 0, sizeof p);
    header(p, MS_S_ERROR, cookie);
    put32(p + MS_E_CODE, err);
    snprintf((char *)p + MS_E_TEXT, MS_E_SIZE - MS_E_TEXT, "%s", why);
    sendto_addr(to, p, sizeof p);
    sStatRefused++;
}

static void send_wait(const Waiter *w, uint32_t place) {
    uint8_t p[MS_W_SIZE];
    memset(p, 0, sizeof p);
    header(p, MS_S_WAIT, w->cookie);
    memcpy(p + MS_W_CODE, w->code, MS_CODE);
    put32(p + MS_W_PLACE, place);
    put_ep(p + MS_W_PUBLIC, &w->addr.a);
    sendto_addr(&w->addr, p, sizeof p);
}

static void send_match(const Pair *m, int side) {
    uint8_t p[MS_M_SIZE];
    memset(p, 0, sizeof p);
    header(p, MS_S_MATCH, m->cookie[side]);
    p[MS_M_ROLE] = (uint8_t)side;
    p[MS_M_RELAY] = (uint8_t)m->relay;
    put32(p + MS_M_SESSION, m->session);
    memcpy(p + MS_M_PUBLIC, m->pub[1 - side], MS_EP);
    memcpy(p + MS_M_LOCAL, m->local[1 - side], MS_EP);
    memcpy(p + MS_M_NAME, m->name[1 - side], MS_NAME);
    memcpy(p + MS_M_CODE, m->code, MS_CODE);
    sendto_addr(&m->addr[side], p, sizeof p);
}

/* ---------------------------------------------------------------------------------------------- the tables */

static Waiter *find_waiter(const struct sockaddr_storage *a, socklen_t len, uint32_t cookie) {
    int i;
    for (i = 0; i < sOpt.maxWaiting; i++) {
        if (sWait[i].used && sWait[i].cookie == cookie && addr_eq(&sWait[i].addr, a, len)) {
            return &sWait[i];
        }
    }
    return NULL;
}

/* The pair this address belongs to, and its side in it. */
static Pair *find_pair(const struct sockaddr_storage *a, socklen_t len, int *side) {
    int i, s;
    for (i = 0; i < sOpt.maxPairs; i++) {
        for (s = 0; s < 2 && sPair[i].used; s++) {
            if (addr_eq(&sPair[i].addr[s], a, len)) {
                *side = s;
                return &sPair[i];
            }
        }
    }
    return NULL;
}

static void drop_waiter(Waiter *w) { memset(w, 0, sizeof *w); }

static void drop_pair(Pair *m, const char *why) {
    if (sOpt.verbose) {
        fprintf(stderr, "ttt-server: pair %08x ended (%s): relayed %llu / %llu bytes, dropped %llu / %llu\n",
                (unsigned)m->session, why, (unsigned long long)m->relayed[0], (unsigned long long)m->relayed[1],
                (unsigned long long)m->dropped[0], (unsigned long long)m->dropped[1]);
    }
    memset(m, 0, sizeof *m);
}

static Waiter *new_waiter(const struct sockaddr_storage *a, socklen_t len, uint32_t cookie, int mode,
                          const uint8_t *req) {
    int i;
    for (i = 0; i < sOpt.maxWaiting; i++) {
        if (!sWait[i].used) {
            Waiter *w = &sWait[i];
            memset(w, 0, sizeof *w);
            w->used = 1;
            w->mode = mode;
            memcpy(&w->addr.a, a, len);
            w->addr.len = len;
            w->cookie = cookie;
            w->proto = get32(req + MS_RQ_PROTO);
            w->build = get64(req + MS_RQ_BUILD);
            memcpy(w->local, req + MS_RQ_LOCAL, MS_EP);
            clean_name(w->name, req + MS_RQ_NAME);
            w->since = w->seen = now_ns();
            return w;
        }
    }
    return NULL;
}

static int code_taken(const char *code) {
    int i;
    for (i = 0; i < sOpt.maxWaiting; i++) {
        if (sWait[i].used && sWait[i].mode == MS_REQ_CREATE && strncmp(sWait[i].code, code, MS_CODE) == 0) {
            return 1;
        }
    }
    return 0;
}

/* A code no open room has (into `out`, which may be the new room's own field: it is written once found). */
static void new_code(char *out) {
    char code[MS_CODE];
    do {
        uint8_t r[MS_CODE_LEN];
        int i;
        random_bytes(r, sizeof r);
        memset(code, 0, MS_CODE);
        for (i = 0; i < MS_CODE_LEN; i++) {
            code[i] = MS_CODE_CHARS[r[i] % (sizeof MS_CODE_CHARS - 1)];
        }
    } while (code_taken(code));
    memcpy(out, code, MS_CODE);
}

/* host: the game that waited (pad 1); guest: the one that came (pad 2). The waiter's entry is freed. */
static void make_pair(Waiter *host, const struct sockaddr_storage *a, socklen_t len, uint32_t cookie,
                      const uint8_t *req) {
    int i;
    Pair *m = NULL;
    for (i = 0; i < sOpt.maxPairs && m == NULL; i++) {
        if (!sPair[i].used) {
            m = &sPair[i];
        }
    }
    if (m == NULL) {
        Addr to;
        memcpy(&to.a, a, len);
        to.len = len;
        send_error(&to, cookie, MS_ERR_FULL, "the server is full");
        return;
    }
    memset(m, 0, sizeof *m);
    m->used = 1;
    m->relay = sOpt.relay;
    m->addr[0] = host->addr;
    m->cookie[0] = host->cookie;
    memcpy(m->local[0], host->local, MS_EP);
    put_ep(m->pub[0], &host->addr.a);
    memcpy(m->name[0], host->name, MS_NAME);
    memcpy(&m->addr[1].a, a, len);
    m->addr[1].len = len;
    m->cookie[1] = cookie;
    memcpy(m->local[1], req + MS_RQ_LOCAL, MS_EP);
    put_ep(m->pub[1], a);
    clean_name(m->name[1], req + MS_RQ_NAME);
    memcpy(m->code, host->code, MS_CODE);
    do {
        random_bytes(&m->session, sizeof m->session);
    } while (m->session == 0);
    m->seen = m->refill[0] = m->refill[1] = now_ns();
    m->tokens[0] = m->tokens[1] = (int64_t)sOpt.rate;
    drop_waiter(host);
    sStatPairs++;
    if (sOpt.verbose) {
        char x[80], y[80];
        addr_text(&m->addr[0].a, x, sizeof x);
        addr_text(&m->addr[1].a, y, sizeof y);
        fprintf(stderr, "ttt-server: paired %s and %s (session %08x%s%s)\n", x, y, (unsigned)m->session,
                m->code[0] ? ", room " : ", quick match", m->code);
    }
    send_match(m, 0);
    send_match(m, 1);
}

/* ---------------------------------------------------------------------------------------------- the requests */

static void request(const uint8_t *p, size_t n, const struct sockaddr_storage *a, socklen_t len) {
    uint32_t cookie = get32(p + 8);
    int type = p[4], side;
    Addr from;
    Pair *m;
    Waiter *w;
    memcpy(&from.a, a, len);
    from.len = len;
    if (p[5] != MS_VERSION) {
        send_error(&from, cookie, MS_ERR_BAD, "another matchmaking protocol version");
        return;
    }
    if (type == MS_REQ_MANIFEST) {
        if (n < MS_MANIFEST_REQ_SIZE) {
            return; /* (too small to answer: no amplification) */
        }
        if (sManifestLen == 0) {
            send_error(&from, cookie, MS_ERR_NO_MANIFEST, "this server has no update manifest");
        } else {
            uint8_t out[MS_MANIFEST_REQ_SIZE];
            header(out, MS_S_MANIFEST, cookie);
            put16(out + MS_HDR, sManifestLen);
            memcpy(out + MS_HDR + 2, sManifest, sManifestLen);
            sendto_addr(&from, out, MS_HDR + 2 + sManifestLen);
        }
        return;
    }
    if (n < MS_REQ_SIZE || cookie == 0) {
        return;
    }
    if ((m = find_pair(a, len, &side)) != NULL) {
        if (m->cookie[side] == cookie) { /* (the game did not hear MATCH yet, or is leaving) */
            if (type == MS_REQ_LEAVE) {
                drop_pair(m, "left");
            } else {
                m->seen = now_ns();
                send_match(m, side);
            }
            return;
        }
        drop_pair(m, "a new game at that address"); /* the program was started again on the same port */
    }
    w = find_waiter(a, len, cookie);
    if (type == MS_REQ_LEAVE) {
        if (w != NULL) {
            drop_waiter(w);
        }
        return;
    }
    if (w != NULL && w->mode != type) { /* it changed its mind: a fresh start */
        drop_waiter(w);
        w = NULL;
    }
    if (type == MS_REQ_CREATE) {
        if (w == NULL) {
            if ((w = new_waiter(a, len, cookie, MS_REQ_CREATE, p)) == NULL) {
                send_error(&from, cookie, MS_ERR_FULL, "the server is full");
                return;
            }
            new_code(w->code);
            if (sOpt.verbose) {
                fprintf(stderr, "ttt-server: room %s opened\n", w->code);
            }
        }
        w->seen = now_ns();
        send_wait(w, 1);
    } else if (type == MS_REQ_JOIN) {
        char code[MS_CODE];
        int i;
        memset(code, 0, sizeof code);
        for (i = 0; i < MS_CODE - 1 && p[MS_RQ_CODE + i] != 0; i++) { /* the code in any case */
            uint8_t c = p[MS_RQ_CODE + i];
            code[i] = (char)(c >= 'a' && c <= 'z' ? c - 32 : c);
        }
        for (i = 0; i < sOpt.maxWaiting; i++) {
            Waiter *h = &sWait[i];
            if (h->used && h->mode == MS_REQ_CREATE && strncmp(h->code, code, MS_CODE) == 0) {
                if (h->proto != get32(p + MS_RQ_PROTO) || h->build != get64(p + MS_RQ_BUILD)) {
                    send_error(&from, cookie, MS_ERR_VERSION,
                               "the room's game is another build of the program (both need the same build)");
                    return;
                }
                if (h->cookie == cookie && addr_eq(&h->addr, a, len)) {
                    return; /* its own room */
                }
                make_pair(h, a, len, cookie, p);
                return;
            }
        }
        send_error(&from, cookie, MS_ERR_NO_ROOM, "no room has that code");
    } else if (type == MS_REQ_QUICK) {
        Waiter *other = NULL;
        uint32_t place = 1;
        int i;
        if (w == NULL && (w = new_waiter(a, len, cookie, MS_REQ_QUICK, p)) == NULL) {
            send_error(&from, cookie, MS_ERR_FULL, "the server is full");
            return;
        }
        w->seen = now_ns();
        for (i = 0; i < sOpt.maxWaiting; i++) { /* the longest waiting game of the same build */
            Waiter *o = &sWait[i];
            if (o != w && o->used && o->mode == MS_REQ_QUICK && o->build == w->build && o->proto == w->proto &&
                (other == NULL || o->since < other->since)) {
                other = o;
            }
            if (o != w && o->used && o->mode == MS_REQ_QUICK && o->since < w->since) {
                place++;
            }
        }
        if (other != NULL) {
            uint8_t req[MS_REQ_SIZE];
            memcpy(req, p, MS_REQ_SIZE);
            drop_waiter(w);
            make_pair(other, a, len, cookie, req);
        } else {
            send_wait(w, place);
        }
    } else {
        send_error(&from, cookie, MS_ERR_BAD, "unknown request");
    }
}

/* A game's own packet from a paired address: on to the other game, within the rate limit. */
static void relay(const uint8_t *p, size_t n, const struct sockaddr_storage *a, socklen_t len) {
    int side;
    Pair *m = find_pair(a, len, &side);
    uint64_t now;
    if (m == NULL || !m->relay) {
        return;
    }
    now = now_ns();
    m->tokens[side] += (int64_t)((now - m->refill[side]) * sOpt.rate / 1000000000ull);
    m->refill[side] = now;
    if (m->tokens[side] > (int64_t)sOpt.rate) {
        m->tokens[side] = (int64_t)sOpt.rate;
    }
    if (m->tokens[side] < (int64_t)n) {
        m->dropped[side] += n;
        return;
    }
    m->tokens[side] -= (int64_t)n;
    m->relayed[side] += n;
    m->seen = now;
    sStatRelayed += n;
    sendto_addr(&m->addr[1 - side], p, n);
}

static void expire(void) {
    uint64_t now = now_ns();
    int i;
    for (i = 0; i < sOpt.maxWaiting; i++) {
        if (sWait[i].used && now - sWait[i].seen > (uint64_t)sOpt.waitS * 1000000000ull) {
            if (sOpt.verbose) {
                fprintf(stderr, "ttt-server: %s%s expired\n", sWait[i].mode == MS_REQ_CREATE ? "room " : "a quick "
                        "match request", sWait[i].code);
            }
            drop_waiter(&sWait[i]);
        }
    }
    for (i = 0; i < sOpt.maxPairs; i++) {
        if (sPair[i].used && now - sPair[i].seen > (uint64_t)sOpt.pairS * 1000000000ull) {
            drop_pair(&sPair[i], "idle");
        }
    }
}

/* ---------------------------------------------------------------------------------------------- start */

static void on_signal(int s) {
    (void)s;
    sStop = 1;
}

static void usage(FILE *f) {
    fprintf(f, "usage: ttt-server [options]\n"
               "  --listen <addr>:<port>   where to listen (default 127.0.0.1:7100; 0.0.0.0:7100 or [::]:7100 for\n"
               "                           every interface; port 0: any free one, printed)\n"
               "  --manifest <file>        a signed update manifest to hand out (ttt-sign; at most %d bytes)\n"
               "  --no-relay               no relay: games that cannot reach each other directly fail\n"
               "  --relay-rate <bytes/s>   the relay's limit per game (default 65536)\n"
               "  --max-waiting <n>        rooms and quick-match requests at once (default 256)\n"
               "  --max-pairs <n>          paired games at once (default 256)\n"
               "  --wait-timeout <s>       a waiting game silent that long is forgotten (default 10)\n"
               "  --pair-timeout <s>       a pair with no traffic that long is forgotten (default 30)\n"
               "  --verbose                a line per event on stderr (addresses, names, codes)\n",
            MS_MANIFEST_MAX);
}

static int resolve_listen(const char *spec, struct sockaddr_storage *out, socklen_t *len) {
    char host[256], port[16];
    const char *colon;
    struct addrinfo hints, *res = NULL;
    size_t hn;
    if (spec[0] == '[' && (colon = strchr(spec, ']')) != NULL && colon[1] == ':') {
        hn = (size_t)(colon - spec - 1);
        colon++;
        if (hn >= sizeof host) {
            return -1;
        }
        memcpy(host, spec + 1, hn);
    } else if ((colon = strrchr(spec, ':')) != NULL) {
        hn = (size_t)(colon - spec);
        if (hn >= sizeof host) {
            return -1;
        }
        memcpy(host, spec, hn);
    } else {
        return -1;
    }
    host[hn] = 0;
    snprintf(port, sizeof port, "%s", colon + 1);
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;
    if (getaddrinfo(host, port, &hints, &res) != 0 || res == NULL) {
        return -1;
    }
    memcpy(out, res->ai_addr, res->ai_addrlen);
    *len = (socklen_t)res->ai_addrlen;
    freeaddrinfo(res);
    return 0;
}

static void load_manifest(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "ttt-server: cannot read %s: %s\n", path, strerror(errno));
        exit(2);
    }
    sManifestLen = (uint32_t)fread(sManifest, 1, sizeof sManifest, f);
    if (fgetc(f) != EOF) {
        fprintf(stderr, "ttt-server: %s: larger than %d bytes\n", path, MS_MANIFEST_MAX);
        exit(2);
    }
    fclose(f);
}

int main(int argc, char **argv) {
    struct sockaddr_storage la;
    socklen_t ll;
    char text[96];
    uint64_t lastExpire = 0;
    int i;
    for (i = 1; i < argc; i++) {
        const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : NULL;
        if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            usage(stdout);
            return 0;
        } else if (strcmp(a, "--no-relay") == 0) {
            sOpt.relay = 0;
        } else if (strcmp(a, "--verbose") == 0) {
            sOpt.verbose = 1;
        } else if (v == NULL) {
            usage(stderr);
            return 2;
        } else if (strcmp(a, "--listen") == 0) {
            sOpt.listen = v, i++;
        } else if (strcmp(a, "--manifest") == 0) {
            sOpt.manifestPath = v, i++;
        } else if (strcmp(a, "--relay-rate") == 0) {
            sOpt.rate = (uint32_t)strtoul(v, NULL, 10), i++;
        } else if (strcmp(a, "--max-waiting") == 0) {
            sOpt.maxWaiting = atoi(v), i++;
        } else if (strcmp(a, "--max-pairs") == 0) {
            sOpt.maxPairs = atoi(v), i++;
        } else if (strcmp(a, "--wait-timeout") == 0) {
            sOpt.waitS = (uint32_t)strtoul(v, NULL, 10), i++;
        } else if (strcmp(a, "--pair-timeout") == 0) {
            sOpt.pairS = (uint32_t)strtoul(v, NULL, 10), i++;
        } else {
            usage(stderr);
            return 2;
        }
    }
    if (sOpt.maxWaiting < 2 || sOpt.maxWaiting > 100000 || sOpt.maxPairs < 1 || sOpt.maxPairs > 100000 ||
        sOpt.rate < 1024 || sOpt.waitS < 1 || sOpt.pairS < 1) {
        fprintf(stderr, "ttt-server: a limit is out of range\n");
        return 2;
    }
    if (sOpt.manifestPath != NULL) {
        load_manifest(sOpt.manifestPath);
    }
    sWait = calloc((size_t)sOpt.maxWaiting, sizeof *sWait);
    sPair = calloc((size_t)sOpt.maxPairs, sizeof *sPair);
    if (sWait == NULL || sPair == NULL) {
        fprintf(stderr, "ttt-server: no memory\n");
        return 1;
    }
    if (resolve_listen(sOpt.listen, &la, &ll) != 0) {
        fprintf(stderr, "ttt-server: --listen wants <numeric addr>:<port> or [<IPv6>]:<port>\n");
        return 2;
    }
#ifdef _WIN32
    {
        WSADATA w;
        u_long on = 1;
        BOOL off = FALSE;
        DWORD got = 0;
        if (WSAStartup(MAKEWORD(2, 2), &w) != 0) {
            fprintf(stderr, "ttt-server: no sockets (WSAStartup)\n");
            return 2;
        }
        sSock = socket(la.ss_family, SOCK_DGRAM, 0);
        if (sSock == SOCK_NONE || bind(sSock, (struct sockaddr *)&la, ll) != 0 ||
            ioctlsocket(sSock, FIONBIO, &on) != 0) {
            fprintf(stderr, "ttt-server: cannot listen on %s: Winsock error %d\n", sOpt.listen, WSAGetLastError());
            return 2;
        }
        /* (an ICMP "port unreachable" from a gone game would fail the next receive: WSAECONNRESET) */
        WSAIoctl(sSock, SIO_UDP_CONNRESET, &off, sizeof off, NULL, 0, &got, NULL, NULL);
    }
#else
    sSock = socket(la.ss_family, SOCK_DGRAM, 0);
    if (sSock == SOCK_NONE || bind(sSock, (struct sockaddr *)&la, ll) != 0) {
        fprintf(stderr, "ttt-server: cannot listen on %s: %s\n", sOpt.listen, strerror(errno));
        return 2;
    }
#endif
    ll = sizeof la;
    getsockname(sSock, (struct sockaddr *)&la, &ll);
    addr_text(&la, text, sizeof text);
    printf("ttt-server: listening on %s (relay %s, manifest %s)\n", text, sOpt.relay ? "on" : "off",
           sManifestLen ? "yes" : "none");
    fflush(stdout);
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    while (!sStop) {
        struct pollfd pf;
        uint8_t p[2048];
        pf.fd = sSock;
        pf.events = POLLIN;
        pf.revents = 0;
        poll(&pf, 1, 250);
        for (;;) {
            struct sockaddr_storage from;
            socklen_t fl = sizeof from;
            long n = (long)recvfrom(sSock, (char *)p, sizeof p, MSG_DONTWAIT, (struct sockaddr *)&from, &fl);
            if (n < 0) {
                break;
            }
            if ((size_t)n > 1400) {
                continue; /* (the game's packets are smaller; the manifest request is 1232 bytes) */
            }
            if (n >= MS_HDR && get32(p) == MS_MAGIC) {
                request(p, (size_t)n, &from, fl);
            } else if (n > 0) {
                relay(p, (size_t)n, &from, fl);
            }
        }
        if (now_ns() - lastExpire > 500000000ull) {
            expire();
            lastExpire = now_ns();
        }
    }
    printf("ttt-server: stopped: %llu pairs made, %llu bytes relayed, %llu requests refused\n",
           (unsigned long long)sStatPairs, (unsigned long long)sStatRelayed, (unsigned long long)sStatRefused);
    return 0;
}
