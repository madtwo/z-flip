# DEMO01 地板依赖：问题说明 · 队友操作步骤 · 根治计划

> 适用症状：打开 `Main` 后**地板/房间本体不见了**，球直接掉虚空；或者"必须手动把某个关卡加到流送列表里"才有地板。
> 结论先说：**地板不在 Level1/Level2 里，而在示例房间关卡 `Demo_01` 里**；世界只有在自己的流送列表里注册过 `Demo_01` 才会加载它。

---

## 1. 根因（机制 + 可复核证据）

1. 房间本体（地板、墙、天花板、管道、灯带）都是 `/Game/LP_SciFi_Interior_JC/Maps/Demo_01` 里的演员（实测 **1145 个**）。
   `Content/Maps/Level1.umap`、`Level2.umap`、`Main.umap` 自己只放**功能物件**（重力方块、圆弧、拾取物、碰撞代理等），房间是**流送**进来的。
2. UE 的规则：**子关卡的流送不会向上传播**。只有"当前世界（`Main`）的流送列表"里注册过的关卡才会被加载。
   所以 `Demo_01` 没注册在 `Main` 里 = 地板永远不渲染。
3. 最省事的自查命令（对任何一份 umap 都成立，二进制里字符串是明文）：

   ```bash
   grep -a -c Demo_01                    Content/Maps/Main.umap   # ≥2 = 注册在案
   grep -a -c LevelStreamingAlwaysLoaded Content/Maps/Main.umap   # ≥1 = 用的"始终加载"
   ```

   实例对比（2026-10-07 实测）：
   * 我方 `Main.umap`：`Demo_01` 2 次、`AlwaysLoaded` 1 次 → **有地板**
   * 队友 2026-10-06 18:35Z 推的 `Main.umap`（blob `8f2f2e5b`）：`Demo_01` **0 次** → **没地板**

## 2. 现在就能用的办法

### A. 直接拉最新版（Level1/Level2 已自带注册）
拉取后打开 `Main` 直接 PIE。刚才推送的 `Level1.umap` 里已经带着 `Demo_01 = AlwaysLoaded` 的注册，`Main` 流送 `Level1` 时会把它一起带进来。

### B. 若仍然没地板 → 在**自己的** Main 里手动注册一次（一次性）

**编辑器 UI 步骤**
1. 打开 `Main`
2. `窗口 → 关卡`（Levels 面板）→ **添加 → 添加现有…** → 选 `/Game/LP_SciFi_Interior_JC/Maps/Demo_01`
3. 该行的流送方式改成 **Always Loaded（始终加载）**（不要用 Blueprint / On Demand）
4. **立刻保存 `Main`**（`Ctrl+S`，只保存 Main）

**等价的 Python（编辑器 Python 控制台，逐行敲或整段粘贴）**

```python
import unreal
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not LES.load_level("/Game/Maps/Main"):
    raise RuntimeError("打开 Main 失败")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
sl = unreal.EditorLevelUtils.add_level_to_world(
    world, "/Game/LP_SciFi_Interior_JC/Maps/Demo_01", unreal.LevelStreamingAlwaysLoaded)
if sl is None:
    raise RuntimeError("add_level_to_world 失败")
if not unreal.EditorLoadingAndSavingUtils.save_map(world, "/Game/Maps/Main"):
    raise RuntimeError("保存 Main 失败")     # ← 必须紧跟着存，见下面的坑
```

> ⚠ **最容易翻车的点**：`LevelEditorSubsystem.load_level()` 会关掉当前持久层**且不保存**。
> 所以"添加关卡"之后如果不**立刻**保存，切到别的关卡/重开编辑器，这份注册就没了——症状看起来就是"我又挂了关卡，但地板还是没了"。
> 判定"真的写进去了"要看三样：① `Main.umap` 体积/哈希变了 ② `grep -a -c Demo_01` ≥2 ③ **重开 Main** 后 Levels 面板里仍有 `Demo_01`，且它的演员数是 1145 上下。

### C. 前提与提交
* 仓库里必须有 `Content/LP_SciFi_Interior_JC/Maps/Demo_01.umap`（远端有）；它引用的美术资产也都在 `Content/LP_SciFi_Interior_JC/` 里。
* 改完 `Main` 记得提交推送，否则别人拉到的仍然是没有注册的版本。
* 注意：`Main.umap` 目前**双方都在改**（我方有 Demo_01 常驻加载那次修改，队友有"场景尺寸适配"），合并前先确认两边各自的改动，别直接互相覆盖。

## 3. 根治计划（不再依赖示例关卡）

**目标**：任何人都能"拉下来就有地板"，彻底不需要"挂关卡"这一步；`Main` 成为**唯一的运行时流送拥有者**。

### 阶段 0 · 准备（10 分钟）
1. 备份：把 `Content/Maps/{Main,Level1,Level2}.umap` 复制到 `_backup_<日期>/`（出问题一键还原）。
2. 约定验收口径（后面每一步都按它验）：**重开编辑器 + PIE 真球落点测试 + 打包**。

### 阶段 1 · 建项目自己的艺术子关卡
3. 新建空关卡 `Level1_Art`、`Level2_Art`（`File → New Level → Empty`，存在 `Content/Maps/`）。
4. 打开 `Level1`，把 `Demo_01` 一起显示着，在视口里**只框选第一关真正用到的那批艺术演员**（地板、墙、天花板、管道等），
   **复制**（`Ctrl+C`）→ 打开 `Level1_Art` → **粘贴到同一世界坐标**（`Ctrl+V`，粘贴时选"保持世界坐标"）。
   * 只复制"看得见的美术"，功能物件（重力方块/圆弧/拾取物/碰撞代理）一个都不要动。
   * 目标：`Level1_Art` 自包含，关掉 `Demo_01` 也能把第一关的观感补全。
5. `Level2_Art` 同法处理第二关。
   ⚠ **不要用"移动资产文件夹"的办法**，也不要转 World Partition，更不要用 `LevelStreamingDynamic`——这三条以前都出过事故/被明确否决。

### 阶段 2 · 切换流送
6. 打开 `Level1`：Levels 面板 `添加现有…` → `Level1_Art`，流送方式 **Always Loaded**；**保存 Level1**。
7. `Level2` 同样挂上 `Level2_Art` 并保存；顺手把 Level2 里 `Demo_01` 那条流送项删掉并保存。
8. `Main`：把 `Level1_Art` / `Level2_Art` 也登记进自己的流送列表（**Always Loaded**），**立刻保存 Main**（见 §2 的坑）。
   此时 `Main` 拥有：`Level1..Level6` + `Level1_Art` + `Level2_Art`，不再需要 `Demo_01`。

### 阶段 3 · 摘掉 Demo_01 依赖（**最后一步**）
9. 逐关卡确认没有别的东西引用 `Demo_01`：在 `Main` / `Level1` / `Level2` 上各执行
   `grep -a -c Demo_01 Content/Maps/<该关卡>.umap` —— 目标是 **0**（或只剩历史残留的字符串、Levels 面板里确实没有该行）。
10. 移除 Main 里的 `Demo_01` 流送项，**立刻保存 Main**。
11. **回归清单**（每条都要过）：
    * 重开编辑器 → Levels 面板列出 `Level1..6 + Level1_Art + Level2_Art`，**没有 Demo_01**；
    * PIE 从 `Main` 出生点出发，两关的**每块地板**都踩得住（用真实 Pawn 落点验证，不要只看射线）；
    * 第一、二关的**重力方块**（118/127/174/160）与**两个圆弧**（SM_Wall_11_V5/V6）行为不变；
    * 楼层碰撞代理复检：`GS_FloorProxy_SM_Walls_Floor_2/_4`（在 Level2，12 cm 水平 Floor 代理）。
      若 `Level*_Art` 里复制过来的地板自带简单碰撞、球不再穿，**再把这两个代理删掉**；只要还有一块漏，就保留代理。
    * 打包一份，跑一遍打包产物（打包路径最容易暴露"只在编辑器里对"的问题）。

### 阶段 4 · 收尾
12. 提交推送（只推被改动的 umap + 新建的 `Level*_Art.umap`），并在提交信息里写清"已摘 Demo_01 依赖"。
13. 更新本文档（把实际做法与偏差记录下来）。

## 4. 禁止事项（血泪清单）

* ❌ 不要移动/整理 `Content/` 下的资产文件夹（历史事故：引用大面积失效）。
* ❌ 不要把这个工程转 World Partition。
* ❌ 不要用 `LevelStreamingDynamic` 来做常驻加载。
* ❌ 不要在替代物可用之前删白盒/删 `Demo_01` 依赖——顺序必须是"先能替代，再删"。
* ❌ "射线打得中"不等于"球踩得住"：碰撞验收只认真实 Pawn 落点。

## 5. 相关文件

* 示例房间关卡：`Content/LP_SciFi_Interior_JC/Maps/Demo_01.umap`
* 受影响关卡：`Content/Maps/{Main,Level1,Level2}.umap`
* 已有局部修补：Level2 内 `GS_FloorProxy_SM_Walls_Floor_2` / `GS_FloorProxy_SM_Walls_Floor_4`
* 同步规范（REST 直传、head 守卫、不许整树重建）：`AgentSkill/ue-nocode/reference/GITHUB_SYNC.md`
