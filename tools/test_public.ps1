# The public test command (Stage H4): the tests that need NO ROM and NO reference folder, so anyone who clones the repository can run them.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\test_public.ps1 [-Build]
#
# -Build  build the test programs first with tools\build_lockstep.cmd (default: they must already be built)
# It runs: the structure test of the embedded tables, the unit tests of the game modules, eleven whole recorded games replayed from the
# tracked inputs and compared with the tracked picture and sound fingerprints (no cartridge, no ROM code in that program), the rule checks
# that do not need the ROM (conventions, Rule 7, no ROM in Git, architecture and test lists complete).
# Not in this set (they need the ROM or the reference traces): the lockstep proofs, the ROM check test, the comparison of the tables with the
# cartridge, the scan for cartridge code, the host program tests (they need SDL built; planned for H5). Prints PASS/FAIL per step and
# 'result: OK' or 'result: FAILED' (exit 0 / 1; 2 = Python missing).
param([switch]$Build)
. (Join-Path $PSScriptRoot 'test_common.ps1')
Start-Tests 'public' -NoRom
if ($Build) {
  Step 'build_lockstep' 'cmd.exe' @('/c', 'tools\build_lockstep.cmd') -Must 'BUILD OK' -TimeoutSeconds 1800
}
Add-PublicSteps
Finish-Tests 'public'
