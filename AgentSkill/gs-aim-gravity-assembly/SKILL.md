---
name: gs-aim-gravity-assembly
description: z-flip / GravityShift 新重力机制(2026-09-12)组装手册（给关卡侧 AI）：准星瞄准改物体重力(RMB 瞄准+LMB 切换 ±Z)、玩家重力只由转向器圆弧控制、无导轨相机+TPS 聚焦。当要摆"可被瞄准改变重力的方块"、搭新一关、或发现旧文档里的 G/1/2/3/导轨相机/重力开关键位与实际不符时使用。
---

# 准星改重力 + 无导轨相机 —— 关卡侧 AI 手册(2026-09-12 新机制)

> **本机制取代了旧机制**(用户拍板):G 翻转、1/2/3 设轴、导轨相机、重力开关全部退场。现在**玩家重力只由转向器圆弧改变**(见 `gs-redirector-assembly`),**物体重力只有 ±Z(掉下来/升起来),由小球玩家瞄准切换**。**不用写代码、不用建蓝图**,摆对东西就行。
> ✅ 代码已编译 + PIE 脚本验收(方块升 59→850 顶棚稳住 / 切回落下 / 转向器回归)+ 用户实机验收通过;详见 `HANDOVER_zflip.md` §27。

## 0. 先懂机制(30 秒)

- **物体重力 = 每个方块自己的 ±Z**(`AGSBlockBase::bGravityRises`,false=掉下来 true=升起来),**不再跟随全局重力管理器**——玩家滚转向器上天花板时,方块不会跟着翻。
- **切换方式**:玩家**按住右键** → 出现准星 + 相机聚焦(FOV 收窄/相机贴近/越肩让位)→ 可改变的方块**边缘微光**(绿色 Overlay)→ **左键**在掉下来↔升起来之间切换。
- **哪些方块能被瞄准**:`GSBlockBase` 且**正在模拟物理**(`bStartSimulatingPhysics=true`)且受重力启用。固定块(Fixed Profile)瞄不中——瞄它准星不变绿。
- **玩家重力**:只由转向器圆弧改变;落地三带的自动反转(≥20 格落差反重力)仍然有效,那是玩家自己的行为。

## 1. 前置条件(不做就"瞄不中/没反应")

1. **方块勾 `Simulating Physics`**(`bStartSimulatingPhysics=true`)+ `bAffectedByGravity=true`。没勾 = `CanChangeGravity()`=false,准星永远不锁它——这是"瞄不中"最常见的原因。
2. **用 `ApplyBlockProfile` 应用 DA**(`DA_GS_Block_Gravity` 等)让配置生效,和旧流程一致。
3. **不要摆** `GS Camera Rail`(导轨相机)和 `GSGravitySwitch`(重力开关)——新机制下它们已退场;摆了导轨相机会把这一关切回旧轨相机模式,鼠标俯仰/聚焦手感全部失效。

## 2. 摆什么

| 放什么 | 数量 | 摆哪 | 说明 |
|---|---|---|---|
| `GS Block Base`(Gravity/Breaker Profile) | 每个可瞄准目标 1 个 | 玩家滚动路径附近、**离地近一点**(第一格高差内),别塞进转向器触发盒 | 勾 Simulating Physics + 受重力;**默认落地不弹**(`bZeroBounceOnLand`);要玩家推不动就勾 `bImmovableByPlayer`(质量抬到 `ImmovableMassKg=2000`,±Z 切换不受质量影响) |
| `GS Redirector` 滑梯 | 玩家要上墙/上天花板处 | 见 `gs-redirector-assembly` | **玩家改变重力的唯一途径** |
| `GS Camera Rail` | **0 个(新关卡禁摆)** | — | 摆了 = 该关回旧轨相机,无聚焦/无鼠标俯仰 |
| `GSGravitySwitch` | **0 个(已退场)** | — | 类还在代码里(兼容旧关),新关卡摆了也没意义 |

PlayerStart / KillVolume / Checkpoint / 钥匙门等照旧(见 USAGE_WHITEBOX),GameMode 自动生成的照旧不摆。

## 3. 键位( HUD 底部提示行同款)

| 键 | 行为 |
|---|---|
| WASD | 滚动(平面加速度驱动;**瞄准中自动 ×0.6 减速**) |
| **右键(按住)** | 瞄准:准星出现、相机聚焦(FOV→55、臂长→250、越肩让位 65cm)、灵敏度自动按 FOV 开方比缩小 |
| **左键(瞄准中)** | 锁定的方块:掉下来 ↔ 升起来 |
| F / R / O/P | 交互 / 重置 / 调速(不变) |
| Q/E | 无导轨相机下**暂不生效**(回退保留旧实现;轨相机关卡里仍调轨距) |
| ~~G~~ / ~~1/2/3~~ | **已删除**。旧文档(§8-§21 时代)提到这两个键的段落全部作废 |

## 4. 手感参数(都在球 Pawn 细节面板,分组 `GravityShift|Input / |Aim / |Camera`)

| 参数 | 默认 | 含义 |
|---|---|---|
| `AimRangeCm` | 2500 | 准星射线最长距离 |
| `AimTargetFOV` | 55 | 聚焦时 FOV(越小越聚焦) |
| `AimArmLengthCm` | 250 | 聚焦时相机臂长 |
| `AimZoomSpeed` | 8 | 聚焦插值速度(松开恢复 ×0.6) |
| `AimDriveScale` | 0.6 | 聚焦时 WASD 减速比 |
| `AimShoulderOffsetCm` | 65 | 越肩让位(视线绕过球本体,抬头瞄天花板必备) |
| 鼠标灵敏度 | Yaw 0.50 / Pitch 0.35 | GSProfiles.h 基准;聚焦中自动 ×√(FOV比) |

**抬头时相机"拉近"是防穿墙探针在工作**(臂长不打折相机就怼进天花板里)——设计行为,不是 bug;松开右键后有 **1.2s 快速回弹窗口**(不等驻留、放长速度×3),视线一离开天花板就迅速回正常距离。

## 5. PIE 自验配方(照抄,判据全可脚本读)

1. `can_change_gravity()` 对 Gravity/Breaker 块为 True、Fixed 块为 False。
2. 按住右键(或直接调 `set_gravity_rises(True)`)→ 方块 z 持续上涨,到天花板 `|v|→0` 稳住不掉。
3. `toggle_gravity_z()` → vz 变负下落,落地安静(零回弹)。
4. 瞄准态肉眼验:准星(锁定变绿)、方块边缘绿光、FOV 收窄、越肩让位。
5. 玩家滚转向器:重力平滑转向;**方块纹丝不动**(不再跟随全局重力)。
6. 按旧键 G / 1/2/3:应无任何反应(已删除)。

## 6. 坑(每条都真实踩过)

1. **方块没勾 Sim Physics = 瞄不中**(第 1 节前置条件),表现为"准星不绿、左键没反应"。
2. **v1 教训(别回退)**:高亮最初用"换材质槽位"实现,方块会变黑——已改为 **Overlay 叠加材质**(`M_GS_RimGlow`,插件 Content 里,unlit 半透明菲涅尔),原材质完全保留。别改回槽位换法。
3. **可动方块别放进转向器触发盒**——转向器是给玩家用的,方块滚进去可能触发意外行为。
4. **ADS 臂长别直接写弹簧臂 `TargetArmLength`**:无导轨相机下 `UpdateCamera` 每帧用防穿墙探针的平滑值覆盖它(写了=白写,松右键还会打架跳变)。要改聚焦臂长就改 `AimArmLengthCm` 参数。
5. 抬头瞄准保持拉近是防穿墙,**别当 bug 修**;要调的是回弹窗口时长(`FastArmExtendUntilSeconds`,写死 1.2s)。
6. 摆了导轨相机的旧关不受影响(兼容保留);但新关卡按本手册一律不摆。
7. **函数级 static 的 UObject 必须紧跟着 `AddToRoot()`**:静态指针不进 GC 引用图,PIE 重启间隙被回收后,下一局设悬空指针会触发 `BodyInstance.cpp` 断言直接崩编辑器(零回弹材质实测踩过,症状还包括接触解算被污染、方块以几十 cm/s 爬行)。

## 7. 相关文档

- 转向器(玩家重力):`AgentSkill/gs-redirector-assembly/SKILL.md`
- 重力区域(检测器/管理器,与 ±Z 方块兼容):`AgentSkill/gs-gravity-zone-assembly/SKILL.md`
- 实现记录:`HANDOVER_zflip.md` §27;实现文件:`GSRollingBallPawn.cpp`(瞄准/聚焦)、`GSBlockBase.cpp`(±Z/高亮)、`GSGravityBodyComponent.cpp`(自有重力方向)、`GSFramework.cpp`(准星/提示行)
