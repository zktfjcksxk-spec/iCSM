iCSM Ubuntu x86_64 私人服务端 — 四图竞技、逐玩家库存与公网直连版

目标：Linux x86_64，Ubuntu 22.04 或更新版本。
CPU需支持CMPXCHG16B（/proc/cpuinfo中的cx16）；虚拟机需要开放此CPU功能。
资源：项目现有 CSGO 2019 1.37.1.9 数据，以及当前采用的 2015 aim_botz。
服务端不使用 Metal，不需要图形界面或 GPU。

本版同步每个玩家在 iCSM 库存里选择的武器、刀具与皮肤，保留拾取武器
原持有者的皮肤，并恢复 USP-S / M4A1-S 的消音器装卸。请搭配已安装的
iCSM build 74 或更新客户端。相同游戏逻辑已在 Mac 服务端与手机/iPad
局域网对战中验证；本 Ubuntu 包已完成交叉编译、ELF 依赖与文件校验，
Ubuntu 上的实际地图运行、多人加入和稳定性仍需部署后验证。

1. 上传整个压缩包到 Ubuntu x86_64，解压：
   sudo apt-get update
   sudo apt-get install -y zstd python3 libstdc++6 libgcc-s1 libssl3 libcurl4 libbz2-1.0 zlib1g
   tar --zstd -xf iCSM-Server.tar.zst
   cd iCSM-Ubuntu-x86_64
   sha256sum -c SHA256SUMS

2. 编辑 server-settings.json：
   password：设置你的房间密码，至少8个字符；空密码会拒绝启动。
   bind：0.0.0.0 监听服务器IPv4接口；可改为具体IPv4地址。
   port：默认27015，UDP游戏端口。
   map：默认de_mirage；只能选择de_mirage、de_inferno、de_dust2、de_vertigo。
   game_type / game_mode：固定0/1（经典竞技），其他模式会拒绝启动。
   maxplayers：默认10。
   tickrate：默认64。

3. 检查与启动：
   ./check-server.sh
   python3 ./run_server.py --check-config
   ./start-server.sh
   控制台输入quit或按Ctrl+C退出。
   如需记录标准输出，可用 ./start-server.sh 2>&1 | tee logs/console.log
   库存诊断命令：icsm_net_inventory，可查看逐玩家已接收的库存与枪械状态。
   Ubuntu云防火墙/安全组需要允许所选UDP端口；本包不会替你修改防火墙。
   RCON默认关闭，没有开启管理端口。

   每场比赛结束后，按以下顺序自动换图并循环：
   荒漠迷城(de_mirage) → 炼狱小镇(de_inferno) → Dust II(de_dust2)
   → 殒命大厦(de_vertigo) → 荒漠迷城。
   保留原版经典竞技规则：30回合、半场换边，默认10人。
   改图投票和赛后选图已关闭，以保持顺序轮换；其他投票仍由原配置控制。

4. 配置与资源分开：
   csgo/cfg/icsm_server.cfg 是可编辑服务器配置。
   csgo/gamemodes_server.txt 定义四图专用地图组；csgo/cfg/icsm_rotation.cfg
   设置比赛结束后换图。csgo/mapcycle_icsm.txt 提供同顺序的备用轮换列表。
   csgo/cfg/icsm_password.cfg 在启动时根据JSON生成，请勿公开。
   content/csgo 为原始游戏素材挂载，启动脚本不下载或更新Steam数据。
   bin为重新编译的Linux ELF模块；没有macOS/iOS二进制。
   build-report.json 与 SHA256SUMS 记录构建与包文件校验信息。

源码认证策略使用原引擎NO_STEAM分支，保留挑战包、协议版本与房间密码
检查，不提供Steam登录、GC或VAC服务。本包不是官方服务端兼容性承诺。

公网直连修复：无 Steam 引擎将挑战包的大厅 ID 设置为 0（没有大厅），
避免旧客户端因 -1 预留值进入局域网白名单检查。保持当前无 Steam 配置，
不需要修改 sv_lan 或申请 Steam 服务器令牌。客户端先设置 password，再
用 connect 公网IPv4:端口 加入。详细更新方法见 public-connect-notes.txt。
此修复已通过编译、ELF 检查与原始客户端连接函数验证，远程实服仍需验收。
