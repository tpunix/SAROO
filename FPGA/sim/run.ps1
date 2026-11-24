param(
    [string[]]$Designs = @('orig', 'new'),
    [string[]]$Extra = @(),
    [int]$SimUs = 1000,
    [string[]]$Configs = @('saroo:8:1:0', 'saroo:8:1:2', 'sega:3:0:0', 'sega:3:0:2')
)
# Compiles the original and experimental memhub with the testbench and runs
# each config "name:NW:EWT:B_LOAD". Prints the RESULT line of every run.
$ms = 'C:\intelFPGA\20.1\modelsim_ase\win32aloem'
$env:PATH = "$ms;$env:PATH"
$sim = $PSScriptRoot
Push-Location $sim
try {
    $src = @{ orig = "$sim\orig"; new = (Split-Path $sim); v123 = "$sim\v123" }
    foreach ($d in $Designs) {
        $lib = "work_$d"
        if (-not (Test-Path $lib)) { & vlib $lib | Out-Null }
        $rtl = 'tsdram.v', 'cacheblk.v', 'cachebus.v', 'memhub.v' | ForEach-Object { Join-Path $src[$d] $_ }
        $out = & vlog -work $lib -timescale 1ns/1ps @rtl 2>&1
        if ($LASTEXITCODE -ne 0) { $out; throw "vlog RTL failed ($d)" }
        $defs = @(); if ($d -eq 'new') { $defs = @('+define+HAS_REF_DEBT') }
        $out = & vlog -work $lib -sv @defs "$sim\sdram_model.sv" "$sim\tb_memhub.sv" 2>&1
        if ($LASTEXITCODE -ne 0) { $out; throw "vlog TB failed ($d)" }
    }
    foreach ($c in $Configs) {
        $p = $c.Split(':'); $n, $nw, $ewt, $bl = $p[0..3]; $idle = if ($p.Count -gt 4) { $p[4] } else { '0' }
        foreach ($d in $Designs) {
            $log = & vsim -c -quiet -lib "work_$d" tb "-GNW=$nw" "-GEWT=$ewt" "-GB_LOAD=$bl" "-GSAT_IDLE_PCT=$idle" "-GSIM_US=$SimUs" "-GNAME=$d-$n" @Extra -do 'run -all; quit -f' 2>&1
            $res = $log | Select-String 'RESULT|CAUSES|STM32 |ERROR|Fatal|Error:' | Select-Object -First 14
            if ($res) { $res | ForEach-Object { $_.Line -replace '^# ', '' } } else { $log | Select-Object -Last 15 }
        }
    }
} finally { Pop-Location }
