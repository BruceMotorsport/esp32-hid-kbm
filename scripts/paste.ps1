$ErrorActionPreference = 'Stop'
# Fill the focused field: clip.txt (UTF-8) -> Set-Clipboard -> Ctrl+A -> Ctrl+V.
# Edit scripts/clip.txt first, or pass a file path:  paste.ps1 C:\path\to\clip.txt
# Use this instead of /type for anything with a URL, a middot, an em dash, or >20 chars.
$dir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$file = if ($args.Count -ge 1) { $args[0] } else { Join-Path $dir 'clip.txt' }
$s = ([System.IO.File]::ReadAllText($file, [System.Text.Encoding]::UTF8)).Trim()
if ($s -like 'EDIT ME*') { Write-Error "REFUSED: $file still holds the placeholder. Put the real text in it first - this script Ctrl+A's the focused field, so running it anyway would overwrite whatever is there."; exit 1 }
Set-Clipboard -Value $s
$ks = New-Object -ComObject WScript.Shell
$ks.SendKeys('^a')
Start-Sleep -Milliseconds 400
$ks.SendKeys('^v')
Start-Sleep -Milliseconds 400
Write-Output ('pasted ' + $s.Length + ' chars from ' + $file)
