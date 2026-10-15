[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$installer = Join-Path $PSScriptRoot '..\install.ps1'
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('workbuddy-installer-test-' + [guid]::NewGuid().ToString('N'))
$originalLocalAppData = $env:LOCALAPPDATA
New-Item -ItemType Directory -Path $testRoot | Out-Null

function Assert-Equal($Actual, $Expected, [string] $Message) {
    if ($Actual -cne $Expected) { throw "$Message; expected <$Expected>, actual <$Actual>" }
}

function New-FakeProcess([string] $Path, [int] $Session = 1, [bool] $Exited = $false, [int] $ExitCode = 0) {
    $result = [pscustomobject]@{ Id = ++$global:workBuddyInstallerFixture.NextId; Path = $Path; SessionId = $Session; HasExited = $Exited; ExitCode = $ExitCode }
    $result | Add-Member ScriptMethod Refresh { }
    $result | Add-Member ScriptMethod WaitForExit { param([int] $Timeout) return $this.HasExited }
    $result | Add-Member ScriptMethod CloseMainWindow { return $false }
    return $result
}

# Only OS/process boundaries are simulated. The installer performs real copies,
# checksums, backups, log reads, and rollback in a newly created temporary tree.
function Get-Process {
    param([int] $Id, [string] $Name, $ErrorAction)
    if ($Id -eq $PID) { return [pscustomobject]@{ SessionId = 1 } }
    $global:workBuddyInstallerFixture.Processes | Where-Object { -not $_.HasExited -and (-not $Id -or $_.Id -eq $Id) }
}
function Stop-Process {
    param([int] $Id, $InputObject, $ErrorAction)
    if ($null -ne $InputObject) {
        $InputObject.HasExited = $true
        if ($global:workBuddyInstallerFixture.FailStopOnce) {
            $global:workBuddyInstallerFixture.FailStopOnce = $false
            throw 'Simulated failure after stopping the old process'
        }
        return
    }
    $global:workBuddyInstallerFixture.Processes | Where-Object { $_.Id -eq $Id } | ForEach-Object { $_.HasExited = $true }
}
function Start-Process {
    param([string] $FilePath, [string] $ArgumentList, [string] $WorkingDirectory, [string] $WindowStyle, [switch] $Wait, [switch] $PassThru)
    if ($ArgumentList) {
        $code = 0
        switch ($ArgumentList) {
            '--self-test' { }
            '--quit' {
                if (-not $global:workBuddyInstallerFixture.OldHostIgnoresQuit) {
                    $global:workBuddyInstallerFixture.Processes | Where-Object { $_.SessionId -eq 1 -and $_.Path -eq $global:workBuddyInstallerFixture.TargetExe } | ForEach-Object { $_.HasExited = $true }
                }
            }
            '--enable-autostart' {
                $global:workBuddyInstallerFixture.TaskXml = '<Task>new enabled task</Task>'
                $global:workBuddyInstallerFixture.RunPresent = $false
                if ($global:workBuddyInstallerFixture.FailConfig) { $code = 11 }
            }
            '--disable-autostart' { $global:workBuddyInstallerFixture.TaskXml = $null; $global:workBuddyInstallerFixture.RunPresent = $false }
            '--autostart-status' { if (-not $global:workBuddyInstallerFixture.TaskXml) { $code = 10 } }
            default { throw "Unexpected host argument $ArgumentList" }
        }
        return New-FakeProcess $FilePath -Exited $true -ExitCode $code
    }
    $oldVersion = [IO.File]::ReadAllText($FilePath) -eq 'old-exe'
    $failed = $global:workBuddyInstallerFixture.FailStart -and -not $oldVersion
    $process = New-FakeProcess $FilePath -Exited $failed -ExitCode $(if ($failed) { 5 } else { 0 })
    $global:workBuddyInstallerFixture.Processes.Add($process)
    if (-not $failed -and ($oldVersion -or -not $global:workBuddyInstallerFixture.DropReady)) {
        $version = if ($oldVersion) { '0.7.0' } else { '0.9.2' }
        [IO.File]::AppendAllText($global:workBuddyInstallerFixture.LogPath, "2026-10-15 12:00:00 HOST_READY version=$version protocol=2 core=ready status=ready tray=ready`n")
    }
    if ($PassThru) { return $process }
}
function Get-ScheduledTask {
    param([string] $TaskName, [string] $TaskPath, $ErrorAction)
    if ($global:workBuddyInstallerFixture.TaskXml) { [pscustomobject]@{ TaskName = 'WorkBuddyAltTabHost'; TaskPath = '\' } }
}
function Export-ScheduledTask {
    param([string] $TaskName, [string] $TaskPath, $ErrorAction)
    return $global:workBuddyInstallerFixture.TaskXml
}
function Register-ScheduledTask {
    param([string] $TaskName, [string] $TaskPath, [string] $Xml, [switch] $Force, $ErrorAction)
    $global:workBuddyInstallerFixture.TaskXml = $Xml
}
function Unregister-ScheduledTask {
    param([string] $TaskName, [string] $TaskPath, [switch] $Confirm, $ErrorAction)
    $global:workBuddyInstallerFixture.TaskXml = $null
}
function Get-Item {
    param([string] $LiteralPath, $ErrorAction)
    if ($LiteralPath -like 'HKCU:*') {
        $key = [pscustomobject]@{}
        $key | Add-Member ScriptMethod GetValueNames { if ($global:workBuddyInstallerFixture.RunPresent) { return @('WorkBuddyAltTabHost') }; return @() }
        $key | Add-Member ScriptMethod GetValue { param($Name, $Default, $Options) return $global:workBuddyInstallerFixture.RunValue }
        $key | Add-Member ScriptMethod GetValueKind { param($Name) return [Microsoft.Win32.RegistryValueKind]::String }
        return $key
    }
    Microsoft.PowerShell.Management\Get-Item -LiteralPath $LiteralPath -ErrorAction $ErrorAction
}
function New-ItemProperty {
    param($LiteralPath, $Path, $Name, $Value, $PropertyType, [switch] $Force, $ErrorAction)
    $global:workBuddyInstallerFixture.RunPresent = $true; $global:workBuddyInstallerFixture.RunValue = $Value
}
function Remove-ItemProperty {
    param($LiteralPath, $Path, $Name, $ErrorAction)
    $global:workBuddyInstallerFixture.RunPresent = $false
}

function New-Fixture([string] $Name, [switch] $Fresh) {
    $root = Join-Path $testRoot $Name
    $package = Join-Path $root 'package'
    $target = Join-Path $root 'installed'
    $env:LOCALAPPDATA = Join-Path $root 'profile'
    $logDirectory = $target
    foreach ($directory in @($package, $target, $logDirectory)) { New-Item -ItemType Directory -Path $directory -Force | Out-Null }
    Copy-Item -LiteralPath $installer -Destination (Join-Path $package 'install.ps1')
    [IO.File]::WriteAllText((Join-Path $package 'WorkBuddyAltTabHost.exe'), 'new-exe')
    [IO.File]::WriteAllText((Join-Path $package 'VERSION.txt'), "Version: 0.9.2`n")
    [IO.File]::WriteAllText((Join-Path $package 'install-status-firewall.ps1'), '# new firewall')
    $global:workBuddyInstallerFixture = [pscustomobject]@{
        Installer = (Join-Path $package 'install.ps1'); Target = $target
        TargetExe = (Join-Path $target 'WorkBuddyAltTabHost.exe'); LogPath = (Join-Path $logDirectory 'host.log')
        TaskXml = $(if ($Fresh) { $null } else { '<Task>old user task</Task>' }); RunPresent = (-not $Fresh)
        RunValue = '"old-registered-path.exe"'; FailConfig = $false; FailStart = $false; DropReady = $false
        OldHostIgnoresQuit = $false; FailStopOnce = $false; NextId = 2000
        Processes = [Collections.Generic.List[object]]::new()
    }
    [IO.File]::WriteAllText($global:workBuddyInstallerFixture.LogPath, "old log`n")
    if (-not $Fresh) {
        [IO.File]::WriteAllText($global:workBuddyInstallerFixture.TargetExe, 'old-exe')
        [IO.File]::WriteAllText((Join-Path $target 'VERSION.txt'), 'old-version')
        [IO.File]::WriteAllText((Join-Path $target 'install.ps1'), '# old installer')
        [IO.File]::WriteAllText((Join-Path $target 'install-status-firewall.ps1'), '# old firewall')
        $global:workBuddyInstallerFixture.Processes.Add((New-FakeProcess $global:workBuddyInstallerFixture.TargetExe))
    }
}
function Assert-RolledBack {
    Assert-Equal ([IO.File]::ReadAllText($global:workBuddyInstallerFixture.TargetExe)) 'old-exe' 'Old EXE must be restored'
    Assert-Equal ([IO.File]::ReadAllText((Join-Path $global:workBuddyInstallerFixture.Target 'VERSION.txt'))) 'old-version' 'Old version file must be restored'
    Assert-Equal ([IO.File]::ReadAllText((Join-Path $global:workBuddyInstallerFixture.Target 'install.ps1'))) '# old installer' 'Old installer must be restored'
    Assert-Equal $global:workBuddyInstallerFixture.TaskXml '<Task>old user task</Task>' 'Previous task must be restored'
    Assert-Equal $global:workBuddyInstallerFixture.RunPresent $true 'Previous Run value must be restored'
    Assert-Equal $global:workBuddyInstallerFixture.RunValue '"old-registered-path.exe"' 'Previous Run command must be restored'
    Assert-Equal @($global:workBuddyInstallerFixture.Processes | Where-Object { -not $_.HasExited }).Count 1 'Previous running state must be restored'
}

try {
    New-Fixture 'default'
    $result = & $global:workBuddyInstallerFixture.Installer -InstallDirectory $global:workBuddyInstallerFixture.Target
    Assert-Equal $result.Version '0.9.2' 'Installed version'
    Assert-Equal ([IO.File]::ReadAllText($global:workBuddyInstallerFixture.TargetExe)) 'new-exe' 'New EXE copied'
    Assert-Equal $global:workBuddyInstallerFixture.TaskXml '<Task>new enabled task</Task>' 'Default installation enables autostart'
    Assert-Equal $global:workBuddyInstallerFixture.RunPresent $false 'Legacy Run removed'
    Assert-Equal @($global:workBuddyInstallerFixture.Processes | Where-Object { -not $_.HasExited }).Count 1 'Exactly one new Host runs'
    Write-Output 'PASS default install enables autostart and starts the new Host'

    New-Fixture 'no-autostart'
    $null = & $global:workBuddyInstallerFixture.Installer -InstallDirectory $global:workBuddyInstallerFixture.Target -NoAutoStart -NoStart
    Assert-Equal $global:workBuddyInstallerFixture.TaskXml $null 'NoAutoStart disables task'
    Assert-Equal @($global:workBuddyInstallerFixture.Processes | Where-Object { -not $_.HasExited }).Count 0 'NoStart leaves Host stopped'
    Write-Output 'PASS explicit flags disable autostart and skip startup'

    New-Fixture 'config-failure'
    $global:workBuddyInstallerFixture.FailConfig = $true
    $caught = $false
    try { $null = & $global:workBuddyInstallerFixture.Installer -InstallDirectory $global:workBuddyInstallerFixture.Target } catch { $caught = $true }
    Assert-Equal $caught $true 'Configuration failure must be reported'
    Assert-RolledBack
    Write-Output 'PASS configuration failure restores files, task, Run, and running state'

    New-Fixture 'startup-failure'
    $global:workBuddyInstallerFixture.FailStart = $true
    $caught = $false
    try { $null = & $global:workBuddyInstallerFixture.Installer -InstallDirectory $global:workBuddyInstallerFixture.Target } catch { $caught = $true }
    Assert-Equal $caught $true 'Host initialization failure must fail installation'
    Assert-RolledBack
    Write-Output 'PASS Host startup failure restores the previous installation'

    New-Fixture 'fresh-failure' -Fresh
    $global:workBuddyInstallerFixture.FailConfig = $true
    $caught = $false
    try { $null = & $global:workBuddyInstallerFixture.Installer -InstallDirectory $global:workBuddyInstallerFixture.Target } catch { $caught = $true }
    Assert-Equal $caught $true 'Fresh install failure must be reported'
    Assert-Equal (Test-Path -LiteralPath $global:workBuddyInstallerFixture.TargetExe) $false 'Failed fresh install removes only new EXE'
    Assert-Equal $global:workBuddyInstallerFixture.TaskXml $null 'Failed fresh install removes new task'
    Assert-Equal $global:workBuddyInstallerFixture.RunPresent $false 'Failed fresh install does not create legacy Run'
    Write-Output 'PASS failed first installation removes newly installed files and task'

    New-Fixture 'other-install'
    $other = New-FakeProcess (Join-Path $testRoot 'another-install\WorkBuddyAltTabHost.exe')
    $global:workBuddyInstallerFixture.Processes.Add($other)
    $caught = $false
    try { $null = & $global:workBuddyInstallerFixture.Installer -InstallDirectory $global:workBuddyInstallerFixture.Target } catch { $caught = $true }
    Assert-Equal $caught $true 'Another active installation must be rejected'
    Assert-Equal $other.HasExited $false 'Another installation must never be stopped'
    Assert-Equal ([IO.File]::ReadAllText($global:workBuddyInstallerFixture.TargetExe)) 'old-exe' 'Conflict leaves original files unchanged'
    Write-Output 'PASS another installation is protected before stopping or copying'

    New-Fixture 'stale-ready'
    $global:workBuddyInstallerFixture.DropReady = $true
    [IO.File]::AppendAllText($global:workBuddyInstallerFixture.LogPath, "2026-10-15 11:00:00 HOST_READY version=0.9.2 protocol=2 core=ready status=ready`n")
    $caught = $false
    try { $null = & $global:workBuddyInstallerFixture.Installer -InstallDirectory $global:workBuddyInstallerFixture.Target } catch { $caught = $_.Exception.Message -like '*did not report HOST_READY*' }
    Assert-Equal $caught $true 'A historical HOST_READY must not count as a successful new start'
    Assert-RolledBack
    Write-Output 'PASS historical Ready log cannot hide a failed new startup'

    New-Fixture 'stop-failure'
    $global:workBuddyInstallerFixture.OldHostIgnoresQuit = $true
    $global:workBuddyInstallerFixture.FailStopOnce = $true
    $caught = $false
    try { $null = & $global:workBuddyInstallerFixture.Installer -InstallDirectory $global:workBuddyInstallerFixture.Target } catch { $caught = $true }
    Assert-Equal $caught $true 'Stopping the old process failure must be reported'
    Assert-RolledBack
    Write-Output 'PASS failure while stopping the old Host restores its running state'

    New-Fixture 'other-session'
    $other = New-FakeProcess $global:workBuddyInstallerFixture.TargetExe -Session 2
    $global:workBuddyInstallerFixture.Processes.Add($other)
    $caught = $false
    try { $null = & $global:workBuddyInstallerFixture.Installer -InstallDirectory $global:workBuddyInstallerFixture.Target } catch { $caught = $true }
    Assert-Equal $caught $true 'Same files in use in another session must be rejected'
    Assert-Equal $other.HasExited $false 'Another session must never be stopped'
    Assert-Equal ([IO.File]::ReadAllText($global:workBuddyInstallerFixture.TargetExe)) 'old-exe' 'Other session conflict leaves files unchanged'
    Write-Output 'PASS another Windows session is protected before stopping or copying'
    Write-Output 'INSTALLER_TESTS_OK cases=9'
} finally {
    $env:LOCALAPPDATA = $originalLocalAppData
    # This exact temporary directory was created by this test run.
    if ($testRoot -like (Join-Path ([IO.Path]::GetTempPath()) 'workbuddy-installer-test-*')) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }
    Remove-Variable -Name workBuddyInstallerFixture -Scope Global -ErrorAction SilentlyContinue
}
