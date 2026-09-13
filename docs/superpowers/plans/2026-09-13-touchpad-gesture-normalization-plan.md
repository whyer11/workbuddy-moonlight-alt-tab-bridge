# Moonlight 双指滚动与 WorkBuddy 三指续拖实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复小米 Pad 在 Moonlight 中双指滚动被放大、WorkBuddy 三指拖动过快和短暂抬手不能续拖的问题，并保留现有 Alt+Tab、Command→Win、方向键与串流功能。

**Architecture:** 采用调查报告中的方案 B。WorkBuddy 在 evdev 层继续被动读取 `Xiaomi Touch`，负责三指识别、固定点位移缩放、1 秒宽限状态和 `WorkBuddy Virtual Mouse` 注入；Moonlight fork 只在应用输入边界识别 `Xiaomi Touch` 与 `WorkBuddy Virtual Mouse`，归一化小米触控板滚轮，并在 WorkBuddy 左键保持期间抑制实体触控板的过渡 MOVE/SCROLL。两侧不建立网络协议，也不 grab 实体触控板。

**Tech Stack:** C11、Linux evdev/uinput、Android NDK 29、Java 17、Android 37/36、Shizuku 13.1.5、Gradle 9.7.1/AGP 9.4、JUnit 4、PowerShell、ADB。

**Spec:** `docs/investigations/2026-09-13-touchpad-gesture-pipeline-report.md`

## Global Constraints

- WorkBuddy 基线为提交 `b632d0f`、版本 `0.8.0`；本功能发布为 `0.9.0`、Android `versionCode 11`、Shizuku UserService `version(11)`，状态协议继续为 V2。
- `windows-host/src/GeneratedBridgeSchema.h` 只因共享 schema 同步版本常量；本次不改 Windows Host 行为，也不重新部署现有 0.8.0 Host。Pad 0.9.0 与 Host 0.8.0 继续通过 protocol 2 和既有规则 ID 兼容。
- Moonlight 基线为 fork `whyer11/moonlight-android` 的提交 `b48494cb`（12.2）；分支命名为 `feature/workbuddy-touchpad-input`。
- Moonlight fork 使用独立包名：正式版 `com.limelight.workbuddy`，调试版 `com.limelight.workbuddy.debug`；应用名分别为 `Moonlight WorkBuddy` 和 `Moonlight WorkBuddy (Debug)`。
- Moonlight fork 版本为 `12.2-workbuddy.1`、`versionCode 315001`。保留 `namespace 'com.limelight'`，不批量改 Java 包名。
- 不修改 Sunshine、Tailscale、路由器、Windows 输入倍率和 Android 全局触控板速度。
- 不对 `Xiaomi Touch` 调用 `EVIOCGRAB`，不重写一指移动、点击、右键、双指识别或掌压抑制。
- 双指修复只匹配设备名 `Xiaomi Touch` 且事件 source 等于 `SOURCE_MOUSE_RELATIVE`；其他鼠标继续逐字保持 `axis * 120` 的原行为。
- 三指速度默认 `25%`，范围 `10%..100%`，步长 `5%`；固定点余数必须按 X/Y 分开保存。
- 续拖默认 `1000 ms`，范围 `0..2000 ms`，步长 `100 ms`；`elapsed < 1000` 连续，`elapsed >= 1000` 释放。
- 四指及以上、cancel、暂停、进程结束、设备断开和 `SYN_DROPPED` 必须立即且只释放一次虚拟左键。
- 不实现“实体与虚拟两路位移去重”，因为真机实验已经排除应用层双流叠加。
- `continuationMs > 0` 依赖 Moonlight WorkBuddy 的实体事件 gate。回退官方 Moonlight 前先把续拖时长设为 `0 ms`，或暂停 WorkBuddy。
- 所有生产变更先写失败测试；只做满足测试的最小实现。每个任务结束运行目标测试和 `git diff --check` 后再提交。
- Moonlight GPL-3.0 的 `LICENSE.txt` 不改；若对外分发 APK，同时公开对应 fork、提交和构建说明。

## 已验证构建基线

- Moonlight 本地仓库：`C:\Users\wanghuanyu\WorkBuddy\2026-09-12-19-05-57\tools\MoonlightWorkBuddy`。
- Android SDK：`C:\Users\wanghuanyu\WorkBuddy\2026-09-12-19-05-57\tools\android-sdk`，包含 platform 37、build-tools 36/37、NDK `29.0.14206865`。
- JDK：全局 Eclipse Adoptium 17。
- 已用 `gradlew.bat assembleNonRootDebug --no-daemon --stacktrace` 构建成功；产物 `app\build\outputs\apk\nonRoot\debug\app-nonRoot-debug.apk`，当时 SHA-256 为 `6BD0BB4238AF62458195C4FF7D8853012EC59EFAEEC6CA7C97677FE57680BFA3`。
- Codex MSIX 子进程内 Java NIO Selector 会失败；Gradle 命令必须运行在普通 Windows PowerShell/一次性计划任务上下文。普通终端直接运行下文命令即可；由 Codex 执行时复用已经验证的一次性计划任务方式，并在结束后删除任务和临时日志。

## 最终数据流与状态机

```text
Xiaomi Touch 双指
  -> Android ACTION_SCROLL(axis)
  -> Moonlight WorkBuddy: axis * (scrollPercent / 100)
  -> Sunshine -> Windows

Xiaomi Touch 三指 ABS
  -> WorkBuddy: centroid delta * (speedPercent / 100) + remainder
  -> WorkBuddy Virtual Mouse: BTN_LEFT + REL_X/Y
  -> Moonlight WorkBuddy -> Sunshine -> Windows

IDLE -> ARMED -> DRAGGING -> GRACE
                    ^          |
                    +-- < deadline 且恰好三指恢复 --+
```

---

### Task 1: 建立跨 Java/C 的手势配置契约

**Files:**

- Modify: `protocol/bridge-schema.json`
- Modify: `protocol/generate-schema.ps1`
- Modify: `protocol/tests/GeneratedSchemaTest.ps1`
- Regenerate: `android-bridge/app/src/main/java/local/workbuddy/alttab/generated/GeneratedBridgeSchema.java`
- Regenerate: `native-bridge/src/generated_bridge_schema.h`
- Regenerate: `windows-host/src/GeneratedBridgeSchema.h`
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/domain/GestureConfig.java`
- Create: `android-bridge/app/src/test/java/local/workbuddy/alttab/domain/GestureConfigTest.java`
- Modify: `android-bridge/app/build.gradle`
- Modify: `android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeController.java`

**Interfaces:**

```java
public final class GestureConfig {
    public static GestureConfig defaults();
    public static GestureConfig normalized(int speedPercent, int continuationMs);
    public int speedPercent();
    public int continuationMs();
    public GestureConfig withSpeedPercent(int value);
    public GestureConfig withContinuationMs(int value);
    @Override public boolean equals(Object other);
    @Override public int hashCode();
}
```

- [ ] **Step 1: 先写配置边界失败测试**

`GestureConfigTest` 精确覆盖：默认 `25/1000`；速度 `7/103` 被限制为 `10/100`；时间 `-1/2100` 被限制为 `0/2000`；合法值不改变；`withXxx()` 不修改原对象；值对象相等性成立。

- [ ] **Step 2: 扩展 schema，仍由一个源生成 Java/C 常量**

把 `productVersion` 改为 `0.9.0`，并加入：

```json
"gestureSettings": {
  "threeFingerSpeedPercent": {"default":25,"min":10,"max":100,"step":5},
  "threeFingerContinuationMs": {"default":1000,"min":0,"max":2000,"step":100}
}
```

生成器输出以下同名语义常量：

```java
THREE_FINGER_SPEED_PERCENT_DEFAULT/MIN/MAX/STEP
THREE_FINGER_CONTINUATION_MS_DEFAULT/MIN/MAX/STEP
```

```c
WB_SCHEMA_THREE_FINGER_SPEED_PERCENT_DEFAULT/MIN/MAX/STEP
WB_SCHEMA_THREE_FINGER_CONTINUATION_MS_DEFAULT/MIN/MAX/STEP
```

`GeneratedSchemaTest.ps1` 读取 JSON 后同时校验生成的 Java/C 文本，防止手工漂移。

- [ ] **Step 3: 实现不可变 `GestureConfig`**

构造函数私有；`normalized()` 先 clamp，再按步长吸附：

```java
private static int snap(int value, int min, int max, int step) {
    int clamped = Math.max(min, Math.min(max, value));
    return min + Math.round((clamped - min) / (float) step) * step;
}
```

时间范围从 0 开始；速度范围从 10 开始。最后再 clamp 一次，避免浮点四舍五入越过上界。

- [ ] **Step 4: 同步版本，不改变 V2 wire contract**

`android-bridge/app/build.gradle` 设置 `versionCode 11`、`versionName '0.9.0'`；`BridgeController.USER_SERVICE_ARGS` 改为 `.version(11)`。`BridgeBinderProtocolV2.DESCRIPTOR`、状态 UDP 端口和 protocolVersion 保持不变。

- [ ] **Step 5: 运行并提交**

```powershell
& .\protocol\generate-schema.ps1
& .\protocol\tests\GeneratedSchemaTest.ps1
$env:JAVA_HOME = (Get-ChildItem 'C:\Program Files\Eclipse Adoptium' -Directory | Where-Object Name -Like 'jdk-17*' | Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty FullName)
$gradlew = 'C:\Users\wanghuanyu\WorkBuddy\2026-09-12-19-05-57\tools\MoonlightWorkBuddy\gradlew.bat'
& $gradlew -p android-bridge :app:testDebugUnitTest --tests local.workbuddy.alttab.domain.GestureConfigTest --no-daemon
git diff --check
git add protocol android-bridge/app/build.gradle android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeController.java android-bridge/app/src/main/java/local/workbuddy/alttab/domain/GestureConfig.java android-bridge/app/src/test/java/local/workbuddy/alttab/domain/GestureConfigTest.java android-bridge/app/src/main/java/local/workbuddy/alttab/generated/GeneratedBridgeSchema.java native-bridge/src/generated_bridge_schema.h windows-host/src/GeneratedBridgeSchema.h
git commit -m "feat: define configurable touchpad gesture contract"
```

Expected: schema test输出 `generated_schema_test: OK`，目标 JUnit 通过。

---

### Task 2: 用测试驱动重写三指速度与宽限状态机

**Files:**

- Modify: `native-bridge/src/touchpad_drag_state.h`
- Modify: `native-bridge/src/touchpad_drag_state.c`
- Modify: `native-bridge/tests/touchpad_drag_state_test.c`

**Public API:**

```c
struct wb_touchpad_drag_config {
    int activation_distance;
    int movement_numerator;
    int movement_denominator;
    int continuation_ms;
};

enum wb_touchpad_drag_phase {
    WB_TOUCHPAD_IDLE,
    WB_TOUCHPAD_ARMED,
    WB_TOUCHPAD_DRAGGING,
    WB_TOUCHPAD_GRACE
};

struct wb_touchpad_drag_output wb_touchpad_drag_finish_frame(
        struct wb_touchpad_drag_state *state, int64_t now_ms);
struct wb_touchpad_drag_output wb_touchpad_drag_tick(
        struct wb_touchpad_drag_state *state, int64_t now_ms);
int wb_touchpad_drag_next_timeout_ms(
        const struct wb_touchpad_drag_state *state,
        int64_t now_ms,
        int fallback_ms);
struct wb_touchpad_drag_output wb_touchpad_drag_cancel(
        struct wb_touchpad_drag_state *state);
```

状态新增：

```c
int64_t grace_deadline_ms;
long long remainder_x;
long long remainder_y;
bool button_down;
```

- [ ] **Step 1: 将旧“抬一指立即释放”测试改成新语义并先跑红**

增加以下精确测试：

```c
test_default_scale_accumulates_four_positive_single_steps();   /* 4 * +1 -> +1 */
test_default_scale_accumulates_four_negative_single_steps();   /* 4 * -1 -> -1 */
test_direction_reversal_consumes_existing_remainder();
test_lift_enters_grace_without_releasing();
test_resume_at_999ms_rebases_without_up_or_second_down();
test_resume_frame_never_moves_cursor();
test_tick_at_1000ms_releases_exactly_once();
test_resume_at_1000ms_releases_then_arms_new_drag();
test_fourth_finger_releases_immediately_during_drag();
test_fourth_finger_releases_immediately_during_grace();
test_cancel_during_grace_releases_exactly_once();
test_next_timeout_is_minimum_of_deadline_and_500ms();
```

测试时间使用显式整数，例如拖动开始 `now=100`、抬手 `now=200`，则 deadline 为 `1200`；恢复 `1199` 连续，`1200` 释放。

```powershell
gcc native-bridge\tests\touchpad_drag_state_test.c native-bridge\src\touchpad_drag_state.c -I native-bridge\src -std=c11 -Wall -Wextra -Werror -pedantic -o .tools\three-finger-drag\touchpad_drag_state_test.exe
& .\.tools\three-finger-drag\touchpad_drag_state_test.exe
```

Expected: 先因新 API/断言未实现而失败。

- [ ] **Step 2: 实现固定点缩放，禁止逐帧整数截断**

核心函数只用整数：

```c
static int scale_delta(int delta, int numerator, int denominator,
                       long long *remainder) {
    long long scaled = (long long)delta * numerator + *remainder;
    int output = (int)(scaled / denominator);
    *remainder = scaled % denominator;
    return output;
}
```

默认配置是 `numerator=25`、`denominator=100`。X/Y 使用不同余数；`arm_at()`、宽限恢复 rebase、超时释放、四指取消和 `cancel()` 都清空余数。

- [ ] **Step 3: 实现四状态转换**

处理顺序固定为：

1. 统计 active contact 和完整坐标。
2. 若 `active_count >= 4`，立即释放并进入 `IDLE`，保留 slot 事实以等待真实抬手。
3. 若处于 `GRACE` 且 `now_ms >= deadline`，先产生一次 `LEFT_UP` 并清 `button_down`。
4. `DRAGGING` 中少于 3 指或 composition 改变，进入 `GRACE`；`continuation_ms == 0` 时同帧释放。
5. `GRACE` 中 deadline 前恢复恰好 3 指且坐标完整：`phase=DRAGGING`，只更新 `last_x/last_y`，不发 DOWN/MOVE。
6. deadline 已过但本帧又是恰好 3 指：输出 UP，同时用当前质心进入 `ARMED`，下一帧重新按阈值启动。
7. 所有释放由 `button_down` 守卫；重复 tick/cancel 不再输出 UP。

`wb_touchpad_drag_next_timeout_ms()` 在非 GRACE 返回 fallback；GRACE 返回 `clamp(deadline-now, 0, fallback)`。

- [ ] **Step 4: 运行绿测并提交**

```powershell
gcc native-bridge\tests\touchpad_drag_state_test.c native-bridge\src\touchpad_drag_state.c -I native-bridge\src -std=c11 -Wall -Wextra -Werror -pedantic -o .tools\three-finger-drag\touchpad_drag_state_test.exe
& .\.tools\three-finger-drag\touchpad_drag_state_test.exe
git diff --check
git add native-bridge/src/touchpad_drag_state.h native-bridge/src/touchpad_drag_state.c native-bridge/tests/touchpad_drag_state_test.c
git commit -m "feat: add scaled three-finger drag continuation"
```

Expected: `touchpad_drag_state_test: OK`。

---

### Task 3: 加入原生参数解析与无事件超时释放

**Files:**

- Create: `native-bridge/src/bridge_options.h`
- Create: `native-bridge/src/bridge_options.c`
- Create: `native-bridge/tests/bridge_options_test.c`
- Modify: `native-bridge/src/touchpad_bridge.h`
- Modify: `native-bridge/src/touchpad_bridge.c`
- Modify: `native-bridge/src/alttab_evdev_bridge.c`

**Command line contract:**

```text
alttab_evdev_bridge [--three-finger-speed-percent N]
                    [--three-finger-continuation-ms N]
                    [/dev/input/eventN]
alttab_evdev_bridge --describe
```

旧的可选键盘路径位置参数继续兼容。未知参数、重复参数、缺值、非整数或越界值退出 `9` 并打印 usage。

```c
struct wb_bridge_options {
    const char *keyboard_path;
    int three_finger_speed_percent;
    int three_finger_continuation_ms;
};

void wb_bridge_options_init(struct wb_bridge_options *options);
int wb_bridge_options_parse(struct wb_bridge_options *options,
                            int argc, char **argv);
```

- [ ] **Step 1: 先写参数测试**

覆盖默认值、上下界、两个参数任意顺序、保留位置路径，以及 `9/101%`、`-1/2100ms`、缺值、重复和未知参数失败。

- [ ] **Step 2: 用生成常量实现严格 `strtol()` 解析**

必须校验 `errno`、`end == '\0'` 和整数边界，不用 `atoi()`。主程序把参数转为：

```c
struct wb_touchpad_drag_config drag_config = {
    .activation_distance = 48,
    .movement_numerator = options.three_finger_speed_percent,
    .movement_denominator = 100,
    .continuation_ms = options.three_finger_continuation_ms,
};
```

ready 辅助日志改为：

```text
TOUCHPAD_READY physical=/dev/input/eventN virtual=WorkBuddy_Virtual_Mouse threshold=48 speed=25% continuation=1000ms
```

- [ ] **Step 3: 给 bridge 增加 tick 与动态 timeout**

```c
int wb_touchpad_bridge_drain(struct wb_touchpad_bridge *bridge, int64_t now_ms);
int wb_touchpad_bridge_tick(struct wb_touchpad_bridge *bridge, int64_t now_ms);
int wb_touchpad_bridge_next_timeout_ms(const struct wb_touchpad_bridge *bridge,
                                       int64_t now_ms,
                                       int fallback_ms);
```

主程序增加：

```c
static int64_t monotonic_ms(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return -1;
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}
```

循环结构必须是：

```c
int64_t before = monotonic_ms();
int timeout = wb_touchpad_bridge_next_timeout_ms(&touchpad, before, 500);
int ready = poll(poll_fds, 2, timeout);
int64_t after = monotonic_ms();
if (wb_touchpad_bridge_tick(&touchpad, after) != 0) { /* fatal */ }
if (ready > 0 && touchpad has POLLIN) {
    if (wb_touchpad_bridge_drain(&touchpad, after) != 0) { /* fatal */ }
}
```

先 tick 再 drain，使恰好到 deadline 的新触摸按“旧拖动结束、新拖动重新 arm”处理。

- [ ] **Step 4: 统一异常释放**

`SYN_DROPPED` 先调用 `wb_touchpad_drag_cancel()` 并 `emit_output()`，然后设置 `errno=EOVERFLOW` 返回失败；主 cleanup 再调用 close 时不产生第二次 UP。EOF、POLLHUP、SIGTERM 和 Java `Process.destroy()` 都走同一 close 路径。

- [ ] **Step 5: 构建测试与 ARM64 二进制**

```powershell
gcc native-bridge\tests\bridge_options_test.c native-bridge\src\bridge_options.c -I native-bridge\src -std=c11 -Wall -Wextra -Werror -pedantic -o .tools\three-finger-drag\bridge_options_test.exe
& .\.tools\three-finger-drag\bridge_options_test.exe
& .\.tools\three-finger-drag\touchpad_drag_state_test.exe

$toolRoot = 'C:\Users\wanghuanyu\WorkBuddy\2026-09-12-19-05-57\tools'
$clang = "$toolRoot\android-sdk\ndk\29.0.14206865\toolchains\llvm\prebuilt\windows-x86_64\bin\aarch64-linux-android26-clang.cmd"
& $clang -std=c11 -O2 -Wall -Wextra -Werror -pedantic -I native-bridge\src native-bridge\src\alttab_evdev_bridge.c native-bridge\src\bridge_options.c native-bridge\src\key_remap.c native-bridge\src\native_protocol.c native-bridge\src\touchpad_drag_state.c native-bridge\src\touchpad_bridge.c -o .tools\three-finger-drag\alttab_evdev_bridge
git diff --check
```

Expected: 两个状态/参数测试输出 `OK`，NDK 严格构建零警告。

- [ ] **Step 6: 提交**

```powershell
git add native-bridge/src/bridge_options.h native-bridge/src/bridge_options.c native-bridge/tests/bridge_options_test.c native-bridge/src/touchpad_bridge.h native-bridge/src/touchpad_bridge.c native-bridge/src/alttab_evdev_bridge.c
git commit -m "feat: configure and time native touchpad gestures"
```

---

### Task 4: 把配置从 WorkBuddy UI 传到 Shizuku 原生进程

**Files:**

- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/ports/GestureSettings.java`
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/adapters/SharedPreferencesGestureSettings.java`
- Create: `android-bridge/app/src/test/java/local/workbuddy/alttab/adapters/SharedPreferencesGestureSettingsTest.java`
- Modify: `android-bridge/app/src/main/java/local/workbuddy/alttab/shizuku/BridgeBinderProtocolV2.java`
- Modify: `android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeController.java`
- Modify: `android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeUserService.java`
- Modify: `android-bridge/app/src/main/java/local/workbuddy/alttab/nativebridge/NativeBridgeSupervisor.java`
- Create: `android-bridge/app/src/test/java/local/workbuddy/alttab/nativebridge/NativeBridgeSupervisorTest.java`
- Modify: `android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeApplication.java`
- Modify: `android-bridge/app/src/main/java/local/workbuddy/alttab/BootReceiver.java`
- Modify: `android-bridge/app/src/main/java/local/workbuddy/alttab/ProbeActivity.java`
- Modify: `android-bridge/app/src/main/res/values/strings.xml`

**Persistence port:**

```java
public interface GestureSettings {
    GestureConfig load();
    void save(GestureConfig config);
}
```

SharedPreferences keys 固定为：

```text
three_finger_speed_percent
three_finger_continuation_ms
```

- [ ] **Step 1: 先写配置存储、命令渲染和相同配置不重启的失败测试**

适配器测试使用一个内存 `Storage` seam，不依赖 Android instrumentation；Supervisor 暴露 package-private 纯函数：

```java
static List<String> commandFor(String executable, GestureConfig config);
```

断言结果严格等于：

```text
[/data/local/tmp/alttab_evdev_bridge,
 --three-finger-speed-percent, 25,
 --three-finger-continuation-ms, 1000]
```

- [ ] **Step 2: 扩展 Binder V2，但不改旧事务编号**

```java
public static final int TRANSACTION_STATUS = IBinder.FIRST_CALL_TRANSACTION;
public static final int TRANSACTION_SET_ACTIVE = IBinder.FIRST_CALL_TRANSACTION + 1;
public static final int TRANSACTION_SET_GESTURE_CONFIG = IBinder.FIRST_CALL_TRANSACTION + 2;
```

请求 Parcel 顺序固定为 `speedPercent`、`continuationMs`；UserService 使用 `GestureConfig.normalized()` 再交给 supervisor，绝不信任跨进程整数。

- [ ] **Step 3: Supervisor 保存最新配置并只在值变化时安全重启**

```java
private final AtomicReference<GestureConfig> gestureConfig =
        new AtomicReference<>(GestureConfig.defaults());
private volatile long configGeneration;

public void setGestureConfig(GestureConfig next) {
    GestureConfig previous = gestureConfig.getAndSet(next);
    if (previous.equals(next)) return;
    configGeneration++;
    Process current = process;
    if (current != null) current.destroy(); // SIGTERM -> native cleanup -> BTN_LEFT UP
    synchronized (signal) { signal.notifyAll(); }
}
```

`run()` 启动时快照 generation。若进程退出时 generation 已变化，立即用新参数重启，清零 backoff，不报告 `native-exited`；真正崩溃仍走原 backoff。

- [ ] **Step 4: Controller 在绑定和重连时自动推送持久化配置**

`BridgeController` 持有 `AtomicReference<GestureConfig> desiredGestureConfig`。接口为：

```java
public static synchronized void initialize(Context context, GestureConfig initial);
public static boolean setGestureConfig(GestureConfig config);
```

`onServiceConnected()` 在标记 connected 后立即发送当前 desired config。`BridgeApplication` 和 `BootReceiver` 都从 `SharedPreferencesGestureSettings` load 后初始化，确保重启 Pad 后恢复用户值。

- [ ] **Step 5: 在 ProbeActivity 增加两个简洁滑块**

新增 UI 字段和帮助文案：

```text
三指拖动速度：25%
三指续拖时间：1000 ms
续拖大于 0 ms 时请配合 Moonlight WorkBuddy 使用。
```

实现一个通用 helper，SeekBar 的 progress 是离散档位，不直接用业务值：

```java
private View createSteppedSlider(String title,
                                 int min, int max, int step, int current,
                                 IntConsumer onCommitted);
```

只在 `onStopTrackingTouch()` 时 `settings.save(next)` 和 `BridgeController.setGestureConfig(next)`，拖动滑块过程中仅更新标签，避免每个像素都重启 native。

- [ ] **Step 6: 运行 Java 回归并提交**

```powershell
$env:JAVA_HOME = (Get-ChildItem 'C:\Program Files\Eclipse Adoptium' -Directory | Where-Object Name -Like 'jdk-17*' | Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty FullName)
$gradlew = 'C:\Users\wanghuanyu\WorkBuddy\2026-09-12-19-05-57\tools\MoonlightWorkBuddy\gradlew.bat'
& $gradlew -p android-bridge :app:testDebugUnitTest --no-daemon
git diff --check
git add android-bridge
git commit -m "feat: configure touchpad gestures from WorkBuddy"
```

Expected: 现有 18+ 测试和新增配置/Supervisor 测试全部通过。

---

### Task 5: 构建并验证 WorkBuddy 0.9.0 候选版，但暂不启用 1 秒续拖

**Files:**

- Replace: `android-bridge/app/src/main/assets/alttab_evdev_bridge`
- Create: `docs/verification/2026-09-13-touchpad-gesture-normalization.md`

- [ ] **Step 1: 把严格构建的 native 写入 APK asset，并验证哈希相等**

```powershell
Copy-Item -LiteralPath .tools\three-finger-drag\alttab_evdev_bridge -Destination android-bridge\app\src\main\assets\alttab_evdev_bridge -Force
(Get-FileHash .tools\three-finger-drag\alttab_evdev_bridge -Algorithm SHA256).Hash
(Get-FileHash android-bridge\app\src\main\assets\alttab_evdev_bridge -Algorithm SHA256).Hash
```

- [ ] **Step 2: 运行 WorkBuddy 全回归**

```powershell
& .\protocol\tests\GeneratedSchemaTest.ps1
& .\.tools\three-finger-drag\key_remap_test.exe
& .\.tools\three-finger-drag\native_protocol_test.exe
& .\.tools\three-finger-drag\touchpad_drag_state_test.exe
& .\.tools\three-finger-drag\bridge_options_test.exe
$env:JAVA_HOME = (Get-ChildItem 'C:\Program Files\Eclipse Adoptium' -Directory | Where-Object Name -Like 'jdk-17*' | Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty FullName)
$gradlew = 'C:\Users\wanghuanyu\WorkBuddy\2026-09-12-19-05-57\tools\MoonlightWorkBuddy\gradlew.bat'
& $gradlew -p android-bridge :app:testDebugUnitTest :app:assembleDebug --no-daemon
git diff --check
```

- [ ] **Step 3: 安装前记录旧 APK 与 native，保留 `.previous` 回滚**

使用全局 adb skill 中的 canonical `adb.exe`，先 `adb devices -l` 确认目标序列；记录已安装 `local.workbuddy.alttab` 的版本、证书和 `/data/local/tmp/alttab_evdev_bridge` SHA-256。禁止依赖旧的 mDNS 端口。

- [ ] **Step 4: 首次安装后把续拖滑块设为 `0 ms`**

此时先只验证 `25%` 三指速度、配置持久化、native 启动参数、暂停/恢复和原有键盘规则。等 Task 8 的 Moonlight gate 验证后，再切换到 `1000 ms`，避免官方 Moonlight 在宽限期收到实体一指移动。

- [ ] **Step 5: 记录候选哈希与测试结果并提交**

```powershell
git add android-bridge/app/src/main/assets/alttab_evdev_bridge docs/verification/2026-09-13-touchpad-gesture-normalization.md
git commit -m "build: package configurable touchpad bridge"
```

---

### Task 6: 在 Moonlight 中实现纯 Java 输入归一化器

**Repository:** `C:\Users\wanghuanyu\WorkBuddy\2026-09-12-19-05-57\tools\MoonlightWorkBuddy`

**Files:**

- Create: `app/src/main/java/com/limelight/binding/input/ExternalPointerNormalizer.java`
- Create: `app/src/test/java/com/limelight/binding/input/ExternalPointerNormalizerTest.java`
- Modify: `app/build.gradle`

**Class contract:**

```java
public final class ExternalPointerNormalizer {
    static final String XIAOMI_TOUCH = "Xiaomi Touch";
    static final String WORKBUDDY_MOUSE = "WorkBuddy Virtual Mouse";

    public ExternalPointerNormalizer(int scrollPercent);
    public boolean isXiaomiCapturedTouchpad(String deviceName,
                                             boolean capturedRelative);
    public void observePrimaryButton(String deviceName, boolean primaryDown);
    public boolean shouldSuppressPhysicalMotion(String deviceName,
                                                 boolean capturedRelative,
                                                 boolean motionLikeAction);
    public short normalizeVerticalScroll(float rawAxis);
    public short normalizeHorizontalScroll(float rawAxis);
    public boolean reset(); // 返回 reset 前 WorkBuddy primary 是否仍按下
}
```

该类不 import `MotionEvent` 或 `InputDevice`，使 JVM 测试不需要 Robolectric。`Game` 负责把 Android event 转成字符串、布尔值和 axis 数值。

- [ ] **Step 1: 先增加 JUnit 依赖和失败测试**

`app/build.gradle`：

```groovy
testImplementation 'junit:junit:4.13.2'
```

测试覆盖：

- 精确设备名和 captured-relative 双条件；`Xiaomi Mouse`、null/空名称不匹配。
- 100% 时 `235.19 -> 235`，而不是旧的 short-cast(`235.19 * 120`)。
- 25% 连续四次 `+1 -> 0,0,0,1`；连续四次 `-1 -> 0,0,0,-1`。
- 300% 正负数、垂直/水平余数互不影响。
- 超过 short 上下界时饱和到 `32767/-32768`，不回绕。
- WorkBuddy primary down 后只抑制 Xiaomi captured motion-like 事件；WorkBuddy 自身事件和普通鼠标不抑制。
- WorkBuddy primary up 解除 gate；`reset()` 仅第一次返回 true，并清空两个余数。

```powershell
git switch -c feature/workbuddy-touchpad-input
.\gradlew.bat testNonRootDebugUnitTest --tests com.limelight.binding.input.ExternalPointerNormalizerTest --no-daemon --stacktrace
```

Expected: 首次因类不存在而失败。

- [ ] **Step 2: 实现两个独立 axis accumulator**

```java
private static final class AxisAccumulator {
    private double remainder;

    short scale(float raw, double gain) {
        double value = raw * gain + remainder;
        if (!Double.isFinite(value)) { remainder = 0; return 0; }
        if (value >= Short.MAX_VALUE) { remainder = 0; return Short.MAX_VALUE; }
        if (value <= Short.MIN_VALUE) { remainder = 0; return Short.MIN_VALUE; }
        int whole = (int) value; // 朝 0 截断，余数保留正负号
        remainder = value - whole;
        return (short) whole;
    }
}
```

构造器 clamp `scrollPercent` 到 `25..300`，并保存 `gain=scrollPercent/100.0`。不要把普通鼠标送进这个 scaler。

- [ ] **Step 3: 实现 gate 状态**

`observePrimaryButton()` 只有精确匹配 `WorkBuddy Virtual Mouse` 时才更新 `workBuddyPrimaryDown`。`shouldSuppressPhysicalMotion()` 只在三条件全为 true 时返回 true：WorkBuddy 左键按下、Xiaomi captured relative、action 是 MOVE/HOVER_MOVE/SCROLL。

- [ ] **Step 4: 跑绿测并提交**

```powershell
.\gradlew.bat testNonRootDebugUnitTest --tests com.limelight.binding.input.ExternalPointerNormalizerTest --no-daemon --stacktrace
git diff --check
git add app/build.gradle app/src/main/java/com/limelight/binding/input/ExternalPointerNormalizer.java app/src/test/java/com/limelight/binding/input/ExternalPointerNormalizerTest.java
git commit -m "feat: normalize WorkBuddy touchpad input"
```

Expected: 归一化器测试全部通过。

---

### Task 7: 把归一化器接入 Moonlight `Game`，处理跨设备按钮状态

**Files:**

- Modify: `app/src/main/java/com/limelight/Game.java`
- Modify: `app/src/main/java/com/limelight/preferences/PreferenceConfiguration.java`
- Modify: `app/src/main/res/xml/preferences.xml`
- Modify: `app/src/main/res/values/strings.xml`
- Modify: `app/src/main/res/values-zh-rCN/strings.xml`

- [ ] **Step 1: 增加滚动设置**

`PreferenceConfiguration` 新增：

```java
private static final String XIAOMI_TOUCHPAD_SCROLL_PERCENT_PREF_STRING =
        "xiaomi_touchpad_scroll_percent";
private static final int DEFAULT_XIAOMI_TOUCHPAD_SCROLL_PERCENT = 100;
public int xiaomiTouchpadScrollPercent;
```

`readPreferences()` 用 `Math.max(25, Math.min(300, prefs.getInt(XIAOMI_TOUCHPAD_SCROLL_PERCENT_PREF_STRING, DEFAULT_XIAOMI_TOUCHPAD_SCROLL_PERCENT)))` 读取。此设置在下一次进入串流时生效，与现有 `prefConfig` 生命周期一致。

在 `category_input_settings` 下添加：

```xml
<com.limelight.preferences.SeekBarPreference
    android:key="xiaomi_touchpad_scroll_percent"
    android:defaultValue="100"
    seekbar:min="25"
    android:max="300"
    seekbar:step="5"
    seekbar:keyStep="5"
    android:title="@string/title_xiaomi_touchpad_scroll_speed"
    android:summary="@string/summary_xiaomi_touchpad_scroll_speed"
    android:text="@string/suffix_percent" />
```

中文说明必须明确：“100% 是修正后的基准值，不是原版速度；只影响 Xiaomi Touch 双指滚动。”

- [ ] **Step 2: 初始化归一化器**

`Game` 增加字段，并紧跟 preference 读取初始化：

```java
private ExternalPointerNormalizer externalPointerNormalizer;

prefConfig = PreferenceConfiguration.readPreferences(this);
externalPointerNormalizer =
        new ExternalPointerNormalizer(prefConfig.xiaomiTouchpadScrollPercent);
```

- [ ] **Step 3: 把 gate 放在 `lastButtonState` 差分之前**

在鼠标分支中先完成现有 DeX `buttonState` 修正和 capture-active 检查，然后执行：

```java
InputDevice device = event.getDevice();
String deviceName = device != null ? device.getName() : "";
boolean capturedRelative = eventSource == InputDevice.SOURCE_MOUSE_RELATIVE;
boolean motionLikeAction = event.getActionMasked() == MotionEvent.ACTION_MOVE
        || event.getActionMasked() == MotionEvent.ACTION_HOVER_MOVE
        || event.getActionMasked() == MotionEvent.ACTION_SCROLL;

externalPointerNormalizer.observePrimaryButton(
        deviceName,
        (buttonState & MotionEvent.BUTTON_PRIMARY) != 0);
if (externalPointerNormalizer.shouldSuppressPhysicalMotion(
        deviceName, capturedRelative, motionLikeAction)) {
    return true;
}

int changedButtons = buttonState ^ lastButtonState;
```

必须从原来的 1816 行附近移除提前计算 `changedButtons`。原因是 `lastButtonState` 是跨设备全局值：WorkBuddy 已按下左键时，Xiaomi 的 `buttonState=0` 若先参与 XOR，会被误判成虚拟左键抬起。

- [ ] **Step 4: 只替换 Xiaomi 的 ACTION_SCROLL 路径**

```java
if (event.getActionMasked() == MotionEvent.ACTION_SCROLL) {
    if (externalPointerNormalizer.isXiaomiCapturedTouchpad(
            deviceName, capturedRelative)) {
        conn.sendMouseHighResScroll(externalPointerNormalizer
                .normalizeVerticalScroll(event.getAxisValue(MotionEvent.AXIS_VSCROLL)));
        conn.sendMouseHighResHScroll(externalPointerNormalizer
                .normalizeHorizontalScroll(event.getAxisValue(MotionEvent.AXIS_HSCROLL)));
    } else {
        // 普通鼠标保持原实现，不做统一抽象或余数处理
        conn.sendMouseHighResScroll((short)
                (event.getAxisValue(MotionEvent.AXIS_VSCROLL) * 120));
        conn.sendMouseHighResHScroll((short)
                (event.getAxisValue(MotionEvent.AXIS_HSCROLL) * 120));
    }
}
```

- [ ] **Step 5: 丢失焦点时安全清理 WorkBuddy 左键**

```java
private void resetExternalPointerState() {
    if (externalPointerNormalizer != null
            && externalPointerNormalizer.reset()
            && conn != null) {
        conn.sendMouseButtonUp(MouseButtonPacket.BUTTON_LEFT);
        lastButtonState &= ~MotionEvent.BUTTON_PRIMARY;
    }
}
```

在 `onWindowFocusChanged(false)` 和 `setInputGrabState(false)` 调用。`reset()` 的返回值保证同一次失焦只补发一次 UP；它只针对已确认来自 WorkBuddy 虚拟鼠标的 primary，不影响普通鼠标按钮。

- [ ] **Step 6: 编译与提交**

```powershell
.\gradlew.bat testNonRootDebugUnitTest assembleNonRootDebug --no-daemon --stacktrace
git diff --check
git add app/src/main/java/com/limelight/Game.java app/src/main/java/com/limelight/preferences/PreferenceConfiguration.java app/src/main/res/xml/preferences.xml app/src/main/res/values/strings.xml app/src/main/res/values-zh-rCN/strings.xml
git commit -m "feat: integrate Xiaomi touchpad normalization"
```

Expected: 单元测试通过，`assembleNonRootDebug` 输出 APK。

---

### Task 8: 建立可与官方版并存的 Moonlight WorkBuddy APK

**Files:**

- Modify: `app/build.gradle`
- Modify: `README.md`

- [ ] **Step 1: 修改应用身份，不修改 namespace**

```groovy
defaultConfig {
    versionName "12.2-workbuddy.1"
    versionCode = 315001
}

productFlavors {
    root { applicationId "com.limelight.workbuddy.root" }
    nonRoot { applicationId "com.limelight.workbuddy" }
}

buildTypes {
    debug {
        applicationIdSuffix ".debug"
        resValue "string", "app_label", "Moonlight WorkBuddy (Debug)"
        resValue "string", "app_label_root", "Moonlight WorkBuddy Root (Debug)"
    }
    release {
        // base applicationId 已独立，不再添加 .unofficial
        resValue "string", "app_label", "Moonlight WorkBuddy"
        resValue "string", "app_label_root", "Moonlight WorkBuddy Root"
        minifyEnabled true
        proguardFiles getDefaultProguardFile('proguard-android-optimize.txt'), 'proguard-rules.pro'
    }
}
```

- [ ] **Step 2: README 记录 fork 差异与许可证**

只列三项差异：小米触控板滚动倍率、WorkBuddy drag gate、独立包名。记录基线提交和 GPL-3.0；不宣称通用支持所有 Android 触控板。

- [ ] **Step 3: 在普通 Windows 上做 clean build**

```powershell
$env:JAVA_HOME = (Get-ChildItem 'C:\Program Files\Eclipse Adoptium' -Directory | Where-Object Name -Like 'jdk-17*' | Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty FullName)
$env:ANDROID_HOME = 'C:\Users\wanghuanyu\WorkBuddy\2026-09-12-19-05-57\tools\android-sdk'
.\gradlew.bat clean testNonRootDebugUnitTest assembleNonRootDebug --no-daemon --stacktrace
```

检查：

```powershell
$apk = '.\app\build\outputs\apk\nonRoot\debug\app-nonRoot-debug.apk'
& "$env:ANDROID_HOME\build-tools\37.0.0\aapt2.exe" dump badging $apk | Select-String "package: name|versionCode|versionName|application-label"
& "$env:ANDROID_HOME\build-tools\37.0.0\apksigner.bat" verify --verbose --print-certs $apk
(Get-FileHash $apk -Algorithm SHA256).Hash
```

Expected: package `com.limelight.workbuddy.debug`、版本 `12.2-workbuddy.1/315001`、签名 v1/v2 有效。

- [ ] **Step 4: 安装调试版并重新配对 Sunshine**

通过全局 adb skill 先解析当前无线设备序列，再执行：

```powershell
$adb = 'C:\Users\wanghuanyu\WorkBuddy\2026-09-12-19-05-57\tools\platform-tools\adb.exe'
$serial = (& $adb devices | Select-Object -Skip 1 | Where-Object { $_ -match '^\S+\s+device$' } | ForEach-Object { ($_ -split '\s+')[0] })
if (@($serial).Count -ne 1) { throw "需要且只能有一个在线 Pad，当前为 $(@($serial).Count) 个" }
& $adb -s $serial install -r .\app\build\outputs\apk\nonRoot\debug\app-nonRoot-debug.apk
& $adb -s $serial shell pm list packages | Select-String 'com.limelight$|com.limelight.workbuddy.debug'
```

确认官方版仍存在；Moonlight WorkBuddy 首次与 Sunshine 重新配对。

- [ ] **Step 5: 先验证 gate，再启用 WorkBuddy 1000 ms**

1. WorkBuddy 续拖保持 `0 ms`，验证 Moonlight WorkBuddy 普通鼠标、键盘、手柄、双指滚动。
2. 使用 Windows 低级鼠标 hook 验证 Xiaomi 双指单包 P95 不超过 240，且不再出现上万增量。
3. 把 WorkBuddy 续拖改为 `1000 ms`。
4. 三指抬起后先落 1 指、2 指、3 指；Windows 被拖对象不得被实体链路带动，第三指恢复帧也不得跳跃。
5. 虚拟 BTN_LEFT up 后，一指与双指实体事件必须立即恢复。

- [ ] **Step 6: 提交并推送 fork 分支**

```powershell
git add app/build.gradle README.md
git commit -m "build: create side-by-side Moonlight WorkBuddy app"
git push -u origin feature/workbuddy-touchpad-input
```

---

### Task 9: 端到端回归、手感校准、正式签名与回滚演练

**Files:**

- Modify: WorkBuddy `docs/verification/2026-09-13-touchpad-gesture-normalization.md`
- Modify: WorkBuddy `README.md`
- Modify only if calibration changes defaults: WorkBuddy `protocol/bridge-schema.json` and regenerated outputs
- Modify only if calibration changes default: Moonlight `PreferenceConfiguration.java` / `preferences.xml`

- [ ] **Step 1: 自动回归**

WorkBuddy：schema、全部 JVM、`key_remap_test`、`native_protocol_test`、`touchpad_drag_state_test`、`bridge_options_test`、NDK 严格构建、APK build/signature/hash、两个虚拟设备能力脚本。

Moonlight：`testNonRootDebugUnitTest`、`assembleNonRootDebug`、package/version/signature、`git diff --check`。

- [ ] **Step 2: 逐项真机验收**

每项至少 10 次：

1. 双指以低/中/高速度滑动相同 30 mm，Windows wheel 单包 P95 `<=240`，没有上万增量和 short 回绕。
2. 一指和三指各移动 30 mm，三指位移先达到一指的 `0.85..1.15`；用户再按文字选择与拖窗手感决定是否从 25% 以 5% 调整。
3. 间隔 `200/500/900 ms` 连续；`1000/1100 ms` 结束。
4. 恢复三指首帧位移为 0，没有第二次 LEFT_DOWN。
5. 四指、暂停、Shizuku 停止、native SIGTERM、`SYN_DROPPED` 测试都只产生一次 LEFT_UP。
6. Alt+Tab、Command→Win、四个方向键全部回归。
7. 3408×2272、144 Hz 连续串流 20 分钟；native 空闲 CPU `<1%`，事件处理 P95 `<2 ms`。

- [ ] **Step 3: 参数校准只改单一事实源**

若用户调整三指默认速度/续拖时间，只改 WorkBuddy schema 后重新生成；若调整双指默认速度，同时改 `PreferenceConfiguration.DEFAULT_XIAOMI_TOUCHPAD_SCROLL_PERCENT` 和 XML default，并由测试断言二者一致。禁止在 `Game.java` 或 C 状态机中散落魔法数。

- [ ] **Step 4: 生成正式 Moonlight WorkBuddy APK**

```powershell
.\gradlew.bat assembleNonRootRelease --no-daemon --stacktrace
```

用仓库外专用 keystore 对 `app-nonRoot-release-unsigned.apk` 执行 `zipalign` 和 `apksigner sign`；密码交互输入，不进入命令行、日志或 Git。验证正式 package 为 `com.limelight.workbuddy`、签名 v1/v2、版本 `315001`，记录 SHA-256。

- [ ] **Step 5: 回滚演练**

- Moonlight 回滚：把 WorkBuddy 续拖设为 `0 ms`，退出 fork，打开官方 `com.limelight`。
- WorkBuddy 回滚：暂停桥接，恢复 `/data/local/tmp/alttab_evdev_bridge.previous`，确认虚拟左键未卡住，再恢复 0.8.0 APK。
- 回滚不改变 Sunshine、Tailscale、显示分辨率或路由器配置。

- [ ] **Step 6: 最终提交**

```powershell
git -C 'C:\Users\wanghuanyu\WorkBuddy\2026-09-12-19-05-57\tools\AltTabBridge\.worktrees\three-finger-drag' status --short
git -C 'C:\Users\wanghuanyu\WorkBuddy\2026-09-12-19-05-57\tools\MoonlightWorkBuddy' status --short
```

两个仓库都必须只剩有意变更；记录 WorkBuddy commit、Moonlight commit、两个 APK SHA-256、签名证书摘要、Pad 实装版本与所有真机结论。然后分别提交验证文档；只有全部验收通过后才宣布完成。
