! Sega Saturn USB flash cart ROM
! by Anders Montonen, 2012
! Creative Commons Attribution-ShareAlike 3.0 Unported (CC BY-SA 3.0)

! System ID block

.section .sysid

hwid:       .ascii "SEGA SEGASATURN "
makerid:    .ascii "ANDERS MONTONEN "
produm:     .ascii "ANTIME-001"
version:    .ascii "V1.003"
reldat:     .ascii "20120610"
devinfo:    .ascii "R  2    "
area:       .ascii "JTUE      "
            .ascii "      "
periph:     .ascii "J               "
title:      .ascii "                "
            .ascii "                "
            .ascii "                "
            .ascii "                "
            .ascii "                "
            .ascii "                "
            .ascii "                "
            .ascii "                "
ipsize:     .long  __ip_len
stackm:     .long 0
            .long 0
stacks:     .long 0
firstaddr:  .long 0
firstsize:  .long 0
            .long 0
            .long 0

            .incbin "sega/sys_sec.bin"
            .incbin "sega/sys_arej.bin"
            .incbin "sega/sys_aret.bin"
            .incbin "sega/sys_areu.bin"
            .incbin "sega/sys_aree.bin"
            .incbin "sega/sys_init.bin"
