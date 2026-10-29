# SAROO Cover Editor — Web Edition

A self-contained, **browser-based** version of the SAROO [`covertool`](https://github.com/tpunix/SAROO/tree/master/tools/covertool) CLI.
Open, view, edit and rebuild SAROO's `cover.bin` cover-image database entirely in
your web browser — no install, no server, no upload. Everything runs locally in
the page; your files never leave your machine.

## Usage

Open [`index.html`](index.html) in any modern browser (double-click it, or drag it
into a browser window). Then:

| Action | What it does |
| --- | --- |
| **Open cover.bin** | Load an existing `cover.bin` and list every cover. (You can also drag & drop the file onto the page.) |
| **New** | Start an empty cover set. |
| **Add cover** | Import one or more images (`.png`, `.bmp`, or any format the browser can decode). Single image opens the editor; multiple images are batch-added. |
| **Edit** (per cover) | Change the image, serial ID, hash, or size. |
| **PNG / BMP** (per cover) | Download that cover as a `.png` or a SAROO-compatible 8-bit `.bmp`. |
| **Delete** (per cover) | Remove a cover from the set. |
| **Disc hash…** | Compute a game's IP.BIN Adler-32 hash (and serial) from a Saturn disc image — same as `covertool -i`. Only the first 256 bytes are read. |
| **Export cover.bin** | Rebuild `cover.bin` from the current covers and download it. |

Covers you add or edit live in memory until you **Export cover.bin**.

## Image format

Covers are stored exactly as the CLI stores them:

- **8-bit, 256-color palette**, uncompressed
- Size is **exactly 128 × 192 or 128 × 128** — no other dimension is valid

When you add a `.png` (or any true-color image), it is automatically **quantized to a
256-color palette** (median-cut) and **snapped to whichever valid size best matches its
aspect ratio** (you can override the choice in the editor). `.bmp` files that are already
8-bit/256-color at a valid size are imported losslessly. When exporting a single cover as
`.bmp`, the output is a standard 8-bit BMP that round-trips through the CLI.

The app enforces the two valid sizes: the editor only offers 128 × 192 / 128 × 128 and
refuses to save anything else, and covers loaded from an existing `cover.bin` with a
non-conforming size are flagged (red border + dimension) with a warning before export.

## Compatibility

The web codec mirrors [`tools/covertool/main.c`](https://github.com/tpunix/SAROO/blob/master/tools/covertool/main.c) byte-for-byte:

- Parsing a CLI-generated `cover.bin` and rebuilding it produces an **identical file**.
- `cover.bin` files exported here unpack correctly with `covertool -u`.
- Duplicate images are de-duplicated (shared blob offset) exactly like the CLI.

## `cover.bin` layout (reference)

```
[ 0x4000 index entries × 32 bytes = 0x80000 bytes ]
[ cover blob: palette (256 × 4 BGRA = 0x400) + pixels (width×height, top-down) ] …

index entry (32 bytes):
  char   serial_id[12]   // NUL-padded, up to 11 meaningful chars
  uint32 ip_hash         // disc IP.BIN Adler-32
  uint32 img_offset      // absolute file offset of this cover's blob
  uint16 width           // <= 128
  uint16 height          // <= 192
  char   unused[8]
```

## Credits

SAROO Cover Editor by [Bucanero](https://github.com/bucanero) — GNU GPLv3.
