# UDP 搜索端到端验证脚本
$ErrorActionPreference = "Stop"

$group = [System.Net.IPAddress]::Parse("239.255.255.249")
$port  = 65000

# 接收 socket: 先 ReuseAddress 再绑定 65000 并加组(设备应答发往组播地址:65000)
$rx = New-Object System.Net.Sockets.Socket([System.Net.Sockets.AddressFamily]::InterNetwork,
        [System.Net.Sockets.SocketType]::Dgram, [System.Net.Sockets.ProtocolType]::Udp)
$rx.SetSocketOption([System.Net.Sockets.SocketOptionLevel]::Socket,
        [System.Net.Sockets.SocketOptionName]::ReuseAddress, $true)
$rx.Bind((New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any, $port)))
$mopt = New-Object System.Net.Sockets.MulticastOption($group, [System.Net.IPAddress]::Any)
$rx.SetSocketOption([System.Net.Sockets.SocketOptionLevel]::IP,
        [System.Net.Sockets.SocketOptionName]::AddMembership, $mopt)
$rx.ReceiveTimeout = 6000

# 发送探测包
$tx = New-Object System.Net.Sockets.UdpClient
$tx.Ttl = 32
$probe = [Text.Encoding]::ASCII.GetBytes('{"info":{"tid":"0","types":"search"}}')
[void]$tx.Send($probe, $probe.Length, $group, $port)
Write-Host ("已发送探测包 {0} 字节: {1}" -f $probe.Length, [Text.Encoding]::ASCII.GetString($probe))

# 收应答
$rxbuf = New-Object byte[] 1500
$ep = New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any, 0)
$found = @{}
for ($i = 0; $i -lt 30; $i++) {
    try {
        $n = $rx.ReceiveFrom($rxbuf, [ref]$ep)
        $data = $rxbuf[0..($n-1)]
        $src = $ep.Address.ToString()
        if (-not $found.ContainsKey($src)) {
            $found[$src] = 1
            $txt = [Text.Encoding]::ASCII.GetString($data)
            Write-Host ("[应答] 来自 {0}:{1}  {2}字节" -f $src, $ep.Port, $data.Length)
            Write-Host ("       {0}" -f $txt.Substring(0, [Math]::Min(200, $txt.Length)))
        }
    } catch [System.Net.Sockets.SocketException] {
        break
    }
}
if ($found.Count -eq 0) { Write-Host "6 秒内未收到任何应答" }
$rx.Close(); $tx.Close()
