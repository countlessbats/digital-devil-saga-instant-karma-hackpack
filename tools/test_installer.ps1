# Automated checks of installer/GoodKarma-Setup.ps1 logic (no window): loads its functions from the staged package
# and runs install / re-install / uninstall against scratch PCSX2 data folders. Usage:
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\test_installer.ps1 -Package <dist\GoodKarma-vX> [-Iso <path>]
param([Parameter(Mandatory)][string]$Package, [string]$Iso)
$ErrorActionPreference = 'Stop'
$script = Join-Path $Package 'GoodKarma\GoodKarma-Setup.ps1'
$src = [IO.File]::ReadAllText($script)
$src = $src.Substring(0, $src.IndexOf('# ---- uninstall mode'))
$src = $src -replace '(?m)^param\(.*$', ''
$Here = Join-Path $Package 'GoodKarma'
. ([scriptblock]::Create($src.Replace('$Here      = Split-Path -Parent $MyInvocation.MyCommand.Path', '')))
$fail = 0
function Check([string]$what, [bool]$ok) { if ($ok) { "PASS $what" } else { "FAIL $what"; $script:fail++ } }

if ($Iso) {
    $r = Test-GameImage $Iso
    Check "ISO verifies ($($r.Text))" ($r.Ok -eq $true)
    $junk = Join-Path $env:TEMP 'gk-notaniso.iso'; [IO.File]::WriteAllBytes($junk, (New-Object byte[] 100000))
    $r = Test-GameImage $junk; Check "junk file is not verified ($($r.Text))" ($r.Ok -ne $true)
    $r = Test-GameImage 'C:\nope\missing.iso'; Check 'missing file rejected' ($r.Ok -eq $false)
}

$roots = Find-DataRoots
"detected data roots: $($roots -join ' | ')"

# scratch data root with a path that needs quoting care
$base = Join-Path $env:TEMP "gk test (it's) $(Get-Random)"
$root = Join-Path $base 'PCSX2'
New-Item -ItemType Directory -Force -Path (Join-Path $root 'inis'), (Join-Path $root 'patches'), (Join-Path $root 'gamesettings') | Out-Null
[IO.File]::WriteAllText((Join-Path $root 'inis\PCSX2.ini'), "[UI]`r`nFoo = 1`r`n")
$ini = Join-Path $root 'gamesettings\SLUS-20974_D7273511.ini'
$orig = "[EmuCore/Speedhacks]`r`nEECycleRate = 3`r`n`r`n[Patches]`r`nEnable = Somebody Else - Widescreen`r`nEnable = Good Karma - Old Thing`r`n`r`n[EmuCore/GS]`r`nupscale_multiplier = 4`r`n"
[IO.File]::WriteAllText($ini, $orig)
$legacy = Join-Path $root 'patches\SLUS-20974_D7273511.pnach'
[IO.File]::WriteAllText($legacy, "[Good Karma - Native Turbo]`r`nauthor=Good Karma v0.7.14`r`n")
$other = Join-Path $root 'patches\SLUS-20974_D7273511_other.pnach'; [IO.File]::WriteAllText($other, "[Someone]`r`n")

Check 'scratch root is recognised' (Test-DataRoot $root)
$mods = Get-Sections (Join-Path $Here $PnachName)
Check "pnach has 11 modules ($($mods.Count))" ($mods.Count -eq 11)
$pick = @('Good Karma - Prey Eyes', 'Good Karma - Native Turbo', 'Good Karma - WordTripper')
Install-GoodKarma $root $pick
$pn = Join-Path $root "patches\$PnachName"
Check 'pnach installed, identical' ((Get-FileHash $pn).Hash -eq (Get-FileHash (Join-Path $Here $PnachName)).Hash)
Check 'legacy Good Karma pnach moved to backup' (-not (Test-Path -LiteralPath $legacy) -and (Test-Path -LiteralPath (Join-Path $root 'goodkarma\backup\SLUS-20974_D7273511.pnach.old')))
Check 'other pnach untouched' (Test-Path -LiteralPath $other)
$t = [IO.File]::ReadAllText($ini)
Check 'other settings kept' ($t -match 'EECycleRate = 3' -and $t -match 'upscale_multiplier = 4' -and $t -match 'Enable = Somebody Else - Widescreen')
Check 'old Good Karma line removed' ($t -notmatch 'Old Thing')
Check 'chosen modules enabled' (((Get-EnabledModules $root) -join '|') -eq ($pick -join '|'))
Check 'enable lines inside [Patches]' ($t -match '\[Patches\]\r\nEnable = Somebody Else - Widescreen\r\nEnable = Good Karma - Prey Eyes')
Check 'original ini backed up' ([IO.File]::ReadAllText((Join-Path $root 'goodkarma\backup\SLUS-20974_D7273511.ini.original')) -eq $orig)
Check 'uninstaller placed' ((Test-Path -LiteralPath (Join-Path $root 'goodkarma\Uninstall Good Karma.bat')) -and (Test-Path -LiteralPath (Join-Path $root 'goodkarma\GoodKarma-Setup.ps1')))
Check 'no BOM in ini' ([IO.File]::ReadAllBytes($ini)[0] -ne 0xEF)

$pick2 = @('Good Karma - SceneSkip', 'Good Karma - QuickStart')
Install-GoodKarma $root $pick2
Check 're-install switches modules' (((Get-EnabledModules $root) -join '|') -eq ($pick2 -join '|'))
Check 'backup keeps the first original' ([IO.File]::ReadAllText((Join-Path $root 'goodkarma\backup\SLUS-20974_D7273511.ini.original')) -eq $orig)

# fresh root without a game settings file, patches folder redirected
$root2 = Join-Path $base 'Portable'
New-Item -ItemType Directory -Force -Path (Join-Path $root2 'inis') | Out-Null
$redir = Join-Path $base 'my patches'
[IO.File]::WriteAllText((Join-Path $root2 'inis\PCSX2.ini'), "[Folders]`r`nPatches = $redir`r`n")
Install-GoodKarma $root2 @('Good Karma - OpenChests')
Check 'redirected patches folder used' (Test-Path -LiteralPath (Join-Path $redir $PnachName))
Check 'new game settings file created' (((Get-EnabledModules $root2) -join '|') -eq 'Good Karma - OpenChests')

# uninstall via the placed uninstaller's own copy of the script, the way the .bat runs it (window-less parts only)
Uninstall-GoodKarma $root
Start-Sleep -Seconds 4
$t = [IO.File]::ReadAllText($ini)
Check 'uninstall removes pnach' (-not (Test-Path -LiteralPath $pn))
Check 'uninstall removes only Good Karma lines' ($t -notmatch 'Good Karma' -and $t -match 'Somebody Else - Widescreen' -and $t -match 'EECycleRate = 3')
Check 'uninstall removes goodkarma folder' (-not (Test-Path -LiteralPath (Join-Path $root 'goodkarma')))
Check 'uninstall leaves other pnach' (Test-Path -LiteralPath $other)

Remove-Item -LiteralPath $base -Recurse -Force
if ($fail) { "$fail check(s) FAILED"; exit 1 } else { 'all checks passed' }
