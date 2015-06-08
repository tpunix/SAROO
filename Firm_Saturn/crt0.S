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
	! Disable interrupts
	mov     #0xf,r0
	shll2   r0
	shll2   r0
	ldc     r0,sr

	! Clear BSS
	mov.l   bss_start,r0
	mov.l   bss_end,r1
	mov     #0,r2
lbss:
	cmp/ge  r0,r1
	bt      lbss_end
	add     #1,r0
	mov.b   r2,@r0
	bra     lbss
lbss_end:

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

main_ptr:   .long __main
stack_ptr:  .long __stack_end
bss_start:  .long __bss_start
bss_end:    .long __bss_end



!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

.global _ubr_isr_entry
.align 4
_ubr_isr_entry:
	mov.l   r0,  @-r15
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
	mov.l   ubr_handle_ptr, r0
	jsr		@r0
	nop

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

brcr_addr:
		.long 0xffffff78



.global _vbi_isr_entry
.align	4
_vbi_isr_entry:
	mov.l   r0,  @-r15
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
    mov.l   vbi_handle_ptr, r0
    jsr		@r0
    nop

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

old_vbi_addr:
			.long 0x06000840
vbi_handle_ptr:
			.long _vbi_handle
ubr_handle_ptr:
			.long _ubr_handle

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

