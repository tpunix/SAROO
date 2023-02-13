
CALL F:\SaturnOrbit\SET_ELF.BAT
make -f Makefile clean
make -f Makefile
copy ramimage.bin F:\hgfs\tftpboot
pause
