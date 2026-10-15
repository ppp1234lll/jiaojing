# 抓取 192.168.1.72(正常设备) 的完整应答, 与问题设备对比
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
while ((Get-Date) -lt $deadline) {
    try {
        $n = $rx.ReceiveFrom($rxbuf, [ref]$ep)
        $src = $ep.Address.ToString()
        $txt = [Text.Encoding]::ASCII.GetString($rxbuf, 0, $n)
        if ($src -eq "192.168.1.72") {
            Write-Host ("`n===== 192.168.1.72 完整应答 ({0} 字节) =====" -f $n)
            Write-Host $txt
            try { $null = $txt | ConvertFrom-Json; Write-Host ">> JSON 校验: 通过" }
            catch { Write-Host (">> JSON 校验: 失败 - {0}" -f $_.Exception.Message) }
            break
        }
    } catch [System.Net.Sockets.SocketException] { }
}
$rx.Close(); $tx.Close()
