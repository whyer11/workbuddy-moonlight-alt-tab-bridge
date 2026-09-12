# Pad 桥接规则清单与实时状态实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 Pad 桥接 App 中展示所有键盘桥接规则的配置状态，并实时确认 Pad 原生桥接和 Windows Host 是否在线。

**Architecture:** Android 使用纯 Java 规则目录和状态归约器驱动页面；Shizuku Binder 返回 Pad 原生进程状态；Windows Host 通过只读 UDP 协议返回版本和支持的规则 ID，Pad 经 Tailscale 定时探测。任何状态查询失败只改变页面显示，不影响现有按键转发。

**Tech Stack:** Android Java、Shizuku UserService/Binder、Java UDP、Win32 C、Winsock2、Linux evdev/uinput、JUnit 4、MinGW GCC、PowerShell、ADB。

**Spec:** `docs/superpowers/specs/2026-09-13-bridge-rule-status-design.md`

## Global Constraints

- 保留包名 `local.workbuddy.alttab`、Shizuku 授权、开机恢复和 native supervisor。
- 保留 `Alt+Tab -> Alt+F12 -> Windows Alt+Tab` 与 `Left Meta -> Scroll Lock -> Left Windows` 的按键行为。
- 状态协议固定为只读 UDP `WB_STATUS_V1`，端口固定为 `57420`，不提供任何控制或按键注入命令。
- Host 防火墙入站范围仅允许 Tailscale CGNAT `100.64.0.0/10`。
- 页面前台每两秒刷新；后台停止轮询；网络或状态查询失败不得停止现有桥接。
- 电脑地址保存在 App 私有配置，不写入 Git 仓库或源码。

---

### Task 1: 固化完整 Win 键桥接基线

**Files:**
- Modify: `README.md`
- Modify: `docs/verification/2026-09-12-end-to-end.md`
- Existing changes: `native-bridge/src/key_remap.c`
- Existing changes: `native-bridge/src/key_remap.h`
- Existing changes: `native-bridge/src/alttab_evdev_bridge.c`
- Existing changes: `native-bridge/tests/key_remap_test.c`
- Existing changes: `windows-host/src/WorkBuddyAltTabHost.c`
- Existing test: `windows-host/tests/WinKeyBridgeIntegrationTest.c`
- Existing changes: `android-bridge/app/build.gradle`
- Existing changes: `android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeController.java`
- Existing binary: `android-bridge/app/src/main/assets/alttab_evdev_bridge`

**Interfaces:**
- Consumes: 已实机确认的 `Left Meta -> Scroll Lock -> VK_LWIN` 行为。
- Produces: 一个干净、已提交的 `0.5.2` 基线，后续状态功能只在其上增量修改。

- [ ] **Step 1: 重新运行已有 RED/GREEN 测试的 GREEN 端**

Run:

```powershell
gcc native-bridge/tests/key_remap_test.c native-bridge/src/key_remap.c -std=c11 -Wall -Wextra -Werror -pedantic -o .tools/build-win-key/key_remap_test.exe
.tools/build-win-key/key_remap_test.exe
gcc windows-host/tests/WinKeyBridgeIntegrationTest.c -std=c11 -O2 -Wall -Wextra -Werror -o .tools/build-win-key/WinKeyBridgeIntegrationTest.exe -luser32
.tools/build-win-key/WinKeyBridgeIntegrationTest.exe
```

Expected: `key_remap_test: OK` and `WIN_KEY_BRIDGE down=2 up=2`.

- [ ] **Step 2: 记录实现和实机证据**

在 README 增加完整 Win 键链路；在验证文档记录 Pad 捕获 `KEY_SCROLLLOCK DOWN/UP`、Windows 捕获 `LWIN DOWN/UP`、用户确认 `Command+E` 生效，以及 native、Host、APK SHA-256。

- [ ] **Step 3: 检查只包含预期改动**

Run:

```powershell
git diff --check
git status --short
```

Expected: 无空白错误，改动仅为本任务列出的文件和本计划文档。

- [ ] **Step 4: 提交基线**

```powershell
git add README.md docs/verification/2026-09-12-end-to-end.md native-bridge/src/key_remap.c native-bridge/src/key_remap.h native-bridge/src/alttab_evdev_bridge.c native-bridge/tests/key_remap_test.c windows-host/src/WorkBuddyAltTabHost.c windows-host/tests/WinKeyBridgeIntegrationTest.c android-bridge/app/build.gradle android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeController.java android-bridge/app/src/main/assets/alttab_evdev_bridge
git commit -m "feat: bridge Xiaomi Command key to Windows"
```

---

### Task 2: 建立规则目录和状态归约器

**Files:**
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeRule.java`
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeRules.java`
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeDashboardState.java`
- Test: `android-bridge/app/src/test/java/local/workbuddy/alttab/BridgeRulesTest.java`
- Test: `android-bridge/app/src/test/java/local/workbuddy/alttab/BridgeDashboardStateTest.java`

**Interfaces:**
- Consumes: 稳定规则 ID `alt_tab`、`windows_key`、`arrow_keys`。
- Produces: `BridgeRules.all(): List<BridgeRule>` 与 `BridgeDashboardState.evaluate(boolean padRunning, boolean computerReachable, Set<String> computerRuleIds)`。

- [ ] **Step 1: 写规则目录失败测试**

```java
@Test public void catalog_describes_all_current_bridges() {
    List<BridgeRule> rules = BridgeRules.all();
    assertEquals(Arrays.asList("alt_tab", "windows_key", "arrow_keys"),
            rules.stream().map(BridgeRule::id).collect(Collectors.toList()));
    assertTrue(rules.get(0).requiresComputer());
    assertTrue(rules.get(1).requiresComputer());
    assertFalse(rules.get(2).requiresComputer());
}
```

- [ ] **Step 2: 运行测试并确认因类型不存在而失败**

Run: `$env:JAVA_HOME = (Resolve-Path '.tools/jdk-extract/jdk-17.0.20.1+1'); & '.tools/gradle-extract/gradle-9.6.0/bin/gradle.bat' -p android-bridge :app:testDebugUnitTest --tests local.workbuddy.alttab.BridgeRulesTest`

Expected: FAIL because `BridgeRule` and `BridgeRules` do not exist.

- [ ] **Step 3: 实现最小不可变规则模型**

`BridgeRule` 提供 `id()`、`title()`、`padInput()`、`padOutput()`、`windowsAction()`、`requiresComputer()`；`BridgeRules.all()` 返回不可修改的三项列表，文案与设计文档一致。

- [ ] **Step 4: 写状态归约失败测试**

```java
@Test public void aggregate_distinguishes_pad_failure_computer_offline_and_healthy() {
    assertEquals(OverallStatus.PAD_ERROR,
            BridgeDashboardState.evaluate(false, false, Collections.emptySet()).overall());
    assertEquals(OverallStatus.COMPUTER_OFFLINE,
            BridgeDashboardState.evaluate(true, false, Collections.emptySet()).overall());
    assertEquals(OverallStatus.HEALTHY,
            BridgeDashboardState.evaluate(true, true,
                    new HashSet<>(Arrays.asList("alt_tab", "windows_key"))).overall());
}

@Test public void arrow_keys_do_not_require_computer_host() {
    RuleRuntime arrows = BridgeDashboardState.evaluate(true, false, Collections.emptySet())
            .rule("arrow_keys");
    assertEquals(ComputerState.NOT_REQUIRED, arrows.computerState());
}
```

- [ ] **Step 5: 运行状态测试并确认缺失实现导致失败**

Run: `$env:JAVA_HOME = (Resolve-Path '.tools/jdk-extract/jdk-17.0.20.1+1'); & '.tools/gradle-extract/gradle-9.6.0/bin/gradle.bat' -p android-bridge :app:testDebugUnitTest --tests local.workbuddy.alttab.BridgeDashboardStateTest`

Expected: FAIL because `BridgeDashboardState` does not exist.

- [ ] **Step 6: 实现状态归约并运行全部纯 Java 测试**

实现 `OverallStatus { PAD_ERROR, COMPUTER_OFFLINE, HEALTHY }`、`ComputerState { ONLINE, OFFLINE, NOT_REQUIRED }` 与每规则 `RuleRuntime`。电脑响应缺少某个必需规则 ID 时，该规则视为离线。

Run: `$env:JAVA_HOME = (Resolve-Path '.tools/jdk-extract/jdk-17.0.20.1+1'); & '.tools/gradle-extract/gradle-9.6.0/bin/gradle.bat' -p android-bridge :app:testDebugUnitTest`

Expected: PASS with no test failures.

- [ ] **Step 7: 提交规则模型**

```powershell
git add android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeRule.java android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeRules.java android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeDashboardState.java android-bridge/app/src/test/java/local/workbuddy/alttab/BridgeRulesTest.java android-bridge/app/src/test/java/local/workbuddy/alttab/BridgeDashboardStateTest.java
git commit -m "feat: model bridge rules and dashboard status"
```

---

### Task 3: 让 Shizuku Binder 返回真实 native 进程状态

**Files:**
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeServiceProtocol.java`
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/PadBridgeSnapshot.java`
- Modify: `android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeUserService.java`
- Modify: `android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeController.java`
- Test: `android-bridge/app/src/test/java/local/workbuddy/alttab/PadBridgeSnapshotTest.java`

**Interfaces:**
- Consumes: `BridgeRules.all()` and the existing Shizuku `ServiceConnection`.
- Produces: Binder transaction `BridgeServiceProtocol.TRANSACTION_STATUS`; `BridgeController.padSnapshot(): PadBridgeSnapshot`.

- [ ] **Step 1: 写 Pad 状态失败测试**

```java
@Test public void snapshot_only_runs_when_binder_and_native_process_are_alive() {
    assertFalse(PadBridgeSnapshot.from(true, false, 1, Arrays.asList("alt_tab")).running());
    assertTrue(PadBridgeSnapshot.from(true, true, 1,
            Arrays.asList("alt_tab", "windows_key", "arrow_keys")).running());
}

@Test public void disconnected_or_failed_query_is_offline() {
    assertFalse(PadBridgeSnapshot.offline().running());
    assertEquals(0, PadBridgeSnapshot.offline().protocolVersion());
    assertTrue(PadBridgeSnapshot.offline().ruleIds().isEmpty());
}
```

- [ ] **Step 2: 运行并确认因类型不存在而失败**

Run: `$env:JAVA_HOME = (Resolve-Path '.tools/jdk-extract/jdk-17.0.20.1+1'); & '.tools/gradle-extract/gradle-9.6.0/bin/gradle.bat' -p android-bridge :app:testDebugUnitTest --tests local.workbuddy.alttab.PadBridgeSnapshotTest`

Expected: FAIL because `PadBridgeSnapshot` does not exist.

- [ ] **Step 3: 实现状态协议和 Binder 查询**

使用 `IBinder.FIRST_CALL_TRANSACTION` 作为只读状态事务。reply 按顺序写入 `nativeAlive`、协议版本 `1` 和字符串规则 ID 数组。`BridgeUserService` 用 `bridgeProcess != null && bridgeProcess.isAlive()` 计算真实进程状态；`BridgeController` 保存 `IBinder`，查询异常时返回 `PadBridgeSnapshot.offline()`。

- [ ] **Step 4: 运行 Android 单元测试并真机查询**

Run:

```powershell
$env:JAVA_HOME = (Resolve-Path '.tools/jdk-extract/jdk-17.0.20.1+1')
& '.tools/gradle-extract/gradle-9.6.0/bin/gradle.bat' -p android-bridge :app:testDebugUnitTest
adb -s 0084941049983540 logcat -d -s WorkBuddyAltTab:I
```

Expected: tests PASS；真机日志显示 native `BRIDGE_READY`，断开 Shizuku 时页面模型降级为离线。

- [ ] **Step 5: 提交 Pad 状态接口**

```powershell
git add android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeServiceProtocol.java android-bridge/app/src/main/java/local/workbuddy/alttab/PadBridgeSnapshot.java android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeUserService.java android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeController.java android-bridge/app/src/test/java/local/workbuddy/alttab/PadBridgeSnapshotTest.java
git commit -m "feat: report native bridge health over Binder"
```

---

### Task 4: 在 Windows Host 中提供只读 UDP 状态服务

**Files:**
- Create: `windows-host/src/WorkBuddyStatusProtocol.h`
- Create: `windows-host/src/WorkBuddyStatusProtocol.c`
- Create: `windows-host/src/WorkBuddyStatusServer.h`
- Create: `windows-host/src/WorkBuddyStatusServer.c`
- Modify: `windows-host/src/WorkBuddyAltTabHost.c`
- Test: `windows-host/tests/StatusProtocolTest.c`
- Test: `windows-host/tests/StatusServerIntegrationTest.c`

**Interfaces:**
- Consumes: UDP request `WB_STATUS_V1 <32-hex-nonce>` on port `57420`.
- Produces: `WB_STATUS_V1 <same-nonce> host=0.6.0 rules=alt_tab,windows_key`; `wb_status_server_start()` and `wb_status_server_stop()`.

- [ ] **Step 1: 写协议解析和格式化失败测试**

```c
assert(wb_status_parse_request("WB_STATUS_V1 00112233445566778899aabbccddeeff", nonce, sizeof(nonce)));
assert(strcmp(nonce, "00112233445566778899aabbccddeeff") == 0);
assert(!wb_status_parse_request("RUN_KEY 00112233445566778899aabbccddeeff", nonce, sizeof(nonce)));
assert(wb_status_format_response(buffer, sizeof(buffer), nonce) > 0);
assert(strcmp(buffer, "WB_STATUS_V1 00112233445566778899aabbccddeeff host=0.6.0 rules=alt_tab,windows_key") == 0);
```

- [ ] **Step 2: 编译并确认缺失实现导致失败**

Run:

```powershell
gcc windows-host/tests/StatusProtocolTest.c windows-host/src/WorkBuddyStatusProtocol.c -std=c11 -Wall -Wextra -Werror -pedantic -o .tools/status/StatusProtocolTest.exe
```

Expected: FAIL because protocol files/functions do not exist.

- [ ] **Step 3: 实现严格只读协议并跑绿测试**

仅接受精确前缀和 32 位十六进制 nonce；拒绝尾随字段、控制命令和超长数据报。响应固定返回 Host 版本和两个需要电脑恢复的规则 ID。

Run: same compile command, then `.tools/status/StatusProtocolTest.exe`.

Expected: `status_protocol_test: OK`.

- [ ] **Step 4: 写 UDP 服务集成失败测试**

测试在回环地址启动服务，发送合法请求并断言 nonce 与规则列表；发送 `RUN_KEY` 并断言 400ms 内无响应；停止服务后断言端口释放。

- [ ] **Step 5: 实现 Winsock 服务线程并运行集成测试**

服务绑定 `0.0.0.0:57420`，使用 stop event 和短接收超时退出。只给合法请求发响应；Host 退出时先停服务，再释放键盘钩子与互斥体。

Run:

```powershell
gcc windows-host/tests/StatusServerIntegrationTest.c windows-host/src/WorkBuddyStatusProtocol.c windows-host/src/WorkBuddyStatusServer.c -std=c11 -Wall -Wextra -Werror -pedantic -o .tools/status/StatusServerIntegrationTest.exe -lws2_32
.tools/status/StatusServerIntegrationTest.exe
gcc windows-host/src/WorkBuddyAltTabHost.c windows-host/src/WorkBuddyStatusProtocol.c windows-host/src/WorkBuddyStatusServer.c -std=c11 -O2 -Wall -Wextra -Werror -municode -mwindows -o .tools/status/WorkBuddyAltTabHost.exe -luser32 -lws2_32
```

Expected: integration test PASS and Host strict compilation succeeds.

- [ ] **Step 6: 回归现有 Win 键测试并提交**

Run: `.tools/build-win-key/WinKeyBridgeIntegrationTest.exe`

Expected: `WIN_KEY_BRIDGE down=2 up=2`.

```powershell
git add windows-host/src/WorkBuddyStatusProtocol.h windows-host/src/WorkBuddyStatusProtocol.c windows-host/src/WorkBuddyStatusServer.h windows-host/src/WorkBuddyStatusServer.c windows-host/src/WorkBuddyAltTabHost.c windows-host/tests/StatusProtocolTest.c windows-host/tests/StatusServerIntegrationTest.c
git commit -m "feat: expose read-only bridge status on Windows"
```

---

### Task 5: 实现 Pad 端电脑探测与地址配置

**Files:**
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/ComputerStatusClient.java`
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/ComputerStatus.java`
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/ComputerHostStore.java`
- Modify: `android-bridge/app/src/main/AndroidManifest.xml`
- Test: `android-bridge/app/src/test/java/local/workbuddy/alttab/ComputerStatusClientTest.java`
- Test: `android-bridge/app/src/test/java/local/workbuddy/alttab/ComputerHostStoreTest.java`

**Interfaces:**
- Consumes: 用户保存的 Tailscale IPv4 或 MagicDNS 名称、UDP `WB_STATUS_V1` 响应。
- Produces: `ComputerStatusClient.probe(String host, int timeoutMillis): ComputerStatus`; `ComputerHostStore.get()/set(String)`。

- [ ] **Step 1: 写 UDP 客户端失败测试**

本地假服务分别返回合法响应、错误 nonce、错误协议版本和缺少规则 ID；测试断言只有来源地址、nonce、版本均匹配时 `reachable()` 为 true，超时返回 offline 而不抛出到 UI。

- [ ] **Step 2: 运行并确认缺失实现导致失败**

Run: `$env:JAVA_HOME = (Resolve-Path '.tools/jdk-extract/jdk-17.0.20.1+1'); & '.tools/gradle-extract/gradle-9.6.0/bin/gradle.bat' -p android-bridge :app:testDebugUnitTest --tests local.workbuddy.alttab.ComputerStatusClientTest`

Expected: FAIL because `ComputerStatusClient` does not exist.

- [ ] **Step 3: 实现异步可关闭 UDP 探测器**

每次生成 16 字节 `SecureRandom` nonce，解析响应时验证 `DatagramPacket.getAddress()` 等于解析后的目标地址。`ComputerStatus` 保存 reachable、Host 版本和不可变规则 ID 集；所有 DNS、网络、超时和解析异常返回 offline。

- [ ] **Step 4: 写并实现地址存储测试**

通过小接口隔离 SharedPreferences，测试 trim、空值清除和合法主机名保存。Manifest 增加 `android.permission.INTERNET`，不增加其他网络权限。

- [ ] **Step 5: 运行全部 Android 测试并提交**

Run: `$env:JAVA_HOME = (Resolve-Path '.tools/jdk-extract/jdk-17.0.20.1+1'); & '.tools/gradle-extract/gradle-9.6.0/bin/gradle.bat' -p android-bridge :app:testDebugUnitTest`

Expected: PASS with no failures.

```powershell
git add android-bridge/app/src/main/java/local/workbuddy/alttab/ComputerStatusClient.java android-bridge/app/src/main/java/local/workbuddy/alttab/ComputerStatus.java android-bridge/app/src/main/java/local/workbuddy/alttab/ComputerHostStore.java android-bridge/app/src/main/AndroidManifest.xml android-bridge/app/src/test/java/local/workbuddy/alttab/ComputerStatusClientTest.java android-bridge/app/src/test/java/local/workbuddy/alttab/ComputerHostStoreTest.java
git commit -m "feat: probe Windows bridge status from Pad"
```

---

### Task 6: 构建 Pad 桥接规则状态页面

**Files:**
- Create: `android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeDashboardPresenter.java`
- Modify: `android-bridge/app/src/main/java/local/workbuddy/alttab/ProbeActivity.java`
- Modify: `android-bridge/app/src/main/res/values/strings.xml`
- Modify: `android-bridge/app/build.gradle`
- Modify: `android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeController.java`
- Test: `android-bridge/app/src/test/java/local/workbuddy/alttab/BridgeDashboardPresenterTest.java`

**Interfaces:**
- Consumes: `BridgeRules.all()`、`BridgeController.padSnapshot()`、`ComputerStatusClient.probe()`、`ComputerHostStore`。
- Produces: 页面标题状态、三张规则卡文案，以及只在前台运行的两秒刷新循环。

- [ ] **Step 1: 写 Presenter 失败测试**

```java
@Test public void presenter_renders_configuration_and_runtime_separately() {
    DashboardView view = BridgeDashboardPresenter.render(BridgeDashboardState.evaluate(
            true, true, new HashSet<>(Arrays.asList("alt_tab", "windows_key"))));
    assertEquals("全链路正常", view.overallText());
    assertEquals("已配置", view.rule("alt_tab").configuredText());
    assertEquals("Pad 端运行中", view.rule("alt_tab").padText());
    assertEquals("电脑端在线", view.rule("alt_tab").computerText());
    assertEquals("电脑端无需桥接", view.rule("arrow_keys").computerText());
}
```

- [ ] **Step 2: 运行并确认缺失 Presenter 导致失败**

Run: `$env:JAVA_HOME = (Resolve-Path '.tools/jdk-extract/jdk-17.0.20.1+1'); & '.tools/gradle-extract/gradle-9.6.0/bin/gradle.bat' -p android-bridge :app:testDebugUnitTest --tests local.workbuddy.alttab.BridgeDashboardPresenterTest`

Expected: FAIL because `BridgeDashboardPresenter` does not exist.

- [ ] **Step 3: 实现数据驱动页面**

保留现有标题、状态和授权按钮；增加电脑地址输入框、保存按钮与“桥接规则”区。遍历 `BridgeRules.all()` 创建规则卡，不按规则 ID 写专用视图。状态用文本和颜色同时表达，避免只靠颜色区分。页尾显示 `BuildConfig.VERSION_NAME`、Pad 协议版本和 Windows Host 响应版本，便于识别版本错配。

- [ ] **Step 4: 实现生命周期安全的刷新循环**

`onResume()` 立即刷新并每两秒调度一次；UDP 和 Binder 查询在单线程 executor 中执行；结果切回主线程前检查 Activity 未销毁；`onPause()` 移除回调并取消未开始任务，现有按键桥接服务不受影响。Intent extra `computerHost` 若非空则保存，供 ADB 部署时预填而不进入源码。

- [ ] **Step 5: 升级版本并运行 Android 测试/构建**

将版本升级为 `versionCode 8`、`versionName 0.6.0`，Shizuku UserService version 同步为 `8`。

Run:

```powershell
$env:JAVA_HOME = (Resolve-Path '.tools/jdk-extract/jdk-17.0.20.1+1')
& '.tools/gradle-extract/gradle-9.6.0/bin/gradle.bat' -p android-bridge :app:testDebugUnitTest
& '.tools/gradle-extract/gradle-9.6.0/bin/gradle.bat' -p android-bridge :app:assembleDebug
```

Expected: tests PASS and debug APK builds successfully.

- [ ] **Step 6: 提交页面**

```powershell
git add android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeDashboardPresenter.java android-bridge/app/src/main/java/local/workbuddy/alttab/ProbeActivity.java android-bridge/app/src/main/res/values/strings.xml android-bridge/app/build.gradle android-bridge/app/src/main/java/local/workbuddy/alttab/BridgeController.java android-bridge/app/src/test/java/local/workbuddy/alttab/BridgeDashboardPresenterTest.java
git commit -m "feat: show bridge rules and live status on Pad"
```

---

### Task 7: 安装、防火墙、端到端验收与发布

**Files:**
- Create: `windows-host/install-status-firewall.ps1`
- Modify: `README.md`
- Modify: `docs/verification/2026-09-12-end-to-end.md`

**Interfaces:**
- Consumes: Windows Host `0.6.0`、Pad APK `0.6.0`、当前电脑 Tailscale IPv4、无线 ADB 目标。
- Produces: 已部署的状态面板、防火墙最小入站规则和可复查的验收记录。

- [ ] **Step 1: 写防火墙脚本静态失败测试**

Run:

```powershell
$script = Get-Content -Raw windows-host/install-status-firewall.ps1
if ($script -notmatch 'UDP' -or $script -notmatch '57420' -or $script -notmatch '100\.64\.0\.0/10') { throw 'firewall scope missing' }
```

Expected before script exists: FAIL.

- [ ] **Step 2: 实现幂等防火墙安装脚本**

脚本固定规则名 `WorkBuddy Bridge Status (Tailscale)`；存在则核对并更新，不存在则创建；协议 UDP、本地端口 57420、远端范围 100.64.0.0/10、方向 Inbound、动作 Allow。脚本不得创建 Any/Any 入站规则。

- [ ] **Step 3: 构建并安全替换 Windows Host**

先运行新 Host `--probe`；退出码 0 后备份已安装 EXE、停止旧进程、原子替换并启动。用 `Get-NetUDPEndpoint -LocalPort 57420`、Host 日志及本机 UDP 集成测试确认服务在线；失败则恢复备份。

- [ ] **Step 4: 发现当前 Tailscale 与无线 ADB 地址**

Run:

```powershell
tailscale ip -4
adb mdns services
adb devices -l
```

若命令不在 PATH，使用已安装程序实际路径；只选择序列号 `0084941049983540` 对应的无线端点。

- [ ] **Step 5: 安装 APK 并预填电脑地址**

Run:

```powershell
adb -s <wireless-endpoint> install -r <signed-0.6.0-apk>
adb -s <wireless-endpoint> shell am start -n local.workbuddy.alttab/.ProbeActivity --es computerHost <computer-tailscale-ip>
```

Expected: App 保留 Shizuku 授权，页面显示三条规则；Pad native 状态为运行中，电脑端为在线，整体为全链路正常。

- [ ] **Step 6: 回归所有自动测试**

Run native、Windows protocol/server、Win key integration、Android unit tests、APK signature verification、`native-bridge/tests/virtual_keyboard_device_test.ps1` 和 `git diff --check`。

Expected: 全部退出码 0；已安装 native/Host/APK hash 与构建产物一致。

- [ ] **Step 7: 实机按键与降级验收**

在 Moonlight 串流中验证一次 Alt+Tab、Command+E 和四个方向键；临时停止 Windows Host，确认页面在超时后显示电脑端离线但方向键仍显示无需电脑端；重新启动 Host 后页面自动恢复全链路正常。

- [ ] **Step 8: 更新文档并提交发布**

README 记录页面与电脑地址配置；验证文档记录版本、hash、进程、UDP、防火墙、页面状态和实机结果。

```powershell
git add windows-host/install-status-firewall.ps1 README.md docs/verification/2026-09-12-end-to-end.md
git commit -m "docs: publish bridge status dashboard verification"
git push origin master
```

Expected: `git status --short` 为空，`git rev-parse HEAD` 与 `git ls-remote origin refs/heads/master` 一致。
