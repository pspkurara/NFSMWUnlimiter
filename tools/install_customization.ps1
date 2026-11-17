[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)][string]$Snapshot,
    [Parameter(Mandatory)][string]$GamePath,
    [Parameter(Mandatory)][string]$BackupRoot
)
$ErrorActionPreference = 'Stop'
$stage = (Resolve-Path -LiteralPath $Snapshot).Path
$game = (Resolve-Path -LiteralPath $GamePath).Path
$manifest = Get-Content -LiteralPath (Join-Path $stage 'manifest.json') -Raw | ConvertFrom-Json
if ($manifest.schema -ne 1 -or $manifest.kind -ne 'unfinished-development-snapshot') { throw 'Unknown snapshot manifest.' }
$exe = Join-Path $game 'speed.exe'
if ((Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash -ne $manifest.inspectedExecutableSHA256) {
    throw 'Executable differs from the inspected PC build; installation stopped.'
}
# A loaded ASI must never be replaced. Unknown process paths fail closed too.
foreach ($process in @(Get-CimInstance Win32_Process -Filter "Name='speed.exe'")) {
    # WER can retain a terminated process object after an access violation.
    # Only a live process can still have the ASI loaded; query failures other
    # than a vanished PID still fail closed under ErrorActionPreference=Stop.
    try { $live = [Diagnostics.Process]::GetProcessById([int]$process.ProcessId) }
    catch [ArgumentException] { continue }
    try { if ($live.HasExited) { continue } }
    finally { $live.Dispose() }
    if (!$process.ExecutablePath -or [IO.Path]::GetFullPath($process.ExecutablePath) -eq [IO.Path]::GetFullPath($exe)) {
        throw 'Save and normally exit this game before installing.'
    }
}
$scripts = Join-Path $game 'scripts'
if (!(Test-Path -LiteralPath $scripts -PathType Container)) { throw 'Existing ASI scripts directory required.' }
if ((Get-Item -LiteralPath $scripts).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Scripts directory must not be a junction or symlink.' }
$names = @('NFSMWUnlimiter.asi','NFSMWCustomizationLab.asi','NFSMWCustomizationCamera.ini','NFSMWCustomizationClip.ini','NFSMWCustomizationLab.ini')
$entries = @()
foreach ($name in $names) {
    $source = Join-Path $stage ('scripts/' + $name)
    $expected = $manifest.files.('scripts/' + $name)
    if (!$expected -or (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $expected) { throw "Payload verification failed: $name" }
    $target = Join-Path $scripts $name
    $exists = Test-Path -LiteralPath $target
    if ($exists -and ((Get-Item -LiteralPath $target).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "Refusing redirected target: $name" }
    # Existing user INIs remain authoritative; never overwrite tuning to update binaries.
    if ($exists -and $name.EndsWith('.ini')) { continue }
    $entries += [pscustomobject]@{ name=$name; source=$source; target=$target; existed=$exists; previousHash=if($exists){(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash}else{$null}; installedHash=$expected }
}
if (!$PSCmdlet.ShouldProcess($game, 'Install verified development snapshot with rollback backup')) { return }
$backup = Join-Path ([IO.Path]::GetFullPath($BackupRoot)) ((Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $backup | Out-Null
foreach ($entry in $entries) {
    if ($entry.existed) { Copy-Item -LiteralPath $entry.target -Destination (Join-Path $backup $entry.name) }
}
$receipt = [ordered]@{ schema=1; game=$game; snapshot=$stage; backup=$backup; state='prepared'; files=$entries }
$receiptFile = Join-Path $backup 'receipt.json'
$receipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $receiptFile -Encoding UTF8
$written = @()
try {
    foreach ($entry in $entries) {
        # Record before attempting a write, so even a partial copy is rolled back.
        $written += $entry
        Copy-Item -LiteralPath $entry.source -Destination $entry.target -Force
        if ((Get-FileHash -LiteralPath $entry.target -Algorithm SHA256).Hash -ne $entry.installedHash) { throw "Installed hash mismatch: $($entry.name)" }
    }
    $receipt.state='installed'
} catch {
    foreach ($entry in $written) {
        if ($entry.existed) { Copy-Item -LiteralPath (Join-Path $backup $entry.name) -Destination $entry.target -Force }
        elseif (Test-Path -LiteralPath $entry.target) { Remove-Item -LiteralPath $entry.target -Force }
    }
    $receipt.state='rolled-back'
    throw
} finally {
    $receipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $receiptFile -Encoding UTF8
}
Write-Output "Installed development snapshot. Backup and receipt: $backup"
