# Moonlight Alt+Tab 桥接工具设计

## 目标与约束

在小米 Pad（Android 17 / HyperOS 4）的 Moonlight 12.1 串流中，让实体键盘 `Alt+Tab` 触发 Windows 原生任务切换，不打开安卓多任务界面，也不让 Moonlight 丢失前台。不能 Root、不能替换 Moonlight、不能依赖辅助功能抢键。

## 真机调查结论

- HyperOS 的“切换应用 Alt Tab”开关开启时，系统在应用收到 Tab 之前执行 `SHOW_RECENT_APPS`；关闭时系统仍可能在策略层吞掉 Tab，Moonlight 只能收到 Alt。
- 具有 `FLAG_REQUEST_FILTER_KEY_EVENTS` 的辅助功能服务已成功绑定，但收不到这组被系统策略提前处理的按键，因此辅助功能路线不可行。
- ADB shell 用户属于 `input`/`uhid` 相关组，能读取 `/dev/input/event9`（Xiaomi Keyboard）、对它执行 `EVIOCGRAB`，并能通过 `/dev/uinput` 创建设备。
- Moonlight 12.1 只完整映射 F1 到 F12，不能使用最初尝试的 F13；`Alt+F12` 可完整传到 Windows。

## 最终架构

### Pad 端

`local.workbuddy.alttab` 通过 Shizuku 13.6.0 启动 daemon UserService。服务以 ADB shell 身份运行，部署 APK 内置的 ARM64 原生桥接程序并持续守护它。

原生桥接程序：

1. 自动查找名称严格等于 `Xiaomi Keyboard` 的输入设备。
2. 创建 `WorkBuddy Virtual Keyboard`，声明按键、扫描码与 LED 能力。
3. 成功创建虚拟设备后才对物理键盘执行 `EVIOCGRAB`。
4. 普通事件原样重放；左/右 Alt 按住期间，把 Tab 的按下、重复和抬起统一改成 F12。
5. 收到终止信号或出现错误时释放独占并销毁虚拟设备。

该程序不读取网络、不存储按键、不输出普通按键日志。它会全局接管当前实体键盘，但只有 Moonlight 位于前台时映射后的 `Alt+F12` 才会被串流到电脑。HyperOS 的原生 Alt+Tab 开关保持关闭。

### Windows 端

无控制台 Win32 程序在当前用户登录时自动启动，并注册 `MOD_ALT | MOD_NOREPEAT + VK_F12` 全局热键。Moonlight 转发的 Alt 仍处于按下状态；热键触发后，程序用 `SendInput` 发送一次 Tab 按下/抬起，组合成 Windows 原生 `Alt+Tab`。

程序不开放端口，不依赖 Tailscale，也不修改 Sunshine。互斥体阻止重复实例；日志只记录程序状态和热键是否成功触发。

## 自动启动

- Windows：HKCU Run 项启动隐藏程序。
- Pad：HyperOS 已允许 Shizuku 与 WorkBuddy 自启动；Shizuku 获得 `WRITE_SECURE_SETTINGS`，以 ADB 模式启动。Android 13+ 连接受信任 WLAN 后，Shizuku 13.6.0 可自动恢复服务。
- WorkBuddy：接收 `BOOT_COMPLETED`，等待 Shizuku Binder，权限已设为“始终允许”，随后绑定 daemon UserService；UserService 会重新部署 APK 内置原生程序并守护退出。

## 失败保护与回滚

- 打不开物理设备、创建虚拟设备或独占失败时立即退出，不留半接管状态；UserService 每秒尝试恢复。
- Windows 热键注册失败时退出并记录错误，不进行键盘注入。
- 卸载 WorkBuddy 与 Shizuku、删除 HKCU Run 项并结束 Windows 常驻进程即可完整回滚；Moonlight、Sunshine、Tailscale 和串流显示设置不受影响。

## 验收标准

- `key_remap_test` 覆盖普通 Tab、左右 Alt、长按重复以及 Alt 先松开的序列。
- Android APK 签名有效，清单中没有辅助功能、网络或存储权限。
- Shizuku、UserService、原生桥接和 Windows 常驻程序均处于运行状态，且自动启动项存在。
- 真机实体键盘操作后，Windows 出现任务切换界面；用户已在实际 Moonlight 会话中确认。
