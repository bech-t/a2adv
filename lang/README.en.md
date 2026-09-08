# lang/ — interface language bases

This folder holds the `.lng` files: the base set of fixed interface strings
for the player (menus, combat screen, saving, system messages...), one file
per language. An adventure picks its base with `@lang <code>` in its `.adv`
(see [docs/GUIDE-FORMAT-ADV.md, `@lang` section](../docs/GUIDE-FORMAT-ADV.md)
— French only for now).

## Format

```
@lang en

@ui menu_new  "START"
@ui menu_quit "QUIT"
...
```

- One `@lang <code>` line at the top: a 2-letter lowercase code that must
  match the file name (`en.lng` → `en`).
- One `@ui <key> "text"` line per string.
- All 46 keys are **mandatory**: an incomplete base fails compilation
  (`a2c` lists the missing keys) instead of leaving the player silent on one
  of them. The full key list, with its French default value, is documented
  in [docs/GUIDE-FORMAT-ADV.md, `@ui` section](../docs/GUIDE-FORMAT-ADV.md).
- Accented characters are accepted: they get transliterated to ASCII at
  compile time (`é` → `e`); any character that can't be converted becomes
  `?`. Keep it short — these strings render on a 40-column screen.

## Adding a language

1. Copy `fr.lng` to `<code>.lng` (e.g. `en.lng`), `<code>` = 2 lowercase
   letters.
2. Replace `@lang fr` with `@lang <code>`.
3. Translate each quoted string, leaving the keys untouched.
4. Reference that code from an adventure with `@lang <code>`.

The compiler looks up `lang/<code>.lng` at the repo root (see
`compiler/a2c/cli.py`) and compiles it into `APP.LNG` on the disk image.

To change just one or two strings without writing a full base, an adventure
can also override any key on the fly with `@ui` directly in its `.adv`.
