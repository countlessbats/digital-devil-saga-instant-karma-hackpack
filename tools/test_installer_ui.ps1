# Opens the packaged setup window for a few seconds (topmost), optionally pre-selects a disc image, saves a
# screenshot of the window to %TEMP%\gk_setup.png and closes it. Nothing is installed.
param([Parameter(Mandatory)][string]$Package, [string]$Iso)
$script = Join-Path $Package 'GoodKarma\GoodKarma-Setup.ps1'
$src = [IO.File]::ReadAllText($script)
$hook = @"
`$form.TopMost = `$true
`$t = New-Object System.Windows.Forms.Timer; `$t.Interval = 2500
`$t.Add_Tick({
    `$t.Stop()
    $(if ($Iso) { "Set-Rom '$($Iso.Replace("'", "''"))'" })
    `$form.Refresh(); Start-Sleep -Milliseconds 300
    `$bmp = New-Object System.Drawing.Bitmap(`$form.Width, `$form.Height)
    `$g = [System.Drawing.Graphics]::FromImage(`$bmp); `$g.CopyFromScreen(`$form.Location, [System.Drawing.Point]::Empty, `$form.Size)
    `$bmp.Save((Join-Path `$env:TEMP 'gk_setup.png'))
    "root=`$(`$rootBox.Text) | status=`$(`$rootStatus.Text) | rom=`$(`$romStatus.Text) | install enabled=`$(`$installBtn.Enabled)" | Set-Content (Join-Path `$env:TEMP 'gk_setup.txt')
    `$form.Close()
})
`$t.Start()
[void]`$form.ShowDialog()
"@
$src = $src.Replace('[void]$form.ShowDialog()', $hook)
$tmp = Join-Path $Package 'GoodKarma\_uitest.ps1'
[IO.File]::WriteAllText($tmp, $src, (New-Object System.Text.UTF8Encoding($true)))
try { & $tmp } finally { Remove-Item -LiteralPath $tmp -Force }
Get-Content (Join-Path $env:TEMP 'gk_setup.txt')
