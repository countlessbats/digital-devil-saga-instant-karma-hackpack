# Good Karma v@@VERSION@@ setup for PCSX2 2.x (Windows PowerShell 5.1+).
#   Install.bat runs this with no arguments: the setup window.
#   The uninstaller placed in <PCSX2 data>\goodkarma runs it with -Uninstall -DataRoot <folder>.
# What it changes, and all the uninstaller removes:
#   <patches>\SLUS-20974_D7273511_GoodKarma.pnach                     (the mods)
#   <gamesettings>\SLUS-20974_D7273511.ini  [Patches] "Enable = Good Karma - ..." lines (other lines are kept;
#                                                                      a backup of the original goes to goodkarma\backup)
#   <PCSX2 data>\goodkarma\                                            (this script, uninstaller, backups)
param([switch]$Uninstall, [string]$DataRoot)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
try { Add-Type -Name Dpi -Namespace GkNative -MemberDefinition '[DllImport("user32.dll")] public static extern bool SetProcessDPIAware();'; [void][GkNative.Dpi]::SetProcessDPIAware() } catch { }
[System.Windows.Forms.Application]::EnableVisualStyles()

$Version   = '@@VERSION@@'
$Serial    = 'SLUS-20974'
$Crc       = 'D7273511'
$PnachName = "${Serial}_${Crc}_GoodKarma.pnach"
$IniName   = "${Serial}_${Crc}.ini"
$Prefix    = 'Good Karma - '
$Here      = Split-Path -Parent $MyInvocation.MyCommand.Path
$Utf8      = New-Object System.Text.UTF8Encoding($false)

function Show-Msg([string]$text, [string]$icon = 'Information', [string]$buttons = 'OK') {
    [System.Windows.Forms.MessageBox]::Show($text, "Good Karma v$Version", $buttons, $icon)
}

# ---- disc image check: reads SYSTEM.CNF and the boot ELF from an ISO 9660 image, ELF CRC as PCSX2 computes it --
Add-Type -Language CSharp -TypeDefinition @'
using System; using System.IO; using System.Text;
public static class GkIso {
    static FileStream fs; static int ssize, doff;
    static byte[] Sector(long lba) {
        var b = new byte[2048]; fs.Seek(lba * ssize + doff, SeekOrigin.Begin);
        int n = 0; while (n < 2048) { int r = fs.Read(b, n, 2048 - n); if (r <= 0) throw new IOException("short read"); n += r; }
        return b;
    }
    static byte[] ReadFile(uint lba, uint size) {
        var o = new byte[size]; uint done = 0;
        for (long s = lba; done < size; s++) { var b = Sector(s); uint k = Math.Min(2048u, size - done); Array.Copy(b, 0, o, done, k); done += k; }
        return o;
    }
    static bool Find(uint dirLba, uint dirSize, string name, out uint lba, out uint size) {
        lba = size = 0; var d = ReadFile(dirLba, dirSize); int p = 0;
        while (p < d.Length) {
            int len = d[p];
            if (len == 0) { p = (p / 2048 + 1) * 2048; continue; }
            int nl = d[p + 32]; string n = Encoding.ASCII.GetString(d, p + 33, nl);
            int semi = n.IndexOf(';'); if (semi >= 0) n = n.Substring(0, semi);
            if (string.Equals(n, name, StringComparison.OrdinalIgnoreCase)) {
                lba = BitConverter.ToUInt32(d, p + 2); size = BitConverter.ToUInt32(d, p + 10); return true;
            }
            p += len;
        }
        return false;
    }
    // Returns "SERIAL CRC" (e.g. "SLUS-20974 D7273511"), or throws with a reason.
    public static string Identify(string path) {
        using (fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite)) {
            int[][] fmts = { new[] { 2048, 0 }, new[] { 2352, 24 }, new[] { 2352, 16 } };
            ssize = 0;
            foreach (var f in fmts) {
                long at = 16L * f[0] + f[1] + 1; if (at + 5 > fs.Length) continue;
                var b = new byte[5]; fs.Seek(at, SeekOrigin.Begin); fs.Read(b, 0, 5);
                if (Encoding.ASCII.GetString(b) == "CD001") { ssize = f[0]; doff = f[1]; break; }
            }
            if (ssize == 0) throw new InvalidDataException("not a readable disc image");
            var pvd = Sector(16);
            uint rootLba = BitConverter.ToUInt32(pvd, 156 + 2), rootSize = BitConverter.ToUInt32(pvd, 156 + 10);
            uint l, s;
            if (!Find(rootLba, rootSize, "SYSTEM.CNF", out l, out s)) throw new InvalidDataException("no SYSTEM.CNF: not a PS2 game disc");
            string cnf = Encoding.ASCII.GetString(ReadFile(l, s)); string boot = null;
            foreach (var line in cnf.Split('\n')) {
                var t = line.Trim(); if (!t.StartsWith("BOOT2", StringComparison.OrdinalIgnoreCase)) continue;
                int c = t.IndexOf(':'); boot = t.Substring(c + 1).TrimStart('\\', '/'); break;
            }
            if (boot == null) throw new InvalidDataException("no BOOT2 line in SYSTEM.CNF");
            int semi = boot.IndexOf(';'); if (semi >= 0) boot = boot.Substring(0, semi);
            if (boot.Contains("\\") || boot.Contains("/")) throw new InvalidDataException("unexpected boot path " + boot);
            if (!Find(rootLba, rootSize, boot, out l, out s)) throw new InvalidDataException("boot file " + boot + " missing");
            var elf = ReadFile(l, s); uint crc = 0;
            for (int i = 0; i + 4 <= elf.Length; i += 4) crc ^= BitConverter.ToUInt32(elf, i);
            string serial = boot.Replace('_', '-').Replace(".", "");
            return serial + " " + crc.ToString("X8");
        }
    }
}
'@

function Test-GameImage([string]$path) {
    # -> @{ Ok = $true/$false/$null (unknown); Text = ... }
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return @{ Ok = $false; Text = 'File not found.' } }
    $ext = [IO.Path]::GetExtension($path).ToLowerInvariant()
    if ($ext -in '.chd', '.cso', '.zso', '.gz', '.7z', '.zip', '.rar') {
        return @{ Ok = $null; Text = "Compressed image ($ext): can't be checked here. Make sure it is the USA release (SLUS-20974)." }
    }
    try { $id = [GkIso]::Identify($path) } catch {
        $e = $_.Exception; if ($e.InnerException) { $e = $e.InnerException }
        return @{ Ok = $null; Text = "Couldn't read the image ($($e.Message))." }
    }
    if ($id -eq "$Serial $Crc") { return @{ Ok = $true; Text = "Verified: Digital Devil Saga (USA), $Serial, CRC $Crc." } }
    return @{ Ok = $false; Text = "This is $id, not Digital Devil Saga USA ($Serial $Crc). Good Karma only works with the USA release." }
}

# ---- PCSX2 data folder ------------------------------------------------------------------------------------------
function Read-IniValue([string]$file, [string]$section, [string]$key) {
    if (-not (Test-Path -LiteralPath $file)) { return $null }
    $in = $false
    foreach ($line in [IO.File]::ReadAllLines($file)) {
        $t = $line.Trim()
        if ($t -match '^\[(.+)\]$') { $in = ($Matches[1] -eq $section); continue }
        if ($in -and $t -match '^([^=]+?)\s*=\s*(.*)$' -and $Matches[1] -eq $key) { return $Matches[2] }
    }
    return $null
}

function Get-Pcsx2Folder([string]$root, [string]$key, [string]$default) {
    $v = Read-IniValue (Join-Path $root 'inis\PCSX2.ini') 'Folders' $key
    if ([string]::IsNullOrWhiteSpace($v)) { $v = $default }
    $v = $v.Trim().Trim('"')
    if ([IO.Path]::IsPathRooted($v)) { return $v }
    return (Join-Path $root $v)
}

function Test-DataRoot([string]$root) {
    return ($root -and (Test-Path -LiteralPath (Join-Path $root 'inis\PCSX2.ini') -PathType Leaf))
}

function Find-DataRoots {
    $found = New-Object System.Collections.Generic.List[string]
    $add = { param($p) if ($p -and (Test-DataRoot $p)) { $full = [IO.Path]::GetFullPath($p).TrimEnd('\'); if (-not ($found -contains $full)) { $found.Add($full) } } }
    $docs = [Environment]::GetFolderPath('MyDocuments')
    if ($docs) { & $add (Join-Path $docs 'PCSX2') }
    # portable installs: PCSX2 program folders with portable.txt / portable.ini
    $exeDirs = New-Object System.Collections.Generic.List[string]
    foreach ($p in @($env:ProgramFiles, ${env:ProgramFiles(x86)}, $env:LOCALAPPDATA)) { if ($p) { $exeDirs.Add((Join-Path $p 'PCSX2')) } }
    foreach ($hive in 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*', 'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*', 'HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*') {
        try {
            Get-ItemProperty -Path $hive -ErrorAction SilentlyContinue | Where-Object { $_.DisplayName -like 'PCSX2*' -and $_.InstallLocation } |
                ForEach-Object { $exeDirs.Add($_.InstallLocation.Trim('"')) }
        } catch { }
    }
    try { Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -like 'pcsx2*' -and $_.Path } | ForEach-Object { $exeDirs.Add((Split-Path -Parent $_.Path)) } } catch { }
    foreach ($d in $exeDirs) {
        if (-not (Test-Path -LiteralPath $d -PathType Container)) { continue }
        $txt = Join-Path $d 'portable.txt'
        if (Test-Path -LiteralPath $txt) {
            $rel = ([IO.File]::ReadAllText($txt)).Trim()
            if ($rel) { if ([IO.Path]::IsPathRooted($rel)) { & $add $rel } else { & $add (Join-Path $d $rel) } } else { & $add $d }
        } elseif (Test-Path -LiteralPath (Join-Path $d 'portable.ini')) { & $add $d }
    }
    return ,$found
}

# ---- install state -----------------------------------------------------------------------------------------------
function Get-Sections([string]$pnachPath) {
    # -> ordered list of @{ Name; Title; Description } from the pnach's [Good Karma - ...] sections
    $list = New-Object System.Collections.Generic.List[object]; $cur = $null
    foreach ($line in [IO.File]::ReadAllLines($pnachPath)) {
        if ($line -match '^\[(.+)\]$') { $cur = @{ Name = $Matches[1]; Title = $Matches[1].Replace($Prefix, ''); Description = '' }; $list.Add($cur) }
        elseif ($cur -and $line -like 'description=*') { $cur.Description = $line.Substring(12) }
    }
    return ,$list
}

function Get-EnabledModules([string]$root) {
    $ini = Join-Path (Get-Pcsx2Folder $root 'GameSettings' 'gamesettings') $IniName
    $on = @(); if (-not (Test-Path -LiteralPath $ini)) { return ,$on }
    $in = $false
    foreach ($line in [IO.File]::ReadAllLines($ini)) {
        $t = $line.Trim()
        if ($t -match '^\[(.+)\]$') { $in = ($Matches[1] -eq 'Patches'); continue }
        if ($in -and $t -match '^Enable\s*=\s*(.+)$' -and $Matches[1].StartsWith($Prefix)) { $on += $Matches[1].Trim() }
    }
    return ,$on
}

function Test-Installed([string]$root) {
    return (Test-Path -LiteralPath (Join-Path (Get-Pcsx2Folder $root 'Patches' 'patches') $PnachName))
}

function Write-TextAtomic([string]$path, [string]$text) {
    $tmp = "$path.gk-new"
    [IO.File]::WriteAllText($tmp, $text, $Utf8)
    if (Test-Path -LiteralPath $path) { [IO.File]::Replace($tmp, $path, [NullString]::Value) } else { [IO.File]::Move($tmp, $path) }
}

function Add-BeforeTrailingBlanks($list, [string[]]$items) {
    $i = $list.Count; while ($i -gt 0 -and $list[$i - 1].Trim() -eq '') { $i-- }
    if ($items.Count) { $list.InsertRange($i, $items) }
}

function Set-EnableLines([string]$ini, [string[]]$names, [string]$backupDir) {
    # Rewrites only the "Enable = Good Karma - ..." lines of [Patches]; every other line is kept as it was.
    $lines = @(); if (Test-Path -LiteralPath $ini) { $lines = [IO.File]::ReadAllLines($ini) }
    if ($backupDir -and (Test-Path -LiteralPath $ini)) {
        $bak = Join-Path $backupDir "$IniName.original"
        if (-not (Test-Path -LiteralPath $bak)) { [IO.File]::Copy($ini, $bak) }
    }
    $new = [string[]]@($names | ForEach-Object { "Enable = $_" })
    $out = New-Object System.Collections.Generic.List[string]
    $inPatches = $false; $inserted = $false
    foreach ($line in $lines) {
        $t = $line.Trim()
        if ($t -match '^\[(.+)\]$') {
            if ($inPatches -and -not $inserted) { Add-BeforeTrailingBlanks $out $new; $inserted = $true }
            $inPatches = ($Matches[1] -eq 'Patches'); $out.Add($line); continue
        }
        if ($inPatches -and $t -match '^Enable\s*=\s*(.+)$' -and $Matches[1].Trim().StartsWith($Prefix)) { continue }
        $out.Add($line)
    }
    if ($inPatches -and -not $inserted) { Add-BeforeTrailingBlanks $out $new; $inserted = $true }
    if (-not $inserted -and $new.Count) {
        if ($out.Count -and $out[$out.Count - 1].Trim() -ne '') { $out.Add('') }
        $out.Add('[Patches]'); $out.AddRange($new)
    }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $ini) | Out-Null
    Write-TextAtomic $ini (($out -join "`r`n") + "`r`n")
}

function Install-GoodKarma([string]$root, [string[]]$names) {
    $patches = Get-Pcsx2Folder $root 'Patches' 'patches'
    $gs = Get-Pcsx2Folder $root 'GameSettings' 'gamesettings'
    $gk = Join-Path $root 'goodkarma'; $backup = Join-Path $gk 'backup'
    New-Item -ItemType Directory -Force -Path $patches, $gs, $backup | Out-Null
    # an older Good Karma patch file under the game's plain name (manual installs) would load twice: move it aside
    $legacy = Join-Path $patches "${Serial}_${Crc}.pnach"
    if ((Test-Path -LiteralPath $legacy) -and ([IO.File]::ReadAllText($legacy) -match 'author=Good Karma')) {
        Move-Item -LiteralPath $legacy -Destination (Join-Path $backup "${Serial}_${Crc}.pnach.old") -Force
    }
    Write-TextAtomic (Join-Path $patches $PnachName) ([IO.File]::ReadAllText((Join-Path $Here $PnachName)))
    Set-EnableLines (Join-Path $gs $IniName) $names $backup
    # uninstaller in the data folder (this script + a launcher), unless we are running from there already
    $self = Join-Path $Here 'GoodKarma-Setup.ps1'; $dest = Join-Path $gk 'GoodKarma-Setup.ps1'
    if ([IO.Path]::GetFullPath($self) -ne [IO.Path]::GetFullPath($dest)) { Copy-Item -LiteralPath $self -Destination $dest -Force }
    $bat = "@echo off`r`nrem Removes Good Karma from this PCSX2 data folder.`r`n" +
           "`"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe`" -NoProfile -ExecutionPolicy Bypass -STA -WindowStyle Hidden -File `"%~dp0GoodKarma-Setup.ps1`" -Uninstall -DataRoot `"%~dp0..`"`r`n"
    [IO.File]::WriteAllText((Join-Path $gk 'Uninstall Good Karma.bat'), $bat, (New-Object System.Text.ASCIIEncoding))
}

function Uninstall-GoodKarma([string]$root) {
    $pn = Join-Path (Get-Pcsx2Folder $root 'Patches' 'patches') $PnachName
    if ((Test-Path -LiteralPath $pn) -and ([IO.File]::ReadAllText($pn) -match 'author=Good Karma')) { Remove-Item -LiteralPath $pn -Force }
    $ini = Join-Path (Get-Pcsx2Folder $root 'GameSettings' 'gamesettings') $IniName
    if (Test-Path -LiteralPath $ini) { Set-EnableLines $ini @() $null }
    $gk = Join-Path $root 'goodkarma'
    if (Test-Path -LiteralPath $gk) {
        # the folder may hold this running script: remove it once this process has exited
        $cmd = "ping -n 3 127.0.0.1 >nul & rd /s /q `"$gk`""
        Start-Process -FilePath "$env:SystemRoot\System32\cmd.exe" -ArgumentList '/c', $cmd -WindowStyle Hidden
    }
}

function Wait-Pcsx2Closed {
    while ($true) {
        $p = @(Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -like 'pcsx2*' })
        if (-not $p.Count) { return $true }
        $r = Show-Msg "PCSX2 is running. Please close it first (it can overwrite the game settings file), then click Retry." 'Warning' 'RetryCancel'
        if ($r -ne 'Retry') { return $false }
    }
}

# ---- uninstall mode (run from the uninstaller) --------------------------------------------------------------------
if ($Uninstall) {
    try {
        $root = [IO.Path]::GetFullPath($DataRoot)
        if (-not (Test-DataRoot $root)) { Show-Msg "Not a PCSX2 data folder:`n$root" 'Error'; exit 1 }
        if ((Show-Msg "Remove Good Karma from`n$root ?" 'Question' 'YesNo') -ne 'Yes') { exit 0 }
        if (-not (Wait-Pcsx2Closed)) { exit 0 }
        Uninstall-GoodKarma $root
        Show-Msg 'Good Karma was removed.'
    } catch { Show-Msg "Uninstall failed:`n$($_.Exception.Message)" 'Error' }
    exit 0
}

# ---- setup window -------------------------------------------------------------------------------------------------
$mutex = New-Object System.Threading.Mutex($false, 'Local\GoodKarmaSetup')
if (-not $mutex.WaitOne(0)) { exit 0 }                  # already open

try {
$modules = Get-Sections (Join-Path $Here $PnachName)
$form = New-Object System.Windows.Forms.Form
$form.Text = "Good Karma v$Version setup"
$form.AutoScaleMode = 'Dpi'
$form.Font = New-Object System.Drawing.Font('Segoe UI', 9)
$form.ClientSize = New-Object System.Drawing.Size(720, 780)
$form.StartPosition = 'CenterScreen'
$form.MinimumSize = New-Object System.Drawing.Size(600, 520)

$layout = New-Object System.Windows.Forms.TableLayoutPanel
$layout.Dock = 'Fill'; $layout.Padding = New-Object System.Windows.Forms.Padding(12); $layout.ColumnCount = 1
$form.Controls.Add($layout)
function New-Label([string]$text, [bool]$bold = $false) {
    $l = New-Object System.Windows.Forms.Label; $l.Text = $text; $l.AutoSize = $true
    $l.MaximumSize = New-Object System.Drawing.Size(680, 0)
    if ($bold) { $l.Font = New-Object System.Drawing.Font('Segoe UI', 9, [System.Drawing.FontStyle]::Bold) }
    $l.Margin = New-Object System.Windows.Forms.Padding(0, 6, 0, 2); return $l
}
function New-PathRow($control, [string]$buttonText) {
    $row = New-Object System.Windows.Forms.TableLayoutPanel; $row.ColumnCount = 2; $row.AutoSize = $true; $row.Dock = 'Top'
    [void]$row.ColumnStyles.Add((New-Object System.Windows.Forms.ColumnStyle('Percent', 100)))
    [void]$row.ColumnStyles.Add((New-Object System.Windows.Forms.ColumnStyle('AutoSize')))
    $control.Dock = 'Fill'; $b = New-Object System.Windows.Forms.Button; $b.Text = $buttonText; $b.AutoSize = $true
    $row.Controls.Add($control, 0, 0); $row.Controls.Add($b, 1, 0); return @($row, $b)
}

$layout.Controls.Add((New-Label "Mods for Shin Megami Tensei: Digital Devil Saga (USA) on PCSX2 2.x. No game data is included or changed: PCSX2 applies the mods when the game boots."))

# 1. game image
$layout.Controls.Add((New-Label '1. Your game disc image' $true))
$romBox = New-Object System.Windows.Forms.TextBox
$r = New-PathRow $romBox 'Browse...'; $layout.Controls.Add($r[0]); $romBrowse = $r[1]
$romStatus = New-Label 'Choose the disc image (.iso / .bin) you play in PCSX2.'; $layout.Controls.Add($romStatus)

# 2. PCSX2 data folder
$layout.Controls.Add((New-Label '2. PCSX2 data folder' $true))
$rootBox = New-Object System.Windows.Forms.ComboBox; $rootBox.DropDownStyle = 'DropDown'
$r = New-PathRow $rootBox 'Browse...'; $layout.Controls.Add($r[0]); $rootBrowse = $r[1]
$rootStatus = New-Label ''; $layout.Controls.Add($rootStatus)

# 3. modules
$layout.Controls.Add((New-Label '3. Modules' $true))
$modPanel = New-Object System.Windows.Forms.FlowLayoutPanel
$modPanel.FlowDirection = 'TopDown'; $modPanel.WrapContents = $false; $modPanel.AutoScroll = $true; $modPanel.Dock = 'Fill'
$modPanel.BorderStyle = 'FixedSingle'
$checks = @{}
foreach ($m in $modules) {
    $cb = New-Object System.Windows.Forms.CheckBox; $cb.Text = $m.Title; $cb.Checked = $true; $cb.AutoSize = $true
    $cb.Font = New-Object System.Drawing.Font('Segoe UI', 9, [System.Drawing.FontStyle]::Bold)
    $cb.Margin = New-Object System.Windows.Forms.Padding(6, 8, 0, 0)
    $d = New-Object System.Windows.Forms.Label; $d.Text = $m.Description; $d.AutoSize = $true
    $d.MaximumSize = New-Object System.Drawing.Size(640, 0); $d.Margin = New-Object System.Windows.Forms.Padding(24, 0, 0, 2)
    $modPanel.Controls.Add($cb); $modPanel.Controls.Add($d); $checks[$m.Name] = $cb
}
$modRow = $layout.Controls.Count; $layout.Controls.Add($modPanel)
$note = New-Label ''; $note.ForeColor = [System.Drawing.Color]::DarkGoldenrod; $layout.Controls.Add($note)

# buttons
$btnRow = New-Object System.Windows.Forms.FlowLayoutPanel; $btnRow.FlowDirection = 'RightToLeft'; $btnRow.AutoSize = $true; $btnRow.Dock = 'Top'
$installBtn = New-Object System.Windows.Forms.Button; $installBtn.Text = 'Install'; $installBtn.AutoSize = $true
$removeBtn = New-Object System.Windows.Forms.Button; $removeBtn.Text = 'Remove Good Karma'; $removeBtn.AutoSize = $true
$allBtn = New-Object System.Windows.Forms.Button; $allBtn.Text = 'Select all'; $allBtn.AutoSize = $true
$noneBtn = New-Object System.Windows.Forms.Button; $noneBtn.Text = 'Select none'; $noneBtn.AutoSize = $true
$btnRow.Controls.AddRange(@($installBtn, $removeBtn, $noneBtn, $allBtn)); $layout.Controls.Add($btnRow)
$layout.RowCount = $layout.Controls.Count
for ($i = 0; $i -lt $layout.RowCount; $i++) {
    $style = if ($i -eq $modRow) { New-Object System.Windows.Forms.RowStyle('Percent', 100) } else { New-Object System.Windows.Forms.RowStyle('AutoSize') }
    [void]$layout.RowStyles.Add($style)
    $layout.SetRow($layout.Controls[$i], $i)
}
$form.Add_Shown({ $modPanel.AutoScrollPosition = New-Object System.Drawing.Point(0, 0) })

$state = @{ RomOk = $null; RomChecked = $false }

function Update-Ui {
    $root = $rootBox.Text.Trim().Trim('"')
    $rootOk = Test-DataRoot $root
    if (-not $root) { $rootStatus.Text = 'No PCSX2 data folder found. Run PCSX2 once, or click Browse and pick the folder that contains "inis".' }
    elseif (-not $rootOk) { $rootStatus.Text = 'This folder has no inis\PCSX2.ini. Pick the PCSX2 data folder (PCSX2: Tools > Open Data Directory).' }
    elseif (Test-Installed $root) { $rootStatus.Text = 'Good Karma is installed here. Install again to change modules or update.' }
    elseif (Test-Path -LiteralPath (Join-Path (Get-Pcsx2Folder $root 'Patches' 'patches') "${Serial}_${Crc}.pnach")) { $rootStatus.Text = 'Good Karma will be added here. A patch file under the game''s plain name was found; if it is an older Good Karma it is moved to goodkarma\backup, otherwise it is left alone.' }
    else { $rootStatus.Text = 'Good Karma will be added here.' }
    $removeBtn.Enabled = $rootOk -and (Test-Installed $root)
    $any = @($checks.Values | Where-Object { $_.Checked }).Count -gt 0
    $installBtn.Enabled = $rootOk -and $state.RomChecked -and ($state.RomOk -ne $false) -and $any
    $skip = $checks["${Prefix}SceneSkip"]; $turbo = $checks["${Prefix}Native Turbo"]
    $note.Text = if ($skip -and $turbo -and $skip.Checked -and -not $turbo.Checked) { "SceneSkip's fast-forward uses Native Turbo; without it, START still skips each scene's skippable parts." } else { '' }
}

function Set-Root([string]$root) {
    $rootBox.Text = $root
    if (Test-DataRoot $root) {
        $on = Get-EnabledModules $root              # modules already on (from an earlier install) stay ticked
        if ($on.Count) { foreach ($k in $checks.Keys) { $checks[$k].Checked = ($on -contains $k) } }
    }
    Update-Ui
}

function Set-Rom([string]$path) {
    $romBox.Text = $path
    $form.Cursor = 'WaitCursor'; $romStatus.Text = 'Checking...'; $form.Refresh()
    $res = Test-GameImage $path
    $form.Cursor = 'Default'
    $state.RomChecked = $true; $state.RomOk = $res.Ok; $romStatus.Text = $res.Text
    $romStatus.ForeColor = if ($res.Ok -eq $true) { [System.Drawing.Color]::DarkGreen } elseif ($res.Ok -eq $false) { [System.Drawing.Color]::Firebrick } else { [System.Drawing.Color]::DarkGoldenrod }
    Update-Ui
}

$romBrowse.Add_Click({
    $dlg = New-Object System.Windows.Forms.OpenFileDialog
    $dlg.Filter = 'PS2 disc images (*.iso;*.bin;*.img;*.chd;*.cso;*.zso;*.gz)|*.iso;*.bin;*.img;*.chd;*.cso;*.zso;*.gz|All files (*.*)|*.*'
    $dlg.Title = 'Choose your Digital Devil Saga (USA) disc image'
    if ($dlg.ShowDialog($form) -eq 'OK') { Set-Rom $dlg.FileName }
})
$romBox.Add_Leave({ $p = $romBox.Text.Trim().Trim('"'); if ($p -and $p -ne $state.LastRom) { $state.LastRom = $p; Set-Rom $p } })
$rootBrowse.Add_Click({
    $dlg = New-Object System.Windows.Forms.FolderBrowserDialog
    $dlg.Description = 'Choose the PCSX2 data folder (the one that contains "inis"). In PCSX2: Tools > Open Data Directory.'
    if ($dlg.ShowDialog($form) -eq 'OK') { Set-Root $dlg.SelectedPath }
})
$rootBox.Add_TextChanged({ Update-Ui })
$rootBox.Add_SelectedIndexChanged({ Set-Root $rootBox.SelectedItem })
foreach ($cb in $checks.Values) { $cb.Add_CheckedChanged({ Update-Ui }) }
$allBtn.Add_Click({ foreach ($cb in $checks.Values) { $cb.Checked = $true } })
$noneBtn.Add_Click({ foreach ($cb in $checks.Values) { $cb.Checked = $false } })

$installBtn.Add_Click({
    $root = [IO.Path]::GetFullPath($rootBox.Text.Trim().Trim('"'))
    if ($state.RomOk -eq $null) {
        if ((Show-Msg "The disc image couldn't be verified.`n$($romStatus.Text)`n`nInstall anyway?" 'Warning' 'YesNo') -ne 'Yes') { return }
    }
    if (-not (Wait-Pcsx2Closed)) { return }
    $names = @($modules | Where-Object { $checks[$_.Name].Checked } | ForEach-Object { $_.Name })
    try {
        Install-GoodKarma $root $names
        Update-Ui
        Show-Msg ("Good Karma v$Version is installed in`n$root`n`nModules on: " + (($names | ForEach-Object { $_.Replace($Prefix, '') }) -join ', ') +
                  "`n`nStart the game in PCSX2. To change modules later, run Install.bat again or use the game's Properties > Patches in PCSX2." +
                  "`n`nTo uninstall: goodkarma\Uninstall Good Karma.bat in that folder.")
    } catch { Show-Msg "Install failed:`n$($_.Exception.Message)" 'Error' }
})
$removeBtn.Add_Click({
    $root = [IO.Path]::GetFullPath($rootBox.Text.Trim().Trim('"'))
    if ((Show-Msg "Remove Good Karma from`n$root ?" 'Question' 'YesNo') -ne 'Yes') { return }
    if (-not (Wait-Pcsx2Closed)) { return }
    try { Uninstall-GoodKarma $root; Update-Ui; Show-Msg 'Good Karma was removed.' } catch { Show-Msg "Remove failed:`n$($_.Exception.Message)" 'Error' }
})

$roots = Find-DataRoots
foreach ($p in $roots) { [void]$rootBox.Items.Add($p) }
if ($roots.Count -ge 1) { Set-Root $roots[0] } else { Update-Ui }
if ($roots.Count -gt 1) { $rootStatus.Text = "Found $($roots.Count) PCSX2 data folders: pick the one you play from in the list. " + $rootStatus.Text }

[void]$form.ShowDialog()
} catch {
    Show-Msg "Good Karma setup hit an error:`n$($_.Exception.Message)" 'Error'
} finally { $mutex.ReleaseMutex() }
