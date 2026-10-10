# =============================================================================
#  Build OTA firmware package (_crc.bin) used by HexBin.bat
#
#  Format required by the device (main/APP/TASK/src/http_update.c):
#    every block = 1024 bytes payload + 2 bytes CRC16-MODBUS (HIGH byte first)
#      section_crc = (body[1024] << 8) | body[1025] = usMBCRC16(body, 1024)
#    total file size MUST be a multiple of 1026, otherwise the device rejects it
#    (http_update_get_crc_bin_size: len % UPDATE_CHUNK_SIZE != 0 -> error)
#
#  The payload is padded with 0xFF (erased-flash value) up to a multiple of 1024.
#
#  Usage:
#    powershell -NoProfile -ExecutionPolicy Bypass -File crc_bin.ps1 <src.bin> <dst_crc.bin>
# =============================================================================

param(
    [Parameter(Mandatory = $true)][string]$Src,
    [Parameter(Mandatory = $true)][string]$Dst
)

$ErrorActionPreference = 'Stop'

$BLOCK = 1024
$POLY  = 0xA001

if (-not (Test-Path -LiteralPath $Src)) { throw "source file not found: $Src" }

$data = [System.IO.File]::ReadAllBytes($Src)
if ($data.Length -eq 0) { throw "source file is empty: $Src" }

# ---- pad payload to a multiple of BLOCK with 0xFF --------------------------
$rem = $data.Length % $BLOCK
$pad = 0
if ($rem -ne 0) { $pad = $BLOCK - $rem }

$payLen = $data.Length + $pad
$payload = New-Object byte[] $payLen
[System.Array]::Copy($data, 0, $payload, 0, $data.Length)
for ($i = $data.Length; $i -lt $payLen; $i++) { $payload[$i] = 0xFF }

# ---- CRC16-MODBUS lookup table (init 0xFFFF, reflected poly 0xA001) --------
$table = New-Object 'uint16[]' 256
for ($i = 0; $i -lt 256; $i++) {
    $c = $i
    for ($j = 0; $j -lt 8; $j++) {
        if (($c -band 1) -ne 0) { $c = (($c -shr 1) -bxor $POLY) } else { $c = ($c -shr 1) }
    }
    $table[$i] = [uint16]$c
}

# ---- build output: 1024 data + 2 CRC (high first) per block ---------------
$blocks = [int]($payLen / $BLOCK)
$out    = New-Object byte[] ($blocks * ($BLOCK + 2))
$op     = 0

for ($b = 0; $b -lt $blocks; $b++) {
    $off = $b * $BLOCK

    [System.Array]::Copy($payload, $off, $out, $op, $BLOCK)
    $op += $BLOCK

    $crc = 0xFFFF
    for ($i = 0; $i -lt $BLOCK; $i++) {
        $crc = ($crc -shr 8) -bxor $table[($crc -bxor $payload[$off + $i]) -band 0xFF]
    }

    $out[$op] = [byte](($crc -shr 8) -band 0xFF)   # CRC high byte
    $op++
    $out[$op] = [byte]($crc -band 0xFF)            # CRC low byte
    $op++
}

[System.IO.File]::WriteAllBytes($Dst, $out)

Write-Host ("[crc] {0} -> {1} bytes, {2} block(s) x 1026" -f (Split-Path -Leaf $Dst), $out.Length, $blocks)
