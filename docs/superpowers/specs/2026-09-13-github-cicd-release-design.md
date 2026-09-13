# WorkBuddy 双仓库 GitHub CI/CD 与自动发布设计

## 目标

为以下两个 GitHub 仓库建立可重复、可审计、不依赖开发电脑的 CI/CD：

- `whyer11/workbuddy-moonlight-alt-tab-bridge`
- `whyer11/moonlight-android`

普通提交和 Pull Request 自动验证源码；版本标签自动构建、签名、校验并发布 GitHub Release。

## 现状

桥接仓库没有 GitHub Actions，Android 工程没有 Gradle Wrapper 和正式签名配置。发布物实际由 Android APK、APK 内的 ARM64 原生桥接程序，以及 Windows Host 组成。当前 Android 版本为 `0.9.1`、`versionCode 12`，Pad 上的测试安装包使用测试证书。

Moonlight 仓库也没有 GitHub Actions，但已有 Gradle Wrapper、NDK 构建和 `root`/`nonRoot` 产品变体。当前正式版本为 `12.2-workbuddy.1`、`versionCode 315001`，GitHub Release 已由本地构建并使用专用正式证书签名。

## 发布模型

两个仓库统一采用两类工作流：

- `ci.yml`：对分支提交和 Pull Request 运行测试、编译与静态校验，只需要只读仓库权限，不接触签名密钥。
- `release.yml`：只响应 `v*` 标签，运行完整验证后从 GitHub `release` Environment 读取签名密钥，构建正式产物并创建非草稿、非预发布的 GitHub Release。

所有第三方源码均从提交检出后重新构建。Release 不把仓库中旧的 APK 或本地构建目录直接上传。

## 桥接仓库流水线

Ubuntu Job 负责：

1. 运行协议生成一致性测试。
2. 使用主机 GCC 编译并运行全部纯 C 单元测试。
3. 安装并固定 Android NDK `29.0.14206865`，用 `aarch64-linux-android26-clang` 构建 `alttab_evdev_bridge`。
4. 将刚构建的原生程序放入 APK assets，再运行 Android JVM 单元测试及 Release 构建。
5. 用 `aapt2`、`zipalign`、`apksigner` 验证包名、版本、对齐和 v2/v3 签名。

Windows Job 使用 MinGW-w64 运行 Windows Host 单元/集成测试，并构建 `WorkBuddyAltTabHost.exe`。发布时把 EXE、状态防火墙脚本和版本说明打成 ZIP。首个自动发布标签为 `v0.9.1`，产物为：

- `WorkBuddy-Bridge-0.9.1.apk`
- `WorkBuddy-Bridge-Windows-0.9.1.zip`
- `SHA256SUMS.txt`

桥接应用创建新的专用正式签名证书。由于当前 Pad 安装的是测试证书版本，迁移到正式版时需要进行一次卸载重装；从此后的所有 GitHub Release 均可原位升级。

## Moonlight 流水线

CI 运行 `testNonRootDebugUnitTest`、`assembleNonRootDebug` 和 lint 所需的最小完整集合。Release 运行 `testNonRootDebugUnitTest assembleNonRootRelease`，让 Gradle 直接使用 Environment 中的正式签名配置，随后验证：

- 包名为 `com.limelight.workbuddy`。
- 标签与 `versionName` 完全一致。
- APK Signature Scheme v1/v2/v3 均通过。
- 签名证书 SHA-256 固定为 `1427DD5A9FEA1B17A0D04EEBED6B510EF66F9DC8A830028D31BF5A446AB6044C`。

现有 `v12.2-workbuddy.1` Release 不重写。流水线先通过手动 CI 和分支构建验证，下一次产品版本（例如 `12.2-workbuddy.2`）再通过标签触发第一个自动 Release。

## 密钥与权限

每个仓库使用独立的 `release` Environment，保存四个 Secret：

- `ANDROID_KEYSTORE_BASE64`
- `ANDROID_KEYSTORE_PASSWORD`
- `ANDROID_KEY_ALIAS`
- `ANDROID_KEY_PASSWORD`

工作流把 keystore 解码到 Runner 临时目录，Job 结束后由临时 Runner 自动销毁。密钥、密码、Base64 文本、签名配置文件都不写入 Git，不输出到日志，也不作为 Artifact 上传。

CI 使用 `contents: read`。Release 仅在发布 Job 使用 `contents: write`。Pull Request 工作流永远不加载 Environment Secrets，来自 Fork 的代码无法接触签名材料。

Moonlight 使用现有正式证书；桥接工具生成独立的 RSA 4096 正式证书，并在本机保存 DPAPI 加密的密码备份。GitHub Secrets 和本机备份都完成后才允许发布首个正式 APK。

## 版本与失败策略

工作流从 Gradle 读取 `versionName`，要求标签严格等于 `v${versionName}`；桥接工具还要求原生程序 `--describe` 中的版本与 Android 版本一致。任何测试失败、版本不匹配、签名证书不匹配、APK 未对齐或资产缺失都会在创建 Release 前终止。

Release Job 使用同一标签并发锁，避免重复发布。已存在的 Release 或同名资产不会被静默覆盖。重新发布必须提升版本号并创建新标签。

## 验收标准

- 两个仓库的 CI 在干净 GitHub Runner 上通过，不依赖 `C:\Users\...` 下的任何文件。
- 桥接工具 `v0.9.1` 由 Actions 创建正式 Release，包含三个约定产物。
- 桥接 APK 和 Moonlight APK 的签名、身份、版本、哈希都由流水线输出并验证。
- Moonlight 分支构建通过；现有 `v12.2-workbuddy.1` Release 保持不变。
- 任意未配置或错误配置签名 Secret 的发布尝试都会失败且不会产生 Release。
- 两个仓库均有简短维护文档，说明版本提升、打标签和密钥恢复步骤。
