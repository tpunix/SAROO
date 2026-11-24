/*
 * SAROO A-Bus timing (SCU ASR0, 0x25FE00B0), CS0 in the upper half, CS1 in
 * the lower half. One value used everywhere the firmware sets ASR0:
 * crt0.S (boot), game_load.c (clock-speed hook, CD player) and the
 * fix_abus_timing() scan.
 *
 *   0x38803880  8 waits, external wait on   (upstream default since Oct 2023)
 *   0x34403440  4 waits, external wait on   (upstream default Feb-Oct 2023)
 *
 * Override at build time with -DSAROO_ASR0=0x........
 * Included by crt0.S, so keep this file to preprocessor lines only.
 */
#ifndef SAROO_ASR0
#define SAROO_ASR0 0x38803880
#endif
