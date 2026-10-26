# Stage H6: performance and pacing. Plays the game in the real window for a while on the real monitor and on pretend
# monitors of other speeds (--present-hz), mute, and checks that the game itself does not depend on the monitor:
#   - game frames a second equal the console's 59.9227 at every monitor speed (within 0.1 %),
#   - the pictures (fingerprints) of the frames all runs share are identical,
#   - no sound underruns, 99 in 100 game frames start less than 2 ms late, computing a frame takes under 4 ms (99th percentile), and at most 5 frames a run take longer than a whole 16.7 ms.
# Also: how fast the game runs with no window (frames a second flat out) and the time to the first picture.
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\h6_performance.ps1 [-Seconds 60] [-Rom rom\pacman.a26] [-Out build\h6\<time>]
param(
  [int]$Seconds = 60,
  [string]$Rom = "rom\pacman.a26",
  [string]$Out = ""
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
if (-not $Out) { $Out = "build\h6\" + (Get-Date -Format "yyyyMMdd-HHmmss") }
if (Test-Path $Out) { Write-Host "Folder $Out already exists; choose another -Out."; exit 2 }
New-Item -ItemType Directory -Path $Out | Out-Null
$exe = "build\host\pacman2600.exe"
if (-not (Test-Path $exe) -or -not (Test-Path $Rom)) { Write-Host "Needs $exe and $Rom"; exit 2 }

$monitor = (Get-CimInstance Win32_VideoController | Where-Object { $_.CurrentRefreshRate } | Select-Object -First 1).CurrentRefreshRate
Write-Host "Real monitor refresh rate reported by Windows: $monitor Hz"

$console = 3579545.4545 / (228.0 * 262.0)
$failures = @()
$runs = @(
  @{ name = "real-monitor"; hz = 0 },
  @{ name = "pretend-30";  hz = 30 },
  @{ name = "pretend-60";  hz = 60 },
  @{ name = "pretend-75";  hz = 75 },
  @{ name = "pretend-144"; hz = 144 },
  @{ name = "pretend-240"; hz = 240 }
)
$results = @()
foreach ($r in $runs) {
  $ini = Join-Path $Out ($r.name + ".ini")
  $rep = Join-Path $Out ($r.name + ".timed.txt")
  $fp  = Join-Path $Out ($r.name + ".fingerprints.csv")
  $argList = @("--timed", "$Seconds", "`"$rep`"", "`"$Rom`"", "--mute", "--reset-at", "120", "--settings", "`"$ini`"", "--fingerprints", "`"$fp`"")
  if ($r.hz -gt 0) { $argList += @("--present-hz", "$($r.hz)") }
  Start-Process -FilePath $exe -ArgumentList $argList -Wait  # the program is a window program: wait for it to end
  $a = @{}
  Get-Content ($ini + ".audio.txt") | ForEach-Object { if ($_ -match '^([a-z_0-9]+)=(.*)$') { $a[$Matches[1]] = $Matches[2] } }
  $a.name = $r.name; $a.hz = $r.hz
  $results += $a
  $fps = [double]$a.game_frames_per_second
  Write-Host ("{0,-13} fps={1:N4} underruns={2} compute mean/p99/max ms={3:N3}/{4:N3}/{5:N3} late p99/max ms={6:N3}/{7:N3} first picture ms={8:N0} refreshes/frame 1..5+ = {9}/{10}/{11}/{12}/{13} never shown={14}" -f `
    $r.name, $fps, $a.underruns, [double]$a.frame_compute_ms_mean, [double]$a.frame_compute_ms_p99, [double]$a.frame_compute_ms_max, `
    [double]$a.frame_start_late_ms_p99, [double]$a.frame_start_late_ms_max, [double]$a.first_picture_ms, `
    $a.refreshes_per_game_frame_1, $a.refreshes_per_game_frame_2, $a.refreshes_per_game_frame_3, $a.refreshes_per_game_frame_4, $a.refreshes_per_game_frame_5_or_more, $a.game_frames_never_shown)
  if ([math]::Abs($fps - $console) / $console -gt 0.001) { $failures += "$($r.name): $fps frames a second, not $console" }
  if ([int64]$a.underruns -ne 0) { $failures += "$($r.name): sound underruns $($a.underruns)" }
  if ([double]$a.frame_start_late_ms_p99 -gt 2.0) { $failures += "$($r.name): frames start late (p99 $($a.frame_start_late_ms_p99) ms)" }
  # Rare spikes (wall clock, so Windows pausing the program counts too) are counted and reported, not failed (owner's decision, T-H6-1):
  # the typical frame must be cheap, and at most 5 frames in a run may take longer than a whole frame period (16.7 ms).
  if ([double]$a.frame_compute_ms_p99 -gt 4.0) { $failures += "$($r.name): 1 frame in 100 takes $($a.frame_compute_ms_p99) ms to compute" }
  if ([int]$a.frames_compute_over_16_7_ms -gt 5) { $failures += "$($r.name): $($a.frames_compute_over_16_7_ms) frames took longer than 16.7 ms to compute" }
  if ([int]$a.frames_compute_over_16_7_ms -gt 0) { Write-Host "  note: $($r.name) had $($a.frames_compute_over_16_7_ms) frame(s) over 16.7 ms (longest $($a.frame_compute_ms_max) ms)" }
}

# The pictures must not depend on the monitor: compare every run with the first over the frames both have.
$base = Get-Content (Join-Path $Out "real-monitor.fingerprints.csv")
foreach ($r in $runs | Select-Object -Skip 1) {
  $other = Get-Content (Join-Path $Out ($r.name + ".fingerprints.csv"))
  $n = [math]::Min($base.Count, $other.Count)
  $diff = 0
  for ($i = 0; $i -lt $n; $i++) { if ($base[$i] -ne $other[$i]) { $diff++ } }
  Write-Host ("fingerprints {0} vs real-monitor: {1} frames compared, {2} differences" -f $r.name, ($n - 1), $diff)
  if ($diff -ne 0 -or $n -lt 1000) { $failures += "$($r.name): $diff picture differences over $n lines" }
}

# No window: how fast the game runs flat out (the margin the real-time pace has) and the whole start-up of the program.
$png = Join-Path $Out "headless.png"
$hini = Join-Path $Out "headless.ini"
$t = Measure-Command { Start-Process -FilePath $exe -ArgumentList @("--headless", "`"$png`"", "`"$Rom`"", "--frames", "6000", "--settings", "`"$hini`"") -Wait -WindowStyle Hidden }
$flat = 6000 / $t.TotalSeconds
Write-Host ("headless: 6000 frames in {0:N2} s = {1:N0} frames a second flat out ({2:N0} times real time)" -f $t.TotalSeconds, $flat, ($flat / $console))
$t2 = Measure-Command { Start-Process -FilePath $exe -ArgumentList @("--headless", "`"$png`"", "`"$Rom`"", "--frames", "1", "--save-frame", "0", "--settings", "`"$hini`"") -Wait -WindowStyle Hidden }
Write-Host ("headless 1 frame, whole program start to exit: {0:N0} ms" -f $t2.TotalMilliseconds)

if ($failures.Count) { Write-Host "result: FAILED"; $failures | ForEach-Object { Write-Host "  $_" }; exit 1 }
Write-Host "result: OK"
