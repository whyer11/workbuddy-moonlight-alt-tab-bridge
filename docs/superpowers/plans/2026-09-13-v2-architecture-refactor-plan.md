# WorkBuddy V2 Architecture Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the V1 status/dashboard internals with a capability-driven V2 modular architecture while preserving every verified keyboard mapping and startup behavior.

**Architecture:** A checked-in JSON schema generates Java and C capability constants. Android uses domain models plus small ports coordinated outside the Activity; Shizuku reports verified native capabilities; Windows separates the mandatory keyboard core from the optional UDP status adapter. Deployment is atomic and rollback-capable.

**Tech Stack:** Android Java, Shizuku Binder, Java UDP, C11 evdev/uinput, Win32 C/Winsock2, PowerShell, JUnit 4, MinGW GCC, Android NDK 29, ADB.

**Spec:** `docs/superpowers/specs/2026-09-13-v2-architecture-refactor-design.md`

## Global Constraints

- Preserve `Alt+Tab -> Alt+F12 -> Windows Alt+Tab`, `Left Meta -> Scroll Lock -> Left Windows`, and all four arrow keys byte-for-byte.
- Preserve package `local.workbuddy.alttab`, the signing certificate, Shizuku permission, Android boot recovery, and Windows login startup.
- Status failures must never stop the keyboard bridge.
- UDP stays read-only on port 57420 and the firewall stays limited to `100.64.0.0/10`.
- V2 product version is `0.7.0`; status protocol and native capability protocol are version 2.
- Use TDD for each behavior change and commit after every independently green task.
- Do not enable injected-event-only filtering until the real Moonlight path proves the required Windows flag; default behavior remains compatible.

---

### Task 1: Generate a single cross-language rule schema

**Files:**
- Create: `protocol/bridge-schema.json`
- Create: `protocol/generate-schema.ps1`
- Create: `protocol/tests/GeneratedSchemaTest.ps1`
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/generated/GeneratedBridgeSchema.java`
- Create: `native-bridge/src/generated_bridge_schema.h`
- Create: `windows-host/src/GeneratedBridgeSchema.h`
- Modify: `android-bridge/app/build.gradle`

**Interfaces:**
- Produces Java constants `PROTOCOL_VERSION`, `PRODUCT_VERSION`, `STATUS_PORT`, and immutable `RULES`.
- Produces C macros `WB_SCHEMA_PROTOCOL_VERSION`, `WB_SCHEMA_PRODUCT_VERSION`, `WB_SCHEMA_STATUS_PORT`, and endpoint rule CSV strings.

- [ ] **Step 1: Write a failing generation consistency test**

The test loads the JSON, runs the generator into a temporary directory, and byte-compares all three outputs with the checked-in files. It also asserts unique rule IDs and exactly `alt_tab`, `windows_key`, `arrow_keys` in order.

Run:

```powershell
& protocol/tests/GeneratedSchemaTest.ps1
```

Expected: FAIL because the schema and generator do not exist.

- [ ] **Step 2: Add the schema and deterministic generator**

Use this schema shape:

```json
{
  "schemaVersion": 1,
  "protocolVersion": 2,
  "productVersion": "0.7.0",
  "statusPort": 57420,
  "nonceHexLength": 32,
  "maxDatagramBytes": 512,
  "rules": [
    {"id":"alt_tab","title":"Alt+Tab 窗口切换","padInput":"Alt+Tab","padOutput":"Alt+F12","windowsAction":"Windows Alt+Tab","pad":true,"windows":true},
    {"id":"windows_key","title":"Command 作为 Windows 键","padInput":"Command / Left Meta","padOutput":"Scroll Lock","windowsAction":"左 Windows 键","pad":true,"windows":true},
    {"id":"arrow_keys","title":"方向键兼容修复","padInput":"上 / 下 / 左 / 右","padOutput":"原键值直通","windowsAction":"Windows 方向键","pad":true,"windows":false}
  ]
}
```

PowerShell must emit UTF-8 without BOM for generated Java/C and sort no data independently of the JSON order. Escape Java strings and C strings explicitly.

- [ ] **Step 3: Replace handwritten version/rule constants**

`BridgeRules` maps generated definitions into domain objects. `WorkBuddyStatusProtocol.h` and native source include generated headers. `build.gradle` becomes versionCode 9/versionName 0.7.0.

- [ ] **Step 4: Run generator and tests**

Run the generator twice, then the consistency test and existing Java/C suites. Expected: checked-in outputs remain unchanged on the second run and all existing tests pass.

- [ ] **Step 5: Commit**

```powershell
git add protocol android-bridge/app/src/main/java/local/workbuddy/alttab/generated android-bridge/app/build.gradle native-bridge/src/generated_bridge_schema.h windows-host/src/GeneratedBridgeSchema.h android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeRules.java windows-host/src/WorkBuddyStatusProtocol.h
git commit -m "refactor: generate bridge capabilities from one schema"
```

---

### Task 2: Make native capabilities real and machine-readable

**Files:**
- Modify: `native-bridge/src/alttab_evdev_bridge.c`
- Create: `native-bridge/src/native_protocol.c`
- Create: `native-bridge/src/native_protocol.h`
- Create: `native-bridge/tests/native_protocol_test.c`
- Modify: `native-bridge/tests/key_remap_test.c`

**Interfaces:**
- `int wb_native_write_description(char *buffer, size_t capacity)`.
- `int wb_native_write_ready(char *buffer, size_t capacity, const char *physical_path)`.
- CLI `--describe` exits 0 after one V2 description line without opening input devices.

- [ ] **Step 1: Write failing protocol tests**

Assert exact output:

```c
assert(strcmp(description,
  "WB_NATIVE_V2 version=0.7.0 protocol=2 rules=alt_tab,windows_key,arrow_keys") == 0);
assert(strcmp(ready,
  "WB_NATIVE_READY_V2 version=0.7.0 protocol=2 rules=alt_tab,windows_key,arrow_keys physical=/dev/input/event18") == 0);
```

Also assert null pointers, truncated buffers and physical paths containing spaces/control characters are rejected.

- [ ] **Step 2: Implement the protocol formatter and `--describe`**

Handle `--describe` before `find_keyboard()`. Use only generated constants and reject extra CLI arguments.

- [ ] **Step 3: Replace the free-form READY line**

After successful EVIOCGRAB, format and print the strict V2 READY line. Keep existing remap event logs unchanged.

- [ ] **Step 4: Prove the key hot path is unchanged**

Compile with NDK 29 `-std=c11 -O2 -Wall -Wextra -Werror -pedantic`; run native protocol tests and the complete existing `key_remap_test`. Compare the disjoint `key_remap.c` diff and require no behavior edits.

- [ ] **Step 5: Commit**

```powershell
git add native-bridge/src native-bridge/tests
git commit -m "feat: report verified native V2 capabilities"
```

---

### Task 3: Introduce truthful Android domain state and ports

**Files:**
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/domain/EndpointState.java`
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/domain/BridgeEndpointSnapshot.java`
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/domain/DashboardState.java`
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/domain/DashboardStateReducer.java`
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/ports/PadBridgeMonitor.java`
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/ports/ComputerBridgeMonitor.java`
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/ports/ComputerHostSettings.java`
- Test: `android-bridge/app/src/test/java/local/workbuddy/alttab/domain/DashboardStateReducerTest.java`

**Interfaces:**

```java
interface PadBridgeMonitor { BridgeEndpointSnapshot read(); }
interface ComputerBridgeMonitor { BridgeEndpointSnapshot read(String host, int timeoutMs); }
interface ComputerHostSettings { String get(); void set(String host); }
```

`BridgeEndpointSnapshot` contains endpoint state, product version, protocol version, immutable rule ID set and error code.

- [ ] **Step 1: Write failing per-rule truth tests**

Cover a READY native missing `windows_key`; assert only that Pad rule is `UNSUPPORTED`. Cover computer offline while arrows remain `NOT_REQUIRED`. Cover protocol mismatch as `UNKNOWN`, not READY.

- [ ] **Step 2: Implement immutable domain types and reducer**

Remove the always-true configured field. Configured means the rule exists in generated catalog; runtime Pad/Windows states come only from endpoint snapshots.

- [ ] **Step 3: Add ports and adapt current storage/client behind them**

Rename or wrap `ComputerHostStore` and `ComputerStatusClient` without changing persistence key or wire behavior yet. No Android View or resource strings may appear in domain or ports.

- [ ] **Step 4: Run all pure Java tests and commit**

Expected: old state tests are migrated, new partial-capability tests pass, and no test depends on Android lifecycle.

```powershell
git add android-bridge/app/src/main/java/local/workbuddy/alttab/domain android-bridge/app/src/main/java/local/workbuddy/alttab/ports android-bridge/app/src/test/java/local/workbuddy/alttab/domain
git commit -m "refactor: model endpoint capabilities per bridge rule"
```

---

### Task 4: Add atomic native deployment and Binder V2

**Files:**
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/nativebridge/NativeCapabilityParser.java`
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/nativebridge/NativeBridgeInstaller.java`
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/nativebridge/NativeBridgeSupervisor.java`
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/nativebridge/NativeBridgeStatus.java`
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/shizuku/BridgeBinderProtocolV2.java`
- Modify: `android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeUserService.java`
- Modify: `android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeController.java`
- Test: corresponding parser, installer state-machine, supervisor backoff and Binder serialization tests.

**Interfaces:**
- Installer returns `InstallResult { INSTALLED, ALREADY_CURRENT, ROLLED_BACK, FAILED }`.
- Supervisor exposes immutable `NativeBridgeStatus snapshot()` and `setActive(boolean)`.
- Binder snapshot transaction returns only structured V2 fields.

- [ ] **Step 1: Write parser and deployment state-machine failures**

Use fake filesystem/process ports. Assert invalid `--describe`, hash mismatch, READY timeout and failed new process restore `.previous`; a good stage replaces the target only after validation.

- [ ] **Step 2: Implement deployment ports and atomic installer**

Abstract file operations and process launch so JVM tests never touch Android filesystem. Production adapter uses `.new`, `.previous`, `fsync`, chmod and rename.

- [ ] **Step 3: Write and implement supervisor tests**

Assert backoff sequence 1/2/4/8 seconds, reset after READY, pause releases the process, resume starts it, and destroy waits then force-kills only after timeout.

- [ ] **Step 4: Write and implement Binder V2 serialization**

Use an interface descriptor, status transaction, and local `setActive` transaction. Reject bad interface tokens and unknown transactions. Controller maps exceptions to UNKNOWN snapshots.

- [ ] **Step 5: Integrate UserService**

UserService becomes composition only: construct installer/supervisor, expose Binder, close supervisor on destroy. It must never synthesize native rule IDs from the Java catalog.

- [ ] **Step 6: Run JVM tests, Android compilation and a device smoke test**

On Pad, execute the bundled native with `--describe`, start UserService, verify one native process and a READY V2 snapshot, then exercise pause/resume and confirm EVIOCGRAB release/reacquire.

- [ ] **Step 7: Commit**

```powershell
git add android-bridge/app/src/main/java/local/workbuddy/alttab/nativebridge android-bridge/app/src/main/java/local/workbuddy/alttab/shizuku android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeUserService.java android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeController.java android-bridge/app/src/test
git commit -m "refactor: isolate native deployment supervision and Binder V2"
```

---

### Task 5: Move refresh orchestration out of the Activity

**Files:**
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/application/DashboardCoordinator.java`
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/application/DashboardViewState.java`
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/application/DashboardTextMapper.java`
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/adapters/ShizukuPadBridgeMonitor.java`
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/adapters/SharedPreferencesComputerHostSettings.java`
- Test: coordinator lifecycle/concurrency and mapper tests.

**Interfaces:**
- `DashboardCoordinator.start(Listener)`, `stop()`, `refreshNow()`, `setHost(String)`, `setBridgeActive(boolean)`.
- Listener receives immutable `DashboardViewState` on the main-thread executor supplied by composition root.

- [ ] **Step 1: Write failing lifecycle and concurrency tests**

Fake monitors use controllable latches. Assert Pad timeout does not delay computer result, computer timeout does not hide Pad result, repeated refresh requests coalesce, `stop()` prevents callbacks, and healthy cadence is five seconds versus two seconds for degraded state.

- [ ] **Step 2: Implement Coordinator with injected executors/scheduler**

Use separate Pad and computer futures, each with a deadline. Maintain one active generation and one pending refresh bit. Coordinator contains no Android classes.

- [ ] **Step 3: Move all status text mapping into one mapper**

Mapper consumes Android resource strings or a small `TextResources` port. Controller returns enum state, never Chinese text.

- [ ] **Step 4: Implement adapters and run tests**

Preserve SharedPreferences file/key and current computer address. Adapt Binder snapshot to domain; do not let adapters update View objects.

- [ ] **Step 5: Commit**

```powershell
git add android-bridge/app/src/main/java/local/workbuddy/alttab/application android-bridge/app/src/main/java/local/workbuddy/alttab/adapters android-bridge/app/src/test
git commit -m "refactor: coordinate independent bridge health checks"
```

---

### Task 6: Upgrade UDP V2 and make Windows status optional

**Files:**
- Modify: `windows-host/src/WorkBuddyStatusProtocol.c`
- Modify: `windows-host/src/WorkBuddyStatusProtocol.h`
- Modify: `windows-host/src/WorkBuddyStatusServer.c`
- Create: `windows-host/src/KeyboardBridgeCore.c`
- Create: `windows-host/src/KeyboardBridgeCore.h`
- Create: `windows-host/src/HostApplication.c`
- Modify: `windows-host/src/WorkBuddyAltTabHost.c`
- Modify: `windows-host/tests/StatusProtocolTest.c`
- Modify: `windows-host/tests/StatusServerIntegrationTest.c`
- Create: `windows-host/tests/HostStatusIsolationTest.c`
- Modify Android UDP adapter/tests for V2.

**Interfaces:**
- `wb_keyboard_core_start`, `wb_keyboard_core_handle_message`, `wb_keyboard_core_stop`.
- `wb_host_application_start` returns success when the keyboard core is ready even if status is degraded.
- V2 response includes `core=ready` and generated Windows rules.

- [ ] **Step 1: Write failing V2 strict-parser tests**

Reject V1, duplicate/unknown fields, invalid nonce and datagrams over 512 bytes. Assert exact V2 response generated from schema constants.

- [ ] **Step 2: Implement V2 protocol and Android parser**

Keep the network protocol read-only. Android returns UNKNOWN for protocol mismatch and retains Pad status independently.

- [ ] **Step 3: Extract KeyboardBridgeCore behind a small C interface**

Move hotkey, hook and Win-key state without changing the state-machine behavior. Existing Win key integration test must remain green before any status change.

- [ ] **Step 4: Write the status-isolation regression test**

Occupy UDP 57420, start HostApplication with fake/log adapters, assert core ready, then inject Alt+F12 and two Scroll Lock cycles and observe Tab plus two LWIN down/up cycles.

- [ ] **Step 5: Make status startup fail-open**

Log `STATUS_DEGRADED` and continue the message loop. Retry status startup on a bounded cadence without blocking keyboard messages; stop it independently on exit.

- [ ] **Step 6: Add `--self-test`**

Self-test validates generated schema, protocol formatting and pure keyboard state machine without registering global hotkeys or binding production port.

- [ ] **Step 7: Run strict Windows, protocol, isolation and Android client tests; commit**

```powershell
git add windows-host android-bridge/app/src/main/java/local/workbuddy/alttab/adapters android-bridge/app/src/test
git commit -m "refactor: isolate keyboard core from optional status V2"
```

---

### Task 7: Reduce ProbeActivity to a view and remove dead abstractions

**Files:**
- Modify: `android-bridge/app/src/main/java/local/workbuddy/alttab/ProbeActivity.java`
- Modify: `android-bridge/app/src/main/res/values/strings.xml`
- Create: `android-bridge/app/src/main/res/color/bridge_status_*.xml`
- Remove after migration: old `BridgeDashboardState`, `BridgeDashboardPresenter`, `PadBridgeSnapshot`, `ComputerStatus`, `ComputerStatusClient`, and `ComputerHostStore` classes.
- Test: mapper/view-state tests and Android source compilation.

**Interfaces:**
- Activity depends only on Coordinator and immutable ViewState.
- All user-visible strings and colors come from resources.

- [ ] **Step 1: Add mapper tests covering every enum state**

Assert configured, Pad READY/UNKNOWN/UNSUPPORTED, computer READY/OFFLINE/NOT_REQUIRED, active/paused and version footer text.

- [ ] **Step 2: Rebuild Activity as composition root plus renderer**

Construct adapters/coordinator once, forward `onStart/onStop`, and render only changed ViewState. Save address and pause/resume through Coordinator.

- [ ] **Step 3: Replace per-refresh drawable allocation**

Create badge backgrounds once and update tint only when state changes. Remove hardcoded Chinese and color literals from controller/domain/application classes.

- [ ] **Step 4: Remove dead V1 classes and prove no production references**

Run `rg` for each removed type and for hardcoded V1/version/rule literals. Only migration documentation may contain V1.

- [ ] **Step 5: Run all Java tests and full Android compilation; commit**

```powershell
git add android-bridge
git commit -m "refactor: make dashboard activity a thin view"
```

---

### Task 8: Build, deploy, rollback-test and run complete regression

**Files:**
- Modify: `README.md`
- Modify: `docs/verification/2026-09-12-end-to-end.md`
- Create: `docs/verification/2026-09-13-v2-refactor.md`
- Modify build/deployment scripts as required by the new file list.

**Interfaces:**
- Produces signed APK 0.7.0, native 0.7.0 and Windows Host 0.7.0 with recorded hashes.

- [ ] **Step 1: Run the complete automated suite**

Run schema generation consistency, all 17+ migrated/new JVM tests, native protocol/remap tests, NDK strict build, Windows protocol/server/core/isolation tests, MinGW strict build, firewall tests and `git diff --check`.

- [ ] **Step 2: Build and verify artifacts**

Build Android with the established manual `aapt2 + javac + d8 + zipalign + apksigner` fallback when Gradle loopback remains unavailable. Verify v2/v3 signature, package/version/permissions and matching original certificate. Run Host `--self-test` and native `--describe`.

- [ ] **Step 3: Exercise rollback before production replacement**

Use temporary paths/fake failing binaries to prove both Android native and Windows EXE rollback. Do not intentionally install a broken production binary.

- [ ] **Step 4: Deploy Windows atomically**

Back up installed Host, stop it, replace, start, verify keyboard core ready and UDP V2. Occupy the status port in a controlled test and confirm keyboard integration remains green; restore normal status afterward.

- [ ] **Step 5: Install Pad APK and verify runtime**

Use wireless ADB for serial `0084941049983540`; `install -r` the signed APK, confirm Shizuku permission preserved, one UserService/native process, native READY V2, saved host preserved, pause/resume works and dashboard reports real rule capabilities.

- [ ] **Step 6: Run real Moonlight regression**

Verify Alt+Tab, Command+E, up/down/left/right, no stuck modifiers, 3408x2272 at 144 Hz, and no status polling while Moonlight is foreground. If injected-event filtering lacks proof, leave compatibility mode enabled and document it.

- [ ] **Step 7: Verify boot/login recovery and final hashes**

Confirm Windows login entry, Android boot receivers/settings, firewall scope, Tailscale V2 response, artifact/device hashes and clean process counts. Avoid a reboot if equivalent prior boot configuration is unchanged; otherwise perform one controlled reboot and restore the session.

- [ ] **Step 8: Document, commit and push**

Record every command/result, known compatibility mode and rollback path. Run final full verification after documentation changes.

```powershell
git add README.md docs android-bridge native-bridge windows-host protocol
git commit -m "release: verify WorkBuddy bridge V2 architecture"
git push origin master
```

Expected: clean worktree, local HEAD equals remote `master`, Pad dashboard is healthy, Windows Host and Pad native are 0.7.0, and all original keyboard behavior remains verified.
