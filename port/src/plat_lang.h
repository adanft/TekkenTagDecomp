/* The port's own text in the player's language (T47, docs/port/translations.md; plat_lang.c). The game's text is the
   game's data and is never here: only the overlay, the setup screen and the port's messages to the player.

   Every string has a stable key and its English text in the code: Tr("video.vsync", "V-sync"). The English source
   catalog port/lang/en.json holds the same pairs (tests/test_port_lang.py checks that the two agree, and that every
   other catalog has the same keys with the same placeholders). Other languages are flat JSON files of key -> text,
   port/lang/<code>.json, read at run time; a key a catalog lacks falls back to the English text.

   Placeholders: {0}..{9} in Lang_Format's texts (string arguments); printf conversions (%d, %s, %%) in TrFmt's texts,
   which are handed to ImGui as formats. A translation whose placeholders differ from the English text's is not used. */
#ifndef PLAT_LANG_H
#define PLAT_LANG_H

#ifdef __cplusplus
extern "C" {
#endif

/* The language from the settings (ui.language, TTT_LANG; "" or "auto": LANGUAGE, LC_ALL, LC_MESSAGES, LANG), its
   catalog loaded. After Settings_Load, before any thread shows text. */
void Lang_Init(void);
/* Switches to `code` ("en", "es", "es_AR"...; "" or "auto" detects). A catalog is read once and kept. 1 when the
   language in use is the one asked for (or its base language: es for es_AR), 0 when it fell back to English. */
int Lang_Set(const char *code);
const char *Lang_Current(void); /* the language in use: "en" when none was found */
const char *Lang_Detect(void);  /* the language the environment asks for ("en" when it names none) */

const char *Tr(const char *key, const char *en);    /* the text of `key`, else `en` */
const char *TrFmt(const char *key, const char *en); /* a printf format: the translation only with en's conversions */
/* `key`'s text with {0}.. replaced by the string arguments (a NULL ends them) into out; its length. */
int Lang_Format(char *out, int n, const char *key, const char *en, ...);
/* The same with English only (no catalog): the port's messages on stdout / stderr. */
int Lang_FormatEn(char *out, int n, const char *en, ...);

/* The languages there are catalogs for (English first): codes and names in their own language. */
typedef struct {
    char code[16];
    char name[64];
} LangInfo;
int Lang_List(LangInfo *out, int max);

int Lang_SelfTest(void); /* the settings self-test's part: failures (TTT_SELFTEST=settings) */

#ifdef __cplusplus
}
#endif
#endif
