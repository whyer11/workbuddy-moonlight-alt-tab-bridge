[CmdletBinding()]
param(
    [string] $InstallDirectory = (Join-Path $env:USERPROFILE 'Applications\WorkBuddyAltTab'),
    # Explicitly disables login startup, including a previous installation's setting.
    [switch] $NoAutoStart,
    [switch] $NoStart
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskName = 'WorkBuddyAltTabHost'
$runKeyPath = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$currentSession = (Get-Process -Id $PID).SessionId

function Invoke-HostCommand {
    param([string] $Executable, [string] $Argument, [int[]] $AllowedExitCodes = @(0))
    $process = Start-Process -FilePath $Executable -ArgumentList $Argument -WorkingDirectory (Split-Path -Parent $Executable) -WindowStyle Hidden -Wait -PassThru
    if ($process.ExitCode -notin $AllowedExitCodes) {
        throw "Bridge command $Argument failed with exit code $($process.ExitCode)"
    }
    return $process.ExitCode
}

function Get-InstalledProcesses {
    @(Get-Process -Name 'WorkBuddyAltTabHost' -ErrorAction SilentlyContinue |
        Where-Object { $_.Path -ieq $targetExe -and $_.SessionId -eq $currentSession })
}

function Stop-InstalledHost {
    $processes = @(Get-InstalledProcesses)
    if ($processes.Count -eq 0) { return }
    # Use the validated release EXE: old installed versions ignore --quit.
    # The preflight rejects other installations in this session before this call.
    $null = Invoke-HostCommand -Executable $sourceExe -Argument '--quit' -AllowedExitCodes @(0, 12)
    foreach ($process in $processes) {
        if (-not $process.WaitForExit(1500)) {
            $stillRunning = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
            if ($null -ne $stillRunning -and $stillRunning.Path -ieq $targetExe -and $stillRunning.SessionId -eq $currentSession) {
                Stop-Process -InputObject $stillRunning -ErrorAction Stop
                if (-not $stillRunning.WaitForExit(5000)) {
                    throw "Unable to stop installed Host process $($process.Id)"
                }
            }
        }
    }
}

function Get-StartupSnapshot {
    $task = @(Get-ScheduledTask -TaskPath '\' -ErrorAction Stop |
        Where-Object { $_.TaskName -eq $taskName })
    $taskXml = if ($task.Count -gt 0) {
        Export-ScheduledTask -TaskName $taskName -TaskPath '\' -ErrorAction Stop
    } else { $null }
    $runKey = Get-Item -LiteralPath $runKeyPath -ErrorAction SilentlyContinue
    $runPresent = $null -ne $runKey -and @($runKey.GetValueNames()) -contains $taskName
    [pscustomobject]@{
        TaskXml = $taskXml
        RunPresent = $runPresent
        RunValue = $(if ($runPresent) { $runKey.GetValue($taskName, $null, [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames) } else { $null })
        RunKind = $(if ($runPresent) { $runKey.GetValueKind($taskName).ToString() } else { $null })
    }
}

function Restore-StartupSnapshot {
    param($Snapshot)
    if ($null -ne $Snapshot.TaskXml) {
        Register-ScheduledTask -TaskName $taskName -TaskPath '\' -Xml $Snapshot.TaskXml -Force -ErrorAction Stop | Out-Null
    } elseif (@(Get-ScheduledTask -TaskPath '\' -ErrorAction Stop | Where-Object { $_.TaskName -eq $taskName }).Count -gt 0) {
        Unregister-ScheduledTask -TaskName $taskName -TaskPath '\' -Confirm:$false -ErrorAction Stop
    }
    if ($Snapshot.RunPresent) {
        if ($null -eq (Get-Item -LiteralPath $runKeyPath -ErrorAction SilentlyContinue)) {
            New-Item -Path $runKeyPath -Force | Out-Null
        }
        New-ItemProperty -LiteralPath $runKeyPath -Name $taskName -Value $Snapshot.RunValue -PropertyType $Snapshot.RunKind -Force -ErrorAction Stop | Out-Null
    } else {
        $runKey = Get-Item -LiteralPath $runKeyPath -ErrorAction SilentlyContinue
        if ($null -ne $runKey -and @($runKey.GetValueNames()) -contains $taskName) {
            Remove-ItemProperty -LiteralPath $runKeyPath -Name $taskName -ErrorAction Stop
        }
    }
}

function Start-InstalledHost {
    param([string] $ExpectedVersion)
    $logOffset = if (Test-Path -LiteralPath $hostLog -PathType Leaf) { (Get-Item -LiteralPath $hostLog -ErrorAction Stop).Length } else { 0 }
    $process = Start-Process -FilePath $targetExe -WorkingDirectory $targetDirectory -WindowStyle Hidden -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    do {
        $process.Refresh()
        if ($process.HasExited) { throw "Installed Host exited during startup (exit code $($process.ExitCode))." }
        if (Test-Path -LiteralPath $hostLog -PathType Leaf) {
            $stream = [IO.File]::Open($hostLog, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
            try {
                $null = $stream.Seek([Math]::Min($logOffset, $stream.Length), [IO.SeekOrigin]::Begin)
                $reader = [IO.StreamReader]::new($stream)
                try { $newLog = $reader.ReadToEnd() } finally { $reader.Dispose() }
            } finally { $stream.Dispose() }
            if ($newLog -match ('(?m) HOST_READY version=' + [regex]::Escape($ExpectedVersion) + ' protocol=\d+ core=ready(?: |$)')) { return }
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    throw 'Installed Host did not report HOST_READY within 10 seconds.'
}

$sourceExe = Join-Path $PSScriptRoot 'WorkBuddyAltTabHost.exe'
$sourceVersion = Join-Path $PSScriptRoot 'VERSION.txt'
foreach ($required in @($sourceExe, $sourceVersion)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Release package file is missing: $required. Extract the complete Windows ZIP first."
    }
}
$versionMatch = [regex]::Match((Get-Content -LiteralPath $sourceVersion -Raw), '(?m)^Version: ([0-9]+\.[0-9]+\.[0-9]+)\s*$')
if (-not $versionMatch.Success) { throw 'Unable to read release version from VERSION.txt' }
$version = $versionMatch.Groups[1].Value
$null = Invoke-HostCommand -Executable $sourceExe -Argument '--self-test'
$targetDirectory = [IO.Path]::GetFullPath($InstallDirectory).TrimEnd('\')
if ($targetDirectory -eq [IO.Path]::GetPathRoot($targetDirectory).TrimEnd('\')) { throw 'The install directory must not be a drive root.' }
if ($targetDirectory -ieq [IO.Path]::GetFullPath($PSScriptRoot).TrimEnd('\')) { throw 'Extract the release ZIP to a separate directory before installing.' }
$targetExe = Join-Path $targetDirectory 'WorkBuddyAltTabHost.exe'
$hostLog = Join-Path $targetDirectory 'host.log'
$fileNames = @('WorkBuddyAltTabHost.exe', 'VERSION.txt', 'install.ps1', 'install-status-firewall.ps1')
foreach ($path in @($targetDirectory) + @($fileNames | ForEach-Object { Join-Path $targetDirectory $_ })) {
    if (Test-Path -LiteralPath $path) {
        if ((Get-Item -LiteralPath $path -ErrorAction Stop).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Installation target must not be a reparse point: $path" }
    }
}
$allProcesses = @(Get-Process -Name 'WorkBuddyAltTabHost' -ErrorAction SilentlyContinue)
if (@($allProcesses | Where-Object { $_.SessionId -eq $currentSession -and $_.Path -ine $targetExe }).Count -gt 0) {
    throw 'A different WorkBuddy Host installation is running in this session. Exit it before installing.'
}
if (@($allProcesses | Where-Object { $_.SessionId -ne $currentSession -and $_.Path -ieq $targetExe }).Count -gt 0) {
    throw 'This installation is in use by another Windows session. Exit that Host before upgrading.'
}
$wasRunning = @(Get-InstalledProcesses).Count -gt 0
$startupSnapshot = Get-StartupSnapshot
New-Item -ItemType Directory -Path $targetDirectory -Force | Out-Null
$backupDirectory = Join-Path $targetDirectory ('backups\' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
New-Item -ItemType Directory -Path $backupDirectory -Force | Out-Null
$originalFiles = @{}
foreach ($name in $fileNames) {
    $oldPath = Join-Path $targetDirectory $name
    $originalFiles[$name] = Test-Path -LiteralPath $oldPath -PathType Leaf
    if ($originalFiles[$name]) { Copy-Item -LiteralPath $oldPath -Destination (Join-Path $backupDirectory $name) }
}
if (Test-Path -LiteralPath $hostLog -PathType Leaf) { Copy-Item -LiteralPath $hostLog -Destination (Join-Path $backupDirectory 'host.log') }
$filesTouched = $false
$startupTouched = $false
try {
    Stop-InstalledHost
    $filesTouched = $true
    Copy-Item -LiteralPath $sourceExe -Destination $targetExe -Force
    if ((Get-FileHash -LiteralPath $sourceExe -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $targetExe -Algorithm SHA256).Hash) {
        throw 'Installed executable checksum does not match the release package.'
    }
    $null = Invoke-HostCommand -Executable $targetExe -Argument '--self-test'
    Copy-Item -LiteralPath $sourceVersion -Destination (Join-Path $targetDirectory 'VERSION.txt') -Force
    foreach ($name in @('install.ps1', 'install-status-firewall.ps1')) {
        $packageFile = Join-Path $PSScriptRoot $name
        if (Test-Path -LiteralPath $packageFile -PathType Leaf) { Copy-Item -LiteralPath $packageFile -Destination (Join-Path $targetDirectory $name) -Force }
    }
    $startupTouched = $true
    if ($NoAutoStart) {
        $null = Invoke-HostCommand -Executable $targetExe -Argument '--disable-autostart'
        $expectedStatus = 10
    } else {
        $null = Invoke-HostCommand -Executable $targetExe -Argument '--enable-autostart'
        $expectedStatus = 0
    }
    $actualStatus = Invoke-HostCommand -Executable $targetExe -Argument '--autostart-status' -AllowedExitCodes @(0, 10)
    if ($actualStatus -ne $expectedStatus) { throw 'Autostart state did not match the installation option.' }
    if (-not $NoStart) { Start-InstalledHost -ExpectedVersion $version }
} catch {
    $installationError = $_
    $rollbackErrors = [Collections.Generic.List[string]]::new()
    try { Stop-InstalledHost } catch { $rollbackErrors.Add("stop Host: $_") }
    if ($filesTouched) {
        foreach ($name in $fileNames) {
            try {
                $installedFile = Join-Path $targetDirectory $name
                if ($originalFiles[$name]) { Copy-Item -LiteralPath (Join-Path $backupDirectory $name) -Destination $installedFile -Force }
                elseif (Test-Path -LiteralPath $installedFile -PathType Leaf) { Remove-Item -LiteralPath $installedFile -Force }
            } catch { $rollbackErrors.Add("restore " + $name + ": $_") }
        }
    }
    if ($startupTouched) {
        try { Restore-StartupSnapshot $startupSnapshot } catch { $rollbackErrors.Add("restore startup: $_") }
    }
    if ($wasRunning -and @(Get-InstalledProcesses).Count -eq 0) {
        try { Start-Process -FilePath $targetExe -WorkingDirectory $targetDirectory -WindowStyle Hidden | Out-Null }
        catch { $rollbackErrors.Add("restart previous Host: $_") }
    }
    if ($rollbackErrors.Count -gt 0) { Write-Warning ("Rollback incomplete; backups: $backupDirectory. " + ($rollbackErrors -join '; ')) }
    throw $installationError
}
[pscustomobject]@{ Version = $version; Executable = $targetExe; AutoStart = (-not $NoAutoStart); TaskName = $taskName; Backup = $backupDirectory }
