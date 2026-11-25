param([string]$Dir)
# Moves declarations above their first use so ModelSim accepts the files.
# Pure reordering: no logic change. Quartus accepts the original order.
function Move-Decls([string]$file, [string[]]$decls, [string]$anchor) {
    $lines = [Collections.Generic.List[string]](Get-Content $file -Encoding UTF8)
    $moved = @()
    foreach ($d in $decls) {
        $i = -1
        for ($k = 0; $k -lt $lines.Count; $k++) { if ($lines[$k].Trim() -ceq $d) { $i = $k; break } }
        if ($i -lt 0) { throw "$file : declaration not found: $d" }
        $moved += $lines[$i]; $lines.RemoveAt($i)
    }
    $a = -1
    for ($k = 0; $k -lt $lines.Count; $k++) { if ($lines[$k].Trim() -ceq $anchor) { $a = $k; break } }
    if ($a -lt 0) { throw "$file : anchor not found: $anchor" }
    $block = @('', "`t// [SIM] declarations moved up for ModelSim (order only)") + $moved
    $lines.InsertRange($a + 1, [string[]]$block)
    [IO.File]::WriteAllLines($file, $lines, (New-Object Text.UTF8Encoding($false)))
    "$file : moved $($moved.Count) declarations"
}

Move-Decls "$Dir\tsdram.v" @('reg ref_ack;') 'reg ref_req;'

Move-Decls "$Dir\memhub.v" @(
    'wire[25:3] cache_addr_a;', 'wire[63:0] cache_data1d_a;', 'wire[ 3:0] cache_valid_a;',
    'wire cache_update_b;',
    'wire[25:3] cache_addr_b;', 'wire[63:0] cache_data1d_b;', 'wire[ 3:0] cache_valid_b;',
    'reg[1:0] state;', 'reg[ 1:0] cmd_req;', 'reg[ 1:0] cmd_mask;', 'reg[25:0] cmd_addr;',
    'reg[15:0] cmd_din;', 'reg act_ab;', 'reg data_ab;', 'reg cmd_ack_a;', 'reg cmd_ack_b;',
    'wire cmd_ack;', 'wire data_valid;', 'wire[15:0] cmd_dout;', 'wire[15:0] sd_dout;', 'wire sd_oe;'
) 'input [11:0] reg_sdram;'
