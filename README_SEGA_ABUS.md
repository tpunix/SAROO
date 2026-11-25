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

2. **Saturn firmware (`Firm_Saturn/`)**: runs games at Sega timing.
   **It needs the new FPGA. Don't use it with the stock FPGA.**
   - Games start at Sega's `0x23301FF0` (`SAROO_GAME_ASR0` in `abus.h`,
     written by the clock-speed hook when the game starts). The SAROO boot,
     menus and CD player stay at SAROO's `0x38803880` (`SAROO_ASR0`): the
     firmware runs from the cart area, and with Sega timing there too, boot
     and the menus froze now and then.
   - tpunix's per-game patches that `nop` a game's own timing write are
     commented out (marked `[SEGA_ABUS]` in `game_patch.c`), so those games
     set Sega timing themselves, as on a real console. The patches' other
     fixes (cart type, game hacks) are unchanged. Not changed, because the
     timing write there is mixed with other hacks: KOF95 (2nd version),
     Pocket Fighter, and Ultraman (2MB ROM cart, `0x34403440`).
   - The earlier `fix_abus_timing()` scan is removed.
   - Builds with a newer GCC (8.4 here): five empty delay loops (the
     Sega→SAROO boot animation in `conio.c`, a CD status poll in
     `cdblock.c`) now contain `__asm volatile("":::"memory")`, as tpunix
     already does elsewhere, so the compiler keeps them. Without it the boot
     animation runs in an instant and looks skipped.

   To build firmware for the stock FPGA again, use upstream `Firm_Saturn`, or
   build this one with `-DSAROO_ASR0=0x38803880` and the `[SEGA_ABUS]`
   lines restored.

### Games whose timing patch is off

These games now set Sega's timing (`0x23301FF0`) themselves, as on a real
console. Their other patches (1MB/4MB cart type, KOF97's 4MB-version code
changes) still apply.

| Game | Serial(s) |
|---|---|
| Dungeons & Dragons Collection (Disc 2) | T-1245G |
| Noel 3 | T-22205G |
| Friends | T-20109G |
| Astra Superstars | T-1521G |
| Magical Night Dreams: Cotton 1 | T-9906G |
| Magical Night Dreams: Cotton 2 | T-9904G |
| Street Fighter Zero 3 | T-1246G |
| Metal Slug v1.002 | T-3111G |
| Metal Slug v1.005 | T-3111G |
| King of Fighters '96 | T-3108G |
| King of Fighters '97 (1MB-cart version) | T-3121G, T-4401G |
| Samurai Spirits 3 | T-3104G |
| Samurai Spirits 4: Amakusa Kourin | T-3116G |
| Real Bout Garou Densetsu | T-3105G |
| Real Bout Garou Densetsu Special v1.001 | T-3119G |
| Real Bout Garou Densetsu Special v1.002 (Rev A) | T-3119G |
| Super Real Mahjong P7 | T-16509G |
| Super Real Mahjong P7 SP | T-16510G |
| Gouketsuji Ichizoku 3: Groove on Fight | T-14411G |
| Vampire Savior | T-1229G |
| Marvel Super Heroes | T-1215G (JP), T-1214H (US), T-7032H (EU), 6106664 (JP demo) |
| X-Men vs. Street Fighter | T-1226G |
| Fighter's History Dynamite | GS-9107 |
| Final Fight Revenge | T-1248G |
| Pia Carrot e Youkoso!! 2 | T-20114G |

Timing patch left as it was:

| Game | Serial(s) | Why | Runs at |
|---|---|---|---|
| King of Fighters '95 | MK-81088, T-3101G | Timing write mixed with other code changes in one version | Sega timing (firmware value) |
| Pocket Fighter | T-1230G | tpunix rewrote that code instead of using a `nop` | Sega timing (firmware value) |
| Ultraman | T-13308G | 2MB ROM cart; the firmware sets the timing itself | `0x34403440` |

KOF95 and Ultraman also need a dump of their 2MB ROM cart on the SD card, as
`/SAROO/ISO/KOF95.BIN` and `/SAROO/ISO/ULTRAMAN.BIN`. Without it they crash
on any SAROO firmware, official or this one.

Tested with this firmware on the change 7 FPGA (2026-11-25): **Marvel Super
Heroes** and **King of Fighters '96** work (both 1MB-cart games). The others
are not tested yet. Patch lines in
`saroocfg.txt` are applied after the built-in patches and can still block a
game's timing write.

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
build). The Sega-timing firmware on this branch has only had a first test:
Marvel Super Heroes and KOF96 work (see the game list above).

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
