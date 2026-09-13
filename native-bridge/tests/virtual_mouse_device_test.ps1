param(
    [Parameter(Mandatory)]
    [string] $AdbPath,

    [Parameter(Mandatory)]
    [string] $Target
)

$ErrorActionPreference = 'Stop'
$deviceName = 'WorkBuddy Virtual Mouse'
$dump = ((& $AdbPath -s $Target shell dumpsys input) | Out-String)
if ($LASTEXITCODE -ne 0) {
    throw 'Could not read Android input state.'
}

$devicePattern = '(?ms)^\s+\d+: ' + [regex]::Escape($deviceName) `
    + '\r?\n(?<Body>.*?)(?=^\s+\d+: |\z)'
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
if ($classes -notmatch '(^|\|\s*)CURSOR(\s*\||$)') {
    throw "Virtual input device is not classified as a cursor: $classes"
}
if ($classes -match 'KEYBOARD|GAMEPAD|STYLUS|TOUCH|TOUCH_MT|TOUCHPAD') {
    throw "Virtual mouse leaks non-mouse classes: $classes"
}

$eventPath = $pathMatch.Groups['Path'].Value
$capabilities = ((& $AdbPath -s $Target shell getevent -il $eventPath) | Out-String)
if ($LASTEXITCODE -ne 0) {
    throw "Could not inspect virtual mouse capabilities at $eventPath."
}
if ($capabilities -notmatch '\b(BTN_MOUSE|BTN_LEFT)\b') {
    throw 'Virtual mouse is missing left-button capability (BTN_MOUSE/BTN_LEFT).'
}
foreach ($required in @('REL_X', 'REL_Y')) {
    if ($capabilities -notmatch "\b$required\b") {
        throw "Virtual mouse is missing required capability: $required"
    }
}
foreach ($forbidden in @(
    'BTN_RIGHT', 'BTN_MIDDLE', 'ABS_MT_POSITION_X', 'ABS_MT_POSITION_Y'
)) {
    if ($capabilities -match "\b$forbidden\b") {
        throw "Virtual mouse leaks forbidden capability: $forbidden"
    }
}

"VIRTUAL_MOUSE_DEVICE_TEST=PASS path=$eventPath classes=$classes"
