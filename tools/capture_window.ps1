# tools/capture_window.ps1 -- screenshot the running reimpl-x64 window.
#
# Layout defects (a squashed icon, a collapsed row) cannot be read from a log, and an
# unattended run has nobody to press PrintScreen. Pair this with
# BANISHED_PANEL_HOLD=<ComponentTypeName>, which opens one building's info panel and
# leaves it up (standalone/panel_hold_harness.cpp).
#
# Usage:  powershell -ExecutionPolicy Bypass -File tools/capture_window.ps1 -Out shot.png
#
# Captures the screen region the window occupies rather than asking the window to redraw
# into a bitmap: PrintWindow on a DirectX swap-chain window usually comes back black,
# because the game's frame lives in the GPU back buffer and never touches the GDI DC.
# That means the window must be visible and unobscured -- it is brought to the front
# first, and a foreground-window check reports honestly if that failed.

param(
    [string]$Out     = "capture.png",
    [string]$Process = "reimpl-x64"
)

Add-Type -AssemblyName System.Drawing

$src = @"
using System;
using System.Runtime.InteropServices;
public class Win {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, IntPtr pid);
    [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool f);
    [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
}
"@
if (-not ("Win" -as [type])) { Add-Type -TypeDefinition $src }

$p = Get-Process -Name $Process -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $p) { Write-Error "$Process is not running"; exit 1 }
$h = $p.MainWindowHandle
if ($h -eq [IntPtr]::Zero) { Write-Error "no main window handle (still booting?)"; exit 1 }

# Windows refuses SetForegroundWindow to a process that does not own the foreground.
# The supported way around it is to attach our input queue to the foreground window's
# thread for the duration of the call, which makes us count as the same input context.
[void][Win]::ShowWindow($h, 9)          # SW_RESTORE
$fg = [Win]::GetForegroundWindow()
$tidFg = [Win]::GetWindowThreadProcessId($fg, [IntPtr]::Zero)
$tidMe = [Win]::GetCurrentThreadId()
$attached = $false
if ($tidFg -ne 0 -and $tidFg -ne $tidMe) {
    $attached = [Win]::AttachThreadInput($tidMe, $tidFg, $true)
}
[void][Win]::BringWindowToTop($h)
[void][Win]::SetForegroundWindow($h)
if ($attached) { [void][Win]::AttachThreadInput($tidMe, $tidFg, $false) }
Start-Sleep -Milliseconds 900           # let it repaint at the front

if ([Win]::GetForegroundWindow() -ne $h) {
    Write-Error ("could not bring the $Process window to the foreground -- NOT capturing, " +
                 "because the image would silently be of whatever is on top of it. " +
                 "Click the game window, then re-run this script.")
    exit 2
}

$r = New-Object Win+RECT
if (-not [Win]::GetClientRect($h, [ref]$r)) { Write-Error "GetClientRect failed"; exit 1 }
$w = $r.R - $r.L
$ht = $r.B - $r.T
if ($w -le 0 -or $ht -le 0) { Write-Error "client rect is empty ($w x $ht)"; exit 1 }

$tl = New-Object Win+POINT
$tl.X = 0; $tl.Y = 0
[void][Win]::ClientToScreen($h, [ref]$tl)

$bmp = New-Object System.Drawing.Bitmap $w, $ht
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($tl.X, $tl.Y, 0, 0, (New-Object System.Drawing.Size $w, $ht))
$g.Dispose()
# Sanity check before saving: a capture that is one flat colour means the window was
# not actually drawn (DX back buffer not presented, or an occluding window). Sampling a
# handful of spread-out pixels is enough to tell "a rendered frame" from "a blank rect",
# and catching it here beats reasoning about a screenshot that shows the wrong thing.
$distinct = @{}
foreach ($fx in 0.15, 0.4, 0.6, 0.85) {
    foreach ($fy in 0.15, 0.4, 0.6, 0.85) {
        $px = $bmp.GetPixel([int]($w * $fx), [int]($ht * $fy))
        $distinct[$px.ToArgb()] = $true
    }
}
if ($distinct.Count -le 2) {
    $bmp.Dispose()
    Write-Error ("capture looks blank ($($distinct.Count) distinct sample colours) -- " +
                 "not saving. The window is probably occluded or not presenting.")
    exit 3
}

$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Output "saved $Out ($w x $ht, $($distinct.Count) distinct sample colours) from pid $($p.Id)"
