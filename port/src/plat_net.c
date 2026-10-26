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
 * Matchmaking (T46, docs/port/matchmaking.md): with TTT_NET_CREATE=1, TTT_NET_ROOM=<code> or TTT_NET_QUICK=1 the
 * peer is found through a rendezvous server (net.server / TTT_NET_SERVER; there is no default one: players host
 * their own, port/server/ttt-server.c). The server pairs two games of the same build (a room's code, or the quick-match
 * queue), tells each the other's address as it sees it, and the two try to reach each other directly (UDP hole
 * punching: PUNCH / PUNCH_ACK to the peer's public and local addresses); when that fails, both send their packets to
 * the server, which relays them. Then the same handshake (HELLO / WELCOME) and session as above, over that path. The
 * room creator (or the first in the queue) is the host. Also here: the chat between the two (CHAT / CHAT_ACK, sent
 * until acknowledged), the session's state for the overlay and the window's title (plat_net.h), and the update check
 * (a manifest from the server, signed with Ed25519: verified here with net.update_key, only ever reported).
 *   TTT_NET_SERVER=<addr>:<port>  the matchmaking server (net.server)
 *   TTT_NET_CREATE=1 / TTT_NET_ROOM=<code> / TTT_NET_QUICK=1  a room of our own, a friend's room, the quick match
 *   TTT_NET_NAME=<name>           the name the peer sees (net.name)
 *   TTT_NET_PUNCH=0               no direct attempt: relayed at once (net.punch)
 *   TTT_NET_PORT=<n>              the local UDP port (default any)
 *   TTT_NET_CHAT=<text>[|<text>]  lines sent as soon as the session is on (tests)
 *   TTT_UPDATE_CHECK=1            ask the server for its update manifest, report, exit (net.update_key)
 *
 * Everything here runs at the VBlank boundary (plat_state.c, on its service stack) or in plat_pad.c's VBlank (off the
 * EE stack): nothing of it reaches the game's memory but the pads' states. None of its own state is saved: the input
 * rings and the saves outlive every load. The window thread only reads the session's state and queues chat lines
 * (plat_net.h, under sShareLock).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "port_host.h"
#include "plat_lang.h"
#include "plat_mods.h"
#include "plat_net.h"
#include "plat_os.h" /* BSD sockets (Winsock 2 on Windows) */
#include "plat_settings.h"
#include "plat_state.h"
#include "../server/ttt_match_proto.h"
#include "../../third_party/tweetnacl/ed25519.h"

/* the C library calls added here, through the GOT (port_host.h: the PLT, and every game function's address, stay;
   plat_os.h declares its own) */
PORT_NOPLT(socket);
PORT_NOPLT(bind);
PORT_NOPLT(connect);
PORT_NOPLT(sendto);
PORT_NOPLT(recvfrom);
PORT_NOPLT(getsockname);
PORT_NOPLT(getaddrinfo);
PORT_NOPLT(freeaddrinfo);
PORT_NOPLT(inet_ntop);
PORT_NOPLT(strncmp);
PORT_NOPLT(strchr);
PORT_NOPLT(strtoull);

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

enum { P_HELLO = 1, P_WELCOME, P_REFUSE, P_INPUT, P_REGIONS, P_CHAT, P_CHAT_ACK, P_PUNCH, P_PUNCH_ACK };

typedef struct {
    uint16_t buttons;
    uint8_t analog[4];
} NetIn;

static const NetIn kIdle = { 0, { 0x80, 0x80, 0x80, 0x80 } };

/* ---------------------------------------------------------------------------------------------- the switches */

static int sDecided, sJoin, sSide; /* sSide: the pad that is ours (the host's is pad 1, the joining side's pad 2) */
static int sMatch;                 /* T46: MS_REQ_CREATE / MS_REQ_JOIN / MS_REQ_QUICK through the server, 0: none */
static const char *sAddr;
static uint32_t sDelay = 2, sMaxRb = 6, sCheck = 60, sWaitS = 60, sTimeoutS = 5, sPaceFrom = 1, sLimit, sPerturb;
static int sNoDraw, sDesyncGoOn, sTrace;
static int sPunch = 1;          /* T46: try a direct path first (net.punch) */
static uint32_t sPunchMs = 2000; /* for that long */
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
        const char *h = getenv("TTT_NET_HOST"), *j = getenv("TTT_NET_JOIN"), *c = getenv("TTT_NET_CREATE");
        const char *r = getenv("TTT_NET_ROOM"), *q = getenv("TTT_NET_QUICK");
        sDecided = 1;
        if (c != NULL && *c && strcmp(c, "0") != 0) { /* matchmaking (T46): the side is known once paired */
            sMatch = MS_REQ_CREATE;
            gPortNet = 1;
        } else if (r != NULL && *r) {
            sMatch = MS_REQ_JOIN;
            sAddr = r; /* (the code) */
            gPortNet = 1;
        } else if (q != NULL && *q && strcmp(q, "0") != 0) {
            sMatch = MS_REQ_QUICK;
            gPortNet = 1;
        } else if (j != NULL && *j) {
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
    sWaitS = env_u("TTT_NET_WAIT", sMatch == MS_REQ_JOIN || sMatch == 0 ? 60 : 600, 86400);
    sTimeoutS = env_u("TTT_NET_TIMEOUT", 5, 3600);
    sPaceFrom = env_u("TTT_NET_PACE_FROM", 1, 0xFFFFFFFFu);
    sLimit = env_u("TTT_FRAMES", 0, 0xFFFFFFFFu);
    sPerturb = env_u("TTT_NET_PERTURB", 0, 0xFFFFFFFFu);
    sTrace = getenv("TTT_NET_TRACE") != NULL;
    sPunch = Settings_GetInt("net.punch") != 0;
    sPunchMs = env_u("TTT_NET_PUNCH_MS", 2000, 30000);
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

static PlatOsSock sSock = PLAT_SOCK_NONE;
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
    if (sendto(sSock, p, n, PLAT_MSG_DONTWAIT, sPeerLen ? (const struct sockaddr *)&sPeer : NULL, sPeerLen) < 0) {
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
static uint64_t sRttSum, sRttN, sRttMax, sRttMin = ~0ull, sRttSmooth;
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
static void ms_leave(void);
static void report_session(void);

void Port_NetReport(void) {
    if (!gPortNet) {
        return;
    }
    sim_drain();
    ms_leave();
    report_session();
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

static void chat_tick(void);

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
    chat_tick();
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
            sRttSmooth = sRttSmooth == 0 ? ns : sRttSmooth + ((int64_t)ns - (int64_t)sRttSmooth) / 8;
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
static void got_chat(const uint8_t *p, uint32_t n);
static void got_chat_ack(const uint8_t *p, uint32_t n);
#define MS_MODS 96 /* T49: the sender's mods (Mods_Summary), information for the player only: never compared, never refused */
#define HELLO_SIZE (HDR + TERMS + MS_NAME + MS_MODS)
static uint8_t sWelcome[HELLO_SIZE]; /* the host's answer, again for each HELLO that comes after it (one was lost) */

/* Everything that has arrived (and TTT_NET_SIM's packets that fell due); waits up to `ms` for the first. */
static void pump(int ms) {
    uint8_t p[PKT_MAX + 64];
    sim_flush();
    if (ms > 0 && sSimN != 0 && ms > 1) {
        ms = 1; /* (held packets fall due meanwhile) */
    }
    PlatOs_SockWait(sSock, ms);
    for (;;) {
        struct sockaddr_storage from;
        socklen_t fl = sizeof from;
        long n = (long)recvfrom(sSock, p, sizeof p, PLAT_MSG_DONTWAIT, (struct sockaddr *)&from, &fl);
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
            raw_send(sWelcome, HELLO_SIZE);
        } else if (p[4] == P_CHAT) {
            got_chat(p + HDR, (uint32_t)n - HDR);
        } else if (p[4] == P_CHAT_ACK) {
            got_chat_ack(p + HDR, (uint32_t)n - HDR);
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

/* ---------------------------------------------------------------------------------------------- shared state (T46) */

/* What the window thread sees (plat_net.h): the session's state, the chat. Under sShareLock. */
#define CHAT_LINES 32
#define CHAT_TEXT 160
#define CHAT_OUT 16
static pthread_mutex_t sShareLock = PTHREAD_MUTEX_INITIALIZER;
static PortNetInfo sInfo;
static struct {
    int mine;
    char text[CHAT_TEXT];
} sChatLog[CHAT_LINES];
static uint32_t sChatLogN;
static uint64_t sChatLastIn;            /* when the last line came (ns) */
static char sChatOut[CHAT_OUT][CHAT_TEXT];
static uint32_t sChatQueued, sChatAcked; /* lines queued so far (line k at sChatOut[(k - 1) % CHAT_OUT]), acknowledged */
static uint32_t sChatGot;                /* the peer's lines taken, in order */
static uint64_t sChatSentNs;
static int sPath, sPhase;
static char sCode[MS_CODE], sPeerName[MS_NAME], sMyName[MS_NAME];
static char sMyMods[MS_MODS], sPeerMods[MS_MODS]; /* Mods_Summary's lines, this side's and the peer's ("" unknown) */
static int sUpdate;
static char sUpdateVersion[48];

static void share_update(void) {
    pthread_mutex_lock(&sShareLock);
    sInfo.phase = sPhase;
    sInfo.side = sSide;
    sInfo.path = sPath;
    memcpy(sInfo.code, sCode, sizeof sInfo.code);
    sInfo.code[sizeof sInfo.code - 1] = 0;
    snprintf(sInfo.peer, sizeof sInfo.peer, "%s", sPeerName);
    snprintf(sInfo.mods, sizeof sInfo.mods, "%s", sMyMods);
    snprintf(sInfo.peerMods, sizeof sInfo.peerMods, "%s", sPeerMods);
    sInfo.rttMs = (uint32_t)((sRttSmooth + 500000u) / 1000000u);
    sInfo.delay = sDelay;
    sInfo.rollbacks = (uint32_t)sRollbacks;
    sInfo.framesAgain = (uint32_t)sResimFrames;
    sInfo.stalls = (uint32_t)sStalls;
    sInfo.vblank = sFrame;
    sInfo.update = sUpdate;
    snprintf(sInfo.updateVersion, sizeof sInfo.updateVersion, "%s", sUpdateVersion);
    pthread_mutex_unlock(&sShareLock);
}

static void set_phase(int phase) {
    sPhase = phase;
    share_update();
}

void Port_NetInfo(PortNetInfo *out) {
    pthread_mutex_lock(&sShareLock);
    *out = sInfo;
    pthread_mutex_unlock(&sShareLock);
}

/* (window thread only) */
int Port_NetTitle(char *out, int n) {
    static char last[200];
    char t[200], a[16], b[16];
    PortNetInfo i;
    Port_NetInfo(&i);
    t[0] = 0;
    if (i.phase == NET_PHASE_SERVER) {
        snprintf(t, sizeof t, "%s", Tr("net.title.server", "Netplay: contacting the matchmaking server"));
    } else if (i.phase == NET_PHASE_WAITING && i.code[0]) {
        Lang_Format(t, sizeof t, "net.title.room", "Netplay: room {0}, waiting for a player", i.code, NULL);
    } else if (i.phase == NET_PHASE_WAITING) {
        snprintf(t, sizeof t, "%s", Tr("net.title.queue", "Netplay: waiting for a quick match"));
    } else if (i.phase == NET_PHASE_CONNECTING) {
        Lang_Format(t, sizeof t, "net.title.connecting", "Netplay: connecting to {0}",
                    i.peer[0] ? i.peer : Tr("net.peer_unknown", "the other player"), NULL);
    } else if (i.phase == NET_PHASE_PLAYING) {
        snprintf(a, sizeof a, "%u", (unsigned)i.rttMs);
        snprintf(b, sizeof b, "%u", (unsigned)i.rollbacks);
        Lang_Format(t, sizeof t, "net.title.playing", "Netplay with {0} ({1}): {2} ms, {3} rollbacks",
                    i.peer[0] ? i.peer : Tr("net.peer_unknown", "the other player"),
                    i.path == NET_PATH_RELAY ? Tr("net.path.relay", "relayed") : Tr("net.path.direct", "direct"), a, b,
                    NULL);
    }
    snprintf(out, (size_t)n, "%s", t);
    if (strcmp(t, last) == 0) {
        return 0;
    }
    snprintf(last, sizeof last, "%s", t);
    return 1;
}

/* A line of text as shown: control characters become spaces, at most CHAT_TEXT - 1 bytes (whole UTF-8 sequences). */
static void clean_text(char *out, const char *in, uint32_t n) {
    uint32_t i, k = 0;
    for (i = 0; i < n && in[i] != 0 && k < CHAT_TEXT - 1; i++) {
        uint8_t c = (uint8_t)in[i];
        out[k++] = c < 0x20 || c == 0x7F ? ' ' : (char)c;
    }
    if (k > 0 && ((uint8_t)out[k - 1] & 0x80) != 0) { /* the last UTF-8 sequence cut short: dropped */
        uint32_t j = k - 1, want;
        while (j > 0 && k - j < 4 && ((uint8_t)out[j] & 0xC0) == 0x80) {
            j--;
        }
        want = (uint8_t)out[j] >= 0xF0 ? 4 : (uint8_t)out[j] >= 0xE0 ? 3 : (uint8_t)out[j] >= 0xC0 ? 2 : 0;
        if (want == 0 || k - j < want) {
            k = j;
        }
    }
    out[k] = 0;
}

static void chat_log(int mine, const char *text) {
    snprintf(sChatLog[sChatLogN % CHAT_LINES].text, CHAT_TEXT, "%s", text);
    sChatLog[sChatLogN % CHAT_LINES].mine = mine;
    sChatLogN++;
}

int Port_NetChatSend(const char *text) {
    char t[CHAT_TEXT];
    int ok = 0;
    const char *c;
    clean_text(t, text, (uint32_t)strlen(text));
    for (c = t; *c == ' '; c++) {
    }
    if (!gPortNet || *c == 0) {
        return 0;
    }
    pthread_mutex_lock(&sShareLock);
    if (sChatQueued - sChatAcked < CHAT_OUT) {
        sChatQueued++;
        snprintf(sChatOut[(sChatQueued - 1) % CHAT_OUT], CHAT_TEXT, "%s", t);
        chat_log(1, t);
        ok = 1;
    }
    pthread_mutex_unlock(&sShareLock);
    if (ok) {
        printf("ttt: net: chat (you): %s\n", t);
        fflush(stdout);
    }
    return ok;
}

int Port_NetChatCount(void) {
    int n;
    pthread_mutex_lock(&sShareLock);
    n = (int)(sChatLogN < CHAT_LINES ? sChatLogN : CHAT_LINES);
    pthread_mutex_unlock(&sShareLock);
    return n;
}

int Port_NetChatLine(int i, int *mine, char *text, int n) {
    int ok = 0;
    pthread_mutex_lock(&sShareLock);
    {
        uint32_t have = sChatLogN < CHAT_LINES ? sChatLogN : CHAT_LINES;
        if (i >= 0 && (uint32_t)i < have) {
            uint32_t k = (sChatLogN - have + (uint32_t)i) % CHAT_LINES;
            *mine = sChatLog[k].mine;
            snprintf(text, (size_t)n, "%s", sChatLog[k].text);
            ok = 1;
        }
    }
    pthread_mutex_unlock(&sShareLock);
    return ok;
}

int Port_NetChatRecent(void) {
    uint64_t t;
    pthread_mutex_lock(&sShareLock);
    t = sChatLastIn;
    pthread_mutex_unlock(&sShareLock);
    return t != 0 && ns_now() - t < 10000000000ull;
}

/* CHAT: line number, length, text (UTF-8). The oldest line not acknowledged, every 200 ms until it is. */
static void chat_tick(void) {
    uint8_t p[HDR + 5 + CHAT_TEXT];
    uint32_t n = 0;
    uint64_t now = ns_now();
    pthread_mutex_lock(&sShareLock);
    if (sStarted && sChatAcked < sChatQueued && now - sChatSentNs > 200000000u) {
        const char *t = sChatOut[sChatAcked % CHAT_OUT];
        n = (uint32_t)strlen(t);
        header(p, P_CHAT);
        put32(p + HDR, sChatAcked + 1);
        p[HDR + 4] = (uint8_t)n;
        memcpy(p + HDR + 5, t, n);
        sChatSentNs = now;
    }
    pthread_mutex_unlock(&sShareLock);
    if (n != 0) {
        raw_send(p, HDR + 5 + n);
    }
}

static void got_chat(const uint8_t *p, uint32_t n) {
    uint32_t seq, len;
    uint8_t a[HDR + 4];
    if (n < 5) {
        return;
    }
    seq = get32(p);
    len = p[4];
    if (len > n - 5 || len >= CHAT_TEXT) {
        return;
    }
    if (seq == sChatGot + 1) {
        char t[CHAT_TEXT];
        clean_text(t, (const char *)p + 5, len);
        sChatGot = seq;
        pthread_mutex_lock(&sShareLock);
        chat_log(0, t);
        sChatLastIn = ns_now();
        pthread_mutex_unlock(&sShareLock);
        printf("ttt: net: chat from %s: %s\n", sPeerName[0] ? sPeerName : "the peer", t);
        fflush(stdout);
    }
    if (seq <= sChatGot) {
        header(a, P_CHAT_ACK);
        put32(a + HDR, seq);
        raw_send(a, HDR + 4);
    }
}

static void got_chat_ack(const uint8_t *p, uint32_t n) {
    uint32_t seq;
    if (n < 4) {
        return;
    }
    seq = get32(p);
    pthread_mutex_lock(&sShareLock);
    if (seq == sChatAcked + 1 && seq <= sChatQueued) {
        sChatAcked = seq;
        sChatSentNs = 0; /* (the next one at once) */
    }
    pthread_mutex_unlock(&sShareLock);
}

/* TTT_NET_CHAT: lines (separated by '|') queued at the start (tests). */
static void chat_from_env(void) {
    const char *e = getenv("TTT_NET_CHAT");
    char line[CHAT_TEXT];
    while (e != NULL && *e) {
        const char *bar = strchr(e, '|');
        size_t k = bar != NULL ? (size_t)(bar - e) : strlen(e);
        snprintf(line, sizeof line, "%.*s", (int)(k < sizeof line - 1 ? k : sizeof line - 1), e);
        Port_NetChatSend(line);
        e = bar != NULL ? bar + 1 : NULL;
    }
}

/* ---------------------------------------------------------------------------------------------- matchmaking (T46) */

static struct sockaddr_storage sServer, sLocalAddr;
static socklen_t sServerLen;
static uint32_t sCookie;
static uint8_t sPeerPub[MS_EP], sPeerLocal[MS_EP];
static int sRelayOk;

static int same_addr(const struct sockaddr_storage *a, const struct sockaddr_storage *b) {
    if (a->ss_family != b->ss_family) {
        return 0;
    }
    if (a->ss_family == AF_INET) {
        const struct sockaddr_in *x = (const struct sockaddr_in *)a, *y = (const struct sockaddr_in *)b;
        return x->sin_port == y->sin_port && x->sin_addr.s_addr == y->sin_addr.s_addr;
    }
    if (a->ss_family == AF_INET6) {
        const struct sockaddr_in6 *x = (const struct sockaddr_in6 *)a, *y = (const struct sockaddr_in6 *)b;
        return x->sin6_port == y->sin6_port && memcmp(&x->sin6_addr, &y->sin6_addr, 16) == 0;
    }
    return 0;
}

static int is_loopback(const struct sockaddr_storage *a) {
    if (a->ss_family == AF_INET) {
        return ((const uint8_t *)&((const struct sockaddr_in *)a)->sin_addr)[0] == 127;
    }
    if (a->ss_family == AF_INET6) {
        static const uint8_t one[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
        return memcmp(&((const struct sockaddr_in6 *)a)->sin6_addr, one, 16) == 0;
    }
    return 0;
}

/* sockaddr <-> the protocol's endpoint (ttt_match_proto.h) */
static void put_ep(uint8_t *ep, const struct sockaddr_storage *a) {
    memset(ep, 0, MS_EP);
    if (a->ss_family == AF_INET) {
        const struct sockaddr_in *s = (const struct sockaddr_in *)a;
        const uint8_t *port = (const uint8_t *)&s->sin_port;
        ep[0] = 4;
        ep[2] = port[1];
        ep[3] = port[0];
        memcpy(ep + 4, &s->sin_addr, 4);
    } else if (a->ss_family == AF_INET6) {
        const struct sockaddr_in6 *s = (const struct sockaddr_in6 *)a;
        const uint8_t *port = (const uint8_t *)&s->sin6_port;
        ep[0] = 6;
        ep[2] = port[1];
        ep[3] = port[0];
        memcpy(ep + 4, &s->sin6_addr, 16);
    }
}

/* 1 when the endpoint is an address this socket can send to (its family, not unspecified, a port) */
static int get_ep(const uint8_t *ep, struct sockaddr_storage *a, socklen_t *len) {
    static const uint8_t zero[16];
    memset(a, 0, sizeof *a);
    if (ep[2] == 0 && ep[3] == 0) {
        return 0;
    }
    if (ep[0] == 4 && sLocalAddr.ss_family == AF_INET && memcmp(ep + 4, zero, 4) != 0) {
        struct sockaddr_in *s = (struct sockaddr_in *)a;
        s->sin_family = AF_INET;
        ((uint8_t *)&s->sin_port)[0] = ep[3];
        ((uint8_t *)&s->sin_port)[1] = ep[2];
        memcpy(&s->sin_addr, ep + 4, 4);
        *len = sizeof *s;
        return 1;
    }
    if (ep[0] == 6 && sLocalAddr.ss_family == AF_INET6 && memcmp(ep + 4, zero, 16) != 0) {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)a;
        s->sin6_family = AF_INET6;
        ((uint8_t *)&s->sin6_port)[0] = ep[3];
        ((uint8_t *)&s->sin6_port)[1] = ep[2];
        memcpy(&s->sin6_addr, ep + 4, 16);
        *len = sizeof *s;
        return 1;
    }
    return 0;
}

static uint32_t random32(void) {
    uint32_t r = 0;
    if (PlatOs_Random(&r, sizeof r) != 0) {
        r = (uint32_t)ns_now() * 2654435761u ^ (uint32_t)(ns_now() >> 32);
    }
    return r;
}

static void ms_header(uint8_t *p, int type) {
    put32(p, MS_MAGIC);
    p[4] = (uint8_t)type;
    p[5] = MS_VERSION;
    p[6] = p[7] = 0;
    put32(p + 8, sCookie);
}

static void ms_send(const uint8_t *p, uint32_t n) {
    sendto(sSock, p, n, PLAT_MSG_DONTWAIT, (const struct sockaddr *)&sServer, sServerLen);
}

/* The build id the server pairs by (TTT_NET_TEST_BUILD: another one, the version-mismatch test). */
static uint64_t ms_build(void) {
    const char *e = getenv("TTT_NET_TEST_BUILD");
    return e != NULL && *e ? strtoull(e, NULL, 16) : Port_StateBuildId();
}

static void ms_request(int type) {
    uint8_t p[MS_REQ_SIZE];
    memset(p, 0, sizeof p);
    ms_header(p, type);
    put32(p + MS_RQ_PROTO, PROTO);
    put64(p + MS_RQ_BUILD, ms_build());
    put_ep(p + MS_RQ_LOCAL, &sLocalAddr);
    if (type == MS_REQ_JOIN && sAddr != NULL) {
        int i;
        for (i = 0; i < MS_CODE - 1 && sAddr[i] != 0; i++) {
            char c = sAddr[i];
            p[MS_RQ_CODE + i] = (uint8_t)(c >= 'a' && c <= 'z' ? c - 32 : c);
        }
    }
    memcpy(p + MS_RQ_NAME, sMyName, MS_NAME - 1);
    ms_send(p, sizeof p);
}

/* The server's address (net.server, never a default one) and our socket: on the loopback interface when the server
   is there, else on every interface (TTT_NET_PORT: a fixed port, for a forwarded one). The local address the peer may
   reach us at is the one the system would send to the server from. */
static void open_match_socket(void) {
    char spec[256], text[96], name[64];
    struct sockaddr_storage b;
    socklen_t bl;
    unsigned port = (unsigned)env_u("TTT_NET_PORT", 0, 65535);
    Settings_GetStr("net.server", spec, sizeof spec);
    if (spec[0] == 0) {
        fatal(2, "no matchmaking server: set net.server or TTT_NET_SERVER=<addr>:<port> (none is built in; "
                 "docs/port/matchmaking.md tells how to run one)");
    }
    if (resolve(spec, 0, &sServer, &sServerLen) != 0) {
        FATAL(2, "TTT_NET_SERVER: cannot resolve %s (<addr>:<port>)", spec);
    }
    memset(&b, 0, sizeof b);
    b.ss_family = sServer.ss_family;
    if (sServer.ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)&b;
        ((uint8_t *)&s->sin_port)[0] = (uint8_t)(port >> 8);
        ((uint8_t *)&s->sin_port)[1] = (uint8_t)port;
        if (is_loopback(&sServer)) {
            ((uint8_t *)&s->sin_addr)[0] = 127;
            ((uint8_t *)&s->sin_addr)[3] = 1;
        }
        bl = sizeof *s;
    } else {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&b;
        ((uint8_t *)&s->sin6_port)[0] = (uint8_t)(port >> 8);
        ((uint8_t *)&s->sin6_port)[1] = (uint8_t)port;
        if (is_loopback(&sServer)) {
            ((uint8_t *)&s->sin6_addr)[15] = 1;
        }
        bl = sizeof *s;
    }
    sSock = PlatOs_NetInit() == 0 ? socket(sServer.ss_family, SOCK_DGRAM, 0) : PLAT_SOCK_NONE;
    if (sSock == PLAT_SOCK_NONE || bind(sSock, (struct sockaddr *)&b, bl) != 0) {
        FATAL(2, "cannot open a UDP socket for the matchmaking (TTT_NET_PORT %u in use?)", port);
    }
    bl = sizeof sLocalAddr;
    getsockname(sSock, (struct sockaddr *)&sLocalAddr, &bl);
    if (!is_loopback(&sServer)) { /* the address the system sends to the server from, with our port */
        PlatOsSock t = socket(sServer.ss_family, SOCK_DGRAM, 0);
        struct sockaddr_storage l;
        socklen_t ll = sizeof l;
        if (t != PLAT_SOCK_NONE && connect(t, (struct sockaddr *)&sServer, sServerLen) == 0 &&
            getsockname(t, (struct sockaddr *)&l, &ll) == 0 && l.ss_family == sLocalAddr.ss_family) {
            if (l.ss_family == AF_INET) {
                ((struct sockaddr_in *)&sLocalAddr)->sin_addr = ((struct sockaddr_in *)&l)->sin_addr;
            } else {
                ((struct sockaddr_in6 *)&sLocalAddr)->sin6_addr = ((struct sockaddr_in6 *)&l)->sin6_addr;
            }
        }
        if (t != PLAT_SOCK_NONE) {
            PlatOs_SockClose(t);
        }
    }
    sCookie = random32() | 1u;
    Settings_GetStr("net.name", name, sizeof name);
    clean_text(sMyName, name, MS_NAME - 1);
    sMyName[MS_NAME - 1] = 0;
    if (sMyName[0] == 0) {
        snprintf(sMyName, sizeof sMyName, "Player");
    }
    addr_text(&sServer, text, sizeof text);
    printf("ttt: net: matchmaking server %s\n", text);
    fflush(stdout);
}

/* ---- the update check: the server's manifest, signed (ed25519) with the key in net.update_key; only reported ---- */

static int unhex(const char *s, uint8_t *out, int n) {
    int i;
    for (i = 0; i < 2 * n; i++) {
        char c = s[i];
        int v = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (v < 0) {
            return -1;
        }
        out[i / 2] = (uint8_t)(i % 2 ? out[i / 2] | v : v << 4);
    }
    return s[2 * n] == 0 || s[2 * n] == '\n' || s[2 * n] == '\r' ? 0 : -1;
}

/* A line "<key> <value>" of the manifest's signed part into out ("" when there is none). */
static void manifest_field(const char *m, uint32_t n, const char *key, char *out, int on) {
    uint32_t i, k = (uint32_t)strlen(key);
    out[0] = 0;
    for (i = 0; i + k + 1 <= n; i++) {
        if ((i == 0 || m[i - 1] == '\n') && strncmp(m + i, key, k) == 0 && m[i + k] == ' ') {
            uint32_t j = i + k + 1, e = j;
            while (e < n && m[e] != '\n' && m[e] != '\r') {
                e++;
            }
            clean_text(out, m + j, e - j < (uint32_t)on - 1 ? e - j : (uint32_t)on - 1);
            return;
        }
    }
}

/* The manifest's verdict (NET_UPDATE_*), with its version text, build and url. */
static int manifest_check(const uint8_t *m, uint32_t n, const uint8_t pk[32], char *version, int vn, char *url, int un,
                          uint64_t *build) {
    uint8_t sig[64];
    char b[24];
    uint32_t i, signedLen = n;
    const char *line = NULL;
    for (i = 0; i + 10 <= n; i++) {
        if ((i == 0 || m[i - 1] == '\n') && strncmp((const char *)m + i, "signature ", 10) == 0) {
            signedLen = i;
            line = (const char *)m + i + 10;
            break;
        }
    }
    if (line == NULL || n - signedLen < 10 + 128 || unhex(line, sig, 64) != 0) {
        return NET_UPDATE_BAD_SIGNATURE;
    }
    if (ed25519_verify(sig, m, signedLen, pk) != 0) {
        return NET_UPDATE_BAD_SIGNATURE;
    }
    if (signedLen < 13 || strncmp((const char *)m, "ttt-update 1\n", 13) != 0) {
        return NET_UPDATE_MALFORMED;
    }
    manifest_field((const char *)m, signedLen, "version", version, vn);
    manifest_field((const char *)m, signedLen, "url", url, un);
    manifest_field((const char *)m, signedLen, "build", b, sizeof b);
    if (version[0] == 0 || strlen(b) != 16) {
        return NET_UPDATE_MALFORMED;
    }
    *build = strtoull(b, NULL, 16);
    return *build == Port_StateBuildId() ? NET_UPDATE_LATEST : NET_UPDATE_NEWER;
}

static int update_key(uint8_t pk[32]) {
    char k[160];
    Settings_GetStr("net.update_key", k, sizeof k);
    return k[0] != 0 && unhex(k, pk, 32) == 0;
}

/* Asks the server (sSock, open) for its manifest, for up to `ms`, and reports. The program never fetches or runs
   anything else: a newer build is only named, with the manifest's url as text. */
static int update_check(uint32_t ms) {
    uint8_t pk[32], p[MS_MANIFEST_REQ_SIZE + 64];
    char url[200];
    uint64_t end = ns_now() + (uint64_t)ms * 1000000u, last = 0, build = 0;
    int st = NET_UPDATE_UNAVAILABLE;
    if (!update_key(pk)) {
        printf("ttt: update: no check (net.update_key is not a public key of 64 hex digits)\n");
        return NET_UPDATE_NONE;
    }
    url[0] = 0;
    while (ns_now() < end) {
        struct sockaddr_storage from;
        socklen_t fl = sizeof from;
        long n;
        if (ns_now() - last > 500000000u) {
            memset(p, 0, MS_MANIFEST_REQ_SIZE);
            ms_header(p, MS_REQ_MANIFEST);
            ms_send(p, MS_MANIFEST_REQ_SIZE);
            last = ns_now();
        }
        PlatOs_SockWait(sSock, 50);
        while ((n = (long)recvfrom(sSock, p, sizeof p, PLAT_MSG_DONTWAIT, (struct sockaddr *)&from, &fl)) >= 0) {
            fl = sizeof from;
            if (n < MS_HDR + 2 || get32(p) != MS_MAGIC || get32(p + 8) != sCookie || !same_addr(&from, &sServer)) {
                continue;
            }
            if (p[4] == MS_S_ERROR) {
                end = 0;
                break;
            }
            if (p[4] == MS_S_MANIFEST && (uint32_t)(p[MS_HDR] | p[MS_HDR + 1] << 8) <= (uint32_t)n - MS_HDR - 2) {
                st = manifest_check(p + MS_HDR + 2, (uint32_t)(p[MS_HDR] | p[MS_HDR + 1] << 8), pk, sUpdateVersion,
                                    sizeof sUpdateVersion, url, sizeof url, &build);
                end = 0;
                break;
            }
        }
    }
    printf("ttt: update: this build is %016llx\n", (unsigned long long)Port_StateBuildId());
    if (st == NET_UPDATE_LATEST) {
        printf("ttt: update: this is the newest build (%s)\n", sUpdateVersion);
    } else if (st == NET_UPDATE_NEWER) {
        printf("ttt: update: version %s is available (build %016llx)%s%s; nothing is downloaded: get it from a source "
               "you trust\n", sUpdateVersion, (unsigned long long)build, url[0] ? ": " : "", url);
    } else if (st == NET_UPDATE_BAD_SIGNATURE) {
        sUpdateVersion[0] = 0;
        printf("ttt: update: the server's manifest is not signed with net.update_key: ignored\n");
    } else if (st == NET_UPDATE_MALFORMED) {
        sUpdateVersion[0] = 0;
        printf("ttt: update: the server's manifest is signed but malformed: ignored\n");
    } else {
        printf("ttt: update: the server has no update manifest (or did not answer)\n");
    }
    fflush(stdout);
    sUpdate = st;
    share_update();
    return st;
}

/* Paired by the server: our side, the session's id, the peer's addresses and name. */
static void matchmake(void) {
    uint64_t end, lastReq = 0;
    uint8_t p[512], pk[32];
    int announced = 0;
    open_match_socket();
    set_phase(NET_PHASE_SERVER);
    if (update_key(pk)) {
        update_check(1500);
    }
    end = ns_now() + (uint64_t)sWaitS * 1000000000ull;
    while (ns_now() < end) {
        struct sockaddr_storage from;
        socklen_t fl = sizeof from;
        long n;
        if (ns_now() - lastReq > 500000000u) {
            ms_request(sMatch);
            lastReq = ns_now();
        }
        PlatOs_SockWait(sSock, 50);
        while ((n = (long)recvfrom(sSock, p, sizeof p, PLAT_MSG_DONTWAIT, (struct sockaddr *)&from, &fl)) >= 0) {
            fl = sizeof from;
            if (n < MS_HDR || get32(p) != MS_MAGIC || get32(p + 8) != sCookie || !same_addr(&from, &sServer)) {
                continue;
            }
            if (p[4] == MS_S_ERROR && n >= MS_E_SIZE) {
                char why[80];
                clean_text(why, (const char *)p + MS_E_TEXT, MS_E_SIZE - MS_E_TEXT);
                printf("ttt: net: the server refused: %s\n", why);
                fatal(4, "refused");
            }
            if (p[4] == MS_S_WAIT && n >= MS_W_SIZE && !announced) {
                announced = 1;
                memcpy(sCode, p + MS_W_CODE, MS_CODE);
                sCode[MS_CODE - 1] = 0;
                if (sMatch == MS_REQ_CREATE) {
                    printf("ttt: net: room %s: waiting for a player (give them this code)\n", sCode);
                } else {
                    printf("ttt: net: waiting in the quick-match queue (place %u)\n", (unsigned)get32(p + MS_W_PLACE));
                }
                fflush(stdout);
                set_phase(NET_PHASE_WAITING);
            }
            if (p[4] == MS_S_MATCH && n >= MS_M_SIZE) {
                char a[96], b[96];
                struct sockaddr_storage x;
                socklen_t xl;
                sSide = p[MS_M_ROLE] & 1;
                sJoin = sSide;
                sRelayOk = p[MS_M_RELAY] != 0;
                sSession = get32(p + MS_M_SESSION);
                memcpy(sPeerPub, p + MS_M_PUBLIC, MS_EP);
                memcpy(sPeerLocal, p + MS_M_LOCAL, MS_EP);
                clean_text(sPeerName, (const char *)p + MS_M_NAME, MS_NAME - 1);
                memcpy(sCode, p + MS_M_CODE, MS_CODE);
                sCode[MS_CODE - 1] = 0;
                snprintf(a, sizeof a, "?");
                snprintf(b, sizeof b, "?");
                if (get_ep(sPeerPub, &x, &xl)) {
                    addr_text(&x, a, sizeof a);
                }
                if (get_ep(sPeerLocal, &x, &xl)) {
                    addr_text(&x, b, sizeof b);
                }
                printf("ttt: net: paired with %s (pad %d; at %s, local %s; relay %s)\n", sPeerName, 2 - sSide, a, b,
                       sRelayOk ? "available" : "off");
                fflush(stdout);
                set_phase(NET_PHASE_CONNECTING);
                return;
            }
        }
    }
    FATAL(3, sMatch == MS_REQ_JOIN ? "no answer from the matchmaking server within %u s" :
                                     "nobody came within %u s", (unsigned)sWaitS);
}

/* Where the peer may be: its address as the server saw it, and its local one when that differs. */
static int punch_targets(struct sockaddr_storage t[2], socklen_t tl[2]) {
    int n = 0;
    if (get_ep(sPeerPub, &t[n], &tl[n])) {
        n++;
    }
    if (get_ep(sPeerLocal, &t[n], &tl[n]) && (n == 0 || !same_addr(&t[0], &t[n]))) {
        n++;
    }
    return n;
}

static void punch_send(int type, const struct sockaddr_storage *to, socklen_t tl) {
    uint8_t p[HDR + 4];
    header(p, type);
    put32(p + HDR, sCookie);
    sendto(sSock, p, sizeof p, PLAT_MSG_DONTWAIT, (const struct sockaddr *)to, tl);
}

/* The joining side: PUNCH to the peer's addresses every 50 ms; a PUNCH_ACK proves a path both ways. 1: direct, with
   sPeer that address. */
static int punch_phase(void) {
    struct sockaddr_storage t[2];
    socklen_t tl[2];
    uint64_t end = ns_now() + (uint64_t)sPunchMs * 1000000u, last = 0;
    uint8_t p[PKT_MAX];
    int n = punch_targets(t, tl), i;
    if (!sPunch || n == 0) {
        return 0;
    }
    while (ns_now() < end) {
        struct sockaddr_storage from;
        socklen_t fl = sizeof from;
        long k;
        if (ns_now() - last > 50000000u) {
            for (i = 0; i < n; i++) {
                punch_send(P_PUNCH, &t[i], tl[i]);
            }
            last = ns_now();
        }
        PlatOs_SockWait(sSock, 10);
        while ((k = (long)recvfrom(sSock, p, sizeof p, PLAT_MSG_DONTWAIT, (struct sockaddr *)&from, &fl)) >= 0) {
            socklen_t got = fl;
            fl = sizeof from;
            if (k < HDR || get32(p) != MAGIC || get32(p + 8) != sSession || p[5] == sSide) {
                continue;
            }
            if (p[4] == P_PUNCH) {
                punch_send(P_PUNCH_ACK, &from, got);
            } else if (p[4] == P_PUNCH_ACK) {
                memcpy(&sPeer, &from, got);
                sPeerLen = got;
                return 1;
            }
        }
    }
    return 0;
}

static void open_socket(void) {
    struct sockaddr_storage a;
    socklen_t al;
    char text[96];
    if (resolve(sAddr, !sJoin, &a, &al) != 0) {
        fatal(2, sJoin ? "TTT_NET_JOIN: cannot resolve the address (<addr>:<port>)" :
                         "TTT_NET_HOST: cannot resolve the address ([<addr>:]<port>)");
    }
    sSock = PlatOs_NetInit() == 0 ? socket(a.ss_family, SOCK_DGRAM, 0) : PLAT_SOCK_NONE;
    if (sSock == PLAT_SOCK_NONE) {
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

/* HELLO / WELCOME: the terms, then the sender's name and its mods (T49: shown to the peer, never compared). */
static void put_hello(uint8_t *p, int type, const Terms *t) {
    header(p, type);
    put_terms(p + HDR, t);
    memset(p + HDR + TERMS, 0, MS_NAME + MS_MODS);
    memcpy(p + HDR + TERMS, sMyName, MS_NAME - 1);
    memcpy(p + HDR + TERMS + MS_NAME, sMyMods, MS_MODS - 1);
}

static void got_name(const uint8_t *p, long n) {
    if (n >= HDR + TERMS + MS_NAME && !sMatch) { /* (matchmaking: the server's name for the peer stays) */
        clean_text(sPeerName, (const char *)p + HDR + TERMS, MS_NAME - 1);
    }
    if (n >= HELLO_SIZE) {
        clean_text(sPeerMods, (const char *)p + HDR + TERMS + MS_NAME, MS_MODS - 1);
        printf("ttt: net: the peer's mods: %s\n", sPeerMods[0] ? sPeerMods : "?");
        fflush(stdout);
    }
}

/* The host: the first HELLO of a peer with the same terms; its address from then on is the only one answered.
   Matchmaking: only the session the server made, from the peer directly or relayed by the server; meanwhile PUNCH
   to the peer's addresses (opening our side's NAT) and PUNCH_ACK to its PUNCH. */
static void host_wait(const Terms *mine) {
    uint64_t end = ns_now() + (uint64_t)sWaitS * 1000000000ull, punchEnd = ns_now() + 2ull * sPunchMs * 1000000u;
    uint64_t last = 0;
    struct sockaddr_storage t[2];
    socklen_t tl[2];
    uint8_t p[PKT_MAX];
    int nt = sMatch && sPunch ? punch_targets(t, tl) : 0;
    while (ns_now() < end) {
        struct sockaddr_storage from;
        socklen_t fl = sizeof from;
        long n;
        Terms theirs;
        const char *why;
        int i;
        if (nt != 0 && ns_now() < punchEnd && ns_now() - last > 50000000u) {
            for (i = 0; i < nt; i++) {
                punch_send(P_PUNCH, &t[i], tl[i]);
            }
            last = ns_now();
        }
        PlatOs_SockWait(sSock, sMatch ? 20 : 100);
        while ((n = (long)recvfrom(sSock, p, sizeof p, PLAT_MSG_DONTWAIT, (struct sockaddr *)&from, &fl)) >= 0) {
            socklen_t got = fl;
            fl = sizeof from;
            if (n < HDR || get32(p) != MAGIC || (sMatch && (get32(p + 8) != sSession || p[5] == sSide))) {
                continue;
            }
            if (p[4] == P_PUNCH && sMatch) {
                if (sPunch) {
                    punch_send(P_PUNCH_ACK, &from, got);
                }
                continue;
            }
            if (n < HDR + TERMS || p[4] != P_HELLO) {
                continue;
            }
            sSession = get32(p + 8);
            memcpy(&sPeer, &from, got);
            sPeerLen = got;
            get_terms(p + HDR, &theirs);
            if ((why = mismatch(mine, &theirs)) != NULL) {
                refuse(why);
                printf("ttt: net: refused a peer: %s\n", why);
                fflush(stdout);
                sPeerLen = 0;
                continue;
            }
            got_name(p, n);
            if (connect(sSock, (struct sockaddr *)&from, got) == 0) {
                sPeerLen = 0; /* (connected: sendto without an address) */
            }
            put_hello(sWelcome, P_WELCOME, mine);
            raw_send(sWelcome, HELLO_SIZE);
            {
                char text[96];
                int relayed = sMatch && same_addr(&from, &sServer);
                sPath = !sMatch ? NET_PATH_ADDRESS : relayed ? NET_PATH_RELAY : NET_PATH_DIRECT;
                addr_text(&from, text, sizeof text);
                printf("ttt: net: %s joined%s\n", text, relayed ? " (relayed by the server)" : "");
                fflush(stdout);
            }
            return;
        }
    }
    FATAL(3, "nobody joined within %u s", (unsigned)sWaitS);
}

/* The joining side: HELLO every 100 ms until WELCOME or REFUSE. Matchmaking: to the peer directly (when the punching
   found a path) or through the server's relay, and through the relay after 2 s without an answer directly. */
static void join_wait(const Terms *mine) {
    uint64_t t0 = ns_now(), end = t0 + (uint64_t)sWaitS * 1000000000ull;
    uint8_t p[PKT_MAX];
    if (!sMatch) {
        sSession = (uint32_t)ns_now() ^ (uint32_t)(ns_now() >> 32) * 2654435761u;
        sSession |= 1;
    }
    while (ns_now() < end) {
        struct sockaddr_storage from;
        socklen_t fl = sizeof from;
        long n;
        put_hello(p, P_HELLO, mine);
        raw_send(p, HELLO_SIZE);
        PlatOs_SockWait(sSock, 100);
        while ((n = (long)recvfrom(sSock, p, sizeof p, PLAT_MSG_DONTWAIT, (struct sockaddr *)&from, &fl)) >= 0) {
            socklen_t got = fl;
            fl = sizeof from;
            if (n < HDR + 1 || get32(p) != MAGIC || get32(p + 8) != sSession || (sMatch && p[5] == sSide)) {
                continue;
            }
            if (p[4] == P_PUNCH && sMatch) {
                if (sPunch) {
                    punch_send(P_PUNCH_ACK, &from, got);
                }
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
                got_name(p, n);
                sDelay = theirs.delay; /* the host's delay and checks for both */
                sCheck = theirs.check;
                if (sMatch && connect(sSock, (struct sockaddr *)&from, got) == 0) {
                    sPeerLen = 0; /* (connected: the path WELCOME came by, and nothing else) */
                }
                return;
            }
        }
        if (sMatch && sPath == NET_PATH_DIRECT && sRelayOk && ns_now() - t0 > 2000000000ull) {
            char text[96];
            memcpy(&sPeer, &sServer, sServerLen);
            sPeerLen = sServerLen;
            sPath = NET_PATH_RELAY;
            addr_text(&sServer, text, sizeof text);
            printf("ttt: net: path: no answer directly, relayed by the server %s\n", text);
            fflush(stdout);
        }
    }
    FATAL(3, "no answer from the host within %u s", (unsigned)sWaitS);
}

static void start(void) {
    Terms mine;
    read_switches();
    if (sMatch) {
        matchmake();
        if (sJoin) {
            char text[96];
            if (punch_phase()) {
                sPath = NET_PATH_DIRECT;
                addr_text(&sPeer, text, sizeof text);
                printf("ttt: net: path: direct to %s\n", text);
            } else if (sRelayOk) {
                memcpy(&sPeer, &sServer, sServerLen);
                sPeerLen = sServerLen;
                sPath = NET_PATH_RELAY;
                addr_text(&sServer, text, sizeof text);
                printf("ttt: net: path: relayed by the server %s%s\n", text, sPunch ? " (no direct path)" : "");
            } else {
                fatal(3, "cannot reach the peer directly, and the server has no relay");
            }
            fflush(stdout);
        }
    } else {
        char name[64];
        Settings_GetStr("net.name", name, sizeof name);
        clean_text(sMyName, name, MS_NAME - 1);
        open_socket();
        set_phase(NET_PHASE_WAITING);
    }
    my_terms(&mine);
    Mods_Summary(sMyMods, sizeof sMyMods); /* (cosmetic: not in the terms, so a difference never refuses a peer) */
    printf("ttt: net: mods: %s\n", sMyMods);
    fflush(stdout);
    share_update();
    if (sJoin) {
        join_wait(&mine);
        if (!sMatch) {
            sPath = NET_PATH_ADDRESS;
        }
    } else {
        host_wait(&mine);
    }
    sRemoteTop = sPeerAck = sRecordTop = idle_until();
    sLocalTop = idle_until();
    sHeard = ns_now();
    sStarted = 1;
    set_phase(NET_PHASE_PLAYING);
    printf("ttt: net: playing as pad %d, input delay %u, rollback up to %u VBlanks%s%s\n", sSide + 1, (unsigned)sDelay,
           (unsigned)sMaxRb, sPeerName[0] ? ", against " : "", sPeerName);
    fflush(stdout);
    chat_from_env();
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
        if (vb % 30 == 0) { /* (the overlay's and the title's view) */
            share_update();
        }
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

/* ---------------------------------------------------------------------------------------------- the end (T46) */

/* The server forgets us at once (else a room or a pair expires on its own). */
static void ms_leave(void) {
    uint8_t p[MS_REQ_SIZE];
    if (sServerLen == 0 || sSock == PLAT_SOCK_NONE) {
        return;
    }
    memset(p, 0, sizeof p);
    ms_header(p, MS_REQ_LEAVE);
    put32(p + MS_RQ_PROTO, PROTO);
    ms_send(p, sizeof p);
}

static void report_session(void) {
    static const char *kPath[] = {"none", "address", "direct", "relayed by the server"};
    printf("ttt: net: path %s; peer %s; chat %u sent (%u acknowledged), %u received\n", kPath[sPath & 3],
           sPeerName[0] ? sPeerName : "?", (unsigned)sChatQueued, (unsigned)sChatAcked, (unsigned)sChatGot);
}

/* ---------------------------------------------------------------------------------------------- the overlay's restart */

static char **sArgv;

int Port_NetCanRestart(void) { return sArgv != NULL && !gPortNet; }

int Port_NetRestart(int mode, const char *code) {
    char srv[256];
    if (!Port_NetCanRestart() || mode < 1 || mode > 3 || (mode == 2 && (code == NULL || *code == 0))) {
        return 0;
    }
    Settings_GetStr("net.server", srv, sizeof srv);
    if (srv[0] == 0) {
        return 0;
    }
    PlatOs_SetEnv("TTT_NET_SERVER", srv, 1);
    PlatOs_SetEnv(mode == 1 ? "TTT_NET_CREATE" : mode == 2 ? "TTT_NET_ROOM" : "TTT_NET_QUICK", mode == 2 ? code : "1", 1);
    printf("ttt: net: starting again in a matchmaking session\n");
    fflush(stdout);
    PlatOs_Restart(sArgv); /* (from boot: a session plays the same simulation from the start on both sides) */
    return 0;
}

/* ---------------------------------------------------------------------------------------------- TTT_SELFTEST=net */

static int self_test(void) {
    static const char kSeed[] = "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60";
    static const char kPk[] = "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a";
    static const char kSig[] = "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9"
                               "b46bd25bf5f0595bbe24655141438e7a100b"; /* RFC 8032, 7.1, TEST 1 (the empty message) */
    static const char kAbc[] = "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3"
                               "feebbd454d4423643ce80e2a9ac94fa54ca49f"; /* FIPS 180-2, SHA-512("abc") */
    uint8_t seed[32], pk[32], want[32], sig[64], wsig[64], h[64], wh[64], m[400];
    char version[48], url[64], t[40];
    uint64_t build;
    int fail = 0, n, st;
    unhex(kSeed, seed, 32);
    unhex(kPk, want, 32);
    unhex(kSig, wsig, 64);
    unhex(kAbc, wh, 64);
    ed_sha512(h, (const uint8_t *)"abc", 3);
    fail += memcmp(h, wh, 64) != 0;
    ed25519_public_key(pk, seed);
    fail += memcmp(pk, want, 32) != 0;
    ed25519_sign(sig, (const uint8_t *)"", 0, seed);
    fail += memcmp(sig, wsig, 64) != 0;
    fail += ed25519_verify(wsig, (const uint8_t *)"", 0, pk) != 0;
    wsig[10] ^= 4;
    fail += ed25519_verify(wsig, (const uint8_t *)"", 0, pk) == 0;
    /* a manifest signed here: verified, then refused once a byte changed */
    n = snprintf((char *)m, sizeof m, "ttt-update 1\nversion 9.9\nbuild %016llx\nurl example.invalid\n",
                 (unsigned long long)(Port_StateBuildId() ^ 1));
    ed25519_sign(sig, m, (uint64_t)n, seed);
    n += snprintf((char *)m + n, sizeof m - (size_t)n, "signature ");
    for (st = 0; st < 64; st++) {
        snprintf(t, sizeof t, "%02x", sig[st]);
        m[n++] = (uint8_t)t[0];
        m[n++] = (uint8_t)t[1];
    }
    m[n++] = '\n';
    st = manifest_check(m, (uint32_t)n, pk, version, sizeof version, url, sizeof url, &build);
    fail += st != NET_UPDATE_NEWER || strcmp(version, "9.9") != 0 || strcmp(url, "example.invalid") != 0;
    m[14] ^= 1;
    fail += manifest_check(m, (uint32_t)n, pk, version, sizeof version, url, sizeof url, &build) !=
            NET_UPDATE_BAD_SIGNATURE;
    clean_text(t, "a\tb\xC3\xA9", 4); /* a control character, a cut UTF-8 sequence */
    fail += strcmp(t, "a b") != 0;
    return fail;
}

/* plat_main.c, after the settings, before the window: the self-test and the update check exit here. */
void Port_NetMain(int argc, char **argv) {
    const char *t = getenv("TTT_SELFTEST"), *u = getenv("TTT_UPDATE_CHECK");
    (void)argc;
    sArgv = argv;
    if (t != NULL && strcmp(t, "net") == 0) {
        int fail = self_test();
        printf(fail ? "ttt: net self-test FAILED (%d)\n" : "ttt: net self-test OK\n", fail);
        fflush(stdout);
        exit(fail ? 1 : 0);
    }
    if (u != NULL && strcmp(u, "1") == 0) {
        int st;
        open_match_socket();
        st = update_check(3000);
        exit(st == NET_UPDATE_LATEST || st == NET_UPDATE_NEWER ? 0 : st == NET_UPDATE_UNAVAILABLE ? 3 : 4);
    }
}
