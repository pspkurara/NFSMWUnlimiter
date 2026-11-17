[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Template,
    [Parameter(Mandatory)][string]$Destination,
    [Parameter(Mandatory)][string]$UnmodifiedUnlimiter,
    [Parameter(Mandatory)][string]$HostBinary,
    [Parameter(Mandatory)][string]$CompanionBinary,
    [string]$CameraBinary
)
$ErrorActionPreference = 'Stop'
$source = (Resolve-Path -LiteralPath $Template).Path
$target = [IO.Path]::GetFullPath($Destination)
if (Test-Path -LiteralPath $target) { throw 'Destination must not exist; this tool never overwrites a game.' }
if (!(Test-Path -LiteralPath (Split-Path -Parent $target) -PathType Container)) { throw 'Destination parent must exist.' }

# Only these immutable asset trees may be shared. Saves, settings and plugins
# are ordinary private copies; never recursively traverse an asset junction.
$assets = @('CARS','CREDITS','FRONTEND','GLOBAL','LANGUAGES','MOVIES','NEXTGENGRAPHICS','NIS','SOUND','SUBTITLES','Support','TRACKS','Uninstall')
$private = @('scripts','MEMCARD','SAVE')
function Assert-PrivateTree([string]$directory) {
    $item = Get-Item -LiteralPath $directory
    if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Private tree contains a redirect: $directory" }
    if ($item.PSIsContainer) {
        foreach ($child in Get-ChildItem -LiteralPath $directory -Force) { Assert-PrivateTree $child.FullName }
    }
}
foreach ($name in $private) { Assert-PrivateTree (Join-Path $source $name) }
foreach ($name in $assets) {
    if (!(Test-Path -LiteralPath (Join-Path $source $name) -PathType Container)) { throw "Missing asset directory: $name" }
}
$widescreen = Join-Path $source 'scripts/NFSMostWanted.WidescreenFix.ini'
$settings = Get-Content -LiteralPath $widescreen -Raw
if ($settings -notmatch '(?im)^\s*CustomUserFilesDirectoryInGameDir\s*=\s*SAVE\s*(?:;.*)?$' -or
    $settings -notmatch '(?im)^\s*WriteSettingsToFile\s*=\s*1\s*(?:;.*)?$') {
    throw 'Template must redirect saves to its own SAVE directory and settings to file.'
}
function Hash([string]$file) { (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() }
if ((Hash (Join-Path $source 'speed.exe')) -ne '0c5675a08cd71fd6d31ca87e992a915054bd8b80d268bff0561d7ecc2067e342') {
    throw 'This validation recipe requires the inspected PC 1.3 executable.'
}
$dependency = (Resolve-Path -LiteralPath $UnmodifiedUnlimiter).Path
# This recipe proves one concrete unchanged dependency, not all 3.3.1 builds.
$dependencyHash = 'd1f7516bbf67bc78563e938515a10cf7c0e1c1654406ef1b6eab9ef3c7ba54c9'
if ((Hash $dependency) -ne $dependencyHash) { throw 'Expected the preserved, unmodified REDUX Unlimiter 3.3.1.1338.' }
$binaries = [ordered]@{
    'NFSMWUnlimiter.asi' = $dependency
    'NFSMWStanceHost.asi' = (Resolve-Path -LiteralPath $HostBinary).Path
    'NFSMWCustomizationLab.asi' = (Resolve-Path -LiteralPath $CompanionBinary).Path
}
if ($CameraBinary) { $binaries['NFSMWExtendedCamera.asi'] = (Resolve-Path -LiteralPath $CameraBinary).Path }
$hashes = [ordered]@{}
foreach ($entry in $binaries.GetEnumerator()) { $hashes[$entry.Key] = Hash $entry.Value }

New-Item -ItemType Directory -Path $target | Out-Null
foreach ($name in @('speed.exe','dinput8.dll','IDI_VISUALTREATMENT_FX')) {
    Copy-Item -LiteralPath (Join-Path $source $name) -Destination (Join-Path $target $name)
}
foreach ($name in $private) { Copy-Item -LiteralPath (Join-Path $source $name) -Destination (Join-Path $target $name) -Recurse }
foreach ($name in $assets) {
    New-Item -ItemType Junction -Path (Join-Path $target $name) -Target (Join-Path $source $name) | Out-Null
}
$scripts = Join-Path $target 'scripts'
# If the template already had a separate camera, do not silently enable it in
# the stance-only case. Retain its bytes under an extension the loader ignores.
$camera = Join-Path $scripts 'NFSMWExtendedCamera.asi'
if (!$CameraBinary -and (Test-Path -LiteralPath $camera)) {
    Move-Item -LiteralPath $camera -Destination ($camera + '.disabled-validation')
}
foreach ($entry in $binaries.GetEnumerator()) {
    $file = Join-Path $scripts $entry.Key
    Copy-Item -LiteralPath $entry.Value -Destination $file -Force
    if ((Hash $file) -ne $hashes[$entry.Key]) { throw "Copied binary differs: $file" }
}
# A new installation has a distinct companion config namespace. Enable only
# its distributed default, with the optional Windows debug editor still off.
$labIni = Join-Path $scripts 'NFSMWCustomizationLab.ini'
$lab = Get-Content -LiteralPath $labIni -Raw
if ($lab -notmatch '(?m)^Enabled\s*=') { throw 'Missing companion Enabled setting.' }
$lab = [regex]::Replace($lab, '(?m)^Enabled\s*=.*$', 'Enabled=1')
$lab = [regex]::Replace($lab, '(?m)^DebugEditorHotkey\s*=.*$', 'DebugEditorHotkey=0')
[IO.File]::WriteAllText($labIni, $lab, [Text.UTF8Encoding]::new($false))
$manifest = [ordered]@{
    schema = 1; kind = 'unchanged-unlimiter-stance-validation'; source = $source
    destination = $target; runtimeVerified = $false; cameraIncluded = [bool]$CameraBinary
    nativeSaveDirectory = (Join-Path $target 'SAVE'); files = $hashes
    sharedAssetDirectories = $assets; privateDirectories = $private
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $target 'stance-validation.json') -Encoding UTF8
Write-Output "Prepared isolated validation copy (not launched): $target"
