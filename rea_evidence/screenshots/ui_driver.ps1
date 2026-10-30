# Script de inspeccion y captura de UI de CYPE 3D (Version 2027)
param(
    [string]$Action = "inspect",
    [string]$ScreenshotName = "screenshot.png",
    [int]$ClickX = -1,
    [int]$ClickY = -1,
    [string]$SendKeysStr = ""
)

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$code = @"
using System;
using System.Text;
using System.Runtime.InteropServices;
using System.Collections.Generic;

public class Win32UI {
    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc enumProc, IntPtr lParam);
    [DllImport("user32.dll")]
    public static extern bool EnumChildWindows(IntPtr hWndParent, EnumWindowsProc lpEnumFunc, IntPtr lParam);
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);
    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll")]
    public static extern void SetCursorPos(int X, int Y);
    [DllImport("user32.dll")]
    public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, UIntPtr dwExtraInfo);
    [DllImport("user32.dll")]
    public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint nFlags);
    [DllImport("user32.dll")]
    public static extern IntPtr GetWindowDC(IntPtr hWnd);
    [DllImport("user32.dll")]
    public static extern int ReleaseDC(IntPtr hWnd, IntPtr hDC);
    [DllImport("gdi32.dll")]
    public static extern bool BitBlt(IntPtr hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, IntPtr hdcSrc, int nXSrc, int nYSrc, int dwRop);
    [DllImport("user32.dll")]
    public static extern IntPtr GetDesktopWindow();

    public const uint MOUSEEVENTF_LEFTDOWN = 0x0002;
    public const uint MOUSEEVENTF_LEFTUP = 0x0004;

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    public static string GetTitle(IntPtr hWnd) {
        StringBuilder sb = new StringBuilder(512);
        GetWindowText(hWnd, sb, 512);
        return sb.ToString();
    }

    public static string GetClass(IntPtr hWnd) {
        StringBuilder sb = new StringBuilder(256);
        GetClassName(hWnd, sb, 256);
        return sb.ToString();
    }

    public static void ClickAt(int x, int y) {
        SetCursorPos(x, y);
        System.Threading.Thread.Sleep(80);
        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, UIntPtr.Zero);
        System.Threading.Thread.Sleep(80);
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, UIntPtr.Zero);
    }
}
"@

Add-Type -TypeDefinition $code -Language CSharp
[void][Win32UI]::SetProcessDPIAware()

$outDir = "d:\Coding\CypeClon\rea_evidence\screenshots"
if (-not (Test-Path $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

function Get-CypeWindows {
    $procs = Get-Process | Where-Object { $_.ProcessName -match "CYPE" }
    $pids = @($procs | Select-Object -ExpandProperty Id)
    $list = New-Object System.Collections.ArrayList
    $cb = [Win32UI+EnumWindowsProc]{
        param($hWnd, $lParam)
        if ([Win32UI]::IsWindowVisible($hWnd)) {
            $wpid = [uint32]0
            [void][Win32UI]::GetWindowThreadProcessId($hWnd, [ref]$wpid)
            if ($pids -contains $wpid) {
                $rect = New-Object Win32UI+RECT
                [void][Win32UI]::GetWindowRect($hWnd, [ref]$rect)
                $w = $rect.Right - $rect.Left
                $h = $rect.Bottom - $rect.Top
                if ($w -gt 10 -and $h -gt 10) {
                    [void]$list.Add([PSCustomObject]@{
                        HWND = "0x{0:X}" -f $hWnd.ToInt64()
                        Handle = $hWnd
                        PID = $wpid
                        Title = [Win32UI]::GetTitle($hWnd)
                        Class = [Win32UI]::GetClass($hWnd)
                        Left = $rect.Left
                        Top = $rect.Top
                        Width = $w
                        Height = $h
                    })
                }
            }
        }
        return $true
    }
    [void][Win32UI]::EnumWindows($cb, [IntPtr]::Zero)
    return $list
}

function Get-ChildControls([IntPtr]$parentHwnd) {
    $children = New-Object System.Collections.ArrayList
    $cb = [Win32UI+EnumWindowsProc]{
        param($hWnd, $lParam)
        if ([Win32UI]::IsWindowVisible($hWnd)) {
            $rect = New-Object Win32UI+RECT
            [void][Win32UI]::GetWindowRect($hWnd, [ref]$rect)
            $w = $rect.Right - $rect.Left
            $h = $rect.Bottom - $rect.Top
            $title = [Win32UI]::GetTitle($hWnd)
            $cls = [Win32UI]::GetClass($hWnd)
            [void]$children.Add([PSCustomObject]@{
                HWND = "0x{0:X}" -f $hWnd.ToInt64()
                Title = $title
                Class = $cls
                Left = $rect.Left
                Top = $rect.Top
                Width = $w
                Height = $h
                CenterX = [int]($rect.Left + $w / 2)
                CenterY = [int]($rect.Top + $h / 2)
            })
        }
        return $true
    }
    [void][Win32UI]::EnumChildWindows($parentHwnd, $cb, [IntPtr]::Zero)
    return $children
}

function Capture-Screen([string]$fileName, [int]$x = 0, [int]$y = 0, [int]$w = 0, [int]$h = 0) {
    $bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    if ($w -le 0 -or $h -le 0) {
        $x = $bounds.X
        $y = $bounds.Y
        $w = $bounds.Width
        $h = $bounds.Height
    }
    $bmp = New-Object System.Drawing.Bitmap($w, $h)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($x, $y, 0, 0, (New-Object System.Drawing.Size($w, $h)))
    $g.Dispose()
    $fullPath = Join-Path $outDir $fileName
    $bmp.Save($fullPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Output "Saved screenshot: $fullPath ($w x $h)"
}

function Capture-Hwnd([IntPtr]$hWnd, [string]$fileName, [int]$w, [int]$h) {
    $bmp = New-Object System.Drawing.Bitmap($w, $h)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    # PW_RENDERFULLCONTENT = 2
    $ok = [Win32UI]::PrintWindow($hWnd, $hdc, 2)
    if (-not $ok) {
        $ok = [Win32UI]::PrintWindow($hWnd, $hdc, 0)
    }
    $g.ReleaseHdc($hdc)
    $g.Dispose()
    $fullPath = Join-Path $outDir $fileName
    $bmp.Save($fullPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Output "Saved HWND screenshot (PrintWindow=$ok): $fullPath ($w x $h)"
}

if ($Action -eq "launch") {
    $exePath = "D:\CYPE Ingenieros\Versi$([char]0xF3)n 2027\CYPE 3D\CYPE 3D.exe"
    $existing = Get-Process | Where-Object { $_.ProcessName -match "CYPE" }
    if (-not $existing) {
        $p = Start-Process -FilePath $exePath -WorkingDirectory (Split-Path $exePath) -PassThru
        Write-Output "Started PID $($p.Id). Polling windows..."
        for ($i = 0; $i -lt 20; $i++) {
            Start-Sleep -Seconds 1
            $wins = Get-CypeWindows
            if ($wins.Count -gt 0) {
                Write-Output "[$i s] Found $($wins.Count) windows:"
                $wins | Format-Table -AutoSize
                foreach ($w in $wins) {
                    if ($w.Width -gt 100 -and $w.Height -gt 100) {
                        Capture-Hwnd -hWnd $w.Handle -fileName ("probe_{0}s_{1}.png" -f $i, $w.HWND) -w $w.Width -h $w.Height
                    }
                }
            } else {
                $alive = Get-Process | Where-Object { $_.ProcessName -match "CYPE" }
                if (-not $alive) {
                    Write-Output "[$i s] CYPE process exited!"
                    break
                }
            }
        }
    } else {
        $wins = Get-CypeWindows
        $wins | Format-Table -AutoSize
    }
} elseif ($Action -eq "inspect") {
    $wins = Get-CypeWindows
    Write-Output "=== TOP-LEVEL CYPE WINDOWS ==="
    $wins | Format-Table -AutoSize
    foreach ($w in $wins) {
        Write-Output "=== CHILDREN OF $($w.HWND) [$($w.Title)] ==="
        $ch = Get-ChildControls $w.Handle
        $ch | Format-Table -AutoSize
    }
} elseif ($Action -eq "click") {
    $wins = Get-CypeWindows
    if ($wins.Count -gt 0) {
        [void][Win32UI]::SetForegroundWindow($wins[0].Handle)
        Start-Sleep -Milliseconds 200
    }
    if ($ClickX -ge 0 -and $ClickY -ge 0) {
        Write-Output "Clicking at ($ClickX, $ClickY)..."
        [Win32UI]::ClickAt($ClickX, $ClickY)
        Start-Sleep -Seconds 2
    }
    if ($SendKeysStr -ne "") {
        Write-Output "Sending keys: $SendKeysStr"
        [System.Windows.Forms.SendKeys]::SendWait($SendKeysStr)
        Start-Sleep -Seconds 2
    }
} elseif ($Action -eq "capture") {
    $wins = Get-CypeWindows
    if ($wins.Count -gt 0) {
        # Bring active dialog or main window to front
        foreach ($w in $wins) {
            [void][Win32UI]::SetForegroundWindow($w.Handle)
        }
        Start-Sleep -Milliseconds 400
    }
    Capture-Screen -fileName $ScreenshotName
} elseif ($Action -eq "capture_window") {
    $wins = Get-CypeWindows
    if ($wins.Count -gt 0) {
        # Pick topmost/dialog or main window
        $target = $wins[0]
        [void][Win32UI]::SetForegroundWindow($target.Handle)
        Start-Sleep -Milliseconds 400
        Capture-Screen -fileName $ScreenshotName -x $target.Left -y $target.Top -w $target.Width -h $target.Height
    }
}
