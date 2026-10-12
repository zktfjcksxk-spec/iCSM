<a name="english"></a>

# Build 200 · October 12, 2026

English | [简体中文](#chinese)

The mobile source, unsigned IPA, matching data package and Ubuntu x86_64 dedicated-server sources are updated together. Version remains 0.7.2; app build is 200.

- Restore firing recoil and view punch; bot takeover retains the bot's active weapon, ammunition and equipment. Kill icons use actual item definitions, including silenced variants.
- Keep UI/font rendering independent of scene resolution; fix left-half movement capture and improve touch sliders. Add fixed-speed/velocity-accelerated aiming, rifle/sniper sensitivity, custom keyboard-mapped buttons and touch-control hiding.
- Restore Xbox-standard controller mapping, isolate left/right sticks, ignore extra buttons, add independent deadzones, and retain controller crash fixes across incremental links.
- Add the available 2019 sticker and glove catalogs. Stickers use original slots and can replace an occupied slot directly; sticker previews are intentionally omitted. Gloves use the knife-style selection list. Cosmetic attribute lookup, material warmup, attachment reuse, menu load and memory use are optimized.
- Startup announcement appears only on the first successful data start, for ten seconds. Existing player preferences and inventory are retained.
- Refine MetalFX motion confidence and previous-depth occlusion checks, reset history on large camera changes, and keep UI/viewmodel protection. Protected weapon animations remain at the native rate.
- Add **Video → 插帧性能测试**: repeatable three-map quick or twelve-map full sweeps, 5v5 bots, turning/firing, three interpolation-detail candidates, per-device saved recommendations, explicit apply, cancellation and interruption recovery. Quality/resolution changes invalidate old recommendations.
- Update Ubuntu server recoil, bot handoff, kill identity and validated sticker attribute replication while retaining protocol 2, detachable silencers, password checks and the four-map competitive rotation.

**Validation:** Build 200's iPad installation, all 36 ARM64/iOS binary signatures, quick-mode 36 samples with real 5v5 counts, saved recommendation application, cancellation and forced-termination recovery passed. No GPU errors were recorded in the 36 samples. At the tested iPad high preset, detail 50% was the conservative choice; some samples remained below 120 FPS. The twelve-map full mode was not executed. Build 200 has not been installed on iPhone in this update. See [frame-calibration measurements](validation/frame-calibration-200.json).

The resource/shader identity is unchanged from the existing paired data, so existing verified data remain compatible. The repackaged data are validated against the IPA manifest. The full IPA + Data cloud workflow has not been run; Build 149 cloud success does not certify Build 200. Ubuntu full compilation/linking and 17 ELF64 x86_64 images/dependencies passed; deployment/multiplayer runtime validation remains separate.

---

<a name="chinese"></a>

# Build 200 · 2026 年 10 月 12 日

[English](#english) | 简体中文

同步更新客户端源码、未签名 IPA、配套数据包与 Ubuntu x86_64 服务端源码。版本号保持0.7.2，构建号200。

- 恢复枪械后坐力和视角抖动；接管 BOT 保留其当前武器、弹药和装备；击杀图标按真实武器定义显示，覆盖消音型号。
- 字体／UI 与场景分辨率独立；修复左半屏移动中断，优化触屏滑条；增加固定速度／速度加速、步枪镜／狙击镜灵敏度、自定义键盘映射按钮和隐藏触控功能。
- 恢复 Xbox 标准手柄布局，隔离左右摇杆、忽略额外按钮、提供独立死区，并确保后续增量链接保留手柄崩溃修复。
- 导入现有2019资源的贴纸与手套目录；贴纸遵循原版槽位，可直接替换已装备位置，不提供贴纸预览；手套使用刀具式列表。优化外观属性查找、材质预热、附着实体复用、大厅负载和内存。
- 公告只在首次成功导入数据后显示10秒；保留已有玩家设置与库存。
- 加强 MetalFX 运动置信度、前一帧深度遮挡检查及大幅转向历史重置，保留 UI／持枪模型保护；受保护武器动画仍按真实帧率运行。
- 新增 **设置 → 视频设置 → 插帧性能测试**：三图快速／十二图完整测试，固定机位反复转向、开枪和5v5机器人场景，比较三档插帧细节，按设备保存推荐结果；手动应用，支持取消和异常中断恢复。画质／分辨率改变后须重测。
- Ubuntu 服务端同步后坐力、BOT接管、击杀标识和贴纸属性复制，保留协议2、消音器、密码校验及四图经典竞技轮换。

**验证范围：** Build200 已安装到 iPad，36个 ARM64/iOS 二进制签名检查通过；三图36组采样均为真实5v5，无 GPU 错误，推荐应用、取消和强制退出恢复通过。本机高画质保守推荐插帧细节50%，部分场景仍低于120帧；未执行十二图完整测试，本轮也未安装到 iPhone。详见[插帧验收记录](validation/frame-calibration-200.json)。

资源／着色器身份未改变，现有已验证数据仍兼容；重打包数据与 IPA 清单匹配。完整 IPA + Data 云流程仍未运行，Build149 云编译成功不能作为 Build200 的证明。Ubuntu 已通过全量编译／链接及17个 ELF64 x86_64 模块依赖检查，部署与多人对局尚未验收。

---

<a name="build149-english"></a>

# Build 149 · October 9, 2026

English | [简体中文](#build149-chinese)

This update brings the iPhone / iPad source snapshot from Build 86 to **0.7.2 (Build 149)**. Local and GitHub Actions builds use the updated native build graph: **3,848 tasks and 35 frameworks**.

- The initial screen can select a matching data ZIP from Files or iCloud Drive. It validates the package, preserves the selected ZIP, and can export import diagnostics before the game starts. File sharing remains available.
- Diagnostics export includes application and import failures. The installer reports missing files, incomplete ZIPs, content mismatches, storage errors and Metal library loading failures.
- The mobile interface now includes frosted menu surfaces, swipeable map selection and settings, a native console with keyboard input, room search, and an FPS toggle.
- Inventory uses category filters and long-press equipment selection. Shared guns use the same skin for both teams; exclusive guns respect their team. The knife category has separate police and terrorist columns, each with the full knife selection. Skin previews and the two Printstream finishes are included.
- Four graphics presets manage scene rendering. The medium preset uses the measured iPhone profile with aspect-ratio adaptation and approximately 1.7 million scene pixels. Map-material, shadow-submission, sampler, buffer-pool and shader/pipeline-cache changes are included. Weapon and skin lighting and texture detail remain independently selectable and default to the highest setting.
- MetalFX spatial upscaling and 60→120 frame interpolation remain available on supported devices. FPS, frame interpolation, motion blur and weapon quality are separate controls; frame rates depend on the device and scene.
- The app icon, loading progress view and startup announcement are updated. A fresh installation uses the player name `icsmer`, which can be changed with the system keyboard.

The minimum OS remains **iOS / iPadOS 27.0**, and the build requires the **iPhoneOS 27.x SDK**. This update does not add support for iOS 26. The original frozen data remain available via [iCloud](https://www.icloud.com/iclouddrive/039w_r3_mnpuCiw9Ecvmvr7zA).

The subsequent **GitHub-only data build** addition includes 7,906 actual MSL sources, resource catalogs and packaging scripts. Large resource inputs are uploaded to Releases; **Build iCSM IPA + Data** creates matched app/data exports. See the [guide](DATA_BUILD.md). No local build was run for this addition and the new full-data workflow has not been executed; the successful run below verifies only the existing app-only workflow.

Build instructions: [Mac](BUILD.en.md) · [Windows / GitHub Actions](CLOUD_BUILD.en.md).

[The complete Build 149 cloud build](https://github.com/zktfjcksxk-spec/iCSM/actions/runs/37881597437) passed. The downloaded IPA also passed independent integrity, dependency and signature verification; see the [validation record](validation/build-149.json). The cloud artifact requires personal re-signing and has not been device-tested.

---

<a name="build149-chinese"></a>

# Build 149 · 2026 年 10 月 9 日

[English](#build149-english) | 简体中文

本次将 iPhone / iPad 源码从 Build 86 同步至 **0.7.2（Build 149）**。本机及 GitHub Actions 使用更新后的原生构建清单：**3,848 项任务、35 个框架**。

- 初始页可从系统“文件”或 iCloud Drive 选择配套数据包 ZIP，校验内容并保留原 ZIP；游戏尚未启动时也能导出导入诊断。电脑文件共享导入继续可用。
- 日志导出覆盖应用与导入异常；导入器记录缺失文件、ZIP 未传完、内容不匹配、存储异常和 Metal 着色器库加载失败等原因。
- 移动界面加入毛玻璃菜单、可滑动选图及设置、支持系统键盘的原生控制台、搜索房间和 FPS 开关。
- 库存支持分类筛选和长按装备；双方共用枪械统一皮肤，专属枪械遵守阵营限制。刀具页左右分为警／匪，各自包含全部刀具；包含皮肤预览及两款印花集。
- 四档预设统一管理场景画质，中档采用 iPhone 实测方案，按屏幕比例适配约 170 万场景像素；同步地图材质、阴影提交、采样器、缓冲池和着色器／管线缓存优化。武器／皮肤光影与贴图仍可单独设置，默认最高。
- 支持设备继续提供 MetalFX 空间超分和 60→120 插帧。帧率、插帧、动态模糊、武器画质可独立选择，实际帧率取决于设备及场景。
- 更新应用图标、加载进度界面和启动公告。全新安装默认昵称为 `icsmer`，可通过系统键盘改名。

最低系统仍为 **iOS / iPadOS 27.0**，编译需要 **iPhoneOS 27.x SDK**；本次没有加入 iOS 26 支持。原冻结数据版本继续通过 [iCloud 链接](https://www.icloud.com/iclouddrive/039w_r3_mnpuCiw9Ecvmvr7zA)提供。

随后新增的 **GitHub 完整数据构建**包含 7,906 份真实 MSL、资源校验清单和打包脚本。大资源输入在 Releases，**Build iCSM IPA + Data** 用于生成配套 IPA 与数据包，见[指南](DATA_BUILD.md#简体中文)。该新增流程未执行本地构建，也尚未运行云端；下方成功记录仅证明原 IPA 工作流。

编译说明：[Mac](BUILD.md) · [Windows / GitHub Actions](CLOUD_BUILD.md)。

[Build 149 完整云编译](https://github.com/zktfjcksxk-spec/iCSM/actions/runs/37881597437)已通过，下载的 IPA 也通过独立完整性、依赖和签名检查，详见[验收记录](validation/build-149.json)。云端产物仍需个人签名，未进行设备测试。
