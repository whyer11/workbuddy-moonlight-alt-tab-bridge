[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string] $PackageDirectory
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$package = (Resolve-Path -LiteralPath $PackageDirectory).Path
$versionText = Get-Content -LiteralPath (Join-Path $package 'VERSION.txt') -Raw
# The installer accepts only a product version, never an archive label such as ci.
$versionMatch = [regex]::Match($versionText, '(?m)^Version: ([0-9]+\.[0-9]+\.[0-9]+)\s*$')
if (-not $versionMatch.Success) { throw 'Package version must be an installable product version.' }
$version = $versionMatch.Groups[1].Value
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('workbuddy-package-test-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temporary | Out-Null
try {
    $stdout = Join-Path $temporary 'self-test.stdout.txt'
    $stderr = Join-Path $temporary 'self-test.stderr.txt'
    $process = Start-Process -FilePath (Join-Path $package 'WorkBuddyAltTabHost.exe') `
        -ArgumentList '--self-test' -WindowStyle Hidden -Wait -PassThru `
        -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if ($process.ExitCode -ne 0) { throw "Packaged Host self-test failed: $($process.ExitCode)" }
    $output = Get-Content -LiteralPath $stdout -Raw
    if ($output -notmatch ('(?m)^SELF_TEST_OK version=' + [regex]::Escape($version) + ' protocol=2\s*$')) {
        throw 'Package version does not match the executable startup version.'
    }
    Write-Output "PASS: Windows package version $version matches its executable and installer contract."
} finally {
    Remove-Item -LiteralPath $temporary -Recurse -Force
}
