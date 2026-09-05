# GravityShift z-flip — UE 5.8 双向重力滚球

双极性/多向重力 + 物理滚球主角的玩法框架。引擎 **Unreal Engine 5.8.2**,核心逻辑全部在原生 C++ 插件 `Plugins/GravityShift`(33 个原生类),Blueprint 只做资产壳,没有任何蓝图 Tick/事件图重力实现。

## 操作

| 键 | 作用 |
|---|---|
| WASD | 滚球(相机相对;贴墙时 W/S 沿墙前后、A/D 爬升/降落) |
| G | 重力翻转(小球+可动方块一起翻;轨相机只调俯仰不滚转) |
| 1 / 2 / 3 | 重力吸到 X / Y / Z 轴正方向(受关卡允许轴限制) |
| Q / E | 相机拉远/拉近(导轨相机跟在球后面的距离,步长 50,默认 700,范围 300–1400) |
| F | 交互:拾取物品/钥匙、对锁住的门看提示(重力开关通常滚过即触发) |
| 空格 | 拾取提示占满屏幕中央时按空格继续(输入临时锁住,球自然滑停) |
| R | 重置(门回锁、拾取物/钥匙复原、锁屏解除) |

> 相机说明:测试关卡使用**导轨相机**(相机套在 `GSCameraRail` 轨道上跟随,防晕设计)。没有导轨的关卡自动回落为旧跟随相机(鼠标可转视角)。

## 快速开始

1. 打开 `z-flip.uproject`(克隆后首次打开右键 uproject → 重编译,约十几秒;启动地图即 `Content/测试案例.umap`)
2. 按 Play:球在棚子里,直接 WASD/G 玩
3. 测试场景已配好:重力/破坏/静态三种方块、重力开关、慢速表面区、上下边界 KillVolume(重置用)、相机导轨
4. 拾取物/钥匙/门等交互积木不在测试图里预摆——照下面「功能积木清单」+ USAGE_WHITEBOX 自己拼,2 分钟的事

## 功能积木清单(关卡策划视角)

所有积木都是**原生 C++ 类,Place Actors 面板搜 "GS" 直接拖进关卡**,在细节面板调参数即可;要做变体就右键该类「创建 Blueprint 子类」。摆放/配置细节统一见 `USAGE_WHITEBOX.md`。

| 积木(Place Actors 搜) | 干什么 | 怎么配对/触发 |
|---|---|---|
| `GS Pickup Item` | 可拾取物品,F 捡起,屏幕中央弹提示并锁输入,空格继续 | `PickupMessage` 留空=静默拾取不锁屏 |
| `GS Key` | 钥匙(是拾取物的子类,行为同上) | `KeyID` 填个名字(如 `red`),捡到瞬间全图匹配的门一起开 |
| `GS Key Door` | 锁住的门,挡路;被匹配钥匙打开后门板滑开 | `RequiredKeyID` 填对应钥匙的 KeyID;`SlideOffset` 定滑动方向/距离,`SlideDuration` 定时长 |
| `GS Gravity Switch` | 重力开关 | 默认球滚过即触发(`bTriggerOnOverlap`);也可 F 按 |
| `GS Surface Modifier Volume` | 表面区(慢速/加速) | `ProfileA` 选 DA_GS_Surface_Slow/Fast,`VolumeExtent` 贴住表面 |
| `GS Landing Response Volume` | 特殊落地区(增强/抑制反弹) | 摆在落点,覆盖球身上的落地三带设置 |
| `GS Block Base` + BlockProfile | 可动方块(随重力/破坏者) | 细节面板配 DA(`DA_GS_Block_Gravity` 等),再点 `ApplyBlockProfile` |
| `GS Kill Volume` | 出界重置 | 房间上下边界外各一个(重力会翻转) |
| `GS Checkpoint` / `GS Collectible` / `GS Finish Goal` | 检查点 / 收集物 / 终点 | 摆上即用 |
| `GS Camera Rail` | 导轨相机轨道(防晕;不摆=旧跟随相机) | 本地 Z=轨道方向,`RailLength`=全长;详见 USAGE_WHITEBOX「相机导轨」 |

自动挂在球身上、不用摆的组件(细节面板可调):`GS Gravity Body`(重力受力)、`GS Landing Response`(**落地三带**:按网格自动判 ≤4格安静 / 5–6格反弹到4格 / ≥7格反重力,参数分组 `GravityShift|Landing|Grid`)、`GS Rail Camera`(导轨相机手感)。砸碎链:破坏者方块(`DA_GS_Block_GravityBreaker`)真砸到可破坏块(`bBreakable` + `DA_GS_Break_Fragile`)就会按能量击碎,无需额外配置。

## 白盒装配(把系统放进你的关卡)

见 **USAGE_WHITEBOX.md**。要点:白盒几何保持静态网格+有效碰撞;Manager/世界状态/滚球由 GameMode 自动生成(不用摆);方块/开关/体积按 USAGE 手册摆 `GS*` 原生类并配 Profile。

## 当前状态(2026-09-05)

各轮实测记录与数据见 `HANDOVER_zflip.md` §8–§14(v5 基础玩法、v6 六方向+导轨相机、砸碎修复+落地三带、拾取/钥匙/门)。最近两轮:

- 拾取/钥匙/门(F 交互链)本机已按白盒手册流程实测通过:摆件→配 KeyID→拾取锁屏→门滑开→重置回锁(§15)
- 落地三带已网格化:阈值不再是 cm/s 硬编码,而是按 `v(格)=√(2·g·格·cell)` 实时推导

## 关键参数(改手感)

| 参数 | 值 | 在哪改 |
|---|---|---|
| 落地三带(安静/反弹/反重力的格数) | 4 / 7 / 4 格 | 球组件 `GS Landing Response` → `GravityShift\|Landing\|Grid`(关掉 `bGridBasedLanding` 回旧 cm/s 模式) |
| 网格细胞尺寸 | 100 cm | 同上 `GridCellSizeCm` |
| 导轨相机跟球距离 | 700(300–1400) | 球组件 `GS Rail Camera` → `TrailDistanceCm`(游戏内 Q/E 也能调) |
| 松键制动 | 60 | Pawn `StopTorqueAcceleration`(细节面板) |
| 主动重力加速度 | 1600 cm/s² | `DA_GS_Gravity_Default` |

⚠ 若关掉网格联动改回 cm/s:反重力线必须高于"房间内最大落体冲击" `sqrt(2·1600·H)`,否则球每次落顶都自动反翻。

## 文档地图

- `USAGE_WHITEBOX.md` — 白盒装配指南(类清单/配置/标签约定/python 批量摆法)
- `AGENT_GUIDE.md` — AI 接手指南(环境/连接/编译/排雷入口)
- `HANDOVER_zflip.md` — 完整交接史(五轮工作、每轮踩坑与修复)
- `AgentSkill/ue-nocode/` — UE 无代码操控工作手册(AI skill,含按症状查询的主题手册)
- `AgentSkill/ue-cpp-build-cnpath/` — 中文路径 C++ 编译排雷

## 代码结构

```
Plugins/GravityShift/Source/GravityShift/
  Public/Private/     33 个原生类:Manager / RollingBallPawn / GravityBody /
                      LandingResponse / BlockBase / Surface|LandingVolume /
                      GravitySwitch / PickupItem / Key / KeyDoor /
                      WorldState(Checkpoint/Kill/Collectible/Goal) / CameraRail / Profiles
  Content/Python/v5/  install_blueprints.py(14 BP 壳)+ generate_data_assets.py(12 DA)
Content/GravityShift/     BP 壳 + Data/Profiles(DataAsset)——见 USAGE_WHITEBOX「文件存放规范」
Content/LevelPrototyping/ 白盒通用资产(网格/材质/门/跳板等原型件)
Content/测试案例.umap      测试关卡(棚子白盒 + 完整一套系统 + 相机导轨)
Config/               MCP 自启、远程 Python、GameMode=GSGravityGameMode、启动地图
```

克隆后 Binaries/Intermediate 不在仓库里(标准 .gitignore),右键 uproject 重编译即可。
