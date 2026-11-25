#
# Sega Saturn USB flash cart ROM
# by Anders Montonen, 2012
# Creative Commons Attribution-ShareAlike 3.0 Unported (CC BY-SA 3.0)
#
# [MENU_WRAM] ssfirm.bin is built from two images:
#   resident image (ldscript_res): runs from the cart at 0x02000000: boot stub,
#     IP header, and the code games call after they start (game start, save
#     emulation, game patches), with its own copies of the helpers it needs;
#   menu image (ldscript_menu): linked for low work RAM (0x002C0000), stored at
#     the end of the resident image (menu_blob.S); crt0 copies it to work RAM
#     and runs it there.
# Steps: resident link with an empty menu image; objcopy the resident ELF down
# to the symbols the menu uses; menu link against them (--just-symbols);
# resident link again with the menu image (it is last: no resident address
# moves; checked with cmp); binary padded to 128 KB, then font_cjk.bin.
# build_menu_wram.ps1 does the same on Windows.
#

CC	=	sh-elf-gcc
AS	=	sh-elf-as
OBJDUMP = sh-elf-objdump
OBJCOPY = sh-elf-objcopy
FLAGS	=	-Wall -m2 -Os -fomit-frame-pointer -std=c99

LIBS	=	-lgcc

EXE	=	ssfirm.elf

RES_OBJ  =	obj/sysid.o \
			obj/crt0.o  \
			obj/game_load.o  \
			obj/game_save.o  \
			obj/game_patch.o

MENU_OBJ =	obj/main.o  \
			obj/conio.o \
			obj/sci_shell.o  \
			obj/language.o  \
			obj/crc32.o  \
			obj/tiny_xm.o \
			obj/version.o

SHARED_OBJ = obj/res_util.o \
			obj/cdblock.o  \
			obj/string.o \
			obj/printk.o \
			obj/ubr_debug.o

OBJ	=	$(RES_OBJ) $(MENU_OBJ) $(SHARED_OBJ)

# resident symbols the menu image uses (C names carry a leading '_'); the
# res_* names are the resident copies of variables the menu also has
ISR_KEEP = $(foreach i,ginst sinst caddr daddr nmi ubr,$(foreach c,m s,--keep-symbol=_$(c)_$(i)_isr_entry))
RES_SYMS = --strip-all \
	--keep-symbol=_bios_cd_cmd --keep-symbol=_my_cdplayer --keep-symbol=_sys_bup_init \
	--keep-symbol=_use_sys_bup --keep-symbol=_use_sys_load $(ISR_KEEP) \
	--keep-symbol=_debug_flag  --keep-symbol=_res_debug_flag  --redefine-sym=_debug_flag=_res_debug_flag \
	--keep-symbol=_bios_type   --keep-symbol=_res_bios_type   --redefine-sym=_bios_type=_res_bios_type \
	--keep-symbol=_to_stm32    --keep-symbol=_res_to_stm32    --redefine-sym=_to_stm32=_res_to_stm32 \
	--keep-symbol=_printk_putc --keep-symbol=_res_printk_putc --redefine-sym=_printk_putc=_res_printk_putc \
	--keep-symbol=_sci_putc    --keep-symbol=_res_sci_putc    --redefine-sym=_sci_putc=_res_sci_putc

define ECHO_CMD
	$(if $(V), , @echo "    "$(1)" " $(2))
	$(if $(V), $(3), @$(3))
endef

all	: obj BUILD_VER $(EXE)

obj:
	$(call ECHO_CMD, "MKDIR  ", obj,      mkdir obj)

BUILD_VER:
	$(call ECHO_CMD, "TOUCH  ", version.c, touch version.c)

$(EXE)	: $(OBJ) ldscript_res ldscript_menu menu_blob.S
	$(call ECHO_CMD, "LD     ", obj/res1.elf,  : > obj/menu.bin)
	@$(CC) -c menu_blob.S -o obj/menu_blob.o $(FLAGS)
	@$(CC) -nostartfiles -T ldscript_res $(RES_OBJ) $(SHARED_OBJ) obj/menu_blob.o $(LIBS) -o obj/res1.elf
	$(call ECHO_CMD, "OBJCOPY", obj/res_syms.elf, $(OBJCOPY) $(RES_SYMS) obj/res1.elf obj/res_syms.elf)
	$(call ECHO_CMD, "LD     ", obj/menu.elf, $(CC) -nostartfiles -T ldscript_menu $(MENU_OBJ) $(SHARED_OBJ) $(LIBS) -Xlinker --just-symbols=obj/res_syms.elf -o obj/menu.elf)
	$(call ECHO_CMD, "OBJCOPY", obj/menu.bin, $(OBJCOPY) -O binary obj/menu.elf obj/menu.bin)
	$(call ECHO_CMD, "LD     ", $@,       $(CC) -c menu_blob.S -o obj/menu_blob.o $(FLAGS))
	@$(CC) -nostartfiles -T ldscript_res $(RES_OBJ) $(SHARED_OBJ) obj/menu_blob.o $(LIBS) -o $(EXE)
	@$(OBJCOPY) $(RES_SYMS) $(EXE) obj/res_syms2.elf
	@$(OBJDUMP) -t obj/res_syms.elf  | grep ' g ' > obj/res_syms.txt
	@$(OBJDUMP) -t obj/res_syms2.elf | grep ' g ' > obj/res_syms2.txt
	@cmp -s obj/res_syms.txt obj/res_syms2.txt || (echo "resident symbols moved between links"; exit 1)
	$(call ECHO_CMD, "OBJDUMP", dump.txt, $(OBJDUMP) -xd $(EXE) > dump.txt)
	$(call ECHO_CMD, "OBJCOPY", tmp.bin,  $(OBJCOPY) -O binary $(EXE) tmp.bin)
	$(call ECHO_CMD, "CAT    ", ssfirm.bin, cat tmp.bin font_cjk.bin >ssfirm.bin)
	$(call ECHO_CMD, "RM     ", tmp.bin,  rm -f tmp.bin)

$(OBJ): main.h smpc.h vdp2.h font_latin.h


obj/%.o :	%.c
	$(call ECHO_CMD, "CC     ", $@, $(CC) -c $< -o $@ $(FLAGS))

obj/%.o :	%.S
	$(call ECHO_CMD, "AS     ", $@, $(CC) -c $< -o $@ $(FLAGS))

clean	:
		rm -f obj/*.o obj/*.elf obj/*.txt obj/menu.bin
		rm -f $(EXE) ssfirm.bin dump.txt


#
# end of makefile
#
