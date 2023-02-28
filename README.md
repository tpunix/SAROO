
### SAROO is a HDLoader for SEGA Saturn.

SAROO是一个土星光驱模拟器。SAROO插在卡槽上，实现原主板的CDBLOCK的功能，从SD卡装载游戏并运行。
SAROO同时还提供1MB/4MB加速卡功能。

--------
### 一些图片

<img src="doc/saroo_v12_top.jpg" width=48%/>  <img src="doc/saroo_v12_bot.jpg" width=48%/>
<img src="doc/saroo_scr1.png" width=48%/>  <img src="doc/saroo_scr2.png" width=48%/>
<img src="doc/saroo_dev1.png"/>
<img src="doc/saroo_devhw.jpg"/>


--------
### 开发历史

#### V1.0
最初的SAROO仅仅是在常见的usbdevcart上增加了一个usbhost接口。需要对游戏主程序进行破解，将对CDBLOCK的操作转化为对U盘的操作。
这种方式需要针对每一个游戏做修改，不具备通用性。性能与稳定性也有很大问题。只有很少的几个游戏通过这种方式跑起来了。
(V1.0相关的文件未包括在本项目中)


#### V1.1
新版本做了全新的设计。采用FPGA+MCU的方式。FPGA(EP4CE6)用来实现CDBLOCK的硬件接口，MCU(STM32F103)运行固件来处理各种CDBLOCK命令。
这个版本基本达到了预期的目的，也有游戏几乎能运行了。但也有一个致命的问题: 随机的数据错误。在播放片头动画时会出现各种马赛克，
并最终死掉。这个问题很难调试定位。这导致了本项目停滞了很长时间。


#### V1.2
1.2版本是1.1版本的bugfix与性能提升，使用了更高性能的MCU:STM32H750。它频率足够高(400MHz)，内部有足够大的SRAM，可以容纳完整的CDC缓存。
FPGA内部也经过重构，抛弃了qsys系统，使用自己实现的SDRAM与总线结构。这个版本不负众望，已经是接近完美的状态了。
同时，通过把FPGA与MCU固件逆移植到V1.1硬件之上，V1.1也基本达到了V1.2的性能了。


--------
### 当前状态

测试的几十个游戏可以完美运行。  
1MB/4MB加速卡功能可以正常使用。  
SD卡支持FAT32/ExFAT文件系统。  
支持cue/bin格式的镜像文件。单bin或多bin。  
部分游戏会卡在加载/片头动画界面。  
部分游戏会卡在进行游戏时。  


--------
### 硬件与固件

原理图与PCB使用AltiumDesign14制作。  
V1.1版本需要飞线才能正常工作。不应该再使用这个版本了。  
V1.2版本仍然需要额外的一个上拉电阻以使用FPGA的AS配置方式。  

FPGA使用Quartus14.0开发。  

Firm_Saturn使用SaturnOrbit自带的SH-ELF编译器编译。  
Firm_v11使用MDK4编译。  
Firm_V12使用MDK5编译。  


--------
### SD卡文件放置

<pre>
/ramimage.bin      ;Saturn的固件程序.
/SAROO/ISO/        ;存放游戏镜像. 每个目录放一个游戏. 目录名将显示在菜单中.
/SAROO/update/     ;存放用于升级的固件.
                   ;  FPGA: SSMaster.rbf
                   ;  MCU : ssmaster.bin
</pre>


--------
一些开发中的记录: [SAROO技术点滴](doc/SAROO技术点滴.txt)



