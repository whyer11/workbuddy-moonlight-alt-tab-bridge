# Moonlight Three-Finger Drag Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add macOS-like three-finger left-button dragging for the Xiaomi Pad physical touchpad while preserving every existing keyboard bridge behavior.

**Architecture:** Keep the current APK, Shizuku UserService, native supervisor, and single native process. Add a pure C gesture state machine plus a Linux evdev/uinput adapter inside the native process; read `Xiaomi Touch` passively and emit only `BTN_LEFT`, `REL_X`, and `REL_Y` through a dedicated virtual mouse.

**Tech Stack:** C11, Linux evdev/uinput, Android NDK 29, Java/Android 36, Shizuku 13.1.5, PowerShell, JUnit 4, ADB.

**Spec:** `docs/superpowers/specs/2026-09-13-three-finger-drag-design.md`

## Global Constraints

- Product version is `0.8.0`; Android `versionCode` is `10`; capability/status protocol remains version `2`.
- Preserve package `local.workbuddy.alttab`, the current signing certificate, Shizuku permission, boot recovery, computer host setting, and native `.new/.previous` rollback flow.
- Never call `EVIOCGRAB` on `Xiaomi Touch`; only the existing physical keyboard remains grabbed.
- The virtual mouse declares exactly `BTN_LEFT`, `REL_X`, and `REL_Y` plus synchronization events.
- Initial activation distance is `48` raw coordinate units and movement scale is `1/1`.
- Any exit, pause, device loss, or read error releases an injected left button before destroying the virtual mouse.
- Do not modify Moonlight, Sunshine, Tailscale, or the installed Windows Host.

## File Map

**Create**

- `native-bridge/src/touchpad_drag_state.h`: platform-independent gesture state and output interface.
- `native-bridge/src/touchpad_drag_state.c`: exact-three-finger state machine and centroid-to-relative-motion conversion.
- `native-bridge/src/touchpad_bridge.h`: native touchpad adapter lifecycle interface used by the process entry point.
- `native-bridge/src/touchpad_bridge.c`: `Xiaomi Touch` discovery, evdev decoding, virtual mouse creation, injection, and safe cleanup.
- `native-bridge/tests/touchpad_drag_state_test.c`: deterministic unit tests for activation, movement, cancellation, and release.
- `native-bridge/tests/virtual_mouse_device_test.ps1`: Pad-side capability and classification test for the uinput device.
- `docs/verification/2026-09-13-three-finger-drag.md`: automated and real-device acceptance record.

**Modify**

- `protocol/bridge-schema.json`: version `0.8.0` and `three_finger_drag` Pad-only rule.
- `protocol/tests/GeneratedSchemaTest.ps1`: expected four-rule order.
- Generated outputs: `GeneratedBridgeSchema.java`, `generated_bridge_schema.h`, `GeneratedBridgeSchema.h`.
- `android-bridge/app/build.gradle`: versionCode `10`, versionName `0.8.0`.
- `BridgeRules.java` and `BridgeRulesTest.java`: expose and verify the new rule.
- `DashboardStateReducerTest.java`: verify the rule is Pad-only and participates in Pad capability health.
- `native_protocol_test.c`: expect 0.8.0 and the fourth native rule.
- `alttab_evdev_bridge.c`: create the touchpad adapter, poll both input devices, and use one cleanup path.
- `README.md`: document the fourth rule and its initial behavior.

Windows Host implementation files are not edited. `windows-host/src/GeneratedBridgeSchema.h` changes only because it is generated from the shared schema; `WB_SCHEMA_WINDOWS_RULES` remains `alt_tab,windows_key`.

---

### Task 1: Publish the 0.8.0 capability and dashboard rule

**Files:**

- Modify: `protocol/bridge-schema.json`
- Modify: `protocol/tests/GeneratedSchemaTest.ps1`
- Modify: `android-bridge/app/build.gradle`
- Modify: `android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeRules.java`
- Modify: `android-bridge/app/src/test/java/local/workbuddy/alttab/BridgeRulesTest.java`
- Modify: `android-bridge/app/src/test/java/local/workbuddy/alttab/domain/DashboardStateReducerTest.java`
- Regenerate: `android-bridge/app/src/main/java/local/workbuddy/alttab/generated/GeneratedBridgeSchema.java`
- Regenerate: `native-bridge/src/generated_bridge_schema.h`
- Regenerate: `windows-host/src/GeneratedBridgeSchema.h`

**Interfaces:**

- Produces rule ID `three_finger_drag` in Java and native Pad capability lists.
- Leaves `WB_SCHEMA_WINDOWS_RULES` exactly `alt_tab,windows_key`.

- [ ] **Step 1: Change tests first**

In `GeneratedSchemaTest.ps1`, require this exact order:

```powershell
if (($ruleIds -join ',') -ne 'alt_tab,windows_key,arrow_keys,three_finger_drag') {
    throw "Unexpected rule order: $($ruleIds -join ',')"
}
```

In `BridgeRulesTest.java`, expect the fourth rule and its complete presentation contract:

```java
assertEquals(Arrays.asList("alt_tab", "windows_key", "arrow_keys", "three_finger_drag"),
        rules.stream().map(BridgeRule::id).collect(Collectors.toList()));
assertEquals("三指移动", rules.get(3).padInput());
assertEquals("虚拟鼠标左键拖动", rules.get(3).padOutput());
assertEquals("Windows 左键拖动", rules.get(3).windowsAction());
assertFalse(rules.get(3).requiresComputer());
```

In `DashboardStateReducerTest.java`, include `three_finger_drag` in ready Pad snapshots and add:

```java
assertEquals(EndpointState.READY, state.rule("three_finger_drag").pad());
assertEquals(EndpointState.NOT_REQUIRED, state.rule("three_finger_drag").computer());
```

- [ ] **Step 2: Run the red tests**

```powershell
& .\protocol\tests\GeneratedSchemaTest.ps1
$env:JAVA_HOME = (Resolve-Path '.\.tools\jdk-extract\jdk-17.0.20.1+1')
& '.\.tools\gradle-extract\gradle-9.6.0\bin\gradle.bat' -p android-bridge :app:testDebugUnitTest --tests local.workbuddy.alttab.BridgeRulesTest --tests local.workbuddy.alttab.domain.DashboardStateReducerTest --no-daemon
```

Expected: schema order and Java catalog assertions fail because the fourth rule does not exist.

- [ ] **Step 3: Add the schema rule and release version**

Set `productVersion` to `0.8.0`, then append this rule without changing the existing three:

```json
{"id":"three_finger_drag","title":"三指拖动","padInput":"三指移动","padOutput":"虚拟鼠标左键拖动","windowsAction":"Windows 左键拖动","pad":true,"windows":false}
```

Set `android-bridge/app/build.gradle` to:

```groovy
versionCode 10
versionName '0.8.0'
```

Add to `BridgeRules.java`:

```java
public static final String THREE_FINGER_DRAG = "three_finger_drag";
```

Regenerate all schema consumers:

```powershell
& .\protocol\generate-schema.ps1
```

- [ ] **Step 4: Run the green tests**

Run the two commands from Step 2 again.

Expected: `generated_schema_test: OK`; both JUnit classes pass. Inspect `windows-host/src/GeneratedBridgeSchema.h` and confirm `WB_SCHEMA_WINDOWS_RULES` did not acquire `three_finger_drag`.

- [ ] **Step 5: Commit the capability contract**

```powershell
git add protocol android-bridge/app/build.gradle android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeRules.java android-bridge/app/src/main/java/local/workbuddy/alttab/generated/GeneratedBridgeSchema.java android-bridge/app/src/test/java/local/workbuddy/alttab/BridgeRulesTest.java android-bridge/app/src/test/java/local/workbuddy/alttab/domain/DashboardStateReducerTest.java native-bridge/src/generated_bridge_schema.h windows-host/src/GeneratedBridgeSchema.h
git commit -m "feat: declare three-finger drag capability"
```

---

### Task 2: Implement the pure three-finger drag state machine

**Files:**

- Create: `native-bridge/src/touchpad_drag_state.h`
- Create: `native-bridge/src/touchpad_drag_state.c`
- Create: `native-bridge/tests/touchpad_drag_state_test.c`

**Interfaces:**

- Consumes semantic slot/tracking/coordinate updates, independent of Linux headers.
- Produces `wb_touchpad_drag_finish_frame()` and `wb_touchpad_drag_cancel()` outputs.

- [ ] **Step 1: Define the failing behavior tests**

Create a test harness with helpers `begin_contact(struct wb_touchpad_drag_state *, int slot, int tracking_id, int x, int y)`, `move_contact(struct wb_touchpad_drag_state *, int slot, int x, int y)`, and `end_contact(struct wb_touchpad_drag_state *, int slot)`. Cover these exact cases:

```c
static void test_three_finger_tap_does_nothing(void);
static void test_threshold_crossing_presses_then_moves(void);
static void test_drag_continues_without_repeating_button_down(void);
static void test_lifting_one_finger_releases_button(void);
static void test_fourth_finger_cancels_and_releases(void);
static void test_replacing_a_finger_releases_before_rearming(void);
static void test_cancel_releases_only_an_active_drag(void);
```

Use three initial coordinates `(300,300)`, `(600,300)`, `(900,300)`. Moving all three by 47 units must remain armed; moving them by the 48th unit must return `WB_DRAG_LEFT_DOWN | WB_DRAG_MOVE`. The next 12-by-6 move must return one `MOVE` with `dx=12`, `dy=6`.

- [ ] **Step 2: Compile to verify the test is red**

```powershell
New-Item -ItemType Directory -Force .tools\three-finger-drag | Out-Null
gcc native-bridge\tests\touchpad_drag_state_test.c native-bridge\src\touchpad_drag_state.c -std=c11 -Wall -Wextra -Werror -pedantic -o .tools\three-finger-drag\touchpad_drag_state_test.exe
```

Expected: compilation fails because the state-machine files and symbols do not exist.

- [ ] **Step 3: Define the state-machine API**

Create `touchpad_drag_state.h` with these public types and functions:

```c
#define WB_TOUCHPAD_MAX_SLOTS 8

enum wb_drag_action {
    WB_DRAG_NONE = 0,
    WB_DRAG_LEFT_DOWN = 1U << 0,
    WB_DRAG_MOVE = 1U << 1,
    WB_DRAG_LEFT_UP = 1U << 2
};

struct wb_touchpad_drag_config {
    int activation_distance;
    int movement_numerator;
    int movement_denominator;
};

struct wb_touchpad_drag_output {
    unsigned int actions;
    int dx;
    int dy;
};

enum wb_touchpad_drag_phase {
    WB_TOUCHPAD_IDLE,
    WB_TOUCHPAD_ARMED,
    WB_TOUCHPAD_DRAGGING
};

struct wb_touchpad_slot {
    int tracking_id;
    int x;
    int y;
    bool active;
    bool has_x;
    bool has_y;
};

struct wb_touchpad_drag_state {
    struct wb_touchpad_slot slots[WB_TOUCHPAD_MAX_SLOTS];
    int selected_slot;
    enum wb_touchpad_drag_phase phase;
    int start_x;
    int start_y;
    int last_x;
    int last_y;
    bool composition_changed;
    struct wb_touchpad_drag_config config;
};

void wb_touchpad_drag_init(struct wb_touchpad_drag_state *state,
                           struct wb_touchpad_drag_config config);
void wb_touchpad_drag_select_slot(struct wb_touchpad_drag_state *state, int slot);
void wb_touchpad_drag_set_tracking_id(struct wb_touchpad_drag_state *state, int tracking_id);
void wb_touchpad_drag_set_x(struct wb_touchpad_drag_state *state, int x);
void wb_touchpad_drag_set_y(struct wb_touchpad_drag_state *state, int y);
struct wb_touchpad_drag_output wb_touchpad_drag_finish_frame(
        struct wb_touchpad_drag_state *state);
struct wb_touchpad_drag_output wb_touchpad_drag_cancel(
        struct wb_touchpad_drag_state *state);
```

Include `<stdbool.h>` in the header. The concrete state is exposed because it is stack-owned; do not allocate memory dynamically.

- [ ] **Step 4: Implement the minimum state transitions**

In `touchpad_drag_state.c`:

- `tracking_id >= 0` activates the selected slot and clears its coordinate-valid flags when the ID changes.
- `tracking_id == -1` clears that slot.
- Any tracking-ID change sets `composition_changed`; `finish_frame` clears this flag after processing.
- At `finish_frame`, count every active tracking ID before checking coordinate validity. This guarantees that a fourth finger cancels immediately even if its coordinates have not arrived yet.
- A count other than exactly three releases an active drag and resets to `IDLE`.
- If an active three-finger composition changes while dragging, release and return to `IDLE`; a later unchanged complete frame may arm again.
- When the count is exactly three but any coordinate is incomplete, emit no action for that frame.
- The first complete three-finger frame arms and stores the integer centroid.
- Compare squared distance with `activation_distance * activation_distance`; do not use floating point or `sqrt()`.
- On threshold crossing, return `LEFT_DOWN | MOVE` and include the complete start-to-current delta.
- During dragging, return scaled last-to-current deltas; omit `MOVE` when both are zero.
- `cancel` returns `LEFT_UP` only when dragging, then clears all slots and resets the phase.

Validate config in `init`: replace non-positive threshold, numerator, or denominator with `48`, `1`, and `1` respectively.

- [ ] **Step 5: Run the unit test green**

Run the compile command from Step 2, then:

```powershell
& .\.tools\three-finger-drag\touchpad_drag_state_test.exe
```

Expected: `touchpad_drag_state_test: OK`.

- [ ] **Step 6: Commit the deterministic gesture core**

```powershell
git add native-bridge/src/touchpad_drag_state.c native-bridge/src/touchpad_drag_state.h native-bridge/tests/touchpad_drag_state_test.c
git commit -m "feat: add three-finger drag state machine"
```

---

### Task 3: Add the passive evdev and virtual-mouse adapter

**Files:**

- Create: `native-bridge/src/touchpad_bridge.h`
- Create: `native-bridge/src/touchpad_bridge.c`
- Create: `native-bridge/tests/virtual_mouse_device_test.ps1`

**Interfaces:**

- Consumes `wb_touchpad_drag_*` from Task 2 and Linux input events from `Xiaomi Touch`.
- Produces a self-contained adapter with `open`, `fd`, `drain`, `path`, and `close` operations.

- [ ] **Step 1: Write the Pad capability test first**

Create `virtual_mouse_device_test.ps1` with parameters `AdbPath` and `Target`. Read `dumpsys input`, find `WorkBuddy Virtual Mouse`, require class `CURSOR`, reject `KEYBOARD|GAMEPAD|STYLUS`, resolve its `/dev/input/eventN`, then run `getevent -il` and require exactly the intended functional capabilities:

```powershell
foreach ($required in @('BTN_LEFT', 'REL_X', 'REL_Y')) {
    if ($capabilities -notmatch "\b$required\b") {
        throw "Virtual mouse is missing required capability: $required"
    }
}
foreach ($forbidden in @('BTN_RIGHT', 'BTN_MIDDLE', 'ABS_MT_POSITION_X', 'ABS_MT_POSITION_Y')) {
    if ($capabilities -match "\b$forbidden\b") {
        throw "Virtual mouse leaks forbidden capability: $forbidden"
    }
}
```

Expected before implementation: the script fails because `WorkBuddy Virtual Mouse` is absent.

- [ ] **Step 2: Define the adapter API**

Create `touchpad_bridge.h`:

```c
struct wb_touchpad_bridge {
    int physical_fd;
    int virtual_fd;
    bool virtual_created;
    char physical_path[64];
    struct wb_touchpad_drag_state drag_state;
};

int wb_touchpad_bridge_open(struct wb_touchpad_bridge *bridge,
                            struct wb_touchpad_drag_config config);
int wb_touchpad_bridge_fd(const struct wb_touchpad_bridge *bridge);
const char *wb_touchpad_bridge_path(const struct wb_touchpad_bridge *bridge);
int wb_touchpad_bridge_drain(struct wb_touchpad_bridge *bridge);
void wb_touchpad_bridge_close(struct wb_touchpad_bridge *bridge);
```

Include `<stdbool.h>` and `touchpad_drag_state.h`; the structure is stack-owned and initialized by `wb_touchpad_bridge_open()` to fd values `-1` before any operation that can fail.

- [ ] **Step 3: Implement discovery and virtual mouse setup**

In `touchpad_bridge.c`:

- Scan `/dev/input/event0` through `/dev/input/event63` and compare `EVIOCGNAME` with `Xiaomi Touch`.
- Open the match using `O_RDONLY | O_NONBLOCK | O_CLOEXEC`.
- Open `/dev/uinput` using `O_WRONLY | O_NONBLOCK | O_CLOEXEC`.
- Enable `EV_KEY/BTN_LEFT`, `EV_REL/REL_X`, `EV_REL/REL_Y` only.
- Create `WorkBuddy Virtual Mouse` with `BUS_USB`, vendor `0x2717`, product `0x5743`, version `1`.
- Never issue `EVIOCGRAB` for the touchpad fd.

On partial setup failure, destroy any created virtual device and close both fds before returning `-1` with the original `errno` retained.

- [ ] **Step 4: Decode evdev frames and emit one synchronized output batch**

`wb_touchpad_bridge_drain()` reads until `EAGAIN`:

```c
ABS_MT_SLOT        -> wb_touchpad_drag_select_slot(&bridge->drag_state, event.value)
ABS_MT_TRACKING_ID -> wb_touchpad_drag_set_tracking_id(&bridge->drag_state, event.value)
ABS_MT_POSITION_X  -> wb_touchpad_drag_set_x(&bridge->drag_state, event.value)
ABS_MT_POSITION_Y  -> wb_touchpad_drag_set_y(&bridge->drag_state, event.value)
SYN_REPORT         -> wb_touchpad_drag_finish_frame(&bridge->drag_state)
```

For a non-empty output, write events in this order and finish with one `EV_SYN/SYN_REPORT`:

1. `EV_KEY/BTN_LEFT/1` when `LEFT_DOWN` is set.
2. Non-zero `EV_REL/REL_X` and `EV_REL/REL_Y` when `MOVE` is set.
3. `EV_KEY/BTN_LEFT/0` when `LEFT_UP` is set.

Return `0` for success/EAGAIN and `-1` for EOF, short reads, or write failures. `close()` first emits the output of `wb_touchpad_drag_cancel()`, then calls `UI_DEV_DESTROY` and closes both fds.

- [ ] **Step 5: Build the adapter with Android NDK strict flags**

```powershell
$clang = '.\.tools\android-sdk\ndk\29.0.14206865\toolchains\llvm\prebuilt\windows-x86_64\bin\aarch64-linux-android26-clang.cmd'
& $clang -std=c11 -O2 -Wall -Wextra -Werror -pedantic -c native-bridge\src\touchpad_bridge.c -o .tools\three-finger-drag\touchpad_bridge.o
```

Expected: compilation succeeds with no warning.

- [ ] **Step 6: Commit the Linux adapter**

```powershell
git add native-bridge/src/touchpad_bridge.c native-bridge/src/touchpad_bridge.h native-bridge/tests/virtual_mouse_device_test.ps1
git commit -m "feat: inject passive touchpad drag events"
```

---

### Task 4: Integrate touchpad input into the existing native process

**Files:**

- Modify: `native-bridge/src/alttab_evdev_bridge.c`
- Modify: `native-bridge/tests/native_protocol_test.c`
- Test: `native-bridge/tests/key_remap_test.c`
- Test: `native-bridge/tests/touchpad_drag_state_test.c`

**Interfaces:**

- Consumes `wb_touchpad_bridge_open/fd/path/drain/close`.
- Produces one supervised binary reporting all four Pad rules through the unchanged V2 native protocol.

- [ ] **Step 1: Update the native protocol expectations first**

Change `native_protocol_test.c` expected strings to:

```c
"WB_NATIVE_V2 version=0.8.0 protocol=2 rules=alt_tab,windows_key,arrow_keys,three_finger_drag"
"WB_NATIVE_READY_V2 version=0.8.0 protocol=2 rules=alt_tab,windows_key,arrow_keys,three_finger_drag physical=/dev/input/event18"
```

Compile and run:

```powershell
gcc native-bridge\tests\native_protocol_test.c native-bridge\src\native_protocol.c -std=c11 -Wall -Wextra -Werror -pedantic -o .tools\three-finger-drag\native_protocol_test.exe
& .\.tools\three-finger-drag\native_protocol_test.exe
```

Expected after Task 1 generation: PASS, proving the static capability line is already correct.

- [ ] **Step 2: Open both device paths before grabbing the keyboard**

In `alttab_evdev_bridge.c`, include `touchpad_bridge.h`, then create:

```c
struct wb_touchpad_bridge touchpad;
struct wb_touchpad_drag_config drag_config = {
    .activation_distance = 48,
    .movement_numerator = 1,
    .movement_denominator = 1,
};
```

Call `wb_touchpad_bridge_open(&touchpad, drag_config)` after the physical keyboard is found but before `EVIOCGRAB` is applied to it. If opening fails, print the exact diagnostic below, clean up the keyboard fd, and exit non-zero so `NativeBridgeSupervisor` retries.

```c
fprintf(stderr, "TOUCHPAD_OPEN_FAILED errno=%d message=%s\n", errno, strerror(errno));
```

After success, log exactly:

```text
TOUCHPAD_READY physical=/dev/input/eventN virtual=WorkBuddy_Virtual_Mouse threshold=48 scale=1/1
```

- [ ] **Step 3: Poll keyboard and touchpad without changing keyboard remapping**

Replace the single `pollfd` with:

```c
struct pollfd poll_fds[2] = {
    {.fd = physical_fd, .events = POLLIN},
    {.fd = wb_touchpad_bridge_fd(&touchpad), .events = POLLIN},
};
```

Keep the existing keyboard event block byte-for-byte in behavior. When the touchpad fd has `POLLIN`, call `wb_touchpad_bridge_drain(&touchpad)`; treat `POLLERR | POLLHUP | POLLNVAL` or a negative drain result as a fatal read error.

- [ ] **Step 4: Consolidate cleanup ordering**

Route all post-open failures and normal termination through one cleanup section in this order:

```c
wb_touchpad_bridge_close(&touchpad); /* sends BTN_LEFT up first */
ioctl(physical_fd, EVIOCGRAB, 0);
ioctl(virtual_fd, UI_DEV_DESTROY);
close(virtual_fd);
close(physical_fd);
```

Do not call touchpad close before it has been initialized. Preserve existing result codes where possible; use result `11` for touchpad open/setup failure and `12` for touchpad read/write failure.

- [ ] **Step 5: Run all native host tests**

```powershell
gcc native-bridge\tests\key_remap_test.c native-bridge\src\key_remap.c -std=c11 -Wall -Wextra -Werror -pedantic -o .tools\three-finger-drag\key_remap_test.exe
& .\.tools\three-finger-drag\key_remap_test.exe
& .\.tools\three-finger-drag\native_protocol_test.exe
& .\.tools\three-finger-drag\touchpad_drag_state_test.exe
```

Expected: `key_remap_test: OK`, `native_protocol_test: OK`, and `touchpad_drag_state_test: OK`.

- [ ] **Step 6: Build the complete ARM64 binary**

```powershell
$clang = '.\.tools\android-sdk\ndk\29.0.14206865\toolchains\llvm\prebuilt\windows-x86_64\bin\aarch64-linux-android26-clang.cmd'
& $clang -std=c11 -O2 -Wall -Wextra -Werror -pedantic `
  native-bridge\src\alttab_evdev_bridge.c `
  native-bridge\src\key_remap.c `
  native-bridge\src\native_protocol.c `
  native-bridge\src\touchpad_drag_state.c `
  native-bridge\src\touchpad_bridge.c `
  -o .tools\three-finger-drag\alttab_evdev_bridge
& .\.tools\platform-tools\adb.exe -s 192.168.0.126:40735 push .tools\three-finger-drag\alttab_evdev_bridge /data/local/tmp/workbuddy-three-finger-drag-probe
& .\.tools\platform-tools\adb.exe -s 192.168.0.126:40735 shell chmod 700 /data/local/tmp/workbuddy-three-finger-drag-probe
& .\.tools\platform-tools\adb.exe -s 192.168.0.126:40735 shell /data/local/tmp/workbuddy-three-finger-drag-probe --describe
```

Expected: strict NDK build succeeds; `--describe` returns version 0.8.0 and all four Pad rule IDs without opening or grabbing any input device.

- [ ] **Step 7: Commit native-process integration**

```powershell
git add native-bridge/src/alttab_evdev_bridge.c native-bridge/tests/native_protocol_test.c
git commit -m "feat: run touchpad drag in native bridge"
```

---

### Task 5: Package, install, and verify the virtual device safely

**Files:**

- Replace generated asset: `android-bridge/app/src/main/assets/alttab_evdev_bridge`
- Test: `native-bridge/tests/virtual_keyboard_device_test.ps1`
- Test: `native-bridge/tests/virtual_mouse_device_test.ps1`

**Interfaces:**

- Consumes the ARM64 binary from Task 4 and unchanged Android installer/supervisor.
- Produces a signed 0.8.0 APK installed over 0.7.0 with settings and Shizuku grant preserved.

- [ ] **Step 1: Put the verified binary in the APK asset**

```powershell
Copy-Item -LiteralPath .tools\three-finger-drag\alttab_evdev_bridge -Destination android-bridge\app\src\main\assets\alttab_evdev_bridge -Force
```

Verify source output and asset hashes match:

```powershell
(Get-FileHash .tools\three-finger-drag\alttab_evdev_bridge -Algorithm SHA256).Hash
(Get-FileHash android-bridge\app\src\main\assets\alttab_evdev_bridge -Algorithm SHA256).Hash
```

- [ ] **Step 2: Run the full local regression suite**

```powershell
& .\protocol\tests\GeneratedSchemaTest.ps1
& .\.tools\three-finger-drag\key_remap_test.exe
& .\.tools\three-finger-drag\native_protocol_test.exe
& .\.tools\three-finger-drag\touchpad_drag_state_test.exe
$env:JAVA_HOME = (Resolve-Path '.\.tools\jdk-extract\jdk-17.0.20.1+1')
& '.\.tools\gradle-extract\gradle-9.6.0\bin\gradle.bat' -p android-bridge :app:testDebugUnitTest --no-daemon
git diff --check
```

Expected: schema test and three native tests print `OK`; all Android JUnit tests pass; `git diff --check` is silent.

- [ ] **Step 3: Build the APK with the established manual Android chain**

```powershell
$javaHome = (Resolve-Path '.\.tools\jdk-extract\jdk-17.0.20.1+1').Path
$sdk = (Resolve-Path '.\.tools\android-sdk').Path
$buildTools = Join-Path $sdk 'build-tools\36.0.0'
$androidJar = Join-Path $sdk 'platforms\android-36\android.jar'
$out = Join-Path (Resolve-Path '.\android-bridge\app\build').Path 'manual-release-0.8.0'
New-Item -ItemType Directory -Force $out, "$out\gen", "$out\classes", "$out\dex" | Out-Null

& "$buildTools\aapt2.exe" compile --dir .\android-bridge\app\src\main\res -o "$out\resources.zip"
& "$buildTools\aapt2.exe" link -o "$out\bridge-unsigned.apk" `
  -I $androidJar `
  --manifest .\android-bridge\app\src\main\AndroidManifest.xml `
  --min-sdk-version 26 --target-sdk-version 36 `
  --version-code 10 --version-name 0.8.0 `
  --java "$out\gen" `
  -A .\android-bridge\app\src\main\assets `
  "$out\resources.zip"

$buildConfig = @'
package local.workbuddy.alttab;
public final class BuildConfig {
    public static final String VERSION_NAME = "0.8.0";
    private BuildConfig() {}
}
'@
$buildConfigPath = "$out\gen\local\workbuddy\alttab\BuildConfig.java"
[System.IO.Directory]::CreateDirectory((Split-Path -Parent $buildConfigPath)) | Out-Null
[System.IO.File]::WriteAllText($buildConfigPath, $buildConfig, [System.Text.UTF8Encoding]::new($false))

$shizuku = @(
  '.\.tools\shizuku-api-13.1.5\annotation-1.3.0.jar',
  '.\.tools\shizuku-api-13.1.5\aidl\classes.jar',
  '.\.tools\shizuku-api-13.1.5\api\classes.jar',
  '.\.tools\shizuku-api-13.1.5\provider\classes.jar',
  '.\.tools\shizuku-api-13.1.5\shared\classes.jar'
) | ForEach-Object { (Resolve-Path $_).Path }
$sources = @(
  Get-ChildItem .\android-bridge\app\src\main\java -Recurse -Filter *.java | ForEach-Object FullName
  Get-ChildItem "$out\gen" -Recurse -Filter *.java | ForEach-Object FullName
)
& "$javaHome\bin\javac.exe" -encoding UTF-8 -source 8 -target 8 `
  -classpath (($androidJar) + ';' + ($shizuku -join ';')) `
  -d "$out\classes" @sources
& "$javaHome\bin\jar.exe" cf "$out\classes.jar" -C "$out\classes" .
& "$buildTools\d8.bat" --min-api 26 --lib $androidJar --output "$out\dex" `
  "$out\classes.jar" @shizuku
Copy-Item "$out\bridge-unsigned.apk" "$out\bridge-with-dex.apk" -Force
& "$javaHome\bin\jar.exe" uf "$out\bridge-with-dex.apk" -C "$out\dex" classes.dex
& "$buildTools\zipalign.exe" -f -P 16 4 "$out\bridge-with-dex.apk" "$out\bridge-aligned.apk"
& "$buildTools\apksigner.bat" sign `
  --ks .\.tools\probe-debug.keystore `
  --ks-key-alias androiddebugkey --ks-pass pass:android --key-pass pass:android `
  --out "$out\workbuddy-alt-tab-0.8.0.apk" "$out\bridge-aligned.apk"
```

Expected: `android-bridge/app/build/manual-release-0.8.0/workbuddy-alt-tab-0.8.0.apk` exists and contains `assets/alttab_evdev_bridge`. This deliberately avoids the already recorded Gradle loopback-selector packaging failure while still using the same repository-local Android 36 tools and signing key as 0.7.0.

- [ ] **Step 4: Verify identity before installation**

```powershell
$buildTools = '.\.tools\android-sdk\build-tools\36.0.0'
$apk = '.\android-bridge\app\build\manual-release-0.8.0\workbuddy-alt-tab-0.8.0.apk'
& "$buildTools\zipalign.exe" -c -P 16 -v 4 $apk
& "$buildTools\apksigner.bat" verify --verbose --print-certs $apk
& .\.tools\platform-tools\adb.exe -s 192.168.0.126:40735 shell dumpsys package local.workbuddy.alttab | Select-String 'versionCode|versionName'
```

Record the installed certificate digest before replacement and require the candidate digest to remain `CB952E9472E9970483FA68E9474ADA70A5A98DA35B6F9EABB1D5A8B5B8D8B7D0`.

- [ ] **Step 5: Install in place and verify one supervised instance**

```powershell
& .\.tools\platform-tools\adb.exe -s 192.168.0.126:40735 install -r $apk
& .\.tools\platform-tools\adb.exe -s 192.168.0.126:40735 shell am force-stop local.workbuddy.alttab
& .\.tools\platform-tools\adb.exe -s 192.168.0.126:40735 shell monkey -p local.workbuddy.alttab 1
```

Wait for Shizuku binding, then require one app process, one UserService, and one `/data/local/tmp/alttab_evdev_bridge`. Confirm the deployed native hash matches the APK asset and `--describe` reports 0.8.0.

- [ ] **Step 6: Validate both virtual input devices**

```powershell
& .\native-bridge\tests\virtual_keyboard_device_test.ps1 -AdbPath .\.tools\platform-tools\adb.exe -Target 192.168.0.126:40735
& .\native-bridge\tests\virtual_mouse_device_test.ps1 -AdbPath .\.tools\platform-tools\adb.exe -Target 192.168.0.126:40735
```

Expected: keyboard test remains PASS; mouse test reports a `CURSOR` device with only the planned mouse capabilities. Confirm `getevent -pl /dev/input/event11` still succeeds while the bridge runs, proving the physical touchpad is not grabbed.

- [ ] **Step 7: Verify pause and forced-stop release paths**

Start a three-finger drag, then use the existing Pad “暂停键盘拦截” control and confirm the process exits, the virtual mouse disappears, and the next ordinary click is not interpreted as a release from a stuck drag. Resume and confirm exactly one virtual mouse returns. Repeat once with `adb shell pkill -TERM -f /data/local/tmp/alttab_evdev_bridge`.

- [ ] **Step 8: Commit the packaged binary**

```powershell
git add android-bridge/app/src/main/assets/alttab_evdev_bridge native-bridge/tests/virtual_mouse_device_test.ps1
git commit -m "build: package three-finger drag bridge"
```

---

### Task 6: Run Moonlight hand-feel acceptance and document the result

**Files:**

- Modify: `README.md`
- Create: `docs/verification/2026-09-13-three-finger-drag.md`
- Optional parameter-only modification after user feedback: `native-bridge/src/alttab_evdev_bridge.c`

**Interfaces:**

- Consumes the installed 0.8.0 bridge.
- Produces a recorded acceptance result and final threshold/scale values.

- [ ] **Step 1: Perform the first Moonlight behavior pass**

In an active Moonlight session, ask the user to perform these in order:

1. Three-finger tap without movement: no click.
2. Drag a Windows title bar: window follows and releases on finger lift.
3. Drag a file icon without opening it.
4. Select a paragraph of text.
5. Add a fourth finger during a drag: immediate release.
6. Lift one finger during a drag: immediate release.

At the same time collect only bridge logs and event counts; do not record typed text or screen contents.

- [ ] **Step 2: Apply at most parameter-only tuning**

Interpret feedback using this fixed table:

| Feedback | Change |
|---|---|
| Starts accidentally | raise `activation_distance` from 48 to 64 |
| Starts too late | lower `activation_distance` from 48 to 32 |
| Cursor is too slow | set scale to `3/2` |
| Cursor is too fast | set scale to `3/4` |
| Cursor jitters | ignore per-frame deltas whose absolute X and Y are both below 2 |

Change only the centralized config/state-machine filter, rerun Tasks 2, 4, and 5 verification, then reinstall. Any need to grab the physical touchpad, modify Moonlight, or detect the foreground app is a new design decision and is not part of this tuning step.

- [ ] **Step 3: Update user-facing documentation**

In `README.md`, change the feature list from three to four rules and document:

```text
三指移动超过约 1.5 mm 后开始 Windows 左键拖动；任意一指抬起或出现第四指即释放。三指轻触不会产生单击。
```

In the verification record include exact version, source/asset/deployed SHA-256, APK certificate digest, automated test outputs, virtual-device capabilities, final threshold/scale, Shizuku preservation, and each hand-feel result.

- [ ] **Step 4: Run final regression before completion**

Repeat the full local suite from Task 5 Step 2, both virtual-device scripts, native deployed-hash comparison, package/signature verification, dashboard four-rule check, and the three existing keyboard checks in a live Moonlight session.

Expected: all automated checks pass; Alt+Tab, Command/Windows, direction keys, and three-finger dragging all work; no left button remains held after release, pause, or process termination.

- [ ] **Step 5: Commit final verification**

```powershell
git add README.md docs/verification/2026-09-13-three-finger-drag.md native-bridge/src/alttab_evdev_bridge.c android-bridge/app/src/main/assets/alttab_evdev_bridge
git commit -m "docs: verify three-finger drag on Xiaomi Pad"
```

Only include the two native files in this commit if the user-requested parameter tuning changed them after Task 5.
