# Launch the private test PCSX2 minimized on a savestate. Usage: launch_test.ps1 <slot>
# Paths come from local/paths.json or DDS_* environment variables (see localpaths.py); python must be on PATH.
param([int]$Slot = 3)
$tools = $PSScriptRoot
function LocalPath([string]$key) { (& python (Join-Path $tools 'localpaths.py') $key).Trim() }
$pcsx2 = (LocalPath "test_pcsx2").Replace("/", "\")
$iso = (LocalPath "iso").Replace("/", "\")
Get-CimInstance Win32_Process -Filter "Name='pcsx2-qt.exe'" | Where-Object { $_.ExecutablePath -like "$pcsx2\*" } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }
$state = Join-Path $pcsx2 ('sstates\SLUS-20974 (D7273511).{0:D2}.p2s' -f $Slot)
Start-Process -WindowStyle Minimized -FilePath (Join-Path $pcsx2 'pcsx2-qt.exe') -ArgumentList '-batch','-statefile',"`"$state`"",'--',"`"$iso`""
# wait until PINE answers (up to 60 s)
$deadline = (Get-Date).AddSeconds(60)
while ((Get-Date) -lt $deadline) {
    try { $c = New-Object System.Net.Sockets.TcpClient('127.0.0.1', 28012); $c.Close(); break } catch { Start-Sleep -Milliseconds 500 }
}
Start-Sleep -Seconds 3
# wait until the VM answers memory reads (the socket opens before the game is loaded)
& python -c "import sys,time; sys.path.insert(0,r'$tools'); from pine import Pine
for i in range(120):
    try:
        p=Pine(); p.r32(0x100000); break
    except Exception: time.sleep(0.5)"
