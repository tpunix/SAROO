
FFMPEG_DIR = /usr/local

CFLAGS = -Wall -g -I$(FFMPEG_DIR)/include --static -s
FFLIBS = -L$(FFMPEG_DIR)/lib -Wl,--start-group -lavutil -lavformat -lavcodec -lswscale -lswresample -lbcrypt -lole32 -lstrmiids -lavfilter -llzma -lbz2 -Wl,--end-group


all:
	gcc $(CFLAGS) -o cpmk cpmk.c -lvfw32 $(FFLIBS)

