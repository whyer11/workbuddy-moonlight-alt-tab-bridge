$ErrorActionPreference = 'Stop'

function Get-NetFirewallRule {
    param(
        [string]$DisplayName,
        [System.Management.Automation.ActionPreference]$ErrorAction
    )
    return @()
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
    # Simulate a firewall provider that returns without applying the requested rule.
}

$scriptPath = Join-Path $PSScriptRoot '..\install-status-firewall.ps1'
$failureMessage = ''
try {
    & $scriptPath | Out-Null
} catch {
    $failureMessage = $_.Exception.Message
}

if ($failureMessage -notmatch 'Firewall rule verification failed') {
    throw "Expected postcondition verification failure, got: $failureMessage"
}

Write-Output 'firewall_apply_failure_test: OK'
