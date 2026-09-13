# WorkBuddy Moonlight Alt+Tab 桥接工具

这是为当前小米平板、Moonlight 12.1 与 Windows Sunshine 主机制作的本地快捷键桥接工具。它解决 HyperOS 在 Moonlight 之前截获实体键盘 `Alt+Tab` 和 `Command/Left Meta` 的问题，并修复方向键被错误识别为游戏手柄输入的问题。

## 工作方式

1. Pad 上的 Shizuku 以 ADB shell 身份启动 `WorkBuddy Alt+Tab` 用户服务。
2. 用户服务部署并守护 `alttab_evdev_bridge`。底层程序独占读取“Xiaomi Keyboard”，把普通键和方向键原样写回虚拟键盘，将 `Alt+Tab` 改成 `Alt+F12`，并将小米键盘的 `Command/Left Meta` 改成 `Scroll Lock`。
3. Moonlight 12.1 能正常转发 `Alt+F12` 和 `Scroll Lock`。
4. Windows 登录项常驻程序把 `Alt+F12` 恢复为 Windows `Alt+Tab`，并把 `Scroll Lock` 的按下/抬起恢复为完整的左 Windows 键，因此支持 `Win+E`、`Win+D`、`Win+R` 等组合键。
5. Pad 应用集中展示三条桥接规则。它通过 Shizuku Binder 检查 Pad 原生桥接，并通过 Tailscale 上的只读 UDP 状态接口检查 Windows Host 版本和支持的规则。

按键桥接链路不使用辅助功能、不经过网络控制协议，也不修改 Moonlight 或 Sunshine。状态接口只接受带随机 nonce 的 `WB_STATUS_V1` 查询，只返回版本和规则 ID，不提供按键注入或其他控制能力。底层程序只输出启动、停止和发生映射的事件，不记录普通键内容。

## 当前安装状态

- Pad 应用：WorkBuddy Alt+Tab 0.6.0（`local.workbuddy.alttab`）
- Shizuku：13.6.0，ADB 模式，已授予 WorkBuddy 永久权限
- Pad 自启动：Shizuku 与 WorkBuddy Alt+Tab 均已允许
- Windows 程序：WorkBuddy Host 0.6.0，位于 `%LOCALAPPDATA%\WorkBuddyAltTab\WorkBuddyAltTabHost.exe`
- Windows 登录启动：`HKCU\Software\Microsoft\Windows\CurrentVersion\Run\WorkBuddyAltTabHost`
- Windows 状态接口：UDP 57420，仅允许 Tailscale 网段 `100.64.0.0/10` 入站
- HyperOS“切换应用 Alt Tab”：保持关闭

Shizuku 13.6.0 在 Android 13 及以上、已授予 `WRITE_SECURE_SETTINGS` 且连接受信任 WLAN 时支持非 Root 开机自启动。WorkBuddy 的开机广播接收器会在 Shizuku Binder 就绪后重新绑定守护服务。

## 使用与检查

正常情况下不需要手动操作。Pad 重启后保持连接常用 Wi-Fi，打开 Moonlight 串流即可。首次使用状态页时，在“电脑端实时状态”中填写电脑的 Tailscale IPv4 或 MagicDNS 名称并保存；该地址只存放在应用私有配置中。

状态页会分别显示“已配置”“Pad 端运行中”和“电脑端在线”。方向键只依赖 Pad 桥接，因此会显示“电脑端无需桥接”。页面只在前台每两秒查询一次；查询失败只改变状态显示，不会暂停或重启按键桥接。

若快捷键或状态异常：

1. 打开 Shizuku，确认显示“正在运行”。
2. 打开 WorkBuddy Alt+Tab，确认显示“状态：底层桥接已运行”。
3. 确认 Pad 和电脑的 Tailscale 均已连接。
4. 确认 Windows 任务管理器中有 `WorkBuddyAltTabHost.exe`；退出并重新登录 Windows 会自动恢复。
5. 若只有电脑端状态离线，重新运行管理员 PowerShell 脚本 `windows-host\install-status-firewall.ps1`。

## 回滚

卸载 Pad 上的 `WorkBuddy Alt+Tab` 和 Shizuku；删除 Windows 登录项 `WorkBuddyAltTabHost`，结束同名进程，并删除 `%LOCALAPPDATA%\WorkBuddyAltTab`。如需同时清理状态接口，可删除名为 `WorkBuddy Bridge Status (Tailscale)` 的 Windows 防火墙规则。回滚不会改动 Moonlight、Sunshine、Tailscale 或串流显示配置。
