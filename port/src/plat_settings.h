/* The player's settings (T36, docs/port/settings.md): a key = value text file, the overlay's choices, the keyboard's
   bindings. Integer and text values only; plat_settings.c. */
#ifndef PLAT_SETTINGS_H
#define PLAT_SETTINGS_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The pads' buttons the keyboard can press, in the file's and the overlay's order (keys.p1.up ... keys.p2.select). */
enum {
    SB_UP, SB_DOWN, SB_LEFT, SB_RIGHT, SB_SQUARE, SB_TRIANGLE, SB_CROSS, SB_CIRCLE,
    SB_L1, SB_R1, SB_L2, SB_R2, SB_START, SB_SELECT, SB_COUNT
};
#define SETTINGS_SC_OPEN 58u   /* F1: opens the settings; never a pad button */
#define SETTINGS_SC_ESCAPE 41u /* Escape: the quit question; never a pad button */
#define SETTINGS_SC_FAST 43u   /* Tab: held, the game runs at game.fast_forward (T40); never a pad button */

/* Reads the file (TTT_CONFIG, else $XDG_CONFIG_HOME/ttt/settings.ini, else ~/.config/ttt/settings.ini). A setting
   that has an environment variable (video.renderer: TTT_RENDERER...) and is in the file becomes that variable's
   value unless the variable is set already: the variables still win. Once, at start, before any thread. */
void Settings_Load(void);
const char *Settings_Path(void); /* "" when there is none (no HOME) */
const char *Settings_LoadError(void); /* "" or why the file could not be read */

/* The effective value: the environment's when its variable was set before Settings_Load, else the file's, else the
   default. Unknown keys: 0 / "". */
int Settings_GetInt(const char *key);
void Settings_GetStr(const char *key, char *out, int n);
int Settings_FromEnv(const char *key);     /* 1: its variable was set by the environment (the file's value waits) */
int Settings_NeedsRestart(const char *key); /* 1: read at start only */
const char *Settings_Env(const char *key);  /* its variable's name, or NULL */
/* Changes the file's value (clamped to the setting's range) and writes the file at once (a temporary file renamed
   over it). Other lines (comments, keys this program does not know) stay as they are. 1 when written. */
int Settings_SetInt(const char *key, int v);
int Settings_SetStr(const char *key, const char *v);
const char *Settings_SaveError(void); /* "" or why the last write failed */

/* Keys by their place (SDL / USB scancodes): a name for the file and the overlay ("W", "Keypad 4", "Left Shift"; else
   "Scancode 123"), and back (names in any case, or a number); 0 when unknown. */
void Settings_KeyName(uint32_t sc, char *out, int n);
uint32_t Settings_KeyFromName(const char *name);

/* The keyboard's bindings: [pad][SB_*] scancodes, from the file over the defaults (pad 1 on W A S D / U I J K, pad 2
   on the arrows and the keypad). */
void Settings_DefaultBindings(uint16_t b[2][SB_COUNT]);
void Settings_Bindings(uint16_t b[2][SB_COUNT]);
const char *Settings_ButtonName(int button); /* "up", "square"... */
/* Binds `sc` to the pad's button: a key presses one button only, so a button that had `sc` gets this one's old key
   (the two swap). F1, Escape and Tab are refused, and so is 0 or a scancode past 511. 1 when done. */
int Settings_Rebind(uint16_t b[2][SB_COUNT], int pad, int button, uint32_t sc);
int Settings_SaveBindings(const uint16_t b[2][SB_COUNT]); /* every keys.* line; 1 when written */

void Settings_SelfTest(void); /* TTT_SELFTEST=settings */

#ifdef __cplusplus
}
#endif
#endif
