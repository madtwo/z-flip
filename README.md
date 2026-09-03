# GravityShift z-flip — UE 5.8 双向重力滚球

双极性 ±Z 重力 + 物理滚球主角的玩法框架。引擎 **Unreal Engine 5.8.2**,核心逻辑全部在原生 C++ 插件 `Plugins/GravityShift`(21 个类),Blueprint 只做资产壳,没有任何蓝图 Tick/事件图重力实现。

## 操作

| 键 | 作用 |
|---|---|
| WASD | 滚球(相机相对,松键快速制动) |
| G | 重力翻转(小球+可动方块一起翻;轨相机只调俯仰不滚转) |
| 1 / 2 / 3 | 重力吸到 X / Y / Z 轴正方向(受关卡允许轴限制) |
| Q / E | 相机拉远/拉近(导轨相机跟在球后面的距离,步长 50,范围 0–2000) |
| F | 交互(重力开关等;开关通常滚过即触发) |
| R | 重置 |

> 相机说明:测试关卡使用**导轨相机**(相机套在 `GSCameraRail` 轨道上跟随,防晕设计)。没有导轨的关卡自动回落为旧跟随相机(鼠标可转视角)。

## 快速开始

1. 打开 `z-flip.uproject`(克隆后首次打开右键 uproject → 重编译,约十几秒;启动地图即 `Content/测试案例.umap`)
2. 按 Play:球在棚子里,直接 WASD/G 玩
3. 测试场景已配好:重力/破坏/静态三种方块、重力开关、慢速表面区、上下边界 KillVolume(重置用)

## 白盒装配(把系统放进你的关卡)

见 **USAGE_WHITEBOX.md**。要点:白盒几何保持静态网格+有效碰撞;Manager/世界状态/滚球由 GameMode 自动生成(不用摆);方块/开关/体积按 USAGE 手册摆 `GS*` 原生类并配 Profile。

## 当前测试状态(2026-09-02)

已实测 ✅:
- 小球移动:WASD 力矩滚动、方向与相机一致(W=前/A=左)、松键快速停稳
- G 翻转:小球与可动方块同时换重力;方块直上直下贴住天花板,无翻滚;再按 G/重置回落
- 落地三档:≤150 无反应 / 150~2000 弹跳一次(不无限弹)/ ≥2000 反重力翻转(仅小球触发)
- 摄像机:翻转时四元数 slerp 180°,操作保持相机相对(无镜像)

未实测 ⚠:
- E 键开关交互、KillVolume/表面体积的实机触发
- 反重力档需要 ≥1250cm 净落差(本测试房最大 850cm,达不到 2000 阈值;临时调低 `DA_GS_Ball_Default.landing_auto_reverse_at_speed_cm` 可看效果)
- 多关卡/其他项目迁移

## 关键参数(改手感)

| 参数 | 值 | 在哪改 |
|---|---|---|
| 反重力线 V_HIGH | 2000 cm/s | `DA_GS_Ball_Default.landing_auto_reverse_at_speed_cm` |
| 无反应线 V_LOW | 150 cm/s | 同上 `no_response_below_impact_speed_cm` |
| 弹跳速度 | 250 cm/s | 同上 `bounce_speed_cm` |
| 松键制动 | 60 | Pawn `StopTorqueAcceleration`(细节面板) |
| 主动重力加速度 | 1600 cm/s² | `DA_GS_Gravity_Default` |

⚠ 反重力线必须高于"房间内最大落体冲击" `sqrt(2·1600·H)`,否则球每次落顶都自动反翻,翻转会被立刻打回。

## 文档地图

- `USAGE_WHITEBOX.md` — 白盒装配指南(类清单/配置/标签约定/python 批量摆法)
- `AGENT_GUIDE.md` — AI 接手指南(环境/连接/编译/排雷入口)
- `HANDOVER_zflip.md` — 完整交接史(五轮工作、每轮踩坑与修复)
- `AgentSkill/ue-nocode/` — UE 无代码操控工作手册(AI skill,含按症状查询的主题手册)
- `AgentSkill/ue-cpp-build-cnpath/` — 中文路径 C++ 编译排雷

## 代码结构

```
Plugins/GravityShift/Source/GravityShift/
  Public/Private/     21 个原生类:Manager / RollingBallPawn / GravityBody /
                      LandingResponse / BlockBase / Surface|LandingVolume /
                      GravitySwitch / WorldState(Checkpoint/Kill/Collectible/Goal) / Profiles
  Content/Python/v5/  install_blueprints.py(14 BP 壳)+ generate_data_assets.py(12 DA)
Content/GravityShift/ BP 壳 + Data/Profiles(DataAsset)
Content/测试案例.umap  测试关卡(棚子白盒 + 完整一套系统)
Config/               MCP 自启、远程 Python、GameMode=GSGravityGameMode、启动地图
```

克隆后 Binaries/Intermediate 不在仓库里(标准 .gitignore),右键 uproject 重编译即可。
