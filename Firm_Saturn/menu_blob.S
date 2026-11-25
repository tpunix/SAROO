! [MENU_WRAM] The menu image (obj/menu.bin, linked for low work RAM) stored in
! the resident image at __menu_lma, after everything else, so adding it moves
! no resident address. crt0 copies it to work RAM at start-up.
! The first resident link uses an empty obj/menu.bin.

.section .menu_blob, "a"
.incbin "obj/menu.bin"
