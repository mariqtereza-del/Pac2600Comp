# Builds out\PacMan2600-<version>-windows.zip (Stage D15, DECISIONS.md T-D15-1).
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\make_release.ps1 [-Version 0.1.0]
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\make_release.ps1 -TestScan
#
# What goes in: the program, the DLLs it needs, README, licences, the source code the licences ask for, and NO ROM.
# What it refuses: a Git tree with uncommitted changes, a program older than its sources, and a finished zip that
# contains anything that could be a ROM (see Scan-Zip). Nothing is deleted: the zip is first written as .partial,
# scanned, and only then renamed; a failed scan leaves the .partial file for you to look at.
# -TestScan only proves that the scan refuses what it should (it makes two small zips of invented files in out\).
param(
  [string]$Version = '0.1.0',
  [switch]$TestScan,
  [switch]$AllowDirty   # for trying the script before a commit: the zip is then called ...-test.zip and says so inside
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$out = Join-Path $root 'out'
$romSha1 = '0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1'   # the Atari Pac-Man ROM (docs\ROM_IDENTITY.md)

function Fail([string]$message) { Write-Host "FAIL: $message" -ForegroundColor Red; exit 1 }

# ---------------------------------------------------------------------------------------------------------------
# The scan: nothing that could be a ROM may be in the zip, nor in the zip inside it.
# A file is refused if: it ends in .a26 .bin .rom .rar .7z .tar .gz; it is a zip other than the source zip; its SHA-1 is the
# known ROM; or it is exactly 4096 bytes and holds a zero byte (text and source files of that size have none, a ROM does).
# ---------------------------------------------------------------------------------------------------------------
# Files of a refused type that are known and harmless, each pinned by its SHA-256 (name ending, fingerprint):
# Stella's macOS disk-image template, part of the unchanged Stella 7.0 source (docs\DEPENDENCIES.md).
$knownFiles = @{ 'third_party/stella/src/os/macos/template.dmg.gz' = '706106344cdaa23403513874bf2a67d07f50f5adf99a6c1d69a0130b30aa6b02' }
$script:scanFiles = 0
$script:scanProblems = New-Object System.Collections.ArrayList
$script:scanTextLike4k = New-Object System.Collections.ArrayList

function Scan-Entries([System.IO.Compression.ZipArchive]$archive, [string]$where) {
  foreach ($entry in $archive.Entries) {
    if ($entry.FullName.EndsWith('/')) { continue }
    $script:scanFiles++
    $label = "$where$($entry.FullName)"
    $ext = [IO.Path]::GetExtension($entry.Name).ToLowerInvariant()
    $isSourceZip = ($where -eq '' -and $entry.FullName -like '*/source/*-source.zip')
    if ($ext -eq '.zip' -and -not $isSourceZip) { [void]$script:scanProblems.Add("$label : an unexpected zip file") }

    $stream = $entry.Open()
    $memory = New-Object System.IO.MemoryStream
    $stream.CopyTo($memory)
    $stream.Dispose()
    $bytes = $memory.ToArray()
    $memory.Dispose()

    $sha256 = (([BitConverter]::ToString((New-Object System.Security.Cryptography.SHA256CryptoServiceProvider).ComputeHash($bytes))) -replace '-', '').ToLowerInvariant()
    $knownName = $knownFiles.Keys | Where-Object { $entry.FullName.EndsWith($_) } | Select-Object -First 1
    $isKnown = $knownName -and $knownFiles[$knownName] -eq $sha256
    if (@('.a26', '.bin', '.rom', '.rar', '.7z', '.tar', '.gz') -contains $ext -and -not $isKnown) { [void]$script:scanProblems.Add("$label : file type $ext") }
    $sha1 = New-Object System.Security.Cryptography.SHA1CryptoServiceProvider
    $hash = ([BitConverter]::ToString($sha1.ComputeHash($bytes)) -replace '-', '').ToLowerInvariant()
    if ($hash -eq $romSha1) { [void]$script:scanProblems.Add("$label : it is the Pac-Man ROM") }

    if ($bytes.Length -eq 4096) {
      if ($bytes -contains 0) { [void]$script:scanProblems.Add("$label : exactly 4096 bytes and not text") }
      else { [void]$script:scanTextLike4k.Add($label) }
    }
    if ($isSourceZip) {
      $inner = New-Object System.IO.Compression.ZipArchive((New-Object System.IO.MemoryStream(, $bytes)), [System.IO.Compression.ZipArchiveMode]::Read)
      Scan-Entries $inner "$label!/"
      $inner.Dispose()
    }
  }
}

function Scan-Zip([string]$path) {
  $script:scanFiles = 0
  $script:scanProblems.Clear()
  $script:scanTextLike4k.Clear()
  $archive = [System.IO.Compression.ZipFile]::OpenRead($path)
  try { Scan-Entries $archive '' } finally { $archive.Dispose() }
  return ($script:scanProblems.Count -eq 0)
}

# ---------------------------------------------------------------------------------------------------------------
# -TestScan: the scan must refuse bad zips and accept a good one. All files are invented; the real ROM is never used.
# ---------------------------------------------------------------------------------------------------------------
function New-TestZip([string]$path, [hashtable]$files) {
  $top = 'PacMan2600-test'
  $fs = [IO.File]::Open($path, 'Create')
  $zip = New-Object System.IO.Compression.ZipArchive($fs, [System.IO.Compression.ZipArchiveMode]::Create)
  foreach ($name in $files.Keys) {
    $e = $zip.CreateEntry("$top/$name")
    $s = $e.Open(); $b = [byte[]]$files[$name]; $s.Write($b, 0, $b.Length); $s.Dispose()
  }
  $zip.Dispose(); $fs.Dispose()
}

if ($TestScan) {
  if (-not (Test-Path $out)) { New-Item -ItemType Directory -Path $out | Out-Null }
  $rng = New-Object System.Random(7)
  $binary4k = New-Object byte[] 4096; $rng.NextBytes($binary4k); $binary4k[0] = 0       # looks like a ROM: 4096 bytes with a zero byte
  $text4k = [byte[]](,0x61 * 4096)                                                     # a 4096-byte text-like file
  $cases = @(
    @{ name = 'an invented 4 KB binary file'; files = @{ 'readme.txt' = [byte[]](65, 66); 'data/mystery.dat' = $binary4k }; expect = $false },
    @{ name = 'a file called game.a26'; files = @{ 'readme.txt' = [byte[]](65, 66); 'roms/game.a26' = [byte[]](1, 2, 3) }; expect = $false },
    @{ name = 'a file called game.bin'; files = @{ 'game.bin' = [byte[]](1, 2, 3) }; expect = $false },
    @{ name = 'an unexpected zip file'; files = @{ 'more.zip' = [byte[]](80, 75, 5, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) }; expect = $false },
    @{ name = 'a 4096-byte text file only (allowed)'; files = @{ 'readme.txt' = [byte[]](65, 66); 'source.txt' = $text4k }; expect = $true }
  )
  $allGood = $true
  foreach ($c in $cases) {
    $p = Join-Path $out 'scan-test.zip'
    New-TestZip $p $c.files
    $passed = Scan-Zip $p
    $ok = ($passed -eq $c.expect)
    if (-not $ok) { $allGood = $false }
    $verdict = if ($passed) { 'accepted' } else { 'refused' }
    Write-Host ("{0,-46} scan {1,-8} {2}" -f $c.name, $verdict, $(if ($ok) { 'as it should' } else { 'WRONG' }))
  }
  # the ROM's fingerprint: an invented file whose SHA-1 is pretended to be the ROM's must be refused
  $invented = [byte[]](9, 8, 7, 6, 5)
  $saved = $romSha1
  $romSha1 = ([BitConverter]::ToString((New-Object System.Security.Cryptography.SHA1CryptoServiceProvider).ComputeHash($invented)) -replace '-', '').ToLowerInvariant()
  New-TestZip (Join-Path $out 'scan-test.zip') @{ 'readme.txt' = [byte[]](65, 66); 'docs/innocent-name.txt' = $invented }
  $passed = Scan-Zip (Join-Path $out 'scan-test.zip')
  $romSha1 = $saved
  if ($passed) { $allGood = $false }
  Write-Host ('{0,-46} scan {1,-8} {2}' -f 'a file with the ROM fingerprint', $(if ($passed) { 'accepted' } else { 'refused' }), $(if (-not $passed) { 'as it should' } else { 'WRONG' }))
  if ($allGood) { Write-Host 'result=OK (the scan refuses what it should and accepts a plain text file of 4096 bytes)'; exit 0 }
  Fail 'the scan did not behave as it should'
}

# ---------------------------------------------------------------------------------------------------------------
# Checks before building
# ---------------------------------------------------------------------------------------------------------------
$git = (Get-Command git -ErrorAction SilentlyContinue)
if (-not $git) { Fail 'git was not found' }
$dirty = [bool](git -C $root status --porcelain)
if ($dirty -and -not $AllowDirty) { Fail 'the Git tree has uncommitted changes: commit first, so the zip is exactly what Git holds' }
if ($dirty) { $Version = "$Version-test"; Write-Host 'WARNING: uncommitted changes: this is a TEST zip, not a release' -ForegroundColor Yellow }
$top = "PacMan2600-$Version-windows"
$commit = (git -C $root rev-parse HEAD).Trim()
$commitDate = (git -C $root log -1 --format=%cI).Trim()

$exe = Join-Path $root 'build\host\pacman2600.exe'
$sdl = Join-Path $root 'build\host\SDL3.dll'
foreach ($f in @($exe, $sdl)) { if (-not (Test-Path $f)) { Fail "missing $f : run tools\build_host.cmd first" } }
$newestSource = git -C $root ls-files src/host third_party/CMakeLists.txt | ForEach-Object { (Get-Item (Join-Path $root $_)).LastWriteTime } | Sort-Object -Descending | Select-Object -First 1
if ((Get-Item $exe).LastWriteTime -lt $newestSource) { Fail 'pacman2600.exe is older than its sources: run tools\build_host.cmd first' }

# The Microsoft runtime files the program needs (read from the Build Tools' Redist folder; they may be shipped with a program).
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Workload.VCTools -property installationPath) | Select-Object -First 1
if (-not $vs) { Fail 'Visual Studio Build Tools were not found (needed for the Microsoft runtime files)' }
$crt = Get-ChildItem (Join-Path $vs 'VC\Redist\MSVC') -Directory | Sort-Object Name -Descending |
  ForEach-Object { Get-ChildItem (Join-Path $_.FullName 'x64') -Directory -Filter 'Microsoft.VC*.CRT' -ErrorAction SilentlyContinue } | Select-Object -First 1
if (-not $crt) { Fail 'the Microsoft runtime folder (Redist\MSVC\...\x64\Microsoft.VC*.CRT) was not found' }
$runtimeFiles = 'msvcp140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll'
foreach ($f in $runtimeFiles) { if (-not (Test-Path (Join-Path $crt.FullName $f))) { Fail "$f is missing from $($crt.FullName)" } }

$licenceStella = Join-Path $root 'third_party\stella\License.txt'
$copyrightStella = Join-Path $root 'third_party\stella\Copyright.txt'
$licenceSdl = Join-Path $root 'third_party\SDL3\LICENSE.txt'
foreach ($f in @($licenceStella, $copyrightStella, $licenceSdl)) { if (-not (Test-Path $f)) { Fail "missing $f" } }

if (-not (Test-Path $out)) { New-Item -ItemType Directory -Path $out | Out-Null }

# The source code: only the folders that are needed to build the program (not the project notes), straight from Git.
$sourceZip = Join-Path $out "PacMan2600-$Version-source.zip"
git -C $root archive --format=zip -o $sourceZip HEAD src tests third_party tools/build_host.cmd tools/build_third_party.cmd
if ($LASTEXITCODE -ne 0) { Fail 'git archive failed' }

# ---------------------------------------------------------------------------------------------------------------
# The text files
# ---------------------------------------------------------------------------------------------------------------
$readme = @"
PAC-MAN FOR PC (Atari 2600 version)   version $Version
=====================================================

This program plays the Atari 2600 game Pac-Man (1981) on your PC. It runs the game with its own rebuilt
game code (proved identical to the original game, picture and sound, frame for frame, against the Stella emulator), at the speed of a real 1981 NTSC console (59.92 pictures a second),
and it shows the picture in the right shape (12:7 pixels) with a black border, scaled to your screen.

THE GAME IS NOT INCLUDED. You need your own copy of the game file (a 4096-byte "Pac-Man (1981) (Atari).a26").
The program only accepts the original file; its fingerprint (SHA-1) must be
    0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1

HOW TO START
  1. Unzip this folder somewhere you can write to (for example your Documents or Desktop; not "Program Files").
  2. Start pacman2600.exe. The first time, it asks for your game file; or put the file into the "roms" folder
     here beforehand. The program remembers where it was, and where you left the window.
  3. Press F3 (or the Options button on a PS4 pad) to start a game.

KEYS                                              PS4 PAD
  Arrow keys or W A S D   move                      D-pad or left stick   move
  Space or Z              fire button               Cross or Square       fire button
  F3 (hold)               Game Reset: new game      Options               Game Reset
  F2 (hold)               Game Select: variation    Share                 Game Select
  F4                      TV type: Colour / B&W
  F5 / F6                 left / right difficulty switch A / B
  F7                      soft glow on / off (off = as on a real TV)
  1 2 3 4 5               picture: Sharp, Fill, TV frame, Game area, Framed (5 is the standard)
  Alt + Enter             fullscreen on / off
  F1                      shows this list on the screen
  Esc                     quit
The three switches (F4, F5, F6) stay where you leave them, also for the next start.

RECORDING (to keep a game and watch it again, or for testers)
  Double-click  "Record a game.cmd"   to play as usual while every frame's input is recorded. Close the game with Esc
                (or the X button): the recording is then saved in the "recordings" folder next to the program.
  Double-click  "Replay a game.cmd"   to play the newest recording back from power-on; the game comes out exactly the
                same. To replay another recording, drag its file onto "Replay a game.cmd". When a recording ends, the
                keyboard takes over again.
  By hand (Command Prompt, in this folder):   pacman2600.exe --record "C:\some folder\my.inputs"
                                              pacman2600.exe --replay "C:\some folder\my.inputs"
  A name without a folder is saved in the folder the Command Prompt was in, not beside the program.

FILES THE PROGRAM WRITES
  settings.ini next to pacman2600.exe: the path of your game file, the window place and size, the three switches, the soft glow.

LICENCES AND SOURCE
  This program contains no emulator, but its picture colours are the colour table of Stella 7.0 (GNU General Public License version 2)
  and it uses SDL 3.4.18 (zlib licence). Because of the GPL, this program is distributed under the GNU General Public License version 2:
  see licences\Stella-License.txt. The complete source code of this program, with Stella and SDL, is in
      source\PacMan2600-$Version-source.zip     (how to build it: source\BUILDING.txt)
  The three Microsoft runtime files (msvcp140.dll, vcruntime140.dll, vcruntime140_1.dll) are Microsoft's
  Visual C++ runtime, shipped next to the program as Microsoft allows.
  Pac-Man is a game by its rights holders. This program is not made by or connected to them.
"@

$building = @"
Building this program from the source in PacMan2600-$Version-source.zip
-------------------------------------------------------------------
Needs: Windows 10 or 11 (64 bit); Visual Studio Build Tools with the "Desktop development with C++" part
(the MSVC compiler 14.51 was used), which brings CMake and Ninja.
Unzip the source zip, then run  tools\build_host.cmd  in a command window. The program appears in build\host.
The libraries are in third_party\ (SDL3 3.4.18 and Stella 7.0, unchanged; the one build-time fix is in
third_party\CMakeLists.txt).
"@

$recordCmd = @"
@echo off
rem Plays the game and records every frame's input. Close the game with Esc: the file is then in the recordings folder.
setlocal
cd /d "%~dp0"
if not exist "recordings" mkdir "recordings"
for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmmss"') do set "STAMP=%%i"
start "" "%~dp0pacman2600.exe" --record "%~dp0recordings\play-%STAMP%.inputs"
echo Recording. Close the game with Esc; the recording is saved in the "recordings" folder next to the program.
timeout /t 5 >nul
"@

$replayCmd = @"
@echo off
rem Plays a recording back: the one dropped on this file, or else the newest one in the recordings folder.
setlocal
cd /d "%~dp0"
set "FILE=%~1"
if "%FILE%"=="" for /f "delims=" %%f in ('dir /b /o-d "recordings\*.inputs" 2^>nul') do if not defined FILE set "FILE=%~dp0recordings\%%f"
if "%FILE%"=="" (
  echo There is no recording yet. Use "Record a game.cmd" first, or drag a .inputs file onto this file.
  pause
  exit /b 1
)
start "" "%~dp0pacman2600.exe" --replay "%FILE%"
"@

$romNote = @"
Put your own copy of the game file here (the original "Pac-Man (1981) (Atari).a26", 4096 bytes).
It is not part of this download and must never be passed on with it.
The program also finds it by asking you the first time it starts.
"@

# ---------------------------------------------------------------------------------------------------------------
# Build the zip (as .partial)
# ---------------------------------------------------------------------------------------------------------------
$zipPath = Join-Path $out "PacMan2600-$Version-windows.zip"
$partial = "$zipPath.partial"
$hashes = New-Object System.Collections.ArrayList

function Add-File($zip, [string]$sourcePath, [string]$name) {
  [void][System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $sourcePath, "$top/$name", [System.IO.Compression.CompressionLevel]::Optimal)
  $h = (Get-FileHash -Algorithm SHA256 -Path $sourcePath).Hash.ToLowerInvariant()
  [void]$hashes.Add("$h  $name  ($((Get-Item $sourcePath).Length) bytes)")
}
function Add-Text($zip, [string]$text, [string]$name) {
  $entry = $zip.CreateEntry("$top/$name", [System.IO.Compression.CompressionLevel]::Optimal)
  $writer = New-Object System.IO.StreamWriter($entry.Open(), (New-Object System.Text.UTF8Encoding($false)))
  $writer.Write(($text -replace "`r?`n", "`r`n"))
  $writer.Dispose()
}

$fs = [IO.File]::Open($partial, 'Create')
$zip = New-Object System.IO.Compression.ZipArchive($fs, [System.IO.Compression.ZipArchiveMode]::Create)
try {
  Add-File $zip $exe 'pacman2600.exe'
  Add-File $zip $sdl 'SDL3.dll'
  foreach ($f in $runtimeFiles) { Add-File $zip (Join-Path $crt.FullName $f) $f }
  Add-File $zip $licenceStella 'licences/Stella-License.txt'
  Add-File $zip $copyrightStella 'licences/Stella-Copyright.txt'
  Add-File $zip $licenceSdl 'licences/SDL3-LICENSE.txt'
  Add-File $zip $sourceZip "source/PacMan2600-$Version-source.zip"
  Add-Text $zip $readme 'README.txt'
  Add-Text $zip $romNote 'roms/PUT_YOUR_ROM_HERE.txt'
  Add-Text $zip $recordCmd 'Record a game.cmd'
  Add-Text $zip $replayCmd 'Replay a game.cmd'
  Add-Text $zip $building 'source/BUILDING.txt'
  $info = "PacMan2600 $Version`n" + $(if ($dirty) { "TEST BUILD: made with uncommitted changes; the source zip is the last commit, not exactly this program`n" } else { '' }) + "Git commit: $commit`nCommit date: $commitDate`nMicrosoft runtime from: $($crt.FullName)`nSHA-256 of the files:`n" + ($hashes -join "`n") + "`n"
  Add-Text $zip $info 'BUILD-INFO.txt'
} finally {
  $zip.Dispose()
  $fs.Dispose()
}

# ---------------------------------------------------------------------------------------------------------------
# The scan, then the finished name
# ---------------------------------------------------------------------------------------------------------------
$passed = Scan-Zip $partial
$report = New-Object System.Collections.ArrayList
[void]$report.Add("PacMan2600 $Version release report")
[void]$report.Add("commit $commit")
[void]$report.Add("files scanned (zip and the source zip inside it): $script:scanFiles")
[void]$report.Add("4096-byte files that are plain text (allowed): $($script:scanTextLike4k.Count)")
foreach ($p in $script:scanProblems) { [void]$report.Add("PROBLEM: $p") }
[void]$report.Add($(if ($passed) { 'scan: nothing that could be a ROM was found' } else { 'scan: FAILED' }))
$report | ForEach-Object { Write-Host $_ }

if (-not $passed) { Fail "the scan found a problem; the unfinished file is $partial (not a release)" }
Move-Item -Force -Path $partial -Destination $zipPath
$size = (Get-Item $zipPath).Length
[void]$report.Add("result=OK $zipPath ($size bytes)")
$report | Set-Content -Path (Join-Path $out "PacMan2600-$Version-windows.report.txt") -Encoding ASCII
Write-Host "result=OK $zipPath ($size bytes)"
exit 0
