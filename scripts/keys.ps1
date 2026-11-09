$ErrorActionPreference = 'Stop'
$ks = New-Object -ComObject WScript.Shell
$ks.SendKeys($args[0])
Start-Sleep -Milliseconds 400
Write-Output ('sent: ' + $args[0])
