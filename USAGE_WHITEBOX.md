# 白盒装配指南 — 把 GravityShift 系统放进你的关卡

适用:你已经有一关白盒几何(地板/墙/天花板/障碍),要在这个空间里跑重力翻转玩法。
范例:`Content/测试案例.umap`(棚子白盒 + 完整一套系统),照抄即可。

## 核心原则

1. **白盒几何保持静态网格**,碰撞必须有效(建模工具生成的网格记得补碰撞或切 ComplexAsSimple)
2. **静态壳永远不要加 GravityBody**——GravityBody 只上可动方块和球
3. 重力方向是**全屋全局**的(只有 ±Z 两态),Manager 是唯一权威
4. 每个可交互物体 = 一个 `GS*` 原生类实例(或其 BP 壳)+ 一个 Profile DataAsset

## 不用摆的(自动生成)

| 系统 | 谁生成 |
|---|---|
| `GSGravityManager`(重力权威,全屋一个) | GameMode BeginPlay 自动 spawn |
| `GSWorldStateManager`(检查点/重置/收集) | 同上 |
| 滚球玩家 | GameMode DefaultPawn,在 **PlayerStart** 生成 → 关卡里必须有一个 PlayerStart |

⚠ 手动再摆一个 Manager 会出现双 Manager(日志警告,良性但别这么干)。

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
6. 最终让真人玩一遍——按键手感只有人能判断
