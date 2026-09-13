# 触摸板手势归一化验证记录

## WorkBuddy 0.9.0 候选构建

- 构建日期：2026-09-13
- Android 包名：`local.workbuddy.alttab`
- 版本：`versionCode 11` / `versionName 0.9.0`
- ARM64 native 严格构建：NDK 29，`-std=c11 -O2 -Wall -Wextra -Werror -pedantic`
- native 产物与 APK asset SHA-256：`DB99A3D545DE97B6663C4B62CBAE6D88E9325C8373FEFF115885D88393B130D6`
- Gradle debug APK SHA-256：`C2EED6821FABD1F4AD0CCA10F5E9E8DEE7F74564A5BEF79730325957E549B1EA`

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

## 启用门槛

首次安装 WorkBuddy 0.9.0 后，三指续拖保持 `0 ms`。只有 Moonlight WorkBuddy 完成实体触摸板 gate、双指滚动归一化和真机验证后，才切换为 `1000 ms`。

## 待补充的端到端证据

- Moonlight WorkBuddy 单元测试、构建身份、签名与 APK 哈希
- WorkBuddy 0.9.0 同签名原位升级结果
- Pad 上配置持久化、native 启动参数、暂停/恢复与旧键盘规则回归
- 双指滚动、三指拖动、续拖窗口和一指实体触摸板 gate 的真机证据
