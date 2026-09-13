param(
    [Parameter(Mandatory = $true)] [string] $ApkPath,
    [Parameter(Mandatory = $true)] [string] $Aapt2,
    [Parameter(Mandatory = $true)] [string] $Zipalign,
    [Parameter(Mandatory = $true)] [string] $Apksigner
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($env:RELEASE_TAG)) {
    throw 'RELEASE_TAG is required'
}

$expectedPackage = 'local.workbuddy.alttab'
$expectedCertificate = 'E2ECAC920270F8949C97397AECE57B4FB6104F99D0DAD0471810613F1431273F'
$expectedVersion = $env:RELEASE_TAG.TrimStart('v')

if (-not (Test-Path -LiteralPath $ApkPath -PathType Leaf)) {
    throw "APK does not exist: $ApkPath"
}

function Invoke-CheckedNative {
    param(
        [Parameter(Mandatory = $true)] [string] $Executable,
        [Parameter(Mandatory = $true)] [string[]] $Arguments
    )

    $output = & $Executable @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "$Executable failed with exit code $LASTEXITCODE`n$($output -join "`n")"
    }
    return $output
}

$badging = Invoke-CheckedNative -Executable $Aapt2 -Arguments @('dump', 'badging', $ApkPath)
$badgingText = $badging -join "`n"
if ($badgingText -notmatch "package: name='$([regex]::Escape($expectedPackage))'") {
    throw "Unexpected package; expected $expectedPackage"
}
if ($badgingText -notmatch "versionName='$([regex]::Escape($expectedVersion))'") {
    throw "APK version does not match tag $($env:RELEASE_TAG)"
}

$null = Invoke-CheckedNative -Executable $Zipalign -Arguments @('-c', '-P', '16', '-v', '4', $ApkPath)
$signature = Invoke-CheckedNative -Executable $Apksigner -Arguments @('verify', '--verbose', '--print-certs', $ApkPath)
$signatureText = $signature -join "`n"
if ($signatureText -notmatch 'Verified using v2 scheme \(APK Signature Scheme v2\): true') {
    throw 'APK Signature Scheme v2 verification failed'
}
if ($signatureText -notmatch 'Verified using v3 scheme \(APK Signature Scheme v3\): true') {
    throw 'APK Signature Scheme v3 verification failed'
}

$certificateMatch = [regex]::Match(
    $signatureText,
    '(?:Signer #1|V\d+(?:\.\d+)? Signer:) certificate SHA-256 digest: ([0-9a-fA-F:]+)'
)
if (-not $certificateMatch.Success) {
    throw 'Unable to read signer certificate SHA-256'
}
$certificate = $certificateMatch.Groups[1].Value.Replace(':', '').ToUpperInvariant()
if ($certificate -ne $expectedCertificate) {
    throw "Unexpected signing certificate SHA-256: $certificate"
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [System.IO.Compression.ZipFile]::OpenRead((Resolve-Path -LiteralPath $ApkPath).Path)
try {
    $nativeAsset = $archive.Entries | Where-Object { $_.FullName -eq 'assets/alttab_evdev_bridge' }
    if ($null -eq $nativeAsset -or $nativeAsset.Length -le 0) {
        throw 'APK is missing assets/alttab_evdev_bridge'
    }
} finally {
    $archive.Dispose()
}

$apkHash = (Get-FileHash -LiteralPath $ApkPath -Algorithm SHA256).Hash
[pscustomobject]@{
    package = $expectedPackage
    version = $expectedVersion
    certificateSha256 = $certificate
    apkSha256 = $apkHash
} | ConvertTo-Json -Compress
