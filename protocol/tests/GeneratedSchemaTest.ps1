$ErrorActionPreference = 'Stop'

$repository = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$schemaPath = Join-Path $repository 'protocol\bridge-schema.json'
$generatorPath = Join-Path $repository 'protocol\generate-schema.ps1'

if (-not (Test-Path -LiteralPath $schemaPath) -or -not (Test-Path -LiteralPath $generatorPath)) {
    throw 'Schema or generator is missing.'
}

$schema = Get-Content -Raw -LiteralPath $schemaPath | ConvertFrom-Json
$ruleIds = @($schema.rules | ForEach-Object { $_.id })
if (($ruleIds -join ',') -ne 'alt_tab,windows_key,arrow_keys') {
    throw "Unexpected rule order: $($ruleIds -join ',')"
}
if (($ruleIds | Sort-Object -Unique).Count -ne $ruleIds.Count) {
    throw 'Rule IDs must be unique.'
}

$temporary = Join-Path ([System.IO.Path]::GetTempPath()) ("workbuddy-schema-" + [guid]::NewGuid())
New-Item -ItemType Directory -Path $temporary | Out-Null
try {
    & $generatorPath -OutputRoot $temporary
    $outputs = @(
        'android-bridge\app\src\main\java\local\workbuddy\alttab\generated\GeneratedBridgeSchema.java',
        'native-bridge\src\generated_bridge_schema.h',
        'windows-host\src\GeneratedBridgeSchema.h'
    )
    foreach ($relativePath in $outputs) {
        $expected = Join-Path $repository $relativePath
        $actual = Join-Path $temporary $relativePath
        if (-not (Test-Path -LiteralPath $expected) -or -not (Test-Path -LiteralPath $actual)) {
            throw "Missing generated output: $relativePath"
        }
        $expectedBytes = [System.IO.File]::ReadAllBytes($expected)
        $actualBytes = [System.IO.File]::ReadAllBytes($actual)
        if (-not [System.Linq.Enumerable]::SequenceEqual[byte]($expectedBytes, $actualBytes)) {
            throw "Generated output is stale: $relativePath"
        }
    }
} finally {
    Remove-Item -LiteralPath $temporary -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Output 'generated_schema_test: OK'
