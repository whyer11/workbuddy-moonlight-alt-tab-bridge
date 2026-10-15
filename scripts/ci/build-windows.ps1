param(
    [string] $Compiler = 'gcc',
    [string] $Version = $env:RELEASE_VERSION,
    [switch] $SkipInteractiveTests,
    [switch] $RunAutostartIntegrationTests
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
$productVersion = (Get-Content -LiteralPath 'protocol\bridge-schema.json' -Raw | ConvertFrom-Json).productVersion

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

    $stdout = "$Path.stdout.txt"
    $stderr = "$Path.stderr.txt"
    $startOptions = @{
        FilePath = $Path
        WindowStyle = 'Hidden'
        Wait = $true
        PassThru = $true
        RedirectStandardOutput = $stdout
        RedirectStandardError = $stderr
    }
    if ($Arguments.Count -gt 0) { $startOptions.ArgumentList = $Arguments }
    $process = Start-Process @startOptions
    Get-Content -LiteralPath $stdout -ErrorAction SilentlyContinue
    Get-Content -LiteralPath $stderr -ErrorAction SilentlyContinue
    if ($process.ExitCode -ne 0) {
        throw "Test failed with exit code $($process.ExitCode): $Path"
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
    'windows-host\tests\FirewallApplyFailureTest.ps1',
    'windows-host\tests\InstallerTest.ps1'
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
$autostartTest = Join-Path $testsDir 'HostAutostartIntegrationTest.exe'
Invoke-Compiler -Arguments (@(
    'windows-host\tests\HostAutostartIntegrationTest.c',
    'windows-host\src\HostAutostart.c'
) + $commonFlags + @('-o', $autostartTest, '-lole32', '-loleaut32', '-luuid', '-ladvapi32'))
if ($RunAutostartIntegrationTests) {
    Invoke-TestExecutable -Path $autostartTest -Arguments @('--live')
}

$resourceObject = Join-Path $outputRoot 'host_rc.o'
$windres = Get-Command windres -ErrorAction SilentlyContinue
if ($null -eq $windres) {
    throw 'windres was not found; MinGW-w64 binutils is required for tray icon resources'
}
& windres 'windows-host\src\host.rc' -O coff -o $resourceObject
if ($LASTEXITCODE -ne 0) {
    throw "windres failed with exit code $LASTEXITCODE"
}
Invoke-Compiler -Arguments (@(
    'windows-host\src\WorkBuddyAltTabHost.c',
    'windows-host\src\HostApplication.c',
    'windows-host\src\HostTrayIcon.c',
    'windows-host\src\HostAutostart.c',
    'windows-host\src\KeyboardBridgeCore.c',
    'windows-host\src\WorkBuddyStatusProtocol.c',
    'windows-host\src\WorkBuddyStatusServer.c',
    $resourceObject
) + $commonFlags + @('-municode', '-mwindows', '-o', $hostExe, '-luser32', '-lws2_32', '-lshell32', '-lole32', '-loleaut32', '-luuid', '-ladvapi32'))

Invoke-TestExecutable -Path $hostExe -Arguments @('--self-test')
foreach ($testLog in @("$hostExe.stdout.txt", "$hostExe.stderr.txt")) {
    Move-Item -LiteralPath $testLog -Destination $testsDir -Force
}

Copy-Item -LiteralPath 'windows-host\install-status-firewall.ps1' -Destination $packageDir
Copy-Item -LiteralPath 'windows-host\install.ps1' -Destination $packageDir
$commit = (& git rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) {
    throw 'Unable to resolve Git commit for VERSION.txt'
}
$versionText = @"
WorkBuddy Bridge Windows Host
Version: $productVersion
Git commit: $commit
Architecture: x86_64
Authenticode: unsigned
"@
[IO.File]::WriteAllText(
    (Join-Path $packageDir 'VERSION.txt'),
    $versionText.Replace("`r`n", "`n"),
    [Text.UTF8Encoding]::new($false)
)

& pwsh -NoProfile -File 'windows-host\tests\WindowsPackageTest.ps1' -PackageDirectory $packageDir
if ($LASTEXITCODE -ne 0) { throw 'Windows package validation failed' }

$zipPath = Join-Path $outputRoot "WorkBuddy-Bridge-Windows-$Version.zip"
Compress-Archive -Path (Join-Path $packageDir '*') -DestinationPath $zipPath -CompressionLevel Optimal
Write-Output $zipPath
