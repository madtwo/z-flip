---
name: gs-block-assembly
description: z-flip / GravityShift 关卡积木拼装手册（给关卡侧 AI）。当你拿着一张"只有建模、没有代码"的白盒地图要接入重力滚球玩法（摆方块/开关/拾取/钥匙门/KillVolume/相机导轨/落地三带/砸碎链）、新建可玩关卡、或 PIE 自验 GS 系统时使用。
---

# GS 积木拼装 —— 关卡侧 AI 接入手册

> 你有一张白盒地图（只有网格/模型），要让它变成重力滚球可玩关卡，**不需要写一行代码、不需要建任何蓝图**。
> 本手册所有配方都在空关卡上实测通过（2026-09-05，球自动生成 + 钥匙开门全链）。
> 深层细节的权威出处：仓库 `README.md`「功能积木清单」与 `USAGE_WHITEBOX.md`；交接史与踩坑看 `HANDOVER_zflip.md`。

## 0. 环境准备（一次性）

1. 克隆本仓库（public，免令牌）
2. 右键 `z-flip.uproject` → Switch Unreal Version → 选 5.8 → 首次打开会重编译（约十几秒）
3. **你不需要建 C++**：核心全在 `Plugins/GravityShift`（原生插件，33 个类），克隆后编译即可用

## 1. 先懂机制：为什么"什么都不摆也能玩"

`Config/DefaultEngine.ini` 里 `GlobalDefaultGameMode=/Script/GravityShift.GSGravityGameMode` 是**全局**的——任何新地图按 Play 都会自动生成：

- `GSGravityManager`（重力权威，全屋一个）
- `GSWorldStateManager`（检查点/重置/收集）
- 滚球玩家（出生在 **PlayerStart**）

所以你的关卡里**必须有至少一个 PlayerStart**。默认重力 −Z（落地）、全轴可用（±X/±Y/±Z 都允许）。

## 2. 最小可玩关卡（先跑通环境，再拼积木）

1. 白盒网格当地板。**碰撞必须有效**——建模软件（Blender/Maya）导出的网格默认常常没有碰撞，会直接穿地。检查：细节面板 → Collision → Collision Presets = BlockAll（或加简化碰撞）
2. 摆一个 `PlayerStart`（离地 ≥100cm，给球落下的余量）
3. 按 Play：球落在地板上、WASD 能滚 = 环境通了，往下拼积木

## 3. 积木总表（Place Actors 面板搜 "GS" 直接拖）

| 搜什么 | 干什么 | 关键配置（细节面板） |
|---|---|---|
| `GS Pickup Item` | 可拾取物，F 捡起 | `PickupMessage` 提示文案（留空=静默拾取不锁屏） |
| `GS Key` | 钥匙 | `KeyID`（如 `red`）——捡到瞬间全图同 ID 的门一起开 |
| `GS Key Door` | 锁住的门，滑开 | `RequiredKeyID` 对应钥匙；`SlideOffset` 滑动方向距离 / `SlideDuration` 时长 |
| `GS Gravity Switch` | 重力开关 | 默认球滚过即触发；`SwitchMode`、`RemainingUses` 可调 |
| `GS Surface Modifier Volume` | 表面区（慢速/加速） | `ProfileA` 选 DA_GS_Surface_Slow/Fast；`VolumeExtent` 贴住表面 |
| `GS Landing Response Volume` | 特殊落地区 | 摆在落点，覆盖球身默认的落地三带 |
| `GS Block Base` | 可动方块 | `BlockProfile` 选 DA（Gravity=随重力/Fixed=定死/GravityBreaker=破坏者）→ 点 `ApplyBlockProfile` 生效 |
| `GS Kill Volume` | 出界重置 | 房间上下边界外各一个（重力会翻转，上下漏出都要能重置） |
| `GS Checkpoint` / `GS Collectible` / `GS Finish Goal` | 检查点 / 收集物 / 终点 | 摆上即用 |
| `GS Camera Rail` | 导轨相机轨道（防晕） | 本地 Z 轴=轨道方向、`RailLength` 全长；不摆=旧跟随相机 |

**自动挂在球上、不用摆**：落地三带（≤4格安静 / 5–6格反弹到4格 / ≥7格反重力，按网格格数自动判，参数在球组件 `GS Landing Response` → `GravityShift|Landing|Grid`）、砸碎链（破坏者方块真砸到挂 `GS Breakable` 组件的块就按动能碎）、导轨相机组件。

## 4. 常用配方

### 4.1 钥匙开门（最常用配对）
1. 摆 `GS Key`，`KeyID` 填 `red`
2. 摆 `GS Key Door`，`RequiredKeyID` 填 `red` —— **ID 相同即配对，没有中央注册表**
3. 想一钥匙开多门：多扇门填同 ID；多把钥匙通用：多把钥匙填同 ID

### 4.2 关卡级重力配置（开局在墙上 / 只允许某些轴）
- 摆**恰好一个** `GS World State Manager`，细节面板 `Gravity|LevelConfig` 配 `DefaultGravityDirection` / `AllowedGravityAxes`
- 摆了就不再自动生成（不会双 Manager）；**摆两个才会**双 Manager（日志警告）
- 什么都不配 = −Z 落地 + 全轴可用，普通关卡不用管

### 4.3 相机导轨（新关卡要防晕相机才摆）
1. 摆 `GS Camera Rail`，旋转 Actor 让**本地 Z 轴沿轨道方向**（朝房间内部看）
2. `RailLength`=轨道全长（中心对称）；多段首尾相接即可，自动带 150cm 切换迟滞

### 4.4 换积木外观
拾取物/门/钥匙默认是引擎立方体，细节面板替换 `Mesh` 的静态网格即可，逻辑不受影响。

## 5. 拼完自验（PIE 清单，逐条过）

1. PIE 无编译错误 / Accessed None（日志 `Saved/Logs/z-flip.log`）
2. 球从 PlayerStart 落地站稳（没站稳=地板碰撞问题，回第 2 节）
3. WASD 方向与相机一致；G 翻转后天花板上操作不镜像
4. 走近拾取物按 F：中央提示出现 + 按空格继续；钥匙捡到门滑开（默认向上 240、0.8s）
5. R 或掉出 KillVolume：门回锁、拾取物复原（死亡回卷语义）
6. 最终让真人玩一遍——手感只有人能判断

## 6. 雷区（前人真踩，别重复）

1. **白盒网格没碰撞** → 球穿地。白盒一律检查 Collision Presets
2. **autosave 污染关卡**：编辑器开着测试时会把脏关卡写穿到 umap 文件。测试完 `git status` 查 umap，脏了就 `git checkout -- <umap>` 回退；文件被编辑器占用回退失败时：先在编辑器切到别的图（如 `/Engine/Maps/Templates/Template_Default`）→ checkout → 再切回
3. **umap 是二进制**：多人同时改同一张图必然冲突。约定同一时间一张图只有一个人改
4. 你的 AI 若用远程 Python 摆积木：类名**没有 A/U 前缀**（`unreal.GSKey` 不是 `AGSKey`）；bool 属性去掉 b 前缀（`bIsLocked` → `is_locked`）；BlueprintPure 函数用方法调用（`ball.is_message_locked()`）不是属性

## 7. 远程 Python 摆积木速查（可选，已实测）

```python
import unreal
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

ky = eas.spawn_actor_from_class(unreal.GSKey, unreal.Vector(300, 0, 60))
ky.set_actor_label("红钥匙")
ky.set_editor_property("key_id", "red")

dr = eas.spawn_actor_from_class(unreal.GSDoor, unreal.Vector(800, 0, 0))
dr.set_editor_property("required_key_id", "red")

# 摆完保存关卡
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
```

改了 C++（很少需要）才要重编：关编辑器 → `AgentSkill/ue-cpp-build-cnpath/SKILL.md` 流程 → 重开。
