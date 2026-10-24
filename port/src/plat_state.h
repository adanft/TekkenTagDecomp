/* The simulation's state as one block (port/src/plat_state.c, docs/port/determinism.md): what each module of the port
   keeps registers here once (a constructor), and the whole of it is hashed, saved and put back at a VBlank boundary. */
#ifndef PORT_PLAT_STATE_H
#define PORT_PLAT_STATE_H
#include <stddef.h>
#include <stdint.h>

enum {
    PS_HASH = 1,  /* part of the gameplay hash (what the game can see); everything registered is saved */
    PS_GSPRIV = 2, /* the GS privileged registers: CSR counts only by FIELD / VSINT, SIGLBLID not at all (the GS threads
                      set FINISH / SIGNAL / LABEL whenever they get there; the game reads none of them) */
    PS_HASHONLY = 4 /* hashed, not saved: the part of a saved region that is free of the program's own addresses */
};

/* A region of the state (registered from a constructor of priority 101, before any self-test). It must hold no host
   heap or library address: pointers to the program's own code and variables are fine (the same in every process of
   one build). */
void Port_StateAdd(const char *name, void *p, uint32_t size, int flags);
/* A module's hooks: `busy` says why the state cannot be taken now (NULL: it can), `before` runs on the game's thread
   before a save or a load (drains, staging copies), `after_load` after a load put the regions back. Any may be NULL. */
void Port_StateHooks(const char *(*busy)(void), void (*before)(void), void (*after_load)(void));

/* The in-process API (rollback), for code that runs inside the VBlank boundary (on the service stack): Port_StateSave
   writes the state into `buf` (Port_StateSize bytes) and returns 0, or -1 when it cannot be taken now (a movie plays,
   a memory card file is open, the GL back end, not at a boundary); Port_StateLoad puts a saved state back and returns
   0 (-1: not a state of this build): when the boundary returns, the game goes on from the boundary that state was
   saved at. */
size_t Port_StateSize(void);
int Port_StateSave(void *buf);
int Port_StateLoad(const void *buf);
/* The gameplay hash: XXH3-64 over the regions marked PS_HASH, host code addresses as stable ids. */
uint64_t Port_StateHash(void);
/* plat_hw.c: called after the poll that raised a VBlank (outside interrupt handlers), and at the first poll when
   gPortStateWant asks for it (TTT_LOADSTATE). A stack switch: the same bytes on the EE stack in every run. */
void Port_StateBoundary(void);
/* Runs fn(arg) off the EE stack when called on it (the game's thread): for port code whose path or locals depend on
   the host's timing (waits on the GS threads), so that what it leaves on the EE stack does not. */
void Port_OffEe(void (*fn)(void *), void *arg);
extern int gPortStateWant;
/* 1 while frames are being run a second time (TTT_SYNCTEST's second pass): no sound out, no recording. */
extern int gPortResim;

#endif
