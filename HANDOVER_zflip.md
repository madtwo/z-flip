# GravityShift v5 — 进度同步 / 交接文档（z-flip 项目）

> 更新时间：2026-09-05 (GMT+8)
> 状态：v5 已验收（§8-§10）；v6 六方向已同步本机并编译（§11；+Y 贴墙/G 两墙摆荡已 PIE 抽测通过，§11.9 全用例矩阵仍待实机）；**§12 = 导轨相机(防晕)+ 全表面操作映射 + Q/E 玩家调距,PIE 验证通过,待用户完整手感验收**
> **§13 = 2026-09-04 第七轮交付：障碍物物理砸碎修复(根因 tick 自检+PIE 15369J)+ 玩家球落地三带网格联动(≤4格安静/5-6格反弹/≥7格反重力,弹回4格)**,均 Live Coding+PIE 验证通过;实机手感 & 前台 7格边界验收待用户
> **§14 = 2026-09-04 第八轮交付：拾取物品 + 拾取钥匙开门(F 交互/拾取锁屏消息空格继续/门按 RequiredKeyID 配对/滑开动画/死亡重置回锁复位)**,UBT 编译通过 + PIE 全用例验收(18/18 断言 + 滑门开/关时序);详见 §14,剩一处已知滑门落座偏差见 §14.6
> **§15 = 2026-09-05 第九轮交付：§13/§14 拉取同步本机+重编 + 「积木式」对接文档(README 功能积木清单 / USAGE_WHITEBOX 文件存放规范+拾取钥匙门手册),手册摆法已实机 PIE 走通**;关卡策划对接入口 = README「功能积木清单」→ USAGE_WHITEBOX
> 写这份文档的目的：先把做到哪、卡在哪、改了什么、踩了什么雷同步清楚，供人工诊断。

---

## 0. 一句话现状

v5 双向 Z 重力滚球系统**已在 z-flip 编译、安装、搭建并 PIE 实测通过**：G 翻转(ACCEPTED/rev++)、球体升空、摄像机 180° slerp 跟转、棚顶撞击触发 FALL_THRESHOLD 自动反向、reset 语义正确、三方块按 Profile 就位。本轮修了 2 个真 bug（弹簧臂滤掉翻转、睡眠刚体不响应翻转）。**剩余：用户按 WASD+G+R 实机游玩验收（摇动力矩路径），以及 git 同步。**本轮详细记录见 §8。

**最新一轮（2026-09-03）速览**：同步队友 v6 六方向源码(§11) → 导轨相机防晕方案落地(§12,核心) → 天花板/墙面 A/D 映射修复 + 墙面爬降控制(§12.1) → Q/E 玩家调距、交互键 E→F。全部 PIE 验证通过,已推 GitHub。

---

## 1. 目标回顾（原始需求）

- 接手 UE5.8 无代码项目 GravityShift，按新交付的 **v5 双向 Z 重力滚动球**文档包（`D:\下载\GravityShift_ZFlip_RollingBall_DocumentPack_v5`）实现并实测。
- 核心规则：重力只有 `NEGATIVE_Z`/`POSITIVE_Z`；主角是真实物理滚动小球 Pawn；所有重力翻转走同一 `AGSGravityManager`；每次翻转摄像机平滑 180°(四元数 slerp)；球/碰撞/网格不因重力事件被人工旋转；白盒→绑定→PIE 验收流程。
- 原话：「请你按照原版的说明 加入到我昨天那个项目 测试案例里进行测试」。
- 原始环境：项目 `D:\UE\MyProject2`（英文路径，别动 `D:\UE\我的项目2`），引擎 `D:\UE_5.8`，MCP 端点 `http://127.0.0.1:8000/mcp`。

---

## 2. 已完成的步骤（时间线）

1. **读文档**：HANDOVER_GravityShift.md（§0/§10/§11/§12）、两个 SKILL（ue-nocode、ue-cpp-build-cnpath）、v5 文档包（AGENT_MASTER_PROMPT_v5 / API_QUICK_REFERENCE_v5 / ACCEPTANCE_MATRIX_v5.csv）。
2. **在 MyProject2 写完 v5 源码**：`Plugins/GravityShiftCore/Source/GravityShiftCore/Public|Private` 下 21 个类 + 7 个 Profile DataAsset。
3. **MyProject2 首次编译成功**：`UnrealEditor-GravityShiftCore.dll` 就位（64KB dll + 65MB pdb）。
4. **编辑器启动 + MCP 拉起**：`ModelContextProtocol.StartServer` 成功，端口 8000 监听；用 `check_v5_classes.py` 验证 **21/21 类 + 7/7 Profile 全部加载**，枚举正确。
5. **写了两个编辑器内 Python 脚本**：
   - `Plugins/GravityShiftCore/Content/Python/v5/install_blueprints.py`（安全幂等安装器）
   - `Plugins/GravityShiftCore/Content/Python/v5/generate_data_assets.py`（12 个 DataAsset）
6. **（踩雷后）pivot 到全新项目 `D:\UE\z-flip`**：放弃被 v2 残留 + 误建 fallback 蓝图污染的 MyProject2，从零搭干净项目。
7. **迁移 v5 源码到 z-flip**：`Plugins/重力翻转/Source/GravityShift/Public|Private`，21 个 .h + 21 个 .cpp 全部就位。
8. **修正 `BuildSettingsVersion`**：`V5` → `V7`（引擎 5.8 要求 V7，否则编译失败）。
9. **清除历史 memory**：删掉 `D:\UE\.workbuddy\memory/2026-09-01.md`。

---

## 3. 当前卡点（待诊断修复）

### 现象
`dotnet UBT.dll ZFlipEditor Win64 Development -Project=... -NoUBA` 编译失败。
- **游戏模块 `ZFlip` 本身已编过**：`Intermediate/.../Development/ZFlip/ZFlipModule.cpp.obj` 存在 ✅
- **插件 `GravityShift`（位于中文目录 `Plugins/重力翻转`）未编过**：`.../Development/GravityShift/GSBlockBase.cpp.obj` **缺失** ❌

### 铁证（.rsp 编码）
插件目录 `D:\UE\z-flip\Plugins\重力翻转\Intermediate\Build\...\GravityShift\GSBlockBase.cpp.obj.rsp` 字节：

```
6c 69 70 2f 50 6c 75 67 69 6e 73 2f e9 87 8d e5 8a 9b e7 bf bb e8 bd ac 2f 53 6f 75 72 63 65 ...
                                 ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
                                 UTF-8: 重力翻转
```

- UTF-8 解码：`.../Plugins/重力翻转/Source/GravityShift/Private/GSBlockBase.cpp`
- GBK 解码：`.../Plugins/閲嶅姏缈昏浆/Source/...` ← cl.exe 实际看到的，路径不存在 → `file not found`

### 根因
`.rsp` 响应文件由 UBT 按 **UTF-8（无 BOM）** 写出中文路径；MSVC 的 `cl.exe` 在中文 Windows 下按 **系统默认 GBK(CP936)** 读取响应文件 → 中文路径乱码 → 找不到源文件。
**路径里任何一层含中文都会进 .rsp**，所以即便项目根目录 `z-flip` 是英文，插件目录 `重力翻转` 这一层就足以让编译崩。

### 已排除的怀疑
- 不是 `BuildSettingsVersion`（已改成 V7）。
- 不是 `-NoUBA`（已加）。
- 不是 .NET 版本（引擎内置 dotnet 10 已用）。
- 不是项目根目录中文（`z-flip` 是英文，游戏模块 ZFlip 已成功编译）。

---

## 4. 踩过的地雷（完整清单）

| # | 地雷 | 表现 | 处置 |
|---|------|------|------|
| 1 | **没打开目标关卡** | 一直跑在 `/Temp/Untitled_1`，以为类加载了就万事大吉；用户指出「你根本没打开那个关卡」 | 用 `unreal.load_asset('/Game/测试案例').get_outer()` + `editor_load` 切到 `测试案例.umap` |
| 2 | **蓝图父类误判** | `install_blueprints.py` 用 `generated_class().get_super_class()`，Python 对象无此方法 → 异常被吞 → 14 个蓝图全走 fallback 误建垃圾 | 改用 `UBlueprint.get_blueprint_parent_class()` |
| 3 | **v2 残留 + fallback 污染难清理** | MyProject2 既有 v2 蓝图资产，又叠了我误建的 14 个 fallback，担心互相干扰 | 决定弃 MyProject2，新建 `D:\UE\z-flip` 干净项目 |
| 4 | **BuildSettingsVersion 过旧** | `V5` 在引擎 5.8 编译失败 | 改为 `V7` |
| 5 | **中文插件目录编码**（当前） | `.rsp` UTF-8 中文被 cl.exe 当 GBK → 找不到文件 | **待修：路径改 ASCII**（见 §6） |
| — | 历史已规避的雷（本会话未再踩） | 中文路径编译崩、UBA Access denied 需 `-NoUBA`、.NET 10 依赖、复制项目后 MCP 不自动起、蓝图类用 `load_asset().generated_class()` 而非 `load_class` | 均按 SKILL 预处理 |

---

## 5. 已做的修改（文件级）

### 新建（z-flip 项目）
- `D:\UE\z-flip\z-flip.uproject`（含插件 `重力翻转` 启用）
- `D:\UE\z-flip\Source\ZFlip.Target.cs` / `ZFlipEditor.Target.cs`（`BuildSettingsVersion = V7`）
- `D:\UE\z-flip\Source\ZFlip\ZFlip.Build.cs` / `ZFlipModule.cpp`（游戏模块，仅挂插件）
- `D:\UE\z-flip\Config\` 全套 ini（含 MCP 自动启动 `ModelContextProtocol.StartServer`、远程 Python、`-game` 端口）
- `D:\UE\z-flip\Plugins\重力翻转\重力翻转.uplugin`
- `D:\UE\z-flip\Plugins\重力翻转\Source\GravityShift\GravityShift.Build.cs`
- `D:\UE\z-flip\Plugins\重力翻转\Source\GravityShift\Public\*.h`（21 个）
- `D:\UE\z-flip\Plugins\重力翻转\Source\GravityShift\Private\*.cpp`（21 个，含 `GravityShift.cpp` 模块实现）

### 模块改名
- 原 `GravityShiftCore`（MyProject2 用）→ 新项目改为 `GravityShift` → 最终游戏模块/项目用 `ZFlip`，插件模块用 `GravityShift`。
- 源码里 `GRAVITYSHIFTCORE_API` 宏全部替换为 `GRAVITYSHIFT_API`（Python 批量替换，已验证 0 处残留）。

### 删除
- `D:\UE\.workbuddy\memory\2026-09-01.md`（按用户要求清除历史记忆）

---

## 6. 建议修复方向（供诊断，未擅自执行）

**SKILL `ue-cpp-build-cnpath` 第 128 行原文**：「路径含中文 → 复制到英文路径，**别想着改系统代码页**」。

据此，推荐方案按优先级：

- **方案 A（最稳，推荐）**：把插件目录 `重力翻转` 改名为 ASCII，例如 `GravityShift` 或 `ZFlipCore`。
  - 改两处：物理目录名 + `z-flip.uproject` 里 `"Name": "重力翻转"` → `"Name": "GravityShift"` + `.uplugin` 文件名。
  - 重编即可，无需动代码页。
- **方案 B（保留中文名，折中）**：保留 `重力翻转` 目录，但在英文路径建一个 **junction/symlink**（如 `D:\UE\z-flip\Plugins\GravityShift -> 重力翻转`），让 UBT 实际读英文路径。
  - 风险：UBT 仍可能从 .uplugin 绝对路径推导中文路径写进 .rsp，不一定彻底。
- **方案 C（不推荐）**：改系统/进程代码页为 UTF-8（Beta 版）。SKILL 明令禁止，且影响面大。

> 我的判断：直接走 **方案 A**，把插件目录改名 ASCII。这是 SKILL 的标准解法，改动最小、风险最低。

---

## 7. 下一步（待诊断确认后）

1. 确认修复方案（预期方案 A：插件目录改 ASCII）。
2. 改名后重编 `ZFlipEditor`，确认 `GravityShift` 插件 21 个 .cpp 全部出 obj、出 `UnrealEditor-GravityShift.dll`。
3. 启动编辑器 + 拉起 MCP，验证 21/21 类加载。
4. 切到目标关卡 `测试案例.umap`（这次**先确认关卡再干活**）。
5. 跑 `install_blueprints.py` + `generate_data_assets.py`（修好父类判定后），清理任何残留。
6. 搭 v5 测试案例（Manager + Ball + 三方块 + 表面 + 开关 + 破坏块 + KillVolume），写 `acceptance_pie.py` 跑 PIE 验收矩阵（G 翻转 / 摄像机 180° / 自动反重力 / 落地弹跳 / 破坏+重置）。
7. 验收通过后更新交接文档 + git 推送（如需要）。

---

## 附：关键路径速查

- 项目：`D:\UE\z-flip`
- 插件源码：`D:\UE\z-flip\Plugins\GravityShift\Source\GravityShift\{Public,Private}`
- 插件编译中间（含 .rsp 铁证）：`D:\UE\z-flip\Plugins\GravityShift\Intermediate\Build\Win64\x64\UnrealEditor\Development\GravityShift\`
- 工具链：`D:\UE_5.8\Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe` + `D:\UE_5.8\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll`
- 编译命令：`dotnet UBT.dll ZFlipEditor Win64 Development -Project="D:/UE/z-flip/z-flip.uproject" -NoUBA`
- MCP 辅助：`C:\Users\20625\.zcode\skills\ue-nocode\reference\{ue.py, ue_pyexec.py}`
- 原始 v5 文档包：`D:\下载\GravityShift_ZFlip_RollingBall_DocumentPack_v5`
- v6_RC1 参考包（GPT 写的完整运行时包，未编译过，仅作参照）：`D:\下载\GravityShift_UE582_RuntimePack_v6_RC1`
- MyProject2 仍保留一份 v5 源码（15 个 .h），可作对照。

---

## 8. 2026-09-02 第四轮接手记录（编译确认 → 验收通过 ✅）

**接手时真相核查**：上一轮日志说"编译卡死等放行"，实际**改名 ASCII 后已编译通过**（dll 时间戳新于源码），文档没回头更新。

**本轮完成**：
1. 从 MyProject2 搬迁测试关卡依赖：`测试案例.umap` + `__ExternalActors__` + `__ExternalObjects__` + `_GENERATED`(棚子网格) + `LevelPrototyping` + `ThirdPerson`(后已删除,见坑 3)。关卡在 z-flip 加载成功。
2. 移植 v5 安装脚本到 `Plugins/GravityShift/Content/Python/v5/`（脚本本身无模块名字面量,无需改）:install_blueprints 创建 14 BP、generate_data_assets 创建 12 DA,**0 fallback 0 失败**。
3. DefaultEngine.ini 启动地图从不存在的 `/Game/Maps/Main` 改为 `/Game/测试案例`。
4. 摆放 v5 测试场景（全部原生类直接 spawn,存盘）：3×GSBlockBase(Fixed/Gravity/Breaker 带 Profile)、GSGravitySwitch、慢速 SurfaceModifierVolume、上下两个 GSKillVolume。Manager/WSM 由 GSGravityGameMode 在 BeginPlay 自动拉起,**不用手摆**。
5. **修 2 个真 C++ bug（已重编译）**：
   - `GSRollingBallPawn` 摄像机不跟转：`CameraArm->bInheritPitch/Yaw=false` 把 CameraPivot 上的 180° 翻转旋转**过滤掉了**。修:`bInheritPitch/Yaw/Roll=true` + `CameraPivot->SetUsingAbsoluteRotation(true)`(顺带消除球滚动渗入摄像机)。
   - 睡眠刚体对翻转无响应：`UGSGravityBodyComponent` 的 `AddForce` 对睡眠刚体无效。修:组件订阅 `OnGravityChanged` 广播,翻转时 `WakeRigidBody()`(BeginPlay AddUniqueDynamic / EndPlay Remove)。
6. **API 级 PIE 验收全过**：toggle ACCEPTED→G -Z→+Z→球升空(未手动 wake!广播唤醒生效)→`CAM_UP` 翻转 slerp→撞棚顶→**REV++ REASON=FALL_THRESHOLD 自动反向**(设计行为,非 bug)→落回 49.5→`reset_gravity(True)` 语义正确(已处默认态返回 NO_CHANGE)。枚举只暴露 NEGATIVE_Z/POSITIVE_Z。三方块按 Profile 落位。
7. ThirdPerson 残留链已根除(见坑 3)。

**本轮新踩的雷(已反哺 ue-nocode SKILL)**：
1. **PIE 启动撞蓝图编译错误会弹模态框**"是否在编辑器中播放?"——游戏线程等确认,远程探测全部超时,**极像编辑器死机**(曾误杀两个实例)。UE 对话框是 Slate 自绘,无 Win32 Button 子控件,BM_CLICK 无效;正确姿势=按 CUA 窗口光栅找按钮坐标,物理坐标=逻辑坐标×缩放(本机 150%),`PostMessage WM_LBUTTONDOWN/UP` 带 client 换算可直接点(不动真实光标,不抢前台)。根治=铲引用链。
2. **远程 python 执行期间 PIE 世界暂停**(游戏时钟冻结,AddForce/摄像机 slerp 全停),脚本之间才恢复。**测动态行为必须"发射后立即退出脚本、隔几秒再开脚本读结果"**——在脚本内 sleep 观测会全部读到冻结值,极像 bug(本轮差点据此误判)+错误修复方向。
3. **模板图残留会拖编译链**：拷来的 `Content/ThirdPerson`(Lvl_ThirdPerson.umap+BP 壳)在 PIE 启动时拉起 BP_ThirdPersonCharacter 编译失败(z-flip 无 Input/Characters 资产)→编译失败弹窗→死等。WorldSettings 的 `DefaultGameMode=BP_ThirdPersonGameMode` 覆盖也藏在 umap 里(python 清空后存盘)。**最终整个 ThirdPerson 文件夹删除,测试关卡不引用它**。
4. bash 给 python -c 传中文路径/字符串会被编码层搅乱;`grep` 模式以 `/` 开头会被 MSYS 当路径转换。中文标题的窗口枚举脚本用 python 写 UTF-8-BOM 的 .ps1 文件再执行。
5. 远程执行通道(组播)偶发连不上:等几秒重试即可;真超时优先查是不是有模态框。

**验收留待用户**:PIE 里实际按 **WASD 滚球( torque 路径)、G 翻转、E 开关、R 重置**——OS 级按键注入会干扰真人操作,未代测。编辑器窗口在后台时 UE 会深度节流,游玩时把编辑器切到前台。

**git**:仓库已建 **https://github.com/madtwo/z-flip**(私有,2026-09-02 晚 REST 直推 122 文件,commit cdd172c)。仓库内含:`README.md`(使用+测试状态)/`USAGE_WHITEBOX.md`(白盒装配)/`AGENT_GUIDE.md`(AI 接手)/`AgentSkill/`(ue-nocode + ue-cpp-build-cnpath 两套 skill 随仓带走)。克隆后右键 uproject 重编译即可玩;推送用 `AgentSkill/ue-nocode/reference/push_via_api.py "madtwo/z-flip" "D:/UE/z-flip"`。开代理后一次 `git fetch origin && git reset --hard origin/main` 对齐历史(REST 引导提交导致 sha 与本地不同,内容一致)。

> 2026-09-02 追加:`.gitignore` 已添加忽略 `.claude/skills/`(两套 skill 的本地实时副本所在,不进 GitHub)。

---

## 9. 2026-09-02 第五轮:手感三项修复(用户试玩反馈 ✅ API 级验收)

用户实机试玩后报了三个问题,全部修复并重编译验证:

1. **WASD 镜像(W 后退、A 向右)**:根因是滚动力学——`ApplyMovement` 力矩轴写成 `Desired×Up`,而接触点运动学给出 `v_center = -wr·(Up×T)`,该轴让球**滚向 Desired 反方向**。修:改为 `Up×Desired`(两轴一起翻正)。松键无制动一并修:新增 `StopTorqueAcceleration`(默认 60,EditAnywhere),无输入且支撑时施加反向力矩,约 0.5s 停稳。
2. **落地判定重做(按用户规则)**:落地法向速度 X 三分区——`X≥V_HIGH(1400)` → 反重力翻转(摄像机跟随,操作相机相对天然不镜像);`V_LOW(150) < X < V_HIGH` → **弹跳且每周期只弹一次**(新增 `bBouncedSinceQuietLanding` 锁存,安静落地复位——原实现固定 250 弹速每次落地重新弹,必然无限弹,用户预言准确);`X≤V_LOW` → 无反应。**半空反飞已禁用**(AutoReverseMode 三层默认值全改 LANDING_IMPACT:组件/GSBallProfile(GSProfiles.h)/DA——只按落地速度判定,这是用户模型)。反重力只有球能触发(LandingResponse 组件只在球上)。
3. **球没拿 Profile 的隐藏 bug**:GameMode 原生 spawn 的 pawn `BallProfile=null`,V_HIGH 实际跑组件默认 900 而非 DA 的 1400 → 棚顶高度落差(≈1050)全部误翻转+上下乒乓。修:Pawn 构造器 `ConstructorHelpers` 自动加载 `DA_GS_Ball_Default`。

**验收证据(PIE fire-and-read)**:W 速度 (138.9,0) 沿相机前向(FORWARD_OK)、A right_dot=-138.9(LEFT_OK)、松键 162.9→12.5(BRAKE_OK);370 落差=单次弹跳无翻转(rev 不变、沉降 49.5);V_HIGH 暂调 900 后同落差 → reason=LANDING_RESPONSE、G 翻 +Z、cam_up 翻转(-0.21,0,-0.98);恢复 1400 + reset ACCEPTED 回 -Z。

**调试教训**:脚本 `SetMoveInput` 会被 `PollNativeInput` 每 tick 用键盘实况覆盖——脚本验证方向前必须 `set_editor_property('enable_native_polling_input', False)`;`move_input` 字段不反射,读不了只能设。

**阈值速记**:V_HIGH=2000(DA `landing_auto_reverse_at_speed_cm`,反重力线,**2026-09-02 晚从 1400 上调**——阈值必须高于房间内最大落体冲击 sqrt(2·1600·850)≈1650,否则手动翻转被球的落地反翻打回、上下乒乓、方块贴不住天花板)、V_LOW=150(无反应线)、弹速=250、制动=60。要改手感改 DA_GS_Ball_Default/DA_GS_Landing_Normal + GSProfiles 默认值。

## 10. 2026-09-02 晚:方块不跟 G 翻转修复(✅)

- **根因(时序家族第三例)**:关卡摆放的方块 BeginPlay 早于 GameMode 自动 spawn 的 Manager → `FindGravityManager` 为空 → 注册/订阅全跳过且无重试 → 方块永远不响应翻转(`GetGravityDirection()` 空管理器恒 -Z)。**诊断信号:`get_registered_body_count()` 只有 1(球)= 关卡 Actor 全没注册**。
- **修复(懒绑定)**:注册+订阅从 BeginPlay 挪进 `RefreshReferences`(RegisterGravityBody 自带去重、AddUniqueDynamic 幂等),`TickComponent` 里 manager 为空就重试。修后 REG=3(球+重力块+破坏块)。
- **实测**:G 翻转 → 重力/破坏方块**纯 z 向直上**(无翻滚)贴住棚顶 850 且速度归零、翻转粘滞(rev 不变);静态方块(sim=False)不动;reset 后回落 47/50/51。配合 V_HIGH 上调 2000(见 §9 速记),球的落地反翻不再打断贴顶状态。
- 方块**没有**落地速度弹跳/反重力机制(设计如此,仅球有)——用户明确确认。

---

## 11. 2026-09-03 第六轮:六方向重力(双向 Z → ±X/±Y/±Z)+ 关卡重力配置 + 方块网格吸附(源码落地,**C++ 编译通过**)

> 状态:本轮为**规格实现 + 源码落地**,按规格 §1-§8 逐步完成;**已用引擎 UBT 编译通过**(`D:\Epic Games\UE_5.8` 内置 dotnet 直调 UBT,ZFlipEditor Win64 Development -NoUBA,62s 成功,`UnrealEditor-GravityShift.dll` 链接就位)。剩 PIE 实测验收。规格与实现的两处偏差见 §11.6(均经用户确认)。

### 11.1 需求回顾(规格要点)

把"双向 Z 重力"升级为**六方向**(重力"向下"可为 ±X/±Y/±Z 任一方向):1/2/3 键把重力吸到 X/Y/Z 轴正方向,G 在当前轴上 ± 翻转;每关可通过 WorldStateManager 指定默认重力方向 + 允许轴(空=全允许);HUD 显示当前方向/允许轴/不可用提示;块(仅块,不含球)支持网格吸附。

### 11.2 改动文件清单

**改(Public|Private 成对)**
- `GravityShiftTypes.h`:新增 `EGSGravityDirection`(±X/±Y/±Z)、`EGSGravityAxis`(X/Y/Z);`EGSGravityRequestResult` 增 `REJECTED_DISABLED`;`GSGravity` 命名空间新增 8 个工具(`DirectionToVector/DirectionToUp/VectorToDirection/FlipDirection/GetAxisFromDirection/IsPositive/GetPositiveDirection/GetNegativeDirection` + 显示名 `GetDirectionDisplayName`/`GetAxisDisplayName` + `IsAxisAllowed`)。
- `GSGravityManager.h/.cpp`:六方向权威(`CurrentDirection/DefaultDirection/AllowedAxes Transient`),新 5 参委托 `OnGravityDirectionChanged`(NewDirection/DirectionVector/Revision/Reason/Requester);请求入口 `RequestGravityDirection/SetGravityAxis/ToggleCurrentAxis/SetAllowedAxes/IsDirectionAllowed`;**保留并继续广播旧 4 参 `OnGravityChanged`**(GravityBody 唤醒、Pawn 摄像机、老 BP 不动);冷却手动 0.25s/自动 0.75s;`ResetGravity` 回 `DefaultDirection`。
- `GSWorldState.h/.cpp`:关卡配置区 `Gravity|LevelConfig`(`DefaultGravityDirection`=NEGATIVE_Z、`AllowedGravityAxes` 空=全轴、`GetAllowedAxes/IsDirectionAllowed/ApplyLevelGravityConfig`);BeginPlay + GameMode 同步调用;默认方向不在允许轴时 Warning + 回退第一个允许轴正方向;配置带 0.1s settle 重推防 BeginPlay 乱序(见 §11.7)。`ResetWorld` 末尾对所有 `AGSBlockBase` 重新 `ApplySnap`。
- `GSRollingBallPawn.h/.cpp`:轮询新增 `AxisSetXKey/YKey/ZKey`(默认 1/2/3)边沿检测 → `HandleSetGravityAxis`(走 `Manager->SetGravityAxis`);`RequestGravityDirection/GetCurrentGravityDirection`;不可用键 → `ShowAxisDisabledHint`(HUD 提示 "X轴不可用" ~1s);G 键经旧入口 `RequestToggleGravity` → 新 `ToggleCurrentAxis` 自动在当前轴翻转,无需改。
- `GSBlockBase.h/.cpp`:ctor 建 `UGSGridSnapComponent`,默认 `SetSnapEnabled(false)`,`ApplyBlockProfile` 按 `UGSBlockProfile.bSnapToGrid` 启停。
- `GSProfiles.h`:`UGSBlockProfile.bSnapToGrid`(**默认 false**,与规格字面 true 不同,见 §11.6)。
- `GSFramework.cpp`:GameMode `HandleStartingNewPlayer` 重推关卡配置;HUD `DrawHUD` 显示当前方向/允许轴列表/不可用提示,控件行加 1/2/3。

**新增**
- `GSGridSnapComponent.h/.cpp`(Task 5):网格吸附组件。`bSnapEnabled/GridSize/SnapToGrid/ApplySnap/SetSnapEnabled`;BeginPlay + Tick(TG_PostPhysics)吸附;订阅新委托,翻转时禁吸附并按 `SnapRestoreDelaySeconds`(默认 0.35)定时恢复;速度闸门 `IsSafeToSnap`(刚体静止或速度 ≤ `SnapMaxMoveSpeedCm`)避免与物理掐架;用 `TeleportPhysics` 遥放置。

### 11.3 行为契约(实现定案)

- **GravityManager 是唯一重力权威**,六方向全走它;所有消费方继续用 `GetGravityDirection()` FVector,方向无关(落地投影早已沿重力轴 DotProduct,见 §11.5),多数物理代码零改动。
- **每关默认由 WSM 覆盖 Manager**:`ApplyLevelGravityConfig` 把 `AllowedGravityAxes` 推给 `Manager->SetAllowedAxes`,把 `DefaultGravityDirection`(回退后)写进 `Manager->DefaultDirection` 并 `RequestGravityDirection(force=true)`,使 R 重置/WSM ResetWorld 都能回到关卡默认方向。GravityProfile 里旧 `DefaultPolarity` 只表达 ±Z,仅在关卡没配(空 WSM 默认)时生效。
- **翻转只切方向不动姿态**:球 Actor/碰撞/网格仍不被人为旋转,摄像机四元数 slerp 到 `Up=-gravity`,方块靠重力体纯直线贴面。
- **旧 API 全兼容**:`RequestGravityPolarity`=请求 ±Z 方向、`RequestToggleGravity`=翻转当前轴,LandingResponse/GravitySwitch/老蓝图不改仍通。

### 11.4 规格逐条核对(§1-§7)

| 规格节 | 交付 |
|---|---|
| §1.1 类型+工具 | ✅ 见 §11.2 |
| §1.2 WSM 关卡配置 | ✅ 默认方向 + 允许轴 + 回退 + Warning + BeginPlay 调用 |
| §1.3 Manager 六方向 | ✅ 新方向/轴状态 + 新 5 参委托 + 新请求入口 + 双委托广播 + 冷却 |
| §1.4 Pawn 输入/摄像机 | ✅ 1/2/3 吸轴、G 翻轴、slerp 到新 Up |
| §1.5 LandingResponse 投影 | ✅ 已方向无关(`DotProduct(Velocity,Dir)`,无需改动,见 §11.5) |
| §2 块网格吸附 | ✅ 组件 + BlockBase 集成 + Profile 开关 + 翻转后 0.35s 恢复 + ResetWorld 重吸 |
| §3 HUD | ✅ 当前方向/允许轴/不可用提示 |
| §4 输入映射 | ⚠ 用项目轮询约定代替 BindAction+DefaultInput.ini(见 §11.6) |
| §5-§7 设计器流程/开发序/集成矩阵 | ✅ 见 §11.8 |

### 11.5 LandingResponse 免改依据

落地组件早已把"下落速度/距离"沿 `GetGravityDirection()` 投影(速度沿重力轴的 DotProduct 累距、法向冲击 `DotProduct(Normal,-Dir)`、反向保留只取沿 Dir 法向分量),六方向下语义不变;自动反向走 `RequestToggleGravity` → 现在翻转当前轴,方向无关。故 §1.5 无源码改动。

### 11.6 与规格字面的两处偏差(用户已拍板,交接备忘)

1. **`UGSBlockProfile.bSnapToGrid` 默认 false**(规格字面 `=true`):用户选"默认 false(推荐)"。现有 `测试案例.umap` 几何非 100 网格对齐(天花板 850、块静置高度 ~47-51),默认开会把旧关卡块吸偏;网格关卡在各 BlockProfile 上按需勾选。
2. **输入走项目轮询约定**(规格要求 BindAction + DefaultInput.ini 注册 1/2/3/G):用户选"沿用项目轮询(推荐)"。原因:仓库已记录"关卡摆放实例的 BindAction 会丢 InputComponent"(本插件踩过的雷),轮询 `IsInputKeyDown` 免疫输入栈时序。实现=FKey 属性(默认 One/Two/Three/G)+ `PollNativeInput` 边沿检测,未建 DefaultInput.ini。

### 11.7 时序处理(本规格新增防雷)

Manager 由 GameMode BeginPlay 自动 spawn,BeginPlay **可能延迟到下一 tick**,其 `ApplyGravityProfile` 会把方向重置回 Profile 的 Z 默认,从而盖掉已推的关卡配置。定案三层兜底:WSM BeginPlay 推一次 + GameMode `HandleStartingNewPlayer`(Pawn 就位后,保证晚于一切 BeginPlay)重推 + 推成功后再挂 0.1s settle 单次重推;`ApplyLevelGravityConfig` 去掉一次性闸,天然幂等(同方向 `RequestGravityDirection` 返回 NO_CHANGE 零成本)。已用"块不同步翻转"同款诊断思路:注册数对不上先查 Manager 时序。

### 11.8 设计器工作流(§5)

新关卡/新几何:① 决定关卡默认重力方向(通常为角色初始脚下方向)与允许轴 → ② 找场景里 WSM,Details `Gravity|LevelConfig` 填 `DefaultGravityDirection` + `AllowedGravityAxes`(空=全轴)→ ③ 决定哪些块需网格对齐:勾它们的 `UGSBlockProfile.bSnapToGrid`,并保证摆放/几何本身在 100cm 网格上 → ④ PIE 用 1/2/3/G 验证。
开发序(规格 §6)已遵守:类型/枚举 → Manager → WSM 配置 → Pawn/摄像机/网格 → HUD。集成矩阵:方向无关核心(GravityBody/LandingResponse/SurfaceReceiver/Breakable/Resettable)零改动;GravitySwitch 的 FORCE ±Z 语义保留,TOGGLE 翻当前轴。

### 11.9 测试用例(待 PIE 实跑;编译已过)

| # | 场景 | 期望 |
|---|---|---|
| 1 | Z-only 关卡(Allowed 只留 Z)按 1/2 | `REJECTED_DISABLED`,方向不变,HUD 弹 "X/Y轴不可用" ~1s 后消失;按 3 → +Z |
| 2 | 关卡默认 +X(WSM `DefaultGravityDirection=POSITIVE_X`) | 出生即 +X(HUD "Gravity: +X"),球自然落向 +X 墙面 |
| 3 | 翻转后按 R / 踩 KillVolume | `ResetGravity` 回关卡默认方向(如 +X),球回 checkpoint/出生位置(方向+位置都复位) |
| 4 | 勾了 bSnapToGrid 的块,在网格对齐面上翻转 G | 翻转稳定后(0.35s 恢复+速度闸门)块中心回到整数网格 |
| 5 | 空 Allowed(全轴)按 1/2/3/G | 六方向自由;G 在 +X↔-X、+Y↔-Y、+Z↔-Z 当前轴上翻 |
| 6 | 默认方向 = 禁止轴(如 Allowed=Z-only 但默认 +X) | WSM Warning 日志,回退 +Z;球出生 +Z |
| 7 | 旧行为回归 | 原 Z 双向关卡无 bSnapToGrid 块:几何不漂移;G + 落地反翻 + reset 语义同 v5 |
| 8 | LandingResponse 自动反向(任意轴) | 六方向下仍沿"当前重力轴"反向(方向无关,改方向后首测) |

### 11.10 已知限制(如实声明)

- **网格吸附只适用于网格对齐几何**:组件把 Actor 位置逐分量 round 到 `GridSize` 网格点;非网格对齐关卡勾了 `bSnapToGrid` 会把块吸偏(故默认关)。块尺寸/半偏移不在网格上时需手动调设计。
- **吸附是"中心对齐"**,不做体素式重叠修正;速度闸门(`SnapMaxMoveSpeedCm`)下高速落体先不吸,稳定后由 tick 吸上,空中翻转瞬间可能有一两帧不吸(可接受)。
- **旧 `EGSGravityPolarity` 仅对 Z 轴有意义**:X/Y 方向下镜像的 `CurrentPolarity`/`DefaultPolarity` 只是正负投影,旧 API/BP 若把"极性"当真会误导;新代码一律用方向。
- **六方向下角色可"站墙/站天花板"**:贴地移动/摇动力矩/摄像机全方向无关,但**尚未实机手感验收**(尤其墙/顶平移与空中转向),与 v5 一样留用户试玩。
- **编译已过但未 PIE 实测**:`AllowedAxes` 语义/冷却/回退/网格吸附的数值与手感细调,留 PIE 按 §11.9 用例验收。

---

## 12. 2026-09-03 晚:导轨相机(防晕新方案,源码落地 + PIE 核心验证通过)

> 背景用户反馈:翻转相机转来转去晕 3D。新方案 = **铁路拍摄机**:相机像小环套在导轨上滑行,横切面位置锁死,视角只做小幅万向调整。测试案例已接好第一根轨并 PIE 验证通过。

### 12.1 设计定案

- **位置**:相机沿 `AGSCameraRail` 本地 Z 轴滑动(目标 = 球沿轴投影 + `LookAheadSeconds`·沿轴速度 − `TrailDistanceCm`·视线方向符号,钳在轨长内;Trail 让球在前相机在后,用户要求"相机相对小球在后面一点");横切面内偏移 = `CrossOffsetHeightCm`(默认 **0 = 正骑在轴心**,>0 沿世界竖直抬升,轴竖直时回退轨本地 X),**全程固定**。
- **朝向**:滚转基准是**世界竖直(不随重力)**——重力翻转只改变球的去向,相机只通过"视线追球"自然调整俯仰/偏航;万向限位 `MaxYawDegrees=35 / MaxPitchDegrees=50`(俯仰被夹住时球仍在 FOV 内,处于屏幕舒适区)。目标视线 = 球位置 + `AimOffsetUpCm=40`(世界竖直抬高,让球落在画面下部)。
- **选轨**:组件每帧扫 `AGSCameraRail`,球在轨长 ±`RailEndMarginCm(100)` 内才算候选,取离轴最近;**活动轨迟滞 ±`RailSwitchMarginCm(150)`** 防相邻轨抖动;全部出圈 → 回落旧跟随相机(BeginPlay 初始化照旧,无缝兼容)。
- **平滑**:位置/姿态双指数平滑(k=8/6);**dt 钳制 [0, 0.1]**(PIE 暂停恢复会给出负 dt,负 α 会把相机外推离目标——PIE 验证时实际踩到,已修)。
- **玩家自调跟距(2026-09-03 追加)**:`TrailDistanceCm` 默认 **700**(用户拍板),**Q/E** 每按一次 ∓`TrailAdjustStepCm=50`,钳在 **[300,1400]**(组件 `AdjustTrailDistance`,上下限也是用户拍的板);**交互键因此从 E 挪到 F**(HUD/README 同步)。PIE 里调的值 Stop 即还原,永久改默认值改 C++ 头文件或告诉我。
- **移动基向量用相机自身框架(2026-09-03 追加)**:ApplyMovement 的 `Right` 从 `Up×Forward` 改成**相机 right 投影到支撑面**——轨相机滚转锁世界竖直,Up 一翻 `Up×Forward` 就反向,天花板/墙面上 A/D 会镜像(用户实测反馈);相机 right 始终等于屏幕右,任何表面都对。旧滚转跟随相机(无轨关卡)不受影响:它 roll 跟随 Up,两种算法结果本就相同。PIE 实测:地面 dot(CamRight)=+86,天花板 +86(修复前天花板为负)。
- **墙面专属控制(2026-09-03 用户规格)**:墙面(Up 水平,`|Up.Z|<0.5` 判定)上 **W/S = 沿墙水平前进/后退**(相机前向去掉 Z 分量),**A/D = 爬升/降落**且方向取决于墙在屏幕哪一侧:重力方向点相机 right ≥0(右墙)→ D 爬 A 降;<0(左墙)→ A 爬 D 降。实现:墙分支 `Right=(0,0,SideSign)`、`Forward=水平化(相机前向)`。PIE 实测:左墙 A → vz=+86.7 爬升 ✓,G 翻到右墙 D → vz=+60.7 爬升 ✓,S 沿墙 −X 后滚 ✓。限制:走廊端墙(重力 ±X,相机正对墙面)时 W/S 无意义(基向量退化,回退通用投影);球在地板-墙角落的过渡瞬间双接触动力学较乱,实机手感待用户验。
- **鼠标视角**:轨模式下忽略(否则积攒的偏航会在换轨瞬间突然生效);无轨关卡照旧。
- **操作基准**:WASD 前向 = 相机前向投影,pawn 原逻辑零改动——W 自然 = 沿轨前进。

### 12.2 改动文件

- 新增 `Public/GSCameraRail.h` / `Private/GSCameraRail.cpp`:导轨 Actor(纯数据标记,无碰撞无渲染;`RailLength`/`bLookAlongNegativeAxis`/`CrossOffsetHeightCm` + 轴投影/距离/范围查询 BlueprintPure)。
- 新增 `Public/GSRailCameraComponent.h` / `Private/GSRailCameraComponent.cpp`:轨相机组件(懒扫描/选轨/万向数学/平滑;`IsDriving/GetActiveRail/GetNumRails` 可查)。
- `GSRollingBallPawn.h/.cpp`:ctor 挂组件;`UpdateCamera` 头部插入轨分支(激活时 `TargetArmLength=0`+关臂碰撞探测,回落时还原);`PollNativeInput` 轨模式下吞掉鼠标增量;新增 Q/E 调距键与交互键 E→F;`ApplyMovement` 相机系基向量 + 墙面专属控制分支(见 §12.1)。
- `GSFramework.cpp`:HUD 控制提示行更新为 `WASD roll | G flip | 1/2/3 set X/Y/Z | Q/E camera dist | F interact | R reset`。

### 12.3 PIE 验证记录(测试案例,`相机导轨_GS`)

| # | 场景 | 期望 | 实测 |
|---|---|---|---|
| 1 | PIE 启动(球在轨范围内) | 轨相机接管 | `IsDriving=True`,rails=1 ✅ |
| 2 | 球静止在 (0,2640,49.5) | 相机锁轴 | CAM=(0,2750,450) 与轴线偏差 dY=0.0 dZ=0.0 ✅ |
| 3 | 球瞬移 X−1500 | 相机沿轴滑 | CAM=(−1500,2750,450),dY=0.0 dZ=−0.0 ✅ |
| 4 | G 翻转、球贴顶 850 | **不滚转**、俯仰转上看 | pitch −50→**+50**,yaw −145 不变,**roll 恒 0**,相机仍在轴上 ✅ |
| 5 | 俯仰限位 | 大落差球不出画 | −50/+50 夹持生效,球在画面下部舒适区 ✅ |

待用户实机手感验收:墙面行走(±X/±Y 重力)时的视角微调舒适度、`AimOffsetUpCm`/限位角/平滑速度手感、多段轨 handoff 实机体验。

### 12.4 踩坑记录(已反哺 skill)

- `IsActive` 与 `UActorComponent::IsActive` 的 UFUNCTION 撞名 → UHT 直接报错;新组件方法避开 `Is*` 常用名(本次改名 `IsDriving`)。
- **UE Python bool 属性去 b 前缀**:`bLookAlongNegativeAxis` → `look_along_negative_axis`(Python API 通用规则,`get_editor_property` 同样适用)。
- **PIE 暂停/恢复给 Tick 塞异常 dt**(负值):指数平滑 α<0 → 相机外推离目标;已加 dt 钳制。远程验证相机/平滑类代码时,"瞬移后立即读数"读到的可能是暂停竞态,等世界推进后再读。
- 编辑器后台窗口会被重度节流(游戏时间远慢于真实时间):远程验证动态行为要给足真实等待。

### 12.5 给下一个 AI

- 新关卡接导轨相机:摆 `GSCameraRail`(本地 Z=轨道方向,朝房间内部看)+ 设 `RailLength`,流程与参数详见 `USAGE_WHITEBOX.md`「相机导轨」节;不摆 = 旧行为,完全向后兼容。
- 相机手感参数集中在球组件 `GSRailCameraComponent` 与导轨 Actor 两处,不需要动 C++。
- 别把重力方向接进轨相机滚转(设计如此:滚转锁世界竖直),要改先看 §12.1 设计定案与用户验收结论。

---

## 13. 2026-09-04 第七轮:障碍物物理砸碎修复 + 玩家球落地三带网格联动(Live Coding + PIE 验证通过)

> 本轮两条交付,均只改 C++、Live Coding 热载成功、PIE fire-and-read 验收;关卡零污染(见 §13.6 地图自动保存雷)。工作树 4 个未提交文件 = §13.2 两件(GravityBody)+ §13.5 两件(LandingResponse);会话开始时仓库 clean,故这些 diff 全是本轮产出,未 commit(如需推送另说)。

### 13.1 需求(用户原话要点)

- **障碍物破坏**:让物理砸碎真实触发(此前看似有能量模型但实际不破)。
- **落地速度检测关联网格系统**:不触发 = 速度 ≤ 从 4 格高度落下;触发反弹 = 5–6 格之间;触发反重力 = ≥ 7 格;**反弹后的高度为 4 格**。

### 13.2 障碍物砸碎修复:velocity 驱动体收不到 OnComponentHit → tick 自主冲击检测

**症状/铁证**:GravityBreaker 真实自由落体砸脆弱块不破;`GetLastImpactReport()` 恒 `bValid=False` → `OnComponentHit→EvaluateImpact` 从未执行;直接调 `apply_impact_energy` 却能破。已排除 GridSnap teleport(Breaker DA snap_to_grid=False)。

**根因**:GS 重力体是 velocity 驱动模型(每帧 `SetPhysicsLinearVelocity`+`AddForce`+`SetEnableGravity(false)`),Chaos 求解器**不为这类体派发 OnComponentHit 通知** → 全部冲击检测走死路径。

**修复(加在 `GSGravityBodyComponent`)**:不依赖引擎通知的 tick 自检——
- TickComponent 读实际速度(pre-physics,post-step):`>100cm/s` 判运动中,跟踪峰值接近速度+方向;掉到 `≤100cm/s` 判停住 = 撞击(100 远高于 resting jitter ~20,远低于真实下坠)。
- 沿接近方向 `LineTraceSingleByChannel(ECC_Visibility)` 自身包围盒 + 15cm 找目标 → 有 `UGSBreakableComponent` → 复用 `ApplyImpactEnergy`(共享 `LastImpactTimeByActor` 冷却去重)。
- 仅 `bCanBreakTargets` 体参与(球/普通块不受影响);OnComponentHit 绑定保留(死路径无害)。

**PIE 验证**:500cm 空投 → 破(`impact 15369.76J … health 0.00`/`BROKEN`;理想 v=√(2·16·5)=12.65m/s≈18000J,实测 11.7m/s 为拖拽损耗,同量级);静置 5s 无假破。

### 13.3 LandingResponse 现状读源(改动前的数据真相)

球 DA `DA_GS_Ball_Default` 落地三值实际是 **150 / 2000 / 250 cm/s** 硬编码(quiet<150 / 反转≥2000 / 弹速250),`AutoReverseMode=LANDING_IMPACT`(只落地判定,半空 mercy 关)。与网格物理值 v4≈1131、v7≈1497 完全不符 → 用户要求把这些阈值从「格数」实时推,真正关联网格。

### 13.4 网格联动落地带设计定案

**公式**:`v(格) = √(2 · g · 格 · cell)`,g = 实时 `GravityManager.GravityAccelerationCm(1600)×GravityBody.GravityScale`,cell = 网格细胞 100cm(与 GSGridSnapComponent/GSBlockProfile 同源)。整格跌落无拖拽(axis drag 0)→ 冲击速度≈距离换算,速度分类即"从 N 格落下"。

| 用户规则 | 落地阈值 | 数值(g=1600, cell=100) |
|---|---|---|
| ≤4格 → 无效果 | impact ≤ v(4) | 1131 |
| 5–6格 → 反弹 | v(4) < impact < v(~6.25) | — |
| ≥7格 → 反重力 | impact ≥ v(7−0.75)=v(6.25) | **1414**(内收原因见下) |
| 反弹到 4格 | 弹速 = v(4) | 1131 → 顶点 400cm=4格 |

反弹带内落地一次 → 弹回 4格 → 回落后 impact≈v(4) ≤ 阈值 → 安静收敛(不再无限弹,靠既有 bounce-once-per-cycle + 新 `<=` 双保险)。

**实现位置 `GSLandingResponseComponent`**:
- 新增 `bGridBasedLanding=true` + `GridCellSizeCm=100` + `QuietLandingMaxCells=4` + `GravityReverseMinCells=7` + `BounceToHeightCells=4`(`GravityShift|Landing|Grid` 分组)。
- `GetEffectiveLandingModifier()`:无 volume 覆盖时用上述格数实时推导三条 cm 阈值;**volume 落地覆盖(cm,最高优先)语义不变**;`bGridBasedLanding=false` 时退回旧 raw cm 字段(逃生舱)。
- `HandleLanding` 安静判定 `<`→`<=`(兑现「≤4格无效果」的等号边界)。
- 新增 helper `GetLandingGravityAccelerationCm()`(manager accel × 体 gravity scale)、`FallImpactSpeedForCells(cells)`。
- 球体 `DA_GS_Ball_Default` / `GSRollingBallPawn::ApplyBallProfile` **零改动**:profile 仍推进的 150/2000/250 cm 字段在网格模式下被忽略(仅 cm 模式/后备用)。反弹沿用既有切线保持(0.85)语义。

**反重力阈值内收 0.75 格(关键防雷)**:落地组件用的是**接触前一物理帧**采样的 `CurrentFallSpeedCm`,天然偏低 ~g·dt(60fps≈25cm/s;低帧率更狠)。若阈值取精确 v(7)=1497,恰好 7格跌落实测可能落在阈值下 → 误判反弹。内收到 v(6.25)=1414:6格真速 1386 **永不高估→任何帧率都反弹**;7格在前台可玩帧率采样 ≥1472 → 稳反转。语义仍整格:反弹只在 5–6格,≥7格必反转。

### 13.5 PIE 验收(网格三带行为轨迹 fire-and-read)

测法:编辑关卡空旷区摆一块立方平台(顶面 Z 定死),球从 PlayerStart 瞬移到平台正上方 N·100+50 处归零速 → bash sleep 分段采样 `z` + 重力方向。平台顶避开上/下 KillVolume 夹层(全图 z1300~1900 上夹层、z<−150 下夹层,平台顶取 Z=300 使 8格 中心仍低于 1300)。

| N(格) | 期望 | 实测轨迹 |
|---|---|---|
| 4 | 安静不弹 | 落至静息 z=350(=顶+50)稳定,不升空 ✅ |
| 5 | 反弹到 ~4格 | apex≈738 后回落静止 ✅ |
| 6 | 反弹到 ~4格 | apex≈750-753(静息+400=4格)后回落静止,gravity 恒 −1 不反转 ✅ |
| 7 | 反重力 | **后台 PIE 只读到 impact≈1356(<1414)→ 反弹**(见 §13.6 节流雷,后台不可分 6/7) |
| 8 | 反重力 | impact 1673 ≥1414 → gravity 翻 +1、持续上行(460→1093)直至顶部 killvol 重置 ✅ |

推导阈值读值确认:`noResp=1131.4 / autoRev=1414.2 / bounce=1131.4`(与公式一致)。反重力路径经 N=8 实触发确认;4/5/6 三带分界轨迹成立;**7格在真实前台帧率**下采样 ≥1472>1414 必反转(数学余量,本环境仅此一格无法在后台精确复现,交用户实机确认)。

### 13.6 踩坑记录(本轮新增)

1. **后台 PIE 深度节流 → 落地采样系统性偏低**:编辑器非前台时 PrePhysics tick 被压到 ~11fps,`CurrentFallSpeedCm` 最高采到"接触前 ~90ms"的速度 → 系统性低 ~g·dt≈140cm/s(N=7 理想 1497 只读到 1356;N=8 读 1673,差 116 同量级)。`Slate.SleepWhenNotForeground 0` / `t.UseLessCPUWhenInBackground 0` 均救不了后台 PIE。**结论:落地阈值这类"边界值等于物理落速"的验收,必须在真实前台帧率跑或用大落差让采样稳超阈值**;6↔7 格(速度差仅 ~111)在后台 90ms tick 下本就不可分,非代码缺陷。
2. **编辑器关卡自动保存污染**:PIE 建场期间编辑器把测试平台自动存进了 `Content/测试案例.umap`(git diff +10KB,磁盘含 `PIE_TestPlatform`),即使事后 destroy actor 也只清内存。本会话对关卡无任何有意修改 → `git checkout` 该 umap 回退,残留清除(grep 0)。**教训:编辑器关卡测试后若 .umap 出现莫名 diff,先查测试 actor 残留并回退,别把脏关卡留在工作区。**
3. `FGSLandingReport` 这类自定 struct 的 enum 字段在 UE Python 读不稳定(偶发 pythonize 崩溃/读成垃圾)——验证改走"轨迹 z+重力方向"行为信号,别依赖 struct 字段读。

### 13.7 给下一个 AI

- 落地三带参数都在 `UGSLandingResponseComponent` 的 `GravityShift|Landing|Grid`(细胞 100 / 4 / 7 / 4),不改 C++ 也能在实例上调;要关网格联动把 `bGridBasedLanding` 勾掉即回 raw cm 模式(组件旧字段 + 球 profile 的 150/2000/250 才重新生效)。
- 障碍物破坏 = 纯 tick 自检,无需引擎 Hit 事件;想调灵敏度改 `GSGravityBodyComponent` 里匿名命名空间 `TickImpactDetectSpeedCm`(现 100)。相关能量/阈值/标签链见既有 Breakable 体系。
- 想确认 7格边界:让用户前台跑 PIE 从 7格顶自由落一次(应反转),或任何 ≥30fps 环境按 §13.5 表重测。
- 详细记忆已存 `.claude/.../memory/`:`physical-break-path-not-firing`(砸碎根因)、`grid-linked-landing-bands`(网格联动 + 节流/autosave 两雷)。

---

## 14. 2026-09-04 第八轮:拾取物品 + 拾取钥匙开门(编译通过 + PIE 全用例验收)

> 本轮交付一条交互链:**F 交互拾取物 → 屏幕中央消息 + 输入锁屏(空格继续)→ 钥匙拾取按 ID 打开匹配的门(滑开动画)→ 死亡/世界重置回锁复位**。全部 C++ 落地,UBT 编译通过,按交付要求写的测试用例在 PIE 自动跑通(逻辑 18/18 + 滑门时序)。工作树 = 6 个源码改动文件(见 §14.3),**未 commit**;关卡 0 污染(§14.6 雷 3 自动存盘已回退)。

### 14.1 需求(用户规格要点)

- **拾取物 AGSPickupItem**:F 交互拾取 → 屏幕中央显示一句提示,**锁住玩家输入直到按空格继续**(锁的是输入,球靠物理自然滑停,不冻结)。
- **AGSKey + AGSDoor**:钥匙拾取后,**所有 `RequiredKeyID` 匹配的门被打开**(门体滑开);支持钥匙/门/拾取物随**世界重置**回到初始(死亡后重新上锁、钥匙复位)。
- 附带:交互提示/中央消息 HUD 绘制;给设计器写配对指南与测试用例(§14.5 即据此实跑)。

### 14.2 实现与关键设计决策

交互检测沿用项目既有的 **无碰撞距离判定**:`GSRollingBallPawn::FindBestInteractable()` 世界扫描 + `IGSInteractable::Execute_CanInteract` 过滤(本次把过滤加进扫描,已收集隐藏拾取物不再遮蔽别的目标),半径 `InteractionRadiusCm=320`,F 键 `TryInteract()` 触发,全部走轮询边缘检测(项目反 BindAction 的既有约定)。

**与最初规格字面的偏差(实现定案)**:
1. **不 Destroy,只隐藏**:规格最初写收集即销毁,但销毁后世界重置无法找回 → 改成 `SetHiddenInGame + 关碰撞`,重置时 `RestoreInitialState` 重新武装(与 AGSCollectible 同款语义)。
2. **Interact 返回 bool**(不是 void):界面签名 `bool Interact(APawn*)`,收集/开门成不成功都要反馈。
3. **无 TMap/门状态表**:规格的 KeyDoorMap 集中配对 → 改为**每扇门自带 `RequiredKeyID`**,钥匙 Interact 里 `TActorIterator<AGSDoor>` 全图广播 `TryUnlock(KeyID)`。一对多/多对一自然成立,少一层管理状态。
4. **"暂停"= 输入抑制**:球不人为冻结,只是锁帧期间不给移动输入(物理让它自然滑停)。中央消息仅当 `PickupMessage` 非空才锁屏(空消息=静默收集,设计器可选)。

**新增公开 API**:
- `AGSPickupItem`(`GSInteractables.h/.cpp` 追加):Mesh 根(NoCollision,默认立方 0.4)+ `PickupMessage`(FText)+ `bIsCollected`;`Collect(APawn*)->bool`(置已收集/隐藏/关碰撞 → 有消息则 `ShowMessageAndLock`)、`RestoreInitialState()`;BeginPlay 快照 `bInitialCollected`。
- `AGSKey : AGSPickupItem`:ctor 预置默认提示「你找到了一把钥匙,匹配的门被打开了。」;`Interact` 先 `Super`(拾取+锁屏)再全图解锁;`KeyID`(FName)。
- `AGSDoor`:DoorRoot 场景根 + DoorMesh 子件(BlockAllDynamic,默认立方 scale (1.6,0.3,2.2) @ Z=110);`RequiredKeyID` / `bIsLocked`(默认 true)/ `SlideOffset`(默认 240)/ `SlideDuration`(0.8);`TryUnlock(FName)->bool`(同 ID 才开)、`SetLocked`、`RestoreInitialState`;`CanInteract` 仅锁着时可交互(解锁后不再是目标),锁着时按 F 若有 `LockedMessage` 则弹提示;`Tick` 驱动 DoorMesh 在 关位↔SlideOffset 间插值滑动。
- **Pawn 锁屏**:`ShowMessageAndLock/GetPendingMessage/IsMessageLocked/GetMessageDismissKey/DismissPendingMessage` + `DismissMessageKey`(默认 Space);Tick 锁帧分支只监听空格上升沿。
- **HUD**(`GSFramework.cpp`):锁屏时中央画 `GetPendingMessage()`(SizeY*0.4)+ 下方「按 空格 继续」。
- **世界重置**(`GSWorldState.cpp` `ResetWorld`):对 `AGSPickupItem`/`AGSDoor` 各调 `RestoreInitialState`;若玩家锁屏被重置则顺带 `DismissPendingMessage`(防死在消息态)。

### 14.3 改动文件清单(6 个,未 commit)

- 改 `Public/GSInteractables.h`(+125)、`Private/GSInteractables.cpp`(+240):追加上述三类实现。
- 改 `Public/GSRollingBallPawn.h`(+25)、`Private/GSRollingBallPawn.cpp`(+70):锁屏消息五件套 + DismissMessageKey + Tick 锁帧分支 + `FindBestInteractable` 加 CanInteract 过滤。
- 改 `Private/GSFramework.cpp`(+20):HUD 中央消息绘制。
- 改 `Private/GSWorldState.cpp`(+20):ResetWorld 拾取物/门复位 + 玩家消息解除。

### 14.4 行为契约

- 门是**全局按 ID 配对**,无距离/无配对表:任何地方捡到 KeyA,全图所有 `RequiredKeyID=KeyA` 的门同时滑开;捡钥匙不捡拾到的那一扇负责解锁全部同名门。
- 重置把门 `bIsLocked` 打回 `bInitialLocked`,滑门动画由 Tick 反向滑回(需世界 tick,重置瞬间状态已回锁、门板随后合拢)。
- 门不具物理模拟(静态),解锁前后都原地;DoorMesh BlockAllDynamic,锁着时是真实路障。

### 14.5 PIE 验收记录(测试案例地图,临时摆场未存盘)

测法:编辑器层在 z=50000 高空摆隔离簇(避开房间几何/夹层 KillVolume)→ StartPIE → `set_game_paused(True)` 冻结 → 脚本把球瞬移到目标旁逐条断言(距离判定在冻结帧内瞬时成立,免去物理漂移)→ 滑门/合拢两段单独放行真实秒数再读。Python 反射注意:UE Python 类名去 A 前缀(`GSDoor/GSKey/GSPickupItem`),bool 属性去 b 前缀(`is_locked`)。

| 用例 | 结果 |
|---|---|
| 靠近 + F 拾取(LK:`__LockOnly__` 钥匙,专测锁屏不碰门) | ✅ TryInteract true、`is_collected` true |
| 拾取后隐藏/不再可二次拾取 | ✅ 再 TryInteract 被 CanInteract gate 拒 |
| 中央消息 + 输入锁 | ✅ `is_message_locked()` true,`GetPendingMessage()` 返回原文「你找到了一把钥匙…」 |
| 空格解除 | ✅ `DismissPendingMessage()` 后解锁 |
| `RestoreInitialState` 重新武装 | ✅ 可再拾取 |
| KeyA 开门 | ✅ DoorA1/A2(`RequiredKeyID=KeyA`)解锁,**DoorB(KeyB)仍锁** |
| KeyB 开门 | ✅ DoorB 解锁,A 门保持已开 |
| 滑开动画(真实时间) | ✅ 放行 ~1.6s 后 DoorMesh rel z 0→**240**(=SlideOffset,alpha=1) |
| 死亡重置回卷 | ✅ `ResetWorld()` 后三钥匙复位、三门重锁、消息清空、球回 checkpoint |
| 滑回合拢 | ✅ 重置放行 ~1.6s 后 rel z 240→**0**,重锁 |

逻辑断言 **18/18 全过** + 开/关两段滑门时序读数符合预期。F/空格键的 OS 级按键注入未代测(沿用项目既验证过的轮询链路,空格由 `DismissPendingMessage` 直调覆盖;交互实体实机游玩留用户)。

### 14.6 已知限制 / 本轮新发现的雷

1. **滑门落座基准不一致(本轮实测暴露的真 bug,用户拍板暂不修,记录在案)**:门动画在 `Lerp(Zero, SlideOffset)` 间插值,而构造函数把 DoorMesh 初始座在相对 Z=110。后果——解锁首帧门板会先瞬落 ~110 再上滑;经历一次开→关循环后,关闭态停在 rel z=0(比初始落座低 110)。浮空测试簇不可见,真关卡里若门根贴地会半截埋地。**修法(一行方向,未实施)**:BeginPlay 记录 `ClosedMeshOffset = DoorMesh 当前相对位置`,`Tick` 改在 `ClosedMeshOffset ↔ ClosedMeshOffset+SlideOffset` 间插值(开态 350、关态回到 110)。要修需重启编辑器→重编→重跑 §14.5 滑门两段确认。
2. 拾取物 `CanInteract` 是距离判定,**无视线遮挡检测**:隔着墙在 320cm 内也能 F 拾取。如需按当前体系规则补 LOS,后续加。
3. **编辑器自动存盘污染关卡(第 §13.6 雷 3 复发)**:本轮在编辑器层摆测试簇 + PIE,编辑器把 `Content/测试案例.umap` 自动存出 +13.5KB diff(即使事后 destroy 也只清内存)。已按既定处置 `git checkout` 回退该 umap,工作树只剩 6 个源码改动。**教训照旧:凡涉及编辑器摆场/PIE,跑完查 umap 是否被 autosave,别把脏关卡留在工作区。**

### 14.7 给下一个 AI

- 摆一对钥匙/门:放一个 `AGSDoor`,Details 填 `RequiredKeyID`(如 `KeyA`);放一个 `AGSKey`,填同 `KeyID`。想"一扇门需要多把不同钥匙"→ 当前模型是"任一同 ID 钥匙即开",需升级再加。
- 中央锁屏提示可选:拾取物 `PickupMessage` 留空 = 静默收集不锁屏;钥匙 ctor 自带默认文案。锁屏键在球 Pawn 的 `DismissMessageKey`(默认 Space),HUD 提示文案在 `GSFramework.cpp`。
- 想确认滑门修正:等用户决定后按 §14.6-1 一行改,重启编辑器重编,PIE 摆一扇门捡钥匙看 rel z 350 / 重置后回 110。
- 工作树 6 文件 = §14.3 全部本轮产出,未 commit(如需推送另说)。

---

## 15. 2026-09-05 第九轮:积木式对接文档(同步 §13/§14 + 白盒手册实测)

> 本轮是**文档轮**,同时把 §13/§14 的代码(提交 294a806 + 6248bcb,12 文件,含 umap)拉回本机、UBT 重编(dll 13:05 > 源码 13:03,本地 commit cb064ee),并按队友要求把"怎么把 GS 积木拼进关卡"写成正式手册——**手册里写的每一步都先在编辑器实机走通过再落笔**,不是纸上谈兵。

### 15.1 队友诉求与本轮回应

诉求(转述):简要说明蓝图等文件存放规范;更新 README / USAGE_WHITEBOX;最好有一份**现有功能积木的简要汇总 + 具体调用方式**,让关卡策划做白盒时能像拼积木一样把系统拼进 UE。

回应(三处,全部已提交):
1. **README「功能积木清单」**:一张表列全 10 类可摆积木(Pickup/Key/KeyDoor/GravitySwitch/SurfaceVolume/LandingVolume/BlockBase+DA/KillVolume/Checkpoint|Collectible|FinishGoal/CameraRail),每块写"干什么 + 怎么配对/触发";另附一行"自动挂球身上不用摆的组件"(GravityBody / LandingResponse 落地三带 / RailCamera)与砸碎链说明。**这就是关卡策划的对接入口。**
2. **USAGE_WHITEBOX 新增「文件与命名规范」**:Content 分域表(GravityShift/Core|Blocks|Interactions|World|Data/Profiles|Tests vs LevelPrototyping 白盒库 vs Content 根放 umap,新关卡建议 Content/Maps/)+ 命名前缀约定(BP_/DA_/SM_/M_/MI_/T_)+ "GS 类全在 C++,BP 只是壳,能直接摆 C++ 类就不必先建 BP"。
3. **USAGE_WHITEBOX 新增「拾取物/钥匙/门」整节**(见 15.2)与「落地三带与重力砸碎」整节(策划视角,含"别拆破坏者的 GSGravityBody"警告)。

### 15.2 手册可行性实机验证(PIE,编辑器摆件→三段式读态)

按手册"最小配对流程"原样执行:编辑器世界 spawn `GSPickupItem`(带文案)+ `GSKey`(KeyID=red)+ `GSDoor`(RequiredKeyID=red)→ PIE:

| 步骤 | 断言 | 结果 |
|---|---|---|
| 传送球到拾取物旁 `TryInteract()` | `is_collected=True` / `is_message_locked=True` / 中央文案=自定义句 | ✅ |
| `DismissPendingMessage()`(=按空格) | `is_message_locked=False` | ✅ |
| 传送球到钥匙旁 `TryInteract()` | 钥匙 `is_collected=True` **且** 门 `is_locked=False`(捡钥匙瞬间全图广播解锁) | ✅ |
| 放行 ~6s 读门板 | DoorMesh 相对位置 z=**240**(=SlideOffset,滑动动画完成) | ✅ |
| `ResetWorld()` | 门回锁 `True`、拾取物/钥匙 `is_collected=False`、锁屏解除 | ✅ |

测后清理:TEST_ 三件已销毁;umap 被 autosave 写脏一次(§13.6 雷 3 三度复发),已 `git checkout` 回退,工作树干净——**教训同前,PIE 摆场后必查 umap**。

### 15.3 本轮新坑(反哺 skill)

1. **Python 类名没有 A 前缀**:`unreal.GSKey`/`GSPickupItem`/`GSDoor`(不是 AGSKey…);`hasattr(unreal,'AGSKey')` 恒 False。C++ 的 A/U 前缀在 Python 绑定里剥掉。
2. **BlueprintPure 函数 ≠ 属性**:`IsMessageLocked()` 要 `ball.is_message_locked()` 方法调用,`get_editor_property('is_message_locked')` 直接异常。
3. `EditorLoadingAndSavingUtils` **没有** `set_dirty_package`;清关卡脏标记用 `load_map` 重载(本轮 PIE 结束后脏列表已空,未走到)。

### 15.4 给下一个 AI

- 关卡策划/新关卡对接:**README「功能积木清单」→ USAGE_WHITEBOX 对应节**,别再口头转述;拾取/钥匙/门测试图里没有预摆,要试照 §15.2 流程 5 分钟搭一套。
- §14.7 末"工作树 6 文件未 commit"已过时:§13/§14 全部产出已随 cb064ee 收进仓库,远端见本轮推送。
- §14.6-1 滑门落座偏差仍**未修**(用户拍板暂不修),别当回归。
