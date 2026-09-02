# GravityShift v5 — 进度同步 / 交接文档（z-flip 项目）

> 更新时间：2026-09-02 19:2x (GMT+8)
> 状态：**v5 系统性验收通过（API 级），待用户实机按键游玩验收**
> 写这份文档的目的：先把做到哪、卡在哪、改了什么、踩了什么雷同步清楚，供人工诊断。

---

## 0. 一句话现状

v5 双向 Z 重力滚球系统**已在 z-flip 编译、安装、搭建并 PIE 实测通过**：G 翻转(ACCEPTED/rev++)、球体升空、摄像机 180° slerp 跟转、棚顶撞击触发 FALL_THRESHOLD 自动反向、reset 语义正确、三方块按 Profile 就位。本轮修了 2 个真 bug（弹簧臂滤掉翻转、睡眠刚体不响应翻转）。**剩余：用户按 WASD+G+R 实机游玩验收（摇动力矩路径），以及 git 同步。**本轮详细记录见 §8。

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

**git**:z-flip 尚未建仓;如需同步 GitHub,MyProject2 的 REST 直传脚本可直接改 REPO/WORKDIR 复用。

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
