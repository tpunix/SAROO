
cpmk
----

这是一个SEGA cpk视频编码器。

它调用ffpmeg解码输入视频，然后调用windows自带的iccvid.dll编码cinepak视频，最后调用ffmpeg输出cpk格式的视频。
为什么不直接用ffmpeg呢? ffmpeg的编码速度实在是太慢了!!!

因为iccvid.dll只有32位版本，所以本程序与ffmpeg都必须在32位环境下编译。ffmpeg目录下提供了一个最简配置。
由于ffmpeg的cpk muxer有点小问题，这里还提供了一个修改过的版本，请自行替换。

编码控制有两种方式，通过"-q"设置编码质量，或者"-br"设置编码码率。
如果指定"-key"，可以设置关键帧的数量。如不指定，会自动选择合适的key。


cplayk
------

一个简单的cpk播放器。将cpk视频拖放上去即可播放。
