/*
 * SAROO A-Bus timing (SCU ASR0, 0x25FE00B0), CS0 in the upper half, CS1 in
 * the lower half. One value used everywhere the firmware sets ASR0:
 * crt0.S (boot) and game_load.c (clock-speed hook, CD player).
 *
 *   0x23301FF0  Sega RAM cart timing (TB#47): 3 waits, external wait off.
 *               Default on the sega-abus-timing branch. NEEDS the
 *               sega-abus-timing FPGA; the stock FPGA can't answer this fast.
 *   0x38803880  8 waits, external wait on   (upstream default since Oct 2023)
 *   0x34403440  4 waits, external wait on   (upstream default Feb-Oct 2023)
 *
 * On this branch the per-game patches that stopped games from setting Sega's
 * timing are commented out (marked [SEGA_ABUS] in game_patch.c), so games
 * set their own timing as on real hardware.
 *
 * Override at build time with -DSAROO_ASR0=0x........
 * Included by crt0.S, so keep this file to preprocessor lines only.
 */
#ifndef SAROO_ASR0
#define SAROO_ASR0 0x23301FF0
#endif
