#
# Sega Saturn USB flash cart ROM
# by Anders Montonen, 2012
# Creative Commons Attribution-ShareAlike 3.0 Unported (CC BY-SA 3.0)
#

CC	=	sh-elf-gcc
AS	=	sh-elf-as
LDFLAGS	=	-nostartfiles -nostdlib -T ldscript
FLAGS	=	-Wall -m2 -Os -fomit-frame-pointer -std=c99

LIBS	=	-lgcc

EXE	=	ramimage.bin

OBJ	=	obj/crt0.o  \
		obj/main.o  \
		obj/conio.o \
		obj/smpc.o  \
		obj/cdblock.o  \
		obj/crc32.o  \
		obj/string.o \
		obj/printk.o \
		obj/tiny_xm.o\
		obj/vdp.o

all	:	$(EXE)

$(EXE)	:	$(OBJ)
		$(CC) $(LDFLAGS) $(OBJ) $(LIBS) -o $(EXE)

obj/%.o :	%.c
		$(CC) -c $< -o $@ $(FLAGS)

obj/%.o :	%.S
		$(AS) $< -o $@

clean	:
		rm -f obj/*.o
		rm -f $(EXE)

makedir :
		mkdir obj

#
# end of makefile
#
