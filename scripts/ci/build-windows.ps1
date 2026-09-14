param(
    [string] $Compiler = 'gcc',
    [string] $Version = $env:RELEASE_VERSION,
    [switch] $SkipInteractiveTests
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($Version)) {
    throw 'RELEASE_VERSION or -Version is required'
}
if ($null -eq (Get-Command $Compiler -ErrorAction SilentlyContinue)) {
    throw "C compiler was not found: $Compiler"
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Set-Location -LiteralPath $repoRoot

$outputRoot = Join-Path $repoRoot 'build\ci\windows'
$testsDir = Join-Path $outputRoot 'tests'
$packageDir = Join-Path $outputRoot 'package'
if (Test-Path -LiteralPath $outputRoot) {
    $resolvedOutput = (Resolve-Path -LiteralPath $outputRoot).Path
    $resolvedRepo = (Resolve-Path -LiteralPath $repoRoot).Path
    if (-not $resolvedOutput.StartsWith($resolvedRepo + [IO.Path]::DirectorySeparatorChar)) {
        throw "Refusing to clean output outside repository: $resolvedOutput"
    }
    Remove-Item -LiteralPath $resolvedOutput -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $testsDir, $packageDir | Out-Null

function Invoke-Compiler {
    param(
        [Parameter(Mandatory = $true)] [string[]] $Arguments
    )

    & $Compiler @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "C compilation failed with exit code $LASTEXITCODE"
    }
}

function Invoke-TestExecutable {
    param(
        [Parameter(Mandatory = $true)] [string] $Path,
        [string[]] $Arguments = @()
    )

    & $Path @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Test failed with exit code $LASTEXITCODE`: $Path"
    }
}

$commonFlags = @('-I', 'windows-host\src', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror')

$altTabTest = Join-Path $testsDir 'AltTabBridgeIntegrationTest.exe'
Invoke-Compiler -Arguments (@(
    'windows-host\tests\AltTabBridgeIntegrationTest.c'
) + $commonFlags + @('-o', $altTabTest, '-luser32'))

$winKeyTest = Join-Path $testsDir 'WinKeyBridgeIntegrationTest.exe'
Invoke-Compiler -Arguments (@(
    'windows-host\tests\WinKeyBridgeIntegrationTest.c'
) + $commonFlags + @('-o', $winKeyTest, '-luser32'))

$statusProtocolTest = Join-Path $testsDir 'StatusProtocolTest.exe'
Invoke-Compiler -Arguments (@(
    'windows-host\tests\StatusProtocolTest.c',
    'windows-host\src\WorkBuddyStatusProtocol.c'
) + $commonFlags + @('-pedantic', '-o', $statusProtocolTest))

$statusServerTest = Join-Path $testsDir 'StatusServerIntegrationTest.exe'
Invoke-Compiler -Arguments (@(
    'windows-host\tests\StatusServerIntegrationTest.c',
    'windows-host\src\WorkBuddyStatusProtocol.c',
    'windows-host\src\WorkBuddyStatusServer.c'
) + $commonFlags + @('-pedantic', '-o', $statusServerTest, '-lws2_32'))

if (-not $SkipInteractiveTests) {
    Invoke-TestExecutable -Path $altTabTest
    Invoke-TestExecutable -Path $winKeyTest
}
Invoke-TestExecutable -Path $statusProtocolTest
Invoke-TestExecutable -Path $statusServerTest

foreach ($firewallTest in @(
    'windows-host\tests\FirewallSpecTest.ps1',
    'windows-host\tests\FirewallApplySuccessTest.ps1',
    'windows-host\tests\FirewallApplyFailureTest.ps1'
)) {
    & pwsh -NoProfile -File $firewallTest
    if ($LASTEXITCODE -ne 0) {
        throw "Firewall test failed: $firewallTest"
    }
}

$pauseTest = Join-Path $testsDir 'BridgePauseIntegrationTest.exe'
Invoke-Compiler -Arguments (@(
    'windows-host\tests\BridgePauseIntegrationTest.c',
    'windows-host\src\KeyboardBridgeCore.c'
) + $commonFlags + @('-o', $pauseTest, '-luser32'))

if (-not $SkipInteractiveTests) {
    Invoke-TestExecutable -Path $pauseTest
}

$hostExe = Join-Path $packageDir 'WorkBuddyAltTabHost.exe'
Invoke-Compiler -Arguments (@(
    'windows-host\src\WorkBuddyAltTabHost.c',
    'windows-host\src\HostApplication.c',
    'windows-host\src\HostTrayIcon.c',
    'windows-host\src\KeyboardBridgeCore.c',
    'windows-host\src\WorkBuddyStatusProtocol.c',
    'windows-host\src\WorkBuddyStatusServer.c'
) + $commonFlags + @('-municode', '-mwindows', '-o', $hostExe, '-luser32', '-lws2_32', '-lshell32'))

Invoke-TestExecutable -Path $hostExe -Arguments @('--self-test')

Copy-Item -LiteralPath 'windows-host\install-status-firewall.ps1' -Destination $packageDir
$commit = (& git rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) {
    throw 'Unable to resolve Git commit for VERSION.txt'
}
$versionText = @"
WorkBuddy Bridge Windows Host
Version: $Version
Git commit: $commit
Architecture: x86_64
Authenticode: unsigned
"@
[IO.File]::WriteAllText(
    (Join-Path $packageDir 'VERSION.txt'),
    $versionText.Replace("`r`n", "`n"),
    [Text.UTF8Encoding]::new($false)
)

$zipPath = Join-Path $outputRoot "WorkBuddy-Bridge-Windows-$Version.zip"
Compress-Archive -Path (Join-Path $packageDir '*') -DestinationPath $zipPath -CompressionLevel Optimal
Write-Output $zipPath
