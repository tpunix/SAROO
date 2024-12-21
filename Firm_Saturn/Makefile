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

all	: obj BUILD_VER $(EXE)

obj:
	mkdir obj

BUILD_VER:
	touch version.c

$(EXE)	: $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) $(LIBS) -o $(EXE)
	$(OBJDUMP) -xd $(EXE) > dump.txt
	$(OBJCOPY) -O binary $(EXE) tmp.bin
	cat tmp.bin font_cjk.bin >ssfirm.bin
	rm -f tmp.bin

$(OBJ): main.h smpc.h vdp2.h font_latin.h


obj/%.o :	%.c
		$(CC) -c $< -o $@ $(FLAGS)

obj/%.o :	%.S
		$(AS) $< -o $@

clean	:
		rm -f obj/*.o
		rm -f $(EXE) ssfirm.bin dump.txt

makedir :
		mkdir obj

#
# end of makefile
#
