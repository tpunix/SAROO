#!/bin/sh

./bdfont cbdf/helvR14c.bdf -l latin.txt -h font_latin.h font_latin
./bdfont bdf/wqy13px.bdf -l cjk.txt -adj 1 -c font_cjk.bin
