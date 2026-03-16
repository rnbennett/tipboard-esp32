$port = [System.IO.Ports.SerialPort]::new('COM8', 115200)
$port.DtrEnable = $false
$port.RtsEnable = $false
$port.Open()
Start-Sleep -Milliseconds 100
$port.RtsEnable = $true
Start-Sleep -Milliseconds 100
$port.RtsEnable = $false
$buf = ''
$sw = [System.Diagnostics.Stopwatch]::StartNew()
while($sw.ElapsedMilliseconds -lt 25000) {
    if($port.BytesToRead -gt 0) { $buf += $port.ReadExisting() }
    else { Start-Sleep -Milliseconds 50 }
}
$port.Close()
Write-Output $buf
