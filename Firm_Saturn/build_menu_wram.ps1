# [MENU_WRAM] Builds ssfirm.bin as two images (Windows, jo engine's sh-elf GCC;
# the Makefile does the same steps with make):
#   resident image: runs from the cart (0x02000000): boot stub, IP header, and
#                   the code games call after they start (game start, save
#                   emulation, game patches), with its own copies of helpers;
#   menu image:     linked for low work RAM (0x002C0000), stored at the end of
#                   the resident image (menu_blob.S); the stub copies it to
#                   work RAM and runs it there.
# Steps: 1. resident link with an empty menu image; 2. objcopy the resident
# ELF down to the symbols the menu uses (res_syms.elf); 3. menu link against
# it (--just-symbols); 4. resident link again with the menu image (it is
# last, so no resident address moves; checked); 5. pad to 128 KB + font.
# Usage: build_menu_wram.ps1 [-Out ssfirm.bin] [-Asr0 0x........] [-Compiler <jo engine Compiler\WINDOWS>]
param(
    [string]$Out = 'ssfirm.bin',
    [string]$Asr0 = '',
    [string]$Compiler = 'C:\Users\charl\Desktop\joengine-2026.1\Compiler\WINDOWS'
)
$ErrorActionPreference = 'Continue'	# gcc warnings go to stderr; failures are checked with $LASTEXITCODE
$env:PATH = "$Compiler\bin;$Compiler\sh-elf\bin;$Compiler\Other Utilities;$env:PATH"
$src = $PSScriptRoot
Push-Location $src
try {
    New-Item -ItemType Directory -Force obj | Out-Null
    $flags = @('-Wall', '-m2', '-Os', '-fomit-frame-pointer', '-std=c99')
    if ($Asr0) { $flags += "-DSAROO_ASR0=$Asr0" }

    $res_only  = 'sysid', 'crt0', 'game_load', 'game_save', 'game_patch'
    $menu_only = 'main', 'conio', 'sci_shell', 'language', 'crc32', 'tiny_xm', 'version'
    $shared    = 'res_util', 'cdblock', 'string', 'printk', 'ubr_debug'
    function Invoke-Native([string]$what) {
        $out = & $args[0] $args[1..($args.Count-1)] 2>&1
        if ($LASTEXITCODE -ne 0) { $out | ForEach-Object { "$_" }; throw "$what failed" }
        $out | ForEach-Object { "$_" } | Where-Object { $_ -match 'warning' } | ForEach-Object { "  $_" }
    }

    foreach ($o in $res_only + $menu_only + $shared) {
        $s = if (Test-Path "$o.S") { "$o.S" } else { "$o.c" }
        Invoke-Native "compile $s" sh-elf-gcc -c $s -o "obj/$o.o" @flags
    }

    $robj = (($res_only + $shared) | ForEach-Object { "obj/$_.o" }) + 'obj/menu_blob.o'
    $mobj = ($menu_only + $shared) | ForEach-Object { "obj/$_.o" }
    $keep = 'bios_cd_cmd', 'my_cdplayer', 'sys_bup_init', 'use_sys_bup', 'use_sys_load'
    foreach ($isr in 'ginst', 'sinst', 'caddr', 'daddr', 'nmi', 'ubr') { foreach ($c in 'm', 's') { $keep += "${c}_${isr}_isr_entry" } }
    $alias = @{ debug_flag = 'res_debug_flag'; bios_type = 'res_bios_type'; to_stm32 = 'res_to_stm32';
                printk_putc = 'res_printk_putc'; sci_putc = 'res_sci_putc' }

    # 1. resident link, empty menu image
    [IO.File]::WriteAllBytes("$src\obj\menu.bin", [byte[]]@())
    Invoke-Native 'assemble menu_blob.S' sh-elf-gcc -c menu_blob.S -o obj/menu_blob.o @flags
    Invoke-Native 'resident link (1)' sh-elf-gcc -nostartfiles -T ldscript_res @robj -lgcc -o obj/res1.elf '-Wl,-Map=obj/res1.map'

    # 2. the resident symbols the menu needs (C names carry a leading '_')
    $oc = @('--strip-all')
    foreach ($k in $keep) { $oc += "--keep-symbol=_$k" }
    foreach ($k in $alias.Keys) { $oc += "--keep-symbol=_$k"; $oc += "--keep-symbol=_$($alias[$k])"; $oc += "--redefine-sym=_$k=_$($alias[$k])" }	# objcopy checks keep against the new name
    Invoke-Native 'objcopy res_syms' sh-elf-objcopy @oc obj/res1.elf obj/res_syms.elf

    # 3. menu link
    Invoke-Native 'menu link' sh-elf-gcc -nostartfiles -T ldscript_menu @mobj -lgcc -o obj/menu.elf '-Wl,-Map=obj/menu.map,--just-symbols=obj/res_syms.elf'
    Invoke-Native 'objcopy menu' sh-elf-objcopy -O binary obj/menu.elf obj/menu.bin

    # 4. resident link with the menu image
    Invoke-Native 'assemble menu_blob.S' sh-elf-gcc -c menu_blob.S -o obj/menu_blob.o @flags
    Invoke-Native 'resident link (2)' sh-elf-gcc -nostartfiles -T ldscript_res @robj -lgcc -o obj/res.elf '-Wl,-Map=obj/res.map'
    function Get-MapAddr($file, [string]$name) {
        $m = Get-Content $file | Select-String "^\s+0x([0-9a-f]{8})\s+$([regex]::Escape($name))(\s*=.*)?\s*$" | Select-Object -First 1
        if (-not $m) { throw "$name not in $file" }
        return [Convert]::ToInt32($m.Matches[0].Groups[1].Value, 16)
    }
    foreach ($k in $keep + $alias.Keys + '__menu_lma') {
        if ((Get-MapAddr obj/res1.map $k) -ne (Get-MapAddr obj/res.map $k)) { throw "resident symbol $k moved between links" }
    }

    # 5. binary: resident image (padded to 128 KB by the linker script) + font
    Invoke-Native 'objcopy res' sh-elf-objcopy -O binary obj/res.elf obj/res.bin
    $img  = [IO.File]::ReadAllBytes("$src\obj\res.bin")
    if ($img.Length -ne 0x20000) { throw "resident image is $($img.Length) bytes, expected 131072" }
    $menu = [IO.File]::ReadAllBytes("$src\obj\menu.bin")
    $lma  = (Get-MapAddr obj/res.map '__menu_lma') - 0x02000000
    for ($i = 0; $i -lt $menu.Length; $i++) { if ($img[$lma + $i] -ne $menu[$i]) { throw "menu image not at __menu_lma (byte $i)" } }
    $hsize = ([int]$menu[12] -shl 24) -bor ([int]$menu[13] -shl 16) -bor ([int]$menu[14] -shl 8) -bor $menu[15]
    if ($hsize -ne $menu.Length) { throw "menu header size $hsize != image $($menu.Length)" }
    $font = [IO.File]::ReadAllBytes("$src\font_cjk.bin")
    $outPath = if ([IO.Path]::IsPathRooted($Out)) { $Out } else { Join-Path $src $Out }
    [IO.File]::WriteAllBytes($outPath, [byte[]]($img + $font))

    $bssEnd = Get-MapAddr obj/menu.map '__bss_end'
    "resident image 0x02000000-0x{0:X8}, menu image stored at 0x{0:X8} ({1} bytes)" -f (0x02000000 + $lma), $menu.Length
    "menu runs at 0x002C0000-0x{0:X8} (code, data, BSS, 16 KB stack)" -f $bssEnd
    "built $outPath ($((Get-Item $outPath).Length) bytes)"
} finally { Pop-Location }
