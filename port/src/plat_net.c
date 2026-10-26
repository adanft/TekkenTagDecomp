/*
 * Rollback netplay (T45, docs/port/netplay.md): two copies of the game, one per player, over UDP.
 *
 * Both run the same simulation from boot on the deterministic clock (plat_hw.c's headless clock: one Timer 0 tick per
 * poll), paced here to one VBlank every 1/59.94 s. The only thing that differs between them is where each pad's state
 * comes from: the local player's pad (the window's pad 1, or TTT_INPUT_PLAY) is ours, the other one the peer's. So the
 * peers exchange the pads' states per VBlank and nothing else; each runs the whole game.
 *
 * Input delay and rollback (GGPO's scheme): the local pad sampled at VBlank n is the state for VBlank n + delay, sent
 * to the peer at once (with every state the peer has not acknowledged yet, so a lost packet costs nothing). When the
 * peer's state for a VBlank has not arrived when the game latches it, the last one received is used (prediction).
 * When it arrives and differs, the state saved at the VBlank boundary before it (plat_state.c, a ring of saves with
 * page tracking) is loaded and the VBlanks since run again with the right states, undrawn and unheard (gPortResim,
 * gPortNoGs: the picture and the sound of each VBlank go out once, the first time). The game never runs more than
 * `rollback` VBlanks ahead of the last state it has from the peer: past that it waits.
 *
 * Time sync: each packet carries the sender's VBlank and its frame advantage (how far ahead of the peer it thinks it
 * is, from the round trip); the one ahead slows down a little (a few ms per VBlank) until both are level.
 *
 * Desync detection: every `check` VBlanks whose inputs are final on both sides, each peer hashes the state
 * (Port_StateHash's XXH3 over the hashed regions) and sends it with its inputs; a difference is reported with the
 * VBlank and the regions that differ (each side sends its region hashes), and stops the session.
 *
 * The connection (at the first VBlank boundary, before the game reads a pad): the joining side sends HELLO until the
 * host answers WELCOME (or REFUSE): the protocol version, the build id (the program's code), the state hash at that
 * first boundary (the game's data and everything the boot read), the settings that change the state (the cheat, the
 * memory card's polls, TTT_SEED) and the host's input delay, which both use. Anything different is refused.
 *
 * Switches (docs/port/netplay.md):
 *   TTT_NET_HOST=[<addr>:]<port>  host on that UDP port (address 127.0.0.1 unless given: 0.0.0.0 for every interface)
 *   TTT_NET_JOIN=<addr>:<port>    join a host
 *   TTT_NET_DELAY=<n>             input delay in VBlanks (the host's is used; default 2)
 *   TTT_NET_ROLLBACK=<n>          the most VBlanks run ahead of the peer's inputs (default 6, at most 12)
 *   TTT_NET_CHECK=<n>             a state hash every n VBlanks (default 60, 0: none)
 *   TTT_NET_WAIT=<s>              how long the host waits for the joining side, and it for the host (default 60)
 *   TTT_NET_TIMEOUT=<s>           a peer silent that long while it is waited for is gone (default 5)
 *   TTT_NET_DESYNC=continue       report a desync and go on (default: stop, exit code 5)
 *   tests and measurements:
 *   TTT_NET_SIM=delay:<ms>,jitter:<ms>,loss:<%>  what this side sends is delayed, shuffled and lost on purpose
 *   TTT_NET_PACE_FROM=<vblank>    paced from that VBlank on (default 1; 0: never, as fast as both can go)
 *   TTT_NET_DRAW=0                nothing drawn at all (headless runs: only the state matters)
 *   TTT_NET_PERTURB=<vblank>      flip a bit of the game's state at that VBlank (the desync test)
 *
 * Everything here runs at the VBlank boundary (plat_state.c, on its service stack) or in plat_pad.c's VBlank (off the
 * EE stack): nothing of it reaches the game's memory but the pads' states. None of its own state is saved: the input
 * rings and the saves outlive every load.
 */
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include "plat_settings.h"
#include "plat_state.h"

/* the C library calls added here, through the GOT (port_host.h: the PLT, and every game function's address, stay) */
#define PORT_NOPLT(f) __typeof__(f) f __attribute__((noplt))
PORT_NOPLT(socket);
PORT_NOPLT(bind);
PORT_NOPLT(connect);
PORT_NOPLT(sendto);
PORT_NOPLT(recvfrom);
PORT_NOPLT(getsockname);
PORT_NOPLT(poll);
PORT_NOPLT(getaddrinfo);
PORT_NOPLT(freeaddrinfo);
PORT_NOPLT(inet_ntop);
PORT_NOPLT(strncmp);
PORT_NOPLT(strchr);
#undef PORT_NOPLT

uint32_t Port_VBlankCount(void); /* plat_hw.c */
void Port_WindowExit(int code);  /* plat_window.c */
void Port_PadLocal(uint32_t frame, int pad, uint16_t *buttons, uint8_t analog[4]); /* plat_pad.c */
void Port_PadRecordNet(uint32_t frame, const uint16_t buttons[2], uint8_t analog[2][4]);
uint64_t Port_StateBuildId(void);                                           /* plat_state.c */
int Port_StateFingerprint(uint64_t *each, int max, uint64_t *total);
const char *Port_StateRegionName(int hashed);
extern int gPortModifiedData; /* plat_data.c */
extern int32_t gRngSpin;      /* src/rng_misc.c: the perturbation of TTT_NET_PERTURB */

int gPortNet; /* 1: a netplay session (set once, from the switches) */

#define VBLANK_NS 16683333u
#define RING 256  /* VBlanks of inputs kept */
#define RB_MAX 12 /* the most VBlanks of rollback */
#define MAX_SEND 64
#define PROTO 1
#define MAGIC 0x314E5454u /* "TTN1" */
#define MAX_HASHED 96

enum { P_HELLO = 1, P_WELCOME, P_REFUSE, P_INPUT, P_REGIONS };

typedef struct {
    uint16_t buttons;
    uint8_t analog[4];
} NetIn;

static const NetIn kIdle = { 0, { 0x80, 0x80, 0x80, 0x80 } };

/* ---------------------------------------------------------------------------------------------- the switches */

static int sDecided, sJoin, sSide; /* sSide: the pad that is ours (the host's is pad 1, the joining side's pad 2) */
static const char *sAddr;
static uint32_t sDelay = 2, sMaxRb = 6, sCheck = 60, sWaitS = 60, sTimeoutS = 5, sPaceFrom = 1, sLimit, sPerturb;
static int sNoDraw, sDesyncGoOn, sTrace;
static uint64_t sTraceT0;
static uint32_t sSimDelay, sSimJitter, sSimLoss;

static uint64_t ns_now(void);

static uint32_t env_u(const char *name, uint32_t dflt, uint32_t max) {
    const char *e = getenv(name);
    uint32_t v;
    if (e == NULL || *e == 0) {
        return dflt;
    }
    v = (uint32_t)strtoul(e, NULL, 0);
    return v > max ? max : v;
}

/* Whether this run is a netplay session (TTT_NET_HOST or TTT_NET_JOIN). Read when first asked (plat_hw.c's clock asks
   before the constructors of this file may have run). */
int Port_NetOn(void) {
    if (!sDecided) {
        const char *h = getenv("TTT_NET_HOST"), *j = getenv("TTT_NET_JOIN");
        sDecided = 1;
        if (j != NULL && *j) {
            sJoin = 1;
            sSide = 1;
            sAddr = j;
            gPortNet = 1;
        } else if (h != NULL && *h) {
            sAddr = h;
            gPortNet = 1;
        }
    }
    return gPortNet;
}

__attribute__((constructor(102))) static void net_decide(void) { Port_NetOn(); }

/* A window by default in a session, unless it is a TTT_FRAMES run (tests): plat_window.c. */
int Port_NetWindow(void) {
    const char *f = getenv("TTT_FRAMES");
    return Port_NetOn() && (f == NULL || *f == 0);
}

int Port_NetSide(void) { return sSide; }

static void read_switches(void) {
    const char *e;
    sDelay = env_u("TTT_NET_DELAY", 2, 30);
    sMaxRb = env_u("TTT_NET_ROLLBACK", 6, RB_MAX);
    sCheck = env_u("TTT_NET_CHECK", 60, 100000);
    sWaitS = env_u("TTT_NET_WAIT", 60, 86400);
    sTimeoutS = env_u("TTT_NET_TIMEOUT", 5, 3600);
    sPaceFrom = env_u("TTT_NET_PACE_FROM", 1, 0xFFFFFFFFu);
    sLimit = env_u("TTT_FRAMES", 0, 0xFFFFFFFFu);
    sPerturb = env_u("TTT_NET_PERTURB", 0, 0xFFFFFFFFu);
    sTrace = getenv("TTT_NET_TRACE") != NULL;
    sTraceT0 = ns_now();
    e = getenv("TTT_NET_DRAW");
    sNoDraw = e != NULL && strcmp(e, "0") == 0;
    e = getenv("TTT_NET_DESYNC");
    sDesyncGoOn = e != NULL && strcmp(e, "continue") == 0;
    if ((e = getenv("TTT_NET_SIM")) != NULL) { /* delay:<ms>,jitter:<ms>,loss:<%> */
        while (*e) {
            const char *c = strchr(e, ':');
            char *end;
            uint32_t v;
            if (c == NULL) {
                break;
            }
            v = (uint32_t)strtoul(c + 1, &end, 10);
            if (strncmp(e, "delay", 5) == 0) {
                sSimDelay = v;
            } else if (strncmp(e, "jitter", 6) == 0) {
                sSimJitter = v;
            } else if (strncmp(e, "loss", 4) == 0) {
                sSimLoss = v > 100 ? 100 : v;
            }
            e = *end == ',' ? end + 1 : end;
            if (*end != ',' ) {
                break;
            }
        }
    }
}

/* ---------------------------------------------------------------------------------------------- time */

static uint64_t ns_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void sleep_ns(uint64_t ns) {
    struct timespec ts;
    ts.tv_sec = (time_t)(ns / 1000000000ull);
    ts.tv_nsec = (long)(ns % 1000000000ull);
    nanosleep(&ts, NULL);
}

/* ---------------------------------------------------------------------------------------------- the inputs */

static NetIn sLocal[RING], sRemote[RING], sUsed[RING];
static uint32_t sLocalTag[RING], sRemoteTag[RING], sUsedTag[RING];
static uint32_t sLocalTop;  /* the last VBlank whose local state is known */
static uint32_t sRemoteTop; /* every peer state up to this VBlank has arrived */
static uint32_t sPeerAck;   /* the peer has every local state up to this VBlank */
static uint32_t sMispredict; /* the first VBlank whose prediction proved wrong (0: none) */
static uint32_t sRecordTop;  /* TTT_INPUT_RECORD: the VBlanks written so far */
static int sStarted;

static int same(const NetIn *a, const NetIn *b) {
    return a->buttons == b->buttons && memcmp(a->analog, b->analog, 4) == 0;
}

/* The VBlanks up to delay + 1 are idle on both sides: their states were sampled before the session began. */
static uint32_t idle_until(void) { return sDelay + 1; }

static NetIn local_at(uint32_t n) {
    if (n <= idle_until() || sLocalTag[n % RING] != n) {
        return kIdle;
    }
    return sLocal[n % RING];
}

static NetIn remote_at(uint32_t n) {
    if (n <= idle_until()) {
        return kIdle;
    }
    if (sRemoteTag[n % RING] == n) {
        return sRemote[n % RING];
    }
    return sRemoteTop > idle_until() ? sRemote[sRemoteTop % RING] : kIdle; /* the prediction: the last one again */
}

static void sample_local(uint32_t f) {
    NetIn in = kIdle;
    if (f <= idle_until() || sLocalTag[f % RING] == f) {
        return;
    }
    Port_PadLocal(f, sSide, &in.buttons, in.analog);
    sLocal[f % RING] = in;
    sLocalTag[f % RING] = f;
    sLocalTop = f;
}

static void record_confirmed(void) {
    while (sRecordTop < sRemoteTop && sRecordTop < sLocalTop) {
        uint32_t f = ++sRecordTop;
        NetIn l = local_at(f), r = remote_at(f);
        uint16_t b[2];
        uint8_t a[2][4];
        b[sSide] = l.buttons;
        memcpy(a[sSide], l.analog, 4);
        b[1 - sSide] = r.buttons;
        memcpy(a[1 - sSide], r.analog, 4);
        Port_PadRecordNet(f, b, a);
    }
}

/* plat_pad.c, at VBlank n (off the EE stack): both pads' states for the game. The first time through a VBlank the
   local pad is sampled for VBlank n + delay. */
void Port_NetPads(uint32_t n, uint16_t buttons[2], uint8_t analog[2][4]) {
    NetIn l, r;
    if (!gPortResim && sStarted && n >= 2) {
        sample_local(n + sDelay);
    }
    l = local_at(n);
    r = remote_at(n);
    sUsed[n % RING] = r;
    sUsedTag[n % RING] = n;
    buttons[sSide] = l.buttons;
    memcpy(analog[sSide], l.analog, 4);
    buttons[1 - sSide] = r.buttons;
    memcpy(analog[1 - sSide], r.analog, 4);
}

/* ---------------------------------------------------------------------------------------------- the socket */

static int sSock = -1;
static struct sockaddr_storage sPeer;
static socklen_t sPeerLen;
static uint32_t sSession;
static uint64_t sHeard; /* the last packet from the peer (ns) */

/* TTT_NET_SIM: this side's packets held back (delay, jitter) or dropped (loss). */
#define SIM_MAX 512
#define PKT_MAX 1200
static struct {
    uint64_t at;
    uint32_t len;
    uint8_t data[PKT_MAX];
} sSim[SIM_MAX];
static uint32_t sSimN, sSimRng = 0x9E3779B9u;
static uint64_t sSent, sRecv, sSimDropped, sSendFail;

static uint32_t rng(void) {
    sSimRng ^= sSimRng << 13;
    sSimRng ^= sSimRng >> 17;
    sSimRng ^= sSimRng << 5;
    return sSimRng;
}

static void raw_send(const uint8_t *p, uint32_t n) {
    if (sendto(sSock, p, n, MSG_DONTWAIT, sPeerLen ? (const struct sockaddr *)&sPeer : NULL, sPeerLen) < 0) {
        sSendFail++;
    }
}

static void send_pkt(const uint8_t *p, uint32_t n) {
    sSent++;
    if (sSimDelay == 0 && sSimJitter == 0 && sSimLoss == 0) {
        raw_send(p, n);
        return;
    }
    if (sSimLoss != 0 && rng() % 100u < sSimLoss) {
        sSimDropped++;
        return;
    }
    if (sSimN < SIM_MAX && n <= PKT_MAX) {
        int64_t d = (int64_t)sSimDelay * 1000000;
        if (sSimJitter != 0) {
            d += (int64_t)(rng() % (2u * sSimJitter * 1000u + 1u)) * 1000 - (int64_t)sSimJitter * 1000000;
        }
        sSim[sSimN].at = ns_now() + (uint64_t)(d < 0 ? 0 : d);
        sSim[sSimN].len = n;
        memcpy(sSim[sSimN].data, p, n);
        sSimN++;
    } else {
        sSimDropped++;
    }
}

/* The held packets that are due (jitter reorders them). */
static void sim_flush(void) {
    uint64_t now = ns_now();
    uint32_t i = 0;
    while (i < sSimN) {
        if (sSim[i].at <= now) {
            raw_send(sSim[i].data, sSim[i].len);
            sSim[i] = sSim[--sSimN];
        } else {
            i++;
        }
    }
}

/* little-endian fields */
static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}
static void put64(uint8_t *p, uint64_t v) {
    put32(p, (uint32_t)v);
    put32(p + 4, (uint32_t)(v >> 32));
}
static uint32_t get32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint64_t get64(const uint8_t *p) { return get32(p) | (uint64_t)get32(p + 4) << 32; }

/* header: magic, type, side, 2 spare, session */
#define HDR 12
static void header(uint8_t *p, int type) {
    put32(p, MAGIC);
    p[4] = (uint8_t)type;
    p[5] = (uint8_t)sSide;
    p[6] = p[7] = 0;
    put32(p + 8, sSession);
}

static void fatal(int code, const char *msg);
static char sMsg[192];
#define FATAL(code, ...) (snprintf(sMsg, sizeof sMsg, __VA_ARGS__), fatal(code, sMsg))

static void addr_text(const struct sockaddr_storage *s, char *out, int n) {
    char ip[64] = "?";
    unsigned port = 0;
    if (s->ss_family == AF_INET) {
        const struct sockaddr_in *a = (const struct sockaddr_in *)s;
        inet_ntop(AF_INET, &a->sin_addr, ip, sizeof ip);
        port = (unsigned)((const uint8_t *)&a->sin_port)[0] << 8 | ((const uint8_t *)&a->sin_port)[1];
    } else if (s->ss_family == AF_INET6) {
        const struct sockaddr_in6 *a = (const struct sockaddr_in6 *)s;
        inet_ntop(AF_INET6, &a->sin6_addr, ip, sizeof ip);
        port = (unsigned)((const uint8_t *)&a->sin6_port)[0] << 8 | ((const uint8_t *)&a->sin6_port)[1];
    }
    snprintf(out, (size_t)n, s->ss_family == AF_INET6 ? "[%s]:%u" : "%s:%u", ip, port);
}

/* "<addr>:<port>", "[<v6 addr>]:<port>" or (host only) "<port>" -> an address. */
static int resolve(const char *spec, int passive, struct sockaddr_storage *out, socklen_t *len) {
    char host[256], port[16];
    const char *colon;
    struct addrinfo hints, *res = NULL;
    size_t hn;
    if (spec[0] == '[' && (colon = strchr(spec, ']')) != NULL && colon[1] == ':') {
        hn = (size_t)(colon - spec - 1);
        memcpy(host, spec + 1, hn < sizeof host ? hn : sizeof host - 1);
        host[hn < sizeof host ? hn : sizeof host - 1] = 0;
        colon++;
    } else if ((colon = strchr(spec, ':')) != NULL) {
        hn = (size_t)(colon - spec);
        memcpy(host, spec, hn < sizeof host ? hn : sizeof host - 1);
        host[hn < sizeof host ? hn : sizeof host - 1] = 0;
    } else if (passive) {
        snprintf(host, sizeof host, "127.0.0.1"); /* a port alone: loopback only, never every interface */
        colon = spec - 1;
    } else {
        return -1;
    }
    snprintf(port, sizeof port, "%s", colon + 1);
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = passive ? AI_PASSIVE : 0;
    if (getaddrinfo(host, port, &hints, &res) != 0 || res == NULL) {
        return -1;
    }
    memcpy(out, res->ai_addr, res->ai_addrlen);
    *len = (socklen_t)res->ai_addrlen;
    freeaddrinfo(res);
    return 0;
}

/* ---------------------------------------------------------------------------------------------- statistics */

static uint64_t sRollbacks, sResimFrames, sRbMaxDepth, sRbNs, sRbMaxNs, sRbStart;
static uint64_t sStalls, sStallNs, sStallMaxNs, sPaceWaitNs, sSlowNs, sReanchors;
static uint64_t sBusySum, sBusyMax, sBusyN, sBusyOver, sPrevEntry, sPrevWait;
static uint64_t sChecks, sCheckNs, sDesyncs, sSaveFails;
static uint64_t sRttSum, sRttN, sRttMax, sRttMin = ~0ull;
static uint32_t sDepthHist[RB_MAX + 2];
/* the same from TTT_NET_PACE_FROM on (the part played at 1x), with the busy time's distribution in 0.5 ms steps */
#define BUSY_BINS 200
static struct {
    uint64_t n, busySum, busyMax, over, rb, frames, depth, rbNs, stalls, stallNs;
    uint32_t hist[BUSY_BINS];
} sPaced;
static int paced(uint32_t vb);

static uint32_t busy_pct(uint32_t pct) {
    uint64_t want = (sPaced.n * pct + 99) / 100, seen = 0;
    uint32_t i;
    for (i = 0; i < BUSY_BINS; i++) {
        seen += sPaced.hist[i];
        if (seen >= want) {
            return i * 500u + 250u;
        }
    }
    return BUSY_BINS * 500u;
}

static void sim_drain(void);

void Port_NetReport(void) {
    if (!gPortNet) {
        return;
    }
    sim_drain();
    record_confirmed(); /* (TTT_INPUT_RECORD: the last VBlanks) */
    printf("ttt: net: %s, pad %d, delay %u, rollback %u; vblank %u, peer inputs to %u; %llu packets sent, %llu received, "
           "%llu dropped on purpose%s\n",
           sJoin ? "joined" : "host", sSide + 1, (unsigned)sDelay, (unsigned)sMaxRb, (unsigned)Port_VBlankCount(),
           (unsigned)sRemoteTop, (unsigned long long)sSent, (unsigned long long)sRecv, (unsigned long long)sSimDropped,
           sNoDraw ? ", undrawn" : "");
    printf("ttt: net: rollbacks %llu, frames run again %llu, max depth %llu, depths 1-%u:", (unsigned long long)sRollbacks,
           (unsigned long long)sResimFrames, (unsigned long long)sRbMaxDepth, RB_MAX);
    {
        int i;
        for (i = 1; i <= RB_MAX; i++) {
            printf(" %u", (unsigned)sDepthHist[i]);
        }
    }
    printf("; rollback mean %u us, max %u us\n", (unsigned)(sRollbacks ? sRbNs / sRollbacks / 1000 : 0),
           (unsigned)(sRbMaxNs / 1000));
    printf("ttt: net: per vblank busy mean %u us, max %u us, %llu of %llu over %u us; waits: %llu stalls (%u ms, max "
           "%u ms), pacing %u ms, slowed %u ms, %llu re-anchored\n",
           (unsigned)(sBusyN ? sBusySum / sBusyN / 1000 : 0), (unsigned)(sBusyMax / 1000),
           (unsigned long long)sBusyOver, (unsigned long long)sBusyN, VBLANK_NS / 1000, (unsigned long long)sStalls,
           (unsigned)(sStallNs / 1000000), (unsigned)(sStallMaxNs / 1000000), (unsigned)(sPaceWaitNs / 1000000),
           (unsigned)(sSlowNs / 1000000), (unsigned long long)sReanchors);
    printf("ttt: net: %llu checks (fingerprint mean %u us), %llu desyncs, %llu saves refused; rtt min %u us, mean %u us, "
           "max %u us\n",
           (unsigned long long)sChecks, (unsigned)(sChecks ? sCheckNs / sChecks / 1000 : 0),
           (unsigned long long)sDesyncs, (unsigned long long)sSaveFails,
           (unsigned)(sRttN ? sRttMin / 1000 : 0), (unsigned)(sRttN ? sRttSum / sRttN / 1000 : 0),
           (unsigned)(sRttMax / 1000));
    if (sPaced.n != 0) {
        printf("ttt: net: paced from vblank %u: %llu vblanks, busy mean %u us, p50 %u us, p95 %u us, max %u us, %llu over "
               "%u us; %llu rollbacks (%llu frames, max depth %llu, mean %u us); %llu stalls (%u ms)\n",
               (unsigned)sPaceFrom, (unsigned long long)sPaced.n, (unsigned)(sPaced.busySum / sPaced.n / 1000),
               (unsigned)busy_pct(50), (unsigned)busy_pct(95), (unsigned)(sPaced.busyMax / 1000),
               (unsigned long long)sPaced.over, VBLANK_NS / 1000, (unsigned long long)sPaced.rb,
               (unsigned long long)sPaced.frames, (unsigned long long)sPaced.depth,
               (unsigned)(sPaced.rb ? sPaced.rbNs / sPaced.rb / 1000 : 0), (unsigned long long)sPaced.stalls,
               (unsigned)(sPaced.stallNs / 1000000));
    }
    fflush(stdout);
}

/* At the end (TTT_NET_SIM): what this side sent and still holds goes out first. */
static void sim_drain(void) {
    uint64_t end = ns_now() + 500000000ull;
    while (sSimN != 0 && ns_now() < end) {
        sim_flush();
        sleep_ns(1000000u);
    }
}

static void fatal(int code, const char *msg) {
    printf("ttt: net: %s\n", msg);
    Port_NetReport();
    Port_WindowExit(code);
}

/* ---------------------------------------------------------------------------------------------- the checks */

/* Our fingerprints of the last few checked VBlanks (final: every input up to it arrived and was the one used), and
   the peer's totals that came before ours. */
#define CHECKS 8
static struct {
    uint32_t vb;
    uint64_t total;
    int n, final;
    uint64_t each[MAX_HASHED];
} sMine[CHECKS];
static struct {
    uint32_t vb;
    uint64_t total;
} sTheirs[CHECKS];
static uint32_t sLastCheck;            /* the last VBlank hashed */
static uint32_t sDesyncVb;             /* the VBlank of a desync being reported */
static int sRegionsShown;

static void send_regions(void);

static void desync(uint32_t vb, uint64_t mine, uint64_t theirs) {
    if (sDesyncVb != 0) {
        return;
    }
    sDesyncs++;
    sDesyncVb = vb;
    printf("ttt: net: desync at vblank %u: state hash %016llx here, %016llx at the peer\n", (unsigned)vb,
           (unsigned long long)mine, (unsigned long long)theirs);
    fflush(stdout);
    send_regions();
}

static void compare_check(uint32_t vb) {
    int i, j;
    for (i = 0; i < CHECKS; i++) {
        if (sMine[i].vb != vb || !sMine[i].final) {
            continue;
        }
        for (j = 0; j < CHECKS; j++) {
            if (sTheirs[j].vb == vb) {
                if (sTheirs[j].total != sMine[i].total) {
                    desync(vb, sMine[i].total, sTheirs[j].total);
                }
                sTheirs[j].vb = 0;
            }
        }
    }
}

static uint32_t sTheirsSeen; /* the last check of the peer's taken (every packet repeats it) */
static void got_check(uint32_t vb, uint64_t total) {
    if (vb == 0 || vb <= sTheirsSeen) {
        return;
    }
    sTheirsSeen = vb;
    sTheirs[vb / (sCheck ? sCheck : 1) % CHECKS].vb = vb;
    sTheirs[vb / (sCheck ? sCheck : 1) % CHECKS].total = total;
    compare_check(vb);
}

/* At every pass through a checked VBlank's boundary (the first and each one run again): its fingerprint, tentative
   until the inputs it was made with are known to be the peer's (finalize). */
static void check_at(uint32_t vb) {
    uint64_t t0;
    int k;
    if (sCheck == 0 || vb % sCheck != 0 || vb <= sLastCheck) {
        return;
    }
    t0 = ns_now();
    k = (int)(vb / sCheck % CHECKS);
    sMine[k].vb = vb;
    sMine[k].final = 0;
    sMine[k].n = Port_StateFingerprint(sMine[k].each, MAX_HASHED, &sMine[k].total);
    sChecks++;
    sCheckNs += ns_now() - t0;
}

/* After the inputs that arrived were dealt with (no prediction pending up to the VBlank): every tentative check up to
   the peer's last input is final, sent with the inputs and compared. */
static void finalize(void) {
    int i;
    if (sMispredict != 0) {
        return;
    }
    for (i = 0; i < CHECKS; i++) {
        if (sMine[i].vb != 0 && !sMine[i].final && sMine[i].vb <= sRemoteTop) {
            sMine[i].final = 1;
            sLastCheck = sMine[i].vb > sLastCheck ? sMine[i].vb : sLastCheck;
            compare_check(sMine[i].vb);
        }
    }
}

/* REGIONS: the VBlank, the number of regions, each region's hash (the same build: the same regions in the same order). */
static void send_regions(void) {
    uint8_t p[HDR + 8 + 8 * MAX_HASHED];
    int i;
    for (i = 0; i < CHECKS; i++) {
        if (sMine[i].vb == sDesyncVb && sDesyncVb != 0 && sMine[i].final) {
            int r;
            header(p, P_REGIONS);
            put32(p + HDR, sDesyncVb);
            put32(p + HDR + 4, (uint32_t)sMine[i].n);
            for (r = 0; r < sMine[i].n; r++) {
                put64(p + HDR + 8 + 8 * r, sMine[i].each[r]);
            }
            send_pkt(p, HDR + 8 + 8 * (uint32_t)sMine[i].n);
        }
    }
}

static void got_regions(const uint8_t *p, uint32_t n) {
    uint32_t vb = get32(p), cnt = get32(p + 4), r, shown = 0;
    int i;
    if (sRegionsShown || n < 8 + 8 * cnt) {
        return;
    }
    for (i = 0; i < CHECKS; i++) {
        if (sMine[i].vb == vb && vb != 0 && sMine[i].final && (uint32_t)sMine[i].n == cnt) {
            for (r = 0; r < cnt; r++) {
                uint64_t theirs = get64(p + 8 + 8 * r);
                if (theirs != sMine[i].each[r] && shown++ < 16) {
                    printf("ttt: net: desync at vblank %u: region %s differs (%016llx here, %016llx at the peer)\n",
                           (unsigned)vb, Port_StateRegionName((int)r), (unsigned long long)sMine[i].each[r],
                           (unsigned long long)theirs);
                }
            }
            sRegionsShown = 1;
            fflush(stdout);
            if (sDesyncVb == 0) { /* (theirs came first) */
                sDesyncVb = vb;
                sDesyncs++;
                send_regions();
            }
        }
    }
}

/* ---------------------------------------------------------------------------------------------- the packets */

static int32_t sAdvLocal, sAdvRemote; /* frame advantage, 1/256 VBlank: how far ahead of the other side */
static uint32_t sPeerFrame, sEchoUs;
static uint64_t sEchoRecvNs;
static uint32_t sFrame; /* the VBlank this side is at (first pass) */

static uint32_t us_now(void) { return (uint32_t)(ns_now() / 1000u); }

/* INPUT: frame, ack, advantage, check vblank, check hash, time sent (us), echo (us), held (us), first, count, states */
#define IN_FIXED 40
static void send_inputs(void) {
    uint8_t p[HDR + IN_FIXED + 6 * MAX_SEND];
    uint32_t first = sPeerAck + 1 > idle_until() + 1 ? sPeerAck + 1 : idle_until() + 1, count, i;
    int k = sLastCheck && sCheck ? (int)(sLastCheck / sCheck % CHECKS) : 0;
    if (sLocalTop >= first + MAX_SEND) {
        first = sLocalTop - MAX_SEND + 1;
    }
    count = sLocalTop >= first ? sLocalTop - first + 1 : 0;
    header(p, P_INPUT);
    put32(p + HDR, sFrame);
    put32(p + HDR + 4, sRemoteTop);
    put32(p + HDR + 8, (uint32_t)sAdvLocal);
    put32(p + HDR + 12, sLastCheck && sMine[k].vb == sLastCheck ? sLastCheck : 0);
    put64(p + HDR + 16, sMine[k].total);
    put32(p + HDR + 24, us_now());
    put32(p + HDR + 28, sEchoUs);
    put32(p + HDR + 32, sEchoRecvNs ? (uint32_t)((ns_now() - sEchoRecvNs) / 1000u) : 0);
    put32(p + HDR + 36, first | count << 24);
    for (i = 0; i < count; i++) {
        NetIn in = local_at(first + i);
        uint8_t *q = p + HDR + IN_FIXED + 6 * i;
        q[0] = (uint8_t)in.buttons;
        q[1] = (uint8_t)(in.buttons >> 8);
        memcpy(q + 2, in.analog, 4);
    }
    send_pkt(p, HDR + IN_FIXED + 6 * count);
}

static void got_inputs(const uint8_t *p, uint32_t n) {
    uint32_t frame = get32(p), ack = get32(p + 4), cv = get32(p + 12), fc = get32(p + 36);
    uint32_t first = fc & 0xFFFFFFu, count = fc >> 24, i, echo = get32(p + 28), held = get32(p + 32);
    if (n < IN_FIXED + 6 * count) {
        return;
    }
    if (ack > sPeerAck) {
        sPeerAck = ack;
    }
    if (frame > sPeerFrame) {
        sPeerFrame = frame;
    }
    if (sEchoRecvNs == 0 || (int32_t)(get32(p + 24) - sEchoUs) > 0) { /* (the newest: jitter reorders packets) */
        sAdvRemote = (int32_t)get32(p + 8);
        sEchoUs = get32(p + 24);
        sEchoRecvNs = ns_now();
    }
    if (echo != 0) {
        uint32_t rtt = us_now() - echo - held;
        if (rtt < 10000000u) {
            uint64_t ns = (uint64_t)rtt * 1000u;
            sRttSum += ns;
            sRttN++;
            sRttMax = ns > sRttMax ? ns : sRttMax;
            sRttMin = ns < sRttMin ? ns : sRttMin;
            /* our advantage: our VBlank against the peer's now (its VBlank then plus half the round trip) */
            {
                int32_t est = (int32_t)((frame << 8) + (uint32_t)((ns / 2) * 256u / VBLANK_NS));
                int32_t adv = (int32_t)(sFrame << 8) - est;
                sAdvLocal += (adv - sAdvLocal) / 8;
            }
        }
    }
    got_check(cv, get64(p + 16));
    for (i = 0; i < count; i++) {
        uint32_t f = first + i;
        const uint8_t *q = p + IN_FIXED + 6 * i;
        NetIn in;
        if (f <= sRemoteTop || f > sRemoteTop + RING / 2) {
            continue;
        }
        in.buttons = (uint16_t)(q[0] | q[1] << 8);
        memcpy(in.analog, q + 2, 4);
        if (sRemoteTag[f % RING] != f) {
            sRemote[f % RING] = in;
            sRemoteTag[f % RING] = f;
            if (sUsedTag[f % RING] == f && !same(&sUsed[f % RING], &in) && (sMispredict == 0 || f < sMispredict)) {
                sMispredict = f;
            }
        }
    }
    while (sRemoteTag[(sRemoteTop + 1) % RING] == sRemoteTop + 1) {
        sRemoteTop++;
    }
    record_confirmed();
}

#define TERMS 40
static uint8_t sWelcome[HDR + TERMS]; /* the host's answer, again for each HELLO that comes after it (one was lost) */

/* Everything that has arrived (and TTT_NET_SIM's packets that fell due); waits up to `ms` for the first. */
static void pump(int ms) {
    struct pollfd pf;
    uint8_t p[PKT_MAX + 64];
    sim_flush();
    pf.fd = sSock;
    pf.events = POLLIN;
    pf.revents = 0;
    if (ms > 0 && sSimN != 0 && ms > 1) {
        ms = 1; /* (held packets fall due meanwhile) */
    }
    poll(&pf, 1, ms);
    for (;;) {
        struct sockaddr_storage from;
        socklen_t fl = sizeof from;
        long n = (long)recvfrom(sSock, p, sizeof p, MSG_DONTWAIT, (struct sockaddr *)&from, &fl);
        if (n < 0) {
            break;
        }
        if (n < HDR || get32(p) != MAGIC || get32(p + 8) != sSession || p[5] == sSide) {
            continue;
        }
        sRecv++;
        sHeard = ns_now();
        if (p[4] == P_INPUT) {
            got_inputs(p + HDR, (uint32_t)n - HDR);
        } else if (p[4] == P_REGIONS) {
            got_regions(p + HDR, (uint32_t)n - HDR);
        } else if (p[4] == P_HELLO && !sJoin) {
            raw_send(sWelcome, HDR + TERMS);
        }
    }
    sim_flush();
}

/* ---------------------------------------------------------------------------------------------- the connection */

/* What both sides must share: HELLO / WELCOME carry it. */
typedef struct {
    uint32_t proto, delay, check;
    uint64_t build, boot, settings;
    uint32_t modified;
} Terms;

static uint64_t fnv(uint64_t h, const char *s) {
    for (; s != NULL && *s; s++) {
        h = (h ^ (uint8_t)*s) * 0x100000001B3ull;
    }
    return (h ^ 0xFF) * 0x100000001B3ull;
}

static void my_terms(Terms *t) {
    char cheat[16];
    uint64_t h = 0xCBF29CE484222325ull;
    snprintf(cheat, sizeof cheat, "%d", Settings_GetInt("cheats.unlock_all"));
    h = fnv(h, cheat);
    h = fnv(h, getenv("TTT_SEED"));
    h = fnv(h, getenv("TTT_MC_POLLS"));
    t->proto = PROTO;
    t->delay = sDelay;
    t->check = sCheck;
    t->build = Port_StateBuildId();
    t->boot = Port_StateHash();
    t->settings = h;
    t->modified = (uint32_t)gPortModifiedData;
}

static void put_terms(uint8_t *p, const Terms *t) {
    put32(p, t->proto);
    put32(p + 4, t->delay);
    put32(p + 36, t->check);
    put64(p + 8, t->build);
    put64(p + 16, t->boot);
    put64(p + 24, t->settings);
    put32(p + 32, t->modified);
}

static void get_terms(const uint8_t *p, Terms *t) {
    t->proto = get32(p);
    t->delay = get32(p + 4);
    t->check = get32(p + 36);
    t->build = get64(p + 8);
    t->boot = get64(p + 16);
    t->settings = get64(p + 24);
    t->modified = get32(p + 32);
}

/* Why the two sides cannot play together (NULL: they can). */
static const char *mismatch(const Terms *a, const Terms *b) {
    if (a->proto != b->proto) {
        return "another netplay protocol version";
    }
    if (a->build != b->build) {
        return "another build of the program";
    }
    if (a->modified || b->modified) {
        return "modified game data on one side";
    }
    if (a->settings != b->settings) {
        return "other settings that change the game (cheats.unlock_all, TTT_SEED, TTT_MC_POLLS)";
    }
    if (a->boot != b->boot) {
        return "another state at the first VBlank (other game data or settings)";
    }
    return NULL;
}

static void refuse(const char *why) {
    uint8_t p[HDR + 128];
    size_t n = strlen(why);
    header(p, P_REFUSE);
    if (n > 127) {
        n = 127;
    }
    memcpy(p + HDR, why, n);
    p[HDR + n] = 0;
    raw_send(p, (uint32_t)(HDR + n + 1));
}

static void open_socket(void) {
    struct sockaddr_storage a;
    socklen_t al;
    char text[96];
    if (resolve(sAddr, !sJoin, &a, &al) != 0) {
        fatal(2, sJoin ? "TTT_NET_JOIN: cannot resolve the address (<addr>:<port>)" :
                         "TTT_NET_HOST: cannot resolve the address ([<addr>:]<port>)");
    }
    sSock = socket(a.ss_family, SOCK_DGRAM, 0);
    if (sSock < 0) {
        FATAL(2, "cannot make a UDP socket");
    }
    if (sJoin) {
        if (connect(sSock, (struct sockaddr *)&a, al) != 0) {
            FATAL(2, "cannot reach the host");
        }
        addr_text(&a, text, sizeof text);
        printf("ttt: net: joining %s\n", text);
    } else {
        if (bind(sSock, (struct sockaddr *)&a, al) != 0) {
            FATAL(2, "cannot listen on that port (in use?)");
        }
        al = sizeof a;
        getsockname(sSock, (struct sockaddr *)&a, &al);
        addr_text(&a, text, sizeof text);
        printf("ttt: net: listening on %s\n", text);
    }
    fflush(stdout);
}

/* The host: the first HELLO of a peer with the same terms; its address from then on is the only one answered. */
static void host_wait(const Terms *mine) {
    uint64_t end = ns_now() + (uint64_t)sWaitS * 1000000000ull;
    uint8_t p[PKT_MAX];
    while (ns_now() < end) {
        struct pollfd pf;
        struct sockaddr_storage from;
        socklen_t fl = sizeof from;
        long n;
        Terms theirs;
        const char *why;
        pf.fd = sSock;
        pf.events = POLLIN;
        pf.revents = 0;
        poll(&pf, 1, 100);
        n = (long)recvfrom(sSock, p, sizeof p, MSG_DONTWAIT, (struct sockaddr *)&from, &fl);
        if (n < HDR + TERMS || get32(p) != MAGIC || p[4] != P_HELLO) {
            continue;
        }
        sSession = get32(p + 8);
        memcpy(&sPeer, &from, fl);
        sPeerLen = fl;
        get_terms(p + HDR, &theirs);
        if ((why = mismatch(mine, &theirs)) != NULL) {
            refuse(why);
            printf("ttt: net: refused a peer: %s\n", why);
            fflush(stdout);
            sPeerLen = 0;
            continue;
        }
        if (connect(sSock, (struct sockaddr *)&from, fl) == 0) {
            sPeerLen = 0; /* (connected: sendto without an address) */
        }
        header(sWelcome, P_WELCOME);
        put_terms(sWelcome + HDR, mine);
        raw_send(sWelcome, HDR + TERMS);
        {
            char text[96];
            addr_text(&from, text, sizeof text);
            printf("ttt: net: %s joined\n", text);
            fflush(stdout);
        }
        return;
    }
    FATAL(3, "nobody joined within %u s", (unsigned)sWaitS);
}

/* The joining side: HELLO every 100 ms until WELCOME or REFUSE. */
static void join_wait(const Terms *mine) {
    uint64_t end = ns_now() + (uint64_t)sWaitS * 1000000000ull;
    uint8_t p[PKT_MAX];
    sSession = (uint32_t)ns_now() ^ (uint32_t)(ns_now() >> 32) * 2654435761u;
    sSession |= 1;
    while (ns_now() < end) {
        struct pollfd pf;
        long n;
        header(p, P_HELLO);
        put_terms(p + HDR, mine);
        raw_send(p, HDR + TERMS);
        pf.fd = sSock;
        pf.events = POLLIN;
        pf.revents = 0;
        poll(&pf, 1, 100);
        while ((n = (long)recvfrom(sSock, p, sizeof p, MSG_DONTWAIT, NULL, NULL)) >= 0) {
            if (n < HDR + 1 || get32(p) != MAGIC || get32(p + 8) != sSession) {
                continue;
            }
            if (p[4] == P_REFUSE) {
                p[n < (long)sizeof p ? n : (long)sizeof p - 1] = 0;
                printf("ttt: net: the host refused: %s\n", (const char *)p + HDR);
                fatal(4, "refused");
            }
            if (p[4] == P_WELCOME && n >= HDR + TERMS) {
                Terms theirs;
                const char *why;
                get_terms(p + HDR, &theirs);
                if ((why = mismatch(mine, &theirs)) != NULL) {
                    printf("ttt: net: the host differs: %s\n", why);
                    fatal(4, "refused");
                }
                sDelay = theirs.delay; /* the host's delay and checks for both */
                sCheck = theirs.check;
                return;
            }
        }
    }
    FATAL(3, "no answer from the host within %u s", (unsigned)sWaitS);
}

static void start(void) {
    Terms mine;
    read_switches();
    open_socket();
    my_terms(&mine);
    if (sJoin) {
        join_wait(&mine);
    } else {
        host_wait(&mine);
    }
    sRemoteTop = sPeerAck = sRecordTop = idle_until();
    sLocalTop = idle_until();
    sHeard = ns_now();
    sStarted = 1;
    printf("ttt: net: playing as pad %d, input delay %u, rollback up to %u VBlanks\n", sSide + 1, (unsigned)sDelay,
           (unsigned)sMaxRb);
    fflush(stdout);
}

/* ---------------------------------------------------------------------------------------------- the boundary */

static uint8_t *sSlot[RB_MAX + 1];
static uint32_t sSlotVb[RB_MAX + 1];
static uint32_t sRbTarget;
static uint64_t sPaceT0, sSlowPending;
static uint32_t sPaceVb, sLastSync;
static uint64_t sWaitAcc, sLastSend;

static void save_slot(uint32_t vb) {
    int k = (int)(vb % (sMaxRb + 1));
    if (sSlot[k] == NULL) {
        sSlot[k] = malloc(Port_StateSize());
        if (sSlot[k] == NULL) {
            FATAL(2, "no memory for the rollback states");
        }
        memset(sSlot[k], 0, 64);
    }
    if (sPerturb != 0 && vb == sPerturb) { /* TTT_NET_PERTURB: this side's timeline differs from here on */
        gRngSpin ^= 0x00010000;
    }
    if (Port_StateSave(sSlot[k]) == 0) {
        sSlotVb[k] = vb;
    } else {
        sSlotVb[k] = 0;
        sSaveFails++;
    }
}

/* Loads the latest save before VBlank sMispredict and runs the VBlanks since again (to `vb`). 0: nothing to load. */
static int rollback(uint32_t vb) {
    uint32_t k, lo = vb > sMaxRb ? vb - sMaxRb : 1;
    for (k = sMispredict - 1; k >= lo && k != 0; k--) {
        if (sSlotVb[k % (sMaxRb + 1)] == k) {
            break;
        }
    }
    if (k < lo || k == 0) {
        FATAL(6, "cannot roll back to vblank %u (at %u): no state saved there", sMispredict - 1, vb);
    }
    sRbStart = ns_now();
    if (Port_StateLoad(sSlot[k % (sMaxRb + 1)]) != 0) {
        FATAL(6, "cannot load the state of vblank %u (at %u)", k, vb);
    }
    sMispredict = 0;
    sRbTarget = vb;
    sRollbacks++;
    sResimFrames += vb - k;
    if (paced(vb)) {
        sPaced.rb++;
        sPaced.frames += vb - k;
        sPaced.depth = vb - k > sPaced.depth ? vb - k : sPaced.depth;
    }
    sRbMaxDepth = vb - k > sRbMaxDepth ? vb - k : sRbMaxDepth;
    sDepthHist[vb - k <= RB_MAX ? vb - k : RB_MAX + 1]++;
    gPortResim = 1;
    gPortNoGs = 1;
    return 1;
}

/* What the next VBlank needs before it may run: the peer's inputs no more than `rollback` VBlanks behind; all of them
   when this boundary's state could not be saved (nothing to roll back to) or at the run's last VBlank (TTT_FRAMES). */
static int must_wait(uint32_t vb) {
    uint32_t next = vb + 1;
    if (next > sRemoteTop + sMaxRb) {
        return 1;
    }
    if (sSlotVb[vb % (sMaxRb + 1)] != vb && sRemoteTop < next) {
        return 1;
    }
    if (sLimit != 0 && next >= sLimit && sRemoteTop < sLimit) {
        return 1;
    }
    return 0;
}

static void keep_sending(void) {
    uint64_t now = ns_now();
    if (now - sLastSend > 8000000u) {
        send_inputs();
        sLastSend = now;
    }
}

static int paced(uint32_t vb) { return sPaceFrom != 0 && vb >= sPaceFrom; }

/* Pacing: one VBlank per 1/59.94 s from TTT_NET_PACE_FROM on, plus what time sync asks for. */
static void pace(uint32_t vb) {
    uint64_t target, now, t0;
    if (sPaceFrom == 0 || vb < sPaceFrom) {
        return;
    }
    now = ns_now();
    if (sPaceT0 == 0) {
        sPaceT0 = now;
        sPaceVb = vb;
        sLastSync = vb;
    }
    /* time sync, every 30 VBlanks: half the difference of the two advantages, a little each VBlank */
    if (vb - sLastSync >= 30) {
        int32_t d = (sAdvLocal - sAdvRemote) / 2;
        sLastSync = vb;
        if (d > 256) {
            sSlowPending += (uint64_t)d * VBLANK_NS / 256u;
        }
    }
    if (sSlowPending != 0) {
        uint64_t s = sSlowPending > VBLANK_NS / 8 ? VBLANK_NS / 8 : sSlowPending;
        sSlowPending -= s;
        sSlowNs += s;
        sPaceT0 += s;
    }
    target = sPaceT0 + (uint64_t)(vb - sPaceVb) * VBLANK_NS;
    if (now > target + 2ull * VBLANK_NS) { /* far behind (the host stalled): that time is dropped */
        sPaceT0 += now - target;
        sReanchors++;
        return;
    }
    t0 = now;
    while (now < target) {
        uint64_t left = target - now;
        if (left > 2000000u) {
            pump((int)((left - 1000000u) / 1000000u));
            keep_sending();
        } else {
            sleep_ns(left);
        }
        now = ns_now();
    }
    sPaceWaitNs += now - t0;
    sWaitAcc += now - t0;
    if (sTrace) {
        printf("ttt: net-trace %u t %llu ms waited %u us adv %d/%d (1/256) peer %u\n", (unsigned)vb,
               (unsigned long long)((now - sTraceT0) / 1000000u), (unsigned)((now - t0) / 1000u), (int)sAdvLocal,
               (int)sAdvRemote, (unsigned)sPeerFrame);
    }
}

/* plat_state.c, at every VBlank boundary of a session (service stack). */
void Port_NetBoundary(uint32_t vb) {
    uint64_t now;
    if (!sStarted) {
        start();
        sPrevEntry = ns_now();
    }
    if (gPortResim) {
        if (vb < sRbTarget) {
            save_slot(vb);
            check_at(vb);
            return;
        }
        gPortResim = 0;
        gPortNoGs = sNoDraw;
        now = ns_now() - sRbStart;
        sRbNs += now;
        sRbMaxNs = now > sRbMaxNs ? now : sRbMaxNs;
        if (paced(vb)) {
            sPaced.rbNs += now;
        }
    } else {
        now = ns_now();
        if (vb > 1 && sFrame != 0) { /* the VBlank just run: its time, waits apart */
            uint64_t busy = now - sPrevEntry - (sWaitAcc - sPrevWait);
            sBusySum += busy;
            sBusyN++;
            sBusyMax = busy > sBusyMax ? busy : sBusyMax;
            sBusyOver += busy > VBLANK_NS;
            if (paced(vb - 1)) {
                sPaced.n++;
                sPaced.busySum += busy;
                sPaced.busyMax = busy > sPaced.busyMax ? busy : sPaced.busyMax;
                sPaced.over += busy > VBLANK_NS;
                sPaced.hist[busy / 500000u < BUSY_BINS ? busy / 500000u : BUSY_BINS - 1]++;
            }
        }
        sPrevEntry = now;
        sPrevWait = sWaitAcc;
        sFrame = vb;
        gPortNoGs = sNoDraw;
        pace(vb);
        pump(0);
        if (sMispredict != 0 && sMispredict <= vb && rollback(vb)) {
            return;
        }
    }
    save_slot(vb);
    check_at(vb);
    finalize();
    send_inputs();
    sLastSend = ns_now();
    if (must_wait(vb)) {
        uint64_t t0 = ns_now(), w;
        sStalls++;
        while (must_wait(vb)) {
            uint32_t f;
            for (f = sLocalTop + 1; f <= vb + 1; f++) { /* (input delay 0: the peer may wait for our next one) */
                sample_local(f);
            }
            pump(2);
            keep_sending();
            if (sMispredict != 0 && sMispredict <= vb) {
                w = ns_now() - t0;
                sStallNs += w;
                sWaitAcc += w;
                rollback(vb);
                return;
            }
            if (ns_now() - sHeard > (uint64_t)sTimeoutS * 1000000000ull) {
                FATAL(3, "the peer is gone (nothing for %u s, at vblank %u)", sTimeoutS, vb);
            }
        }
        w = ns_now() - t0;
        sStallNs += w;
        sStallMaxNs = w > sStallMaxNs ? w : sStallMaxNs;
        if (paced(vb)) {
            sPaced.stalls++;
            sPaced.stallNs += w;
        }
        sWaitAcc += w;
    }
    if (sMispredict != 0 && sMispredict <= vb) { /* (arrived with the last pump) */
        rollback(vb);
        return;
    }
    finalize();
    if (sDesyncVb != 0) {
        uint64_t t0 = ns_now(), end = t0 + 1000000000ull, last = 0;
        /* the peer's region hashes for the report, and ours to the peer (a while after theirs came) */
        while ((!sRegionsShown || ns_now() - t0 < 300000000u) && ns_now() < end) {
            pump(5);
            if (ns_now() - last > 50000000u) {
                send_regions();
                last = ns_now();
            }
        }
        if (!sDesyncGoOn) {
            FATAL(5, "stopped: the two sides' states differ (vblank %u, now %u)", sDesyncVb, vb);
        }
        sDesyncVb = 0;
        sRegionsShown = 0;
    }
    if (sLimit != 0 && vb + 1 >= sLimit) { /* the last VBlank: the peer should have all of ours before we go */
        uint64_t end = ns_now() + 1000000000ull;
        while (sPeerAck < sLimit && ns_now() < end) {
            pump(2);
            keep_sending();
        }
    }
}
