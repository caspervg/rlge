# Fonts

VOXHAVEN ships two typefaces from [Google Fonts](https://fonts.google.com), both
licensed under the [SIL Open Font License 1.1](https://openfontlicense.org),
which permits redistribution alongside this example.

| File | Family | Used for |
| --- | --- | --- |
| `rubik_ui.ttf` | [Rubik](https://fonts.google.com/specimen/Rubik) Medium (500) — Hubert Jan Kowalski, Dave Crossland, Jakub Jankiewicz | HUD, menus, labels |
| `rubik_mono.ttf` | [Rubik Mono One](https://fonts.google.com/specimen/Rubik+Mono+One) — Hubert Jan Kowalski, Dave Crossland | The VOXHAVEN logo |

Rubik's slightly rounded geometric letterforms sit well against the chunky voxel
silhouettes, and Rubik Mono One's heavy fixed-width caps make the logo read as
built out of blocks.

These are the only binary assets in the example — every texture, sound and
terrain feature is generated procedurally at startup. If the files are missing,
`setupFonts_()` falls back to raylib's built-in font and the game still runs.
