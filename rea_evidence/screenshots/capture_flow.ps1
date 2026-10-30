# Script autonomo para capturar todas las pantallas de referencia de CYPE 3D (Version 2027)
$ErrorActionPreference = "Continue"

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$code = @"
using System;
using System.Text;
using System.Runtime.InteropServices;
using System.Collections.Generic;

public class Win32Cap {
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
    public static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")]
    public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern bool SetWindowText(IntPtr hWnd, string lpString);

    public const uint MOUSEEVENTF_LEFTDOWN = 0x0002;
    public const uint MOUSEEVENTF_LEFTUP = 0x0004;
    public const uint WM_LBUTTONDOWN = 0x0201;
    public const uint WM_LBUTTONUP = 0x0202;
    public const uint WM_CLOSE = 0x0010;
    public const uint BM_CLICK = 0x00F5;

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

    public static IntPtr MakeLParam(int loWord, int hiWord) {
        return (IntPtr)((hiWord << 16) | (loWord & 0xFFFF));
    }

    public static void ClickWindowClient(IntPtr hWnd, int clientX, int clientY) {
        IntPtr lp = MakeLParam(clientX, clientY);
        PostMessage(hWnd, WM_LBUTTONDOWN, (IntPtr)1, lp);
        System.Threading.Thread.Sleep(60);
        PostMessage(hWnd, WM_LBUTTONUP, IntPtr.Zero, lp);
    }

    public static void ClickScreen(int x, int y) {
        SetCursorPos(x, y);
        System.Threading.Thread.Sleep(60);
        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, UIntPtr.Zero);
        System.Threading.Thread.Sleep(60);
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, UIntPtr.Zero);
    }
}
"@

Add-Type -TypeDefinition $code -Language CSharp
[void][Win32Cap]::SetProcessDPIAware()

$outDir = "d:\Coding\CypeClon\rea_evidence\screenshots"
# Clean old probe files
Get-ChildItem -Path $outDir -Filter "probe_*.png" | Remove-Item -Force -ErrorAction SilentlyContinue
Get-ChildItem -Path $outDir -Filter "00_probe.png" | Remove-Item -Force -ErrorAction SilentlyContinue

function Get-CypeWindows([uint32]$targetPid) {
    $procs = Get-Process | Where-Object { $_.ProcessName -match "CYPE|m3d|metal|entorno" -or $_.Id -eq $targetPid }
    $pids = @($procs | Select-Object -ExpandProperty Id)
    $list = New-Object System.Collections.ArrayList
    $cb = [Win32Cap+EnumWindowsProc]{
        param($hWnd, $lParam)
        if ([Win32Cap]::IsWindowVisible($hWnd)) {
            $wpid = [uint32]0
            [void][Win32Cap]::GetWindowThreadProcessId($hWnd, [ref]$wpid)
            $title = [Win32Cap]::GetTitle($hWnd)
            $cls = [Win32Cap]::GetClass($hWnd)
            if (($pids -contains $wpid) -or ($cls -match "Cype") -or ($title -match "CYPE|Obra|Generador|Datos")) {
                $rect = New-Object Win32Cap+RECT
                [void][Win32Cap]::GetWindowRect($hWnd, [ref]$rect)
                $w = $rect.Right - $rect.Left
                $h = $rect.Bottom - $rect.Top
                if ($w -gt 80 -and $h -gt 80 -and $cls -notmatch "UAC") {
                    [void]$list.Add([PSCustomObject]@{
                        HWND = "0x{0:X}" -f $hWnd.ToInt64()
                        Handle = $hWnd
                        PID = $wpid
                        Title = $title
                        Class = $cls
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
    [void][Win32Cap]::EnumWindows($cb, [IntPtr]::Zero)
    return ,$list.ToArray()
}

function Get-ChildControls([IntPtr]$parentHwnd) {
    $children = New-Object System.Collections.ArrayList
    $cb = [Win32Cap+EnumWindowsProc]{
        param($hWnd, $lParam)
        if ([Win32Cap]::IsWindowVisible($hWnd)) {
            $rect = New-Object Win32Cap+RECT
            [void][Win32Cap]::GetWindowRect($hWnd, [ref]$rect)
            $w = $rect.Right - $rect.Left
            $h = $rect.Bottom - $rect.Top
            $title = [Win32Cap]::GetTitle($hWnd)
            $cls = [Win32Cap]::GetClass($hWnd)
            [void]$children.Add([PSCustomObject]@{
                HWND = "0x{0:X}" -f $hWnd.ToInt64()
                Handle = $hWnd
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
    [void][Win32Cap]::EnumChildWindows($parentHwnd, $cb, [IntPtr]::Zero)
    return ,$children.ToArray()
}

function Save-WindowCapture([PSCustomObject]$win, [string]$fileName) {
    $bmp = New-Object System.Drawing.Bitmap($win.Width, $win.Height)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    $ok = [Win32Cap]::PrintWindow($win.Handle, $hdc, 2)
    if (-not $ok) {
        $ok = [Win32Cap]::PrintWindow($win.Handle, $hdc, 0)
    }
    $g.ReleaseHdc($hdc)
    $g.Dispose()
    $fullPath = Join-Path $outDir $fileName
    $bmp.Save($fullPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Output "Captured [$($win.Title)] ($($win.Width)x$($win.Height)) -> $fileName"
}

function Save-CompositeCapture([array]$wins, [string]$fileName) {
    # Find main window (largest)
    $main = $wins | Sort-Object { $_.Width * $_.Height } -Descending | Select-Object -First 1
    if (-not $main) { return }
    $bmp = New-Object System.Drawing.Bitmap($main.Width, $main.Height)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    [void][Win32Cap]::PrintWindow($main.Handle, $hdc, 2)
    $g.ReleaseHdc($hdc)

    # Draw any smaller dialogs on top at their relative position
    foreach ($w in $wins) {
        if ($w.HWND -ne $main.HWND) {
            $subBmp = New-Object System.Drawing.Bitmap($w.Width, $w.Height)
            $subG = [System.Drawing.Graphics]::FromImage($subBmp)
            $subHdc = $subG.GetHdc()
            [void][Win32Cap]::PrintWindow($w.Handle, $subHdc, 2)
            $subG.ReleaseHdc($subHdc)
            $subG.Dispose()
            $relX = $w.Left - $main.Left
            $relY = $w.Top - $main.Top
            $g.DrawImage($subBmp, $relX, $relY)
            $subBmp.Dispose()
        }
    }
    $g.Dispose()
    $fullPath = Join-Path $outDir $fileName
    $bmp.Save($fullPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Output "Saved composite capture -> $fileName"
}

$exePath = "D:\CYPE Ingenieros\Versi$([char]0xF3)n 2027\CYPE 3D\CYPE 3D.exe"
$proc = Start-Process -FilePath $exePath -WorkingDirectory (Split-Path $exePath) -PassThru
Write-Output "Launched CYPE 3D PID=$($proc.Id)"

# Wait for main startup window
Start-Sleep -Seconds 5
$wins = Get-CypeWindows $proc.Id
Write-Output "=== STEP 1: STARTUP WINDOW ==="
foreach ($w in $wins) {
    Write-Output ("TOP WIN: {0} [{1}] Class={2} Rect=({3},{4} {5}x{6})" -f $w.HWND, $w.Title, $w.Class, $w.Left, $w.Top, $w.Width, $w.Height)
}
if ($wins.Count -gt 0) {
    $mainWin = $wins | Sort-Object { $_.Width * $_.Height } -Descending | Select-Object -First 1
    Save-WindowCapture $mainWin "01_startup_welcome_screen.png"
    $children = Get-ChildControls $mainWin.Handle
    Write-Output "Startup window children ($($children.Count)):"
    foreach ($c in $children) {
        if ($c.Title -ne "" -or $c.Class -notmatch "Static") {
            Write-Output ("  CHILD: {0} [{1}] Class={2} Rect=({3},{4} {5}x{6}) Center=({7},{8})" -f $c.HWND, $c.Title, $c.Class, $c.Left, $c.Top, $c.Width, $c.Height, $c.CenterX, $c.CenterY)
        }
    }
    # Find child near (320, 455) (Nuevo...)
    $nuevoCtrl = $children | Where-Object { $_.Left -le 330 -and ($_.Left + $_.Width) -ge 310 -and $_.Top -le 465 -and ($_.Top + $_.Height) -ge 445 }
    Write-Output "Controls overlapping (320, 455) [Nuevo...]:"
    foreach ($nc in $nuevoCtrl) {
        Write-Output ("  HIT: {0} [{1}] Class={2} Rect=({3},{4} {5}x{6})" -f $nc.HWND, $nc.Title, $nc.Class, $nc.Left, $nc.Top, $nc.Width, $nc.Height)
    }
}

Write-Output "=== STEP 2: CLICKING NUEVO... ==="
[void][Win32Cap]::SetForegroundWindow($mainWin.Handle)
$tbNuevo = $children | Where-Object { $_.Class -eq "ToolbarWindow32" -and $_.Left -eq 562 -and $_.Top -eq 449 } | Select-Object -First 1
if ($tbNuevo) {
    Write-Output "Clicking Nuevo... Toolbar $($tbNuevo.HWND) at screen ($($tbNuevo.CenterX), $($tbNuevo.CenterY)) and client (20, 20)..."
    [Win32Cap]::ClickScreen($tbNuevo.CenterX, $tbNuevo.CenterY)
    [Win32Cap]::ClickWindowClient($tbNuevo.Handle, 20, 20)
}
Start-Sleep -Seconds 3

$wins = Get-CypeWindows $proc.Id
Write-Output "Windows after clicking Nuevo... ($($wins.Count)):"
foreach ($w in $wins) {
    Write-Output ("TOP WIN: {0} [{1}] Class={2} Rect=({3},{4} {5}x{6})" -f $w.HWND, $w.Title, $w.Class, $w.Left, $w.Top, $w.Width, $w.Height)
}
if ($wins.Count -gt 1) {
    $dlg = $wins | Where-Object { $_.HWND -ne $mainWin.HWND } | Select-Object -First 1
    Save-WindowCapture $dlg "02_dialog_nueva_obra.png"
    Save-CompositeCapture $wins "02_composite_nueva_obra.png"
    $dlgCh = Get-ChildControls $dlg.Handle
    $btnAceptar = $dlgCh | Where-Object { $_.Title -match "Aceptar" } | Select-Object -First 1
    if ($btnAceptar) {
        Write-Output "=== STEP 3: CLICKING ACEPTAR IN NUEVA OBRA ==="
        $cx = [int]($btnAceptar.Width / 2)
        $cy = [int]($btnAceptar.Height / 2)
        [Win32Cap]::ClickWindowClient($btnAceptar.Handle, $cx, $cy)
        [void][Win32Cap]::PostMessage($btnAceptar.Handle, [Win32Cap]::BM_CLICK, [IntPtr]::Zero, [IntPtr]::Zero)
        Start-Sleep -Seconds 3
        $wins2 = Get-CypeWindows $proc.Id
        Write-Output "Windows after clicking Aceptar ($($wins2.Count)):"
        foreach ($w in $wins2) {
            Write-Output ("TOP WIN: {0} [{1}] Class={2} Rect=({3},{4} {5}x{6})" -f $w.HWND, $w.Title, $w.Class, $w.Left, $w.Top, $w.Width, $w.Height)
            $ch2 = Get-ChildControls $w.Handle
            foreach ($c in $ch2) {
                if ($c.Title -ne "" -or $c.Class -notmatch "Static") {
                    Write-Output ("  CHILD of {0}: {1} [{2}] Class={3} Rect=({4},{5} {6}x{7}) Center=({8},{9})" -f $w.HWND, $c.HWND, $c.Title, $c.Class, $c.Left, $c.Top, $c.Width, $c.Height, $c.CenterX, $c.CenterY)
                }
            }
        }
        Save-CompositeCapture $wins2 "03_after_nueva_obra_aceptar.png"
        $smallDlg = $wins2 | Where-Object { $_.HWND -ne $mainWin.HWND } | Sort-Object { $_.Width * $_.Height } | Select-Object -First 1
        if ($smallDlg -and $smallDlg.Width -lt 500) {
            $smCh = Get-ChildControls $smallDlg.Handle
            $btnSi = $smCh | Where-Object { $_.Class -eq "Button" -and ($_.Title -match "S|Aceptar") } | Select-Object -First 1
            if ($btnSi) {
                Write-Output "Clicking overwrite confirmation [$($btnSi.Title)]..."
                [Win32Cap]::ClickWindowClient($btnSi.Handle, [int]($btnSi.Width / 2), [int]($btnSi.Height / 2))
                [void][Win32Cap]::PostMessage($btnSi.Handle, [Win32Cap]::BM_CLICK, [IntPtr]::Zero, [IntPtr]::Zero)
                Start-Sleep -Seconds 3
            }
        }
        $wins3 = Get-CypeWindows $proc.Id
        Write-Output "Windows at Wizard stage ($($wins3.Count)):"
        foreach ($w in $wins3) {
            Write-Output ("TOP WIN: {0} [{1}] Class={2} Rect=({3},{4} {5}x{6})" -f $w.HWND, $w.Title, $w.Class, $w.Left, $w.Top, $w.Width, $w.Height)
        }
        Save-CompositeCapture $wins3 "03_wizard_nueva_obra.png"
        $wizDlg = $wins3 | Where-Object { $_.HWND -ne $mainWin.HWND } | Select-Object -First 1
        if ($wizDlg) {
            Save-WindowCapture $wizDlg "03_dialog_wizard_nueva_obra.png"
            $wizCh = Get-ChildControls $wizDlg.Handle
            $btnWizAceptar = $wizCh | Where-Object { $_.Class -eq "Button" -and $_.Title -match "Aceptar" } | Select-Object -First 1
            if ($btnWizAceptar) {
                Write-Output "=== STEP 4: CLICKING ACEPTAR IN WIZARD NUEVA OBRA ==="
                [Win32Cap]::ClickWindowClient($btnWizAceptar.Handle, [int]($btnWizAceptar.Width / 2), [int]($btnWizAceptar.Height / 2))
                [void][Win32Cap]::PostMessage($btnWizAceptar.Handle, [Win32Cap]::BM_CLICK, [IntPtr]::Zero, [IntPtr]::Zero)
                Start-Sleep -Seconds 6
                $wins4 = Get-CypeWindows $proc.Id
                Write-Output "Windows after Wizard Aceptar ($($wins4.Count)):"
                foreach ($w in $wins4) {
                    Write-Output ("TOP WIN: {0} [{1}] Class={2} Rect=({3},{4} {5}x{6})" -f $w.HWND, $w.Title, $w.Class, $w.Left, $w.Top, $w.Width, $w.Height)
                    $ch4 = Get-ChildControls $w.Handle
                    foreach ($c in $ch4) {
                        if ($c.Title -ne "" -or $c.Class -notmatch "Static") {
                            Write-Output ("  CHILD of {0}: {1} [{2}] Class={3} Rect=({4},{5} {6}x{7}) Center=({8},{9})" -f $w.HWND, $c.HWND, $c.Title, $c.Class, $c.Left, $c.Top, $c.Width, $c.Height, $c.CenterX, $c.CenterY)
                        }
                    }
                }
                Save-CompositeCapture $wins4 "04_after_wizard_aceptar.png"
                $dlg4 = $wins4 | Where-Object { $_.HWND -ne $mainWin.HWND } | Select-Object -First 1
                if ($dlg4) {
                    Save-WindowCapture $dlg4 "04_dialog_step4.png"
                    $ch4 = Get-ChildControls $dlg4.Handle
                    $btn4Aceptar = $ch4 | Where-Object { $_.Class -eq "Button" -and ($_.Title -match "Aceptar|Terminar|Finalizar") } | Select-Object -First 1
                    if ($btn4Aceptar) {
                        Write-Output "=== STEP 5: CLICKING [$($btn4Aceptar.Title)] IN STEP 4 DIALOG ==="
                        [Win32Cap]::ClickWindowClient($btn4Aceptar.Handle, [int]($btn4Aceptar.Width / 2), [int]($btn4Aceptar.Height / 2))
                        [void][Win32Cap]::PostMessage($btn4Aceptar.Handle, [Win32Cap]::BM_CLICK, [IntPtr]::Zero, [IntPtr]::Zero)
                        Start-Sleep -Seconds 4
                        $wins5 = Get-CypeWindows $proc.Id
                        Write-Output "Windows after Step 5 ($($wins5.Count)):"
                        foreach ($w in $wins5) {
                            Write-Output ("TOP WIN: {0} [{1}] Class={2} Rect=({3},{4} {5}x{6})" -f $w.HWND, $w.Title, $w.Class, $w.Left, $w.Top, $w.Width, $w.Height)
                            $ch5 = Get-ChildControls $w.Handle
                            foreach ($c in $ch5) {
                                if ($c.Title -ne "" -or $c.Class -notmatch "Static") {
                                    Write-Output ("  CHILD of {0}: {1} [{2}] Class={3} Rect=({4},{5} {6}x{7}) Center=({8},{9})" -f $w.HWND, $c.HWND, $c.Title, $c.Class, $c.Left, $c.Top, $c.Width, $c.Height, $c.CenterX, $c.CenterY)
                                }
                            }
                        }
                        Save-CompositeCapture $wins5 "05_after_step5.png"
                        $dlg5 = $wins5 | Where-Object { $_.HWND -ne $mainWin.HWND } | Select-Object -First 1
                        if ($dlg5) {
                            Save-WindowCapture $dlg5 "05_dialog_step5.png"
                            $ch5 = Get-ChildControls $dlg5.Handle
                            $btn5Aceptar = $ch5 | Where-Object { $_.Class -eq "Button" -and ($_.Title -match "Aceptar|Terminar|Finalizar") } | Select-Object -First 1
                            if ($btn5Aceptar) {
                                Write-Output "=== STEP 6: CLICKING [$($btn5Aceptar.Title)] IN STEP 5 DIALOG ==="
                                [Win32Cap]::ClickWindowClient($btn5Aceptar.Handle, [int]($btn5Aceptar.Width / 2), [int]($btn5Aceptar.Height / 2))
                                [void][Win32Cap]::PostMessage($btn5Aceptar.Handle, [Win32Cap]::BM_CLICK, [IntPtr]::Zero, [IntPtr]::Zero)
                                Start-Sleep -Seconds 5
                                $wins6 = Get-CypeWindows $proc.Id
                                Write-Output "Windows after Step 6 ($($wins6.Count)):"
                                foreach ($w in $wins6) {
                                    Write-Output ("TOP WIN: {0} [{1}] Class={2} Rect=({3},{4} {5}x{6})" -f $w.HWND, $w.Title, $w.Class, $w.Left, $w.Top, $w.Width, $w.Height)
                                    $ch6 = Get-ChildControls $w.Handle
                                    foreach ($c in $ch6) {
                                        if ($c.Title -ne "" -or $c.Class -notmatch "Static") {
                                            Write-Output ("  CHILD of {0}: {1} [{2}] Class={3} Rect=({4},{5} {6}x{7}) Center=({8},{9})" -f $w.HWND, $c.HWND, $c.Title, $c.Class, $c.Left, $c.Top, $c.Width, $c.Height, $c.CenterX, $c.CenterY)
                                        }
                                    }
                                }
                                Save-CompositeCapture $wins6 "06_after_step6.png"
                            }
                        }
                    }
                }
            }
        }
    }
}

Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
Write-Output "Done."
