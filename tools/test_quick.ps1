# The quick test command (Stage H3): run it after every stage. About three minutes. Needs the ROM and the local reference\ folder.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\test_quick.ps1
#
# It does not build anything: build first with tools\build_lockstep.cmd and tools\build_host.cmd if the code changed.
# It runs the unit tests, the rule checks, the short proofs against the original and five key game traces, and prints
# PASS/FAIL per step and 'result: OK' or 'result: FAILED' (exit code 0 only if every step passed; 2 if it cannot run).
# The full command (tools\test_full.ps1) adds everything else. Both write only to build\test_runs (a new folder every run).
. (Join-Path $PSScriptRoot 'test_common.ps1')
Start-Tests 'quick'
Add-QuickSteps
Finish-Tests 'quick'
