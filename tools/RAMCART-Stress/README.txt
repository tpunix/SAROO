RAMCART-Stress 1.2 - RAM cartridge stress test at Sega's own cart timing
=========================================================================
1.2: test 10 CODE FROM CART (SH-2 code run from the cart, like the SAROO
     firmware), and error totals over all runs (ALL RUNS).
1.1: errors are re-read and classified as READ BAD / STORED BAD / OTHER.

Disc image: game.iso / game.cue (build: compile.bat).

Building: this is a jo engine (jo-engine.org) project. Copy this folder into
the jo engine "Projects" folder (the makefile expects ../../jo_engine and
../../Compiler) and run compile.bat. cd/LOAD.BIN is the CD-load test file:
32-bit big-endian word n = (n * 2654435761) XOR 0x5A5AA5A5, 32768 words.

What it does
- Every run writes Sega's TB#47 cart setup first: 0x257EFFFE = 1,
  ASR0 = 0x23301FF0 (3 wait states, external WAIT off), AREF = 0x13.
  The cart has to answer in time with no bus stretching.
- 10 tests over the whole 4 MB (or 1 MB) cart: probe, aliasing, address
  pattern, CACHED READ (cache-line fills through 0x02xxxxxx), RANDOM W/R
  (write A, read B, read A, no gap), inverted, 16-bit checkerboard,
  3 s retention, SH-2 DMA, CODE FROM CART.
- 10 CODE FROM CART: the SAROO firmware runs from the cart area
  (0x02000000), so all its instructions are fetched from the cart. Each
  frame a 64 KB block of generated SH-2 code (add/rotl/not on r0, with a
  known result) is written to the next part of the cart, then called
  4 times through the cache (cache purged first; the block is bigger than
  the 4 KB cache, so everything comes in as cache-line fills) and once
  cache-through (one bus read per instruction). The result is checked, then
  the block is read back as data.
  Wrong result, block reads back right -> READ BAD (an instruction fetch
  was wrong). A wrong fetch can also crash the CPU: if the screen stops
  changing during test 10, that is the failure. The bottom line shows the
  step (WRITE, UNCACHED, PURGE n, CACHED n, VERIFY), the block address and
  the self-test result at the moment it stopped.
  Before the first block, the same code is run from work RAM (CODE
  SELFTEST): OK proves the test program itself computes the right answer.
  EMULATORS: Kronos can't run code from the cart area and hangs at
  "UNCACHED"; press Z to switch test 10 off there.
- CD LOAD: LOAD.BIN (128 KB) is read from the disc in the background
  continuously (16 KB per frame) and checked. On a SAROO that keeps the
  STM32 busy on the same SDRAM while the cart is hammered.
- Loops forever by default. One run takes roughly 15-20 seconds.

Pad
  START  restart: clears ALL RUNS, the kept failure and RUNS PASSED
  A      loop on/off (on at start)
  B      cart setup for the next run: SEGA TB47 (default) / AUTO / NONE
  C      CD load on/off (on at start)
  Z      test 10 CODE FROM CART on/off (on at start; off shows "OFF (Z)")

Reading the screen
  TIMING THIS RUN  yellow "SEGA ASR0 23301FF0" = Sega timing in force
  tests 1-10       PASS / FAIL <error count>
  CD LOAD ...      LOADS = completed loads; DATA OK, or BAD x/y in red
                   (x = loads with wrong data, y = loads that failed to start)
  ERRORS           errors in this run
  ALL RUNS n IN m RUNS  (1.2) errors over all runs since power-on or START,
                   and how many runs had errors; green "ALL RUNS 0" = none
  FIRST AT / EXPECTED / GOT  the first error of this run (red)
  RUN n FAILED: ... (1.2, yellow) when this run is clean but an earlier one
                   failed: the first failed run's first error, kept until
                   START
  READ BAD / STORED BAD / OTHER  (yellow, shown when there are errors)
                   every error is re-read twice at once:
                   READ BAD   = the re-reads are right: the RAM holds the
                                right data, the first read came back wrong
                   STORED BAD = the re-reads give the same wrong value:
                                the wrong data is in the RAM (a write failed)
                   OTHER      = the re-reads disagree, or a DMA timeout
                   "ALL: READ / STORED / OTHER" = the same over all runs
  1ST STORED BAD addr = value   (bottom line) the first STORED BAD case
  RUNS PASSED n OF m

Expected results
  Real Sega or Action Replay 4 MB cart       PASS
  SAROO, stock v0.9 FPGA                     FAIL (it can't take Sega timing)
  SAROO, sega-abus-timing FPGA (change 7)    1.1: PASS (163/163 runs,
                                             2026-11-24); test 10 is new
  Kronos (4 MB cart selected)                PASS with test 10 off (Z);
                                             test 10 hangs (see above)

Useful comparisons on a SAROO
  - C off vs on: errors only with CD LOAD on point at STM32 contention.
  - Errors only in 4 CACHED READ or 10 CODE: cache-line fills behave
    differently from single reads.
  - Errors only in 5 RANDOM W/R: write followed at once by a read.
  - B -> NONE: the SAROO's own timing stays in force; should always pass.

Safety on a SAROO
  Nothing here writes saves, but a cart timing that fails can corrupt
  anything in the SAROO's SDRAM, its emulated backup RAM included.
  Back up SS_SAVE.BIN and SS_MEMS.BIN from the SD card first.
  The test destroys the cart RAM contents (that's fine on a SAROO).
