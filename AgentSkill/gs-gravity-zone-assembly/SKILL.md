---
name: gs-gravity-zone-assembly
description: z-flip / GravityShift 重力区域检测器(检测器 + 区域管理器)拼装手册（给关卡侧 AI）。当要让"可移动方块平时不掉、球穿过门口才让某一侧方块受重力"、要摆 GS Gravity Detector / GS Gravity Zone Manager、或 PIE 自验双向切区域时使用。
---

# 重力区域检测器拼装 —— 关卡侧 AI 手册

> 检测器 = 门口一层**不可见薄触发面**（`AGSGravityDetector`）+ 一个全场唯一的协调者（`AGSGravityZoneManager`）。球穿过检测器 → 全场方块重力清零 → 只把**球要去的那个区域**的方块重力打开。**不用写代码、不用建蓝图**。
> ✅ **代码已编译通过、7 条 PIE 用例已实跑全绿（2026-09-11）**，结果见 §4。
> 类定义见 `Plugins/GravityShift/Source/GravityShift/Public/GSGravityZones.h`；交接史见 `HANDOVER_zflip.md` §25。

## 0. 先懂机制（30 秒）

- **两个类**：
  - `AGSGravityZoneManager`（一个关卡放**一个**）：`SetActiveZone(Blocks)` = 先全图 `SetAffectedByGravity(false)`，再把 `Blocks` 里的逐个打开。**同一时刻最多一个区域受重力**这句话就是它保证的。
  - `AGSGravityDetector`（一条分界线放一个）：球穿过时按**穿行方向**把 `ZoneA_Blocks` 或 `ZoneB_Blocks` 交给管理器，并闪一次光。
- **朝向约定（唯一要记死的一条）**：检测器的 **前向箭头指着 ZoneB 那一侧**，背后是 ZoneA。
  - 球**朝前向**穿过（速度·前向 > 0）→ 激活 **ZoneB_Blocks**
  - 球**朝背向**穿过（速度·前向 < 0）→ 激活 **ZoneA_Blocks**
  - 所以**同一个检测器天然双向**：两个区域各配一次，球来回穿自动切换，不用摆两个检测器。
- **"禁用重力" = 瞬间定住**：被禁用的方块除了停止受重力，还会**线速度/角速度清零**（`AGSBlockBase::FreezeMotion()`），不会保留速度继续滑行。被重新激活时速度从 0 开始正常恢复物理模拟，无需额外处理。
  - 只有**被禁用**的方块会清零；即将激活的目标区域方块不走冻结路径（`SetActiveZone` 是单次遍历，跳过目标集合），避免球在门口来回穿时区域方块一顿一顿。

## 1. 前置条件（不做这三条，PIE 里什么都看不见）

1. **关卡里必须有且只有一个 `AGSGravityZoneManager`**。没有它，检测器只闪灯并在 Output Log 打 Warning：`关卡里没有 AGSGravityZoneManager`。它**不会自动 spawn**。
2. **方块必须勾 `Simulating Physics`**（`AGSBlockBase` 的 `bStartSimulatingPhysics`）。没勾的话方块根本不参与物理，开关重力它也不动——这是"改了没反应"最常见的原因。
3. **管理器开局的禁用是延到下一帧执行的**（`SetTimerForNextTick`）。原因：Actor 的 `BeginPlay` 顺序不保证，方块自己的 `BeginPlay` 会用序列化下来的 `bAffectedByGravity` 覆盖一遍，立即禁用会被后跑的方块顶掉。所以**第一帧内方块可能还带着作者设的重力**，第一帧之后才归零——属正常。

## 2. 摆什么

| 放什么 | 数量 | 摆哪 |
|---|---|---|
| `GS Gravity Zone Manager` | **整关 1 个** | 随便放（空 Actor，无组件），放关卡角落即可 |
| `GS Gravity Detector` | 每条分界线 1 个 | 两个区域之间的**门口/通道口**，前向箭头指向 ZoneB 侧 |
| `GS Block Base` | 若干 | 两个区域内，**勾 Simulating Physics** |

放置路径：Place Actors 搜 `GS Gravity` / `GS Block`。

## 3. 配置

### 3.1 Zone Manager（通常不用动）

| 参数 | 默认 | 作用 |
|---|---|---|
| `bStartWithGravityDisabled` | true | 开局全图禁用方块重力。**false** = 沿用方块各自的 `bAffectedByGravity` 初始值，检测器只做"切换"不做"起步禁用" |

### 3.2 Detector

| 参数 | 默认 | 作用 |
|---|---|---|
| `ZoneB_Blocks` | 空 | **前向箭头那一侧**的方块。留空接口，见 §3.3 |
| `ZoneA_Blocks` | 空 | **背后那一侧**的方块。留空接口，见 §3.3 |
| `TriggerExtent` | (5, 200, 200) | 触发盒**半尺寸**（`SetBoxExtent` 语义，不是全长）。X=5 → **10cm 厚**。门比 400cm 宽就调大 Y/Z |
| `FlashDuration` | 0.3 | 闪一次亮多少秒（重复触发会重置计时，不叠加） |
| `MinTriggerSpeed` | 50 | 低于此速度不触发——球停在触发面上来回蹭不该反复切区域 |
| `MinDirectionalDot` | 0.2 | 速度方向与前向夹角余弦的下限。球**侧向擦过**（分不出正反）时不切区域，否则 `Dot` 的正负会把区域随机切到一侧 |

- **旋转检测器 = 旋转触发面**：actor 的前向就是盒子的局部 X。只用正交角度（0/90/180/270）摆放，斜着放会让"前向"含非轴向分量，分界语义会歪。
- **闪光网格 `FlashMesh`**：编辑器里**可见**（方便摆位），游戏里默认隐身。默认材质是引擎基础方块——要发光效果就给它挂一个半透明自发光材质，这一步由关卡/美术配。
- **薄面不会漏触发**：小球开了 CCD（`BallCollision->SetUseCCD(true)`），高速穿越不会被跳过。真需要更厚直接改 `TriggerExtent.X`。

### 3.3 方块列表：**先留空**（本节是给后续 AI 的）

摆关卡阶段**不要手填**这两个数组，按 §5 的约定由脚本按空间位置批量填。

## 4. PIE 自验结果（✅ 2026-09-11 实跑，7/7 通过）

**测试台**：临时关卡 `_TempGravZone`（已删除，没碰 `测试案例`）——管理器 1 个、检测器 1 个（前向 +X，摆在 x=0，`TriggerExtent` 默认 (5,200,200)）、4 个方块（x=-804 两个 = ZoneA，x=+795 两个 = ZoneB，**都勾 Simulating Physics**）。球的驱动用关卡里现成的持续驱动（**不是脚本注入速度**——会被松键刹车在恢复帧吃掉，见 `HANDOVER_zflip.md` §23）。

| # | 用例 | 实测结果 |
|---|---|---|
| 1 | 基线：开局静止 | ✅ 4 个方块全 `grav=False`、`z=300`、`\|v\|=0` |
| 2 | 球朝 +X 穿过检测器 → 对应区域受重力 | ✅ ZoneB（x=+795）`grav=True`，`z` 300→50 落地；ZoneA（x=-804）仍 `grav=False`、`z=300` 静止 |
| 3 | 反向（−X）穿过 → 切换区域 | ✅ ZoneA `grav=True` 下落（z 300→69→50）；ZoneB 转 `grav=False` |
| 4 | 侧向擦过（dot < `MinDirectionalDot`）→ 不触发 | ✅ 触发盒临时加宽到 (300,200,200) 保证球**必然穿过**：球 x 全程 ≈ −2，y 从 −1200 穿到 +844.8，其中 y=149.3 时**正在盒内**、速度纯 +Y。全程**不闪光、不切区域**。（`MinDirectionalDot` 确实拦下了横向穿越，不是"没撞上"） |
| 5 | 闪光反馈 | ✅ `flash_duration` 临时调到 60s 便于观测：触发后 `hidden_in_game=False` / `visible=True`，跨两次读取持续亮着；触发前为 `True` |
| 6 | 全局禁用时**瞬间定住**（不再滑行） | ✅ **关键项**：ZoneB 正以 3052cm/s 下落（z=7627）时反向触发 → 立刻读到 `\|v\|=0.0`、`z=7017.0`；约 2 秒游戏时间后再读，z **仍精确等于 7017.0** → 是真冻住，不是滑行 |
| 7 | 同一时刻只有一个区域激活 | ✅ 全程任意时刻只有一侧 `grav=True` |
| 8 | 重置接口 | ✅ `reset_all_zones()` → 4 个方块全 `grav=False`、`\|v\|=0`；`reset_detector()` → 闪光重新隐身 |

**额外确认**：被冻住的方块重新激活后能正常恢复下落（z 7017→6967→6193），即"重新启用不需要额外处理"成立。

**遗留**：闪光只验证了显隐翻转，未验证自定义发光材质的观感（材质由关卡/美术配，见 §3.2）。

排查入口：区域不变先看有没有管理器；管理器在但方块不动先看 `Simulating Physics`；数值调试用 `AGSBlockBase->GravityBody->bGravityEnabled`（PIE 里选中方块看组件详情）。

## 5. 后续 AI 填入方块引用（**暂未实现，只留了数组接口**）

关卡几何搭完后，脚本扫描 + 按空间位置写入：

1. 取全关 `AGSGravityDetector` 和 `AGSBlockBase`（`UnrealEditorSubsystem.get_editor_world()` + `unreal.GameplayStatics.get_all_actors_of_class`，**编辑器世界不是 PIE 世界**）。
2. 对每个检测器，算每个方块中心相对检测器原点的位移 `D`，投影到检测器前向 `F`：`s = dot(D, F)`。
   - `s > 0` → 方块在**前向侧** → 填 `ZoneB_Blocks`
   - `s < 0` → 方块在**背后侧** → 填 `ZoneA_Blocks`
   - `|s|` 太小的（几乎贴在面上）建议**跳过并报告**，让人工判断。
3. 写实例属性：`detector.set_editor_property('ZoneB_Blocks', [...])`（同理 `ZoneA_Blocks`）。
4. **写完必须存盘**：数组是实例数据，随 umap 保存；只 set 不 save，PIE 一跑就没了。
5. 多检测器/多分区的关卡，"哪个方块归哪个检测器"光靠 `s` 正负不够（相邻分区的方块会同时落进两侧）。空间划分规则得按关卡实际几何再定——**别硬套**。

## 6. 相关文档

- `README.md`「功能积木清单」—— 所有 GS 积木一览
- `USAGE_WHITEBOX.md` —— 摆放/文件规范
- `HANDOVER_zflip.md` §25 —— 本轮实现记录
- `AgentSkill/gs-block-assembly/SKILL.md` —— 关卡侧 AI 总拼装手册
- `AgentSkill/gs-redirector-assembly/SKILL.md` —— 转向器（同样是"关卡驱动的重力变化"，可对照看）
