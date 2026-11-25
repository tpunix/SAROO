/*
 * SAROO A-Bus timing (SCU ASR0, 0x25FE00B0), CS0 in the upper half, CS1 in
 * the lower half.
 *
 * SAROO_ASR0       the SAROO firmware's own timing: boot (crt0.S), menus and
 *                  the CD player (game_load.c _call_04c8). The firmware runs
 *                  from the cart area (0x02000000), so every instruction it
 *                  runs is fetched over the A-Bus.
 * SAROO_GAME_ASR0  written when a game starts (the bios_set_clock_speed hook
 *                  in game_load.c, code_06b8 and my_06b0), so games run at
 *                  this timing unless they set their own.
 *
 *   0x23301FF0  Sega RAM cart timing (TB#47): 3 waits, external wait off.
 *               Needs the sega-abus-timing FPGA; the stock FPGA can't answer
 *               this fast.
 *   0x38803880  8 waits, external wait on   (upstream default since Oct 2023)
 *   0x34403440  4 waits, external wait on   (upstream default Feb-Oct 2023)
 *
 * The menus stay at SAROO's timing: with Sega timing everywhere, boot and the
 * menus froze now and then (2026-11-25).
 *
 * On this branch the per-game patches that stopped games from setting Sega's
 * timing are commented out (marked [SEGA_ABUS] in game_patch.c), so games
 * set their own timing as on real hardware.
 *
 * Override at build time with -DSAROO_ASR0=0x........ / -DSAROO_GAME_ASR0=...
 * Included by crt0.S, so keep this file to preprocessor lines only.
 */
#ifndef SAROO_ASR0
#define SAROO_ASR0 0x38803880
#endif

#ifndef SAROO_GAME_ASR0
#define SAROO_GAME_ASR0 0x23301FF0
#endif
