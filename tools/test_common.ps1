# Shared parts of tools\test_quick.ps1 and tools\test_full.ps1 (Stage H3). Not run on its own.
# Each step runs one test program or proof, records PASS/FAIL, the seconds and the last output line, and keeps the whole output in
# build\test_runs\<time stamp>\logs (a NEW folder every run; nothing is ever deleted).

$script:Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$script:Rom = Join-Path $script:Root 'rom\pacman.a26'
$script:Results = New-Object System.Collections.ArrayList
$script:Started = Get-Date
$script:RunDir = $null
$script:Python = $null

function Start-Tests([string]$Kind, [switch]$NoRom) {
  Set-Location $script:Root
  $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
  $script:RunDir = Join-Path $script:Root ("build\test_runs\" + $stamp + "-" + $Kind)
  New-Item -ItemType Directory -Path (Join-Path $script:RunDir 'logs') -Force | Out-Null
  New-Item -ItemType Directory -Path (Join-Path $script:RunDir 'scratch') -Force | Out-Null
  $py = Get-Command python -ErrorAction SilentlyContinue
  if ($py) { $script:Python = $py.Source }
  Write-Host ("Test run '" + $Kind + "' started " + $script:Started + "; logs in " + $script:RunDir)
  $missing = @()
  if ($NoRom) {
    # the public set: needs only Python and the built test programs, never the ROM or reference\
    if (-not $script:Python) { $missing += 'python' }
  } else {
  if (-not (Test-Path $script:Rom)) { $missing += 'rom\pacman.a26 (the working copy of the ROM)' }
  if (-not (Test-Path (Join-Path $script:Root 'reference\f6\coverage_all.txt'))) { $missing += 'reference\ (the local golden traces and maps)' }
  if (-not $script:Python) { $missing += 'python' }
  }
  if ($missing.Count -gt 0) {
    Write-Host 'CANNOT RUN. Missing on this PC:'
    $missing | ForEach-Object { Write-Host ('  ' + $_) }
    Write-Host 'result: NOT RUN (exit 2)'
    exit 2
  }
}

function Quote([string]$a) { if ($a -match '\s') { return '"' + $a + '"' } else { return $a } }

# Step <name> <program> <arguments array> [-Must regex] [-MustNot regex] [-TimeoutSeconds n]
function Step([string]$Name, [string]$Program, [string[]]$Arguments, [string]$Must = '', [string]$MustNot = '', [int]$TimeoutSeconds = 3600) {
  $sw = [Diagnostics.Stopwatch]::StartNew()
  $detail = ''
  $ok = $false
  try {
    if (-not (Test-Path $Program) -and -not (Get-Command $Program -ErrorAction SilentlyContinue)) {
      $detail = 'program not found: ' + $Program + ' (build it first: tools\build_lockstep.cmd and tools\build_host.cmd)'
    } else {
      $psi = New-Object Diagnostics.ProcessStartInfo
      $psi.FileName = $Program
      $psi.Arguments = (($Arguments | ForEach-Object { Quote $_ }) -join ' ')
      $psi.WorkingDirectory = $script:Root
      $psi.UseShellExecute = $false
      $psi.RedirectStandardOutput = $true
      $psi.RedirectStandardError = $true
      $psi.CreateNoWindow = $true
      $p = [Diagnostics.Process]::Start($psi)
      $outTask = $p.StandardOutput.ReadToEndAsync()
      $errTask = $p.StandardError.ReadToEndAsync()
      if (-not $p.WaitForExit($TimeoutSeconds * 1000)) {
        try { $p.Kill() } catch {}
        $detail = 'timed out after ' + $TimeoutSeconds + ' s'
      } else {
        $p.WaitForExit()
        $text = $outTask.Result + $errTask.Result
        Set-Content -Path (Join-Path $script:RunDir ('logs\' + $Name + '.log')) -Value $text -Encoding UTF8
        $lines = @($text -split "`r?`n" | Where-Object { $_.Trim() -ne '' })
        $detail = if ($lines.Count -gt 0) { $lines[$lines.Count - 1].Trim() } else { '(no output)' }
        if ($detail.Length -gt 110) { $detail = $detail.Substring(0, 110) }
        $ok = ($p.ExitCode -eq 0)
        if (-not $ok) { $detail = 'exit ' + $p.ExitCode + ': ' + $detail }
        if ($ok -and $Must -ne '' -and $text -notmatch $Must) { $ok = $false; $detail = 'expected text not found (' + $Must + '): ' + $detail }
        if ($ok -and $MustNot -ne '' -and $text -match $MustNot) { $ok = $false; $detail = 'forbidden text found (' + $MustNot + '): ' + $detail }
      }
    }
  } catch {
    $detail = 'error: ' + $_.Exception.Message
  }
  $sw.Stop()
  $secs = [int][Math]::Round($sw.Elapsed.TotalSeconds)
  $tag = if ($ok) { 'PASS' } else { 'FAIL' }
  Write-Host ('{0}  {1}  ({2} s)  {3}' -f $tag, $Name, $secs, $detail)
  [void]$script:Results.Add([pscustomobject]@{ Name = $Name; Ok = $ok; Seconds = $secs; Detail = $detail })
}

function Py([string]$Name, [string]$Tool, [string[]]$ToolArgs = @(), [string]$Must = 'result: OK', [string]$MustNot = '') {
  Step $Name $script:Python (@('-I', ('tools\' + $Tool)) + $ToolArgs) -Must $Must -MustNot $MustNot
}

function Scratch([string]$Name) {
  $d = Join-Path $script:RunDir ('scratch\' + $Name)
  New-Item -ItemType Directory -Path $d -Force | Out-Null
  return $d
}

# The steps of the quick set, also the first part of the full set.
function Add-QuickSteps {
  $L = Join-Path $script:Root 'build\lockstep'
  $H = Join-Path $script:Root 'build\host'
  $rom = 'rom\pacman.a26'
  # unit tests
  Step 'test_rom_check' "$L\test_rom_check.exe" @($rom)
  Step 'test_tables' "$L\test_tables.exe" @($rom)
  Step 'test_game_modules' "$L\test_game_modules.exe" @() -Must 'result: OK'
  Step 'test_no_rom BL-001' "$L\test_no_rom.exe" @('baseline\BL-001.inputs', 'tests\game\data\BL-001.fingerprints.txt') -Must 'IDENTICAL'
  Step 'test_no_rom BL-035' "$L\test_no_rom.exe" @('baseline\BL-035.inputs', 'tests\game\data\BL-035.fingerprints.txt') -Must 'IDENTICAL'
  Step 'test_layers BL-001' "$L\test_layers.exe" @('baseline\BL-001.inputs', 'tests\game\data\BL-001.fingerprints.txt') -Must 'result: OK'
  Step 'test_objects_view BL-001' "$L\test_objects_view.exe" @('baseline\BL-001.inputs', 'tests\game\data\BL-001.fingerprints.txt') -Must 'result: OK'
  Step 'test_lookahead BL-001' "$L\test_lookahead.exe" @('baseline\BL-001.inputs', 'tests\game\data\BL-001.fingerprints.txt') -Must 'result: OK'
  Step 'test_smooth BL-001' "$L\test_smooth.exe" @('baseline\BL-001.inputs', 'tests\game\data\BL-001.fingerprints.txt') -Must 'result: OK'
  Step 'test_smooth BL-035 switched on at frame 1000' "$L\test_smooth.exe" @('baseline\BL-035.inputs', 'tests\game\data\BL-035.fingerprints.txt', '100000', '1000') -Must 'result: OK'
  Step 'test_controls' "$H\test_controls.exe" @() -Must 'ALL TESTS PASSED'
  Step 'test_menu' "$H\test_menu.exe" @() -Must 'ALL TESTS PASSED'
  Step 'test_background' "$H\test_background.exe" @() -Must 'ALL TESTS PASSED'
  Step 'test_framerate' "$H\test_framerate.exe" @() -Must 'ALL TESTS PASSED'
  Step 'test_hdr' "$H\test_hdr.exe" @() -Must 'ALL TESTS PASSED'
  Step 'test_options' "$H\test_options.exe" @() -Must 'ALL TESTS PASSED'
  Step 'test_crt' "$H\test_crt.exe" @() -Must 'ALL TESTS PASSED'
  Step 'test_volume' "$H\test_volume.exe" @() -Must 'ALL TESTS PASSED'
  Step 'test_feedback' "$H\test_feedback.exe" @() -Must 'ALL TESTS PASSED'
  # Stage I5: the Escape menu and the game played together, three starting states and all eight games; the report file says PASS for each (exit 0 = all right)
  $menuDir = Scratch 'menu'
  Step 'menu-check' "$H\pacman2600.exe" @('--menu-check', (Join-Path $menuDir 'menu.txt'), $rom, '--settings', (Join-Path $menuDir 's.ini'))
  # Stage I6: two players on one set of controls, replayed in the native program and the one with the original ROM; pictures and sounds must agree
  Py 'i6_shared_replay' 'i6_shared_replay.py' @($rom, (Scratch 'i6'))
  Step 'test_layout' "$H\test_layout.exe" @() -Must 'ALL TESTS PASSED'
  Step 'test_inputlog' "$H\test_inputlog.exe" @((Scratch 'inputlog')) -Must 'ALL TESTS PASSED'
  Step 'test_trace' "$H\test_trace.exe" @() -Must 'ALL TESTS PASSED'
  Step 'test_windowstate' "$H\test_windowstate.exe" @((Scratch 'windowstate')) -Must 'ALL TESTS PASSED'
  Step 'test_rom_finder' "$H\test_rom_finder.exe" @((Scratch 'rom_finder'), $rom) -Must 'ALL TESTS PASSED'
  # rules of the project
  Py 'g2_conventions --strict' 'g2_conventions_check.py' @('--strict') -Must 'result: CLEAN'
  Py 'g7_gate_check' 'g7_gate_check.py' @($rom)
  Py 'h1_testing_check' 'h1_testing_check.py'
  Py 'h7_preservation_check' 'h7_preservation_check.py'
  Py 'h7_preservation drill' 'h7_preservation_check.py' @('--drill', (Join-Path (Scratch 'h7_drill') 'run')) -Must 'drill result: OK'
  # proofs against the original (the short ones)
  foreach ($t in @('f10_pacman', 'f11_maze', 'f13_ghosts', 'f14_random', 'f15_scoring', 'f16_levels', 'f17_states')) { Py $t ($t + '.py') @($rom) }
  Py 'f18_kernel --cached' 'f18_kernel.py' @($rom, '--cached')
  Py 'f19_sound --cached' 'f19_sound.py' @($rom, '--cached')
  foreach ($t in @('f20_lockstep', 'f21_native', 'f22_loader', 'f23_kernel', 'f24_audio')) { Py $t ($t + '.py') @($rom) }
  # five key traces: different inputs (start, the whole first level, the first power pill, two players, ghosts walking home)
  foreach ($id in @('BL-001', 'BL-035', 'BL-044', 'BL-067', 'BL-070')) {
    Py ('baseline ' + $id) 'baseline.py' @('check', $rom, $id) -Must 'equals the stored SHA-256' -MustNot 'DIFFERS'
  }
}

# The steps of the public set (Stage H4): no ROM, no reference folder. Whole games are replayed from the tracked inputs and compared with
# the tracked fingerprint files (tests\game\data); the game's tables are part of the program.
function Add-PublicSteps {
  $L = Join-Path $script:Root 'build\lockstep'
  Step 'test_tables (structure)' "$L\test_tables.exe" @() -Must 'result: OK'
  Step 'test_game_modules' "$L\test_game_modules.exe" @() -Must 'result: OK'
  $games = Get-ChildItem (Join-Path $script:Root 'tests\game\data') -Filter 'BL-*.fingerprints.txt' | Sort-Object Name
  foreach ($g in $games) {
    $id = $g.Name.Substring(0, 6)
    Step ('test_no_rom ' + $id) "$L\test_no_rom.exe" @(('baseline\' + $id + '.inputs'), ('tests\game\data\' + $g.Name)) -Must 'result: IDENTICAL'
    Step ('test_layers ' + $id) "$L\test_layers.exe" @(('baseline\' + $id + '.inputs'), ('tests\game\data\' + $g.Name)) -Must 'result: OK'
    Step ('test_objects_view ' + $id) "$L\test_objects_view.exe" @(('baseline\' + $id + '.inputs'), ('tests\game\data\' + $g.Name)) -Must 'result: OK'
    if ($id -in 'BL-001', 'BL-035', 'BL-044', 'BL-070') {
      Step ('test_lookahead ' + $id) "$L\test_lookahead.exe" @(('baseline\' + $id + '.inputs'), ('tests\game\data\' + $g.Name)) -Must 'result: OK'
      Step ('test_smooth ' + $id) "$L\test_smooth.exe" @(('baseline\' + $id + '.inputs'), ('tests\game\data\' + $g.Name)) -Must 'result: OK'
      Step ('test_smooth ' + $id + ' switched on at frame 1000') "$L\test_smooth.exe" @(('baseline\' + $id + '.inputs'), ('tests\game\data\' + $g.Name), '100000', '1000') -Must 'result: OK'
    }
  }
  Py 'g2_conventions --strict' 'g2_conventions_check.py' @('--strict') -Must 'result: CLEAN'
  Py 'g7_gate_check --no-rom' 'g7_gate_check.py' @('--no-rom')
  Py 'h1_testing_check' 'h1_testing_check.py'
  Py 'h7_preservation_check' 'h7_preservation_check.py'
}

function Finish-Tests([string]$Kind) {
  $total = $script:Results.Count
  $passed = @($script:Results | Where-Object { $_.Ok }).Count
  $failed = $script:Results | Where-Object { -not $_.Ok }
  $secs = [int]((Get-Date) - $script:Started).TotalSeconds
  $lines = @()
  $lines += ('Test run ' + $Kind + ': ' + $passed + ' of ' + $total + ' steps passed in ' + $secs + ' s')
  foreach ($f in $failed) { $lines += ('  FAILED: ' + $f.Name + ' - ' + $f.Detail) }
  $ok = ($failed | Measure-Object).Count -eq 0
  $lines += ('result: ' + $(if ($ok) { 'OK' } else { 'FAILED' }))
  $lines | ForEach-Object { Write-Host $_ }
  Set-Content -Path (Join-Path $script:RunDir 'summary.txt') -Value ($script:Results | ForEach-Object { ('{0}  {1}  {2} s  {3}' -f $(if ($_.Ok) { 'PASS' } else { 'FAIL' }), $_.Name, $_.Seconds, $_.Detail) }) -Encoding UTF8
  Add-Content -Path (Join-Path $script:RunDir 'summary.txt') -Value $lines -Encoding UTF8
  Write-Host 'Note: the proof tools rewrite the running time inside some docs\ files. That is expected; it is not a change to the project.'
  Write-Host ('Logs: ' + (Join-Path $script:RunDir 'logs'))
  if ($ok) { exit 0 } else { exit 1 }
}
