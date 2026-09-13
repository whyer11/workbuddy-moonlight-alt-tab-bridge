[CmdletBinding(SupportsShouldProcess)]
param(
    [switch]$DescribeOnly
)

$ErrorActionPreference = 'Stop'

$desired = [pscustomobject]@{
    DisplayName   = 'WorkBuddy Bridge Status (Tailscale)'
    Direction     = 'Inbound'
    Action        = 'Allow'
    Protocol      = 'UDP'
    LocalPort     = 57420
    RemoteAddress = '100.64.0.0/10'
    Profile       = 'Any'
}

if ($DescribeOnly) {
    $desired
    return
}

function Test-DesiredRule {
    param(
        [Parameter(Mandatory)]$Rule
    )

    $portFilter = $Rule | Get-NetFirewallPortFilter
    $addressFilter = $Rule | Get-NetFirewallAddressFilter
    $remoteAddresses = @($addressFilter.RemoteAddress)

    return $Rule.Enabled.ToString() -eq 'True' `
        -and $Rule.Direction.ToString() -eq $desired.Direction `
        -and $Rule.Action.ToString() -eq $desired.Action `
        -and $Rule.Profile.ToString() -eq $desired.Profile `
        -and $portFilter.Protocol.ToString() -in @('UDP', '17') `
        -and $portFilter.LocalPort.ToString() -eq $desired.LocalPort.ToString() `
        -and $remoteAddresses.Count -eq 1 `
        -and $remoteAddresses[0].ToString() -in @(
            $desired.RemoteAddress,
            '100.64.0.0/255.192.0.0'
        )
}

$existing = @(Get-NetFirewallRule -DisplayName ($desired.DisplayName) -ErrorAction SilentlyContinue)
if ($existing.Count -eq 1 -and (Test-DesiredRule -Rule $existing[0])) {
    Write-Output 'WorkBuddy status firewall rule is already configured.'
    return
}

if ($existing.Count -gt 0 -and $PSCmdlet.ShouldProcess(
        $desired.DisplayName, 'Remove mismatched firewall rules')) {
    $existing | Remove-NetFirewallRule -ErrorAction Stop
}

if ($PSCmdlet.ShouldProcess($desired.DisplayName, 'Create Tailscale-scoped inbound rule')) {
    New-NetFirewallRule `
        -DisplayName ($desired.DisplayName) `
        -Description 'Read-only WorkBuddy keyboard bridge status query' `
        -Direction ($desired.Direction) `
        -Action ($desired.Action) `
        -Protocol ($desired.Protocol) `
        -LocalPort ($desired.LocalPort) `
        -RemoteAddress ($desired.RemoteAddress) `
        -Profile ($desired.Profile) `
        -Enabled True `
        -ErrorAction Stop | Out-Null

    $verified = @(Get-NetFirewallRule `
            -DisplayName ($desired.DisplayName) `
            -ErrorAction Stop)
    if ($verified.Count -ne 1 -or -not (Test-DesiredRule -Rule $verified[0])) {
        throw 'Firewall rule verification failed after creation.'
    }
    Write-Output 'WorkBuddy status firewall rule was created.'
}
