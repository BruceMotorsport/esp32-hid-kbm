$ErrorActionPreference = 'Stop'
# Navigate his Chrome: clipboard -> Ctrl+L -> Ctrl+V -> Enter.
# Edit scripts/url.txt (UTF-8) first, or pass a file path:  go.ps1 C:\path\to\url.txt
# Run raise.ps1 first so keystrokes land on the right window.
$dir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$file = if ($args.Count -ge 1) { $args[0] } else { Join-Path $dir 'url.txt' }
$s = ([System.IO.File]::ReadAllText($file, [System.Text.Encoding]::UTF8)).Trim()
Set-Clipboard -Value $s
$ks = New-Object -ComObject WScript.Shell
$ks.SendKeys('^l')
Start-Sleep -Milliseconds 600
$ks.SendKeys('^v')
Start-Sleep -Milliseconds 600
$ks.SendKeys('{ENTER}')
Write-Output ('nav: ' + $s)
