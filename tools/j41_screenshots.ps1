# Stage J4.1 (DECISIONS.md T-J4.1-1): makes the pictures of the README (docs\images\*.png) with the program's own screenshot options, so that they are the program's real
# output and can be made again. Needs the built program (tools\build_host.cmd) and the game file copy (rom\pacman.a26). It writes into build\j41_shots\<time> (ignored by
# Git), then copies the five chosen pictures into docs\images (an existing picture of the same name is replaced).
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\j41_screenshots.ps1
# The program is used with a scratch settings file, so the owner's window and choices are not touched. Windows may be in HDR mode: the standard-colour (sdr) pictures are
# the ones used, because those are what almost every viewer sees.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root 'build\host\pacman2600.exe'
$rom = Join-Path $root 'rom\pacman.a26'
foreach ($f in @($exe, $rom)) { if (-not (Test-Path $f)) { throw "missing $f" } }
$work = Join-Path $root ('build\j41_shots\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force -Path $work | Out-Null

function Run([string]$argText) {
  $p = Start-Process -Wait -PassThru -FilePath $exe -ArgumentList $argText -WorkingDirectory $root
  if ($p.ExitCode -ne 0) { throw "the program ended with exit code $($p.ExitCode): $argText" }
}
function Q([string]$path) { return '"' + $path + '"' }   # paths hold spaces: quote them

Run ("--crt-shot " + (Q "$work\crt") + " " + (Q $rom) + " --crt lottes --window 1280x960 --settings " + (Q "$work\a.ini"))
Run ("--menu-shot " + (Q "$work\menu.png") + " " + (Q $rom) + " --settings " + (Q "$work\b.ini"))
Run ("--smooth-shot " + (Q "$work\smooth") + " " + (Q $rom) + " --window 1280x960 --settings " + (Q "$work\c.ini"))

# What each report says must be OK before anything is used.
foreach ($r in @("$work\menu.png.txt")) {
  if (-not (Select-String -Path $r -Pattern '^result=OK' -Quiet)) { throw "$r does not say result=OK" }
}

$out = Join-Path $root 'docs\images'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$pick = [ordered]@{
  'gameplay.png'              = 'smooth_s1_p0.png'          # the game in play, the exact original picture (progress 0 of a step), the default Framed look
  'crt-off.png'               = 'crt_sdr_off.png'           # the picture with the CRT filter off
  'crt-lottes.png'            = 'crt_sdr_lottes.png'        # the same picture through the CRT-Lottes filter
  'menu-game-options.png'     = 'menu.png'                  # the menu, Game options page
  'menu-graphics-options.png' = 'menu.png.graphics.png'     # the menu, Graphics options page (background, TV type, CRT filter, HDR)
}
foreach ($name in $pick.Keys) {
  $src = Join-Path $work $pick[$name]
  if (-not (Test-Path $src)) { throw "the program did not make $($pick[$name])" }
  Copy-Item -LiteralPath $src -Destination (Join-Path $out $name) -Force
  '{0,-28} {1,9:N0} bytes   from {2}' -f $name, (Get-Item (Join-Path $out $name)).Length, $pick[$name]
}
"result=OK  ($work)"
