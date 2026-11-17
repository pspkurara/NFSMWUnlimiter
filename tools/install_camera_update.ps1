[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)][string]$Snapshot,
    [Parameter(Mandatory)][string]$GamePath,
    [Parameter(Mandatory)][string]$BackupRoot
)
$ErrorActionPreference='Stop'
$stage=(Resolve-Path -LiteralPath $Snapshot).Path
$game=(Resolve-Path -LiteralPath $GamePath).Path
$manifest=Get-Content -LiteralPath (Join-Path $stage 'manifest.json') -Raw | ConvertFrom-Json
if ($manifest.kind -ne 'camera-only-baseline-update' -or $manifest.baseline -ne '18894c9155dc488cd3cda74594c6fd3fdc46874d') { throw 'Unknown baseline.' }
$exe=Join-Path $game 'speed.exe'
if ((Get-FileHash -LiteralPath $exe).Hash -ne '0c5675a08cd71fd6d31ca87e992a915054bd8b80d268bff0561d7ecc2067e342') { throw 'Unverified executable.' }
$scripts=Join-Path $game 'scripts'
$target=Join-Path $scripts 'NFSMWUnlimiter.asi'
$source=Join-Path $stage 'NFSMWUnlimiter.asi'
foreach ($path in @($scripts,$target)) {
    if ((Get-Item -LiteralPath $path).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Redirected installation target.' }
}
$oldHash=(Get-FileHash -LiteralPath $target).Hash
if ($oldHash -notin @('24608d38dd6e546d40d72db55c4bc30c441ab4ffe4546baeb7ae29ee58c930de',
    '24463a2bf4580f7dfa3f99b35307480c06f8a21a9d910e61a66db6988abbb919')) { throw 'Installed ASI differs from the recorded baseline.' }
if ((Get-FileHash -LiteralPath $source).Hash -ne $manifest.sha256) { throw 'Payload hash mismatch.' }
if (@(Get-CimInstance Win32_Process -Filter "Name='speed.exe'").Count) { throw 'Normally exit all game instances before installing.' }
if (!$PSCmdlet.ShouldProcess($target,'Install camera-only update with rollback backup')) { return }
$backup=Join-Path ([IO.Path]::GetFullPath($BackupRoot)) ('camera-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath $target -Destination (Join-Path $backup 'NFSMWUnlimiter.asi')
$receipt=[ordered]@{state='prepared'; target=$target; snapshot=$stage; previousHash=$oldHash; installedHash=$manifest.sha256; backup=$backup}
$receiptPath=Join-Path $backup 'receipt.json'
$receipt | ConvertTo-Json | Set-Content -LiteralPath $receiptPath -Encoding UTF8
$writeAttempted=$false
try {
    # Recheck immediately before writing; never overwrite a loaded module.
    if (@(Get-CimInstance Win32_Process -Filter "Name='speed.exe'").Count) { throw 'Game restarted during preparation.' }
    $writeAttempted=$true
    Copy-Item -LiteralPath $source -Destination $target -Force
    if ((Get-FileHash -LiteralPath $target).Hash -ne $manifest.sha256) { throw 'Installed hash mismatch.' }
    $receipt.state='installed'
} catch {
    if ($writeAttempted) {
        Copy-Item -LiteralPath (Join-Path $backup 'NFSMWUnlimiter.asi') -Destination $target -Force
        $receipt.state='rolled-back'
    } else { $receipt.state='aborted' }
    throw
} finally {
    $receipt | ConvertTo-Json | Set-Content -LiteralPath $receiptPath -Encoding UTF8
}
Write-Output "Camera update installed. Receipt: $receiptPath"
