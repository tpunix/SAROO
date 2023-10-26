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
	.long	0x00000004
	.long	0x00000008
	.long	0x0000000c
	.long	0x00000010
	.long	0x00000014
	.long	0x00000018
	.long	0x0000001c
	.long	0x00000020
	.long	0x00000024
	.long	0x00000028
	.long	0x0000002c
	.long	0x00000030
	.long	0x00000034
	.long	0x00000038
	.long	0x0000003c

_next_start:
	! Disable interrupts
	mov     #0xf,r0
	shll2   r0
	shll2   r0
	ldc     r0,sr

	! set ASR0
	mov.l   asr0_ptr, r0
	mov.l	asr0_val, r1
	mov.l	r1, @r0
	mov		#0x13, r1
	mov.l	r1, @(8,r0)

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
	! Clear BSS
	mov.l   bss_start,r0
	mov.l   bss_end,r1
	mov     #0,r2
1:
	mov.b   r2,@r0
	add     #1,r0
	cmp/hs  r1,r0
	bf		1b

    ! Set initial stack pointer. Stack is from 0x6002000-0x6003FFF
    mov.l   stack_ptr,r15
    ! Jump to _main()
    mov.l   main_ptr,r0
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
main_ptr:   .long __main
stack_ptr:  .long __stack_end
bss_start:  .long __bss_start
bss_end:    .long __bss_end
asr0_ptr:   .long 0x25fe00b0
asr0_val:   .long 0x38803880


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

