# GravityShift C++ 模式与修复案例(z-flip / v5)

> 什么时候读这个文件:要改 GravityShiftCore/GravityShift 插件的 C++(重力、翻转、摄像机、刚体行为)、评审或排查"翻转不生效/摄像机不跟/按了没反应"。

## 当前架构速览(z-flip,v5)

- 插件模块 `GravityShift`(21 类),项目模块 `ZFlip`;源码 `D:\UE\z-flip\Plugins\GravityShift\Source\GravityShift\{Public,Private}`
- **重力只有两态**:`NEGATIVE_Z=(0,0,-1)` / `POSITIVE_Z=(0,0,1)`(枚举只暴露这两个,无 X/Y)
- 玩家 = `AGSRollingBallPawn`(物理滚球,禁用内建重力,自定义加速度);**禁用 ACharacter/CharacterMovement**
- 所有翻转走 `AGSGravityManager::CommitPolarity`(同步提交 + `OnGravityChanged` 广播 + revision++);GameMode `BeginPlay` 自动拉起 Manager 和 WorldStateManager(**不用手摆**)
- 输入 = **Tick 轮询 `IsInputKeyDown` + 边缘检测**(WASD 滚动力矩、G 翻转、E 交互、R 重置),不依赖输入栈(原因见 PIE_TESTING.md)
- 摄像机 = CameraPivot(绝对旋转)→ SpringArm → Camera,每 tick 从 Manager 读目标方向,`FQuat::Slerp` 插值(**禁 Euler 跨 180°**);球体/碰撞/网格不被翻转回调旋转(`DoesGravityFlipRotateBall()=false`)
- 自动反向:落地/坠落超阈值(速度 1400 / 距离 2200 cm)经 Manager 以 `FALL_THRESHOLD`/`LANDING_RESPONSE` 翻回;一次飞行至多消耗一次自动反向
- 测试场景(z-flip 测试案例.umap):3×GSBlockBase(Fixed/Gravity/Breaker 带 Profile)+ GravitySwitch + 慢速 SurfaceVolume + 上下 KillVolume;BP 壳由 `Plugins/GravityShift/Content/Python/v5/install_blueprints.py` 生成(14 BP + 12 DA,零污染)

## 两个必查 bug 模式(2026-09-02 实修)

0. **滚球力矩轴反了 = 操作镜像(W 后退、A 向右)**:让球滚向 `Desired` 的力矩轴是 **`Up × Desired`**,不是 `Desired × Up`——接触点运动学 `v_center = -wr·(Up×T)`,写反后球滚向反方向且左右镜像。配套:**松键制动**靠反向力矩(无输入且支撑时 `T = Up×(-v̂)`),物理滚球只靠摩擦会滑很远。
1. **弹簧臂滤掉翻转旋转**:`CameraPivot` 每 tick `SetWorldLocationAndRotation` 写绝对旋转,但中间 `SpringArmComponent` 若 `bInheritPitch=false`(模板常见写法),180° 翻转(本质是 pitch)传到相机前就被臂吃掉。表象:摄像机永不跟转,`CAM_UP` 恒等 (0,0,1)。
   修:`CameraPivot->SetUsingAbsoluteRotation(true)`(隔绝球体滚动渗入)+ 臂 `bInheritPitch/Yaw/Roll = true`。
2. **睡眠刚体无视 AddForce**:重力组件每 tick `AddForce(bAccelChange=true)`,但刚体落定睡眠后力全部无效。表象:翻转 ACCEPTED、`GetGravityDirection()` 变了,球/方块纹丝不动。
   修:`UGSGravityBodyComponent` 订阅 `OnGravityChanged`(`BeginPlay` AddUniqueDynamic / `EndPlay` Remove),handler 里 `TargetPrimitive->WakeRigidBody()`。
3. **关卡摆放 Actor 的 BeginPlay 早于 GameMode 自动 spawn 的 Manager**(时序家族第三例,v2 的 EnableInput 丢失是第一例):BeginPlay 时 `FindGravityManager` 返回空 → 注册/订阅全跳过且**无重试** → 该 Actor 永远不响应翻转(`GetGravityDirection()` 空管理器时恒返回 -Z)。**验证信号:`get_registered_body_count()` 只有球一个 = 关卡 Actor 全没注册**。
   修:**懒绑定**——注册+订阅挪进 `RefreshReferences`(两者都幂等),`TickComponent` 里 manager 为空就重试。
   **评审"自定义重力+翻转"类代码先查这几处。**

## Profile 默认值覆盖链(改阈值要改对地方)

生效优先级:**组件实例值 ← ApplyBallProfile(DA 字段) ← GSProfiles.h 类默认 ← DA 序列化值**。
- 原生 spawn 的 pawn `BallProfile=null` 时跑的是**组件头文件默认**(曾致 V_HIGH=900 而非 1400,棚顶落差全部误翻转)→ 修:Pawn 构造器 `ConstructorHelpers::FObjectFinder` 自动加载 `DA_GS_Ball_Default`
- 改"翻转模式/阈值"要同步改三层:组件默认、**GSProfiles.h 里 Profile 字段默认**(ApplyBallProfile 会覆盖组件)、DA 资产(未序列化的字段自动跟随类默认,已序列化的要显式改)
- 落地判定三带(2026-09-02 用户定版):`X≥2000` 反重力翻转(仅球触发,LandingResponse 只在球上)、`150<X<2000` 弹跳且**每周期一次**(`bBouncedSinceQuietLanding` 锁存,安静落地复位;否则固定弹速会无限弹)、`X≤150` 无反应。`AutoReverseMode=LANDING_IMPACT`(半空反飞已禁用)
- **反重力线必须高于"房间内最大落体冲击"** `sqrt(2·g·H_room)`:本房间地板↔棚顶落差 850cm → 冲击 ≈1650,阈值 1400 时球每次落顶都自动反翻,手动翻转被立刻打回、万物上下乒乓、方块贴不住天花板。抬到 2000(>房间内最大 1760)后翻转粘滞,方块可平稳贴顶;真正超高坠落仍会翻。改阈值改 DA_GS_Ball_Default 的 `landing_auto_reverse_at_speed_cm`(+ generate_data_assets.py 源)

## 历史:G/R 按键失效(v2 六向,2026-09-01 修通)

- 根因:BeginPlay `EnableInput+BindKey` 对关卡摆放实例不生效(输入栈投递丢失),DemoRoom 子 Actor 原生实例却正常
- 修复:弃 BindKey → Tick 轮询边缘检测(模式全文见 PIE_TESTING.md);重编译 14s(`dotnet UBT.dll <Target>Editor Win64 Development -project=... -WaitMutex -NoUBA`)
- 验收:OS 级注入真实按键,G=横移 2156cm、R=RESET 归位 ✅
- 旁注:DemoRoom 自带一个 ChildActor 的 Manager,PIE 里"Use exactly one"双 Manager 警告是**良性**的——两者状态同步,关卡方块跟的是关卡实例,别当 bug 修

## 调试与验收要点

- **动态行为观测必须 fire-and-read 三段式**(远程 python 执行期间 PIE 暂停),见 PIE_TESTING.md 坑 2
- API 探测:UFUNCTION 签名靠 TypeError 逐个补参;读状态用 getter(`get_gravity_direction()`);`gravity_revision`/`last_change_reason` 可作翻转证据
- **FALL_THRESHOLD 自动反向是设计行为**——翻转后球撞顶弹回,先查 `last_change_reason` 再怀疑 bug
- 编译命令与中文路径排雷见 `ue-cpp-build-cnpath` SKILL(引擎内置 dotnet + UBT.dll + `-NoUBA`,约 15s)
- v5 完整交接:`D:\UE\z-flip\HANDOVER_zflip.md`;遗留=用户实机 WASD/G/E/R 游玩验收 + z-flip git 建仓
