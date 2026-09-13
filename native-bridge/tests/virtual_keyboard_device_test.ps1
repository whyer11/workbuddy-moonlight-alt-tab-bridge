param(
    [Parameter(Mandatory)]
    [string] $AdbPath,

    [Parameter(Mandatory)]
    [string] $Target
)

$ErrorActionPreference = 'Stop'
$deviceName = 'WorkBuddy Virtual Keyboard'
$dump = ((& $AdbPath -s $Target shell dumpsys input) | Out-String)
if ($LASTEXITCODE -ne 0) {
    throw 'Could not read Android input state.'
}

$devicePattern = '(?ms)^\s+\d+: ' + [regex]::Escape($deviceName) + '\r?\n(?<Body>.*?)(?=^\s+\d+: |\z)'
$deviceMatch = [regex]::Match($dump, $devicePattern)
if (-not $deviceMatch.Success) {
    throw "Input device '$deviceName' was not found."
}

$body = $deviceMatch.Groups['Body'].Value
$classesMatch = [regex]::Match($body, '(?m)^\s+Classes:\s*(?<Classes>.+)$')
$pathMatch = [regex]::Match($body, '(?m)^\s+Path:\s*(?<Path>/dev/input/event\d+)\s*$')
if (-not $classesMatch.Success -or -not $pathMatch.Success) {
    throw "Input device '$deviceName' is missing Classes or Path metadata."
}

$classes = $classesMatch.Groups['Classes'].Value.Trim()
if ($classes -notmatch '(^|\|\s*)KEYBOARD(\s*\||$)') {
    throw "Virtual input device is not classified as a keyboard: $classes"
}
if ($classes -match 'GAMEPAD|STYLUS') {
    throw "Virtual keyboard leaks non-keyboard capabilities: $classes"
}

$eventPath = $pathMatch.Groups['Path'].Value
$capabilities = ((& $AdbPath -s $Target shell getevent -il $eventPath) | Out-String)
if ($LASTEXITCODE -ne 0) {
    throw "Could not inspect virtual keyboard capabilities at $eventPath."
}
foreach ($requiredKey in @(
    'KEY_UP', 'KEY_DOWN', 'KEY_LEFT', 'KEY_RIGHT',
    'KEY_LEFTALT', 'KEY_TAB', 'KEY_F12', 'KEY_SCROLLLOCK'
)) {
    if ($capabilities -notmatch "\b$requiredKey\b") {
        throw "Virtual keyboard is missing required capability: $requiredKey"
    }
}

"VIRTUAL_KEYBOARD_DEVICE_TEST=PASS path=$eventPath classes=$classes"
