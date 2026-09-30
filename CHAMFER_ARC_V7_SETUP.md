# 倒角圆弧重力转换 V7 —— 接入说明（给队友）

> 这套东西解决：**球顺着倒角圆弧（四分之一凸圆弧）滚过去，重力跟着曲面连续旋转，到另一面自然落上去**，
> 而且是**双向的**：从平面滚进去转到竖直面，从竖直面滚回来再转回平面。
> 代码只有两个文件：`GSOneWayGravityRedirectVolume.h/.cpp`（本仓库已更新）。没有动 `UGSRedirectorComponent`，也没有动任何 11/12 那类旧转向器。

## 一、代码怎么拿

仓库 `main` 已经包含 V7 源码：

```
Plugins/GravityShift/Source/GravityShift/Public/GSOneWayGravityRedirectVolume.h
Plugins/GravityShift/Source/GravityShift/Private/GSOneWayGravityRedirectVolume.cpp
```

拉下来后**关掉编辑器**，重新编译 Editor（本机是 `ZFlipEditor`）：

```
Build.bat ZFlipEditor Win64 Development -Project=<你的工程路径>/z-flip.uproject
```

> 不要用 Live Coding / 热重载换这套源码；改完必须整编一次。

## 二、地图里怎么摆（每张图 3 步）

1. **放体积**：在倒角弧附近放一个 `AGSOneWayGravityRedirectVolume`（蓝图/编辑器里显示名 `GSOneWayGravityRedirectVolume`），
   label 无要求。它的位置**不影响双向模式是否生效**（双向模式已不看 TriggerBox），
   但建议仍摆在弧附近便于查看；TriggerBox / ArcAssistBox 尺寸保持默认即可。

2. **给弧打 tag**：把你想要生效的倒角弧 actor（例如 `Blockout_Corner_Curved` 那类四分之一凸圆弧）
   加一个 actor tag，例如 `ZFlipTwoWayArc`。

3. **配体积参数**（编辑器选中体积 → Details）：

| 参数 | 值 | 说明 |
|---|---|---|
| `bEnabled` | true | |
| `bBidirectional` | **true** | 打开后走"面吸附式倒角"分支 |
| `LocalGravityADirection` | (0,0,-1) | 入口面重力（通常就是地面 -Z） |
| `LocalGravityBDirection` | 见下表 | **出口面重力 = 出口面外法线的反向** |
| `ArcAssistSurfaceTag` | `ZFlipTwoWayArc` | **必须**与弧上的 tag 完全一致（严格匹配） |
| `bArcAssistEnabled` | **false** | 双向模式下面吸附是唯一控制者，旧 ArcAssist 必须关 |
| `TwoWayFaceCaptureDriveSpeedCm` | 700 | 吸附后沿面驱动速度 |
| `TwoWayFaceCaptureStickAccelCm` | 6000 | 把球摁在凸面上的加速度 |
| `TwoWayFaceCaptureExitNormalDot` | 0.85 | 接触法线接近出口面多少度算"到了"→ 释放 |
| `TwoWayFaceCaptureGroundMinSpeedCm` / `MaxSpeedCm` | 280 / 800 | 从平面（A 面）进入时的速度窗 |
| `TwoWayFaceCaptureRejectAwaySpeedCm` | 80 | 明显朝反方向离开时拒绝触发 |
| `TwoWayArcContactMarginCm` | 20 | 球面与弧面的最大间隙（超过就不算接触） |

其余双向门（`TwoWayMinTriggerSpeedCm=20`、`TwoWayMinApproachSpeedCm=20`、`bTwoWayRequireSupport=true`、
`TwoWayMaxAirborneSeconds=0.20`、`bTwoWayRequireArcContact=true`、`TwoWayMaxLateralNormalDot=0.70`、
`TwoWayArcProgressMinDot=0.05`）保持默认即可。

### B 怎么取值（关键，取错就"碰到也不转"）

`B = -(出口面朝外的法线)`。常见两种摆法（同一套网格镜像摆放）：

| 弧的摆法 | 顶切面（入口，法线 +Z） | 竖直切面（出口）的外法线 | 应设 `LocalGravityBDirection` |
|---|---|---|---|
| `rot = (roll 0, pitch 90, yaw 0)` | y 较小那一侧 | **+Y** | **(0,-1,0)** |
| `rot = (roll 0, pitch 90, yaw 180)` | y 较大那一侧 | **-Y** | **(0,+1,0)** |

**一张图里如果同时有这两种镜像摆法的弧，就放两个体积、各用一个自己的 tag**（例如
`ZFlipTwoWayArc` 给 yaw180 那组、`ZFlipTwoWayArcOrig` 给 yaw0 那组），因为 tag 是严格匹配，
两个体积不会互相抢球。本机 `Re_Blockout` 就是这个做法。

> ⚠️ 别把同一个 tag 打到 B 取值相反的弧上，否则那组弧的出口判定永远不满足，球会被一直吸附着走不出弧。

### 网格摆放的坑（`Blockout_Corner_Curved` 这类灰盒）

- 枢轴在**盒角**不在盒心：actor 的 Location 是那个"内角"点，不是弧的中心。
  `pitch=90` 时，世界盒 = X `[loc.x-100, loc.x]`、Y `[loc.y, loc.y+100]`、Z `[loc.z, loc.z+100]`（每段 100³）。
- 弧面是半径 100 的四分之一圆，圆心就在 actor 的 Location 上。
- 想让"顶切面正好接住地面"，就把 actor 摆成"顶切面与地面同高、且切线落在地板边缘"。

## 三、验证（进游戏看这三类日志）

先把日志打开（编辑器控制台）：`Log LogTemp VeryVerbose`。然后看：

```
[GSOneWayRedirect] <体积名> armed mode=two-way-face-capture entry=-Y target=-Z A=(0,0,-1) B=(0,+1,0) ... assistSurfaces=N
[GSOneWayRedirect] <体积名> face-capture#1 arc=<弧的actor名> gap=<球面与弧面间隙> approach=<接近速度> current=(..) target=(..)
[GravityShift] dir=+Y revision=N reason=4 requester=GSRollingBallPawn_0
```

- 第一条：体积起来了，`assistSurfaces` 应该 ≥1（= 找到带该 tag 的弧）。若是 0 → tag 名字写错或弧没打 tag。
- 第二条：**咬住弧**（`gap` 要小，几十 cm 以内）。
- 第三条：**重力真的转了**（`dir` 从 (0,0,-1) 变成你设的 B 方向）。反射方向从竖直面滚回来时会看到它再转回 (0,0,-1)。

## 四、别踩这些

1. **双向模式不需要球心进 TriggerBox**：只要球**真实碰到打过 tag 的弧面**就会开始吸附。如果你的球撞上了弧却没反应，
   先查 tag 与 `ArcAssistSurfaceTag` 是否一致、`bArcAssistEnabled` 是否被误开。
2. **不要**再把 `bArcAssistEnabled` 打开来"帮一把"——两套力会互相抢，表现是球在弧上抖。
3. **不要**为了让它工作去改 `UGSRedirectorComponent`，也不要动那些旧的 `SM_LDI_Gravityshift_*` 转向器。
4. 出口判定 `TwoWayFaceCaptureExitNormalDot` **不是入口条件**，别拿它当"碰到就转"的开关。
5. 一张图里两套镜像弧 → 两个体积两个 tag（见上），不要共用一个。
