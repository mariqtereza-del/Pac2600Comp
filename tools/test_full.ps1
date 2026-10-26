# The full test command (Stage H3): run it at gates and before a release. About 30 minutes. Needs the ROM and the local reference\ folder.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\test_full.ps1 [-SkipBuild] [-Slow]
#
# -SkipBuild  do not build first (default: both build scripts run first, which also proves the code compiles)
# -Slow       also run f18_kernel and f19_sound without --cached (about 15 more minutes)
# It runs everything of the quick command, then the long proofs (whole-game lockstep over 120 random games and the 70 recorded games,
# every picture, every sound, the key replay), all 70 recorded baseline games, and the host program checks of both programs
# (windows open briefly; sound is muted). Prints PASS/FAIL per step and 'result: OK' or 'result: FAILED'.
# Exit code 0 only if every step passed; 2 if it cannot run, or if the host checks need bash and it is not found.
param([switch]$SkipBuild, [switch]$Slow)
. (Join-Path $PSScriptRoot 'test_common.ps1')
Start-Tests 'full'
$rom = 'rom\pacman.a26'

if (-not $SkipBuild) {
  Step 'build_lockstep' 'cmd.exe' @('/c', 'tools\build_lockstep.cmd') -Must 'BUILD OK' -TimeoutSeconds 1800
  Step 'build_host' 'cmd.exe' @('/c', 'tools\build_host.cmd') -Must 'BUILD OK' -TimeoutSeconds 1800
}

Add-QuickSteps

if ($Slow) {
  Py 'f18_kernel (full)' 'f18_kernel.py' @($rom)
  Py 'f19_sound (full)' 'f19_sound.py' @($rom)
}
Py 'g3_state_layout' 'g3_state_layout.py' @($rom) -Must 'state_layout_checks'
foreach ($t in @('f12_collisions', 'f21_routines', 'f23_tia', 'f23_pictures', 'f23_collisions', 'f24_sound')) { Py $t ($t + '.py') @($rom) }
Py 'f25_games (120 random + 70 recorded)' 'f25_games.py' @($rom, '--games', '120')
Step 'd10_replay_compare' $script:Python @('-I', 'tools\d10_replay_compare.py', $rom) -Must 'TOTAL \d+ frames compared, 0 differences'

$ids = 1..70 | ForEach-Object { 'BL-{0:D3}' -f $_ }
Step 'baseline check, all 70 games' $script:Python (@('-I', 'tools\baseline.py', 'check', $rom) + $ids) -Must 'equals the stored SHA-256' -MustNot 'DIFFERS' -TimeoutSeconds 7200

$bash = $null
$git = Get-Command git -ErrorAction SilentlyContinue
if ($git) {
  $cand = Join-Path (Split-Path (Split-Path $git.Source)) 'usr\bin\bash.exe'
  if (Test-Path $cand) { $bash = $cand }
}
if ($bash) {
  $env:PATH = (Split-Path $bash) + ';' + $env:PATH   # bash started from PowerShell needs Git's own tools (mkdir, grep) on the path
  Step 'host checks, both programs' $bash @('tools/host_checks.sh', ((Scratch 'host') -replace '\\', '/')) -Must 'ALL DONE fail=0' -MustNot 'FAIL ' -TimeoutSeconds 1800
} else {
  Write-Host 'The host checks need bash (it comes with Git for Windows) and it was not found next to git.exe.'
  Write-Host 'result: INCOMPLETE (exit 2)'
  exit 2
}
Finish-Tests 'full'
