# Launch the private test PCSX2 minimized on a savestate. Usage: launch_test.ps1 <slot>
param([int]$Slot = 3)
Get-CimInstance Win32_Process -Filter "Name='pcsx2-qt.exe'" | Where-Object { $_.ExecutablePath -like '<local path>' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }
$state = '<local path>).{0:D2}.p2s' -f $Slot
Start-Process -WindowStyle Minimized -FilePath '<local path>' -ArgumentList '-batch','-statefile',"`"$state`"",'--','"<local path>).iso"'
