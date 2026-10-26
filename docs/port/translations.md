# PC port: translations of the port's own text (T47)

The port's own text can be shown in other languages: the settings overlay (F1), the setup screen, the setup's messages
on that screen, the window title's `[modified game data]` mark and the screen reader's texts. The game's text (its
menus, names, subtitles) is the game's data, read from the player's disc. It is never copied into this repository and
it is not translated here.

Messages on stdout and stderr (`ttt: ...`) stay in English. Scripts and tests read them.

## Catalogs

A catalog is a flat JSON object of key to text, in UTF-8, one file per language: `port/lang/<code>.json`.

```json
{
  "@name": "Español",
  "overlay.quit.yes": "Sí, salir",
  "overlay.controls.pad_line": "Mando %d: %s",
  "a11y.player_is": "Jugador {0}: {1}"
}
```

- **Keys** are stable dotted names (`overlay.video.vsync`, `setup.install`, `data.err.no_disc`). A key never changes
  meaning. When the English text changes meaning, the key is renamed.
- **Keys that start with `@`** describe the catalog itself. `@name` is the language's name in that language, shown by
  the picker.
- **`port/lang/en.json` is the source catalog.** The English text also lives in the code, in calls such as
  `Tr("overlay.video.vsync", "V-sync")`. The program needs no file for English, and a key that a catalog lacks falls
  back to its English text.
- **Placeholders.** Texts for `Lang_Format` use `{0}`..`{9}`, which take string arguments and may be reordered. Texts
  for `TrFmt` are printf formats that ImGui receives (`%d`, `%s`, `%.1f`, `%%`). They must keep the English text's
  conversions, in the same order. A translation whose placeholders differ from the English text's is not used: the
  English text is shown instead.
- The reader (`port/src/plat_lang.c`, about 100 lines of JSON, no library) accepts:
  - the standard string escapes;
  - `\uXXXX`, including surrogate pairs;
  - a UTF-8 byte order mark.

  It refuses any value that is not a string, and files over 1 MB. A broken file is reported once on stderr
  (`ttt: lang: <file>: <why>: not used`), and that language falls back to English.

| Code | Language | Keys |
|---|---|---|
| `en` | English (source, built in) | 185 |
| `es` | Spanish (neutral, formal "usted") | 185 |

### Where catalogs are found

The program uses the first folder that holds `<code>.json`, in this order:

1. `TTT_LANG_DIR`.
2. `lang/` in the player's data folder ([install.md](install.md)). Translations added by the player go here.
3. `lang/` beside the program. A release carries its catalogs here.
4. `port/lang/` of the repository, when the program is `build/port/ttt` of a checkout.

## The language

The language comes from the first of these that is set:

1. `ui.language` in the settings file, or the variable `TTT_LANG`, which wins over the file
   ([settings.md](settings.md)).
2. When the setting is empty or `auto`, the environment, read the way gettext reads it:
   - `LANGUAGE`, then `LC_ALL`, `LC_MESSAGES`, `LANG`.
   - `es_AR.UTF-8` tries `es_AR`, then `es`.
   - `C` and `POSIX` mean English.

Choose the language on the overlay's Accessibility page, or at the top of the setup screen. The switch is immediate,
with no restart. A catalog is read once and kept until the program exits, so a text that has already been handed out
stays valid while the language changes.

The setup's reasons (a refused disc, no disc) are kept as a key plus arguments. They are formatted when the screen shows
them, because the first reason is found at start, before the language is known.

## Font

The overlay uses Dear ImGui's built-in font (ProggyClean, or ProggyForever at larger sizes). Since ImGui 1.92 it renders
glyphs on demand, so no glyph ranges are configured. The built-in font covers Latin-1, which includes Spanish's
`á é í ó ú ñ ü ¿ ¡`. `TTT_OVERLAY_TEST` reports `accented glyphs n/21` from `ImFont::IsGlyphInFont`, and
`tests/test_port_a11y.py` requires all of them.

A language outside Latin-1 (Greek, Cyrillic, CJK) would need a TTF font loaded from memory.
`IMGUI_DISABLE_FILE_FUNCTIONS` is set, so ImGui itself cannot open the file. That font is not part of T47.

## Checks

`tests/test_port_lang.py` needs no build. It fails when:

- the code passes a key and an English text that are not in `en.json`, or `en.json` holds a key that no code uses;
- the same key appears with two different English texts;
- the overlay passes ImGui a literal with words that bypasses the catalog;
- a catalog is not a flat object of strings, repeats a key, or misses or adds keys compared with `en.json`;
- a translated text is empty, or changes the `{n}` placeholders or the printf conversions.

`python3 tests/test_port_lang.py --write-en` regenerates `en.json` from the code, with keys sorted.

`TTT_SELFTEST=settings` checks the language's round trip through `ui.language`, `TTT_LANG` and `es.json`. It also checks
the reader on valid and broken input, the fallbacks and the locale detection.

`tests/test_port_a11y.py` runs the overlay in Spanish in a hidden window and reads what the screen reader said.

## Adding a language

1. Copy `port/lang/en.json` to `port/lang/<code>.json`. Set `@name` and translate every value.
2. Keep every `{n}` and every `%` conversion.
3. Run `python3 tests/test_port_lang.py`.

Text that is not translated:

- Key names on the Controls page (`Left Shift`, `Keypad 4`): they are the names the settings file stores.
- Gamepad inputs beyond the common ones (`misc1`, paddles).
- `L1`..`R3`.
