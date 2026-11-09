$ErrorActionPreference = 'Stop'
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class Win {
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc cb, IntPtr l);
  public delegate bool EnumWindowsProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern IntPtr GetShellWindow();
  public static System.Collections.Generic.List<string> Hits = new System.Collections.Generic.List<string>();
  public static System.Collections.Generic.List<IntPtr> Handles = new System.Collections.Generic.List<IntPtr>();
  public static bool Cb(IntPtr h, IntPtr l) {
    if (!IsWindowVisible(h)) return true;
    var sb = new StringBuilder(512);
    GetWindowText(h, sb, 512);
    string t = sb.ToString();
    uint pid; GetWindowThreadProcessId(h, out pid);
    Hits.Add(pid + "|" + t);
    Handles.Add(h);
    return true;
  }
}
"@
$match = $args[0]
[Win]::Hits.Clear(); [Win]::Handles.Clear()
[Win]::EnumWindows([Win+EnumWindowsProc]{ param($h,$l) [Win]::Cb($h,$l) }, [IntPtr]::Zero) | Out-Null
$chromePids = (Get-Process chrome -ErrorAction SilentlyContinue | ForEach-Object { $_.Id })
$idx = -1
for ($i=0; $i -lt [Win]::Hits.Count; $i++) {
  $parts = [Win]::Hits[$i] -split '\|',2
  if ($chromePids -contains [int]$parts[0] -and $parts[1] -like "*$match*") { $idx = $i; break }
}
if ($idx -lt 0) {
  Write-Output "NO MATCH for [$match]. Chrome windows:"
  for ($i=0; $i -lt [Win]::Hits.Count; $i++) {
    $parts = [Win]::Hits[$i] -split '\|',2
    if ($chromePids -contains [int]$parts[0] -and $parts[1].Length -gt 0) { Write-Output ("  " + $parts[1]) }
  }
  exit 1
}
$h = [Win]::Handles[$idx]
$ks = New-Object -ComObject WScript.Shell
$ks.SendKeys('%')
[Win]::ShowWindow($h, 3) | Out-Null
[Win]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 700
Write-Output ("raised handle " + $h + " title [" + (([Win]::Hits[$idx]) -split '\|',2)[1] + "]")
