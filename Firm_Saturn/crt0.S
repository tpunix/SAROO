!
! Bart's Custom Sega Saturn Start-Up Code
! Bart Trzynadlowski, 2001
! Public domain
!
! For use with the GNU C Compiler. This code has been tested with gcc version
! cygnus-2.7-96q3.
!
! Make sure this is the first file linked into your project, so that it is at
! the very beginning. Use the BART.LNK linker script. Load the resulting
! binary at 0x6004000 on the Sega Saturn and begin execution there.
!

#include "abus.h"

.section .text

!
! Entry point
!
.global start
.align	4
start:
	bra		_next_start
	nop

_func_table:
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000

_next_start:
	! Disable interrupts
	mov     #0xf,r0
	shll2   r0
	shll2   r0
	ldc     r0,sr

	! [MENU_WRAM] The menu runs from Saturn work RAM, not from the cart: this
	! stub copies it there. The copy runs at SAROO's own cart timing (external
	! WAIT on: the cart can hold the CPU, so every read is right whatever the
	! STM32 is doing); Sega's timing is set after it, just before the menu
	! starts. Only the resident game-side code (game start, save hooks) still
	! runs from the cart.
	mov.l   asr0_ptr, r0
	mov.l	asr0_safe, r1
	mov.l	r1, @r0
	mov		#0x13, r1
	mov.l	r1, @(8,r0)

	! Menu image header at __menu_lma (cart), see ldscript_menu:
	!   +0 'MENU'  +4 entry  +8 run address  +12 size (bytes, x4)
	!   +16 bss start  +20 bss end (= stack top)
_copy_menu:
	mov.l	menu_lma, r4		! r4 = source (cart)
	mov.l	@(8,r4), r5			! r5 = destination (work RAM)
	mov.l	@(12,r4), r6		! r6 = size
	mov		r4, r1
	mov		r5, r2
	mov		r6, r3
1:
	mov.l	@r1+, r0
	mov.l	r0, @r2
	add		#4, r2
	add		#-4, r3
	cmp/pl	r3
	bt		1b

	! check: read the cart again and compare every word with the copy
	mov		r4, r1
	mov		r5, r2
	mov		r6, r3
2:
	mov.l	@r1+, r0
	mov.l	@r2+, r7
	cmp/eq	r0, r7
	bf		_copy_menu			! different: copy again
	add		#-4, r3
	cmp/pl	r3
	bt		2b

	! clear the menu's BSS (work RAM)
	mov.l	@(16,r4), r0
	mov.l	@(20,r4), r1
	mov		#0, r2
3:
	cmp/hs	r1, r0
	bt		4f
	mov.b	r2, @r0
	bra		3b
	add		#1, r0
4:

!	! Relocate self
!	mova	_real, r0
!	mov.l	start_ptr, r1
!_real:
!	shlr8	r0
!	shll8	r0
!	cmp/eq	r1, r0
!	bt		_clear_bss

!	mov.l	end_ptr, r2
!1:
!	mov.l	@r0, r3
!	mov.l	r3, @r1
!	add		#4, r0
!	add		#4, r1
!	cmp/hs	r2, r1
!	bf		1b

_clear_bss:
	! Clear the resident image's BSS (cart)
	mov.l   bss_start,r0
	mov.l   bss_end,r1
	mov     #0,r2
1:
	cmp/hs  r1,r0
	bt		2f
	mov.b   r2,@r0
	bra		1b
	add     #1,r0
2:

	! [MENU_WRAM] purge the SH-2 cache (keeps it on or off as it was), so no
	! line from before the copy is used for the menu's code
	mov.l	ccr_ptr, r1
	mov.b	@r1, r0
	or		#0x10, r0
	mov.b	r0, @r1

	! Sega's cart timing from here on
	mov.l   asr0_ptr, r0
	mov.l	asr0_val, r1
	mov.l	r1, @r0

	! stack at the top of the menu's BSS (work RAM); jump to the menu's
	! _main() in work RAM
	mov.l	menu_lma, r4
	mov.l	@(20,r4), r15
	mov.l	@(4,r4), r0
	jsr		@r0
	nop

    ! Once _main() has terminated, disable interrupts and loop infinitely
    mov     #0xf,r0
    shll2   r0
    shll2   r0
    ldc     r0,sr
end:
    bra     end
    nop

.align	4

start_ptr:  .long start
end_ptr:    .long __ip_end
menu_lma:   .long __menu_lma
bss_start:  .long __bss_start
bss_end:    .long __bss_end
asr0_ptr:   .long 0x25fe00b0
asr0_val:   .long SAROO_ASR0
asr0_safe:  .long 0x38803880	! SAROO timing, external WAIT on (for the copy)
ccr_ptr:    .long 0xfffffe92	! SH-2 cache control register


!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

.macro define_isr_entry  isr_name isr_code
.global \isr_name
.align 2
\isr_name :
	mov.l	r0, @-r15
	bra		common_exc_handle
	mov		# \isr_code, r0
.endm


define_isr_entry  _m_ginst_isr_entry, 0x04
define_isr_entry  _s_ginst_isr_entry, 0x84
define_isr_entry  _m_sinst_isr_entry, 0x06
define_isr_entry  _s_sinst_isr_entry, 0x86
define_isr_entry  _m_caddr_isr_entry, 0x09
define_isr_entry  _s_caddr_isr_entry, 0x89
define_isr_entry  _m_daddr_isr_entry, 0x0a
define_isr_entry  _s_daddr_isr_entry, 0x8a
define_isr_entry  _m_nmi_isr_entry,   0x0b
define_isr_entry  _s_nmi_isr_entry,   0x8b
define_isr_entry  _m_ubr_isr_entry,   0x0c
define_isr_entry  _s_ubr_isr_entry,   0x8c


common_exc_handle:
	mov.l   r1,  @-r15
	mov.l   r2,  @-r15
	mov.l   r3,  @-r15
	mov.l   r4,  @-r15
	mov.l   r5,  @-r15
	mov.l   r6,  @-r15
	mov.l   r7,  @-r15
	mov.l   r8,  @-r15
	mov.l   r9,  @-r15
	mov.l   r10, @-r15
	mov.l   r11, @-r15
	mov.l   r12, @-r15
	mov.l   r13, @-r15
	mov.l   r14, @-r15
	mov		r15, r1
	add		#0x48, r1
	mov.l   r1,  @-r15
	sts.l	macl,@-r15
	sts.l	mach,@-r15
	stc.l	vbr, @-r15
	stc.l	gbr, @-r15
	sts.l	pr,  @-r15

	mov		r15, r4
	mov.l   exc_handle_ptr, r1
	jsr		@r1
	mov		r0, r5

	lds.l	@r15+, pr
	ldc.l	@r15+, gbr
	ldc.l	@r15+, vbr
	lds.l	@r15+, mach
	lds.l	@r15+, macl
	mov.l	@r15+, r1
	mov.l	@r15+, r14
	mov.l	@r15+, r13
	mov.l	@r15+, r12
	mov.l	@r15+, r11
	mov.l	@r15+, r10
	mov.l	@r15+, r9
	mov.l	@r15+, r8
	mov.l	@r15+, r7
	mov.l	@r15+, r6
	mov.l	@r15+, r5
	mov.l	@r15+, r4
	mov.l	@r15+, r3
	mov.l	@r15+, r2
	mov.l	@r15+, r1
	mov.l	@r15+, r0
	rte
	nop
.align	4

exc_handle_ptr:
			.long _exc_handle

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

