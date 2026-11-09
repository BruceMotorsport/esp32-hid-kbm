$ErrorActionPreference = 'Stop'
$ks = New-Object -ComObject WScript.Shell
for ($i = 1; $i -le 3; $i++) { $ks.SendKeys('^-'); Start-Sleep -Milliseconds 350 }
Write-Output 'zoomed out'
