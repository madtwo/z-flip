# GravityShift v5 — 进度同步 / 交接文档（z-flip 项目）

> 更新时间：2026-09-05 (GMT+8)
> 状态：v5 已验收（§8-§10）；v6 六方向已同步本机并编译（§11；+Y 贴墙/G 两墙摆荡已 PIE 抽测通过，§11.9 全用例矩阵仍待实机）；**§12 = 导轨相机(防晕)+ 全表面操作映射 + Q/E 玩家调距,PIE 验证通过,待用户完整手感验收**
> **§13 = 2026-09-04 第七轮交付：障碍物物理砸碎修复(根因 tick 自检+PIE 15369J)+ 玩家球落地三带网格联动(≤4格安静/5-6格反弹/≥7格反重力,弹回4格)**,均 Live Coding+PIE 验证通过;实机手感 & 前台 7格边界验收待用户
> **§14 = 2026-09-04 第八轮交付：拾取物品 + 拾取钥匙开门(F 交互/拾取锁屏消息空格继续/门按 RequiredKeyID 配对/滑开动画/死亡重置回锁复位)**,UBT 编译通过 + PIE 全用例验收(18/18 断言 + 滑门开/关时序);详见 §14,剩一处已知滑门落座偏差见 §14.6
> **§15 = 2026-09-05 第九轮交付：§13/§14 拉取同步本机+重编 + 「积木式」对接文档(README 功能积木清单 / USAGE_WHITEBOX 文件存放规范+拾取钥匙门手册),手册摆法已实机 PIE 走通**;关卡策划对接入口 = README「功能积木清单」→ USAGE_WHITEBOX
> **§25 = 2026-09-11 第十七轮交付：相机俯仰反转修复(鼠标上抬→相机上抬)+ 重力区域检测器系统(检测器/区域管理器两个新类,"禁用重力"= 停重力+清速度瞬间定住)**,UBT 编译通过 + PIE 7/7 全过;关卡侧拼装手册见 `AgentSkill/gs-gravity-zone-assembly/SKILL.md`,遗留:两个 Zone 数组按需求留空待后续 AI 按空间位置填
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

---

## 16. 2026-09-07 第十轮:同步「白盒搭建基础」(Blockout 地图+插件入库) + 队友"看不到东西"诊断

> 队友李昱辰 push 提交 48a7eb6「白盒搭建基础」(101 文件):`Content/Maps/Blockout.umap` + 完整 Blockout Tools 插件 v1.52(`Plugins/Blockoute60d8e1bd542V15/`,含 Runtime+Editor 两个 C++ 模块与全部 Content)+ `z-flip.uproject` 启用插件。本机已拉取备份(`_sync_backup/48a7eb6/`)、UBT 重编通过(BlockoutToolsPlugin/EditorPlugin 两个 dll 生成)、编辑器验证通过。

### 16.1 验证结果(本机)

- 插件:uproject 已启用 `BlockoutToolsPlugin`;引擎 Marketplace 无同名冲突;`/BlockoutToolsPlugin/Blueprints/Blockout_Box` 蓝图类可正常加载 → 插件功能可用
- 地图:`/Game/Maps/Blockout` 打开正常,**14 个 Actor** = 灯光组(方向光/天空光/SkyAtmosphere/体积云/天球)+ PlayerStart + 1 个 BSP 笔刷(约在 (0,2640,90),与测试案例同区)——它是**白盒搭建起点图**,不是摆满的成品关卡
- 非 World Partition(仓库无 `__ExternalActors__` 路径),单 umap 自包含,无"外部 Actor 没提交导致打开空"风险
- PlayerStart 在 → Blockout 图按 Play 球照常生成(GameMode 全局),GS 系统可直接在里面玩

### 16.2 队友"看不到东西"诊断与解法(已写进 README「拉取队友更新后看不到东西?」)

根因:这次新增**带 C++ 的插件**,只 `git pull` 不重编译 → 插件加载不了/被禁用,依赖插件的内容打不开或地图看着是空的。解法(已写进 README 快速开始下方,可直接转给队友):

1. 关掉开着的编辑器
2. 右键 `z-flip.uproject` → Switch Unreal Version… 选当前引擎(触发重编译);或双击打开在弹窗选「重编译」
3. 编完再开

次要澄清:Blockout 图本身就是"起点"(灯光+PlayerStart+1 笔刷),在里头用 Blockout Tools 的 BP 积木(Blockout_Box/Ramp/Stairs 等,Place Actors 搜 Blockout)画白盒;看到的主要是天空+笔刷属正常。

### 16.3 给下一个 AI

- 同步轮 diff 里凡出现 `Source/` 或新插件目录:**拉完必重编**,并验证 `Plugins/<新插件>/Binaries/Win64/*.dll` 生成(新模块不会走 GravityShift 的 dll 时间戳检查,要单独看)
- 队友报"看不到东西"排查链:①本地有没有 pull ②pull 的 diff 里有没有 C++ → 重编了没 ③地图非 WP 就查 Actor 数(空图≠坏图,Blockout 本来就是起点图)

### 16.4 用户验收拍板:Blockout 保持空底座,底座补全已回退(同日)

- 用户按 Play 曾反馈"只有个球"(出生点 (0,0,252) 周围无地板,球坠虚空)。AI 曾补过一版基础底座(白盒地板+KillVolume+3 个 Blockout 演示件)并 PIE 验证可玩
- **用户随后拍板:这张图就该是空盒子,不用改——底座补全已全部回退**,`Content/Maps/Blockout.umap` 恢复李昱辰原版(灯光组+PlayerStart+1 笔刷)
- 结论给后人:Blockout 起点图**按原样用**;谁要在里面按 Play 试球,先自己摆地板(PlayerStart 在 z=252,球会坠,直到李昱辰正式搭出白盒);摆法照 `AgentSkill/gs-block-assembly/SKILL.md` §2 最小可玩关卡

---

## 17. 2026-09-08 第十一轮:相机移动抖动修复(枢轴绝对定位,PIE 逐帧数据验证)

> 队友报告:**移动时相机"卡卡的",还会瞬间小幅抖动**;样例图(导轨相机)和 Blockout(旧跟随相机)都有——两套相机共用同一个枢轴,症状一致,判断为共性问题而非某一张图的问题。

### 17.1 根因:相机枢轴挂在物理球下,位置却走相对模式

- 组件链:`Camera(UCameraComponent) → CameraArm(USpringArm) → CameraPivot(UScene) → BallCollision(球的物理根,每帧翻滚+位移)`
- 旧代码只对枢轴做了 `SetUsingAbsoluteRotation(true)`(**旋转**绝对),**位置仍是相对父级**
- 后果链:每帧 pawn Tick 把平滑后的相机位姿 `SetWorldLocationAndRotation` 写到枢轴 → 本帧稍后的物理步里球根组件移动 → 枢轴作为子组件**继承球当帧的位移** → 这段位移**完全绕过导轨相机的指数平滑层**,直接进入画面
- 表现:匀速滚动 = "卡卡的"(相机被球逐帧拖拽);加速/急刹/碰撞 = "瞬间小幅度抖动"(那几帧球位移突然变大)。样例图与 Blockout 同时中招的原因即"共用枢轴"

### 17.2 修复(2 个源码文件,零关卡改动)

`GSRollingBallPawn.h/.cpp`:
1. 新增 `bUseAbsoluteCameraLocation = true`(`GravityShift|Camera` 分组):BeginPlay 里 `CameraPivot->SetUsingAbsoluteLocation(true)`——枢轴位置改成**纯世界定位**,写多少就是多少,不再被物理球带跑;所有平滑由此完整生效。这是修复本体,默认开
2. 新增 `bRailCamDebugLog`(`GravityShift|Debug`):逐帧打印 `[RailCam] t/dt/pivotPrev/ball/railTarget` 到 `Saved/Logs/z-flip.log`,排查相机问题专用,pivotPrev = 上帧写入值在本帧被漂移到哪(直接读出泄漏量)

### 17.3 验证数据(PIE 脚本驱动球前进,逐帧日志)

| 状态 | 枢轴帧间漂移(= 绕过平滑进画面的量) |
|---|---|
| 旧行为(相对定位) | = 球当帧物理位移,未平滑直接进画面(即队友看到的抖) |
| **修复后(绝对定位)** | **0.00 cm**(39 帧逐帧实测,每帧都精确停在上帧写入点) |

球每帧位移实测 0.4–1.9cm(低速)——旧行为下这就是每帧直入画面的抖动量,急刹/撞击帧更大,与"瞬间小幅"描述吻合。

### 17.4 给队友/下一个 AI

- **pull 后必须重编译**(AGENT_GUIDE §3 SOP;只有 2 个 .h/.cpp,无新模块,正常编一次即可)
- 想 A/B 复现旧行为:球 Pawn 细节面板把 `use_absolute_camera_location` 关掉再玩(同样的抖动会回来);不用改代码
- 以后报相机抖动:开 `rail_cam_debug_log` 跑一圈,看 `Saved/Logs/z-flip.log` 里 `[RailCam]` 行,`pivotPrev` 与上帧 `railTarget` 的差就是泄漏量(正常应恒 0)
- 本轮**没有动任何关卡/umap**,Blockout 仍保持李昱辰原样;只动了 `GSRollingBallPawn.h/.cpp` 两个文件

---

## 18. 2026-09-08 第十二轮:同步「白盒部分迁移」(Blockout 白盒几何入库)

> 队友李昱辰推送 9cc101b/9148df8「白盒部分迁移」:Blockout.umap 更新 + `Content/Maps/_GENERATED/96590/` 9 个建模网格(CubeGridToolOutput×4/Stairs×2/Box×3)。本机已拉取(备份 `_sync_backup/9148df8/`)、无 C++ 改动免重编、编辑器打开盘点完毕。

### 18.1 白盒内容盘点(41 Actor)

- 大体量:`CubeGridToolOutput_4FC468CC`(4425×1300×1000,约 (-1400,-200,0) 平台结构)、`CubeGridToolOutput_C39DF78F`(5800×5700×2800,约 (2600,100,400) 主结构)、`CubeGridToolOutput2`(400³)
- 楼梯 9 件(`Stairs_8C47BBC0`):两段上行(2150→2350,z 200→400;z 750→950)
- 散块 12 件(`Box_13B49198`/`Box_B2F6A1E6`):尺寸 60–320cm,分布 (-2300..1000, 200..1300, -240..956)
- 4 个 GroupActor(成组白盒件)+ 原有灯光组/PlayerStart/BSP 笔刷
- 仍是单 umap 非 WP;无 C++ 改动

### 18.2 事故记录:load_map 撞 PIE → 游戏线程死锁(已写入 PIE_TESTING 坑 6)

- AI 在用户**正在 PIE 游玩时**调 `load_map` 切图盘点 → 游戏线程死锁,双远程通道超时、窗口无法激活;无未保存内容(右下角"所有已保存"),杀进程重启恢复
- **铁律:切图前必查 `is_in_play_in_editor()`**;动手前先扫日志尾部有无 PIE 活动

---

## 19. 2026-09-08 第十三轮:WASD 手感强化 + 无导轨第三人称相机调整(PIE 自测迭代定案)

> 需求(用户):①减少移动惯性、加快过慢的速度;②无导轨(第三人称)视角——G 翻转后视角应仍对准同一方向(旧实现有镜像)、球放画面下三分之一(常见第三人称取景,贴近后期导轨相机观感)、转动惯性太大。**约束:重力/掉落相关参数一律不动,只改 WASD 手感。**

### 19.1 根因链(实测定位,关键数据)

- **旧滚球"移动过慢+惯性大"的真相**:力矩驱动在低摩擦接触下大量**打滑**——实测球自转 ω·r≈3063 cm/s 而线速度仅 241 cm/s(转着不动);平面速度被重力体切向拖拽封顶,且换力矩档位(120/360)终端几乎不动。纯反力矩刹车同样受限(松键停不下来)。
- **切向拖拽(TangentDragHz=0.35)是重力侧参数(队友领地),本轮未动一字**——终端速度的解决靠换驱动模型而非碰它。
- 调试工具教训:`ω·r vs |v|` 对照读数 + 位置/速度联合探针;早期多轮"终端速度"数据全是地形(斜坡/棚子内墙/校准板穿透)污染,**一切手感标定必须在干净平地上做**。

### 19.2 改动(只动 WASD 路径)

`GSRollingBallPawn.h/.cpp` + `GSProfiles.h`:
1. **驱动改为平面加速度**:`AddForce(Desired·DriveAccelerationCm, bAccelChange=true)` 替代滚动力矩(只作用于支撑面上的 WASD;重力/掉落/反弹路径完全不走这里)。终端速度 ≈ DriveAccelerationCm×0.28(实测三组数据线性吻合;机制上等效 3.5/s 的综合减速,与 Profile 的 0.35 差 10 倍的原因未深究,以实测为准)。
2. **松键刹车**:平面速度直接按 `exp(-ReleaseBrakeHz·dt)` 衰减(实测 186→95→41→13 ≈ exp(-3t),约 1 秒停稳);竖直分量原样保留,不影响掉落。旧 StopTorque 反力矩保留作自转收尾。
3. Profile 新增 `DriveAccelerationCm`(默认 400)与 `ReleaseBrakeHz`(默认 3.0),DA_GS_Ball_Default 烘入 **3600**(终端 ≈1080 cm/s ≈10.8 m/s,约旧手感的 4 倍;1600 的平面限速仍在兜底)。

### 19.3 无导轨第三人称相机(回退相机重做)

1. **G 翻转方向一致**:旧实现把偏航角绕"新上轴"重算,+Z/-Z 符号互换导致瞄准镜像(FindBetweenNormals 在反向平行时旋转轴还不确定)。改为**世界航向向量** `CameraAimHeading`(鼠标转向绕当前上轴旋转它,±Z 互换时同一鼠标动作的屏幕转向天然一致)。实测:翻转前后相机 forward 均为 (1,0,0) ✓
2. **球放画面下三分之一**:枢轴沿当前"上"轴抬高 `CameraPivotLiftHeightCm=150`(新参数)。实测球心在屏高 69.4%(翻转前)→67.3%(翻转后),正好下三分之一 ✓;天花板态(上=−Z)同样成立。
3. **转动惯性**:平滑时按角距自适应 τ——大角度(G 翻转 180°)保留 CameraFlipDurationSeconds 的防晕慢速;鼠标微调 τ≈0.12 倍≈瞬时。旧实现统一 0.35s 造成"转动严重惯性"。
4. 无导轨关卡(Blockout 等)自动用这套相机;导轨相机(样例图)行为不变。

### 19.4 给队友/下一个 AI

- pull 后重编译(4 个文件:2 个 .h + 1 个 .cpp + 1 个 DA)
- 手感再调:**全在 `DA_GS_Ball_Default`**——`drive_acceleration_cm`(提速/减速)、`release_brake_hz`(刹车快慢)、`maximum_planar_speed_cm`(极速上限 1600)
- ⚠ 速度/刹车标定必须在**无斜坡/无结构/无表面体积**的干净平地做(本轮在样例图测了半天全是地形干扰;校准法:PIE 里脚本驱动+位置/速度联合采样)
- 已知待用户实机确认:新手感/相机观感(数值是初版标定,不满意继续调 DA)

### 19.5 追加(同轮续):无导轨相机防颠倒(用户续定案)+ O/P 调速键

- **用户反馈**:G 翻转后画面整个颠倒会晕 3D → **无导轨相机滚转锁世界竖直**(`bCameraFlipsWithGravity` 默认 false,pawn+profile):翻转后画面 up 恒 (0,0,1) 实测 ✓,只随球平移+瞄准微调。
- **枢轴抬升改为沿"支撑面外侧"(=−重力)**:`CameraLiftDirection` 以 400°/s 平滑摆动(VInterpNormalRotationTo 是**度/秒**,8=180°要22s,踩过)。地板态相机在球上方(下三分之一 ✓),天花板态在球下方的房间内侧(球在画面上三分之一,用户定案"这样好看点")。
- **瞄准微调**:`CameraAimUpBaseCm=142 + SwingCm=-7×dot(支撑上,世界上)`——地板态轴微俯(球~76% 屏高),天花板态轴水平(球~上三分之一);早先用 dot(抬升方向,支撑上) 做判别度恒 +1 失效(抬升会收敛到支撑上),已改用 dot(支撑上,世界上)。⚠ 标定发现投影读数与几何角差 8°+,疑似视口 FOV 与假设不符——**细调以实机手感为准,两个参数都是 UPROPERTY**。
- **弹簧臂碰撞探针已关**(回退相机):白盒紧空间把臂长 700→250 压塌,球被顶出取景框;与导轨相机同规矩(球贴墙穿模后议)。
- **O/P 调速键**(沿用 Q/E 轮询边缘检测):`AdjustDriveSpeed(±1)` 步长 350(终端≈±98 cm/s),范围 600–7200 夹紧;HUD 提示行加 O/P;PIE 实测 3xP/6xO/双向夹紧全对。
- **编译纪律(再次踩)**:编辑器运行时 UBT 报 Succeeded 但 dll 实际没链接(dll mtime 落后源码)——**重编前必关编辑器,编完必对 dll mtime**。

### 19.6 相机随地形自适应 + G 过渡平滑(用户反馈续)

- **相机出房间** → 恢复弹簧臂碰撞探针(`bDoCollisionTest=true`,v5 原行为,贴墙不出房);同时**枢轴抬升按实测臂长等比缩放**(`LiftScale = ArmNow/Cfg`,实测臂 700→250 时抬升 150→54),球的角取景恒定——相机适配地形且球不出取景框(两全)。
- **按 G 抖动** → 回退相机枢轴位置加**指数平滑**(速率=Profile 的 `CameraFollowInterpSpeed=18`,此前一直闲置);UpdateCamera 顶部补 dt 钳位(PIE 暂停恢复/掉帧尖峰会让平滑外推——导轨相机的 ComputeCameraPose 早有钳位,回退路径漏了)。
- 平滑参数都是 UPROPERTY:球 Pawn `CameraFollowInterpSpeed`(跟随快慢)/`CameraPivotLiftHeightCm`/`CameraAimUpBaseCm|SwingCm`(球在屏幕上的高度)。

### 19.7 按下 G 的"向下/上窜动"修复:抬升摆速 400→150°/s(2026-09-09)

- **用户反馈**:按 G 相机会"突然向下/上抖一下",很突兀;过渡应该**在原位置连续变化**。
- **根因**:G 按下瞬间"支撑侧抬升"目标换边(相对球 ±150cm,共 300cm 行程),`CameraLiftDirection` 以 400°/s 摆动 → 枢轴在 **~0.45s 内被拽着掠过这 300cm**(≈10 m/s 的镜头猛冲)。19.6 的指数平滑只滤高频噪声,压不住这种快速扫掠本身。
- **修复**:摆速降到 **150°/s**(180° 走 1.2s,与视角翻转 ~1s 同步)→ 整个过渡变成一次从原位置出发的连续慢速平移(≈4 m/s,无任何跳变)。改点:`GSRollingBallPawn.cpp` UpdateCamera 里 `VInterpNormalRotationTo(..., 150.0f)`,嫌快/慢直接改这一个数。
- **验证方法(可复用)**:Blockout PIE + `set_global_time_dilation(w, 0.05)` 时间膨胀 20×,用 ue.py 每 ~1.7s 实时采一个点(=0.067s 游戏时)。实测抬升偏移 `pivot−ball` 从 +53.5cm 沿圆弧**连续**滑到 −53.5cm(中间 X 分量峰值 −44cm,即标准 180° 弧),总耗时 **~1.33s**,与 150°/s 理论值 1.2s 吻合,相邻采样无跳变;采样法已反哺 PIE_TESTING.md 坑 2。
- **注意**:Blockout 走的是无导轨回退相机(导轨组件存在但未激活,枢轴跟随球);样例图「测试案例」的导轨相机全程静止,不受本改动影响——报"G 窜动"类问题先确认所在关卡用的是哪套相机。
- **19.7 追加(同日续,用户复测"还是会突变"后定位)**:上一条只压住了**位置**的快速扫掠;**旋转**突变是另一个独立根因——瞄准点用的是**瞬间换边**的 `SupportUp`:G 按下那一帧瞄准点瞬移 ~284cm(支撑面从球下翻到球上),画面俯仰瞬间跳 **~22°**,随后又随枢轴在 1.2s 内慢慢飘回来(所以"突变"感依旧)。**修复**:瞄准点与判别项改用已平滑的 `CameraLiftDirection`(收敛后与 SupportUp 等价,过程中连续),瞄准点与枢轴同步沿弧线移动,俯仰全程只变 ~15° 且是连续滑过去的。
- **复测数据**(同法,时间膨胀 0.05× 采样):pitch 从 +6.6° 连续滑到 −8.3°,全程 ~1.33s,单步最大 3.6°(出现在球撞天花板那一帧的物理冲击,非 G 突变);修复前同一时刻应有 −22.7° 的瞬时跳变。**教训:验证相机"突变"必须同时采位置和旋转——第一轮只采了枢轴位置,漏掉了俯仰跳变。**

### 19.8 爬楼梯(Z 轴重力态)画面抖动修复:探针翻转去弹 + 瞄准随臂长缩放(2026-09-09)

- **用户反馈**:爬阶梯(Z 轴重力态)时画面抖动。
- **复现与定位**:新增逐帧日志开关 `bFallbackCamDebugLog` + 持续前推调试开关 `bDebugAutoDriveForward`(都默认关),Blockout 里让球从 (2060,250,300) 自动爬完整段楼梯,逐帧日志(`Saved/Logs/z-flip.log` 的 `[FallbackCam]`)显示:弹簧臂自带碰撞探针在第二段楼梯(命中 `StaticMeshActor_11`,z=750)的棱边处**逐帧翻转**——臂长 700↔179、LiftScale 1.00↔0.25,相机 X 每帧 ±500cm、俯仰 ±6.4°。这就是"画面抖动"。
- **修复**(三处,`GSRollingBallPawn.h/.cpp`):
  1. **关掉弹簧臂自带探针**(`bDoCollisionTest=false`),改为**自建 ECC_Camera 线探针**(枢轴→期望相机位):命中立即收短(8/s)、**连续无命中 0.7s 才缓慢放长**(`ArmExtendHoldSeconds`/`ArmLengthInterpSpeed` 可调)——探针翻转被去弹,相机稳定在安全距离,离开后平滑拉远。
  2. **瞄准高度随臂长缩放**:AimUp 也乘 `LiftScale`(此前只缩了枢轴抬升,臂塌缩时俯仰会甩 +13°);同时 `CamPosApprox` 改用平滑后的实际臂长(而非配置的 700)。
  3. 试过弹簧臂自带相机滞后(`bEnableCameraLag`),压不住 ~1.5Hz 方波,已关。
- **实测**(同法逐帧日志):楼梯段相机 X 每帧平滑 +15~22cm(修复前 ±500cm 翻转),俯仰稳定在 −1°±3°(修复前摆到 +14.6°),离开楼梯后臂长平滑回到 700。探针命中信息也进了日志(`hit=` 字段,本例 StaticMeshActor_11)。
- **注意**:相机"抖动/突变"类问题**必须看逐帧日志**(C++ 内 gated log)——python 采样在后台节流下每秒只能采 ~3 帧,会漏掉翻转;两个调试开关默认关,不影响发布行为。

## 20. 2026-09-09 第十四轮:小球不可推动可移动物块(质量方案,数据改动)

- **目标/根因**:小球(30kg)撞可移动物块会被推开——推动是**纯物理求解器接触冲量**,球↔物块间**无任何 OnComponentHit 代码**可拦。要“球推不动、但块仍受重力/翻转正常移动、且对球保持实心”,唯一不破坏实心语义的杠杆是**质量**。
- **关键前提**:`UGSGravityBodyComponent::Tick` 用 `AddForce(..., bAccelChange=true)` 施加**加速度**(`GSGravityBodyComponent.cpp:211`),质量无关 → 调大物块质量**不影响重力翻转/下落速度**,只增加抵抗小球冲量的惯性。
- **改动内容(Profile 数据,非代码)**:
  - `DA_GS_Block_Gravity.MassOverrideKg` **40 → 5000**(小球 30kg,冲量分享 ≈30/(30+5000)≈0.6%,1600cm/s 撞上块只获 ≈19cm/s 且被 `TangentDragHz` 0.15 衰减 → 观感推不动)。
  - **不改** `DA_GS_Block_GravityBreaker`(保持 90):砸碎能量 `0.5·m·v²` 随质量,调大会让 Breaker 一击碎万物。
  - 生效链:块 `BeginPlay → ApplyBlockProfile → ApplyCurrentConfiguration → Mesh->SetMassOverrideInKg`(`GSBlockBase.cpp:147-150`),一处 Profile 改动全体使用该 Profile 的块生效。
- **脚本(已入库 `Plugins/GravityShift/Content/Python/v5/`)**:
  - `set_gravity_block_mass.py` —— 只改 `DA_GS_Block_Gravity` 一个资产(**首选**,别跑全量 `generate_data_assets.py`,那会重写所有 Profile)。
  - `set_movable_blocks_mass.py` —— 对**当前打开的关卡**批量处理:筛选 `bStartSimulatingPhysics && bAffectedByGravity && !bCanBreakTargets`,设 5000 并 `apply_current_configuration()`,存关卡。
- **已知限制/待办**:
  - 质量 5000 在 PhysX 安全区(<1e6),高帧率骤降时重物堆叠可能偶尔微陷,可接受。
  - 小球撞 5000kg 块时**自身会反弹**——物理实心语义的正常表现。
  - “同一 Profile 全体变重”:若要逐块精细控制可推性,需复制 Profile 或用编辑器逐实例覆盖。
  - **PIE 验证待编辑器执行**(本轮编辑器未开):①球 1600cm/s 撞块纹丝不动;②G 翻转块正常下落/翻转;③球仍可在块上滚动/被挡。同步的 C++ 落地改动(网格三带 10/20/10 + 静落归零,§19.9 未单列)尚未编译验证,下次进编辑器一起做。
- **更新(19:40,UBT 重编译通过)**:`ZFlipEditor Win64 Development` 增量编译成功(27s,`GSLandingResponseComponent.cpp` + 模块重链,DLL mtime 19:39)。编辑器重启后经 MCP 读 CDO 确认:**QuietLandingMaxCells=10.0 / GravityReverseMinCells=20.0 / BounceToHeightCells=10.0** —— 改动 1+改动 3 已实际进运行态。剩余 PIE 三检(①块撞不动②G 翻转③球站立)与 §19.9 手感/分界(≤10 静落/10–20 反弹/≥20 反重力)待实测。

## 21. 2026-09-09 第十五轮:落地手感收敛——网格三带阈值重定(4/7/4→10/20/10)+ 静落归零 + 接触零回弹(Restitution=0)

- **目标/根因链(实测定位)**:落地带旧值 4/7/4 太激进,普通小落差就进反弹带;且球落定后总"ride on residual normal speed"+ 求解器接触微弹(引擎默认 restitution **0.3**,`PhysicalMaterial.cpp:47`,球无材质覆盖 → 每次接触按 0.3 弹)让球带一会上跳(GROUNDED_RISING 追踪)。逐层往下拆:先代码层 QUIET-ZERO + settle guard 压制(打地鼠),spawn/接触残余上跳仍压不住 → 怀疑到求解器 restitution → 本轮直接归零,源头断掉。**三改动是一条链:阈值定"该不该动",零回弹定"动了之后落定即稳",guard 从主治降为纵深**。
- **改动 1:网格三带阈值 4/7/4 → 10/20/10(落地弹跳与反重力阈值,`GSLandingResponseComponent.h` 默认值)**
  - `QuietLandingMaxCells` **4→10**:impact ≤ v(10 格)静落,不做任何响应;
  - `GravityReverseMinCells` **7→20**:impact ≥ v(20 格)自动反重力;
  - `BounceToHeightCells` **4→10**:介于两者间(10–20 格落差)落地 → 以 v(10 格)速弹回原高。
  - 阈值动态推导:v(cells)=√(2·g·cells·cell),g=1600×球 GravityScale、cell=100cm → 实测静落 <1789cm/s、反重力 >2482cm/s、之间 1789cm/s 弹回(LAND diag 数值即此)。
  - **反重力下沿内收 0.75 格**:帧率掉时下落采样比真实低百分之几,精确 20 格落地的 v 会落在采样噪声内;内收 0.75 格让"正好 20 格"判定果断、19 格(v19 采样永不过冲)仍弹回。
- **改动 2:静落归零 + 0.4s settle guard**(`GSLandingResponseComponent.cpp/.h`)
  - 静落带落地瞬间把**重力轴速度归零、保留切向**(v_after 只留水平)——否则球带残余法向速度"ride"一下再停;
  - 随后 0.4s 窗口内(仍 probe 支持)把任何把球带离表面的重力轴运动压回静止噪声底(18cm/s)——治"刚落稳被求解器微顶一下"的可见小跳;窗口制 → 弹跳带/反重力的合法离地永不误压。旧注释的 4/7/4 语义一并更新成 10/20/10。
- **改动 3:接触零回弹 Restitution=0**(`GSRollingBallPawn.cpp:36`,唯一源文件)
  - `NewObject<UPhysicalMaterial>(Transient,"GSBallZeroRestitution")` → Restitution=0 + **bOverrideRestitutionCombineMode=true + CombineMode=Min**,挂 `BallCollision->SetPhysMaterialOverride`。
  - **Min 合并是必须**:项目默认 Average 会把(球0+地面0.3)/2=0.15 消不干净;Min 让 Min(0,任意表面)=0,**球对任何表面恒不回弹,不用动地面/块材质**;摩擦 0.7 不动,手感不变。纯运行时 NewObject,无内容资产、零打包依赖。
  - 代码弹跳全部直接写线速度(BOUNCE/QUIET-ZERO/reverse),不经 restitution → 蹦床/反重力照常。include 走 5.8 新路径 `PhysicalMaterials/PhysicalMaterial.h`(旧 `PhysicsEngine/` 已删)。
- **改动文件清单(落地批次;块质量属 §20,同在工作区未 commit 需区分)**:
  - 代码:`GSLandingResponseComponent.h/.cpp`(改动1+2 + TEMP-DIAG)、`GSRollingBallPawn.cpp`(改动3)。
  - §20 另计:`DA_GS_Block_Gravity.uasset`+`generate_data_assets.py`(质量 40→5000)、新增两块质量脚本、`测试案例.umap`(临时摆场,入库前清理)。
  - `[GSLandDiag]` TEMP-DIAG(LAND/QUIET-ZERO/BOUNCE/GROUNDED_RISING)是追踪残留微弹的**临时日志,确认后必摘**。
- **PIE 实测(2026-09-09 晚;改动3 已编译,DLL mtime 21:18:33,编辑器重启读进程 Module 确认加载新模块)**:
  - ✅ 球 spawn 后、每次受扰后都回**精确静止**(loc 49.50,vel 0.000×3),无发散/持续弹跳。
  - ⚠️ **spawn 瞬态仍在**:开局 ~66cm 衰减跳(LAND impact 461→435→212,~7s 归零)——restitution=0 下仍发生 → **不是 restitution**,是球生成吃地 0.5cm(中心 z=49.5,半径 50)的穿透恢复;想消:让球以中心 z=半径 静止高度生成。
  - ⚠️ 脚本注入速度测"干净落地"不可靠(python 暂停世界设速→恢复 + CCD 非弹道瞬态)→ 手感项留实机。
  - ❓ §20 三检(球撞 5000kg 块不动 / G 翻转块下落 / 球站块上)仍未做,编辑器已开可顺手补。
- **已知限制/待办(给下一个 AI/队友)**:
  1. §20 PIE 三检 + 本批实机手感三档(≤10 格落定即稳 / 10–20 格弹回 / ≥20 格反重力)盖章。
  2. 实机确认微弹消失后:摘 `[GSLandDiag]` 4 处 TEMP-DIAG;`QuietSettleGuard`+QUIET-ZERO(改动2)降为纵深,留可、删亦可。
  3. spawn 穿透瞬态若碍眼 → 生成高度修正,独立于本批。
  4. §20+§21 全部未 commit;提交时按"块质量数据 / 落地手感代码"拆两个 commit 更清晰。

## 22. 2026-09-11 第十四轮:转向器(滑梯式重力转向)系统——双向(新功能,PIE 正反 8 例全过)

- **需求**(用户):不再靠 G 直接改重力;关卡里加了 4 个"转向器"(方块+圆柱布尔出的滑梯),球碰到就自动滑过去、滑到它连接的墙面上,重力随之改变——视觉上像小球通过滑梯滑进另一个面。**且必须双向生效**(第二版反馈:从墙上滚下来要能滑回地面)。
- **关卡布局**(测试案例,x≈800、y∈[2400,3100]、z∈[0,900] 的槽里):转向器2 地面↔y=2400 墙,转向器3 y=2400 墙↔天花板,转向器1 地面↔y=3100 墙,转向器4 y=3100 墙↔天花板。四件共用同一布尔网格 `Boolean_7E5C5321`(沿 X 挤出的 90° 弯道),实例旋转不同;地面↔墙两件互成镜像,另两件 roll 180。地面→墙→天花板→……构成闭环,可正可反。
- **几何结论(实测)**:网格资产**无简单碰撞体**(box/sphere/convex 全 0),球原本直接穿过去;把资产改成 **CTF_USE_COMPLEX_AS_SIMPLE**(复杂碰撞当简单用,三角面直接参与物理)后球才真被滑梯接住——4 个实例共用资产,一次改全生效。
- **实现(C++ 三件套,含 2 个新文件)**:
  1. `UGSRedirectorComponent`(新,挂到 4 个 StaticMeshActor 实例上):**双向靠"两个面"建模**——`GravityDirectionA/B`(必须互相垂直)是转向器连接的两面;**入口面由球当前重力自动识别**(与哪面更近就是哪面,容差 `EntryFaceGravityMin=0.5`),出口就是另一面。90° 弯道下"入口行进方向恒等于出口重力方向",所以**进入判定(速度·出口方向≥0.35)一条规则天然覆盖正反两个方向**,同时排除从背面/天花板误触发;最小触发速度 80(静止球不触发);滑行中每帧判定,球离开触发盒才释放。
  2. `AGSRollingBallPawn` 新增"定向重力过渡"状态机(`BeginGravityRedirect/EndGravityRedirect`):重力按**滑行距离**(不是时间)旋转——球滚得快就转得快,永不"球还在坡上重力已转完";滑行速度锁在**弯道当前切向 `forward = cross(当前上, 弯道轴)`**(轴 = `cross(入口上, 出口上)`,反向时自动反向);每帧沿 -Up 打**球面探针**取实际接触面,指令速度投影到切平面 + 按空隙补法向吸附速度(布尔网格实际形状说了算,球贴着滑梯面走);旋转走完即提交管理器(方向已一致,无跳变);滑行期抑制 WASD/刹车/G/1-2-3,并让落地响应与自动反转让位(防半路弹跳/反转)。
  3. `UGSGravityBodyComponent` 加 `GravityDirectionOverride`(非零时代替管理器方向)→ 物理重力、相机抬升、驱动平面读同一份"旋转中的重力",全程连续。
- **PIE 实测(正 4 + 反 4,全过)**:

  | 方向 | 案例 | 起始 | 结果 |
  |---|---|---|---|
  | 正 | 1f | 地面 +Y 滚入 | 上 y=3100 墙,重力 +Y ✓ |
  | 正 | 2f | 地面 -Y 滚入 | 上 y=2400 墙,贴墙 ~443cm/s 爬升,重力 -Y ✓ |
  | 正 | 3f | y=2400 墙 +Z 爬入 | 上天花板,沿 +Y 滑出,重力 +Z ✓ |
  | 正 | 4f | y=3100 墙 +Z 爬入 | 上天花板,沿 -Y 滑出,重力 +Z ✓ |
  | 反 | 1r | y=3100 墙 -Z 下滑 | 回地面,沿 -Y 滚出,重力 -Z ✓ |
  | 反 | 2r | y=2400 墙 -Z 下滑 | 回地面,沿 +Y 滚出,重力 -Z ✓ |
  | 反 | 3r | 天花板 -Y 滚入 | 上 y=2400 墙,沿墙向下滑出,重力 -Y ✓ |
  | 反 | 4r | 天花板 +Y 滚入 | 上 y=3100 墙,沿墙向下滑出,重力 +Y ✓ |

- **调试开关(默认关)**:Pawn `bRedirectDebugLog`(逐帧球位/实际速度/指令速度/接触面/空隙/重力上/进度 → `Saved/Logs/z-flip.log` 的 `[GSRedirect]`);组件 `bDebugLog`(armed/fired/released,含 A→B 方向)。
- **踩坑(重要,已反哺 skill)**:
  - **单向建模是坑**:只按"出口方向"建模的设备天然只能单向;正确姿势是"连接的两个面",入口由球当前重力识别、出口取另一面——判定规则一条覆盖双向(`cross(入口上,出口上)` 的轴会自动反向)。
  - **远程 python 注入的速度活不过暂停帧**:恢复帧的巨大 dt 会把 `exp(-3·dt)` 松键刹车一次抹平;更隐蔽的是**自动驱动的方向如果和注入速度不同向,巨大 dt 会把速度方向直接盖成驱动方向**(判定失败、看着像"没触发")。测反向用例时把 `DriveAccelerationCm` 调到 100 即可。
  - **CDO 修改被引擎安全层拦**:`get_default_object()` 写入直接报 "Blocked unsafe Python code: get_default_object() modification"(这也解释了更早一轮"CDO 改了 PIE 实例不生效"的谜团)。开关一律在**实例**上设。
  - **相机航向是实例级残留状态**:自动驱动的方向 = 相机前向投影,多次用例之间航向会累加跑偏——要么按绝对角度重设(读 `camera_pivot.get_forward_vector()` 的水平分量反推当前航向,再用 `AddCameraLookInput(delta)` 校正),要么每轮重启 PIE。
  - 墙上爬升方向由 `SideSign = dot(重力, 相机右)` 决定,A/D 哪边是"上"随墙不同(测试用 `move_input=(±1,0)` 试一次)。
  - PIE 期间 `get_editor_world()` 拿不到游戏世界,用 `UnrealEditorSubsystem.get_game_world()`。
- **G 键仍在**(用户设计上改用转向器驱动;要彻底关:把 `FlipGravityKey` 设成未用键,或 `HandleFlipPressed` 直接 return)。
- **调参入口**:滑行速度上下限 `RideSpeedCm/MinEntrySpeedCm/MaxRideSpeedCm`、弯道弧长 `RidePathLengthCm`、触发提前量 `TriggerInflateCm`、贴面吸附 `SurfaceFollowRangeCm/SurfaceFollowGain`(组件/Pawn 上的 UPROPERTY)。

## 23. 2026-09-11 第十五轮:转向器两项手感修正(碰到圆弧才转 + 反向可靠)

- **用户反馈**:①"应该是碰到圆弧才会转过去上墙,不然手感很诡异"(之前触发盒外扩 120cm,球还在半空就被旋转重力);②"有些地方还是只能单向换重力,没法逆过去把重力换回来"(反向进入常被拒)。
- **修 1:必须真碰到圆弧才触发**(`UGSRedirectorComponent::IsBallTouchingChute`)。
  - 判据:从球心沿三个方向(朝网格包围盒最近点 / 入口"下" / 出口"下")打**线探针**,命中自身网格且距离 ≤ 球半径+`ContactTouchMarginCm`(默认 20)才算碰到。实测正向进入时**正好在球碰到坡面那一帧**触发(日志:fire at ball=(809,2599,62),球心 z=62 = 贴着坡脚)。
  - **踩过的坑**:先用**球面扫掠**做接触判定——球静止在地面时扫掠起点已与地面重叠,扫掠立刻返回地面、永远打不到滑梯(实测所有触发全部失效);改用线探针(从球心出发不与地面重叠)解决。`GetClosestPointOnCollision` 对"复杂碰撞当简单用"的网格**返回 -1**(文档:必须 simple collision),不可用。
- **修 2:贴住滑梯时放宽速度/方向门**(反向可靠的关键)。反向(从墙上/天花板上滑下来)时,球常被滑梯几何**顶停在坡面上**(速度掉到几十),旧逻辑被"最小触发速度 50"和"方向 align≥0.35"两道门拒掉 → 表现为"逆不回去"。现在:**贴住滑梯**(接触为真)时速度门不生效、方向门放宽到 -0.5(只要不是明显往外走就带走);没贴住时仍按原门限。最小触发速度默认降到 50,冷却 0.3s。
- **修 3:释放条件 = 球离开触发盒 **且** 重力旋转已走完**(`GetGravityRedirectProgress()>=1`)。旋转没走完就释放会在"两个面之间"留下一个重力,看着像没转过去。弧长参数相应从 320 调到 260(碰到才触发,弧长≈弯道本身长度)。
- **PIE 实测(自动按转向器实际位置定位)**:
  | 用例 | 起始 | 结果 |
  |---|---|---|
  | 2f 正向 | 地面 -Y 滚入 | 在坡脚(z=62)触发,-Z→-Y,上墙 ✓ |
  | 2r 反向 | -Y 墙 z=400 下滑 | 在 z=197 触发,-Y→-Z,滑回地面 ✓ |
  | 3r 反向 | 天花板 -Y 滚入 | 在 (2592,833) 触发,+Z→-Y,落墙后沿墙下滑 ✓ |
- **调试**:组件 `bDebugLog` 打开后,球在触发盒内**每帧**打印各门限实测值(`gate[speed|face|align|touch]` + 球位/速度/align/touch),另有每 30 帧心跳(`tick ball=... inBox=... riding=...`)确认 tick 在跑——定位"为什么不触发"一次到位。
- **测试方法坑(重要)**:
  - **本关是导轨相机**:`move_input`/自动驱动的方向由固定导轨决定、且随球沿导轨的位置变化(实测同一 move_input 在不同位置驱动方向不同)→ 测试**不要**依赖 move_input 方向;用"**注入世界速度 + 极小驱动力(50)只为让松键刹车失效**"的方式驱动球,并配合**反复重设速度**(每次脚本调用之间世界推进约 0.4s)模拟持续驱动。
  - **用例之间会串场**:上一个滑行(状态机)没释放就放球,新的重力设置会被旧的 `GravityRedirectCurrent` 覆盖 → 用例间留足时间(等球离开触发盒)或重启 PIE。
  - 转向器被拖动时**只沿 X 平移是无害的**(滑梯是沿 X 挤出的,仍在同一墙角);测试坐标应按**actor 实际位置**推导,不要写死。
- **关卡现状提醒**:转向器1 现位于 (550,3010,100)、转向器3 位于 (1120,2500,800)(相比初始摆放沿 X 移动过;y/z 仍在墙角上,功能不受影响)。

## 24. 2026-09-11 第十六轮:转向器"撞侧面不触发"(正面圆弧接地那块才触发)

- **用户反馈**:「撞到侧面还是会动 撞到侧面不要移动 只有从正面圆弧上去才会动 …… 只有碰到正面的圆弧接地的那一块才会触发转向判定」。
- **三个新增判定(顺序:接触 → 侧壁 → 接地 → 速度 → 方向)**:
  1. **接触**(上一轮已有的三方向线探针,顺带回传接触面法线)。
  2. **撞侧面不触发**:`|dot(接触面法线, 弯道轴)| > MaxLateralNormalDot(0.7)` → 撞的是滑梯**侧壁**(平直面,法线沿滑梯宽度方向),不是正面圆弧,直接不触发。**这条是根治**——之前只看速度方向,球撞侧面**弹开后速度一转**就又被放行了(实测:先 `gate[side]` 拒了两次,弹开后 align≈0 却放行触发了)。看法线而不是看速度,球只要还贴着侧壁就永远不触发,不依赖它的瞬时速度。
  3. **"正面接地那块"**:球心到**入口侧面**(地面/墙/天花板那一侧的面)的距离 ≤ 球半径 + `EntryLipBandCm(60)`。入口侧面 = 网格包围盒朝入口重力方向那一侧的支撑面。**符号坑**:写成 `Center - Sign(N)*Extent` 会取到对面(顶面),实测所有正常触发全被 `gate[lip]` 拒掉;正确是 `Center + Sign(N)*Extent`(入口重力朝哪边就取哪边)。
  4. **速度必须大体朝出口**:`align(速度,出口方向) ≥ EntryAlignmentMin(0.35)`;只有**速度 < SlowEntrySpeedCm(100)** 的球豁免(反向下滑常被滑梯顶停在坡面,速度方向已无意义)。
- **PIE 实测(4 参数已同步到关卡四个实例并保存)**:
  | 用例 | 结果 |
  |---|---|
  | 侧面撞击 ×2(地面沿 ±X 撞滑梯侧壁) | **零触发** ✓(41 次 `gate[side]` 拒绝) |
  | 2f 正向(地面 → -Y 墙) | 在坡脚 (814,2599,62) 触发 ✓ |
  | 2r 反向(-Y 墙 → 地面) | 触发,-Y→-Z,滑回地面 ✓ |
  | 3r 反向(天花板 → -Y 墙) | 触发,+Z→-Y,落墙下滑 ✓ |
- **测试脚本坑(本轮踩到)**:`_tmp_drive.py`(持续驱动用)在上一轮收尾时被 `rm -f _tmp_*` 清掉,导致后续几轮"球没被驱动、正常用例没触发",差点误判成代码回归。**测试脚本要单独放一个目录别混在临时清理里**。
- **当前参数**(四实例已保存):minV=20、lip=60、normalDot=0.7、align=0.35、slow=100、touch=20、path=260、cooldown=0.3、debug_log=False。

---

## 25. 2026-09-11 第十七轮:相机俯仰反转修复 + 重力区域检测器系统(编译通过 + PIE 7/7 全过 ✅)

**交付物**:`GSGravityZones.h/.cpp`(两个新类)+ `GSRollingBallPawn.cpp` 一行符号修正。UBT 编译通过,无 warning。

### 25.1 相机俯仰反向(已修,待 PIE 验收)

- **用户反馈**:鼠标向上 → 摄像机向下,上下反了。
- **根因**:`GSRollingBallPawn.cpp:786` 调用点写的是 `AddCameraLookInput(MouseX * YawPerUnit, -MouseY * PitchPerUnit)` —— **只有 Pitch 取了负号,Yaw 没有**。符号链:UE 鼠标 delta Y 上抬为正 → `-MouseY` 为负 → `CameraPitchDegrees` 变小;而 `BuildCameraRotation`(`:443`,其中 `:467` 是 `PitchQuat = FQuat(Right, radians(-TotalPitch))`)里推导得 **`CameraPitchDegrees` 为正 = 抬头**。于是负增量 = 低头,与上抬鼠标相反。
- **修复**:去掉那个负号(调用点,不是函数内部)。
- **为什么改调用点而不是 `AddCameraLookInput` 内部**:①该函数只有一个调用点,两处等价;②它是 `UFUNCTION(BlueprintCallable)`,在函数内取反会让蓝图/未来调用方传正值得到"低头",把反号藏进 API 语义里。改调用点是一字之差,且 Yaw/Pitch 在那里恢复对称。
- **注意**:`AddCameraLookInput` 只在**非导轨相机**时被调用(`:779` 的 `if (!RailCamera || !RailCamera->IsDriving())`)。导轨关卡里鼠标俯仰本来就不生效——若在导轨关验收不到变化,是这条守卫,不是修复没生效。

### 25.2 重力区域检测器(新功能,编译通过 + PIE 7/7 全过)

- **设计**:全场方块平时不受重力;球穿过门口检测器 → 全图禁用 + 只激活球**要去的那个区域**。检测器双向:朝前向穿过激活 `ZoneB_Blocks`,反向激活 `ZoneA_Blocks`。
- **两个新类**(`Public/GSGravityZones.h`):
  - `AGSGravityZoneManager` —— `SetActiveZone/DisableAllGravity/ResetAllZones/OnResetWorld/FindZoneManager`。`SetActiveZone` = 全图 `SetAffectedByGravity(false)` 后逐个打开传入列表。
  - `AGSGravityDetector` —— `UBoxComponent TriggerPlane`(Trigger profile,10cm 厚半尺寸 5)+ `UStaticMeshComponent FlashMesh`(编辑器可见/游戏隐身,触发亮 `FlashDuration` 秒)。触发门:速度 ≥ `MinTriggerSpeed`(50) ∧ `|dot(速度,前向)| ≥ MinDirectionalDot`(0.2)。
- **复用现有接口,核心逻辑零改动**:走的是已有的 `AGSBlockBase::SetAffectedByGravity`(`GSBlockBase.cpp:113`,内部即 `GravityBody->SetGravityEnabled`),没碰 `AGSBlockBase`/`AGSGravityManager`/`AGSRollingBallPawn` 的核心。
- **两个实现坑(都是顺序/类型问题,已解)**:
  1. **UHT 不收 `TObjectPtr` 当 UFUNCTION 参数** → `SetActiveZone` 参数和两个 Zone 数组用裸 `TArray<AGSBlockBase*>`(UHT 报 `UFunctions cannot take a TObjectPtr as a function parameter`)。
  2. **Actor BeginPlay 顺序不保证** → 管理器的"开局禁用"若在 `BeginPlay` 里立即执行,会被**后跑的方块 BeginPlay** 用它序列化的 `bAffectedByGravity` 顶掉(`GSBlockBase::BeginPlay` → `ApplyCurrentConfiguration` → `GravityBody->bGravityEnabled = bAffectedByGravity`)。改为 `SetTimerForNextTick` 延一帧,在所有 BeginPlay 之后执行。**与 §11 的"关卡 Actor BeginPlay 早于 GameMode 生成 Manager"是同一类顺序坑**。
- **行为边界**:最初的实现里"禁用重力"= 停止**施加**重力,方块会保留速度继续滑行。**该行为已在 §25.4 改为"瞬间定住"**——现在禁用 = 停重力 + 清零线/角速度。
- **前置条件(没做就看不到效果)**:关卡里必须放 1 个 `AGSGravityZoneManager`(不自动 spawn);方块必须勾 `Simulating Physics`,否则物理上根本不动。`AGSGravityDetector` 找不到管理器时**只闪灯**并打 Warning。
- **待办**:两个 Zone 数组**按需求留空**,由后续 AI 按空间位置批量填(约定见 `AgentSkill/gs-gravity-zone-assembly/SKILL.md` §5);与 `AGSWorldStateManager::ResetWorld` 的联动**未接线**(`OnResetWorld` 已留,等价 `ResetAllZones`)。

### 25.3 交接文档

- 新增 `AgentSkill/gs-gravity-zone-assembly/SKILL.md`:机制、前置条件(管理器/Simulating Physics/延一帧)、参数表、**已实跑的 7 条 PIE 结果**、后续 AI 填数组的空间判定约定。

### 25.4 禁用重力改为"瞬间定住" + 7 条 PIE 用例实跑

- **需求**:`SetAffectedByGravity(false)` 原本只停施力,已在下落的方块会带着速度滑行。改为禁用时**同时清零速度**,瞬间定住。
- **改动(3 处)**:
  1. `GSBlockBase.h/.cpp` —— 新增 `FreezeMotion()`(`BlueprintCallable`):对 `Mesh` 调 `SetPhysicsLinearVelocity(ZeroVector)` + `SetPhysicsAngularVelocityInDegrees(ZeroVector)`。
  2. `GSGravityZones.cpp` —— 新增私有静态 `AGSGravityZoneManager::DisableBlockGravity(AGSBlockBase*)` = `SetAffectedByGravity(false)` + `FreezeMotion()`。**所有"禁用"路径都收敛到这一个函数**,`DisableAllGravity()` 和 `SetActiveZone()` 都调它,不在多处各写一遍。
  3. `AGSGravityZoneManager::SetActiveZone` —— 由"先全图禁用再启用传入列表"改成**单次遍历**:先把传入列表收成 `TSet` 跳过,其余才禁用。否则正要激活的那批方块会被先冻一瞬,球在同一扇门前反复穿时区域方块会一顿一顿。
- **`AGSBlockBase` 核心逻辑未动**:只在类上加了一个新方法,`SetAffectedByGravity` / `AGSGravityManager` / `AGSRollingBallPawn` 核心零改动(相机那行除外,见 25.1)。
- **PIE 实测(2026-09-11,临时关卡 `_TempGravZone`,4 方块 + 1 管理器 + 1 检测器,已删除,`测试案例.umap` 未被污染)7/7 全过**:
  - 基线 4 方块全 `grav=False`、`z=300`、`|v|=0`;朝 +X 穿过 → ZoneB 转 `grav=True` 落至 `z=50`,ZoneA 不动。
  - 反向 −X → ZoneA 转 `grav=True` 下落,ZoneB 转 `grav=False`。
  - **侧向(dot≈0)不触发**:触发盒临时加宽到 (300,200,200) 保证球必然穿过,球 x 全程 ≈ −2、y 从 −1200 穿到 +844.8(y=149.3 时正在盒内、速度纯 +Y),全程不闪光不切区域 → 是 `MinDirectionalDot` 拦下的,不是"没撞上"。
  - 闪光:`flash_duration` 临时调 60s,触发后 `hidden_in_game` 由 True 变 False 并持续亮,跨两次读取成立。
  - **瞬间定住(关键项)**:ZoneB 正以 3052cm/s 下落(`z=7627`)时反向触发 → 立刻读到 `|v|=0.0`、`z=7017.0`;约 2s 游戏时间后再读 `z` **仍精确等于 7017.0** → 真冻住,不是滑行。
  - 同一时刻只有一个区域激活;`reset_all_zones()` → 4 方块全 `grav=False` 且 `|v|=0`,`reset_detector()` → 闪光重新隐身。
  - 额外确认:被冻住的方块重新激活后正常恢复下落(7017→6967→6193),"重新启用不需额外处理"成立。
- **遗留**:闪光只验证了显隐翻转,自定义发光材质的观感未验(材质由关卡/美术配)。
- **测试方法备忘**:PIE 阶段用 `unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()` 拿**游戏世界**;结束 PIE 是 `LevelEditorSubsystem.editor_request_end_play()`(**没有** `editor_end_play`;`end_play` 与 `load_level` 必须分两个脚本发,同脚本会死锁);相机偏航 `add_camera_look_input(delta, 0)` 是**相对量且方向为 +X→+Y→−X→−Y**,别假设一次 +90 就到 +Y。

### 25.5 本轮交付清单

| 文件 | 状态 | 内容 |
|---|---|---|
| `Public/GSGravityZones.h` / `Private/GSGravityZones.cpp` | **新增** | `AGSGravityZoneManager` + `AGSGravityDetector` 两个类(SKILL 手册所述的积木) |
| `Public/GSBlockBase.h` / `Private/GSBlockBase.cpp` | 改 | +`FreezeMotion()`(清线/角速度),其余零改动 |
| `Private/GSRollingBallPawn.cpp` | 改 | 相机 Pitch 去掉 `-MouseY` 的负号(1 行);核心逻辑未动 |
| `AgentSkill/gs-gravity-zone-assembly/SKILL.md` | 新增 | 关卡侧拼装手册:机制/朝向约定/前置条件/参数表/实测结果/后续填数组约定 |
| `README.md` | 改 | 功能积木清单 + 文档地图各一行 |
| `AgentSkill/ue-nocode/reference/ue_pyexec.py` | 改 | 修 bug:临时脚本路径原本硬编码队友机器的 `C:\Users\20625\...`,改 `tempfile.gettempdir()` |

**未动**:`AGSGravityManager`、`AGSRollingBallPawn` 的重力/移动核心、任何 umap。

### 25.6 后续待办

1. **两个 Zone 数组填引用**(`ZoneA_Blocks` / `ZoneB_Blocks`)**本轮按需求不做**——只留了数组接口,由后续 AI 按空间位置批量填,规则见 SKILL §5;填完**必须存盘**,数组是实例数据。
2. **与 `AGSWorldStateManager::ResetWorld` 接线**——`OnResetWorld()` 已留桩(等价 `ResetAllZones()`),尚未挂进死亡重置流程。
3. **闪光材质**——目前只有显隐翻转,自发光观感待关卡/美术配。
4. **相机俯仰修复的实机验收**——PIE 逻辑已验证,用户手感验收待做(注意导轨相机下俯仰本就不生效,见 25.1)。
