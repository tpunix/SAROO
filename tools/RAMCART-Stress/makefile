# RAMCART-Stress: RAM cartridge stress test at Sega's cart timing (4 MB / 1 MB), with CD load

JO_GAME_TITLE = RAM CART STRESS
# J: control pad, W: RAM cartridge
JO_GAME_PERIPHERALS = JW

JO_COMPILE_WITH_VIDEO_MODULE = 0
JO_COMPILE_WITH_BACKUP_MODULE = 0
JO_COMPILE_WITH_TGA_MODULE = 0
JO_COMPILE_WITH_AUDIO_MODULE = 0
JO_COMPILE_WITH_3D_MODULE = 0
JO_COMPILE_WITH_PSEUDO_MODE7_MODULE = 0
JO_COMPILE_WITH_EFFECTS_MODULE = 0
JO_COMPILE_WITH_DUAL_CPU_MODULE = 0
# MUST stay 0: with it the engine initializes the cartridge itself and gives its RAM to jo_malloc().
# This program has to be the only one that touches the cartridge.
JO_COMPILE_WITH_RAM_CARD_MODULE = 0
JO_DEBUG = 1
JO_NTSC = 1
SRCS=main.c
JO_ENGINE_SRC_DIR=../../jo_engine
COMPILER_DIR=../../Compiler
include $(COMPILER_DIR)/COMMON/jo_engine_makefile
