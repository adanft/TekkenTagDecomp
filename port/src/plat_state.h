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
    PS_HASHONLY = 4, /* hashed, not saved: the part of a saved region that is free of the program's own addresses */
    PS_GSSIDE = 8   /* the GS threads' side (VIF1, VU1, the GS front end, GS memory): left out of saves and loads while
                       gPortStateNoGs is set */
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
   a memory card file is open, not at a boundary); Port_StateLoad puts a saved state back and returns 0 (-1: not a
   state of this build): when the boundary returns, the game goes on from the boundary that state was saved at.
   With page tracking on (Port_StateTrack, F07) a save into a buffer that holds an earlier save of this process copies
   only the pages written since, and a load from one only the pages written since it was saved: the caller must not
   change a saved buffer's bytes (copying a whole buffer elsewhere is fine). The result is the same as without. */
size_t Port_StateSize(void);
int Port_StateSave(void *buf);
int Port_StateLoad(const void *buf);
/* 1 while a load's `before` hooks run (they may skip work a save needs). */
int Port_StateLoading(void);
/* Page tracking of the large regions (EE RAM, GS memory, sound RAM) through the kernel's write protection
   (userfaultfd, asynchronous, and PAGEMAP_SCAN; Linux 6.7 or later): 1 = on, 0 = off. Returns whether it is on (it
   stays off where the kernel does not offer it: every save is then a whole copy). Inside the boundary only. */
int Port_StateTrack(int on);
/* The gameplay hash: XXH3-64 over the regions marked PS_HASH, host code addresses as stable ids. */
uint64_t Port_StateHash(void);
/* plat_hw.c: called after the poll that raised a VBlank (outside interrupt handlers), and at the first poll when
   gPortStateWant asks for it (TTT_LOADSTATE). A stack switch: the same bytes on the EE stack in every run. */
void Port_StateBoundary(void);
/* Runs fn(arg) off the EE stack when called on it (the game's thread): for port code whose path or locals depend on
   the host's timing (waits on the GS threads), so that what it leaves on the EE stack does not. */
void Port_OffEe(void (*fn)(void *), void *arg);
/* Runs fn() on a stack below 4 GB when called on one above (x86-64, TTT_CC=clang64: the host's stacks are up there, and
   the game's memory keeps addresses in 4 bytes); elsewhere, and in the 32-bit build, it just calls fn. For the
   self-tests, which run from constructors on the host stack and hand addresses of their locals to game interfaces. */
void Port_OnLowStack(void (*fn)(void));
extern int gPortStateWant;
/* 1 while frames are being run a second time (TTT_SYNCTEST's second pass): no sound out, no recording. */
extern int gPortResim;
/* 1 while frames are run again undrawn (TTT_ROLLBACK_DRAW=0, a rollback's frames before the last): plat_hw.c drops
   the DMA into the GS path (VIF1, GIF) and the VBlank's picture. The game reads nothing back from the GS, so its state
   is the same; the GS's side of the state is not (it lacks those frames' work). */
extern int gPortNoGs;
/* 1: saves and loads leave out the regions marked PS_GSSIDE and do not wait for the GS threads (the GS's hooks do
   nothing): a load leaves the GS's side as it is. A buffer saved so is marked; loading it never touches that side. */
extern int gPortStateNoGs;

#endif
