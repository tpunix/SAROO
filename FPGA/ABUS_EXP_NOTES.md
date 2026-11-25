# Sega A-Bus timing changes: development log

> This is the working log, kept as it was written. It started as a separate
> copy of the design called `FPGA_abus_exp`; on this branch the changes are
> in `FPGA/` itself. Paths such as `quartus_build\...` refer to the
> development PC. The early "not run on hardware / do not flash" warnings were
> true when written; hardware results are at the end of this file, and the
> summary is in `README_SEGA_ABUS.md` at the top of the repository.

A copy of `../FPGA` (SAROO FPGA design, Quartus 13.0/14.1, EP4CE6F17C8) with three
changes aimed at answering Saturn cart accesses fast enough for Sega's RAM cart
timing (ASR0 = 0x23301FF0: 3 waits, external WAIT off). Every change is
marked `[ABUS_EXP]` in the source.

**Status: simulated in ModelSim and compiled in Quartus (fits, meets timing
at 100MHz; see below). Not run on any hardware.** Do NOT flash this to a SAROO: there is no
JTAG header and the SD-card update has no fallback. Compile it in Quartus and
soak-test it on a Cyclone IV dev kit first.

In `tsdram.v` and `memhub.v` some declarations were moved above their first
use (`sim/fix_decl_order.ps1`, marked `[SIM]`). That changes only the order,
not the logic. ModelSim rejects the original order; Quartus accepts both.

## Simulation (ModelSim-Intel FPGA Starter 20.1, `sim/`)
`sim/run.ps1` compiles the original memhub (an order-fixed copy in `sim/orig`)
and this version, each with `tb_memhub.sv` and `sdram_model.sv`, and runs:
- A model of the Saturn SCU on CS0: 28.636MHz, asynchronous to the FPGA.
  55% sequential, 15% repeat, 30% random addresses; 30% writes.
- Random STM32 traffic on port B (0 none, 1 light, 2 heavy).
- Saturn refresh pulses every 15us.
The SDRAM model checks protocol and timing, and the testbench checks every
Saturn read against a shadow copy. The SCU cycle and the SDRAM output timing
are approximations: the comparison between the two designs is what counts,
not the absolute numbers.

1ms per run, 2,846 Saturn reads in the Sega-timing runs:
| Timing | STM32 | Original errors | New errors | Worst read, orig / new |
|---|---|---|---|---|
| Saroo 0x3880 (8 waits, WAIT on) | none | 0 | 0 | 241 / 200ns |
| Saroo 0x3880 | heavy | 0 | 0 | 316 / 276ns |
| 0x3440 (4 waits, WAIT on) | heavy | 0 | 0 | 311 / 266ns |
| Sega 0x2330 (3 waits, WAIT off) | none | 380 (13%) | 17 (0.6%) | - |
| Sega 0x2330 | light | 562 | 230 | - |
| Sega 0x2330 | heavy | ~900 | ~630 | - |
No SDRAM protocol/timing errors and no data-bus contention in any run.
Refresh count is unchanged (138 per ms); the worst refresh gap is 8.00us vs
7.92us before.

Cause of each Sega-timing error (the testbench classifies them):
- Original, idle: 357 too slow, 23 refresh. New, idle: **all 17 are refreshes
  already in progress**. The normal read path now fits.
- Heavy STM32 load: original 698 STM32-in-progress, 165 too slow, 44 refresh;
  new 583 STM32-in-progress, 40 refresh, 1 other.

## Change 4: refresh scheduled around Saturn accesses (`tsdram.v`, `memhub.v`, `cachebus.v`)
The refresh timer (reg_sdram, 7.81us) keeps running while a refresh is
postponed, and counts what is owed in `ref_debt`. Owed refreshes are issued
only in slots where a Saturn access can't arrive before the refresh ends:
- `in_hit`: a Saturn read is being answered from the FPGA's line cache
  (new `hit` output from cachebus), so the SDRAM is free for the rest of
  that bus cycle;
- `after_sat`: the first idle clock after a Saturn SDRAM command;
- `quiet`: CS0 inactive for 640ns (new `sat_cs` input, synchronised);
- `ext_req`: a Saturn refresh cycle (FC1), as before;
- forced if 4 are owed (at most ~31us between refreshes; SDRAM allows 8
  postponed).
Otherwise a waiting Saturn request still goes first (Change 1), and STM32
requests go after a due refresh.
A slot right after CS0 goes inactive was tried and dropped: the next Saturn
access can start ~35ns later and collide with the refresh.

The testbench was also corrected: the Saturn refresh (FC1) is now a bus cycle
of its own, between SCU accesses, as on real hardware. Before, it fired at
random moments, which overstated refresh collisions in every version.

Results with the corrected testbench (Sega timing 0x23301FF0):
| Version | STM32 | Reads | Read errors |
|---|---|---|---|
| Original | none | 2,846 (1ms) | 349 |
| Changes 1-3 | none | 8,416 (3ms) | 24 (all internal refreshes) |
| **Changes 1-4** | none | 8,416 (3ms) | **0** |
| **Changes 1-4** | none | 3 seeds x 10ms, ~84,500 | **0** |
| Changes 1-4 | light | 8,416 | 596 (570 STM32 in progress) |
Saroo timing (8 waits) and the 0x3440 test timing (4 waits) stay at 0 errors,
with or without heavy STM32 traffic, and the worst read time under heavy load
drops to 220ns (was 250-330ns). No SDRAM protocol errors or contention in any
run. Refresh rate: 1,337 in 10.2ms (one per 7.6us on average); the worst gap
is 8.2-9.5us.

**Margin:** the worst Sega-timing read is 130ns and the modelled SCU samples
at 139.7ns, so the margin is about 10ns. Board delays, the bus transceivers
and the real SCU timing aren't modelled, so this has to be confirmed on
hardware.

Later testbench correction: Sega's value has read precharge but no write
precharge (A0RPC=1, A0WPC=0), so a write can be followed at once by the next
access. With that modelled, the `after_sat` slot proved unsafe after Saturn
writes (9 errors), so it is now used after Saturn reads only. The numbers in
the table above predate this; see the final table below.

## Change 5: STM32 access slots (`tsdram.v`, `memhub.v`)
With the STM32 active, almost every remaining error was an STM32 SDRAM access
already in progress when the Saturn asked. Now, while the Saturn is using the
cart with fast cycles, memhub starts an STM32 access only when tsdram's
`stm_ok` says it is safe:
- up to 6 clocks after a Saturn cache hit (`stm_win`), or the first idle
  clock after a Saturn read (`after_sat`); a due refresh takes the slot first;
- **fast cycles only:** tsdram measures how long CS0 stays low. At <=160ns
  (Sega 0x23301FF0, ~140ns) the gate is on. With SAROO's timings (0x3880,
  0x3440: >=175ns, external WAIT on) the Saturn waits for the SDRAM anyway,
  so the STM32 is left alone;
- if CS0 has been idle for `SAT_IDLE_CLKS` (default 3us), the STM32 is free;
- memhub lets an STM32 request through after waiting 2us (`STM_MAX_WAIT`),
  so long runs of Saturn writes (no safe slot) can't starve it.
`STM_GATE=0` on memhub turns Change 5 off for comparison.

Trade-off (bursty Saturn traffic: 5-20% of accesses followed by a 0.5-3us
pause): the first Saturn access after a pause can meet an STM32 access that
started while the Saturn was idle.
| SAT_IDLE_CLKS | Bursty + light STM32: errors | Bursty + heavy: errors | STM32 accesses (heavy) |
|---|---|---|---|
| 1us | 33 | 168 | 13,366 |
| **3us (default)** | 18 | 27 | 4,893 |
| 10us | 10 | 11 | 4,803 |

## Final results (changes 1-5, corrected testbench, 3ms per run)
| Timing | STM32 | Reads | Original errors | Changes 1-5 errors | STM32 accesses orig / new | STM32 worst wait orig / new |
|---|---|---|---|---|---|---|
| Sega 0x2330 | none | 8,855 | 1,078 | **0** | - | - |
| Sega 0x2330 | light | 8,855 | 1,615 | **0** | 7,823 / 5,441 | 310 / 1,600ns |
| Sega 0x2330 | heavy | 8,855 | 2,816 | **0** | 24,135 / 10,180 | 310 / 1,980ns |
| Sega 0x2330, bursty | light | 6,436 | 1,184 | 18 | 7,939 / 4,504 | 300 / 2,230ns |
| SAROO 0x3880 | heavy | 5,042 | 0 | 0 | 26,057 / 26,118 | 320 / 320ns |
| Test 0x3440 | heavy | 7,345 | 0 | 0 | 24,948 / 25,014 | 310 / 300ns |
Also 0 errors in 3 x 10ms Sega-timing runs without STM32 traffic (~88,000
reads, before Change 5; Change 5 doesn't act without STM32 traffic).
No SDRAM protocol errors or data-bus contention in any run. Worst read at
SAROO timing under heavy load: 220ns (original 300ns).

What the STM32 numbers mean: the testbench STM32 waits for each access before
starting the next, so fewer accesses = it was held back. Under Sega timing
it still gets 1.8-3.4 million accesses/s. CD streaming needs far less (2x CD
is ~150,000 words/s), but some STM32 accesses now wait up to ~2us. Whether
the STM32 firmware tolerates that has to be checked.

**Hardware check against the model (2026-11-24):** with the stock FPGA, a
firmware build using 0x34403440 (4 waits, WAIT on) makes the SAROO fail to
boot intermittently, while this testbench shows 0 errors at that timing
(original design included). So the model is optimistic about the real
A-Bus (when the SCU samples WAIT/data, cycle shapes, or something not
modelled, such as the bus transceivers or the menu's STM32 traffic). Treat
all the Sega-timing numbers here as relative until the model is calibrated
against hardware.

Remaining known problems:
- The ~10ns margin on Sega-timing reads (unchanged).
- The first Saturn access after a pause can still meet an STM32 access.
- Whether the real SCU timing matches the model (cycle length, precharge,
  when data is sampled), and whether the CS0-length detection threshold suits
  the real bus.

`SSMaster.v` (pins, bus direction, STM32/FSMC bus, the update register at
0x60000016, EPCS pins) is unchanged. So is the SDRAM mode register (CL=2, BL=4
sequential).

## Quartus compile (Quartus Prime Lite 25.1std, 2026-11-24)
Build copies in `../quartus_build/orig` and `../quartus_build/new`. In those
copies only, SignalTap (`bus.stp` and the others aren't in the repository)
and the `make_jic.tcl` post-flow script are switched off in `SSMaster.qsf`.
Command: `quartus_sh --flow compile SSMaster`.

| | Original | Changes 1-5 |
|---|---|---|
| Result | 0 errors, 44 warnings | 0 errors, 44 warnings (same list) |
| Logic elements (EP4CE6: 6,272) | 1,477 (24%) | 1,675 (27%) |
| Registers | 825 | 948 |
| Memory bits | 32,768 | 32,768 |
| Worst setup slack, 100MHz, slow 85C | +0.508ns | +0.167ns |
| Worst slack inside the 100MHz domain | +1.897ns | +1.349ns |
| Worst hold slack | +0.452ns | +0.452ns |

Timing is met in every corner. The worst paths in both designs are the
original STM32 FSMC address paths (`fsmc_addr`, clocked by ST_ALE) into the
100MHz domain, not the new logic. The new design's worst is
`fsmc_addr[3] -> st_reg_data_out[2]`, which my changes don't touch. It got
worse only through different placement. Inside the 100MHz domain the worst
new path is the cache-coherence update (`req_addr` -> `cache_data`), with
1.35ns to spare.

Caveats (same for both): `SSMaster.sdc` only derives the PLL clocks. There is
no clock uncertainty (Quartus raises a critical warning; real slack is ~0.1-0.2ns
lower) and no I/O constraints, so the Saturn A-Bus, STM32 and SDRAM pin
timing is not analysed by Quartus at all.

The `.rbf` files are in `../quartus_build/*/output_files/`. **Do not flash the
new one to a SAROO**: the pin timing isn't analysed, and there is no JTAG and
no fallback.

## Change 1: Saturn before refresh (`tsdram.v`, `memhub.v`)
Before: in `M_IDLE` a pending refresh always went before a waiting request.
Now a waiting **Saturn** request (memhub passes `cmd_prio = ~act_ab`) goes
first. STM32 requests still go after refresh, as before. A refresh that has
waited 48 clocks (480ns) is forced (`ref_force`), so refresh can't be held off.
Saturn cycles are at least ~14 clocks apart, so the refresh fits in the gaps.
The refresh interval itself (`reg_sdram`, 7.81us) is unchanged.

## Change 2: requested word first (`memhub.v`, `cacheblk.v`)
Before: reads always started the 4-word burst at word 0 of the line
(`cmd_addr[2:0] <= 3'b0`), so the wanted word could arrive last.
Now the burst starts at the requested word. The SDRAM wraps within the 4-word
line by itself (sequential burst), and the cache fill counter starts at the
same word (`dcnt <= iaddr[2:1]`). This saves up to 3 clocks (30ns) per miss.
It applies to both the Saturn and the STM32. Both wait on
`cache_valid[word]`, so the order doesn't matter to them.

## Change 3: capture requests immediately (`cachebus.v`, `memhub.v`)
Before: memhub took the address, write data and mask from the live bus when it
got round to the request, and the read word was picked with the live address.
That is fine while the Saturn is held by WAIT. With WAIT off, the Saturn can
end its cycle first, and a late write could then land at the wrong address
with the wrong data.
Now cachebus latches `req_addr/req_wdata/req_mask` on the clock the miss or
write is seen. memhub, the cache fill/update and the read-word select all use
those.

## Estimated read-miss timing at 100MHz (from the RTL, not measured)
| Step | Time |
|---|---|
| CS0 low -> `ss_rd_start` (3-flop synchroniser) | 20-30ns |
| cachebus miss -> memhub latch -> tsdram `M_IDLE` | ~30ns |
| ACTIVE, tRCD, READ, CL=2 -> first (requested) word | ~50ns |
| cacheblk store -> cache_valid -> rdata | ~20ns |
| Total, SDRAM idle | **~130-140ns** |

Sega's 3-wait budget is roughly 140-175ns, so even the best case is tight.

## Remaining problems (not solved here)
1. **An STM32 access already started** when the Saturn asks still runs to the
   end: up to ~BL+tRP+tRCD ≈ 80-100ns extra. That's the main reason
   Sega timing would still fail occasionally during CD activity.
2. **A refresh already issued** can't be aborted: up to tRFC ≈ 70ns extra.
3. **Clock:** 100MHz on the slowest-grade part. WASCA runs at 133MHz.
4. **Possible next step:** forward `cmd_dout` straight to `rdata` when the
   requested word arrives, skipping the cacheblk register (about -10-20ns).

The normal SAROO timing (external WAIT on) should behave exactly as before.
Regression-test that first.

## Test plan
1. Compile in Quartus 14.1 (open `SSMaster.qpf` here). Fix syntax errors.
   Check TimeQuest meets 100MHz.
2. Simulate `memhub` with a testbench: an SCU model at Sega timing (3 waits,
   no WAIT) plus random STM32 burst traffic and refreshes. Count late reads
   and wrong data.
3. Dev kit (pins re-assigned for the kit): run the same traffic generator
   in hardware for hours, with error/late counters.
4. Only then consider the SAROO, with the recovery tools ready (see
   saroo_patch_exramtest.txt / earlier notes).

Where 0x34403440 fails on hardware (stock FPGA): freeze at the Sega logo, or
the Sega-to-SAROO logo animation not running. That is the SAROO firmware's
own start-up running from CS0 right after crt0.S sets ASR0, while the STM32
is busy with SSCMD_STARTUP (loading the menu into SDRAM). With the same
build, RAMCART-Test, DOOM (asm) and the 4MBRAM demo work. So the failure
appears when the Saturn uses the cart during heavy STM32 traffic, which is
the case the model handles least realistically.

HARDWARE (2026-11-24): user flashed an FPGA update and reports "the update
fixes the out.iso". To confirm: which SSMaster.rbf (experimental changes 1-5,
sha256 1A05A31B...), which ssfirm.bin (official v0.9 without the scan, or the
scan build), and whether saroocfg.txt still had the [EXRAMTEST V1.000]
nop lines. Only the combination without the scan and without the nops
shows the FPGA handling Sega's timing.
  Details: SSMaster.rbf = experimental (quartus_build\new). saroocfg.txt =
  stock v0.9 file, with NO [EXRAMTEST V1.000] section (no nops, no
  M_060050A4). Still to confirm: which ssfirm.bin (official v0.9 or the
  scan build).

  CONFIRMED 2026-11-24: ssfirm.bin on the SD card = official v0.9 (sha256 FB905E2F..., identical file). saroocfg.txt on the card has no EXRAMTEST/M_ lines. So out.iso ran at Sega's real RAM cart timing (0x23301FF0, external WAIT off) on the experimental FPGA (changes 1-5), with no patch. First real-hardware evidence the redesign works; M_060050A4 was not needed either. One test, one console so far.

RAMCART-Stress on the experimental FPGA (2026-11-24, run 1, Sega timing,
CD load OFF): FAIL, 1,197 errors. PROBE, ALIASING, DMA pass. ADDRESS 2,
CACHED 17, RANDOM W/R 1,167, INVERTED 2, CHECKER16 4, RETENTION 5.
First error: 0x2241D630 (test 3) expected 868CA876, got C1B2A876. The low
halfword is right. The high halfword C1B2 is the LOW halfword of the
previous longword (pattern at 0x2241D62C = 0DAEC1B2), i.e. the value the
previous bus read returned. 0x2241D630 starts a new 8-byte FPGA cache line,
so this read was a cache miss: the Saturn sampled before the new data
reached the bus and got the previous read's value. So on real hardware the
read-miss path is a little too slow for Sega's 3 waits: the ~10ns margin
seen in simulation isn't there. Rare in sequential tests (one miss per 4
halfwords), common in RANDOM (every access a miss, writes right before).
No STM32 CD traffic was running, so it's the basic miss path, not change 5.

## Change 6: faster read-miss path (2026-11-24)
Why: RAMCART-Stress on hardware (experimental FPGA, changes 1-5, Sega
timing, CD load off) gave 1,197 errors, all of the "previous read's value"
kind on cache-line misses. The read-miss path was too slow on real hardware.

Calibration: the testbench got SAMPLE_ADV (the SCU samples read data that
many ns early, standing in for transceiver/board/pin delays the model lacks)
and TRAFFIC modes 1 (stress-test random 32-bit write/read/read) and 2
(sequential 32-bit reads). With changes 1-5 the errors start between
SAMPLE_ADV 17 and 18ns (0 at 17; 0.8% sequential / 1.7% random at 18). The
hardware's very low rates (~2 per million sequential, ~0.08% random) put it
right at that edge: real hardware ~= SAMPLE_ADV 17.5ns.

Changes:
- 6a (cachebus.v, memhub.v): the first word of a line fill (the requested
  one, Change 2) goes from the SDRAM straight into the Saturn's read register
  (fill_valid/fill_data) instead of through the cache register: one clock.
- 6c (tsdram.v): ACTIVE is issued in M_IDLE when the request is accepted,
  instead of one clock later in M_ACTIVE (ACT->READ still 2 clocks = tRCD).
- Not done: a 2-flop CS0 synchroniser (it would use the first flop directly).

Result: worst read miss 130 -> 110ns; the error cliff moves from ~17.5ns to
between 35 and 40ns, so about 20ns of margin at the calibrated point.
At SAMPLE_ADV 17.5 (3ms runs): Sega timing 0 errors without STM32 traffic,
with heavy STM32 traffic, and with stress-test traffic plus light or heavy
STM32 traffic; 1 error with mixed traffic + light STM32; 7 with bursty
traffic (the known pause case). SAROO/0x3440 timing 0 errors, STM32 accesses
up (29,047 vs 26,118 heavy). No SDRAM protocol errors.

Quartus 25.1: 1,688 LEs (27%), 949 registers. Worst setup slack 0.407ns
(the original FSMC paths); worst inside the 100MHz domain 0.643ns (cache
coherence update, was 1.35ns). Timing met in all corners.
Image: quartus_build\flash_change6\SAROO\update\SSMaster.rbf (the folder is
laid out like the SD card). Previous image kept as
quartus_build\new\SSMaster_change1-5.rbf.

RAMCART-Stress on change 6 (2026-11-24, run 1, Sega timing, CD load OFF):
7 errors (was 1,197 with changes 1-5). ADDRESS 1, CACHED 1, RANDOM W/R 5,
everything else PASS. First error: 0x22791AC4 (test 3) expected DFF906CA,
got FFF906CA: one bit (bit 13 of the high halfword) wrong. The previous bus
value (low half of 0x22791AC0's pattern = 0x2006) differs from DFF9 in
every bit, and only bit 13 kept its old value: a value caught mid-change.
0x22791AC4 is word 2 of its 8-byte FPGA line, so its read was a cache HIT
(fast). The likely culprit is the WRITE: Change 3 latches the write data
~40ns after CS0 falls (at wr_start), possibly before the Saturn's write data
has settled through the bus transceiver (the previous data on the bus was
0x2006). To confirm: re-read failing addresses (stored wrong = write;
reads right on re-read = read).

RAMCART-Stress 1.1 on change 6 (2026-11-24, run 1, Sega timing, CD off):
6 errors, READ BAD 0, STORED BAD 6, OTHER 0. The first error is the same
address and value as the 1.0 run: 0x22791AC4 = FFF906CA (expected
DFF906CA). The same seed gives the same failure, so it's deterministic and
tied to the data: the previous write on the bus was 0x2006, and all 16 bits
flip to 0xDFF9. CONFIRMED: the wrong data is stored, so the write data is
latched before it has settled (Change 3 latches it at wr_start, ~40ns after
CS0). Reads are fine. -> Change 7.

RAMCART-Stress 1.1 on change 6, run 2 (2026-11-24): timing UNCHANGED (B was
AUTO, the SAROO answered, so no Sega setup: SAROO's own 0x38803880 with
external WAIT on), CD load ON (271 loads, data OK): 7 errors, all STORED
BAD. First: 0x226C6830 expected 2220DFDD stored 2220FFDD: bit 13 again
(the Sega-timing run lost bit 13 too: DFF9 -> FFF9).
=> The write-capture problem is NOT specific to Sega timing. Change 3
latches the write data ~40ns after CS0 whatever the cycle length, and on
this console data line D13 hasn't settled by then. The stock design latched
later (memhub). Changes 3-6 are therefore UNSAFE for normal use (writes
can be corrupted at SAROO timing too). Change 7 latches ~80ns in.
The CD side was fine with change 6 (271 checked loads).

## Change 7: write data latched later (2026-11-24)
- cachebus.v: after a write is seen, req_wdata/req_mask keep sampling the
  bus for WTRACK_CLKS = 4 more clocks; the line-cache update (cache_update)
  happens when the write completes, with the final data.
- memhub.v: in C_WAITACK, cmd_din/cmd_mask follow req_wdata/req_mask until
  tsdram takes them.
- tsdram.v: a Saturn write waits SAT_WR_EXTRA = 2 more clocks before M_WRITE.
Data is now taken ~80ns after CS0 instead of ~40ns, at every timing.
Testbench WDATA_DELAY (new data valid this long after the cycle starts):
change 6 fails from 45ns (40ns ok), change 7 ok up to 80ns (fails at 90).
Regression at SAMPLE_ADV 17.5 + WDATA_DELAY 45: Sega timing 0 errors (no /
heavy STM32, stress random W/R with none / heavy STM32), 1 with mixed +
light STM32, 5 bursty; SAROO and 0x3440 timing 0. No SDRAM protocol errors.
Quartus: 1,704 LEs, 955 registers, worst setup slack 0.274ns (original FSMC
path), worst inside 100MHz 0.403ns; met in all corners.
Image: quartus_build\flash_change7\SAROO\update\SSMaster.rbf; change 6 kept
as quartus_build\new\SSMaster_change6.rbf.

HARDWARE (2026-11-24): change 7 image + RAMCART-Stress 1.1:
- Run 1, Sega timing 0x23301FF0, CD load OFF: all 9 tests PASS, 0 errors.
- Run 2, SAROO's own timing (no setup), CD load ON (146 loads, data OK):
  all PASS, 0 errors. Runs passed 2 of 2.
Change 7 fixed the bit-13 write problem; Sega timing passes on real hardware.

SOAK (2026-11-24, same power-on, auto loop, Sega timing 0x23301FF0 every run):
- Run 100, CD load OFF: runs passed 100 of 100, CD loads so far 817, data OK.
- Run 163, CD load ON: runs passed 163 of 163, CD loads 8,279, data OK,
  0 errors. (~7,460 CD loads during runs 101-163, ~1,690 frames per run.)
RUNS PASSED counts across runs, so no run in the soak failed (per-run error
details are reset each run, but a failed run would show as n < m).
USE (2026-11-24, change 7 image): cold boots OK; Castlevania 4MB hack OK.
Only one console is available, so this has not been tested on another one.
Castlevania (4MB hack) covers the rest: FMV intro plays, the full 4MB cart is
loaded, CD audio plays, and progress saves work (Saroo backup RAM in SDRAM).
Status: change 7 passes every test available on this console.

SEGA-TIMING FIRMWARE (2026-11-25): Firm_Saturn with ASR0 = 0x23301FF0 everywhere
and the per-game timing nops removed. Marvel Super Heroes and KOF96 (1MB cart)
work. But boot and the SAROO menus froze now and then. The firmware runs from
the cart area (linked at 0x02000000), so every menu instruction is fetched from
the SDRAM over CS0, at Sega timing in that build.
- Firmware changed: SAROO_ASR0 (boot, menus, CD player) back to 0x38803880;
  SAROO_GAME_ASR0 = 0x23301FF0 written by the bios_set_clock_speed hook when a
  game starts.
- RAMCART-Stress 1.2 test 10 (CODE FROM CART: 64 KB generated SH-2 blocks
  over the whole cart, run cached and cache-through, at Sega timing, CD load
  on): PASS on hardware. Kronos can't run code from the cart (hangs).
So plain code fetch at Sega timing, even with STM32 CD traffic, does not
reproduce the menu freeze. Not covered by the stress test: the menu's own
STM32 traffic (SD file access, data the STM32 writes into the SDRAM that the
Saturn then reads), and CS1 at Sega timing (0x1FF0 half).

BOOT ANIMATION / BUILD (2026-11-25): every firmware built here with GCC 8.4 lost
the empty delay loops (conio.c logo animation, cdblock.c status poll): the
animation ran in an instant. Fixed with __asm volatile("":::"memory") in the
loop bodies (as cdblock.c already does elsewhere); animation confirmed back.
Result (2026-11-25): with the loops back, the rare boot freezes and menu
freezes are gone too, as well as the animation problem. Every earlier build
from this tree, including the Sega-timing-everywhere one, lacked these loops,
so the freezes blamed on Sega timing in the menus were most likely the missing
delays (cdblock.c status poll with no wait), not the cart timing. The
menu/game ASR0 split stays until Sega timing in the menus is retested with the
fixed loops.
Retest (2026-11-25): ssfirm built with SAROO_ASR0 = 0x23301FF0 (Sega timing
from power-on, delay loops fixed; differs from the release ssfirm.bin only in
the two ASR0 words, crt0.S and _call_04c8) still freezes. So Sega timing for
the firmware's own code does cause freezes, separately from the delay-loop
bug. The split stays. Still unexplained, since RAMCART-Stress code-from-cart
passes at Sega timing: candidates are the menu's own STM32 traffic (SD access,
data the STM32 writes into SDRAM for the Saturn) and CS1 at Sega timing (the
0x1FF0 half; RAMCART-Stress only tests CS0).
CS1 test (2026-11-25): ssfirm with SAROO_ASR0 = 0x23303880 (CS0 Sega 0x2330,
CS1 SAROO 0x3880; otherwise identical to the release ssfirm.bin) also freezes
in boot/menus. CS1 is not the cause: it is CS0 (the SDRAM the firmware runs
from) at 3 waits with external wait off. Most likely a rare late answer to an
instruction fetch while the STM32 is busy with menu work (SD reads, game
list/cover data written into SDRAM), a load RAMCART-Stress does not create.
The fix, if wanted, is in the FPGA; games are unaffected (they run from work
RAM). The release keeps SAROO timing for the menus.

EXTRA RAM (2026-11-25): the FPGA maps the whole 16 MB CS0 window onto SDRAM, so
0x22800000-0x22FFFFFF (8 MB above the 4 MB cart) is RAM on any SAROO. YZB's
"Saroo only" hacks (e.g. D&D Shadow over Mystara) use it. RAMCART-Stress 1.3
test 11 (fill, cached verify, random W/R, check over those 8 MB): PASS on
hardware at Sega timing with CD load on, together with tests 1-10.

AA24 (2026-11-25): the board (marked 1.66) routes cart address line AA24 to the
FPGA pin SS_SCLK. The official v0.9 mcuapp.bin has no ROMCTL; a mcuapp.bin built
from current master (Keil MDK 5.43, see README_SEGA_ABUS.md) has it. With
ROMCTL(1), RAMCART-Stress 1.4 test 12: 64 address pairs 16 MB apart all stay
separate, then fill / cached verify / random W/R / check over
0x23000000-0x23FFFFFF: PASS at Sega timing. Usable cart RAM with AA24 on:
0x22400000-0x23FFFFFF = 28 MB (0x22000000-0x223FFFFF stays the SAROO's own).

SOAK 1.4 (2026-11-25, ~2 h 15 min): RAMCART-Stress 1.4, all 12 tests, Sega timing
0x23301FF0, CD load on: runs passed 100 of 100, ALL RUNS 0, CD loads 30,329
data OK, code self-test OK. Covers the 4 MB cart, code fetch from the cart, the
8 MB above it and the AA24 16 MB, ~4,900 frames per run.

MENU FREEZE CAUSE: COVERS (2026-11-25): with Sega timing everywhere and
/SAROO/cover.bin removed, the menus didn't freeze. With covers, scrolling fast
up and down the game list froze quickly. Every cursor move sends SSCMD_SELECT;
the STM32's load_cover() reads the cover from cover.bin into SDRAM 0x61400c00
(Saturn 0x22400c00) while the menu runs from the same SDRAM, and gif_timer()
SCU-DMAs it to VDP2. The CD sector buffer is in the STM32's internal AXI SRAM
(0x2400a000, 200 x 2352), not the SDRAM, which is why games were never hit.
Fix in Firm_Saturn/main.c: select_notify() only records a cursor move;
select_poll() (called from gif_timer) sends SSCMD_SELECT once the cursor has
rested 15 frames. Enter/exit are sent at once. No SS_CMD==0 check: the STM32
never clears SS_CMD after SSCMD_SELECT (a first build with the check loaded
only the first cover). SAROO_ASR0 is now 0x23301FF0 (Sega timing from
power-on). Fast scrolling with covers: no freeze. One console only.

MENU IN WORK RAM (2026-11-25, branch menu-wram): the
menu freezes at Sega timing came from the menu running from the cart while the
STM32 writes the SDRAM (cover loading). Firm_Saturn now builds two images:
- resident (cart, 0x02000000, ldscript_res): sysid, crt0 (boot stub), and what
  games call after they start: game_load.c, game_save.c, game_patch.c, plus
  their own copies of res_util.c (helpers moved unchanged out of main.c and
  sci_shell.c), cdblock.c, string.c, printk.c, ubr_debug.c, libc/libgcc;
- menu (low work RAM 0x002C0000-0x002D2600, ldscript_menu): everything else,
  with its own copies of the same helpers, its variables and its 16 KB stack.
  Stored at the end of the resident image (menu_blob.S, 51,824 bytes).
crt0 copies the menu to work RAM at SAROO timing (0x38803880, external WAIT
on), re-reads the cart and compares every word (copies again on a
difference), clears both BSS areas, purges the SH-2 cache, sets SAROO_ASR0
(Sega 0x23301FF0) and jumps to the menu. The menu reaches resident code only
through bios_cd_cmd / my_cdplayer (game and CD player start) and writes the
resident copies of debug_flag, bios_type, to_stm32 and printk_putc once at
start-up (plain data writes). A scan of the menu image found no other
reference into the cart's code area. The menu still reads data from the cart:
the game list, covers (after the STM32's frame flag), the CJK font. A misread
there is a wrong character or pixel, not a crash.
Build: make (two-pass link, objcopy --just-symbols) or build_menu_wram.ps1;
both give the same ssfirm.bin (484,246 bytes, SHA-256 85D6A41F...).
Placement: top of low work RAM so BINs loaded at 0x00200000 don't overwrite
the menu while loading; the 64-byte BIOS trampoline at 0x00280000 used at
game start is below it.
Hardware (2026-11-25, change 7 FPGA, Sega timing from power-on, cover.bin in
place): boots, boot animation OK, fast scrolling through the menu with covers:
no freezes. King of Fighters '96 (1MB cart) starts and works; Castlevania 4MB
hack works, its save reloads after a power cycle; CD player and reset back
to the menu work. All checks passed.
