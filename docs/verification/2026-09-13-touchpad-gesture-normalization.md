# 触摸板手势归一化验证记录

## WorkBuddy 0.9.0 候选构建

- 构建日期：2026-09-13
- Android 包名：`local.workbuddy.alttab`
- 版本：`versionCode 11` / `versionName 0.9.0`
- ARM64 native 严格构建：NDK 29，`-std=c11 -O2 -Wall -Wextra -Werror -pedantic`
- native 产物与 APK asset SHA-256：`DB99A3D545DE97B6663C4B62CBAE6D88E9325C8373FEFF115885D88393B130D6`
- Gradle debug APK SHA-256：`C2EED6821FABD1F4AD0CCA10F5E9E8DEE7F74564A5BEF79730325957E549B1EA`
- 同证书升级 APK：`android-bridge/app/build/outputs/apk/debug/workbuddy-alt-tab-0.9.0.apk`
- 同证书升级 APK SHA-256：`86B986D6FB87778001A45CF7AB2FC744C51CEEFEA94C83BBDB11ECEDD93B9C38`
- 升级 APK 签名证书 SHA-256：`CB952E9472E9970483FA68E9474ADA70A5A98DA35B6F9EABB1D5A8B5B8D8B7D0`（与 Pad 已安装 0.8.0 一致）

自动回归结果：

- `generated_schema_test: OK`
- `key_remap_test: OK`
- `native_protocol_test: OK`
- `touchpad_drag_state_test: OK`
- `bridge_options_test: OK`
- `:app:testDebugUnitTest :app:assembleDebug`：`BUILD SUCCESSFUL`
- `git diff --check`：通过

## 安装前回滚基线

- 目标 Pad 序列号：`0084941049983540`
- 无线 ADB 端点由 mDNS 动态发现并验证，未复用旧端口
- 已安装 WorkBuddy：`versionCode 10` / `versionName 0.8.0`
- 已安装 APK SHA-256：`98DE12F0277F797CC482EB844FA159E7AA67B79ED9CEA3EEADE288933745766D`
- 已安装签名证书 SHA-256：`CB952E9472E9970483FA68E9474ADA70A5A98DA35B6F9EABB1D5A8B5B8D8B7D0`
- Pad 当前 native SHA-256：`6E099E9EB0BF675A93E74EE2337EC1AFBD4C86891B4701BAE3E8450C27CC6961`
- Pad native 已复制为 `/data/local/tmp/alttab_evdev_bridge.previous`
- 已安装 APK 已拉取到本地忽略目录 `.tools/three-finger-drag/device-backup/workbuddy-installed.previous.apk`

Gradle debug APK 使用 Android Debug 证书，只作为构建验证产物；设备内升级包必须使用既有 WorkBuddy Local Test 证书重新签名，并在安装前再次核对证书摘要。

## Moonlight WorkBuddy 候选构建与配对

- 分支：`feature/workbuddy-touchpad-input`
- Pad 当前实装提交：`b334cd62`；后续状态清理提交：`cb81f70f`（均已推送到 `whyer11/moonlight-android`）
- Android 包名：`com.limelight.workbuddy.debug`
- 版本：`versionCode 315001` / `versionName 12.2-workbuddy.1`
- Pad 当前实装 Debug APK SHA-256：`09BCD8CE4B0EC0ED96889BD153EBD2A20AA8EC20097A07D8F1A4F3B38ABE02CC`
- 提交 `cb81f70f` 的 Debug APK SHA-256：`9FB0CC31A373A348BE45BAE89D25F15F6CEB244650F665D0E7AE10E81861D582`
- 提交 `cb81f70f` 执行 `clean testNonRootDebugUnitTest assembleNonRootDebug`：`BUILD SUCCESSFUL`（58 tasks）
- Pad 已安装该调试包，官方 `com.limelight` 仍并存。
- Sunshine 已出现并启用新客户端 `PADMAX`，Moonlight WorkBuddy 已完成独立配对。

真机阶段结论：

- 用户在 Moonlight WorkBuddy 中确认双指滚动已修复，不再出现此前的双倍或更大幅度移动。
- 当前结论为手感验收通过；Windows wheel 单包 P95 与 30 mm 定量数据仍待采集。

## 非三指输入立即打断续拖候选版

- 提交：`96279a3`
- 规则：续拖空档中出现新的一指、两指或对应位移时立即发送一次 `LEFT_UP`；仅抬手过渡不误打断；三指恢复不重复发送 `LEFT_DOWN`。
- 新增状态机测试覆盖一指、两指、两指移动、交错抬手、三指恢复以及重复释放防护。
- `touchpad_drag_state_test`、`key_remap_test`、`native_protocol_test`、`bridge_options_test`、`generated_schema_test`：全部 `OK`。
- NDK 29 ARM64 严格构建：零警告。
- native 与 APK asset SHA-256：`4A3112B63420942E0B0EA7B51F1EA322AD309F7D98BEB2F2FCA07B73D861B759`。
- `:app:testDebugUnitTest :app:assembleDebug`：`BUILD SUCCESSFUL`（39 tasks）。
- 同证书升级 APK SHA-256：`23D10F9937C0120D87970093AB7C4353B665665925033A73C9986EE5C350EDA6`。
- APK 签名证书 SHA-256：`CB952E9472E9970483FA68E9474ADA70A5A98DA35B6F9EABB1D5A8B5B8D8B7D0`。
- Pad 已覆盖安装；重启 Shizuku UserService 后，设备 native 哈希与候选版一致。
- 设备启动参数：`--three-finger-speed-percent 25 --three-finger-continuation-ms 1500`。
- 一指、两指立即打断与三指直接恢复的手工验收：待用户确认。

## 启用门槛

首次安装 WorkBuddy 0.9.0 后，三指续拖保持 `0 ms`。只有 Moonlight WorkBuddy 完成实体触摸板 gate、双指滚动归一化和真机验证后，才切换为 `1000 ms`。

## 待补充的端到端证据

- WorkBuddy 0.9.0 同签名原位升级结果
- Pad 上配置持久化、native 启动参数、暂停/恢复与旧键盘规则回归
- 双指滚动定量数据、三指拖动、续拖窗口和一指实体触摸板 gate 的真机证据
