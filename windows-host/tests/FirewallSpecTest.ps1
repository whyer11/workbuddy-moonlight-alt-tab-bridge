$ErrorActionPreference = 'Stop'

$scriptPath = Join-Path $PSScriptRoot '..\install-status-firewall.ps1'
$scriptText = [System.IO.File]::ReadAllText((Resolve-Path $scriptPath))
if ($scriptText -match '[^\x00-\x7F]') {
    throw 'Firewall installer must remain ASCII for Windows PowerShell 5.1 compatibility'
}

$spec = & $scriptPath -DescribeOnly

if ($spec.DisplayName -ne 'WorkBuddy Bridge Status (Tailscale)') {
    throw "Unexpected firewall rule name: $($spec.DisplayName)"
}
if ($spec.Direction -ne 'Inbound' -or $spec.Action -ne 'Allow') {
    throw 'Firewall rule must be inbound allow'
}
if ($spec.Protocol -ne 'UDP' -or $spec.LocalPort -ne 57420) {
    throw 'Firewall rule must allow UDP 57420'
}
if ($spec.RemoteAddress -ne '100.64.0.0/10') {
    throw 'Firewall rule must be limited to Tailscale CGNAT'
}
if ($spec.Profile -ne 'Any') {
    throw 'Firewall rule must work regardless of the Tailscale adapter profile'
}

Write-Output 'firewall_spec_test: OK'
