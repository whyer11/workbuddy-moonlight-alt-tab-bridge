# WorkBuddy 双仓库 GitHub CI/CD 与自动发布实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 WorkBuddy 桥接工具和 Moonlight WorkBuddy 在 GitHub Runner 上自动测试、构建、签名并按版本标签发布可校验的正式安装包。

**Architecture:** 两个仓库各自维护只读 CI 与标签发布工作流。桥接仓库采用 Ubuntu/Windows 两个构建 Job 后汇总发布 APK、Windows ZIP 和校验文件；Moonlight 使用现有 Gradle/NDK 构建链并复用已经发布过正式 APK 的证书。签名材料仅存在 GitHub `release` Environment 和本机 DPAPI 备份中。

**Tech Stack:** GitHub Actions、GitHub CLI、Gradle 9.6/9.7.1、Android Gradle Plugin、JDK 17、Android SDK 36/37、Android NDK 29.0.14206865、GCC/MinGW-w64、PowerShell 7、`aapt2`、`zipalign`、`apksigner`。

**Spec:** `docs/superpowers/specs/2026-09-13-github-cicd-release-design.md`

## Global Constraints

- 桥接仓库为 `whyer11/workbuddy-moonlight-alt-tab-bridge`；Moonlight 仓库为 `whyer11/moonlight-android`。
- 桥接 Android 身份保持 `applicationId local.workbuddy.alttab`、`versionCode 12`、`versionName 0.9.1`；首个自动 Release 标签为 `v0.9.1`。
- Moonlight 身份保持 `applicationId com.limelight.workbuddy`；现有 `v12.2-workbuddy.1` Release 不覆盖、不删除、不替换资产。
- Moonlight 正式证书 SHA-256 必须为 `1427DD5A9FEA1B17A0D04EEBED6B510EF66F9DC8A830028D31BF5A446AB6044C`。
- 桥接工具使用新建的独立 RSA 4096 正式证书；Pad 从测试证书迁移时允许一次卸载重装。
- Android NDK 固定为 `29.0.14206865`，原生目标固定为 `aarch64-linux-android26`。
- Secret 名统一为 `ANDROID_KEYSTORE_BASE64`、`ANDROID_KEYSTORE_PASSWORD`、`ANDROID_KEY_ALIAS`、`ANDROID_KEY_PASSWORD`。
- CI 不读取签名 Secret；Release Secret 只放入 GitHub `release` Environment，任何密钥或密码不得写入仓库、日志或 Artifact。
- CI 权限为 `contents: read`；只有标签发布 Job 使用 `contents: write`。
- 已存在的 Release 和资产不覆盖；发布失败时不得留下草稿 Release。
- 执行由当前主任务单人完成，不分派子代理；每个 Task 验收后再进入下一个 Task。

---

## 文件结构

### 桥接仓库

- Create: `.github/workflows/ci.yml` — 分支/PR 的 Android、原生和 Windows 验证编排。
- Create: `.github/workflows/release.yml` — 标签构建、签名、跨 Job 汇总和 GitHub Release 创建。
- Create: `scripts/ci/build-native-android.sh` — 唯一的 ARM64 原生程序构建入口。
- Create: `scripts/ci/run-native-tests.sh` — Linux 纯 C 测试入口。
- Create: `scripts/ci/build-windows.ps1` — Windows 测试、Host 编译与 ZIP 打包入口。
- Create: `scripts/ci/verify-android-release.ps1` — 标签、包名、版本、对齐、签名与 APK 内资产校验。
- Create: `android-bridge/gradlew`, `android-bridge/gradlew.bat`, `android-bridge/gradle/wrapper/gradle-wrapper.jar`, `android-bridge/gradle/wrapper/gradle-wrapper.properties` — 固定 Gradle 9.6.0。
- Modify: `android-bridge/app/build.gradle` — 从环境变量建立 Release signingConfig；不改变无 Secret 的本地 Debug 构建。
- Modify: `.gitignore` — 排除 keystore、签名临时目录和发布输出。
- Modify: `README.md` — 记录 Release 安装、一次性签名迁移和维护流程。
- Create: `docs/verification/2026-09-13-github-cicd-release.md` — 记录 Actions run、Release URL、资产哈希和证书指纹。

### Moonlight 仓库

- Create: `.github/workflows/ci.yml` — NonRoot 单测、Debug 构建和必要静态检查。
- Create: `.github/workflows/release.yml` — NonRoot 正式构建、签名校验和标签发布。
- Create: `scripts/ci/verify-release.ps1` — 校验标签、包名、版本、签名证书和产物命名。
- Modify: `app/build.gradle` — 仅在四个签名环境变量齐全时配置 Release signingConfig。
- Modify: `.gitignore` — 排除 CI、本地签名临时文件。
- Modify: `README.md` — 记录 WorkBuddy Fork 的 CI/CD 和下一版本发布方式。

---

### Task 1: 固化桥接仓库 Gradle 与 CI 脚本接口

**Files:**

- Create: `android-bridge/gradlew`
- Create: `android-bridge/gradlew.bat`
- Create: `android-bridge/gradle/wrapper/gradle-wrapper.jar`
- Create: `android-bridge/gradle/wrapper/gradle-wrapper.properties`
- Create: `scripts/ci/run-native-tests.sh`
- Create: `scripts/ci/build-native-android.sh`
- Test: `native-bridge/tests/*.c`
- Test: `protocol/tests/GeneratedSchemaTest.ps1`

**Interfaces:**

- Consumes: 仓库根目录、环境变量 `ANDROID_NDK_ROOT`。
- Produces: `build/ci/native/alttab_evdev_bridge`；脚本成功返回 `0`，失败返回非 `0`。

- [ ] **Step 1: 生成并检查 Gradle Wrapper**

在桥接仓库根目录执行：

```powershell
$env:JAVA_HOME = 'C:\Users\wanghuanyu\WorkBuddy\2026-09-12-19-05-57\tools\AltTabBridge\.tools\jdk-extract\jdk-17.0.20.1+1'
& 'C:\Users\wanghuanyu\WorkBuddy\2026-09-12-19-05-57\tools\AltTabBridge\.tools\gradle-extract\gradle-9.6.0\bin\gradle.bat' -p android-bridge wrapper --gradle-version 9.6.0
& .\android-bridge\gradlew.bat -p android-bridge --version
```

Expected: 输出 `Gradle 9.6.0`，Wrapper 文件中不存在绝对本机路径。

- [ ] **Step 2: 先写原生测试脚本的失败验收**

创建 `scripts/ci/run-native-tests.sh`，使用 `set -euo pipefail`，把以下测试逐个编译到 `build/ci/tests` 并执行：

```bash
cc native-bridge/tests/key_remap_test.c native-bridge/src/key_remap.c -I native-bridge/src -std=c11 -Wall -Wextra -Werror -pedantic -o build/ci/tests/key_remap_test
cc native-bridge/tests/native_protocol_test.c native-bridge/src/native_protocol.c -I native-bridge/src -std=c11 -Wall -Wextra -Werror -pedantic -o build/ci/tests/native_protocol_test
cc native-bridge/tests/touchpad_drag_state_test.c native-bridge/src/touchpad_drag_state.c -I native-bridge/src -std=c11 -Wall -Wextra -Werror -pedantic -o build/ci/tests/touchpad_drag_state_test
cc native-bridge/tests/bridge_options_test.c native-bridge/src/bridge_options.c -I native-bridge/src -std=c11 -Wall -Wextra -Werror -pedantic -o build/ci/tests/bridge_options_test
```

脚本依次运行四个二进制，并要求每个输出包含 `OK`。暂时把其中一个源文件名写错，运行脚本，确认它立即非零退出；随后恢复正确文件名。

- [ ] **Step 3: 实现 ARM64 构建脚本**

创建 `scripts/ci/build-native-android.sh`，严格验证 `ANDROID_NDK_ROOT`，并执行：

```bash
"$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android26-clang" \
  -std=c11 -O2 -Wall -Wextra -Werror -pedantic \
  -I native-bridge/src \
  native-bridge/src/alttab_evdev_bridge.c \
  native-bridge/src/bridge_options.c \
  native-bridge/src/key_remap.c \
  native-bridge/src/native_protocol.c \
  native-bridge/src/touchpad_drag_state.c \
  native-bridge/src/touchpad_bridge.c \
  -o build/ci/native/alttab_evdev_bridge
```

输出后运行 `file`，要求包含 `ELF 64-bit`、`ARM aarch64`；运行 `readelf -h`，要求 Machine 为 `AArch64`。

- [ ] **Step 4: 本地验证脚本语法与现有测试**

```powershell
& pwsh -NoProfile -File .\protocol\tests\GeneratedSchemaTest.ps1
bash -n scripts/ci/run-native-tests.sh
bash -n scripts/ci/build-native-android.sh
```

Expected: 协议测试 PASS，两个 shell 脚本语法检查为零退出。

- [ ] **Step 5: 提交 Gradle 与构建入口**

```powershell
git add android-bridge/gradlew android-bridge/gradlew.bat android-bridge/gradle/wrapper scripts/ci/run-native-tests.sh scripts/ci/build-native-android.sh
git commit -m "build: add reproducible bridge build entrypoints"
git push
```

---

### Task 2: 建立桥接 Android 正式签名与发布校验

**Files:**

- Modify: `android-bridge/app/build.gradle`
- Create: `scripts/ci/verify-android-release.ps1`
- Modify: `.gitignore`

**Interfaces:**

- Consumes: `ANDROID_KEYSTORE_FILE`、`ANDROID_KEYSTORE_PASSWORD`、`ANDROID_KEY_ALIAS`、`ANDROID_KEY_PASSWORD`、`RELEASE_TAG`。
- Produces: 已签名 `android-bridge/app/build/outputs/apk/release/app-release.apk`；验证脚本返回包版本和 SHA-256。

- [ ] **Step 1: 写签名配置缺失的失败检查**

在 `verify-android-release.ps1` 开头要求四个签名环境变量和 `RELEASE_TAG` 非空；使用空环境运行：

```powershell
Remove-Item Env:ANDROID_KEYSTORE_FILE,Env:ANDROID_KEYSTORE_PASSWORD,Env:ANDROID_KEY_ALIAS,Env:ANDROID_KEY_PASSWORD -ErrorAction SilentlyContinue
& pwsh -NoProfile -File scripts/ci/verify-android-release.ps1 -ApkPath missing.apk -Aapt2 aapt2 -Zipalign zipalign -Apksigner apksigner
```

Expected: 非零退出并明确报告缺少签名环境变量，不输出任何 Secret 值。

- [ ] **Step 2: 在 Gradle 中增加环境驱动的 Release signingConfig**

在 `android-bridge/app/build.gradle` 中读取四个环境变量；只有全部存在时创建 `signingConfigs.release` 并赋给 `buildTypes.release`。若运行 `assembleRelease` 时变量不完整，使用 `GradleException('Release signing environment is incomplete')` 终止。`assembleDebug` 不需要签名环境变量。

- [ ] **Step 3: 完成 Release 验证脚本**

`verify-android-release.ps1` 必须执行并断言：

```powershell
$expectedVersion = $env:RELEASE_TAG.TrimStart('v')
$badging = & $Aapt2 dump badging $ApkPath
$signature = & $Apksigner verify --verbose --print-certs $ApkPath
& $Zipalign -c -P 16 -v 4 $ApkPath
```

要求 badging 含 `package: name='local.workbuddy.alttab'` 与 `versionName='$expectedVersion'`；签名输出含 v2/v3 `true`；ZIP 内存在 `assets/alttab_evdev_bridge`。脚本最后输出 APK SHA-256 和签名证书 SHA-256，不输出密码。

- [ ] **Step 4: 防止签名材料进入 Git**

在 `.gitignore` 增加：

```gitignore
*.jks
*.keystore
*.p12
.signing/
build/ci/
```

运行 `git check-ignore` 验证 `.signing/release.p12` 被忽略。

- [ ] **Step 5: 用临时证书执行本地冒烟构建**

创建仅用于本步骤的临时 keystore，设置四个环境变量，把 Task 1 的 ARM64 产物复制到 `android-bridge/app/src/main/assets/alttab_evdev_bridge`，运行：

```powershell
& .\android-bridge\gradlew.bat -p android-bridge :app:testDebugUnitTest :app:assembleRelease --no-daemon
```

再运行验证脚本。Expected: Java 测试通过、APK 对齐、v2/v3 签名通过、包名与 `v0.9.1` 一致。删除临时 keystore；不得提交测试证书或本步骤生成的 asset 差异。

- [ ] **Step 6: 提交签名与验证逻辑**

```powershell
git add android-bridge/app/build.gradle scripts/ci/verify-android-release.ps1 .gitignore
git commit -m "build: verify signed bridge releases"
git push
```

---

### Task 3: 建立桥接 Windows 构建与发布包

**Files:**

- Create: `scripts/ci/build-windows.ps1`
- Test: `windows-host/tests/*.c`
- Package: `windows-host/install-status-firewall.ps1`

**Interfaces:**

- Consumes: 环境变量 `RELEASE_VERSION`，PATH 中的 MinGW-w64 `gcc`。
- Produces: `build/ci/windows/WorkBuddy-Bridge-Windows-${RELEASE_VERSION}.zip`。

- [ ] **Step 1: 写缺少编译器和版本的失败测试**

脚本在开始时运行 `Get-Command gcc` 并校验 `RELEASE_VERSION`。分别清空版本和传入不存在的 gcc 路径，要求脚本非零退出且不产生 ZIP。

- [ ] **Step 2: 编译并运行 Windows 测试**

脚本把现有测试编译到 `build/ci/windows/tests`，至少覆盖：

```powershell
gcc windows-host/tests/AltTabBridgeIntegrationTest.c windows-host/src/KeyboardBridgeCore.c -I windows-host/src -std=c11 -O2 -Wall -Wextra -Werror -o build/ci/windows/tests/AltTabBridgeIntegrationTest.exe -luser32
gcc windows-host/tests/WinKeyBridgeIntegrationTest.c -std=c11 -O2 -Wall -Wextra -Werror -o build/ci/windows/tests/WinKeyBridgeIntegrationTest.exe -luser32
gcc windows-host/tests/StatusProtocolTest.c windows-host/src/WorkBuddyStatusProtocol.c -I windows-host/src -std=c11 -O2 -Wall -Wextra -Werror -o build/ci/windows/tests/StatusProtocolTest.exe
gcc windows-host/tests/StatusServerIntegrationTest.c windows-host/src/WorkBuddyStatusProtocol.c windows-host/src/WorkBuddyStatusServer.c -I windows-host/src -std=c11 -O2 -Wall -Wextra -Werror -o build/ci/windows/tests/StatusServerIntegrationTest.exe -lws2_32
```

还要运行两个 `Firewall*Test.ps1` 和 `FirewallSpecTest.ps1`；任何测试失败立即停止。

- [ ] **Step 3: 构建并打包 Windows Host**

使用：

```powershell
gcc windows-host/src/WorkBuddyAltTabHost.c windows-host/src/KeyboardBridgeCore.c windows-host/src/WorkBuddyStatusProtocol.c windows-host/src/WorkBuddyStatusServer.c -I windows-host/src -std=c11 -O2 -Wall -Wextra -Werror -municode -mwindows -o build/ci/windows/package/WorkBuddyAltTabHost.exe -luser32 -lws2_32
```

把 `WorkBuddyAltTabHost.exe`、`windows-host/install-status-firewall.ps1` 和生成的 `VERSION.txt` 放入 ZIP。`VERSION.txt` 写入版本、Git commit 和“未做 Authenticode 签名”的明确说明。

- [ ] **Step 4: 本机验证 ZIP**

展开 ZIP 到临时目录，运行 `WorkBuddyAltTabHost.exe --self-test`；要求零退出。检查 ZIP 不含源码构建临时文件和任何 keystore。

- [ ] **Step 5: 提交 Windows 构建入口**

```powershell
git add scripts/ci/build-windows.ps1
git commit -m "build: package the Windows bridge host"
git push
```

---

### Task 4: 建立桥接 CI 工作流

**Files:**

- Create: `.github/workflows/ci.yml`

**Interfaces:**

- Consumes: GitHub push/Pull Request 事件，无 Secret。
- Produces: Android Debug APK 与 Windows Host ZIP 的短期 CI Artifacts；两个平台的测试结论。

- [ ] **Step 1: 创建最小权限工作流**

`ci.yml` 触发条件为 `push` 和 `pull_request`，顶层设置：

```yaml
permissions:
  contents: read
concurrency:
  group: bridge-ci-${{ github.ref }}
  cancel-in-progress: true
```

建立 `android`（`ubuntu-latest`）与 `windows`（`windows-latest`）两个 Job，均使用 `actions/checkout@v4`。

- [ ] **Step 2: 配置 Android Job**

使用 `actions/setup-java@v4` 配置 Temurin 17，使用 `gradle/actions/setup-gradle@v4`。通过 `sdkmanager` 安装 `platforms;android-36`、`build-tools;36.0.0`、`ndk;29.0.14206865`。依次运行：

```bash
pwsh -NoProfile -File protocol/tests/GeneratedSchemaTest.ps1
bash scripts/ci/run-native-tests.sh
ANDROID_NDK_ROOT="$ANDROID_SDK_ROOT/ndk/29.0.14206865" bash scripts/ci/build-native-android.sh
install -m 0755 build/ci/native/alttab_evdev_bridge android-bridge/app/src/main/assets/alttab_evdev_bridge
./android-bridge/gradlew -p android-bridge :app:testDebugUnitTest :app:assembleDebug --no-daemon
```

上传 `app-debug.apk`，保留 7 天。

- [ ] **Step 3: 配置 Windows Job**

安装 MinGW-w64 GCC，设置 `RELEASE_VERSION=ci`，运行 `scripts/ci/build-windows.ps1`，上传 Windows ZIP，保留 7 天。安装步骤不得执行仓库代码提供的未校验下载器。

- [ ] **Step 4: 先验证 YAML 与无 Secret 属性**

运行 YAML 解析；搜索工作流并要求不存在 `ANDROID_KEYSTORE_`、`environment: release` 和 `contents: write`。

- [ ] **Step 5: 提交并观察 GitHub CI**

```powershell
git add .github/workflows/ci.yml
git commit -m "ci: validate bridge builds on GitHub"
git push
& 'C:\Program Files\GitHub CLI\gh.exe' run watch --repo whyer11/workbuddy-moonlight-alt-tab-bridge --exit-status
```

Expected: Android、Windows Job 均通过。失败时只修复根因并追加独立 commit，不跳过测试。

---

### Task 5: 配置桥接正式密钥和标签发布工作流

**Files:**

- Create: `.github/workflows/release.yml`
- Modify: `README.md`

**Interfaces:**

- Consumes: `v*` 标签与 GitHub `release` Environment 中的四个 Secret。
- Produces: APK、Windows ZIP、`SHA256SUMS.txt` 和 GitHub Release。

- [ ] **Step 1: 生成桥接正式证书并建立可恢复备份**

在仓库外创建：

```text
%LOCALAPPDATA%\WorkBuddy\AltTabBridge\signing\workbuddy-bridge-release.p12
%LOCALAPPDATA%\WorkBuddy\AltTabBridge\signing\workbuddy-bridge-release-password.dpapi
```

证书使用 RSA 4096、alias `workbuddy-bridge-release`、有效期至少 25 年。密码由密码学安全随机源生成并立即用当前 Windows 用户 DPAPI 加密；命令行和日志不出现明文密码。

- [ ] **Step 2: 创建 GitHub release Environment 并写入 Secrets**

使用 GitHub API 创建 `release` Environment。keystore Base64 和三项文本 Secret 全部通过标准输入传给 `gh secret set --env release`，不能作为命令行参数。随后用 `gh secret list --env release` 仅确认名称和更新时间。

- [ ] **Step 3: 创建跨平台 Release 工作流**

`release.yml` 仅触发：

```yaml
on:
  push:
    tags: ['v*']
permissions:
  contents: read
concurrency:
  group: bridge-release-${{ github.ref }}
  cancel-in-progress: false
```

Android Job 使用 `environment: release`，解码临时 keystore，构建原生 asset，运行全部测试和 `assembleRelease`，再调用 `verify-android-release.ps1`。Windows Job 运行 Task 3 脚本。两个 Job 都只上传内部 workflow Artifact。

- [ ] **Step 4: 创建原子发布 Job**

`publish` Job 依赖 Android/Windows Job，单独设置：

```yaml
permissions:
  contents: write
```

下载两个产物，重命名为约定文件名，生成按文件名字典序排列的 `SHA256SUMS.txt`。先用 `gh release view "$GITHUB_REF_NAME"` 断言 Release 不存在，再执行一次 `gh release create`，同时上传三个文件；失败时不创建草稿。

- [ ] **Step 5: 更新发布维护文档**

README 增加：版本号先改 `android-bridge/app/build.gradle` 与原生版本，再合并代码，最后创建 `v${versionName}` 标签。记录首次正式 APK 需卸载测试签名版、后续可覆盖升级，Windows EXE 未做 Authenticode 签名。

- [ ] **Step 6: 提交并推送 Release 工作流**

```powershell
git add .github/workflows/release.yml README.md
git commit -m "ci: publish signed bridge releases"
git push
```

---

### Task 6: 发布并验证桥接工具 v0.9.1

**Files:**

- Create: `docs/verification/2026-09-13-github-cicd-release.md`

**Interfaces:**

- Consumes: 已通过的桥接 CI、远端提交和四个签名 Secret。
- Produces: GitHub Release `v0.9.1` 与可审计验证记录。

- [ ] **Step 1: 发布前本地门禁**

要求 `git status --porcelain` 为空、当前分支与 upstream ahead/behind 为 `0 0`、CI 最新 run 成功、`gh release view v0.9.1` 返回不存在、远端同名标签不存在。

- [ ] **Step 2: 创建并推送精确标签**

```powershell
git tag -a v0.9.1 -m "WorkBuddy Bridge 0.9.1"
git push origin v0.9.1
```

- [ ] **Step 3: 等待 Release 工作流完成**

使用 `gh run watch --exit-status` 等待，不在运行期间修改标签。Expected: Android、Windows、publish 三部分全部成功。

- [ ] **Step 4: 反查 Release 和资产**

用 `gh release view v0.9.1 --json` 确认非 draft、非 prerelease，资产名和数量准确。逐个下载到临时目录，运行 `sha256sum -c SHA256SUMS.txt`。对 APK 再运行 `aapt2`、`zipalign`、`apksigner`，记录正式证书 SHA-256。

- [ ] **Step 5: 写验证记录并提交**

记录 GitHub Actions run URL、标签提交、Release URL、三个资产大小和哈希、APK 包名/版本/签名方案/证书指纹、Windows `--self-test` 结果。

```powershell
git add docs/verification/2026-09-13-github-cicd-release.md
git commit -m "docs: record bridge release verification"
git push
```

---

### Task 7: 给 Moonlight 增加环境驱动的正式签名

**Files:**

- Modify: `app/build.gradle`
- Create: `scripts/ci/verify-release.ps1`
- Modify: `.gitignore`

**Interfaces:**

- Consumes: `ANDROID_KEYSTORE_FILE`、`ANDROID_KEYSTORE_PASSWORD`、`ANDROID_KEY_ALIAS`、`ANDROID_KEY_PASSWORD`、`RELEASE_TAG`。
- Produces: 已签名 NonRoot Release APK；验证脚本要求既有证书指纹。

- [ ] **Step 1: 写错误证书和错误标签的失败验收**

验证脚本固定期望包名 `com.limelight.workbuddy` 和证书 SHA-256 `1427DD5A9FEA1B17A0D04EEBED6B510EF66F9DC8A830028D31BF5A446AB6044C`。用 Debug APK 和 `RELEASE_TAG=v0.0.0` 分别运行，要求因证书与版本不匹配非零退出。

- [ ] **Step 2: 修改 Gradle Release signingConfig**

采用与桥接仓库相同的四环境变量接口。没有变量时 Debug 和 unsigned Release 仍可构建；`CI=true` 且执行 Release 任务时，变量不完整必须失败。`nonRootRelease` 在变量完整时必须由 Gradle 直接签名，不再执行构建后的二次签名。

- [ ] **Step 3: 实现 Moonlight 验证脚本**

脚本通过 `aapt2 dump badging`、`zipalign -c`、`apksigner verify --verbose --print-certs` 验证包名、标签版本、v1/v2/v3 和固定证书指纹，输出 APK SHA-256。

- [ ] **Step 4: 本地使用现有正式证书回归**

从 DPAPI 文件在进程内解密密码并设置环境变量，运行：

```powershell
& .\gradlew.bat testNonRootDebugUnitTest assembleNonRootRelease --no-daemon --stacktrace
```

调用验证脚本，要求证书指纹与 `v12.2-workbuddy.1` 一致。清理环境变量；日志中搜索不得出现密码或 keystore Base64。

- [ ] **Step 5: 提交签名和验证逻辑**

```powershell
git add app/build.gradle scripts/ci/verify-release.ps1 .gitignore
git commit -m "build: support verified Moonlight releases"
git push
```

---

### Task 8: 建立 Moonlight CI 与自动 Release 工作流

**Files:**

- Create: `.github/workflows/ci.yml`
- Create: `.github/workflows/release.yml`
- Modify: `README.md`

**Interfaces:**

- Consumes: 分支/PR 或 `v*` 标签；Release 使用 GitHub `release` Environment。
- Produces: CI Debug APK；未来版本标签对应的正式 APK、`SHA256SUMS.txt` 和 GitHub Release。

- [ ] **Step 1: 创建 Moonlight CI**

CI 使用只读权限、Temurin 17、仓库 Gradle Wrapper和 NDK `29.0.14206865`，运行：

```bash
./gradlew testNonRootDebugUnitTest assembleNonRootDebug --no-daemon --stacktrace
```

上传 Debug APK 并保留 7 天。CI 文件不引用 Environment 或签名 Secrets。

- [ ] **Step 2: 配置现有 Moonlight 正式证书 Secrets**

创建 `release` Environment，把现有 `moonlight-workbuddy-release.p12` 的 Base64、DPAPI 解密得到的密码、alias `moonlight-workbuddy-release` 写入四个 Secrets。只用 `gh secret list --env release` 验证名称。

- [ ] **Step 3: 创建 Moonlight Release 工作流**

标签触发、并发锁与权限模型和桥接仓库相同。Release Job 解码 keystore，执行：

```bash
./gradlew testNonRootDebugUnitTest assembleNonRootRelease --no-daemon --stacktrace
```

运行 `scripts/ci/verify-release.ps1` 后将 APK 命名为 `Moonlight-WorkBuddy-${version}.apk`，生成 `SHA256SUMS.txt`。发布 Job 先断言 Release 不存在，再用 `gh release create` 一次性上传两个文件。

- [ ] **Step 4: 更新 Moonlight 发布文档**

README 说明：修改 `versionName`/`versionCode`、CI 通过后创建同名 `v${versionName}` 标签；`v12.2-workbuddy.1` 是最后一个本机构建 Release，下一版本开始由 GitHub Actions 发布。

- [ ] **Step 5: 提交并推送工作流**

```powershell
git add .github/workflows/ci.yml .github/workflows/release.yml README.md
git commit -m "ci: automate Moonlight WorkBuddy releases"
git push
```

- [ ] **Step 6: 验证分支 CI，不创建重复 Release**

等待最新 CI run 成功，下载 Debug Artifact 并校验包名为 `com.limelight.workbuddy.debug`。不得重新推送 `v12.2-workbuddy.1`，不得替换现有 Release 资产。

---

### Task 9: 双仓库安全与回归审计

**Files:**

- Modify if evidence requires: 两仓库 `.github/workflows/*.yml`
- Modify if evidence requires: 两仓库 `README.md`

**Interfaces:**

- Consumes: 两仓库已完成的 CI/CD 配置和 GitHub 状态。
- Produces: 可复核的最终审计结果，不产生新的产品行为变化。

- [ ] **Step 1: Secret 泄漏扫描**

搜索 tracked files、Git diff 和 Actions logs，要求不存在 keystore Base64、明文密码、`.p12` 内容或 DPAPI blob。运行 `git ls-files` 确认无 keystore 文件。

- [ ] **Step 2: 权限与触发器审计**

确认 PR CI 均为 `contents: read` 且不声明 `environment: release`；`contents: write` 只存在于标签 Release 的 publish Job；Release Job 不响应 `pull_request_target`。

- [ ] **Step 3: 可重复性审计**

桥接 CI 必须从源码重新生成 ARM64 asset；Moonlight 必须从仓库 Gradle Wrapper 和固定 NDK 构建。两个 Release 都拒绝标签与 versionName 不一致。

- [ ] **Step 4: 最终 Git 与 GitHub 状态检查**

两个仓库分别要求：工作树干净、分支 ahead/behind `0 0`、最新 CI 成功。桥接 Release `v0.9.1` 可下载且哈希验证通过；Moonlight `v12.2-workbuddy.1` URL、资产和哈希保持不变。

- [ ] **Step 5: 最终汇报**

汇报桥接 Release URL、Moonlight CI run URL、两个证书指纹、正式资产 SHA-256，以及 Pad 从桥接测试证书迁移到正式证书需要一次卸载重装。不要声称 Moonlight 新自动 Release 已发布；只有下一个新版本标签才会触发它。
