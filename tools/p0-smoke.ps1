param(
    [string]$Exe = "dist\ZaYuCursorHalo.exe"
)

$ErrorActionPreference = "Stop"
$exePath = (Resolve-Path $Exe).Path

Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class P0Native
{
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }

    [StructLayout(LayoutKind.Sequential)]
    public struct POINT { public int X, Y; }

    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr hWnd, StringBuilder text, int count);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT point);
    [DllImport("user32.dll")] public static extern IntPtr WindowFromPoint(POINT point);
    [DllImport("user32.dll")] public static extern IntPtr GetWindow(IntPtr hWnd, uint command);
    [DllImport("user32.dll")] public static extern void mouse_event(uint flags, int dx, int dy, uint data, UIntPtr extraInfo);
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extraInfo);

    public static IntPtr FindOverlay()
    {
        IntPtr result = IntPtr.Zero;
        EnumWindows((hWnd, lParam) =>
        {
            var name = new StringBuilder(256);
            GetClassName(hWnd, name, name.Capacity);
            if (name.ToString() == "ZaYuCursorHalo.Overlay.P0")
            {
                result = hWnd;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }

    public static string Describe(IntPtr hWnd)
    {
        if (hWnd == IntPtr.Zero)
            return "<null>";
        var name = new StringBuilder(256);
        GetClassName(hWnd, name, name.Capacity);
        RECT rect;
        GetWindowRect(hWnd, out rect);
        return String.Format("{0} visible={1} rect={2},{3},{4},{5}",
            name, IsWindowVisible(hWnd), rect.Left, rect.Top, rect.Right, rect.Bottom);
    }

    public static void SendHotkey()
    {
        const byte VK_CONTROL = 0x11;
        const byte VK_MENU = 0x12;
        const byte VK_H = 0x48;
        keybd_event(VK_CONTROL, 0, 0, UIntPtr.Zero);
        keybd_event(VK_MENU, 0, 0, UIntPtr.Zero);
        keybd_event(VK_H, 0, 0, UIntPtr.Zero);
        System.Threading.Thread.Sleep(60);
        keybd_event(VK_H, 0, 2, UIntPtr.Zero);
        keybd_event(VK_MENU, 0, 2, UIntPtr.Zero);
        keybd_event(VK_CONTROL, 0, 2, UIntPtr.Zero);
    }

    public static void MoveMouseRelative(int dx, int dy)
    {
        const uint MOUSEEVENTF_MOVE = 0x0001;
        mouse_event(MOUSEEVENTF_MOVE, dx, dy, 0, UIntPtr.Zero);
    }
}
'@

function Get-Overlay {
    [P0Native]::FindOverlay()
}

function Wait-Overlay([bool]$visible, [int]$timeoutMs = 2000) {
    $deadline = [DateTime]::UtcNow.AddMilliseconds($timeoutMs)
    do {
        $h = Get-Overlay
        if ($h -ne [IntPtr]::Zero -and ([P0Native]::IsWindowVisible($h)) -eq $visible) {
            return $h
        }
        Start-Sleep -Milliseconds 50
    } while ([DateTime]::UtcNow -lt $deadline)
    return [IntPtr]::Zero
}

function Get-CpuSeconds($process) {
    $process.Refresh()
    $process.TotalProcessorTime.TotalSeconds
}

Get-Process ZaYuCursorHalo -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 400

$app = Start-Process -FilePath $exePath -PassThru
try {
    Start-Sleep -Milliseconds 900
    if ($app.HasExited) {
        throw "ZaYuCursorHalo exited during startup with code $($app.ExitCode)"
    }

    $overlay = Wait-Overlay $true 3000
    if ($overlay -eq [IntPtr]::Zero) {
        throw "Overlay did not become visible"
    }

    $foregroundBefore = [P0Native]::GetForegroundWindow()
    $initialDescription = [P0Native]::Describe($overlay)
    $initialRect = New-Object P0Native+RECT
    [P0Native]::GetWindowRect($overlay, [ref]$initialRect) | Out-Null
    Write-Host "overlay_initial: $initialDescription"
    Write-Host "foreground_before: $([P0Native]::Describe($foregroundBefore))"

    [P0Native]::MoveMouseRelative(70, 45)
    Start-Sleep -Milliseconds 500
    $movedHwnd = Wait-Overlay $true 1000
    $movedRect = New-Object P0Native+RECT
    [P0Native]::GetWindowRect($movedHwnd, [ref]$movedRect) | Out-Null
    $moved = ($movedRect.Left -ne $initialRect.Left) -or ($movedRect.Top -ne $initialRect.Top)
    Write-Host "overlay_after_move: $([P0Native]::Describe($movedHwnd)); moved=$moved"

    $foregroundAfterMove = [P0Native]::GetForegroundWindow()
    Write-Host "foreground_after_move: $([P0Native]::Describe($foregroundAfterMove)); unchanged=$($foregroundAfterMove -eq $foregroundBefore)"

    $cursor = New-Object P0Native+POINT
    [P0Native]::GetCursorPos([ref]$cursor) | Out-Null
    $ringPoint = New-Object P0Native+POINT
    $ringPoint.X = $cursor.X + 30
    $ringPoint.Y = $cursor.Y
    $hit = [P0Native]::WindowFromPoint($ringPoint)
    Write-Host "window_from_ring_point: $([P0Native]::Describe($hit)); overlay_hit=$($hit -eq $overlay)"

    [P0Native]::SendHotkey()
    $hidden = Wait-Overlay $false 2000
    Write-Host "hotkey_toggle_hidden=$($hidden -ne [IntPtr]::Zero)"

    [P0Native]::SendHotkey()
    $shown = Wait-Overlay $true 2000
    Write-Host "hotkey_toggle_shown=$($shown -ne [IntPtr]::Zero)"

    Start-Sleep -Milliseconds 800
    $cpu0 = Get-CpuSeconds $app
    $wall = [Diagnostics.Stopwatch]::StartNew()
    Start-Sleep -Seconds 4
    $wall.Stop()
    $cpu1 = Get-CpuSeconds $app
    $app.Refresh()
    $idleCpu = (($cpu1 - $cpu0) / $wall.Elapsed.TotalSeconds) * 100.0
    Write-Host ("idle_cpu_percent_one_core={0:N2}" -f $idleCpu)
    Write-Host ("working_set_mb={0:N1}" -f ($app.WorkingSet64 / 1MB))
    Write-Host "foreground_after_hotkey: $([P0Native]::Describe([P0Native]::GetForegroundWindow()))"
}
finally {
    Get-Process ZaYuCursorHalo -ErrorAction SilentlyContinue | Stop-Process -Force
}
