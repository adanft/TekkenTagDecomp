/* Screen reader output (T48, docs/port/accessibility.md; plat_speech.c, plat_a11y.c): what the port's interface and,
   on request, the game's menus and fights say. Off by default (ui.screen_reader, TTT_SCREEN_READER). */
#ifndef PLAT_SPEECH_H
#define PLAT_SPEECH_H

#ifdef __cplusplus
extern "C" {
#endif

/* After Settings_Load and Lang_Init: the settings' choice, and the output: TTT_SR_LOG=<file> (each text a line,
   nothing spoken: tests, or another program reading it), else speech-dispatcher (libspeechd loaded at run time, else
   its spd-say command), else nothing. */
void Speech_Init(void);
void Speech_SetEnabled(int on);
int Speech_Enabled(void);
/* Says `text` (UTF-8) when the screen reader is on; `interrupt`: what is being said stops first (a new focus). Any
   thread; never waits for the speech. */
void Speech_Say(const char *text, int interrupt);
const char *Speech_Output(void); /* "log", "libspeechd", "spd-say", "none" (after the first text) or "off" */

/* The game's menus and fights (plat_a11y.c): read once a VBlank from the game's memory, never written. On when the
   screen reader and ui.announce_game are on. */
void A11y_Init(void);
void A11y_SetGame(int on);
void Port_A11yVBlank(void); /* plat_hw.c's VBlank, off the EE stack */
void Port_A11yReport(void); /* the run's end: how many texts were said */

#ifdef __cplusplus
}
#endif
#endif
