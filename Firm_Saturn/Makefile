#
# Sega Saturn USB flash cart ROM
# by Anders Montonen, 2012
# Creative Commons Attribution-ShareAlike 3.0 Unported (CC BY-SA 3.0)
#

CC	=	sh-elf-gcc
AS	=	sh-elf-as
OBJDUMP = sh-elf-objdump
OBJCOPY = sh-elf-objcopy
#LDFLAGS	=	-nostartfiles -nostdlib -T ldscript
LDFLAGS	=	-nostartfiles -T ldscript
FLAGS	=	-Wall -m2 -Os -fomit-frame-pointer -std=c99

LIBS	=	-lgcc

EXE	=	ssfirm.elf

OBJ	=	obj/crt0.o  \
		obj/main.o  \
		obj/sysid.o \
		obj/conio.o \
		obj/sci_shell.o  \
		obj/ubr_debug.o  \
		obj/cdblock.o  \
		obj/game_load.o  \
		obj/game_patch.o  \
		obj/game_save.o  \
		obj/language.o  \
		obj/crc32.o  \
		obj/string.o \
		obj/printk.o \
		obj/tiny_xm.o \
		obj/version.o

define ECHO_CMD
	$(if $(V), , @echo "    "$(1)" " $(2))
	$(if $(V), $(3), @$(3))
endef

all	: obj BUILD_VER $(EXE)

obj:
	$(call ECHO_CMD, "MKDIR  ", obj,      mkdir obj)

BUILD_VER:
	$(call ECHO_CMD, "TOUCH  ", version.c, touch version.c)

$(EXE)	: $(OBJ)
	$(call ECHO_CMD, "LD     ", $@,       $(CC) $(LDFLAGS) $(OBJ) $(LIBS) -o $(EXE))
	$(call ECHO_CMD, "OBJDUMP", dump.txt, $(OBJDUMP) -xd $(EXE) > dump.txt)
	$(call ECHO_CMD, "OBJCOPY", tmp.bin,  $(OBJCOPY) -O binary $(EXE) tmp.bin)
	$(call ECHO_CMD, "CAT    ", ssfirm.bin, cat tmp.bin font_cjk.bin >ssfirm.bin)
	$(call ECHO_CMD, "RM     ", tmp.bin,  rm -f tmp.bin)

$(OBJ): main.h smpc.h vdp2.h font_latin.h


obj/%.o :	%.c
	$(call ECHO_CMD, "CC     ", $@, $(CC) -c $< -o $@ $(FLAGS))

obj/%.o :	%.S
	$(call ECHO_CMD, "AS     ", $@, $(AS) $< -o $@)

clean	:
		rm -f obj/*.o
		rm -f $(EXE) ssfirm.bin dump.txt


#
# end of makefile
#
