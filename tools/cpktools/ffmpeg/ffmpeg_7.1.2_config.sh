#!/bin/sh

./configure \
	--enable-static \
	--disable-shared \
	--disable-w32threads \
	--disable-iconv \
	--disable-zlib \
	--disable-doc \
	--disable-parsers \
	--disable-encoders \
	--disable-avdevice \
	--disable-network \
	--disable-hwaccels \
	--disable-bsfs \
	--disable-protocols \
	--enable-protocol=file \
	--disable-muxers \
	--enable-muxer=segafilm \
	--disable-demuxers \
	--enable-demuxer=mov,avi,matroska \
	--disable-decoders \
	--enable-decoder=aac,ac3,ape,flac,h261,h263,h264,hevc,mp3,mpeg1video,mpeg2video,mpeg4,mpegvideo \
	--disable-filters \
	--enable-filter=aformat,aresample,asetnsamples,ashowinfo,crop,format,fps,scale,hflip,vflip

#	--disable-x86asm \
#	--enable-debug=gdb \

