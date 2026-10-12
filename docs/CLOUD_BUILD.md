# 云编译与 Windows 安装

[English](CLOUD_BUILD.en.md) | 简体中文

Windows 用户可以在 GitHub 上触发云端 ARM64 Mac 编译，下载 IPA，再在本机签名安装。此流程无需本地 Mac；云端使用 Xcode 27，从仓库源码编译全部 35 个原生框架。设备需要 **iOS / iPadOS 27.0 或更新版本**。

需要**仅通过 GitHub 生成 IPA 和数据包**时，运行新工作流 **Build iCSM IPA + Data**，按[完整指南](DATA_BUILD.md#简体中文)操作。新流程会重新编译 7,906 份着色器并将配套数据包放到 Releases。下方原 IPA 流程仍配合冻结 iCloud 包使用；此前成功记录不代表新完整流程已验证。

## 1. 在 GitHub 编译

1. 打开自己的 Fork，或有权限运行工作流的仓库副本。
2. 进入 **Actions → Build iCSM IPA**。若 Fork 提示启用 Actions，先启用工作流。
3. 点击 **Run workflow**，选择 `main` 分支；可保留默认 Bundle ID 和 `2` 个编译任务，也可填写自己的 Bundle ID。
4. 等待运行完成并出现绿色勾号。打开该次运行，在下方 **Artifacts** 下载 **iCSM-IPA**。
5. 解压下载的产物 ZIP，得到 **`iCSM-unsigned.ipa`**、`iCSM.entitlements`、`ipa-report.json` 和 `SHA256SUMS.txt`。

云编译不需要 Apple 登录、证书、描述文件、设备 ID 或仓库 Secret。私有仓库会使用账户的 Actions 配额，云端运行环境和限制以 GitHub 为准。产物保留七天，过期后可重新运行编译。

自动验收会检查 ZIP 完整性、全部 35 个框架、ARM64 iOS 真机二进制、动态库依赖和应用必需资源。报告记录源码提交和 IPA 的 SHA-256。在 Windows PowerShell 中运行 `Get-FileHash .\iCSM-unsigned.ipa -Algorithm SHA256`，可与报告核对。

## 2. 在 Windows 签名安装

云端 IPA 使用 **ad-hoc 临时签名**，没有 Apple 描述文件，必须使用自己的 Apple 账户或证书重新签名，才能安装到普通 iPhone / iPad。

1. 按 [Sideloadly 官网](https://sideloadly.io/) 的说明安装 Windows 版及所需依赖。
2. 连接并解锁设备，信任电脑，在设备上开启开发者模式。
3. 在 Sideloadly 中选择设备，载入 `iCSM-unsigned.ipa`，使用自己的 Apple 账户签名安装。
4. 若设备要求信任开发者，在“设置 → 通用 → VPN 与设备管理”中完成。

免费账户签名通常七天到期，可按 [Sideloadly FAQ](https://sideloadly.io/faq) 刷新或重新签名。更新时保留相同 Apple 账户和 Bundle ID，覆盖安装以保留数据包和配置。

工程请求 `com.apple.developer.kernel.increased-memory-limit` 权限；ad-hoc 应用签名保留了该项，产物也附带 `iCSM.entitlements`。最终权限由签名工具和描述文件决定。如果签名工具移除该项，支持额外内存的设备可能只能使用较低的内存额度。云编译验收本身不代表已经验证 Windows 签名工具及设备游玩效果。

## 3. 导入数据包

也可在设备的系统“文件”中下载配套 ZIP，打开 iCSM 后点击初始页左上角“选择数据包 ZIP”直接选择，无需解压或改名，此方式保留原 ZIP。导入失败可从右上角“导出报错日志”。

下载 **[iCSM-v1.zip（iCloud）](https://www.icloud.com/iclouddrive/039w_r3_mnpuCiw9Ecvmvr7zA)**，保留文件名，**不要解压数据包 ZIP**。

在 **Apple Devices → 设备 → 文件 → iCSM** 中添加 `iCSM-v1.zip`。传输完成后，打开游戏并点击“检查并导入”。首次导入请至少预留 **35 GB** 空间，详细步骤见[数据导入指南](BUILD.md)。

## 在 Mac 本机生成相同产物

```sh
./icsm build --unsigned --bundle-id com.icsm.client --jobs 2
python3 scripts/verify_ipa.py Build/iCSM-unsigned.ipa --unsigned --report Build/ipa-report.json
```

此模式不读取 `config.local.json`。使用 Xcode 本机签名的流程继续按[原 Mac 编译说明](BUILD.md)执行。

## 云编译验收记录

[Build 149 完整云端运行](https://github.com/zktfjcksxk-spec/iCSM/actions/runs/37881597437)于 **2026 年 10 月 9 日**通过，对应源码提交 `7594166`，使用 Xcode 27.0 / iPhoneOS SDK 27.0、两个并行编译任务。全部 **3,848 项原生构建任务**、IPA 导出、**35 个框架／36 个 ARM64 iOS 二进制**验收和产物上传通过，总耗时约 **39 分钟**。下载后的 IPA 已独立核对 SHA-256、ZIP CRC、动态库依赖、内存权限和完整的 ad-hoc 签名，详见[验收记录](validation/build-149.json)。本次云端产物尚未进行设备安装和游玩测试；安装前须使用自己的账户签名。

[首次完整云端运行](https://github.com/zktfjcksxk-spec/iCSM/actions/runs/37577652912)于 **2026 年 10 月 7 日**通过，使用 Xcode 27.0 / iPhoneOS SDK 27.0、两个并行编译任务。全部 **3,845 项原生构建任务**完成，IPA 导出、**35 个框架／36 个 ARM64 iOS 二进制**验收和产物上传全部成功，总耗时约 **35 分钟**。下载的 IPA 也已独立核对校验和、依赖和签名完整性。本次未测试 Windows 实机签名安装及游玩。

返回[项目主页](https://github.com/zktfjcksxk-spec/iCSM#chinese)。


当前源码已更新至 **Build200**；上方云端成功记录仅适用于 Build149。最新已验证发布见 [Build200](https://github.com/zktfjcksxk-spec/iCSM/releases/tag/v0.7.2-build200)。
