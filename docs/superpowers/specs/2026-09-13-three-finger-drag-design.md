# Moonlight 三指拖动设计

## 目标

在小米 Pad 的实体触摸板上提供接近 macOS“三指拖移”的操作：三指移动超过阈值后，Moonlight 收到鼠标左键按下和连续相对移动；三指状态结束时收到左键释放。

## 范围

- 只支持名称为 `Xiaomi Touch` 的实体触摸板。
- 复用当前 WorkBuddy App、Shizuku UserService、原生进程、协议清单、状态页面和升级回滚链。
- 不修改 Moonlight、Sunshine 或 Windows Host。
- 不独占实体触摸板；原始触摸板事件继续交给 Android。当前 Moonlight 会阻止 HyperOS 执行三指系统动作，桥接只补充左键拖动语义。
- 首版随现有桥接总开关启停，不增加单独开关、前台应用检测或设置页面。

## 输入语义

原生模块跟踪 `ABS_MT_SLOT`、`ABS_MT_TRACKING_ID`、`ABS_MT_POSITION_X`、`ABS_MT_POSITION_Y`，并在每个 `SYN_REPORT` 先按 tracking ID 计算活动触点数量，再在恰好三指且坐标完整时计算中心点。

状态机只有三种状态：

1. `IDLE`：活动触点不是三指。
2. `ARMED`：恰好三指，记录起始中心点，但尚未按下左键。
3. `DRAGGING`：中心点离起点达到 48 个原始坐标单位后，发送一次左键按下；随后把中心点增量按 1:1 映射为相对鼠标移动。

三指未达到阈值便抬起时不产生鼠标事件。进入拖动后，只要触点数量不再等于三、三指中的任意 tracking ID 被替换，包括抬起一指或增加第四指，就发送左键释放并回到 `IDLE`。进程退出、设备断开或读取失败时同样强制释放左键。

初始阈值 48 约等于该触摸板的 1.3 至 1.5 毫米。阈值与移动倍率集中在一个配置结构中，真机试用后只调整参数，不改变状态机接口。

## 架构

### 纯状态机

`native-bridge/src/touchpad_drag_state.c/.h` 不依赖 evdev 或 uinput。它接收当前 slot、触点进入/离开、坐标更新和帧结束，输出 `LEFT_DOWN`、`MOVE`、`LEFT_UP` 三类动作。该层可在 Windows 上由 GCC 直接执行单元测试。

### Linux 输入适配器

`native-bridge/src/touchpad_bridge.c/.h`：

- 按设备名发现并以非阻塞只读方式打开 `Xiaomi Touch`。
- 创建只声明 `BTN_LEFT`、`REL_X`、`REL_Y` 的 `WorkBuddy Virtual Mouse`。
- 把 evdev 多点事件转换为状态机调用，再把状态机动作写入 uinput。
- 不调用 `EVIOCGRAB`。
- 关闭前保证发送 `BTN_LEFT UP` 并销毁虚拟鼠标。

### 进程集成

`alttab_evdev_bridge.c` 继续负责生命周期和键盘桥接，通过两个 `pollfd` 同时等待实体键盘和触摸板。触摸板及虚拟鼠标必须创建成功后，才抓取实体键盘并输出 `WB_NATIVE_READY_V2`。任一设备读取出现不可恢复错误时，进程统一清理并退出，由现有 `NativeBridgeSupervisor` 退避重启。

这样保持单原生进程和既有守护模型，同时让键盘映射与触摸板状态机分别位于独立文件。

## 能力与版本

- 产品版本升级为 `0.8.0`，Android `versionCode` 升级为 `10`。
- 协议版本保持 `2`；现有数据格式不变。
- `protocol/bridge-schema.json` 新增 Pad 侧规则 `three_finger_drag`，Windows 侧标记为不需要桥接。
- `--describe` 与 `WB_NATIVE_READY_V2` 的规则列表增加 `three_finger_drag`；Pad 状态页由生成后的规则目录自动展示该能力。

## 安全与回滚

- 实体触摸板从不被独占，原生进程崩溃不会令触摸板失效。
- 所有退出路径均释放可能按下的虚拟左键。
- APK 保留包名和签名证书，`install -r` 保留 Shizuku 授权与本机配置。
- 原生文件继续使用 `.new`、`.previous`、哈希和 `--describe` 校验；安装失败自动恢复 0.7.0。

## 验收

- 三指轻触不产生左键点击。
- 三指移动超过阈值后可在 Windows 中拖动窗口、文件并选择文本。
- 任意一指抬起或第四指落下时立即释放左键。
- 暂停桥接或终止原生进程不会留下卡住的左键。
- Alt+Tab、Command/Windows 键和四个方向键全部回归通过。
- Pad 页面显示四条规则，Pad 原生版本为 0.8.0，Windows Host 保持 0.7.0 且整体状态正常。
