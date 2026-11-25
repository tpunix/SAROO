# SAROO: Sega RAM-cart A-Bus timing (branch `sega-abus-timing`)

**Experimental. Use at your own risk.** This is an unofficial fork of
[tpunix/SAROO](https://github.com/tpunix/SAROO). It is not from tpunix and
has been tested on only one console.

## What it does

Games and demos that set up the RAM cart the way Sega documents it (Sega
Technical Bulletin #47: `ASR0 = 0x23301FF0`, 3 wait states, external WAIT off)
don't work on a stock SAROO. The stock FPGA needs the bus to wait for it
(SAROO sets `ASR0 = 0x38803880`). On the stock firmware these games need a
per-game patch.

This branch has two independent changes:

1. **FPGA (`FPGA/`)**: the SDRAM path answers fast enough for Sega's timing,
   so these games run without patches. Every change is marked `[ABUS_EXP]` in
   `tsdram.v`, `memhub.v`, `cachebus.v` and `cacheblk.v`:
   1. Saturn requests go before internal refresh.
   2. Reads start at the requested word (critical word first).
   3. Address and data are latched when the request is seen.
   4. Refresh is scheduled into gaps: after Saturn reads, during cache hits,
      when the bus is idle, and forced only when 4 refreshes are owed.
   5. STM32 (CD side) accesses are held off while the Saturn is using the
      cart at fast timing, with a 2 µs limit so they can't starve.
   6. Faster read misses: the first SDRAM word goes straight to the bus, and
      ACT is issued directly from idle.
   7. Write data is sampled again until about 80 ns into the cycle. On the
      test console, data line D13 settles late.

   Lines marked `[SIM]` only move declarations up so ModelSim accepts the
   files. They don't change the logic.

2. **Saturn firmware (`Firm_Saturn/`)**: runs everything at Sega timing.
   **It needs the new FPGA. Don't use it with the stock FPGA.**
   - The firmware's own A-Bus value is Sega's `0x23301FF0` (set in one place,
     `abus.h`). SAROO used `0x38803880`.
   - tpunix's per-game patches that `nop` a game's own timing write are
     commented out (marked `[SEGA_ABUS]` in `game_patch.c`), so those games
     set Sega timing themselves, as on a real console. The patches' other
     fixes (cart type, game hacks) are unchanged. Not changed, because the
     timing write there is mixed with other hacks: KOF95 (2nd version),
     Pocket Fighter, and Ultraman (2MB ROM cart, `0x34403440`).
   - The earlier `fix_abus_timing()` scan is removed.

   To build firmware for the stock FPGA again, use upstream `Firm_Saturn`, or
   build this one with `-DSAROO_ASR0=0x38803880` and the `[SEGA_ABUS]`
   lines restored.

## Test results (one console, 2026-11-24)

| Test | Result |
|---|---|
| `tools/RAMCART-Stress` soak at Sega timing | 163 of 163 runs passed, 0 errors |
| …the same with continuous CD loading (STM32 busy on the same SDRAM) | 8,279 CD loads, data OK |
| SAROO's own timing, CD loading on | pass |
| Cold boots | OK |
| Castlevania 4MB hack: FMV intro, CD audio, full 4MB cart, saves | OK |
| Unpatched 4MB RAM demo at Sega timing | runs |

Quartus: 1,704 LEs, 955 registers, timing met in all corners (worst setup
slack 0.274 ns, on the original FSMC path).

The tests above used SAROO's official v0.9 Saturn firmware (and the scan
build). **The Sega-timing firmware on this branch has not been tested on
hardware yet.**

Not tested: other consoles. The late D13 write data may differ from console
to console. Write data is latched about 80 ns into the cycle, and simulation
fails at 90 ns, so a slower console has little margin. The cart pins have no
I/O timing constraints in the `.sdc` file, so a rebuild could move the timing
a little. Re-run RAMCART-Stress after any rebuild.

## Installing

> **The SAROO FPGA update has no verify step and no fallback.** If the
> FPGA image is bad, recovery means opening the cart and using JTAG on the
> unpopulated pads. Back up `SS_SAVE.BIN` and `SS_MEMS.BIN` from the SD card
> first.

1. Download `SSMaster.rbf` from this fork's Releases (or build `FPGA/` in
   Quartus).
2. Copy it to `/SAROO/update/SSMaster.rbf` on the SD card and update from the
   SAROO menu (or set `auto_update=1` under `[global]` in `saroocfg.txt`).
3. Optional, to run everything at Sega timing: copy `ssfirm.bin` from the
   release to `/SAROO/ssfirm.bin`. Keep your old one: the firmware is loaded
   from the SD card at every boot, so going back is just a file swap.
4. To go back to stock, put back the official `ssfirm.bin` **first**, then
   flash the official `SSMaster.rbf` from the official SAROO release.

## Files

| Path | What |
|---|---|
| `FPGA/` | FPGA design with the changes |
| `FPGA/ABUS_EXP_NOTES.md` | Full development log: every change, simulation tables, hardware results |
| `FPGA/sim/` | ModelSim testbench: SCU bus model, SDRAM model with protocol checks, `run.ps1` |
| `Firm_Saturn/` | Firmware: Sega timing everywhere, A-Bus timing patches off |
| `tools/RAMCART-Stress/` | Saturn test disc (jo engine): stress-tests a RAM cart at Sega's timing |

## License

GPL-3.0-only, the same as SAROO (see `LICENSE`). The original design and
firmware are © tpunix. The changes on this branch are marked in the source.
