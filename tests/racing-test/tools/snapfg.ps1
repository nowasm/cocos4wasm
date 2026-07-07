# snapfg.ps1 — bring window to foreground and capture its screen rect.
# Usage: powershell -File snapfg.ps1 -Title "Cocos" -OutFile shot.png
param(
    [string]$Title,
    [string]$OutFile = "shot.png"
)

Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class WinFG {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
}
"@

$proc = Get-Process | Where-Object { $_.MainWindowTitle -like "*$Title*" } | Select-Object -First 1
if (-not $proc) { Write-Error "no window matching '$Title'"; exit 1 }
$hwnd = $proc.MainWindowHandle
[WinFG]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 600
$rect = New-Object WinFG+RECT
[WinFG]::GetWindowRect($hwnd, [ref]$rect) | Out-Null
$w = $rect.R - $rect.L; $h = $rect.B - $rect.T
$bmp = New-Object System.Drawing.Bitmap($w, $h)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($rect.L, $rect.T, 0, 0, (New-Object System.Drawing.Size($w, $h)))
$g.Dispose()
$bmp.Save($OutFile, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Output "saved $OutFile ($w x $h)"
