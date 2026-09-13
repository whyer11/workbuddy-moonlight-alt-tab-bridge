$ErrorActionPreference = 'Stop'

$global:WorkBuddyFirewallTestCreated = $false

function Get-NetFirewallRule {
    param(
        [string]$DisplayName,
        [System.Management.Automation.ActionPreference]$ErrorAction
    )
    if (-not $global:WorkBuddyFirewallTestCreated) {
        return @()
    }
    return [pscustomobject]@{
        Enabled = $true
        Direction = 'Inbound'
        Action = 'Allow'
        Profile = 'Any'
    }
}

function New-NetFirewallRule {
    param(
        [string]$DisplayName,
        [string]$Description,
        [string]$Direction,
        [string]$Action,
        [string]$Protocol,
        [int]$LocalPort,
        [string]$RemoteAddress,
        [string]$Profile,
        [string]$Enabled
    )
    $global:WorkBuddyFirewallTestCreated = $true
}

function Get-NetFirewallPortFilter {
    return [pscustomobject]@{
        Protocol = 'UDP'
        LocalPort = '57420'
    }
}

function Get-NetFirewallAddressFilter {
    return [pscustomobject]@{
        RemoteAddress = '100.64.0.0/255.192.0.0'
    }
}

$scriptPath = Join-Path $PSScriptRoot '..\install-status-firewall.ps1'
$result = & $scriptPath

if (-not $global:WorkBuddyFirewallTestCreated) {
    throw 'Expected the firewall rule to be created'
}
if ($result -notcontains 'WorkBuddy status firewall rule was created.') {
    throw "Expected successful creation, got: $result"
}

Write-Output 'firewall_apply_success_test: OK'
