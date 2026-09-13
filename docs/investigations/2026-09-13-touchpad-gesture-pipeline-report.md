# Moonlight 双指滚动与 WorkBuddy 三指拖动调查报告

日期：2026-09-13
状态：根因已由真机实验确认；本文只形成方案，尚未修改或部署生产代码。

## 1. 结论摘要

这不是一个“统一倍率太大”的单点问题，而是两条输入链路各自存在问题：

1. **双指滚动过快发生在 Moonlight。** HyperOS 在指针捕获状态下把 `Xiaomi Touch` 的双指滚动交给应用，事件类型是 `ACTION_SCROLL`，本机实测 `AXIS_VSCROLL` 单包可达到 `-227.58 ~ +235.19`。Moonlight 12.1 又无条件乘以 `120` 后发送到 Windows，造成巨大的滚轮增量。
2. **三指拖动过快发生在 WorkBuddy。** WorkBuddy 把触摸板的原始 ABS 多点质心差值按 `1:1` 注入为虚拟鼠标 REL 位移，绕过了 Android 对实体触摸板所做的单位换算和加速。真机低速样本显示，Android 正常触摸板输出的基础步长约为 X `0.26`、Y `0.23`，而 WorkBuddy 输出的是未缩放整数；首轮合理校准值应为当前速度的约 `25%`，再依手感微调。
3. **三指短暂抬起不能续拖是当前状态机的确定行为。** 当前代码遇到触点数不等于 3 或触点组成变化，会立即发送左键抬起。实测多组两次三指拖动间隔只有 `199~397 ms`，仍然每次都出现一次 `LEFT_UP` 和下一次 `LEFT_DOWN`。
4. **已排除“三指时 Moonlight 同时收到实体与虚拟两路移动，所以叠加翻倍”。** evdev 层确实可同时观察原始设备与虚拟设备，但在应用层指针捕获实验中，三指活动期间 Moonlight 等价测试 App 只收到 `WorkBuddy Virtual Mouse` 的位移，没有收到同期 `Xiaomi Touch` 位移。过快来自单位尺度错误，不是应用层双路叠加。

推荐采用一个很小的双组件修复：

- WorkBuddy：固定点缩放三指位移，默认 `25%`；加入默认 `1000 ms`、可配置的续拖宽限状态。
- Moonlight：基于 12.1 做一个可并存的最小 fork，只对 `Xiaomi Touch` 的 `ACTION_SCROLL` 做设备级归一化，默认从当前 `axis × 120` 改为 `axis × 1.0`，并提供倍率滑块；普通鼠标滚轮不受影响。

不建议独占并完整接管触摸板，因为那会迫使 WorkBuddy 重写一指移动、点击、右键、双指滚动、掌压抑制等整套触摸板行为，风险远高于本次需求。

## 2. 调查范围与环境

目标问题：

- Moonlight 中双指上下滚动明显过快。
- WorkBuddy 三指拖动/选择明显过快。
- 两次三指拖动间隔小于 1 秒时，应保持同一次左键拖动；1 秒应可配置。

真机环境：

| 项目 | 实测值 |
| --- | --- |
| Pad | Xiaomi `M367FC` |
| 系统 | Android 17 / API 37 / Xiaomi HyperOS OS4.0 |
| Moonlight | 12.1，versionCode 314，包名 `com.limelight` |
| WorkBuddy | 0.8.0，包名 `local.workbuddy.alttab` |
| 实体触摸板 | `/dev/input/event11`，`Xiaomi Touch` |
| 触摸板坐标 | X `0..3683`、33 units/mm；Y `0..2175`、37 units/mm；8 slots |
| 虚拟设备 | `WorkBuddy Virtual Mouse` |
| 系统速度 | `pointer_speed=-3`，`touchpad_pointer_speed=-2` |
| 主机 | Windows，经 Moonlight/Sunshine 串流 |

## 3. 输入链路

```text
双指：Xiaomi Touch(evdev)
        -> Android InputReader/触摸板手势识别
        -> ACTION_SCROLL + AXIS_VSCROLL
        -> Moonlight × 120
        -> Sunshine 网络协议
        -> Windows 滚轮事件

三指：Xiaomi Touch(evdev)
        -> WorkBuddy 读取 ABS_MT 三个触点并计算质心
        -> 1:1 注入 WorkBuddy Virtual Mouse 的 REL_X/REL_Y + BTN_LEFT
        -> Android InputReader
        -> Moonlight
        -> Windows 鼠标拖动
```

Android 官方输入文档说明，`EventHub` 从 evdev 读取原始事件，`InputReader` 维护触点状态、识别指针手势并归一化设备特定单位，最后由 `InputDispatcher` 交给应用；原始触摸设备数值本来就可能使用设备专属单位。[Android AOSP：Touch device operation](https://source.android.com/docs/core/interaction/input/touch-devices#touch-device-operation)

WorkBuddy 对触摸板是被动读取，没有 `EVIOCGRAB`；所以它能看见原始事件，但不会让 Android 丢失这些事件。Linux 输入子系统也明确规定，只有 grab 后句柄才成为唯一接收者。[Linux Kernel：Input Subsystem](https://www.kernel.org/doc/html/v5.18/driver-api/input.html)

## 4. 实验方法

### 4.1 静态代码审计

- 审计当前 WorkBuddy 三指状态机、uinput 虚拟鼠标与主循环。
- 将已安装 Moonlight 12.1 对应的提交 `1b1b100e...` 与当前处理逻辑逐行核对。
- 检查 Android 触摸板/指针捕获的官方行为和 Moonlight 已有触摸板问题。

### 4.2 Pad 原始 evdev 观察

编译只读探针，同时观察：

- `Xiaomi Touch`
- 同一附件的 `Xiaomi Mouse` 接口
- `WorkBuddy Virtual Mouse`

探针没有执行 `EVIOCGRAB`，没有注入事件。首轮 180 秒样本得到：

| 设备 | motion frames | 累计 dx/dy | 绝对位移和 | 左键 down/up |
| --- | ---: | ---: | ---: | ---: |
| Xiaomi Mouse | 0 | 0 / 0 | 0 / 0 | 0 / 0 |
| Xiaomi Touch | 1304 | 4903 / -1328 | 11871 / 13154 | 0 / 0 |
| WorkBuddy Virtual Mouse | 358 | -400 / -396 | 2460 / 2108 | 4 / 4 |

三指样本中，实体质心帧 `dx=14, dy=17` 后紧接着虚拟鼠标帧 `dx=14, dy=17`，确认 WorkBuddy 当前是原始单位 `1:1` 注入。`Xiaomi Mouse` 全程 0 帧，排除了附件另一个鼠标接口造成第三路输入。

### 4.3 Android MotionEvent 指针捕获实验

制作临时测试 App，像 Moonlight 一样请求 pointer capture，记录：设备名、source、action、pointerCount、历史样本、相对轴、滚动轴和按键状态。APK SHA-256：

```text
79268412B0B1BC727A87099A10E39B335A5FD237EA140B453C8F2BF08B72A28A
```

完整日志共 1118 行、283464 bytes，位于本地 [motion-events-live.txt](../../.tools/motion-event-probe/motion-events-live.txt)。实验完成后测试 App 已卸载。

解析到 1116 条事件：

| 设备 / action | 条数 | 关键观察 |
| --- | ---: | --- |
| Xiaomi Touch / MOVE | 675 | `source=0x00020004`，pointerCount 恒为 1 |
| Xiaomi Touch / SCROLL | 135 | VSCROLL `-227.58 ~ +235.19`，绝对值总和 15004.69 |
| WorkBuddy Virtual Mouse / MOVE | 262 | 整数位移，例如 `(59,49)`、`(32,21)`、`(40,23)` |
| WorkBuddy / DOWN + BUTTON_PRESS | 10 + 10 | 每段拖动重新按下 |
| WorkBuddy / BUTTON_RELEASE + UP | 10 + 10 | 每段拖动立即释放 |

`0x00020004` 是 Android 的 `SOURCE_MOUSE_RELATIVE`。Android API 37 说明，相对指针捕获下，一指移动会作为相对鼠标移动、双指滚动会作为 `ACTION_SCROLL` 和 `AXIS_VSCROLL/HSCROLL` 交付。[Android `View` pointer capture 文档](https://developer.android.com/reference/android/view/View#POINTER_CAPTURE_MODE_RELATIVE) [Android `InputDevice` 文档](https://developer.android.com/reference/android/view/InputDevice#SOURCE_MOUSE_RELATIVE)

### 4.4 Windows 端到端钩子

在 Windows 使用只读低级鼠标 hook，同时在 Moonlight 中完成多次双指滚动。45 秒日志位于本地 [windows-focused.txt](../../.tools/gesture-probe/windows-focused.txt)：

| 指标 | 实测值 |
| --- | ---: |
| MOVE 包 | 417 |
| 垂直滚轮包 | 115 |
| 滚轮绝对值总和 | 222163 |
| 每包绝对值平均 | 1931.85 |
| 每包平均标准刻度数 | 16.10（1931.85 / 120） |
| 单包最小 / 最大 | -12276 / +1133 |
| 左键 down/up | 1 / 1 |

Windows 标准滚轮刻度为 120。也就是说，本次双指样本平均每约 7 ms 的一个事件包，就相当于约 16 个传统滚轮刻度；问题不是稳定的“正好 2 倍”，而是随速度和应用处理方式变化的严重放大。

## 5. 根因判定

### 5.1 双指滚动：Moonlight 把连续触摸板量再次当作滚轮刻度放大

Moonlight 12.1 在 `Game.java:1915-1916` 中执行：

```java
conn.sendMouseHighResScroll((short) (event.getAxisValue(MotionEvent.AXIS_VSCROLL) * 120));
conn.sendMouseHighResHScroll((short) (event.getAxisValue(MotionEvent.AXIS_HSCROLL) * 120));
```

对应的官方开源代码可在 [Moonlight 12.1 `Game.java`](https://github.com/moonlight-stream/moonlight-android/blob/1b1b100e63e8ffcf1496e324ff7b57ee9f2db2a9/app/src/main/java/com/limelight/Game.java#L1915-L1916) 查看。Moonlight 的 pointer-capture provider 还会累加每个批次的历史相对样本，这是正确行为，不是本次滚动重复的来源。[AndroidNativePointerCaptureProvider.java](https://github.com/moonlight-stream/moonlight-android/blob/1b1b100e63e8ffcf1496e324ff7b57ee9f2db2a9/app/src/main/java/com/limelight/binding/input/capture/AndroidNativePointerCaptureProvider.java#L114-L141)

`×120` 对值域接近 `-1..+1` 的传统滚轮输入合理，但本机 HyperOS 已经把触摸板连续滚动量作为大幅度数值交给 App。把 `-158.22` 再乘 120 会得到约 `-18986` 的高分辨率滚轮值。Windows 端实际捕获到的数千至上万单包增量，与这条代码完全吻合。

因此：

- 根因在 Pad 端的“HyperOS 事件语义 + Moonlight 固定换算”交界处。
- Sunshine、Tailscale 和公网链路只运输已经放大的输入，不是倍率来源。
- Android 系统全局调低触摸板速度不是稳妥修复：它会连正常一指移动一起改变，而且不能保证 OEM 滚动曲线与 Moonlight 固定 `×120` 恰好抵消。

Moonlight 社区已有实体触摸板在不同 Android 厂商上产生不同事件语义的案例；2026 年的一个开放 PR 也采用“设备/触摸板专用、默认关闭”的输入修正，而不是假设所有触摸板一致。[Moonlight issue #1383](https://github.com/moonlight-stream/moonlight-android/issues/1383) [Moonlight PR #1569](https://github.com/moonlight-stream/moonlight-android/pull/1569)

### 5.2 三指速度：原始 ABS 坐标被当作 REL 鼠标计数

当前 [touchpad_drag_state.c](../../native-bridge/src/touchpad_drag_state.c#L131-L158) 计算三个触点的原始质心差，然后使用配置中的 `movement_numerator / movement_denominator`；[alttab_evdev_bridge.c](../../native-bridge/src/alttab_evdev_bridge.c#L154-L166) 把它硬编码为 `1/1`。

实体触摸板的 X/Y 分辨率分别为 33/37 raw units/mm；这些是传感器单位，不是鼠标 REL 计数。Android 正常路径会做设备归一化和加速，而 WorkBuddy 创建的虚拟鼠标只声明 `REL_X/REL_Y`，因此 Android 会把每个整数直接视为鼠标位移。

应用层实验还推翻了“双路叠加”猜测：三指活动区间只出现 WorkBuddy 虚拟鼠标 MOVE，实体 `Xiaomi Touch` 没有同期 MOVE。修复点应是 WorkBuddy 的数值换算，而不是做事件去重。

根据低速正常路径的 `0.26/0.23` 基础步长与虚拟路径的整数步长，初始倍率 `25%` 有客观依据；但 Android 还会按速度应用加速曲线，所以它只能作为首轮默认值，最终手感需要在真机上用统一距离 A/B 校准，不能把 `25%` 宣称为理论精确值。

当前整数除法如果直接从 `1/1` 改成 `1/4`，小于 4 的逐帧位移会反复截断成 0，造成低速发涩。因此必须加入每轴余数累积的固定点换算，而不是只改常量。

### 5.3 三指续拖：状态机明确立即释放

当前 [touchpad_drag_state.c](../../native-bridge/src/touchpad_drag_state.c#L115-L123) 在以下任一条件出现时立刻 `LEFT_UP`：

- active contacts 不等于 3；
- 拖动期间触点组成发生变化。

真机记录到的连续短间隔为：

```text
291 ms, 248 ms, 199 ms, 243 ms, 219 ms,
383 ms, 397 ms, 347 ms
```

它们都小于 1 秒，但每组中间仍有 `BUTTON_RELEASE/UP`，所以当前实现不具备 macOS 式的短暂抬手续拖。

## 6. 方案比较

| 方案 | 内容 | 优点 | 代价 / 结论 |
| --- | --- | --- | --- |
| A. 只改 WorkBuddy | 三指减速与续拖；双指依赖系统全局设置 | 改动最少，官方 Moonlight 不变 | 无法可靠修复 Moonlight 收到的双指 `ACTION_SCROLL`；不完整 |
| **B. WorkBuddy + 最小 Moonlight fork** | WorkBuddy 修三指；Moonlight 仅归一化 Xiaomi 双指滚动 | 两个根因各在所属边界修复；范围小；普通鼠标与其他触摸板不受影响 | 需维护一个自签名 Moonlight APK；**推荐** |
| C. WorkBuddy 永久 grab 触摸板 | 独占 `Xiaomi Touch`，自行实现全部一/二/三指行为 | 所有配置集中在 WorkBuddy | 必须重写整套触摸板语义，容易造成点击、滚动、掌压、休眠唤醒回归；本次不采用 |

“只改 Moonlight”也不能完成需求：当前 HyperOS 指针捕获交给应用的实体事件 pointerCount 恒为 1，Moonlight 看不到可用于可靠识别三指的原始触点；WorkBuddy 的 evdev 层仍然不可替代。

## 7. 推荐方案的具体设计

### 7.1 WorkBuddy：固定点速度归一化

- UI 名称：`三指拖动速度`。
- 默认：`25%`；范围建议 `10%..100%`，步长 `5%`。
- 原始质心算法不变。
- 每轴分别保存余数：

```text
scaled = raw_delta * numerator + remainder
output = scaled / denominator
remainder = scaled % denominator
```

- 触点重组、重新进入三指、取消拖动时清空余数，避免旧余数带来跳动。
- 保留当前 48 raw units 的启动阈值；它约等于 1.3~1.5 mm，和速度倍率是两个概念。

### 7.2 WorkBuddy：1 秒续拖状态

状态机保持四个状态即可：

```text
IDLE -> ARMED -> DRAGGING -> GRACE
                    ^          |
                    |--三指恢复-|
```

规则：

1. `DRAGGING` 中少于 3 指或触点发生替换：进入 `GRACE`，保持左键按下，设置 `deadline = now + continuationMs`。
2. 截止时间前恢复恰好 3 指：仅以新质心重设基线，不发第二次 `LEFT_DOWN`，下一帧继续移动。
3. `elapsed >= continuationMs`：发且只发一次 `LEFT_UP`，回到 `IDLE`。用户要求“短于 1 秒”连续，因此恰好 1000 ms 应释放。
4. 出现 4 指及以上：立即释放，作为防误操作保护。
5. 进程停止、桥接暂停、设备断开、`SYN_DROPPED`：无条件释放，绝不留下 Windows 卡住的左键。
6. WorkBuddy 虚拟鼠标保持左键按下期间，Moonlight 忽略来自实体 `Xiaomi Touch` 的过渡 MOVE/SCROLL，避免第 1、2、3 根手指依次落下时让实体链路带着已按下的左键误移动；虚拟鼠标自身事件不受影响，收到它的左键释放后解除抑制。

超时不能只等下一次触摸事件。主循环当前固定 `poll(..., 500)`；应让状态机暴露下一个 deadline，主循环使用 `min(500ms, deadline-now)` 作为 poll timeout，并在 timeout 后调用 `tick(now)`。这样没有新输入时也会准时释放，而且没有忙轮询。

### 7.3 Moonlight：只归一化 Xiaomi 触摸板滚动

新增一个小型 `ExternalPointerNormalizer`，职责只有三项：

- 识别 `event.getDevice().getName() == "Xiaomi Touch"` 且 source 为 captured relative pointer。
- 对该设备的 `ACTION_SCROLL` 使用 `axis × normalizedGain`，默认 `normalizedGain=1.0`，替代当前 `axis × 120`；这等价于默认缩小到当前实现的 `1/120`。
- 保存浮点余数并在转成 `short` 前做饱和裁剪，避免低速尾部丢失和极值溢出回绕。

设置页显示 `小米触摸板双指滚动速度`：

- UI 默认 `100%`，表示归一化基线 `axis × 1.0`。
- 建议范围 `25%..300%`，步长 `5%`。
- 普通蓝牙/USB 鼠标、触屏模拟触摸板、其他设备继续走 Moonlight 原逻辑。

根据本次 Windows 记录，把当前输出除以 120 后：

- 每包平均由 1931.85 降到约 16.10 个高分辨率单位；
- 整段样本由约 1851 个标准刻度降到约 15.4 个标准刻度。

这是合适的首轮基线，但最终倍率仍应通过固定物理距离的真机测试确认。

为不覆盖官方 `com.limelight`，fork 使用独立 applicationId（建议 `com.limelight.workbuddy`）和应用名 `Moonlight WorkBuddy`，与官方版并存。这样失败时可立即切回官方版，不丢失原版设置和配对。首次使用 fork 需要重新与 Sunshine 配对。Moonlight Android 使用 GPL-3.0；若发布 APK，需同时保留许可证并公开对应源代码。[Moonlight Android 仓库](https://github.com/moonlight-stream/moonlight-android)

## 8. 文件级落地计划

### 8.1 WorkBuddy 仓库

| 文件 | 修改内容 |
| --- | --- |
| `protocol/bridge-schema.json` | 增加三指速度与续拖时长的默认值、范围；仍作为跨层单一事实源 |
| `protocol/generate-schema.ps1` | 生成新增常量 |
| `native-bridge/src/touchpad_drag_state.h` | 增加 `GRACE`、deadline、X/Y 余数、continuation 配置和 `tick` API |
| `native-bridge/src/touchpad_drag_state.c` | 实现固定点缩放、宽限期、恢复时 rebase、超时/4 指/取消释放 |
| `native-bridge/src/touchpad_bridge.h/.c` | 注入 monotonic 时间；暴露 deadline/timeout；统一安全释放 |
| `native-bridge/src/alttab_evdev_bridge.c` | 新增速度与超时启动参数；动态 poll timeout；ready 行显示实际配置 |
| `native-bridge/tests/touchpad_drag_state_test.c` | 先补失败测试：999/1000 ms 边界、恢复无跳变、无重复 down、固定点余数、4 指、cancel |
| `android-bridge/.../domain/GestureConfig.java` | 不可变配置对象与边界校验 |
| `android-bridge/.../ports/GestureSettings.java` | 配置存储端口 |
| `android-bridge/.../adapters/SharedPreferencesGestureSettings.java` | 持久化速度和续拖时长 |
| `android-bridge/.../ProbeActivity.java`、`strings.xml` | 增加两个简洁滑块并显示当前值 |
| `android-bridge/.../shizuku/BridgeBinderProtocolV2.java` | 新增配置事务，不改变现有状态事务语义 |
| `android-bridge/.../BridgeController.java`、`BridgeUserService.java` | App 将持久化配置传给 Shizuku 用户服务 |
| `android-bridge/.../nativebridge/NativeBridgeSupervisor.java` | 以参数启动原生程序；配置变化时先安全释放再重启 |
| 对应 Java tests | 配置边界、持久化、Binder 序列化、启动参数和重启行为 |

### 8.2 Moonlight fork

| 文件 | 修改内容 |
| --- | --- |
| `app/build.gradle` | 独立 applicationId / app label，确保和官方版并存 |
| `Game.java` | 把 ACTION_SCROLL 交给 normalizer；其他输入路径不改 |
| 新建 `binding/input/ExternalPointerNormalizer.java` | 设备识别、滚动换算、余数、short 裁剪、宽限期实体事件 gate |
| `preferences/PreferenceConfiguration.java` | 读取归一化后的滚动倍率 |
| `res/xml/preferences.xml` | 增加滚动速度滑块 |
| `res/values/strings.xml`、`values-zh-rCN/strings.xml` | 英文/中文设置文案 |
| 新增 JVM tests | Xiaomi/非 Xiaomi 分流、正负滚动、余数、极值、普通鼠标不回归 |

## 9. 测试与验收标准

### 9.1 自动化测试

- `999 ms` 内三指恢复：无 `LEFT_UP`、无第二个 `LEFT_DOWN`、首帧只 rebase 不位移。
- `1000 ms` 及以后：恰好一个 `LEFT_UP`。
- 4 指、cancel、进程退出、`SYN_DROPPED`：立即且恰好一次释放。
- 连续 `±1` raw delta 在 25% 下通过余数累计，四帧输出总和仍为 `±1`。
- Moonlight 的 Xiaomi 路径默认输出约为旧值的 `1/120`；普通鼠标输出逐位保持原行为。
- `short` 极值使用饱和裁剪，不发生正负回绕。
- 现有 Alt+Tab、Command→Win、方向键和状态协议测试全部通过。

### 9.2 真机回归

使用同一把尺标记触摸板距离，每项至少 10 次：

1. 一指与三指各移动 30 mm，三指在 Windows 的位移目标先调到一指基线的 `0.85~1.15`；再由用户按选择文字和拖窗口的手感微调。
2. 双指以低/中/高三种速度各滑 30 mm；Windows hook 的单包 P95 目标不超过 240，不能再出现上万的增量或 `short` 回绕。
3. 依次验证 200、500、900、1000、1100 ms 抬手间隔；前 3 组选择连续，后 2 组结束。
4. 宽限期重新落下三指时光标不得跳跃；先落下一两根手指不得带动被选内容。
5. 桥接暂停、Shizuku 停止、Pad 锁屏/解锁、Moonlight 退出后，Windows 左键均不得卡住。
6. 3408×2272、144 Hz 实际串流 20 分钟；原生桥接空闲 CPU 目标 `<1%`，事件处理 P95 目标 `<2 ms`。

## 10. 发布与回滚

1. 先只部署新的 WorkBuddy 到测试版本，保留 `/data/local/tmp/alttab_evdev_bridge.previous`。
2. Moonlight fork 与官方 Moonlight 并存，使用不同包名；只在 fork 中做输入 A/B。
3. 完成自动化和真机验收后，再把用户最终选择的倍率设为默认值。
4. 若三指出现异常，恢复 WorkBuddy previous binary；若双指异常，退出 fork 并直接打开官方 Moonlight。回滚不改 Sunshine、Tailscale、路由器或显示配置。

## 11. 调查后的生产状态

- 临时 MotionEvent 测试 App 已卸载。
- 临时 `/data/local/tmp/workbuddy_gesture_probe` 已删除。
- Pad 仅保留正式 WorkBuddy 与 Moonlight。
- Shizuku、WorkBuddy 用户服务、`alttab_evdev_bridge` 均正常运行。
- 本报告生成前没有向生产 App、原生桥接、Moonlight、Sunshine 或系统设置写入功能性改动。
