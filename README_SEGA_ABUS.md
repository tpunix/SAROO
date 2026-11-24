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

2. **Saturn firmware (`Firm_Saturn/`)**: a fallback for the **stock** FPGA.
   When a game loads, `fix_abus_timing()` in `game_load.c` scans
   0x06004000–0x06100000 for the Sega value `0x23301FF0` and replaces it with
   SAROO's own timing. The SAROO value is set in one place (`abus.h`).
   You don't need this with the new FPGA.

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
3. To go back, do the same with the official `SSMaster.rbf` from the
   official SAROO release.

## Files

| Path | What |
|---|---|
| `FPGA/` | FPGA design with the changes |
| `FPGA/ABUS_EXP_NOTES.md` | Full development log: every change, simulation tables, hardware results |
| `FPGA/sim/` | ModelSim testbench: SCU bus model, SDRAM model with protocol checks, `run.ps1` |
| `Firm_Saturn/` | Firmware with the A-Bus timing scan |
| `tools/RAMCART-Stress/` | Saturn test disc (jo engine): stress-tests a RAM cart at Sega's timing |

## License

GPL-3.0-only, the same as SAROO (see `LICENSE`). The original design and
firmware are © tpunix. The changes on this branch are marked in the source.
