/*
 * The Sony kernel, SIF and CD/DVD calls of the game on PC.
 *
 * Kernel: the game runs on one thread. Its only threads and semaphores belong to the movie player (movie.c), which
 * the PC build does not play yet: threads are created and recorded but never run, semaphores count. Interrupt
 * handlers (AddIntcHandler / AddDmacHandler, EnableIntc ...) live in plat_hw.c, which runs them.
 *
 * SIF / IOP: there is no second processor. The IOP reboot and the module loads succeed at once. The remote calls
 * the game makes go through Port_SifCall, a table of the IOP servers the game binds (the same numbers as the
 * modules of docs/systems/iop.md): RSPU2DRV 0x80000601 is served by plat_file.c (disc reads, the status word) and
 * answers the sound commands harmlessly; the pad, multitap and memory card servers answer harmlessly too (their EE
 * libraries are replaced function by function, plat_lib.c). The real sound, pad and memory card come later.
 *
 * CD: the game only initialises the drive; the files are read through RSPU2DRV's commands (plat_file.c).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_addr.h"
#include "plat_state.h"

/* ---- kernel: cache, threads, semaphores ---- */
int FlushCache(int mode) { (void)mode; return 0; }
void iFlushCache(int mode) { (void)mode; }
int GsPutIMR(uint64_t imr) { (void)imr; return 0; }

#define MAX_THREADS 16
static struct { int used, started; uint32_t param; } sThreads[MAX_THREADS]; /* (the game's address: 4 bytes) */

int GetThreadId(void) { return 1; }
int CreateThread(void *param) {
    int i;
    for (i = 2; i < MAX_THREADS; i++) { /* 1 is the main thread */
        if (!sThreads[i].used) {
            sThreads[i].used = 1;
            sThreads[i].started = 0;
            sThreads[i].param = Port_Addr(param);
            return i;
        }
    }
    return -1;
}
/* Started threads are not run (single-threaded port): only the movie player has any. */
int StartThread(int id, void *arg) {
    (void)arg;
    if (id <= 1 || id >= MAX_THREADS || !sThreads[id].used) {
        return -1;
    }
    if (!sThreads[id].started) {
        printf("ttt: thread %d started but not run (single-threaded port)\n", id);
    }
    sThreads[id].started = 1;
    return id;
}
int TerminateThread(int id) { return id > 1 && id < MAX_THREADS && sThreads[id].used ? (sThreads[id].started = 0, id) : -1; }
int iTerminateThread(int id) { return TerminateThread(id); }
int DeleteThread(int id) { return id > 1 && id < MAX_THREADS && sThreads[id].used ? (sThreads[id].used = 0, id) : -1; }
void ExitThread(void) {}
int SleepThread(void) { return 0; }
int WakeupThread(int id) { return id; }
int iWakeupThread(int id) { return id; }
int _iWakeupThread(int id) { return id; }
int RotateThreadReadyQueue(int prio) { (void)prio; return 0; }
void DisableDispatchThread(void) {}
void EnableDispatchThread(void) {}
int ChangeThreadPriority(int id, int prio) { (void)id; (void)prio; return 0; }

#define MAX_SEMAS 32
static struct { int used, count, max; } sSemas[MAX_SEMAS];

/* ee_sema_t: count, max_count, init_count, wait_threads, attr, option. */
int CreateSema(int32_t *param) {
    int i;
    for (i = 1; i < MAX_SEMAS; i++) {
        if (!sSemas[i].used) {
            sSemas[i].used = 1;
            sSemas[i].count = param[2];
            sSemas[i].max = param[1];
            return i;
        }
    }
    return -1;
}
int DeleteSema(int id) { return id > 0 && id < MAX_SEMAS && sSemas[id].used ? (sSemas[id].used = 0, id) : -1; }
int SignalSema(int id) {
    if (id <= 0 || id >= MAX_SEMAS || !sSemas[id].used) {
        return -1;
    }
    if (sSemas[id].max <= 0 || sSemas[id].count < sSemas[id].max) {
        sSemas[id].count++;
    }
    return id;
}
int iSignalSema(int id) { return SignalSema(id); }
/* With one thread nobody else can signal: a wait on an empty semaphore would never end. It is reported once and
   returns, as if the semaphore had been signalled. */
int WaitSema(int id) {
    if (id <= 0 || id >= MAX_SEMAS || !sSemas[id].used) {
        return -1;
    }
    if (sSemas[id].count > 0) {
        sSemas[id].count--;
    } else {
        static int told;
        if (!told++) {
            printf("ttt: WaitSema(%d) on an empty semaphore: no other thread can signal it\n", id);
        }
    }
    return id;
}
int PollSema(int id) {
    if (id <= 0 || id >= MAX_SEMAS || !sSemas[id].used || sSemas[id].count == 0) {
        return -1;
    }
    sSemas[id].count--;
    return id;
}

/* ---- SIF ---- */
int sceSifInitRpc(int mode) { (void)mode; return 0; }
int sceSifRebootIop(const char *img) { (void)img; return 1; }
int sceSifSyncIop(void) { return 1; }
int sceSifLoadFileReset(void) { return 0; }
static int sModules;
int sceSifLoadModule(const char *path, int argSize, const char *args) {
    (void)path; (void)argSize; (void)args;
    return ++sModules; /* a module id: success */
}

/* SIF commands: the game registers handlers for commands 0 and 1 (file.c). The IOP never sends any here (the BGM
   requests of RSPU2DRV come with the real sound, T35+); commands the game sends to the IOP are dropped. */
typedef void (*SifCmdHandler)(void *, void *);
static struct { uint32_t fn, data; } sSifCmd[32]; /* the game's addresses, 4 bytes in every build (state words) */
int sceSifAddCmdHandler(int cid, SifCmdHandler fn, void *data) {
    cid &= 0x7FFFFFFF;
    if (cid >= 0 && cid < 32) {
        sSifCmd[cid].fn = Port_Addr(fn);
        sSifCmd[cid].data = Port_Addr(data);
    }
    return 0;
}
static uint32_t sSifCmdId;
uint32_t sceSifSendCmd(int cid, void *packet, int packetSize, void *srcExtra, void *destExtra, int sizeExtra) {
    (void)cid; (void)packet; (void)packetSize; (void)srcExtra; (void)destExtra; (void)sizeExtra;
    return ++sSifCmdId; /* a DMA id, never 0 (0 means the queue was full) */
}
int sceSifDmaStat(uint32_t id) { (void)id; return -1; } /* transfer finished */

/* ---- remote calls: the IOP servers ---- */
/* A server takes the call's function number and its send / receive buffers and returns the reply's size. */
typedef int (*SifServer)(int fn, void *send, int ssize, void *recv, int rsize);

int Port_Rspu2Serve(int fn, void *send, int ssize, void *recv, int rsize); /* plat_file.c */

/* Pad, multitap, memory card: nothing connected, nothing to do. Replies are zeros (T35+ serve them). */
static int serve_quiet(int fn, void *send, int ssize, void *recv, int rsize) {
    (void)fn; (void)send; (void)ssize;
    if (recv != NULL && rsize > 0) {
        memset(recv, 0, (size_t)rsize);
    }
    return rsize;
}

static const struct { uint32_t id; SifServer serve; const char *name; } kServers[] = {
    { 0x80000601u, Port_Rspu2Serve, "RSPU2DRV" },
    { 0x80000100u, serve_quiet, "padman" },
    { 0x80000101u, serve_quiet, "padman (ext)" },
    { 0x80000400u, serve_quiet, "mcserv" },
    { 0x80000901u, serve_quiet, "mtapman port" },
    { 0x80000902u, serve_quiet, "mtapman slot" },
    { 0x80000903u, serve_quiet, "mtapman connection" },
};

/* A bound client: the server it reaches. sceSifClientData's `server` pointer (offset 0x24 in Sony's layout) is
   what the libraries test to see that the bind went through; it is set at once. */
int Port_SifBind(void *client, uint32_t id) {
    unsigned i;
    for (i = 0; i < sizeof kServers / sizeof kServers[0]; i++) {
        if (kServers[i].id == id) {
            if (client != NULL) {
                *(uint32_t *)((uint8_t *)client + 0x24) = Port_Addr(client);
                *(uint32_t *)((uint8_t *)client + 0x08) = id; /* rpcd.id: the server this client calls */
            }
            return 0;
        }
    }
    return -1;
}

int Port_SifCall(uint32_t id, int fn, void *send, int ssize, void *recv, int rsize) {
    unsigned i;
    for (i = 0; i < sizeof kServers / sizeof kServers[0]; i++) {
        if (kServers[i].id == id) {
            return kServers[i].serve(fn, send, ssize, recv, rsize);
        }
    }
    {
        static int told;
        if (!told++) {
            printf("ttt: remote call to unknown IOP server 0x%08x\n", (unsigned)id);
        }
    }
    return serve_quiet(fn, send, ssize, recv, rsize);
}

int sceSifBindRpc(void *client, uint32_t id, int mode) { (void)mode; return Port_SifBind(client, id); }
typedef struct { uint32_t id; int fn; void *send; int ssize; void *recv; int rsize; } RpcCall;
static void rpc_off(void *p) {
    RpcCall *c = p;
    Port_SifCall(c->id, c->fn, c->send, c->ssize, c->recv, c->rsize);
}
/* (off the EE stack, as sceSdRemote: the servers read the disc through the C library) */
int sceSifCallRpc(void *client, int fn, int mode, void *send, int ssize, void *recv, int rsize, void *end,
                  void *endArg) {
    RpcCall c = { *(uint32_t *)((uint8_t *)client + 0x08), fn, send, ssize, recv, rsize };
    (void)mode; (void)end; (void)endArg;
    Port_OffEe(rpc_off, &c);
    return 0;
}
int sceSifCheckStatRpc(void *client) { (void)client; return 0; } /* never busy */

/* ---- CD/DVD: the drive is ready, the media is a CD ---- */
int sceCdInit(int mode) { (void)mode; return 1; }
int sceCdMmode(int media) { (void)media; return 1; }

/* ---- self-test (TTT_SELFTEST=rpc) ---- */
int sceSdRemote(int mode, int cmd, int a0, int a1, int a2, int a3, int a4, int a5); /* plat_file.c */
int Port_FileSelfTest(void);                                                         /* plat_file.c */

static int sFails;
static void expect(int ok, const char *what) {
    if (!ok) {
        printf("ttt: rpc self-test FAILED: %s\n", what);
        sFails++;
    }
}

void Port_RpcSelfTest(void) {
    uint8_t client[0x40], buf[0x40];
    int sema;
    int32_t sp[6] = { 0, 1, 1, 0, 0, 0 };

    memset(client, 0, sizeof client);
    expect(sceSifRebootIop("cdrom0:\\IRX\\IOPRP165.IMG;1") != 0, "IOP reboot succeeds");
    expect(sceSifSyncIop() != 0, "IOP synced");
    expect(sceSifLoadModule("cdrom0:\\IRX\\PADMAN.IRX;1", 0, NULL) >= 0, "module load succeeds");
    expect(sceCdInit(0) == 1 && sceCdMmode(1) == 1, "CD init / media");
    expect(sceSifBindRpc(client, 0x80000100u, 0) == 0 && *(uint32_t *)(client + 0x24) != 0, "bind answered at once");
    memset(buf, 0xAA, sizeof buf);
    sceSifCallRpc(client, 1, 0, buf, 0x10, buf, 0x20, NULL, NULL);
    expect(buf[0] == 0 && buf[0x1F] == 0 && buf[0x20] == 0xAA, "pad server replies with zeros, reply size kept");
    expect(Port_SifBind(NULL, 0x80000666u) < 0, "unknown server not bound");
    memset(buf, 0xAA, sizeof buf);
    expect(sceSdRemote(1, 0x6418, Port_Addr(buf), 0, 0, 0, 0, 0) == 0 && buf[0] == 0 && buf[23] == 0,
           "0x6418: every voice key off");
    expect((sceSdRemote(1, 0x2030, 0, 0, 0, 0, 0, 0) & 0x51) == 0x40, "0x2030: no read pending, stream idle");
    expect(sceSdRemote(1, 0x2000, 0x2237, 0, 0x446E, 0, 0, 0) == 0, "0x2000: driver started");
    expect(sceSdRemote(1, 0x7F0F, 0, 0, 0, 0, 0, 0) == 0, "0x7F0F: sound queue taken");
    expect(Port_FileSelfTest(), "disc reads through 0x204E");
    sema = CreateSema(sp);
    expect(sema > 0 && WaitSema(sema) == sema && PollSema(sema) < 0 && SignalSema(sema) == sema &&
           PollSema(sema) == sema, "semaphore counts");
    if (sFails == 0) {
        printf("ttt: rpc self-test OK\n");
    }
    exit(sFails != 0);
}

/* The kernel's tables (the thread parameters and SIF handler data are game addresses, the handlers game functions). */
__attribute__((constructor(101))) static void sys_state(void) {
    Port_StateAdd("sys.threads", sThreads, sizeof sThreads, PS_HASH);
    Port_StateAdd("sys.semas", sSemas, sizeof sSemas, PS_HASH);
    Port_StateAdd("sys.modules", &sModules, sizeof sModules, PS_HASH);
    Port_StateAdd("sys.sifCmd", sSifCmd, sizeof sSifCmd, PS_HASH);
    Port_StateAdd("sys.sifCmdId", &sSifCmdId, sizeof sSifCmdId, PS_HASH);
}
