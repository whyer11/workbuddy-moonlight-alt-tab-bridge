# WorkBuddy Moonlight Alt+Tab 桥接工具

这是为当前小米平板、Moonlight 12.1 与 Windows Sunshine 主机制作的本地快捷键桥接工具。它解决 HyperOS 在 Moonlight 之前截获实体键盘 `Alt+Tab` 和 `Command/Left Meta` 的问题，并修复方向键被错误识别为游戏手柄输入的问题。

## 工作方式

1. Pad 上的 Shizuku 以 ADB shell 身份启动 `WorkBuddy Alt+Tab` 用户服务。
2. 用户服务部署并守护 `alttab_evdev_bridge`。底层程序独占读取“Xiaomi Keyboard”，把普通键和方向键原样写回虚拟键盘，将 `Alt+Tab` 改成 `Alt+F12`，并将小米键盘的 `Command/Left Meta` 改成 `Scroll Lock`。
3. Moonlight 12.1 能正常转发 `Alt+F12` 和 `Scroll Lock`。
4. Windows 登录项常驻程序把 `Alt+F12` 恢复为 Windows `Alt+Tab`，并把 `Scroll Lock` 的按下/抬起恢复为完整的左 Windows 键，因此支持 `Win+E`、`Win+D`、`Win+R` 等组合键。

整个链路不使用辅助功能、不经过网络控制协议、不修改 Moonlight 或 Sunshine。底层程序只输出启动、停止和发生映射的事件，不记录普通键内容。

## 当前安装状态

- Pad 应用：WorkBuddy Alt+Tab 0.5.2（`local.workbuddy.alttab`）
- Shizuku：13.6.0，ADB 模式，已授予 WorkBuddy 永久权限
- Pad 自启动：Shizuku 与 WorkBuddy Alt+Tab 均已允许
- Windows 程序：`%LOCALAPPDATA%\WorkBuddyAltTab\WorkBuddyAltTabHost.exe`
- Windows 登录启动：`HKCU\Software\Microsoft\Windows\CurrentVersion\Run\WorkBuddyAltTabHost`
- HyperOS“切换应用 Alt Tab”：保持关闭

Shizuku 13.6.0 在 Android 13 及以上、已授予 `WRITE_SECURE_SETTINGS` 且连接受信任 WLAN 时支持非 Root 开机自启动。WorkBuddy 的开机广播接收器会在 Shizuku Binder 就绪后重新绑定守护服务。

## 使用与检查

正常情况下不需要手动操作。Pad 重启后保持连接常用 Wi-Fi，打开 Moonlight 串流即可。若快捷键失效：

1. 打开 Shizuku，确认显示“正在运行”。
2. 打开 WorkBuddy Alt+Tab，确认显示“状态：底层桥接已运行”。
3. 确认 Windows 任务管理器中有 `WorkBuddyAltTabHost.exe`；退出并重新登录 Windows 会自动恢复。

## 回滚

卸载 Pad 上的 `WorkBuddy Alt+Tab` 和 Shizuku；删除 Windows 登录项 `WorkBuddyAltTabHost`，结束同名进程，并删除 `%LOCALAPPDATA%\WorkBuddyAltTab`。回滚不会改动 Moonlight、Sunshine、Tailscale 或串流显示配置。
