# drivekeys.ps1 — focus a window and inject scan-code key events (works with Raw Input).
# Usage: powershell -File drivekeys.ps1 -Title "Cocos Racing" -Script "W:4000,W+A:1500,W:3000"
#   Script = comma list of <keys>:<holdMs>; keys joined by '+', e.g. "W+A".
param(
    [string]$Title,
    [string]$Script = "W:3000"
)

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class KeyInj {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, IntPtr pid);
    [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool attach);
    [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hwnd, int cmd);

    public static bool Focus(IntPtr hwnd) {
        if (GetForegroundWindow() == hwnd) return true;
        uint fgThread = GetWindowThreadProcessId(GetForegroundWindow(), IntPtr.Zero);
        uint me = GetCurrentThreadId();
        AttachThreadInput(me, fgThread, true);
        ShowWindow(hwnd, 9); // SW_RESTORE
        bool ok = SetForegroundWindow(hwnd);
        AttachThreadInput(me, fgThread, false);
        System.Threading.Thread.Sleep(300);
        return GetForegroundWindow() == hwnd;
    }
    [StructLayout(LayoutKind.Sequential)]
    public struct INPUT { public uint type; public KEYBDINPUT ki; public long pad; }
    [StructLayout(LayoutKind.Sequential)]
    public struct KEYBDINPUT { public ushort wVk, wScan; public uint dwFlags, time; public IntPtr dwExtraInfo; }
    [DllImport("user32.dll")] public static extern uint SendInput(uint n, INPUT[] inputs, int size);
    [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint mapType);

    public static void Key(char c, bool up) {
        ushort vk = (ushort)char.ToUpper(c);
        var inp = new INPUT[1];
        inp[0].type = 1; // INPUT_KEYBOARD
        inp[0].ki.wVk = vk;
        inp[0].ki.wScan = (ushort)MapVirtualKey(vk, 0);
        inp[0].ki.dwFlags = up ? 2u : 0u; // KEYEVENTF_KEYUP
        SendInput(1, inp, Marshal.SizeOf(typeof(INPUT)));
    }
}
"@

$proc = Get-Process | Where-Object { $_.MainWindowTitle -like "*$Title*" } | Select-Object -First 1
if (-not $proc) { Write-Error "no window matching '$Title'"; exit 1 }
$focused = [KeyInj]::Focus($proc.MainWindowHandle)
Write-Output "foreground=$focused"
if (-not $focused) { Write-Error "could not focus target window"; exit 2 }
Start-Sleep -Milliseconds 400

foreach ($step in $Script.Split(',')) {
    $parts = $step.Split(':')
    $keys = $parts[0].Split('+')
    $hold = [int]$parts[1]
    foreach ($k in $keys) { [KeyInj]::Key($k[0], $false) }
    Start-Sleep -Milliseconds $hold
    foreach ($k in $keys) { [KeyInj]::Key($k[0], $true) }
    Start-Sleep -Milliseconds 120
}
Write-Output "drive script done"
