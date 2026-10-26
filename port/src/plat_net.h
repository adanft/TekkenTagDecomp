/* Netplay's interface for the window thread (T45 / T46, plat_net.c; docs/port/netplay.md, docs/port/matchmaking.md):
   the session's state for the overlay and the window's title, the chat, and starting a matchmaking session from the
   overlay. Everything here may be called from any thread (a mutex covers what the game thread shares). */
#ifndef PLAT_NET_H
#define PLAT_NET_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { NET_PATH_NONE, NET_PATH_ADDRESS, NET_PATH_DIRECT, NET_PATH_RELAY }; /* address: TTT_NET_HOST / TTT_NET_JOIN */
enum { NET_PHASE_OFF, NET_PHASE_SERVER, NET_PHASE_WAITING, NET_PHASE_CONNECTING, NET_PHASE_PLAYING };
enum { NET_UPDATE_NONE, NET_UPDATE_LATEST, NET_UPDATE_NEWER, NET_UPDATE_BAD_SIGNATURE, NET_UPDATE_UNAVAILABLE,
       NET_UPDATE_MALFORMED };

typedef struct {
    int phase, side, path;  /* side: 0 host (pad 1), 1 joining (pad 2) */
    char code[8];           /* the room's code, "" none */
    char peer[32];          /* the peer's name */
    uint32_t rttMs, delay, rollbacks, framesAgain, stalls, vblank;
    int update;             /* NET_UPDATE_* */
    char updateVersion[48]; /* the manifest's version text */
    char mods[96], peerMods[96]; /* T49: this side's mods and the peer's (Mods_Summary; information only, "" unknown) */
} PortNetInfo;

void Port_NetInfo(PortNetInfo *out);
/* The window's title for the session ("" when there is none); 1 when it changed since the last call. */
int Port_NetTitle(char *out, int n);

/* The chat: a line to send to the peer (1 when queued: a session is on and the queue has room), and the lines so far
   (the last 32, oldest first; `mine` 1 for this side's). */
int Port_NetChatSend(const char *text);
int Port_NetChatCount(void);
int Port_NetChatLine(int i, int *mine, char *text, int n);
int Port_NetChatRecent(void); /* 1 while a line arrived in the last 10 s (the overlay shows them) */

/* The overlay's buttons: starts the program again from boot in a matchmaking session (mode 1 create a room, 2 join
   `code`, 3 quick match) with the settings' server; returns only when that failed (0). */
int Port_NetRestart(int mode, const char *code);
int Port_NetCanRestart(void);

void Port_NetMain(int argc, char **argv); /* plat_main.c, after the settings: TTT_SELFTEST=net, TTT_UPDATE_CHECK=1 */

#ifdef __cplusplus
}
#endif
#endif
