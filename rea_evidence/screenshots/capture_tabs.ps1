# Script para abrir la obra via Nuevo..., capturar todas las pestanas Ribbon de Estructura, Datos Generales, y la solapa Cimentacion
$ErrorActionPreference = "Continue"

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$code = @"
using System;
using System.Text;
using System.Runtime.InteropServices;
using System.Collections.Generic;

public class Win32Tabs2 {
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
    public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint nFlags);
    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

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
}
"@

Add-Type -TypeDefinition $code -Language CSharp
[void][Win32Tabs2]::SetProcessDPIAware()

$outDir = "d:\Coding\CypeClon\rea_evidence\screenshots"

function Get-CypeWindows([uint32]$targetPid) {
    $procs = Get-Process | Where-Object { $_.ProcessName -match "CYPE|m3d|metal|entorno" -or $_.Id -eq $targetPid }
    $pids = @($procs | Select-Object -ExpandProperty Id)
    $list = New-Object System.Collections.ArrayList
    $cb = [Win32Tabs2+EnumWindowsProc]{
        param($hWnd, $lParam)
        if ([Win32Tabs2]::IsWindowVisible($hWnd)) {
            $wpid = [uint32]0
            [void][Win32Tabs2]::GetWindowThreadProcessId($hWnd, [ref]$wpid)
            $title = [Win32Tabs2]::GetTitle($hWnd)
            $cls = [Win32Tabs2]::GetClass($hWnd)
            if (($pids -contains $wpid) -or ($cls -match "Cype")) {
                $rect = New-Object Win32Tabs2+RECT
                [void][Win32Tabs2]::GetWindowRect($hWnd, [ref]$rect)
                $w = $rect.Right - $rect.Left
                $h = $rect.Bottom - $rect.Top
                if ($w -gt 50 -and $h -gt 50 -and $cls -notmatch "UAC") {
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
    [void][Win32Tabs2]::EnumWindows($cb, [IntPtr]::Zero)
    return ,$list.ToArray()
}

function Get-ChildControls([IntPtr]$parentHwnd) {
    $children = New-Object System.Collections.ArrayList
    $cb = [Win32Tabs2+EnumWindowsProc]{
        param($hWnd, $lParam)
        if ([Win32Tabs2]::IsWindowVisible($hWnd)) {
            $rect = New-Object Win32Tabs2+RECT
            [void][Win32Tabs2]::GetWindowRect($hWnd, [ref]$rect)
            $w = $rect.Right - $rect.Left
            $h = $rect.Bottom - $rect.Top
            $title = [Win32Tabs2]::GetTitle($hWnd)
            $cls = [Win32Tabs2]::GetClass($hWnd)
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
    [void][Win32Tabs2]::EnumChildWindows($parentHwnd, $cb, [IntPtr]::Zero)
    return ,$children.ToArray()
}

function Save-CompositeCapture([array]$wins, [string]$fileName) {
    $main = $wins | Sort-Object { $_.Width * $_.Height } -Descending | Select-Object -First 1
    if (-not $main) { return }
    $bmp = New-Object System.Drawing.Bitmap($main.Width, $main.Height)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    [void][Win32Tabs2]::PrintWindow($main.Handle, $hdc, 2)
    $g.ReleaseHdc($hdc)

    $others = $wins | Where-Object { $_.HWND -ne $main.HWND } | Sort-Object { $_.Width * $_.Height } -Descending
    foreach ($w in $others) {
        $subBmp = New-Object System.Drawing.Bitmap($w.Width, $w.Height)
        $subG = [System.Drawing.Graphics]::FromImage($subBmp)
        $subHdc = $subG.GetHdc()
        [void][Win32Tabs2]::PrintWindow($w.Handle, $subHdc, 2)
        $subG.ReleaseHdc($subHdc)
        $subG.Dispose()
        $relX = $w.Left - $main.Left
        $relY = $w.Top - $main.Top
        $g.DrawImage($subBmp, $relX, $relY)
        $subBmp.Dispose()
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
Start-Sleep -Seconds 5

$wins = Get-CypeWindows $proc.Id
$mainWin = $wins | Sort-Object { $_.Width * $_.Height } -Descending | Select-Object -First 1
$children = Get-ChildControls $mainWin.Handle

# Step A: Click "Nuevo..." Toolbar at (562, 449)
$tbNuevo = $children | Where-Object { $_.Class -eq "ToolbarWindow32" -and $_.Left -eq 562 -and $_.Top -eq 449 } | Select-Object -First 1
[Win32Tabs2]::ClickWindowClient($tbNuevo.Handle, 20, 20)
Start-Sleep -Seconds 2

# Step B: Click "Aceptar" in Nueva obra dialog
$wins = Get-CypeWindows $proc.Id
$dlg1 = $wins | Where-Object { $_.HWND -ne $mainWin.HWND } | Select-Object -First 1
$ch1 = Get-ChildControls $dlg1.Handle
$btn1 = $ch1 | Where-Object { $_.Title -match "Aceptar" } | Select-Object -First 1
[Win32Tabs2]::ClickWindowClient($btn1.Handle, 35, 12)
[void][Win32Tabs2]::PostMessage($btn1.Handle, [Win32Tabs2]::BM_CLICK, [IntPtr]::Zero, [IntPtr]::Zero)
Start-Sleep -Seconds 2

# Step C: If "Pregunta" overwrite popup appears, click "Si"
$wins = Get-CypeWindows $proc.Id
$smallDlg = $wins | Where-Object { $_.HWND -ne $mainWin.HWND } | Sort-Object { $_.Width * $_.Height } | Select-Object -First 1
if ($smallDlg -and $smallDlg.Width -lt 500) {
    $smCh = Get-ChildControls $smallDlg.Handle
    $btnSi = $smCh | Where-Object { $_.Class -eq "Button" -and ($_.Title -match "S|Aceptar") } | Select-Object -First 1
    if ($btnSi) {
        [Win32Tabs2]::ClickWindowClient($btnSi.Handle, 35, 12)
        [void][Win32Tabs2]::PostMessage($btnSi.Handle, [Win32Tabs2]::BM_CLICK, [IntPtr]::Zero, [IntPtr]::Zero)
        Start-Sleep -Seconds 2
    }
}

# Step D: Click "Aceptar" in Wizard Nueva obra ("Obra vacia" / "Introduccion automatica IFC")
$wins = Get-CypeWindows $proc.Id
$wizDlg = $wins | Where-Object { $_.HWND -ne $mainWin.HWND } | Select-Object -First 1
$wizCh = Get-ChildControls $wizDlg.Handle
$btnWiz = $wizCh | Where-Object { $_.Class -eq "Button" -and $_.Title -match "Aceptar" } | Select-Object -First 1
[Win32Tabs2]::ClickWindowClient($btnWiz.Handle, 35, 12)
[void][Win32Tabs2]::PostMessage($btnWiz.Handle, [Win32Tabs2]::BM_CLICK, [IntPtr]::Zero, [IntPtr]::Zero)
Start-Sleep -Seconds 4

# Step E: Click "Terminar" in Obra vacia (Datos generales wizard)
$wins = Get-CypeWindows $proc.Id
$dlg4 = $wins | Where-Object { $_.HWND -ne $mainWin.HWND } | Select-Object -First 1
$ch4 = Get-ChildControls $dlg4.Handle
$btnTerm = $ch4 | Where-Object { $_.Class -eq "Button" -and ($_.Title -match "Terminar|Aceptar") } | Select-Object -First 1
[Win32Tabs2]::ClickWindowClient($btnTerm.Handle, 40, 12)
[void][Win32Tabs2]::PostMessage($btnTerm.Handle, [Win32Tabs2]::BM_CLICK, [IntPtr]::Zero, [IntPtr]::Zero)
Start-Sleep -Seconds 3

# Step F: Click "Aceptar" in Estados limite dialog
$wins = Get-CypeWindows $proc.Id
$dlg5 = $wins | Where-Object { $_.HWND -ne $mainWin.HWND } | Select-Object -First 1
if ($dlg5) {
    $ch5 = Get-ChildControls $dlg5.Handle
    $btn5 = $ch5 | Where-Object { $_.Class -eq "Button" -and $_.Title -match "Aceptar" } | Select-Object -First 1
    if ($btn5) {
        [Win32Tabs2]::ClickWindowClient($btn5.Handle, 35, 12)
        [void][Win32Tabs2]::PostMessage($btn5.Handle, [Win32Tabs2]::BM_CLICK, [IntPtr]::Zero, [IntPtr]::Zero)
        Start-Sleep -Seconds 4
    }
}

# Now we are in the main window (Solapa Estructura)!
$wins = Get-CypeWindows $proc.Id
$mainWin = $wins | Sort-Object { $_.Width * $_.Height } -Descending | Select-Object -First 1
$children = Get-ChildControls $mainWin.Handle
Write-Output "=== IN MAIN WINDOW: $($mainWin.Title), children=$($children.Count) ==="

# Inspect all controls with Top >= 980
Write-Output "=== BOTTOM BAR CONTROLS (Top >= 980) ==="
foreach ($c in ($children | Where-Object { $_.Top -ge 980 })) {
    Write-Output ("  BOTTOM: {0} [{1}] Class={2} Rect=({3},{4} {5}x{6})" -f $c.HWND, $c.Title, $c.Class, $c.Left, $c.Top, $c.Width, $c.Height)
}
# Also inspect all CypeMiniSolapas or SysTabControl32 in the window
Write-Output "=== TAB CONTROLS IN MAIN WINDOW ==="
foreach ($c in ($children | Where-Object { $_.Class -match "Solapa|Tab" })) {
    Write-Output ("  TABCTRL: {0} [{1}] Class={2} Rect=({3},{4} {5}x{6})" -f $c.HWND, $c.Title, $c.Class, $c.Left, $c.Top, $c.Width, $c.Height)
}

# Click each Ribbon tab at Top == 26
$ribbonTabs = @(
    @{ Name = "06_estructura_tab_obra.png"; Left = 47 },
    @{ Name = "07_estructura_tab_geometria.png"; Left = 97 },
    @{ Name = "08_estructura_tab_propiedades.png"; Left = 176 },
    @{ Name = "09_estructura_tab_carga.png"; Left = 265 },
    @{ Name = "10_estructura_tab_uniones.png"; Left = 320 },
    @{ Name = "11_estructura_tab_calculo.png"; Left = 387 },
    @{ Name = "12_estructura_tab_armaduras.png"; Left = 451 },
    @{ Name = "13_estructura_tab_ventana.png"; Left = 533 }
)

foreach ($rt in $ribbonTabs) {
    $children = Get-ChildControls $mainWin.Handle
    $tb = $children | Where-Object { $_.Class -eq "ToolbarWindow32" -and $_.Top -eq 26 -and [Math]::Abs($_.Left - $rt.Left) -le 8 } | Select-Object -First 1
    if ($tb) {
        Write-Output "Clicking Ribbon tab at Left=$($tb.Left) -> $($rt.Name)..."
        [Win32Tabs2]::ClickWindowClient($tb.Handle, [int]($tb.Width / 2), [int]($tb.Height / 2))
        Start-Sleep -Seconds 1
        $winsNow = Get-CypeWindows $proc.Id
        Save-CompositeCapture $winsNow $rt.Name
    }
}

# Switch back to Obra tab (Left=47) and click Datos Generales (Left=1, Top=51)
$children = Get-ChildControls $mainWin.Handle
$tbObra = $children | Where-Object { $_.Class -eq "ToolbarWindow32" -and $_.Top -eq 26 -and [Math]::Abs($_.Left - 47) -le 8 } | Select-Object -First 1
if ($tbObra) {
    [Win32Tabs2]::ClickWindowClient($tbObra.Handle, 25, 12)
    Start-Sleep -Seconds 1
}
$children = Get-ChildControls $mainWin.Handle
$tbDatosGen = $children | Where-Object { $_.Class -eq "ToolbarWindow32" -and $_.Top -eq 51 -and $_.Left -eq 1 } | Select-Object -First 1
if ($tbDatosGen) {
    Write-Output "Clicking Datos Generales button..."
    [Win32Tabs2]::ClickWindowClient($tbDatosGen.Handle, 28, 45)
    Start-Sleep -Seconds 2
    $winsDG = Get-CypeWindows $proc.Id
    Save-CompositeCapture $winsDG "14_dialog_datos_generales.png"
    $dlgDG = $winsDG | Where-Object { $_.HWND -ne $mainWin.HWND } | Select-Object -First 1
    if ($dlgDG) {
        $chDG = Get-ChildControls $dlgDG.Handle
        $btnCancel = $chDG | Where-Object { $_.Class -eq "Button" -and ($_.Title -match "Cancelar|Aceptar") } | Select-Object -Last 1
        if ($btnCancel) {
            [Win32Tabs2]::ClickWindowClient($btnCancel.Handle, 30, 12)
            [void][Win32Tabs2]::PostMessage($btnCancel.Handle, [Win32Tabs2]::BM_CLICK, [IntPtr]::Zero, [IntPtr]::Zero)
            Start-Sleep -Seconds 1
        }
    }
}

# Now click Cimentacion tab at the bottom (x=95, y=1002 in screen coords)
$children = Get-ChildControls $mainWin.Handle
$overCim = $children | Where-Object { $_.Left -le 95 -and ($_.Left + $_.Width) -ge 95 -and $_.Top -le 1002 -and ($_.Top + $_.Height) -ge 1002 }
Write-Output "Controls overlapping Cimentacion tab (95, 1002):"
foreach ($oc in $overCim) {
    $rx = 95 - $oc.Left
    $ry = 1002 - $oc.Top
    Write-Output ("  OVER CIM: {0} [{1}] Class={2} Rect=({3},{4} {5}x{6}) -> clicking client ({7},{8})" -f $oc.HWND, $oc.Title, $oc.Class, $oc.Left, $oc.Top, $oc.Width, $oc.Height, $rx, $ry)
    [Win32Tabs2]::ClickWindowClient($oc.Handle, $rx, $ry)
}
Start-Sleep -Seconds 3

$winsCim = Get-CypeWindows $proc.Id
Save-CompositeCapture $winsCim "15_cimentacion_main_window.png"
$childrenCim = Get-ChildControls $mainWin.Handle
Write-Output "=== CIMENTACION TOP CONTROLS (Top <= 165) ==="
foreach ($c in ($childrenCim | Where-Object { $_.Top -le 165 -and ($_.Title -ne "" -or $_.Class -notmatch "Static") })) {
    Write-Output ("  CIM CTRL: {0} [{1}] Class={2} Rect=({3},{4} {5}x{6})" -f $c.HWND, $c.Title, $c.Class, $c.Left, $c.Top, $c.Width, $c.Height)
}

Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
Write-Output "Done."
