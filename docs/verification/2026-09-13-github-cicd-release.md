# GitHub CI/CD 与发布验证记录

验证日期：2026-09-13

## 结论

- Bridge 与 Moonlight 的 CI、签名 Release 工作流均已部署到默认分支 `master`。
- 两个仓库的 Release 工作流都完成了手动签名构建验证；手动运行只产出 Actions 制品，不创建 GitHub Release。
- Bridge `v0.9.1` 已由标签触发的工作流自动发布，APK、Windows ZIP 和校验和均通过下载后复验。
- 已发布的 Moonlight `v12.2-workbuddy.1` 保持不变，CI/CD 部署没有覆盖原发布。

## Bridge

仓库：https://github.com/whyer11/workbuddy-moonlight-alt-tab-bridge

### 工作流

- 默认分支 CI：https://github.com/whyer11/workbuddy-moonlight-alt-tab-bridge/actions/runs/34762623643
- 签名 Release 手动验证：https://github.com/whyer11/workbuddy-moonlight-alt-tab-bridge/actions/runs/34762629135
- `v0.9.1` 正式发布：https://github.com/whyer11/workbuddy-moonlight-alt-tab-bridge/actions/runs/34762779155
- Release：https://github.com/whyer11/workbuddy-moonlight-alt-tab-bridge/releases/tag/v0.9.1

### 下载制品复验

| 制品 | 大小 | SHA-256 |
| --- | ---: | --- |
| `WorkBuddy-Bridge-0.9.1.apk` | 704970 bytes | `5F9E6890D4D1353522A6D7F0C4E69D752F224BB6128CDCA7A9DD473F4B423248` |
| `WorkBuddy-Bridge-Windows-0.9.1.zip` | 23516 bytes | `10643B0659504B57D022EAF402AF777118453F5B5A0E262BB0884F9E796DED97` |
| `SHA256SUMS.txt` | 194 bytes | `BDAE05EAF6F60136D4B530CF155A9DBDDC4CDEA508FB20C3803464DAD2931D1F` |

APK 验证结果：

- 包名：`local.workbuddy.alttab`
- 版本：`0.9.1`
- APK Signature Scheme：v2、v3
- 证书 SHA-256：`E2ECAC920270F8949C97397AECE57B4FB6104F99D0DAD0471810613F1431273F`
- APK 对齐、包名、版本、签名方案、证书和内置 ARM64 桥接程序均由验证脚本检查通过。

Windows ZIP 内容：

- `WorkBuddyAltTabHost.exe`
- `install-status-firewall.ps1`
- `VERSION.txt`

解压后自检输出：

```text
SELF_TEST_OK version=0.9.1 protocol=2
```

注意：正式 APK 使用了新的发布证书。若平板上现有 Bridge 是旧调试证书版本，需要先卸载旧版再安装一次 `v0.9.1`；以后同一发布证书签出的版本可以覆盖升级。

## Moonlight WorkBuddy

仓库：https://github.com/whyer11/moonlight-android

### 工作流

- 默认分支 CI：https://github.com/whyer11/moonlight-android/actions/runs/34762453080
- 签名 Release 手动验证：https://github.com/whyer11/moonlight-android/actions/runs/34762472387

手动签名制品复验：

- 文件：`Moonlight-WorkBuddy-12.2-workbuddy.1.apk`
- 大小：11144309 bytes
- SHA-256：`D3D8BBBB8DDD53022DFC4DC2DAB3E42A16058E5F064151C167DD746171AA387A`
- 包名：`com.limelight.workbuddy`
- 版本：`12.2-workbuddy.1`
- APK Signature Scheme：v1、v2、v3
- 证书 SHA-256：`1427DD5A9FEA1B17A0D04EEBED6B510EF66F9DC8A830028D31BF5A446AB6044C`

现有正式 Release 保持不变：

- Release：https://github.com/whyer11/moonlight-android/releases/tag/v12.2-workbuddy.1
- 标签提交：`cb81f70f9d9a4dae73202ebf80069596e603489c`
- 已发布 APK SHA-256：`DE6E0FBEAFA4B33B0F4D665DA050569AA89B50BD7ADCF91DB09B8A27B1C929F2`

手动工作流制品与既有 Release APK 的文件哈希不同，是独立重建时 ZIP 元数据等非确定性内容造成的；二者包名、版本和发布证书一致，且既有 Release 未被覆盖。

## 发布密钥与边界

- 两个仓库均使用 GitHub Environment `release` 保存四项 Android 签名 Secret。
- 签名材料和密码文件均未纳入 Git。
- Windows Host 当前是无独立 UI 的后台桥接程序，ZIP 同时附带防火墙安装脚本。
- Windows EXE 尚未做 Authenticode 签名；Android APK 已做正式签名并固定校验证书指纹。
