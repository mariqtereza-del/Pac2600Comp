<#
.SYNOPSIS
  Make a checkpoint zip of the whole project (including Git history) outside the project folder.
.DESCRIPTION
  Stage B8, DECISIONS.md T-B8-1 / T-B8-2. Only reads the project. Writes only <Destination>\<name>.zip
  and <name>.zip.sha256. Leaves out build\ and every ROM file (by extension and by content), refuses
  a destination inside the project, never overwrites, and checks the finished zip before keeping it.
.EXAMPLE
  powershell -NoProfile -ExecutionPolicy Bypass -File tools\make_checkpoint.ps1 -Stage B8
#>
param(
  [Parameter(Mandatory = $true)][ValidatePattern('^[A-J][0-9]{1,2}$|^Gate-[A-J]$')][string]$Stage,
  [string]$Destination
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path.TrimEnd('\')
if (-not $Destination) { $Destination = "$root checkpoints" }
$Destination = [IO.Path]::GetFullPath($Destination).TrimEnd('\')
if (($Destination + '\').StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase)) {
  throw "Destination '$Destination' is inside the project folder. Checkpoints must be stored outside it."
}
if (-not (Test-Path -LiteralPath $Destination -PathType Container)) {
  throw "Destination folder '$Destination' does not exist. Create it first (with the owner's approval)."
}

# The original ROM's fingerprint (docs\ROM_IDENTITY.md); any file with this content is a ROM, whatever its name.
$romSha1 = '0940fea7f04cdb6d4b90c5ad1a7e344e68f6dbb1'
$romSize = 4096
$romExt = '\.(a26|bin|rom)$'
function Get-Sha1([byte[]]$bytes) {
  $h = [Security.Cryptography.SHA1]::Create()
  try { -join ($h.ComputeHash($bytes) | ForEach-Object { $_.ToString('x2') }) } finally { $h.Dispose() }
}

$name = 'pacman2600-{0}-{1}.zip' -f (Get-Date -Format 'yyyy-MM-dd'), $Stage
$zipPath = Join-Path $Destination $name
$partial = "$zipPath.partial"
if (Test-Path -LiteralPath $zipPath) { throw "Checkpoint '$zipPath' already exists; it is never overwritten." }
if (Test-Path -LiteralPath $partial) { throw "Leftover '$partial' from an earlier failed run; look at it and remove it first." }

# Pick the files: everything except build\ and ROM files.
$files = New-Object System.Collections.Generic.List[object]
$skipped = New-Object System.Collections.Generic.List[string]
foreach ($f in Get-ChildItem -LiteralPath $root -Recurse -Force -File) {
  $rel = $f.FullName.Substring($root.Length + 1)
  if ($rel -like 'build\*') { continue }
  if ($rel -match $romExt) { $skipped.Add($rel); continue }
  if ($f.Length -eq $romSize -and (Get-Sha1 ([IO.File]::ReadAllBytes($f.FullName))) -eq $romSha1) {
    throw "File '$rel' has the ROM's content under another name. Stopping; nothing was written."
  }
  $files.Add([pscustomobject]@{ Full = $f.FullName; Entry = $rel.Replace('\', '/') })
}
$longest = ($files | ForEach-Object { $_.Entry.Length } | Measure-Object -Maximum).Maximum
Write-Output ("Project: {0}`nFiles to store: {1} (longest path inside the project: {2} characters)" -f $root, $files.Count, $longest)
Write-Output ("Left out as ROM files: {0}" -f ($(if ($skipped.Count) { $skipped -join ', ' } else { 'none' })))

try {
  $zip = [IO.Compression.ZipFile]::Open($partial, [IO.Compression.ZipArchiveMode]::Create)
  try {
    foreach ($f in $files) {
      [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $f.Full, $f.Entry, [IO.Compression.CompressionLevel]::Optimal)
    }
  } finally { $zip.Dispose() }

  # Check the finished zip before keeping it.
  $zip = [IO.Compression.ZipFile]::OpenRead($partial)
  try {
    if ($zip.Entries.Count -ne $files.Count) { throw "Zip holds $($zip.Entries.Count) entries, expected $($files.Count)." }
    foreach ($e in $zip.Entries) {
      if ($e.FullName -match $romExt) { throw "Zip entry '$($e.FullName)' looks like a ROM." }
      if ($e.Length -eq $romSize) {
        $ms = New-Object IO.MemoryStream; $s = $e.Open(); try { $s.CopyTo($ms) } finally { $s.Dispose() }
        if ((Get-Sha1 $ms.ToArray()) -eq $romSha1) { throw "Zip entry '$($e.FullName)' has the ROM's content." }
      }
    }
  } finally { $zip.Dispose() }
} catch {
  if (Test-Path -LiteralPath $partial) { Remove-Item -LiteralPath $partial -Force }
  throw "Checkpoint failed and its partial zip was removed: $($_.Exception.Message)"
}

Move-Item -LiteralPath $partial -Destination $zipPath
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $zipPath).Hash.ToLower()
[IO.File]::WriteAllText("$zipPath.sha256", "$hash  $name`n", (New-Object Text.UTF8Encoding $false))
Write-Output ("Checkpoint OK: {0}`nSize: {1:N0} bytes, {2} entries, no ROM inside (checked by name and content)`nSHA-256: {3}" -f $zipPath, (Get-Item -LiteralPath $zipPath).Length, $files.Count, $hash)
