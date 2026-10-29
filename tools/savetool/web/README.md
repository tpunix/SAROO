# SAROO Save Editor — Web Edition

A self-contained, **browser-based** version of the SAROO [`savetool`](https://github.com/tpunix/SAROO/tree/master/tools/savetool)
CLI. Open, view, edit and rebuild Sega Saturn backup memory images entirely in
your web browser — no install, no server, no upload. Everything runs locally in
the page; your files never leave your machine.

## Usage

Open [`index.html`](index.html) in any modern browser (double-click it, or drag it
into a browser window). Then:

| Action | What it does |
| --- | --- |
| **Open .BIN** | Load a save image and list its slots/saves. (You can also drag & drop the file onto the page.) |
| **New SAROO save** | Start an empty SAROO save file (`SS_SAVE.BIN`). |
| **New slot** (SAROO only) | Add an empty 64&nbsp;KB game slot. |
| **Import save…** | Import a `.SRO`, `.BUP`, or `.XML` save file into a slot/volume. |
| **Edit** (per save) | Rename a save, change its comment or language, and edit the raw save data in a built-in hex editor (click a byte to change its value). |
| **Replace** (per save) | Overwrite a save's data from a `.SRO` / `.BUP` / `.XML` file. |
| **Export ▾** (per save) | Download that save as `.SRO`, `.BUP`, `.XML`, or raw `.BIN`. |
| **Delete** (per save) | Remove a save and free its blocks. |
| **Export .BIN** | Rebuild the whole image and download it. |

Changes live in memory until you **Export .BIN**.

## Supported files

The format is auto-detected from the file header:

| File | Format | Support |
| --- | --- | --- |
| `SS_SAVE.BIN` | **SAROO save** (`Saroo Save File`) | full — list, import, replace, edit, delete, new slot |
| `SS_MEMS.BIN` | **SaroMems** extended save (`SaroMems`) | full — list, import, replace, edit, delete |
| `SS_BUP.BIN` | **Saturn Backup RAM** (`BackUpRam Format`, incl. interleaved) | read-only — list & export (matches the CLI) |

Individual saves are read/written in three interchange formats:

- **`.SRO`** — SAROO raw save (`SSAVERAW` header, CRC-32 checked).
- **`.BUP`** — Pseudo Saturn Kai / Vmem format (works with PS Kai and jo-engine tools).
- **`.XML`** — [SS Backup RAM Parser](https://github.com/hitomi2500) format. Imports read the base64 `*_binary`, `size`, `data`, `language_code`, and `date`/`time` fields; `date`/`time` values are used verbatim.

## Japanese text (Shift-JIS)

Saturn save names and comments are stored in **Shift-JIS**. The editor decodes them to
Unicode for display, so a comment like `bb b8 d7 be b0 cc de` shows as **ｻｸﾗｾｰﾌﾞ**
("Sakura Save") instead of mojibake. When you edit a name or comment, the text is
encoded back to Shift-JIS, so ASCII, half-width katakana, and full-width kana/kanji all
round-trip losslessly. This uses the browser's built-in `TextDecoder('shift-jis')` (and a
reverse encoder derived from it at runtime) — no encoding tables are bundled, and the raw
bytes of any save you don't edit are preserved exactly.

## Fidelity

The codec is a faithful port of the C `savetool`, verified **byte-for-byte identical**
to the CLI against real save images:

- **SAROO** (`SS_SAVE.BIN`) — slot create/format, import (into formatted *and* unformatted
  slots), delete, and export of 25 real game saves across 21 slots.
- **Saturn Backup RAM** (`SS_BUP.BIN`, interleaved) — export of every save, including the
  chained multi-block reader.
- **SaroMems** (`SS_MEMS.BIN`) — import, delete, and export.

Big-endian layout, the block allocators (128 B / 64 B / 1 KB), per-save block bitmaps,
the save linked-list, the SaroMems directory, and free-block accounting all mirror the
original exactly. `.SRO` files round-trip bit-for-bit and are cross-consistent with their
`.BUP` twins.

Everything is in a single `index.html` (HTML + CSS + JS, no dependencies, no network).

## Credits

SAROO Save Editor by [Bucanero](https://github.com/bucanero) — GNU GPLv3.
