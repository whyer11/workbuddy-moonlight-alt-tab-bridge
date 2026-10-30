# WorkBuddy Moonlight Alt+Tab 桥接工具

这是为当前小米平板、Moonlight 12.1 与 Windows Sunshine 主机制作的本地输入桥接工具。它解决 HyperOS 在 Moonlight 之前截获实体键盘 `Alt+Tab` 和 `Command/Left Meta` 的问题，修复方向键被错误识别为游戏手柄输入的问题，并提供类似 macOS 的三指拖动。

从 KeySwap 1.1.0 起，需要同时交换 Alt/Ctrl 的 Windows 用户可使用 [KeySwap 的统一 Windows 主机](https://github.com/whyer11/KeySwap)。它将本仓库的 Windows 键盘桥接与互换放入同一个状态机，使用一个钩子、托盘和登录任务，避免 Alt 被先换成 Ctrl 后导致 Alt+F12 桥接失效。Pad APK、原生桥接、三指拖动和 Moonlight 适配继续由本仓库及原有 Moonlight 分支提供，无需重新安装。

迁移时退出独立 `WorkBuddyAltTabHost.exe` 并停用它的登录任务，在 KeySwap 中开启“小米 Pad 键盘桥接”。原 UDP 57420 / 协议 V2 与 Tailscale 防火墙规则保持兼容。KeySwap 仓库提供带备份及失败回滚的迁移脚本；不要同时运行两套 Windows 键盘桥接。不需要互换时可关闭 KeySwap 的互换选项，仅保留桥接。本仓库的独立 Windows Host 作为历史实现保留，后续 Windows 功能开发集中在 KeySwap；Pad 端代码继续在本仓库维护。

## 工作方式

1. Pad 上的 Shizuku 以 ADB shell 身份启动 `WorkBuddy Alt+Tab` 用户服务。
2. 用户服务部署并守护 `alttab_evdev_bridge`。底层程序独占读取“Xiaomi Keyboard”，把普通键和方向键原样写回虚拟键盘，将 `Alt+Tab` 改成 `Alt+F12`，并将小米键盘的 `Command/Left Meta` 改成 `Scroll Lock`。
3. 同一底层程序被动读取“Xiaomi Touch”（不独占触摸板），把三指移动转换为只具备左键和相对位移能力的虚拟鼠标。移动超过 48 个原始坐标单位（约 1.5 mm）后开始左键拖动；三指速度、抬手后的续拖时间和重新落下三指的容错窗均可配置。一指或两指在容错窗内发生实际移动、出现第四指或等待超时都会释放左键。三指轻触不会产生单击。
4. Moonlight 12.1 能正常转发 `Alt+F12`、`Scroll Lock` 和虚拟鼠标事件。
5. Windows 登录项常驻程序把 `Alt+F12` 恢复为 Windows `Alt+Tab`，并把 `Scroll Lock` 的按下/抬起恢复为完整的左 Windows 键，因此支持 `Win+E`、`Win+D`、`Win+R` 等组合键。三指拖动不需要电脑端二次桥接。
6. Pad 应用集中展示四条桥接规则。它通过 Shizuku Binder V2 读取原生程序实际声明的能力，并通过 Tailscale 上的只读 UDP 状态接口检查 Windows Host 版本和支持的规则。

按键桥接链路不使用辅助功能、不经过网络控制协议，也不修改 Moonlight 或 Sunshine。状态接口只接受带随机 nonce 的 `WB_STATUS_V2` 查询，只返回版本、协议、核心状态和规则 ID，不提供按键注入或其他控制能力。底层程序只输出启动、停止和发生映射的事件，不记录普通键内容。

## 安装与运行约定

- 产品版本：WorkBuddy Bridge 0.9.3；Pad 应用标识为 `local.workbuddy.alttab`，Windows Host 可单独升级，继续兼容协议 V2 的旧版 Pad 应用。
- Shizuku：13.6.0，ADB 模式，已授予 WorkBuddy 永久权限
- Pad 自启动：Shizuku 与 WorkBuddy Alt+Tab 均已允许
- Windows 程序：WorkBuddy Host，默认安装在 `%USERPROFILE%\Applications\WorkBuddyAltTab\WorkBuddyAltTabHost.exe`
- Windows 登录启动：当前用户交互会话的计划任务 `WorkBuddyAltTabHost`，登录后延迟 15 秒，失败后每分钟重试，最多 3 次。
- Windows 状态接口：UDP 57420，仅允许 Tailscale 网段 `100.64.0.0/10` 入站
- HyperOS“切换应用 Alt Tab”：保持关闭

Shizuku 13.6.0 在 Android 13 及以上、已授予 `WRITE_SECURE_SETTINGS` 且连接受信任 WLAN 时支持非 Root 开机自启动。WorkBuddy 的开机广播接收器会在 Shizuku Binder 就绪后重新绑定守护服务。

## 使用与检查

下载并完整解压 Windows ZIP，在解压目录运行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\install.ps1
```

安装器默认启用开机自启动，并立即启动桥接。旧程序和日志会保存在安装目录的 `backups` 子目录，运行日志为同目录的 `host.log`。默认位置独立于桌面应用的 AppData/MSIX 隔离目录，保证 Windows 登录任务和手工运行访问同一个程序。旧版若安装在 AppData 下，先退出旧程序，再执行新安装器。右键系统托盘的 WorkBuddy 图标，可以勾选或取消“开机自启动”，也可以暂停桥接、打开日志或退出程序。自启实际在当前用户登录后执行，无需管理员权限。

安装时传入 `-NoAutoStart` 可关闭自启；传入 `-NoStart` 则不立即运行。命令行 `--enable-autostart`、`--disable-autostart` 切换同一个计划任务；`--autostart-status` 以退出码 `0` 表示启用、`10` 表示关闭，其他值表示查询失败。安装器和托盘不会各创建一套自启；成功配置后会移除本产品的旧 Run 登录项。

正常情况下不需要手动操作。Pad 重启后保持连接常用 Wi-Fi，打开 Moonlight 串流即可。首次使用状态页时，在“电脑端实时状态”中填写电脑的 Tailscale IPv4 或 MagicDNS 名称并保存；该地址只存放在应用私有配置中。

状态页会分别显示“已配置”“Pad 端运行中”和“电脑端运行中”。方向键与三指拖动只依赖 Pad 桥接，因此会显示“电脑端无需桥接”。健康时每五秒刷新，异常时每两秒刷新；Pad 与电脑探测互相隔离，状态通道失败不会停止输入核心。页面提供“暂停键盘拦截/恢复键盘拦截”开关，便于临时释放实体键盘和虚拟鼠标。“触控板手势”区域可调整三指速度、续拖时间和 `0–500 ms` 的三指落下容错；容错步长为 `1 ms`，滑块松手后立即生效并持久化。

0.9.3 使用 `protocol/bridge-schema.json` 作为 Java、Pad 原生程序和 Windows Host 的唯一版本、规则与手势配置来源。Pad 更新采用 `.new` 暂存、哈希与 `--describe` 能力校验、`.previous` 备份和失败回滚；Windows 键盘核心与 UDP 状态服务分离，即使 UDP 57420 被占用，按键桥接仍会启动。

若快捷键或状态异常：

1. 打开 Shizuku，确认显示“正在运行”。
2. 打开 WorkBuddy Alt+Tab，确认显示“状态：底层桥接已运行”。
3. 确认 Pad 和电脑的 Tailscale 均已连接。
4. 确认 Windows 任务管理器中有 `WorkBuddyAltTabHost.exe`；退出并重新登录 Windows 会自动恢复。
5. 若只有电脑端状态离线，重新运行管理员 PowerShell 脚本 `windows-host\install-status-firewall.ps1`。

## 正式发布

GitHub Actions 会在普通提交和 Pull Request 上重新编译并测试 Pad 原生桥接、Android 应用和 Windows Host。推送与 `android-bridge/app/build.gradle` 中 `versionName` 完全一致的 `v*` 标签后，Release 工作流会使用 GitHub `release` Environment 中的正式证书，自动发布：

- `WorkBuddy-Bridge-<version>.apk`
- `WorkBuddy-Bridge-Windows-<version>.zip`
- `SHA256SUMS.txt`

Windows ZIP 包含托盘程序 `WorkBuddyAltTabHost.exe`、默认启用自启的 `install.ps1`、状态防火墙脚本和版本说明。该 EXE 当前没有 Authenticode 商业代码签名；APK 使用 WorkBuddy Bridge 专用长期证书签名。

`0.9.1` 之前安装到 Pad 的版本使用本地测试证书，因此第一次迁移到 GitHub 正式版时必须先卸载旧 APK，再安装正式 APK并重新授予 Shizuku 权限。完成这一次迁移后，后续正式版本可以原位升级。发布维护顺序固定为：同步修改协议/原生/Android 版本，等待分支 CI 通过，再创建并推送 `v${versionName}` 标签。已存在的标签或 Release 不允许覆盖。

## 回滚

Pad 原生程序的上一版本保存在 `/data/local/tmp/alttab_evdev_bridge.previous`；Windows 安装器将旧 EXE 和版本说明备份在 `%USERPROFILE%\Applications\WorkBuddyAltTab\backups`。回滚 Windows 时先退出托盘程序，再从所需备份目录恢复 EXE 和版本说明。完整卸载前运行 `WorkBuddyAltTabHost.exe --disable-autostart`，然后退出程序并删除安装目录；如需同时清理状态接口，可删除名为 `WorkBuddy Bridge Status (Tailscale)` 的 Windows 防火墙规则。Pad 上可单独卸载 WorkBuddy 和 Shizuku。回滚不会改动 Moonlight、Sunshine、Tailscale 或串流显示配置。
