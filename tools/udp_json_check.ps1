# 校验问题设备应答的 JSON 合法性(按原始字节)
$group = [System.Net.IPAddress]::Parse("239.255.255.249")
$port  = 65000
$rx = New-Object System.Net.Sockets.Socket([System.Net.Sockets.AddressFamily]::InterNetwork,
        [System.Net.Sockets.SocketType]::Dgram, [System.Net.Sockets.ProtocolType]::Udp)
$rx.SetSocketOption([System.Net.Sockets.SocketOptionLevel]::Socket,
        [System.Net.Sockets.SocketOptionName]::ReuseAddress, $true)
$rx.Bind((New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any, $port)))
$mopt = New-Object System.Net.Sockets.MulticastOption($group, [System.Net.IPAddress]::Any)
$rx.SetSocketOption([System.Net.Sockets.SocketOptionLevel]::IP,
        [System.Net.Sockets.SocketOptionName]::AddMembership, $mopt)
$rx.ReceiveTimeout = 8000
$tx = New-Object System.Net.Sockets.UdpClient
$tx.Ttl = 32
$probe = [Text.Encoding]::ASCII.GetBytes('{"info":{"tid":"0","types":"search"}}')
[void]$tx.Send($probe, $probe.Length, $group, $port)

$rxbuf = New-Object byte[] 2048
$ep = New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any, 0)
$deadline = (Get-Date).AddSeconds(7)
$done100 = $false
while ((Get-Date) -lt $deadline -and -not $done100) {
    try {
        $n = $rx.ReceiveFrom($rxbuf, [ref]$ep)
        $src = $ep.Address.ToString()
        if ($src -eq "192.168.1.100") {
            $raw = $rxbuf[0..($n-1)]
            Write-Host ("`n192.168.1.100 应答 {0} 字节" -f $n)
            Write-Host ("末尾16字节: " + (($raw[-16..-1] | ForEach-Object { '{0:x2}' -f $_ }) -join ' '))
            $txt = [Text.Encoding]::UTF8.GetString($raw)
            try { $null = $txt | ConvertFrom-Json; Write-Host ">> UTF8+ConvertFrom-Json: 通过" }
            catch { Write-Host (">> UTF8+ConvertFrom-Json: 失败 - {0}" -f $_.Exception.Message) }
            try { $null = [Text.Encoding]::GetEncoding("GBK").GetString($raw) | ConvertFrom-Json; Write-Host ">> GBK+ConvertFrom-Json: 通过" }
            catch { Write-Host (">> GBK+ConvertFrom-Json: 失败 - {0}" -f $_.Exception.Message) }
            $done100 = $true
        }
    } catch [System.Net.Sockets.SocketException] { }
}
$rx.Close(); $tx.Close()
