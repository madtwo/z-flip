# 白盒装配指南 — 把 GravityShift 系统放进你的关卡

适用:你已经有一关白盒几何(地板/墙/天花板/障碍),要在这个空间里跑重力翻转玩法。
范例:`Content/测试案例.umap`(棚子白盒 + 完整一套系统),照抄即可。

## 核心原则

1. **白盒几何保持静态网格**,碰撞必须有效(建模工具生成的网格记得补碰撞或切 ComplexAsSimple)
2. **静态壳永远不要加 GravityBody**——GravityBody 只上可动方块和球
3. 重力方向是**全屋全局**的(只有 ±Z 两态),Manager 是唯一权威
4. 每个可交互物体 = 一个 `GS*` 原生类实例(或其 BP 壳)+ 一个 Profile DataAsset

## 文件与命名规范(蓝图/资产放哪)

Content 目录按功能分域,**游戏积木资产(C++ 类的 BP 壳/DA)只进 `GravityShift/`,关卡白盒通用件只进 `LevelPrototyping/`**,别混:

| 目录 | 放什么 | 现有内容 |
|---|---|---|
| `Content/GravityShift/Core/` | 框架级 BP 壳(GameMode/Manager/Pawn) | BP_GSGravityGameMode、BP_GSRollingBallPawn 等 |
| `Content/GravityShift/Blocks/` | 方块类 BP 壳 | BP_GSBlockBase |
| `Content/GravityShift/Interactions/` | 交互积木 BP 壳 | 重力开关、表面/落地体积、SurfaceControllerDevice |
| `Content/GravityShift/World/` | 世界规则件 BP 壳 | Checkpoint、KillVolume、Collectible、FinishGoal |
| `Content/GravityShift/Data/Profiles/` | 全部 Profile DataAsset | DA_GS_*(Ball/Block/Break/Collectible/Gravity/Landing/Surface 共 12 个) |
| `Content/GravityShift/Tests/` | 一次性测试/演示 BP | BP_GSIntegratedDemoRoom |
| `Content/LevelPrototyping/` | 关卡白盒通用库(与插件无关) | SM_ 网格、M_/MI_ 材质、BP_JumpPad、BP_DoorFrame 等原型件 |
| `Content/` 根 | 关卡 umap | 测试案例.umap;新关卡多了以后建议建 `Content/Maps/` 按关卡放 |

命名前缀(照旧,新资产别发明新前缀):`BP_` 蓝图、`DA_` DataAsset、`SM_` 静态网格、`M_`/`MI_` 材质/实例、`T_` 贴图、关卡 umap 用中文名没关系(仓库已验证支持)。GS 前缀类 33 个都在 C++ 里,**BP 只是壳**——能直接摆 C++ 类的(拾取/钥匙/门/导轨等全部支持)就不必先建 BP。

## 不用摆的(自动生成)

| 系统 | 谁生成 |
|---|---|
| `GSGravityManager`(重力权威,全屋一个) | GameMode BeginPlay 自动 spawn |
| `GSWorldStateManager`(检查点/重置/收集) | 同上 |
| 滚球玩家 | GameMode DefaultPawn,在 **PlayerStart** 生成 → 关卡里必须有一个 PlayerStart |

⚠ 手动再摆一个 Manager 会出现双 Manager(日志警告,良性但别这么干)。
ℹ 例外——想给**本关**改默认重力方向或限制可用轴(如只许 Z、或开局就在墙上):摆**恰好一个** `GS World State Manager`(代码会识别已摆放的、不再自动生成),细节面板 `Gravity|LevelConfig` 里配 `DefaultGravityDirection` / `AllowedGravityAxes`。什么都不配 = 默认 −Z 落地 + 全轴可用,普通关卡不用管。

## 需要摆的(Place Actors 搜 "GS",或用 /Game/GravityShift 下的 BP 壳)

| 类 | 摆哪 | 关键配置(细节面板) |
|---|---|---|
| `GSBlockBase` | 每个可动方块 | `BlockProfile` 选 DA;`bStartSimulatingPhysics`(可动=true)/`bAffectedByGravity`=true;可破坏块再开 `bBreakable`+`bUseContinuousCollisionDetection` |
| `GSGravitySwitch` | 开关位 | `bTriggerOnOverlap=true`(球滚过触发);或关掉用 E 交互;`SwitchMode=TOGGLE` |
| `GSSurfaceModifierVolume` | 包住慢/加速表面 | `VolumeExtent` 贴合白盒表面(薄薄一层);`ProfileA`=DA_GS_Surface_Slow / Fast;重叠取优先级不叠加 |
| `GSLandingResponseVolume` | 特殊落地区 | 覆盖落点区域(弹跳增强/抑制) |
| `GSKillVolume` | **房间上、下边界外各一个** | `VolumeExtent` 覆盖整个房间截面——重力会翻转,上下漏出去都要能重置 |
| `GSCheckpoint` | 检查点 | 球碰到即记录复活点 |
| `GSCollectible` / `GSFinishGoal` | 收集物 / 终点 | 配 DA_GS_Collectible_Default |
| `GSPickupItem` | 想让玩家捡的任何东西 | `PickupMessage` 填提示文案;留空=静默拾取 |
| `GSKey` | 钥匙 | `KeyID` 填名字(如 `red`)——捡到即解锁全图同 ID 的门 |
| `GSDoor` | 锁住的门 | `RequiredKeyID` 填对应钥匙的 KeyID;`SlideOffset`/`SlideDuration` 定滑开动作 |
| `GSCameraRail` | **每段相机导轨一根**(不摆则该关回落到旧跟随相机) | 细节见下节「相机导轨」;测试案例已有一根 `相机导轨_GS` |

### 相机导轨(新式防晕相机,2026-09-03)

原理:相机像**套在钢筋上的小环**——沿 `GSCameraRail` 的本地 Z 轴滑动跟随球,在横切面里的位置全程锁死;视角以**世界竖直为滚转基准**、只做小幅万向调整(偏航 ≤35°/俯仰 ≤50°,可调),随球上墙/上天花板自动微调俯仰,**重力翻转不再滚转整个画面**。

摆法(每个新关卡):
1. Place Actors 搜 `GS Camera Rail` 摆进关卡,移到导轨线位置,**旋转 Actor 使其本地 Z 轴沿轨道方向**(摆放窗口里盯 Z 轴箭头)
2. `RailLength` = 轨道全长(中心对称,两端各一半);`bLookAlongNegativeAxis` 勾不勾决定相机朝 +Z 还是 −Z 看,**要朝房间内部看**
3. `CrossOffsetHeightCm`:0=相机正好骑在轴心(默认,推荐);>0 沿世界竖直抬高
4. 相机手感参数在**球身上的 `GSRailCameraComponent`**(偏航/俯仰限位、平滑速度、视线抬升 `AimOffsetUpCm` 等),全关通用,一般不用动
5. 多段导轨直接首尾相接摆即可——组件自动选最近导轨、带 150cm 迟滞防抖,球出了所有导轨范围自动回落旧跟随相机

测试案例现状:圆柱体参考件 `相机导轨`(无碰撞/游戏不显示)保留作视觉参照,旁边已摆好对应的 `相机导轨_GS`(轴线沿 X、长 6293、朝 −X 看进房间)。

方块 Profile 三配方:`DA_GS_Block_Fixed`(定死)/`DA_GS_Block_Gravity`(随重力)/`DA_GS_Block_GravityBreaker`(撞击破坏者,CCD 已开)。赋完 Profile 调 `ApplyBlockProfile`(细节面板按钮或脚本调)生效。

## 拾取物 / 钥匙 / 门(F 交互链,2026-09-05 实测)

三个 C++ 原生类,Place Actors 搜 `GS Pickup` / `GS Key` / `GS Key Door` 直接拖,**不需要先建 BP**;做变体才右键类→创建 Blueprint 子类。交互判定与门/开关一样是**无碰撞距离判定**:球心距目标 ≤320cm(球 Pawn 的 `InteractionRadiusCm`)就能 F。

**最小配对流程(积木拼法)**:
1. 摆一个 `GS Key`,细节面板 `KeyID` 填 `red`(随意起,同一把钥匙可配多扇门,多把钥匙也可同 ID)
2. 摆一个 `GS Key Door`,`RequiredKeyID` 填 `red`——KeyID 与 RequiredKeyID 相符即成一对,**没有任何中央注册表,改名字就是换锁**
3. (可选)摆 `GS Pickup Item` 当普通拾取物,`PickupMessage` 填屏幕中央那句提示

**行为细则**(都是默认值,细节面板可调):
- 走近按 **F** 拾取:物品消失 + `PickupMessage` 显示在屏幕中央 + **输入临时锁住**(球不被冻结,只是没有输入、靠物理滑停),按**空格**继续;`PickupMessage` 留空 = 静默拾取,不锁屏
- 钥匙捡到**瞬间**,全图所有 `RequiredKeyID` 匹配的门一起开始滑开:门板从原位滑向 `SlideOffset`(默认向上 240),用时 `SlideDuration`(默认 0.8s);对准门按 F 只会弹 `LockedMessage` 提示,不消耗任何东西
- 拾取物不 Destroy 只隐藏(碰撞也关掉),所以**世界重置(R/死亡/KillVolume)后全部复原**:门回锁滑回、钥匙/拾取物重新出现、若重置时提示还挂在屏上会自动解除,不会卡死在提示界面
- 给拾取物换个好看样子:细节面板替换 `Mesh` 的静态网格即可(默认 40cm 立方;门默认 160×30×220 门板,同样可换)

本轮实测记录(编辑器摆件→PIE):拾取→`is_collected=True`+锁屏文案正确→空格解锁→拾 `red` 钥匙→门 `is_locked=False`→0.8s 后门板相对位置=SlideOffset(0,0,240)→`ResetWorld` 后门回锁/拾取物与钥匙全部重武装 ✅

## 落地三带(网格联动)与重力砸碎(2026-09-04,队友交付)

**落地三带**自动生效,不用摆任何东西:球从不同高度落到地面,按**网格格数**(不是 cm/s)分三档——≤4格安静、5–6格反弹一次(弹回 4 格高)、≥7格反重力翻转。阈值按 `v(格)=√(2·g·格·cell)` 实时推导(g 取 GravityManager×GravityScale),所以改重力强度/网格尺寸时三带自动跟手。想调格数或换算基准:球组件 `GS Landing Response` → `GravityShift|Landing|Grid`(`QuietLandingMaxCells`/`GravityReverseMinCells`/`BounceToHeightCells`/`GridCellSizeCm`);某块落点要特殊规则就摆 `GS Landing Response Volume` 覆盖(它的显式 cm/s 设定优先级最高)。

**重力砸碎**也自动生效:破坏者方块(`DA_GS_Block_GravityBreaker`,即 `bCanBreakTargets`)真实砸到可破坏块(挂 `GS Breakable` 组件 + `DA_GS_Break_Fragile`)时按动能 `½mv²` 击碎。注意破坏者的速度是 GS 重力体逐帧驱动的,收不到引擎 Hit 事件——冲击由组件 Tick 自检(峰值速度追踪+落停判定),所以**别把破坏者的 GSGravityBody 拆掉**,拆了就不砸了。

## 标签约定(为自动化绑定留的口)

给每个标记 Actor:Label = `WB_房间_类型_编号`(如 `WB_R01_Switch_1`),Tag 加 `GS_TYPE_GRAVITY_SWITCH` / `GS_TYPE_GRAVITY_BLOCK` / `GS_TYPE_SURFACE_SLOW` 等。规范全文见文档包 `design/WHITEBOX_BINDING_MATRIX_v5.csv`(D:\下载\GravityShift_ZFlip_RollingBall_DocumentPack_v5)。当前仓库**未包含**自动 scan/apply 脚本(v6 计划),现阶段手动摆放即可。

## Python 批量摆法(可选)

脚本已就位:`Plugins/GravityShift/Content/Python/v5/`(install_blueprints.py 建 14 个 BP 壳;generate_data_assets.py 建 12 个 DA,先跑这两个)。摆放示例:

```python
import unreal
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
P = '/Game/GravityShift/Data/Profiles/'
da = unreal.load_asset(P + 'DA_GS_Block_Gravity')

b = actors.spawn_actor_from_class(unreal.GSBlockBase, unreal.Vector(800, 0, 300))
b.set_actor_label('WB_R01_Block_1')
b.set_editor_property('block_profile', da)
b.apply_block_profile(da)

kv = actors.spawn_actor_from_class(unreal.GSKillVolume, unreal.Vector(0, 0, -450))
kv.set_volume_extent(unreal.Vector(12000, 12000, 300))
```

摆完 `unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()`。

## 验证清单

1. PIE 无编译错误/Accessed None(日志:`Saved/Logs/`)
2. 球从 PlayerStart 落地;WASD 方向与相机一致;松键 ~0.5s 停
3. G:球+可动方块直上直下贴住天花板(无翻滚);摄像机 180° 跟转;再按 G 回来
4. 中高落差:弹一次就稳(不无限弹)
5. (配了 KillVolume)掉出边界自动重置
6. (配了钥匙/门)拾钥匙门滑开;R 重置后门回锁、拾取物复原
7. 最终让真人玩一遍——按键手感只有人能判断
