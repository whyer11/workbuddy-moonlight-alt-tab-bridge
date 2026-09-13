# WorkBuddy 键盘桥接 V2 架构重构设计

## 目标

一次性重构 Android App、Shizuku UserService、native evdev 桥接和 Windows Host 的内部架构，同时保持已经验收的行为不变：

- Pad 实体 `Alt+Tab` 继续在 Windows 打开任务切换界面。
- 小米键盘 `Command/Left Meta` 继续作为完整左 Windows 键，支持 `Win+E` 等组合键。
- 上、下、左、右继续由 Moonlight 正确转发。
- Shizuku、Pad 桥接和 Windows Host 的开机/登录恢复行为不退化。
- 状态页面继续显示规则配置、Pad 实际运行状态和电脑状态。
- 状态功能的任何故障都不得停止核心按键桥接。

## 非目标

- 不修改 Moonlight 或 Sunshine 源码。
- 不引入辅助功能服务、Root、云端服务或远程按键控制接口。
- 不把当前工具拆成多个需要独立安装和维护的 Windows/Android 产品。
- 不在缺少真机证据时改变已验证的键值映射。

## 架构选择

采用模块化单体与 Ports/Adapters。Android、native 和 Windows 仍各自保持现有部署单元，但业务规则、状态查询、平台适配和 UI 生命周期分离。跨端稳定常量由一份共享规则清单生成，避免 Java 和 C 手工复制后漂移。

不采用多进程状态守护或完整 RPC 框架；它们会增加启动、升级、日志和攻击面，超出当前工具需要。

## 唯一规则来源

新增 `protocol/bridge-schema.json`，包含：

- Schema 与状态协议版本。
- App、native 与 Windows Host 产品版本。
- 状态端口、nonce 长度、报文大小上限。
- 规则 ID、显示信息、Pad 输入/输出、Windows 动作及是否需要 Windows Host。
- 各端实际实现的规则集合。

仓库脚本根据清单生成：

- Android `GeneratedBridgeSchema.java`。
- native `generated_bridge_schema.h`。
- Windows `GeneratedBridgeSchema.h`。

生成文件提交进 Git，测试重新生成后检查工作树无差异。这样本机不依赖额外生成器也能构建，同时 CI/验收能发现陈旧产物。

## Android 分层

### Domain

- `BridgeRuleCatalog`：只提供不可变规则定义，不引用 Android API。
- `BridgeSnapshot`：分别保存 Pad 和 Windows 的协议版本、产品版本、端点状态及真实规则集合。
- `DashboardStateReducer`：按规则归约 `configured`、`padState`、`computerState` 和整体状态。
- Domain 不包含中文文案、颜色、线程、Binder、UDP 或 SharedPreferences。

### Ports

- `PadBridgeMonitor`：异步读取 Pad 快照。
- `ComputerBridgeMonitor`：异步读取 Windows 快照。
- `ComputerHostSettings`：读取和保存电脑地址。
- `RefreshScheduler`：安排和取消下一次刷新。

### Application

- `DashboardCoordinator`：拥有刷新状态机、超时、结果合并、代次取消和前后台生命周期。
- Pad 与 Windows 查询相互独立；任何一个超时都能提交自己的降级结果。
- 同一时刻最多一个刷新周期，额外刷新请求合并为一次待执行刷新。
- Coordinator 输出不可变 `DashboardViewState`，不持有 Android View。

### Adapters 与 UI

- `ShizukuPadBridgeAdapter` 实现 `PadBridgeMonitor`。
- `UdpV2ComputerBridgeAdapter` 实现 `ComputerBridgeMonitor`。
- `SharedPreferencesComputerHostSettings` 实现地址存储。
- `ProbeActivity` 只创建/绑定 View、转发生命周期和渲染 `DashboardViewState`。
- 所有用户可见文字进入 Android resources；状态模型只使用 enum 与数据。

## native V2 能力与生命周期

native 增加无副作用命令：

```text
alttab_evdev_bridge --describe
WB_NATIVE_V2 version=0.7.0 protocol=2 rules=alt_tab,windows_key,arrow_keys
```

`--describe` 不打开 `/dev/input`、不创建 uinput、也不执行 EVIOCGRAB。

正常运行成功抓取键盘后输出严格 READY 行：

```text
WB_NATIVE_READY_V2 version=0.7.0 protocol=2 rules=alt_tab,windows_key,arrow_keys physical=/dev/input/eventN
```

UserService 只报告解析并验证过的 native READY 信息。进程存在但未 READY、版本不兼容或规则缺失时不得显示为运行中。

### 原子部署

1. 将 asset 写到 `.new` 并 `fsync`。
2. 校验 SHA-256、执行权限与 `--describe` 输出。
3. 将当前文件保留为 `.previous`。
4. 通知旧进程退出并限时等待释放 EVIOCGRAB。
5. 原子替换目标并启动新进程。
6. 两秒内未获得合法 READY，恢复 `.previous` 并重启旧进程。
7. 部署和回滚结果进入结构化状态，不以日志推断成功。

Supervisor 使用有上限的指数退避，重启等待为 1、2、4、8 秒，成功稳定运行后重置。销毁时先请求退出，等待后再强制终止，确保释放实体键盘。

## Binder V2

Binder 使用接口描述符与固定事务号。快照包含：

- Binder 协议版本。
- native 产品版本。
- `STARTING / READY / DEGRADED / STOPPED` 状态。
- native 实际规则集合。
- 最近错误码，不传递自由格式异常堆栈。

Binder 调用放在独立执行器并施加 500 毫秒上限。超时只将 Pad 状态置为 `UNKNOWN`，不会停止或重绑正在工作的 native 进程。

Binder 另提供仅限本应用调用的本机 `setActive(boolean)` 事务，用于暂停或恢复 native；它不暴露到 UDP，不允许电脑端远程改变 Pad 状态。调用返回后必须用后续快照确认最终状态。

## Windows Host 分层与 V2 状态协议

Windows Host 内部分为：

- `KeyboardBridgeCore`：注册/注销热键、键盘 Hook、键状态机和退出清理。
- `StatusServer`：只读 UDP 适配器。
- `HostCapabilities`：生成规则清单的只读视图。
- `HostApplication`：组合组件生命周期并记录结构化日志。

核心先启动，状态服务后启动。状态服务启动失败只记录 `STATUS_DEGRADED`，消息循环继续运行；Host 退出码只由核心启动或消息循环失败决定。

V2 请求和响应：

```text
WB_STATUS_V2 nonce=<32-hex>
WB_STATUS_V2 nonce=<same> host=0.7.0 protocol=2 core=ready rules=alt_tab,windows_key
```

要求：

- UTF-8/ASCII 单行，最大 512 字节。
- 字段顺序固定，禁止重复、未知或尾随字段。
- 只接受合法 nonce；客户端校验源地址、源端口、协议和 nonce。
- 继续使用 UDP 57420，防火墙仅允许 Tailscale `100.64.0.0/10`。
- 不提供状态修改、进程控制或按键注入命令。

## 全局拦截边界

重构后的默认状态保持桥接活动，避免升级后改变现有行为。Pad 页面提供明确的“活动/暂停”状态；暂停时 native 释放 EVIOCGRAB，恢复时重新发现并抓取 Xiaomi Keyboard。

Windows 优先只处理带 `LLKHF_INJECTED` 的流式 Scroll Lock/Alt+F12，使本地实体 Scroll Lock 和本地 Alt+F12 不被误转换。该过滤只有在真实 Moonlight/Sunshine 输入被证明带有此标志后才能启用；否则保留已验证的兼容路径，并把全局拦截作为明确显示的兼容模式。不能为了缩小范围牺牲现有快捷键功能。

V2 状态协议保持只读，因此暂停操作只在 Pad 本机控制 native，不通过网络控制 Windows。

## 性能策略

- 页面后台不执行状态查询。
- 异常、启动中状态每两秒刷新；连续健康后退避至五秒，用户操作触发立即刷新。
- DNS 结果短期缓存并在网络错误后失效；UDP socket 生命周期由适配器管理。
- UI 仅在 `DashboardViewState` 变化时重绘；背景和颜色使用资源/状态列表，不在每轮创建多个 Drawable。
- native 按键热路径保持无堆分配的 `poll/read/remap/write`，状态功能不得进入热路径。

## 无损 Windows 部署

新 Host 支持无副作用 `--self-test`，检查协议生成数据、键状态机和状态模块初始化，但不占用全局热键或生产端口。

部署流程：

1. 构建并运行 `--self-test`。
2. 备份已安装 EXE。
3. 停止旧 Host 并原子替换。
4. 启动新 Host，验证进程、核心 ready 日志、键桥接测试和 UDP V2。
5. 核心检查失败立即恢复旧 EXE。
6. UDP 单独失败不回滚核心，修复端口或防火墙后状态可自行恢复。

Pad APK 保持原包名和签名证书，通过 `install -r` 保留 Shizuku 授权和私有电脑地址。升级前保存旧 APK、native 与 Host 以便整体回滚。

## 测试与验收

### 自动测试

- Schema 生成幂等测试和 Java/C 生成产物一致性测试。
- native `--describe`、READY 解析、录制 evdev 序列和异常退出退避测试。
- native 部署成功、校验失败、READY 超时和 `.previous` 回滚测试。
- Binder V2 序列化、协议不兼容、缺失规则和500毫秒超时测试。
- Reducer 按规则覆盖 READY、UNKNOWN、UNSUPPORTED、NOT_REQUIRED。
- Coordinator 覆盖前后台、请求合并、独立超时和停止后无回调。
- UDP V2 严格解析、错误 nonce/来源/端口、未知字段、超长报文测试。
- Windows 端口占用时核心仍可处理 Alt+F12 与 Win 键的集成测试。
- 本地非注入 Scroll Lock 与流式注入事件的分类测试。
- 防火墙最小范围和脚本幂等测试。

### 真机回归门禁

- Pad 重启后 Shizuku、UserService 和 native 自动恢复，只有一个实例。
- Windows 登录恢复，只有一个 Host，核心与状态分别报告结果。
- 真实 Moonlight 会话验证一次 Alt+Tab、Command+E、上、下、左、右。
- 验证暂停释放键盘、恢复重新抓取且无卡键。
- 占用 UDP 57420 后确认按键仍正常，再释放端口确认状态自动恢复。
- 关闭/恢复 Tailscale 后页面降级和恢复，按键核心不被状态查询操作。
- APK、native、Host、生成文件与设备部署文件哈希一致。

## 六大原则对应

- 单一职责：UI、协调、状态适配、部署、监督和按键核心分离。
- 开闭原则：新增规则先改 Schema，生成目录和协议能力，Reducer/UI 按目录迭代。
- 里氏替换：各 Port 的 fake 与平台 Adapter 遵守相同快照和超时契约。
- 依赖倒置：Coordinator 依赖 Port，不依赖 Shizuku、UDP 或 SharedPreferences。
- 接口隔离：Pad、电脑、设置和调度分别为小接口。
- 迪米特法则：Activity 只认识 Coordinator/ViewState，领域层不穿透平台对象。

## 完成定义

全部自动测试、严格编译、签名验证和真机回归通过；状态模块故障不会影响核心；页面逐规则反映实际能力；旧版本可恢复；仓库工作树干净且远端提交与本地一致。
