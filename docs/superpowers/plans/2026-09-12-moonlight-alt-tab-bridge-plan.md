# Moonlight Alt+Tab 桥接工具实施计划

> **执行要求：** 每个实现步骤都遵循红—绿—重构；不得在自动测试失败时继续安装到真机。

**目标：** 在小米平板的 Moonlight 串流界面内，让实体键盘 `Alt+Tab` 切换 Windows 前台窗口，同时保持 Moonlight 前台和 Sunshine 会话不断开。

**架构：** Pad 端无障碍服务只在 `com.limelight` 前台时捕获 `Alt+Tab`，向电脑的 Tailscale 地址发送 HMAC 认证 UDP 指令；Windows 用户态常驻程序验证来源、时效、随机数和签名后，通过 `SendInput` 注入一次原子化 `Alt+Tab`。

**技术栈：** Android Java、Android AccessibilityService、UDP/HMAC-SHA256、C#/.NET Framework 4.8、Windows `SendInput`、PowerShell 安装脚本、Windows 任务计划程序和防火墙。

---

## 任务 1：建立工程骨架和可复现构建环境

**文件：**

- 新建：`.gitignore`
- 新建：`README.md`
- 新建：`android-bridge/settings.gradle`
- 新建：`android-bridge/build.gradle`
- 新建：`android-bridge/app/build.gradle`
- 新建：`android-bridge/app/src/main/AndroidManifest.xml`
- 新建：`windows-host/AltTabBridge.Host.csproj`
- 新建：`scripts/build.ps1`

**步骤：**

1. 在仓库私有工具目录准备 JDK 17、Android 命令行工具、SDK Platform 和 Build Tools，并把目录加入 `.gitignore`。
2. 创建最小 Android 应用和 .NET Framework 控制台项目。
3. 运行空项目构建，确认 APK 和 EXE 都可产生。
4. 提交骨架。

## 任务 2：Pad 端快捷键判定逻辑

**文件：**

- 新建：`android-bridge/app/src/main/java/local/workbuddy/alttab/KeyDecision.java`
- 测试：`android-bridge/app/src/test/java/local/workbuddy/alttab/KeyDecisionTest.java`

**步骤：**

1. 先写测试：非 Moonlight、非 Tab、未按 Alt、重复按下均不得触发；Moonlight 中首次 Alt+Tab 按下触发一次；对应抬起被消费。
2. 运行测试并确认因类不存在或行为未实现而失败。
3. 实现最小状态机，让测试通过。
4. 重构命名并重新运行全部 Pad 端单元测试。

## 任务 3：Pad 端无障碍探针与真机可行性验证

**文件：**

- 新建：`android-bridge/app/src/main/java/local/workbuddy/alttab/AltTabAccessibilityService.java`
- 新建：`android-bridge/app/src/main/res/xml/accessibility_service.xml`
- 新建：`android-bridge/app/src/main/res/values/strings.xml`
- 修改：`android-bridge/app/src/main/AndroidManifest.xml`

**步骤：**

1. 服务申请窗口状态事件和键盘过滤能力，只记录前台包名、Alt/Tab 的动作和判定结果，不记录其他按键。
2. 构建并安装调试 APK。
3. 打开系统无障碍设置并启用服务。
4. 分别在 Moonlight 前台、系统快捷键开和关两种状态下，用实体键盘触发一次 Alt+Tab。
5. 通过日志确认服务能收到并消费真实事件；若收不到，停止正式实现并保留原系统配置。

## 任务 4：消息协议与 Pad 端发送器

**文件：**

- 新建：`android-bridge/app/src/main/java/local/workbuddy/alttab/SignedCommand.java`
- 新建：`android-bridge/app/src/main/java/local/workbuddy/alttab/UdpCommandSender.java`
- 测试：`android-bridge/app/src/test/java/local/workbuddy/alttab/SignedCommandTest.java`
- 修改：`android-bridge/app/src/main/java/local/workbuddy/alttab/AltTabAccessibilityService.java`

**步骤：**

1. 先写固定时间、固定随机数、固定密钥的签名向量测试。
2. 运行测试并确认失败。
3. 实现 `v1|timestamp_ms|nonce_hex|ALT_TAB|hmac_hex` 编码与异步 UDP 发送。
4. 将发送器接入快捷键状态机，确保每次按下只发送一条指令。
5. 运行全部 Android 单元测试和 lint。

## 任务 5：Windows 端认证、去重与按键注入

**文件：**

- 新建：`windows-host/CommandValidator.cs`
- 新建：`windows-host/ReplayCache.cs`
- 新建：`windows-host/IKeyInjector.cs`
- 新建：`windows-host/SendInputKeyInjector.cs`
- 新建：`windows-host/Program.cs`
- 测试：`windows-host.tests/CommandValidatorTests.cs`
- 测试：`windows-host.tests/ReplayCacheTests.cs`
- 测试：`windows-host.tests/HostDispatchTests.cs`

**步骤：**

1. 先写正确签名、错误签名、过期时间、未来时间、未知指令、错误来源和重复随机数测试。
2. 运行测试并确认失败。
3. 实现常量时间签名比较、±10 秒时效、来源 IP 白名单和 10 秒去重缓存。
4. 用可替换的 `IKeyInjector` 验证通过时只调用一次，拒绝消息时不调用。
5. 实现 `SendInput` 的 Alt 下、Tab 下、Tab 上、Alt 上序列。
6. 运行全部 Windows 测试。

## 任务 6：安全安装、自动启动与回滚

**文件：**

- 新建：`scripts/install.ps1`
- 新建：`scripts/uninstall.ps1`
- 新建：`scripts/status.ps1`
- 新建：`config/config.example.json`

**步骤：**

1. 生成 32 字节随机密钥，不在终端或日志输出密钥。
2. 将电脑端配置写到当前用户专用目录，并限制 ACL 为当前用户。
3. 创建只绑定 `100.81.73.56:42837` 的常驻程序配置。
4. 创建当前用户登录任务，启动隐藏窗口。
5. 创建仅允许 `100.74.136.33` 入站 UDP 42837 的防火墙规则。
6. 安装 APK，配置电池无限制和后台自启动所需设置。
7. 用 `status.ps1` 验证进程、监听地址、任务和防火墙规则。
8. 用 `uninstall.ps1` 在测试环境演练完整回滚，再重新安装。

## 任务 7：端到端真机验收

**文件：**

- 新建：`docs/verification/2026-09-12-end-to-end.md`

**步骤：**

1. 将 HyperOS 的“切换应用 Alt+Tab”恢复为开启，保证离开 Moonlight 后系统行为正常。
2. 启动实际 `3408×2272 @ 144 FPS` 串流。
3. 记录 Pad 顶层 Activity、Sunshine 活动会话和 Windows 前台窗口。
4. 用 Pad 实体键盘按一次 Alt+Tab。
5. 验证 Pad 仍为 `com.limelight/.Game`、Sunshine 会话未断开、Windows 前台窗口发生变化。
6. 重复快速和长按测试，确认一次按压只切换一次且不会卡住 Alt。
7. 离开 Moonlight 后再次按 Alt+Tab，确认安卓多任务仍正常。
8. 记录命令输出和结果，运行全部自动测试后提交最终版本。
