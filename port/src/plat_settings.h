/* The player's settings (T36, docs/port/settings.md): a key = value text file, the overlay's choices, the keyboard's
   and the gamepads' bindings (T34). Integer and text values only; plat_settings.c. */
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
/* The gamepads also press the sticks' buttons (L3, R3), which have no key (pad.p1.l3 ...). */
enum { SB_L3 = SB_COUNT, SB_R3, SB_GAMEPAD_COUNT };
#define SETTINGS_PADS 4 /* gamepads: four with the multitap (input.multitap), the keyboard plays pads 1 and 2 */
/* A gamepad's inputs: SDL's gamepad buttons 0..25 (SDL_GamepadButton: south, east, west, north, back, guide, start,
   the sticks, the shoulders, the d-pad, misc and paddles...), then the two triggers. */
#define SETTINGS_GP_GUIDE 5u      /* opens the settings (as F1 does); never a pad button */
#define SETTINGS_GP_LTRIGGER 26u
#define SETTINGS_GP_RTRIGGER 27u
#define SETTINGS_GP_INPUTS 28u
#define SETTINGS_GP_NONE 0xFFu    /* not bound */

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
void Settings_Summary(char *out, int n);   /* every effective value, "key=value ..." */
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
const char *Settings_ButtonName(int button); /* "up", "square"... "l3", "r3" (SB_GAMEPAD_COUNT) */
/* Binds `sc` to the pad's button: a key presses one button only, so a button that had `sc` gets this one's old key
   (the two swap). F1, Escape and Tab are refused, and so is 0 or a scancode past 511. 1 when done. */
int Settings_Rebind(uint16_t b[2][SB_COUNT], int pad, int button, uint32_t sc);
int Settings_SaveBindings(const uint16_t b[2][SB_COUNT]); /* every keys.* line; 1 when written */

/* The gamepads' bindings (T34): [pad][SB_* and SB_L3 / SB_R3] gamepad inputs (SETTINGS_GP_*), the same on every pad by
   default (the d-pad, south cross, east circle, west square, north triangle, shoulders L1 / R1, triggers L2 / R2, start,
   back select, stick clicks L3 / R3), from the file's pad.p<n>.<button> lines. An input's name ("south", "dpad_up",
   "left_trigger"; "none"), and back (SETTINGS_GP_NONE when unknown). */
void Settings_DefaultGamepad(uint8_t g[SETTINGS_PADS][SB_GAMEPAD_COUNT]);
void Settings_Gamepad(uint8_t g[SETTINGS_PADS][SB_GAMEPAD_COUNT]);
const char *Settings_GamepadInputName(unsigned input);
unsigned Settings_GamepadInputFromName(const char *name);
/* Binds gamepad input `input` to the pad's button: an input presses one button of that pad, so the button that had it
   gets this one's old input (the two swap; other pads are their own gamepads). The guide button is refused (it opens
   the settings), and so is an input past the triggers. 1 when done. */
int Settings_RebindGamepad(uint8_t g[SETTINGS_PADS][SB_GAMEPAD_COUNT], int pad, int button, unsigned input);
int Settings_SaveGamepad(const uint8_t g[SETTINGS_PADS][SB_GAMEPAD_COUNT]); /* every pad.* line; 1 when written */

void Settings_SelfTest(void); /* TTT_SELFTEST=settings */

#ifdef __cplusplus
}
#endif
#endif
