# Automated checks of installer/InstantKarma-Setup.ps1 logic (no window): loads its functions from the staged package
# and runs install / re-install / uninstall against scratch PCSX2 data folders. Usage:
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\test_installer.ps1 -Package <dist\InstantKarma-vX> [-Iso <path>]
param([Parameter(Mandatory)][string]$Package, [string]$Iso)
$ErrorActionPreference = 'Stop'
$script = Join-Path $Package 'InstantKarma\InstantKarma-Setup.ps1'
$src = [IO.File]::ReadAllText($script)
$src = $src.Substring(0, $src.IndexOf('# ---- uninstall mode'))
$src = $src -replace '(?m)^param\(.*$', ''
$Here = Join-Path $Package 'InstantKarma'
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
$orig = "[EmuCore/Speedhacks]`r`nEECycleRate = 1`r`n`r`n[Patches]`r`nEnable = Somebody Else - Widescreen`r`nEnable = Instant Karma - Old Thing`r`n`r`n[EmuCore/GS]`r`nupscale_multiplier = 4`r`n"
[IO.File]::WriteAllText($ini, $orig)
$legacy = Join-Path $root 'patches\SLUS-20974_D7273511.pnach'
[IO.File]::WriteAllText($legacy, "[Instant Karma - Native Turbo]`r`nauthor=Instant Karma v0.7.14`r`n")
$other = Join-Path $root 'patches\SLUS-20974_D7273511_other.pnach'; [IO.File]::WriteAllText($other, "[Someone]`r`n")

Check 'scratch root is recognised' (Test-DataRoot $root)
$mods = Get-Sections (Join-Path $Here $PnachName)
Check "pnach has 15 modules ($($mods.Count))" ($mods.Count -eq 15)
$pick = @('Instant Karma - Prey Eyes', 'Instant Karma - Native Turbo', 'Instant Karma - WordTripper')
Install-InstantKarma $root $pick
$pn = Join-Path $root "patches\$PnachName"
Check 'pnach installed, identical' ((Get-FileHash $pn).Hash -eq (Get-FileHash (Join-Path $Here $PnachName)).Hash)
Check 'legacy Instant Karma pnach moved to backup' (-not (Test-Path -LiteralPath $legacy) -and (Test-Path -LiteralPath (Join-Path $root 'instantkarma\backup\SLUS-20974_D7273511.pnach.old')))
Check 'other pnach untouched' (Test-Path -LiteralPath $other)
$t = [IO.File]::ReadAllText($ini)
Check 'other settings kept' ($t -match 'upscale_multiplier = 4' -and $t -match 'upscale_multiplier = 4' -and $t -match 'Enable = Somebody Else - Widescreen')
Check 'old Instant Karma line removed' ($t -notmatch 'Old Thing')
Check 'chosen modules enabled' (((Get-EnabledModules $root) -join '|') -eq ($pick -join '|'))
Check 'Core enabled with them' ([IO.File]::ReadAllText($ini) -match 'Enable = Instant Karma - Core')
Check 'enable lines inside [Patches]' ($t -match '\[Patches\]\r\nEnable = Somebody Else - Widescreen\r\nEnable = Instant Karma - Core\r\nEnable = Instant Karma - Prey Eyes')
Check 'original ini backed up' ([IO.File]::ReadAllText((Join-Path $root 'instantkarma\backup\SLUS-20974_D7273511.ini.original')) -eq $orig)
Check 'uninstaller placed' ((Test-Path -LiteralPath (Join-Path $root 'instantkarma\Uninstall Instant Karma.bat')) -and (Test-Path -LiteralPath (Join-Path $root 'instantkarma\InstantKarma-Setup.ps1')))
Check 'Native Turbo sets EE Cycle Rate 300%' ((Read-IniValue $ini 'EmuCore/Speedhacks' 'EECycleRate') -eq '3')
Check 'EECycleRate appears once' (([regex]::Matches([IO.File]::ReadAllText($ini), 'EECycleRate')).Count -eq 1)
Check 'no BOM in ini' ([IO.File]::ReadAllBytes($ini)[0] -ne 0xEF)

$pick2 = @('Instant Karma - OpenChests', 'Instant Karma - QuickStart')
Install-InstantKarma $root $pick2
Check 'Turbo off restores EE Cycle Rate' ((Read-IniValue $ini 'EmuCore/Speedhacks' 'EECycleRate') -eq '1')
Check 're-install switches modules' (((Get-EnabledModules $root) -join '|') -eq ($pick2 -join '|'))
Install-InstantKarma $root @('Instant Karma - SceneSkip')
Check 'SceneSkip brings Native Turbo' ((Get-EnabledModules $root) -contains 'Instant Karma - Native Turbo')
Install-InstantKarma $root $pick2
Check 'backup keeps the first original' ([IO.File]::ReadAllText((Join-Path $root 'instantkarma\backup\SLUS-20974_D7273511.ini.original')) -eq $orig)

# fresh root without a game settings file, patches folder redirected
$root2 = Join-Path $base 'Portable'
New-Item -ItemType Directory -Force -Path (Join-Path $root2 'inis') | Out-Null
$redir = Join-Path $base 'my patches'
[IO.File]::WriteAllText((Join-Path $root2 'inis\PCSX2.ini'), "[Folders]`r`nPatches = $redir`r`n")
Install-InstantKarma $root2 @('Instant Karma - OpenChests')
Check 'redirected patches folder used' (Test-Path -LiteralPath (Join-Path $redir $PnachName))
Check 'new game settings file created' (((Get-EnabledModules $root2) -join '|') -eq 'Instant Karma - OpenChests')
Check 'no cycle rate without Turbo' ((Read-IniValue (Join-Path $root2 'gamesettings\SLUS-20974_D7273511.ini') 'EmuCore/Speedhacks' 'EECycleRate') -eq $null)
Install-InstantKarma $root2 @('Instant Karma - OpenChests', 'Instant Karma - Native Turbo')
Check 'Turbo adds cycle rate section' ((Read-IniValue (Join-Path $root2 'gamesettings\SLUS-20974_D7273511.ini') 'EmuCore/Speedhacks' 'EECycleRate') -eq '3')
Uninstall-InstantKarma $root2; Start-Sleep -Seconds 3
Check 'uninstall removes the cycle rate it added' ((Read-IniValue (Join-Path $root2 'gamesettings\SLUS-20974_D7273511.ini') 'EmuCore/Speedhacks' 'EECycleRate') -eq $null)

Install-InstantKarma $root @('Instant Karma - Native Turbo')
# uninstall via the placed uninstaller's own copy of the script, the way the .bat runs it (window-less parts only)
Uninstall-InstantKarma $root
Start-Sleep -Seconds 4
$t = [IO.File]::ReadAllText($ini)
Check 'uninstall removes pnach' (-not (Test-Path -LiteralPath $pn))
Check 'uninstall removes only Instant Karma lines' ($t -notmatch 'Instant Karma' -and $t -match 'Somebody Else - Widescreen')
Check 'uninstall restores EE Cycle Rate' ((Read-IniValue $ini 'EmuCore/Speedhacks' 'EECycleRate') -eq '1')
Check 'uninstall removes instantkarma folder' (-not (Test-Path -LiteralPath (Join-Path $root 'instantkarma')))
Check 'uninstall leaves other pnach' (Test-Path -LiteralPath $other)

Remove-Item -LiteralPath $base -Recurse -Force
if ($fail) { "$fail check(s) FAILED"; exit 1 } else { 'all checks passed' }
