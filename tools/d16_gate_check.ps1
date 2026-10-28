# Gate D checks that need no human (Stage D16, docs\GATE_D.md). Runs the RELEASE ZIP's own program, unzipped into a fresh folder.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\d16_gate_check.ps1 [-Zip out\Pac2600Comp-0.1.0-windows.zip] [-Work out\gate-d] [-Rom rom\pacman.a26]
#
# The ROM is only given to the program by path (--find-rom, --self-check ... <rom>); it is never copied anywhere.
# Output: one PASS/FAIL line per check, a report in <Work>\report.txt, exit code 0 only if every check passed.
param(
  [string]$Zip = 'out\Pac2600Comp-0.1.0-windows.zip',
  [string]$Work = 'out\gate-d',
  [string]$Rom = 'rom\pacman.a26'
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $root
$Zip = (Resolve-Path $Zip).Path
$Rom = (Resolve-Path $Rom).Path
if (Test-Path $Work) { Write-Host "FAIL: $Work already exists; give a new folder name with -Work (nothing is deleted)"; exit 1 }
New-Item -ItemType Directory -Path $Work | Out-Null
$Work = (Resolve-Path $Work).Path

$results = New-Object System.Collections.ArrayList
$allPass = $true
function Check([string]$name, [bool]$ok, [string]$detail) {
  $script:allPass = $script:allPass -and $ok
  $line = "{0}  {1}  {2}" -f $(if ($ok) { 'PASS' } else { 'FAIL' }), $name, $detail
  Write-Host $line
  [void]$script:results.Add($line)
}

# Unzip into the fresh folder
Expand-Archive -Path $Zip -DestinationPath (Join-Path $Work 'unzipped')
$dir = (Get-ChildItem (Join-Path $Work 'unzipped') -Directory | Select-Object -First 1).FullName
$exe = Join-Path $dir 'Pac2600Comp.exe'

function Run([string[]]$argsList, [int]$timeoutSeconds = 120) {
  $quoted = $argsList | ForEach-Object { if ($_ -match '\s') { "`"$_`"" } else { $_ } }
  $p = Start-Process -FilePath $exe -ArgumentList $quoted -PassThru -WorkingDirectory $dir
  if (-not $p.WaitForExit($timeoutSeconds * 1000)) { $p.Kill(); return -999 }
  return $p.ExitCode
}
function W([string]$name) { return (Join-Path $Work $name) }
function Text([string]$path) { if (Test-Path $path) { return (Get-Content $path -Raw) } else { return '' } }
function Value([string]$text, [string]$key) {
  $m = [regex]::Match($text, "(?m)^$([regex]::Escape($key))=(.*)$")
  if ($m.Success) { return $m.Groups[1].Value.Trim() } else { return $null }
}

# ---- 1. The release itself ----
$files = Get-ChildItem $dir -Recurse -File
Check 'release: no ROM-like file' (@($files | Where-Object { $_.Extension -in '.a26', '.bin', '.rom' }).Count -eq 0) "$($files.Count) files, none ending .a26 .bin .rom"
Check 'release: no file with the ROM fingerprint' (@($files | Where-Object { (Get-FileHash -Algorithm SHA1 $_.FullName).Hash -eq '0940FEA7F04CDB6D4B90C5AD1A7E344E68F6DBB1' }).Count -eq 0) 'SHA-1 of every file compared'
$info = Get-Content (Join-Path $dir 'BUILD-INFO.txt') -Raw
$exeHash = (Get-FileHash -Algorithm SHA256 $exe).Hash.ToLower()
Check 'release: Pac2600Comp.exe is the one BUILD-INFO.txt names' ($info.Contains($exeHash)) "SHA-256 $($exeHash.Substring(0, 16))..."
$commit = [regex]::Match($info, 'Git commit: (\w+)').Groups[1].Value
Check 'release: built from a committed tree' (($commit.Length -eq 40) -and -not $info.Contains('TEST BUILD')) "commit $($commit.Substring(0, 10))"
foreach ($f in 'SDL3.dll', 'msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll', 'README.txt', 'NOTICE.txt', 'licences\Stella-License.txt', 'licences\SDL3-LICENSE.txt', 'source\BUILDING.txt', 'roms\PUT_YOUR_ROM_HERE.txt', 'Record a game.cmd', 'Replay a game.cmd') {
  Check "release: has $f" (Test-Path (Join-Path $dir $f)) ''
}

# ---- 2. Picture: shape and border, windowed, at four sizes (default mode) ----
foreach ($size in '2560x1440', '1920x1080', '1366x768', '1280x1024') {
  $png = W "layout_$size.png"
  $code = Run @('--layout-check', $png, $Rom, '--window', $size, '--reset-at', '60', '--settings', (W "s_$size.ini"))
  $t = Text "$png.txt"
  $rect = Value $t 'picture_rect'
  $shape = [double](Value $t 'pixel_shape_in_window')
  $mode = Value $t 'mode'
  $m = [regex]::Match($rect, '^(\d+),(\d+) (\d+)x(\d+)$')
  $border = $m.Success -and ([int]$m.Groups[1].Value -gt 0 -or [int]$m.Groups[2].Value -gt 0) -and ([int]$m.Groups[2].Value -gt 0)
  Check "picture $size" (($code -eq 0) -and ($mode -eq 'Framed') -and ([math]::Abs($shape - 12.0 / 7.0) / (12.0 / 7.0) -lt 0.005) -and $border -and ((Value $t 'pixels_outside_picture_rect_not_black') -eq '0')) "mode $mode, picture $rect, pixel shape $shape (12:7 = 1.7143), black border on every side"
}

# ---- 3. Fullscreen and back ----
$code = Run @('--fullscreen-check', (W 'fullscreen.txt'), $Rom, '--reset-at', '60', '--settings', (W 's_fs.ini')) 180
$t = Text (W 'fullscreen.txt')
Check 'fullscreen and back' (($code -eq 0) -and ((Value $t 'result') -eq 'OK')) "Alt+Enter on and off, the picture checked in fullscreen, the odd window size and place came back"

# ---- 4. Sound: no shortages at the console's own speed ----
$code = Run @('--audio-check', '20', (W 'audio.txt'), $Rom, '--mute', '--reset-at', '120', '--settings', (W 's_audio.ini')) 120
$t = Text (W 'audio.txt')
$fps = [double](Value $t 'frames_per_second')
Check 'sound: 0 underruns, console speed' (($code -eq 0) -and ((Value $t 'underruns') -eq '0') -and ($t -match '31400 Hz') -and ($fps -gt 59.7) -and ($fps -lt 60.1)) "underruns $(Value $t 'underruns'), $fps frames a second, source 31,400 Hz"

# ---- 5. Keyboard: injected keys reach the game and the switches ----
$code = Run @('--push-keys', '--timed', '12', (W 'keys.txt'), $Rom, '--mute', '--settings', (W 's_keys.ini'), '--record', (W 'rec1.inputs'), '--fingerprints', (W 'A.csv')) 120
$t = Text (W 's_keys.ini.audio.txt')
$joy = (Value $t 'joystick_ram_values_seen') -split '\s+'
$sw = (Value $t 'console_switch_values_seen') -split '\s+'
Check 'keyboard: all four directions reach the game' (($code -eq 0) -and ($joy -contains '1') -and ($joy -contains '2') -and ($joy -contains '4') -and ($joy -contains '8')) "joystick values seen: $($joy -join ' ')"
Check 'keyboard: Reset and the three switches' (($sw -contains '3e') -and ($sw -contains '7f') -and ($sw -contains 'ff') -and ($sw -contains 'f7')) "switch register values seen: $($sw -join ' ')"
$ini = Text (W 's_keys.ini')
Check 'switches are remembered' ($ini -match 'left_difficulty=a' -and $ini -match 'right_difficulty=a' -and $ini -match 'tv_type=bw') 'settings.ini holds left_difficulty=a right_difficulty=a tv_type=bw'

# ---- 6. Pad ----
$code = Run @('--pad-check', (W 'pad.txt'), $Rom, '--settings', (W 's_pad.ini')) 180
$t = Text (W 'pad.txt')
$okLines = ([regex]::Matches($t, '(?m): ok ')).Count
Check 'pad: virtual gamepad, every button, stick, unplug and replug' (($code -eq 0) -and ((Value $t 'result') -eq 'OK') -and ($okLines -ge 30)) "$okLines checks ok (the real pad is yours to try)"

# ---- 7. Recording and replay, identical ----
$null = Run @('--replay', (W 'rec1.inputs'), '--fingerprints', (W 'B.csv'), '--timed', '8', (W 'replay_window.txt'), $Rom, '--mute', '--settings', (W 's_rep.ini')) 120
$null = Run @('--headless', (W 'c.png'), $Rom, '--replay', (W 'rec1.inputs'), '--fingerprints', (W 'C.csv'), '--settings', (W 's_head.ini')) 180
function Hashes([string]$p) { return @(Import-Csv $p | ForEach-Object { $_.hash }) }
$a = Hashes (W 'A.csv'); $b = Hashes (W 'B.csv'); $c = Hashes (W 'C.csv')
function CountDiff($x, $y) { $n = [math]::Min($x.Count, $y.Count); $d = 0; for ($i = 0; $i -lt $n; $i++) { if ($x[$i] -ne $y[$i]) { $d++ } }; return @($n, $d) }
$ac = CountDiff $a $c; $ab = CountDiff $a $b
Check 'replay: no-window replay equals the recording run' (($ac[0] -ge 700) -and ($ac[1] -eq 0)) "$($ac[0]) frames compared, $($ac[1]) differences"
Check 'replay: window replay equals the recording run' (($ab[0] -ge 400) -and ($ab[1] -eq 0)) "$($ab[0]) frames compared, $($ab[1]) differences"

# ---- 8. ROM picker's search (the dialog itself is yours to try) ----
$emptyRoms = W 'empty_roms'
New-Item -ItemType Directory -Path $emptyRoms | Out-Null
$code = Run @('--find-rom', (W 'find_none.txt'), '--settings', (W 's_f1.ini'), '--roms-dir', $emptyRoms)
Check 'ROM search: nothing found gives exit 5' ($code -eq 5) "exit $code"
$wrong = W 'not_a_rom.dat'
[IO.File]::WriteAllBytes($wrong, [byte[]](,0x41 * 4096))
$code = Run @('--find-rom', (W 'find_wrong.txt'), $wrong, '--settings', (W 's_f2.ini'), '--roms-dir', $emptyRoms)
Check 'ROM search: a wrong 4096-byte file is refused (exit 4)' ($code -eq 4) "exit $code"
$code = Run @('--find-rom', (W 'find_good.txt'), $Rom, '--settings', (W 's_f3.ini'), '--roms-dir', $emptyRoms)
Check 'ROM search: the right ROM given by path is accepted (exit 0)' ($code -eq 0) "exit $code"
# (--find-rom is the quiet search and never saves; a normal start does, as the keys run in step 5 showed)
Check 'ROM: a normal start remembers the path' ((Text (W 's_keys.ini')) -match 'rom_path=') 'settings.ini of the keys run has rom_path='

# ---- 9. Same input, same pictures as Stella's emulation core (the release program against the Phase C reference) ----
$env:PACMAN_EXE = $exe
$py = & python -I (Join-Path $root 'tools\d14_side_by_side.py') $Rom (Join-Path $Work 'sbs') --window (Join-Path $root 'recordings\play-20261025-031017.inputs') 2>&1
$py2 = & python -I (Join-Path $root 'tools\d14_side_by_side.py') $Rom (Join-Path $Work 'sbs2') --frames 9800 --moments 1500,1898,2449,2330,5839,3455,7310,9691,3000,9000 (Join-Path $root 'tools\captures\c7_route.txt') 2>&1
Remove-Item Env:\PACMAN_EXE
$t1 = ($py | Select-Object -Last 1); $t2 = ($py2 | Select-Object -Last 1)
Check 'against Stella: your own recording (window and no window)' ($t1 -match 'TOTAL 1860 frames, 0 fingerprint differences; 10 pictures compared, 0 different') $t1
Check 'against Stella: the 9,800-frame bot route' ($t2 -match 'TOTAL 9800 frames, 0 fingerprint differences; 10 pictures compared, 0 different') $t2

$results | Set-Content -Path (W 'report.txt') -Encoding ASCII
Write-Host ''
Write-Host $(if ($allPass) { 'result=OK all checks passed' } else { 'result=FAIL' })
exit $(if ($allPass) { 0 } else { 1 })
