param(
    [string]$Exe = "dist/CursorHalo.exe"
)

$ErrorActionPreference = "Stop"
$exePath = (Resolve-Path $Exe).Path
$artifactRoot = Join-Path (Resolve-Path ".").Path "artifacts"
$configPath = Join-Path $env:APPDATA "CursorHalo\config.ini"

Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class UiProbe
{
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }

    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr hWnd, StringBuilder text, int count);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint flags, int dx, int dy, uint data, UIntPtr extraInfo);

    public static IntPtr FindByClass(string className)
    {
        IntPtr result = IntPtr.Zero;
        EnumWindows((hWnd, lParam) =>
        {
            var name = new StringBuilder(256);
            GetClassName(hWnd, name, name.Capacity);
            if (name.ToString() == className)
            {
                result = hWnd;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }

    public static void Click(int x, int y)
    {
        const uint LEFTDOWN = 0x0002;
        const uint LEFTUP = 0x0004;
        SetCursorPos(x, y);
        System.Threading.Thread.Sleep(80);
        mouse_event(LEFTDOWN, 0, 0, 0, UIntPtr.Zero);
        System.Threading.Thread.Sleep(50);
        mouse_event(LEFTUP, 0, 0, 0, UIntPtr.Zero);
        System.Threading.Thread.Sleep(250);
    }
}
'@

Get-Process CursorHalo -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 400
$app = Start-Process -FilePath $exePath -ArgumentList @("--settings") -PassThru
try {
    Start-Sleep -Milliseconds 1200
    $settings = [UiProbe]::FindByClass("CursorHalo.Settings.P1")
    if ($settings -eq [IntPtr]::Zero) {
        throw "settings window not found"
    }
    [UiProbe]::SetForegroundWindow($settings) | Out-Null
    $rect = New-Object UiProbe+RECT
    [UiProbe]::GetWindowRect($settings, [ref]$rect) | Out-Null
    $left = $rect.Left
    $top = $rect.Top
    Write-Host "settings_rect=$left,$top,$($rect.Right),$($rect.Bottom)"

    # Appearance tab, then amber swatch.
    [UiProbe]::Click($left + 78, $top + 162)
    Start-Sleep -Milliseconds 250
    Add-Type -AssemblyName System.Drawing
    $bmp = New-Object System.Drawing.Bitmap(640, 600)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($left - 20, $top - 20, 0, 0, $bmp.Size)
    $bmp.Save((Join-Path $artifactRoot "p1-settings-appearance.png"), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()

    [UiProbe]::Click($left + 328, $top + 392)
    Start-Sleep -Milliseconds 800

    # General tab and autostart toggle on/off.
    [UiProbe]::Click($left + 78, $top + 206)
    Start-Sleep -Milliseconds 250
    $bmp = New-Object System.Drawing.Bitmap(640, 600)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($left - 20, $top - 20, 0, 0, $bmp.Size)
    $bmp.Save((Join-Path $artifactRoot "p1-settings-general.png"), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()

    [UiProbe]::Click($left + 556, $top + 99)
    Start-Sleep -Milliseconds 400
    $runValue = (Get-ItemProperty "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run" -Name CursorHalo -ErrorAction SilentlyContinue).CursorHalo
    Write-Host "autostart_value=$runValue"
    [UiProbe]::Click($left + 556, $top + 99)
    Start-Sleep -Milliseconds 400
    $runValueAfter = (Get-ItemProperty "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run" -Name CursorHalo -ErrorAction SilentlyContinue).CursorHalo
    Write-Host "autostart_after=$runValueAfter"

    # Highlight tab and set radius to 50 DIP (100 px diameter) at the midpoint.
    [UiProbe]::Click($left + 78, $top + 118)
    Start-Sleep -Milliseconds 250
    [UiProbe]::Click($left + 382, $top + 428)
    Start-Sleep -Milliseconds 900

    if (Test-Path $configPath) {
        Write-Host "config_after:"
        Get-Content $configPath
    } else {
        Write-Host "config_missing"
    }

    # Close with the custom X and verify hiding, not exiting.
    [UiProbe]::Click($left + 569, $top + 29)
    Start-Sleep -Milliseconds 500
    $settingsVisible = [UiProbe]::IsWindowVisible($settings)
    Write-Host "settings_visible_after_close=$settingsVisible"
    Write-Host "process_alive=$(-not $app.HasExited)"
}
finally {
    $main = [UiProbe]::FindByClass("CursorHalo.Main.P0")
    if ($main -ne [IntPtr]::Zero) {
        # WM_CLOSE = 0x0010
        Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices;
public static class Closer { [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l); }
'@ -ErrorAction SilentlyContinue
        [Closer]::PostMessage($main, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
        Start-Sleep -Milliseconds 500
    }
    Get-Process CursorHalo -ErrorAction SilentlyContinue | Stop-Process -Force
}
