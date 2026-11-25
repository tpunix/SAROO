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
   - Sega's `0x23301FF0` from power-on: the SAROO boot, menus and CD player
     (`SAROO_ASR0` in `abus.h`) and games (`SAROO_GAME_ASR0`, written by the
     clock-speed hook when the game starts).
   - **The menu runs from Saturn work RAM**, not from the cart. `ssfirm.bin`
     holds two images:
     - a resident one that runs from the cart (`ldscript_res`): the boot
       stub, the IP header, and what games call after they start (game start
       in `game_load.c`, save emulation in `game_save.c`, game patches in
       `game_patch.c`), with its own copies of the helpers they use;
     - the menu (`ldscript_menu`), linked for low work RAM
       (`0x002C0000–0x002D2600`, code, data, variables and stack), stored at
       the end of the resident image.

     At power-on the boot stub (`crt0.S`) copies the menu at SAROO timing
     (the cart can hold the CPU, so the copy is always right), compares it
     with the cart word by word (copies again on a difference), switches to
     Sega timing and starts it. The menu calls the cart only to start a game
     or the CD player. Helpers both sides use moved unchanged into
     `res_util.c`, which is linked into both images, so the cart code never
     calls into work RAM (which a game overwrites). The menu still reads
     data from the cart (game list, covers after the STM32's ready flag, CJK
     font); a bad read there would be a wrong character or pixel.

     Build with `make` (two-pass link: the resident image, then the menu
     against its symbols with `--just-symbols`, then the resident image
     again with the menu inside) or, on Windows, `build_menu_wram.ps1`; both
     give the same `ssfirm.bin`.
   - **Debounced covers** (`select_poll()` in `main.c`): a cursor move in the
     game list only records the game; the cover is requested from the STM32
     once the cursor has rested for 15 frames (~¼ s). Entering and leaving
     the list are sent at once.

     Why: the menu runs from the cart area, so every instruction is fetched
     from the SAROO's SDRAM. Upstream, every cursor move makes the STM32 read
     a cover from `cover.bin` into that same SDRAM. At Sega timing the
     Saturn can't wait for the SDRAM (external wait is off), so scrolling
     fast through the list made an instruction fetch collide with the STM32's
     writes sooner or later, and the menu froze. Tests (2026-11-25, one
     console): Sega timing in the menus froze, also on CS0 only
     (`0x23303880`, CS1 ruled out); with `cover.bin` removed it didn't
     freeze; fast scrolling with covers froze; with the debounce, fast
     scrolling with covers doesn't freeze. Games don't run from the cart and
     the CD buffer is in the STM32's own RAM, so games weren't affected. With
     the menu in work RAM as well, the menu fetches no code from the cart at
     all.

     For the earlier split (SAROO timing in the menus, Sega timing in games)
     build with `-DSAROO_ASR0=0x38803880`.
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
     animation runs in an instant and looks skipped, and boot and the menus
     freeze now and then (the CD status poll runs with no delay). With the
     loops back, those freezes and the animation problem are gone (tested
     2026-11-25).

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
| RAMCART-Stress 1.4 soak, all 12 tests (4MB cart, code from cart, 8MB extra RAM, AA24 16MB), CD loading on (2026-11-25) | 100 of 100 runs, 0 errors, 30,329 CD loads OK |

Quartus: 1,704 LEs, 955 registers, timing met in all corners (worst setup
slack 0.274 ns, on the original FSMC path).

The tests above used SAROO's official v0.9 Saturn firmware (and the scan
build). The firmware on this branch (menu in work RAM, Sega timing from
power-on, debounced covers), 2026-11-25, change 7 FPGA, `cover.bin` in place:

| Test | Result |
|---|---|
| Cold boot, Sega→SAROO animation | OK |
| Fast scrolling through the game list with covers | no freezes |
| King of Fighters '96 (1MB cart) | OK |
| Castlevania 4MB hack; save, reload after a power cycle | OK |
| CD player; reset from a game back to the menu | OK |

Marvel Super Heroes also worked with the earlier Sega-timing build.

Not tested: other consoles. The late D13 write data may differ from console
to console. Write data is latched about 80 ns into the cycle, and simulation
fails at 90 ns, so a slower console has little margin. The cart pins have no
I/O timing constraints in the `.sdc` file, so a rebuild could move the timing
a little. Re-run RAMCART-Stress after any rebuild.

## Extra cart RAM on a SAROO

The FPGA maps the Saturn's whole 16MB cart window onto the SAROO's SDRAM, so
there is more RAM than the 4MB cart. Only software that knows about it can use
it (check for the SAROO first: `0x5253`, "SR", at `0x25807000`); real Sega carts
and emulators don't have it.

| Range (cache-through) | Size | Needs | Tested here |
|---|---|---|---|
| `0x22000000–0x223FFFFF` | 4MB | — | **SAROO's own** (firmware, saves): don't use |
| `0x22400000–0x227FFFFF` | 4MB | — | standard RAM cart: RAMCART-Stress tests 1–10 |
| `0x22800000–0x22FFFFFF` | 8MB | any SAROO | test 11: PASS at Sega timing |
| `0x23000000–0x23FFFFFF` | 16MB | AA24 board + `ROMCTL` | test 12: PASS at Sega timing |

So 12MB (`0x22400000–0x22FFFFFF`) on any SAROO, 28MB (`0x22400000–0x23FFFFFF`)
with AA24. YZB's "Saroo only" hacks use the 16MB window (not AA24).

**AA24** (the upper 16MB): the board must route cart address line AA24 to the
FPGA (tested on a board marked 1.66), and the MCU firmware must have
`SSCMD_ROMCTL` (added to master in Oct 2026; the official v0.9 `mcuapp.bin`
doesn't have it). A program switches it on itself; the SAROO menu switches it
off again:

```c
#define SS_CMD (*(volatile unsigned short *)0x25897010)
#define SS_ARG (*(volatile unsigned short *)0x25897014)
SS_ARG = 1;              /* 1 = AA24 on, 0 = off */
SS_CMD = 0x0041;         /* SSCMD_ROMCTL */
while (SS_CMD) ;         /* use a timeout: an older MCU firmware never answers */
/* SS_ARG now holds ST_CTRL bits 15..8; bit 4 set = AA24 on */
```

Without AA24, `0x23xxxxxx` is a copy of `0x22xxxxxx`, so writing there would
overwrite the SAROO's own RAM: always check first (RAMCART-Stress test 12 shows
a safe way).

### Building `mcuapp.bin` with Keil MDK 5.43

`Firm_MCU` is a Keil µVision project (target `App` → `mcuapp.bin`; never put the
`BootLoader` output, `mcuboot.bin`, on the SD card: that one is flashed). It was
made with Arm Compiler 6.13 and older packs. With MDK 5.43 (free MDK-Community
licence: `armlm activate --server https://mdk-preview.keil.arm.com --product
KEMDK-COM0`) these changes were needed, in a copy of the project:

1. Packs: `Keil::STM32H7xx_DFP` **2.3.1** (4.x has no `stm32h7xx.h`) and
   `ARM::CMSIS` **5.9.0** (its RTX5 library matches the project's short
   enums/wchar; the separate `ARM::CMSIS-RTX` pack's library doesn't). Pin both
   in *Select Software Packs* (untick "Use latest versions").
2. Compiler: select the installed Arm Compiler 6 (the project names 6.13.1).
3. Add `…\STM32H7xx_DFP\2.3.1\Drivers\CMSIS\Device\ST\STM32H7xx\Include` to the
   include path if µVision doesn't find `stm32h7xx.h`.
4. `inc/main.h`: newer CMSIS declares `__disable_irq()` as `void`, but the code
   uses its return value as the old PRIMASK. Use
   `#define disable_irq() ({ uint32_t _pm = __get_PRIMASK(); __disable_irq(); _pm; })`.

Result: 62,600 bytes, same vector table as the official v0.9 build. The
bootloader loads `/SAROO/mcuapp.bin` from the SD card at every power-up, so
trying one is a file swap.

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
| `Firm_Saturn/` | Firmware: menu in work RAM, Sega timing everywhere, A-Bus timing patches off |
| `tools/RAMCART-Stress/` | Saturn test disc (jo engine): stress-tests a RAM cart at Sega's timing |

## License

GPL-3.0-only, the same as SAROO (see `LICENSE`). The original design and
firmware are © tpunix. The changes on this branch are marked in the source.
