
cpmk
----

This is a SEGA cpk video encoder.

It calls ffmpeg to decode input video, then calls Windows' built-in iccvid.dll to encode cinepak video, and finally calls ffmpeg to output cpk format video.
Why not use ffmpeg directly? ffmpeg's encoding speed is just too slow!!!

Since iccvid.dll only has a 32-bit version, this program and ffmpeg must both be compiled in a 32-bit environment. A minimal configuration is provided in the ffmpeg directory.
Since ffmpeg's cpk muxer has a small issue, a modified version is also provided here, please replace it yourself.

Encoding control has two methods: set encoding quality through "-q", or set encoding bitrate through "-br".
If "-key" is specified, you can set the number of keyframes. If not specified, appropriate keys will be automatically selected.


cplayk
------

A simple cpk player. Drag and drop a cpk video onto it to play.
