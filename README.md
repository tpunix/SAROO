
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

English version

### SAROO is a HDLoader for SEGA Saturn.

SAROO is a Saturn optical drive emulator. SAROO is inserted into the card slot, which functions as a cdblock on the original motherboard, and the game is loaded and run from the sd card.
SAROO also provides 1MB/4MB accelerator card function.

--------
### Some pictures

<img src="doc/saroo_v12_top.jpg" width=48%/>  <img src="doc/saroo_v12_bot.jpg" width=48%/>
<img src="doc/saroo_scr1.png" width=48%/>  <img src="doc/saroo_scr2.png" width=48%/>
<img src="doc/saroo_dev1.png"/>
<img src="doc/saroo_devhw.jpg"/>


--------
### Development History

#### V1.0
The original SAROO simply added a usbhost interface to the common usbdevcart. It is necessary to crack the main program of the game and convert the operation of CDBLOCK into the operation of USB flash drive.
This approach needs to be modified for each game and is not universal. There are also big problems with performance and stability. There are only a handful of games that run this way.
(V1.0 related documents are not included in this project)


#### V1.1
The new version has a completely new design. FPGA + MCU is adopted. The FPGA (EP4CE6) is used to implement the hardware interface of CDBLOCK, and the MCU (STM32F103) runs the firmware to handle various CDBLOCK commands.
This version basically served its intended purpose, and there are games that are almost operational. But there is also a fatal problem: random data errors. There will be various mosaics when playing the opening animation,
and eventually died. This problem is difficult to debug positioning. This led to a long period of stagnation of the project.


#### V1.2
Version 1.2 is a bugfix and performance improvement from version 1.1, using a higher performance MCU: STM32H750. It is high enough frequency (400MHz) and has large enough SRAM inside to accommodate the full CDC cache.
The internal structure of the FPGA has also been reconstructed, abandoning the qsys system and using its own SDRAM and bus structure. This version lives up to expectations and is already in a state close to perfection.
At the same time, by backporting the FPGA and MCU firmware to the V1.1 hardware, V1.1 has basically reached the performance of V1.2.


--------
### Current status

The dozens of games tested worked flawlessly.  
The 1MB/4MB accelerator card function can be used normally.  
SD cards support FAT32/ExFAT file systems.  
Image files in cue/bin format are supported. Single bin or multiple bins.  
Some games get stuck on the loading/intro animation screen.  
Some games get stuck while playing.  


--------
### Hardware & Firmware

Schematics and PCBs were created using AltiumDesign 14.  
The V1.1 version requires flying leads to work properly. This version should not be used anymore.  
Version 1.2 still requires an additional pull-up resistor to use the FPGA AS configuration.  

The FPGA was developed using Quartus 14.0.  

Firm_Saturn compiled using SaturnOrbit's built-in SH-ELF compiler.  
Firm_v11 compiled with MDK4.  
Firm_V12 compiled using MDK5。  


--------
### SD card file placement

<pre>
/SAROO/ssfirm.bin   ;Saturn firmware program;
/SAROO/mcuapp.bin   ;MCU application file;
/SAROO/saroocfg.txt ;configuration file;
/SAROO/SS_SAVE.BIN  ;game saves;
/SAROO/SS_MEMS.BIN  ;memory backups;
/SAROO/ISO/         ;Stores game images. One game per directory. The directory name will be displayed in the menu;
/SAROO/BIN/         ;Stores bins. One bin per directory. The directory name will be displayed in the menu;
/SAROO/update/      ;Stores firmware upgrades;
                    ;  FPGA: ssmaster.bin
                    ;  MCU : mcuboot.bin
</pre>


--------
一Some features under developmen: [SAROO tech info](doc/SAROO技术点滴.txt)