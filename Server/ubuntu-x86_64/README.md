<a name="english"></a>
# Ubuntu x86_64 dedicated server

English | [简体中文](#chinese)

Build 200 server source is in `Server/ubuntu-x86_64`, or download `iCSM-Server-Source.zip` from the release. It is a headless Linux server; no Metal or macOS game is included. The game/inventory protocol remains compatible with the mobile client. A server does not need frame interpolation, touch UI or MetalFX.

On Ubuntu 22.04 or later, Intel/AMD x86_64:

```sh
sudo apt-get update
sudo apt-get install -y clang lld cmake ninja-build python3 libssl-dev libcurl4-openssl-dev libbz2-dev zlib1g-dev
cd Server/ubuntu-x86_64
bash tools/build-server.sh
python3 tools/audit_elf.py
python3 tools/package_runtime.py --build build --data /path/to/iCSM-v1.zip --output /path/to/iCSM-Server
cd /path/to/iCSM-Server
```

For the standalone source ZIP, enter its `iCSM-Server-Source` folder instead. Edit `server-settings.json` and set your own password (at least 8 characters), then run `./check-server.sh`, `python3 run_server.py --check-config`, and `./start-server.sh`. CPU requires `cx16`. Allow the chosen UDP game port (default 27015) in your own firewall/security group. RCON is disabled by default. This package does not change your firewall.

Rotation remains **Mirage → Inferno → Dust II → Vertigo**, classic competitive, default ten players and 64 tick. Validated inventory messages retain password/challenge checks; no official matchmaking or Steam authentication is provided. Source synchronization adds recoil, bot handoff, correct kill icons and sticker attribute replication. GPU interpolation and glove viewmodel rendering are client features.

Do not replace an existing server directory in place: keep its password, settings and logs, build a new runtime, and migrate your own settings. No personal room password or client identity is shipped. Linux compilation checks and real Ubuntu deployment/multiplayer testing are distinct; see the release validation record.

---
<a name="chinese"></a>
# Ubuntu x86_64 服务端

[English](#english) | 简体中文

源码位于 `Server/ubuntu-x86_64`，发布附件也提供独立的 `iCSM-Server-Source.zip`。这是无图形界面的 Linux 服务端；不包含 macOS 游戏，不需要 Metal、超分或插帧。

在 Ubuntu22.04+ 的 Intel／AMD x86_64 服务器上执行上方命令。独立源码 ZIP 解压后进入 `iCSM-Server-Source` 目录，运行同样的构建与打包命令。`--data` 指向合并后的完整 `iCSM-v1.zip`，不能使用单个分卷。

生成运行目录后，在 `server-settings.json` 设置自己的至少8位密码，再运行 `./check-server.sh`、`python3 run_server.py --check-config` 和 `./start-server.sh`。CPU需要 `cx16`，自行允许防火墙／安全组的游戏UDP端口，默认27015；RCON默认关闭，脚本不会代改防火墙。

保留荒漠迷城→炼狱小镇→Dust II→殒命大厦的经典竞技轮换，默认10人、64tick。同步后坐力、BOT接管、击杀图标及贴纸属性复制，保留原挑战包与房间密码校验；不接入官方匹配。手套第一人称渲染和插帧属于客户端功能。

请在新目录生成运行包，再迁移原服务器的个人配置；不要直接覆盖旧目录的密码、配置和日志。源码编译验证与真实Ubuntu部署／多人联机验收是不同的项目，发布验收记录会分别注明。


Build200 validation: all Linux modules compiled/linked; 17 ELF64 x86_64 images and direct dependencies passed. Real Ubuntu deployment and multiplayer gameplay were not run. The runtime packager includes the same two-Printstream item schema as the client, so private inventory validation can recognize those paints.

Build200 已通过服务端全量编译与链接，17个 ELF64 x86_64 模块及直接依赖校验通过；未做真实 Ubuntu 部署和多人对局。本版运行包生成器同步客户端两款印花集物品定义，保留服务端皮肤校验。
