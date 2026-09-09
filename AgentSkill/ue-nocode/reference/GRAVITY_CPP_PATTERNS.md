# GravityShift C++ 模式与修复案例(z-flip / v5)

> 什么时候读这个文件:要改 GravityShiftCore/GravityShift 插件的 C++(重力、翻转、摄像机、刚体行为)、评审或排查"翻转不生效/摄像机不跟/按了没反应"。

## 当前架构速览(z-flip,v5)

- 插件模块 `GravityShift`(24 类),项目模块 `ZFlip`;源码 `D:\UE\z-flip\Plugins\GravityShift\Source\GravityShift\{Public,Private}`
- **重力六方向(v6,2026-09-03 起)**:`EGSGravityDirection` ±X/±Y/±Z;Manager 新 API `RequestGravityDirection/SetGravityAxis/ToggleCurrentAxis`,旧 ±Z 极性 API 保留兼容;每关默认方向+允许轴由 WorldStateManager `Gravity|LevelConfig` 配置并三重时序兜底(详见项目 HANDOVER §11)
- 玩家 = `AGSRollingBallPawn`(物理滚球,禁用内建重力,自定义加速度);**禁用 ACharacter/CharacterMovement**
- 所有翻转走 `AGSGravityManager::CommitPolarity`(同步提交 + `OnGravityChanged` 广播 + revision++);GameMode `BeginPlay` 自动拉起 Manager 和 WorldStateManager(**不用手摆**)
- 输入 = **Tick 轮询 `IsInputKeyDown` + 边缘检测**(WASD 滚动力矩、G 翻转、E 交互、R 重置),不依赖输入栈(原因见 PIE_TESTING.md)
- 摄像机双模式:关卡里有 `AGSCameraRail` → **轨相机**(相机套轨轴滑行、横切面锁死、滚转锁世界竖直、万向限位 35°/50°,PIE 核心验证见项目 HANDOVER §12);没有 → 旧跟随模式(CameraPivot 绝对旋转 → SpringArm → Camera,`FQuat::Slerp` 插值禁 Euler 跨 180°)。球体/碰撞/网格不被翻转回调旋转(`DoesGravityFlipRotateBall()=false`)
- 自动反向:落地/坠落超阈值(落地速度 2000 cm)经 Manager 以 `FALL_THRESHOLD`/`LANDING_RESPONSE` 翻回;一次飞行至多消耗一次自动反向
- 测试场景(z-flip 测试案例.umap):3×GSBlockBase(Fixed/Gravity/Breaker 带 Profile)+ GravitySwitch + 慢速 SurfaceVolume + 上下 KillVolume;BP 壳由 `Plugins/GravityShift/Content/Python/v5/install_blueprints.py` 生成(14 BP + 12 DA,零污染)

## 两个必查 bug 模式(2026-09-02 实修)

0. **滚球力矩轴反了 = 操作镜像(W 后退、A 向右)**:让球滚向 `Desired` 的力矩轴是 **`Up × Desired`**,不是 `Desired × Up`——接触点运动学 `v_center = -wr·(Up×T)`,写反后球滚向反方向且左右镜像。配套:**松键制动**靠反向力矩(无输入且支撑时 `T = Up×(-v̂)`),物理滚球只靠摩擦会滑很远。
1. **弹簧臂滤掉翻转旋转**:`CameraPivot` 每 tick `SetWorldLocationAndRotation` 写绝对旋转,但中间 `SpringArmComponent` 若 `bInheritPitch=false`(模板常见写法),180° 翻转(本质是 pitch)传到相机前就被臂吃掉。表象:摄像机永不跟转,`CAM_UP` 恒等 (0,0,1)。
   修:`CameraPivot->SetUsingAbsoluteRotation(true)`(隔绝球体滚动渗入)+ 臂 `bInheritPitch/Yaw/Roll = true`。
2. **睡眠刚体无视 AddForce**:重力组件每 tick `AddForce(bAccelChange=true)`,但刚体落定睡眠后力全部无效。表象:翻转 ACCEPTED、`GetGravityDirection()` 变了,球/方块纹丝不动。
   修:`UGSGravityBodyComponent` 订阅 `OnGravityChanged`(`BeginPlay` AddUniqueDynamic / `EndPlay` Remove),handler 里 `TargetPrimitive->WakeRigidBody()`。
3. **关卡摆放 Actor 的 BeginPlay 早于 GameMode 自动 spawn 的 Manager**(时序家族第三例,v2 的 EnableInput 丢失是第一例):BeginPlay 时 `FindGravityManager` 返回空 → 注册/订阅全跳过且**无重试** → 该 Actor 永远不响应翻转(`GetGravityDirection()` 空管理器时恒返回 -Z)。**验证信号:`get_registered_body_count()` 只有球一个 = 关卡 Actor 全没注册**。
   修:**懒绑定**——注册+订阅挪进 `RefreshReferences`(两者都幂等),`TickComponent` 里 manager 为空就重试。
4. **不滚转的相机下,移动基向量别用 `Up×Forward`**(2026-09-03):轨相机 roll 锁世界竖直,重力翻转(Up 反向)会让 `Up×Forward` 右向量反向而画面不翻 → **天花板/墙面 A/D 镜像**(用户实测)。修:移动基向量直接取**相机自身 forward/right 投影到支撑面**(相机 right 恒等于屏幕右,任何姿态都对);滚转跟随型相机两种算法等价,不受影响。配套:验证时读"相机朝向"要用 **CameraComponent** 的 right,`get_component_by_class(SceneComponent)` 拿到的是根碰撞体(物理翻滚中),读出来全是误导。
   **评审"自定义重力+翻转"类代码先查这几处。**

## UHT/编译期撞名(2026-09-03)

- **组件新 UFUNCTION 撞基类名**:给 `UActorComponent` 子类加 `IsActive()` 的 UFUNCTION → UHT 报 "Override of UFUNCTION 'IsActive' in parent 'UActorComponent' cannot have a UFUNCTION()"。`UActorComponent` 已占用 `IsActive`;新组件方法避开 `Is*/Get*` 常用名,或先查基类(本次改名 `IsDriving`)。
- **平滑/积分类 Tick 代码必须钳 dt**:`DeltaSeconds = FMath::Clamp(DeltaSeconds, 0.f, 0.1f)`——PIE 暂停恢复会给负 dt,指数平滑 α<0 会向目标反方向外推(现象:锁轴相机瞬间偏出后自愈,详见 PIE_TESTING.md 坑 2 旁注)。

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

## 平滑相机的"写后漂移"坑(2026-09-08 相机抖动轮,PIE 逐帧数据实证)

**症状**:相机移动"卡卡的"+瞬间小幅抖动;多张图同发 = 查共用的相机链,别怀疑关卡。

**根因模式(泛用,记牢)**:平滑组件的父级是**物理模拟的根**(翻滚球体等),且只把**旋转**设绝对(`SetUsingAbsoluteRotation`)、**位置仍是相对**——每帧 Tick 里 `SetWorldLocationAndRotation` 写完平滑位姿后,同帧物理步里父级一动,子组件继承父级当帧位移 → **这段未平滑位移绕过全部平滑层直进画面**。匀速=拖拽感,加速/急刹/碰撞帧=瞬间抖动。本质:"写时平滑、写后漂移"。

**修复模式**:枢轴位置同样绝对化——`SetUsingAbsoluteLocation(true)`(BeginPlay 应用,配 `bUseAbsoluteCameraLocation` UPROPERTY 默认开,留 A/B 口)。修复后逐帧实测漂移 0.00cm。

**逐帧数据采集法(关键,python 采样不出来)**:远程 python 执行期间世界暂停,组播采样只能拿到粗粒度。正确姿势=C++ 里加 UPROPERTY 门控的逐帧 `UE_LOG`(默认关),PIE 里开、跑完读 `Saved/Logs/<项目>.log` 用 python 解析。**打点读"写入前"的组件位置**(pivotPrev):它 - 上帧写入值 = 每帧泄漏量,旧行为=球位移,修复后=0。日志开关设在**PIE 实例**上立即生效(别走 CDO,见 PYTHON_API_PITFALLS)。

**A/B 复现法**:开关做成 UPROPERTY 且 BeginPlay 逐实例应用 → 细节面板切换即可对比新旧行为,免重编。

## 滚球 WASD 手感:力矩驱动会打滑,平面加速度才是正解(2026-09-08 实测定案)

- **症状**:力矩驱动(AddTorqueInRadians)的滚球移动"过慢+惯性大",调力矩档位终端速度几乎不动。实测真相:球在低摩擦接触下大量**打滑**(自转 ω·r≈3000 cm/s 而 |v|≈240)——力矩把球转起来了,接触摩擦没把转动转化成位移。
- **修复模式**:支撑面上的 WASD 驱动改用**平面加速度** `AddForce(Desired·DriveAccel, NAME_None, true)`(bAccelChange=质量无关加速度),绕开牵引耦合;松键刹车用**平面速度直接衰减** `Planar *= Exp(-ReleaseBrakeHz·dt)`(~3Hz≈1s 停稳),竖直分量原样保留。
- **经验公式(实测三组线性吻合)**:终端速度 ≈ DriveAccelerationCm×0.28(综合减速约 3.5/s,比重力体 Profile 的切向拖拽 0.35 大 10 倍,原因未深究——手感调参以实测为准)。目标终端 → 除以 0.28 得驱动力。
- ⚠ 标定必须在干净平地(无斜坡/结构/表面体积/薄地板);校准板要厚(≥5m)防高速穿透;位置+速度联合采样,单看速度全是地形噪声。切向拖拽是重力侧参数(影响掉落),别拿它调移动手感。

- **`FMath::VInterpNormalRotationTo(Current, Target, dt, Speed)` 的 Speed 单位是度/秒**:转 180° 要 180/Speed 秒——想要 ~0.5s 完成的方向摆动得传 360-400(2026-09-08 传 8 实测摆了 22 秒没到位)。同理注意 dll mtime:编辑器运行时 UBT 报 Succeeded 但可能没真正链接,重编前必关编辑器、编完必对 dll mtime

## 弹簧臂探针在棱边逐帧翻转 = 画面抖动;自建探针 + 去弹(2026-09-09 爬楼梯轮,逐帧日志实证)

**症状**:爬楼梯(Z 重力态)时画面抖动;平地上正常。

**根因模式(泛用)**:`USpringArmComponent` 自带碰撞探针在**台阶/平台棱边**处会**逐帧翻转**——本帧命中(臂长被压到 179)、下帧失配(回到 700)。相机位置 = 枢轴 + 臂向 × 实测臂长,于是相机每帧前后抽 ±500cm、俯仰 ±6.4°。**任何一阶滤波都压不住这种方波**(试过 `bEnableCameraLag`,周期≈帧长时只削幅值几乎无效)。

**修复模式**——关掉自带探针(`bDoCollisionTest=false`),自建探针 + 去弹:
1. 每帧沿"枢轴→期望相机位"打一条 `ECC_Camera` 线探针(`LineTraceSingleByChannel`,忽略自身);
2. 命中 → 立即收短(`FInterpTo` 8/s,防穿墙);**连续无命中 ≥ `ArmExtendHoldSeconds`(0.7s)才缓慢放长**(5/s)——翻转被去弹,相机稳定在安全距离,离开棱边后平滑拉远;
3. `TargetArmLength = 平滑臂长` 每帧写回;**抬升缩放(LiftScale)与瞄准高度 AimUp 必须同乘**、`CamPosApprox` 也用平滑臂长——否则臂一收短,枢轴抬升缩了而瞄准点没缩,俯仰会甩 13°、构图跳变。

**实测**:修复前楼梯段相机 X 每帧 ±500cm、俯仰摆到 +14.6°;修复后每帧平滑 +15~22cm、俯仰 −1°±3°。

**逐帧诊断法**:加 `bFallbackCamDebugLog` 门控日志(球位/枢轴/平滑前后目标/相机位/臂长/命中体/缩放/俯仰),再配一个 `bDebugAutoDriveForward` 调试开关持续前推(替代 OS 按键注入),脚本把球放到楼梯口即可自动爬完,读 `Saved/Logs/*.log` 解析。**后台节流下 python 采样只有 ~3 帧/秒,必须靠 C++ 逐帧日志。**

## 瞄准目标里不能用"瞬间换边"的源(2026-09-09 按 G 突变轮)

**症状**:按 G 翻转时画面"突然向上/下跳一下";位置通道已平滑但仍有突变。

**根因模式**:瞄准点用 `SupportUp`(=−重力)构造——G 按下瞬间它**瞬间反向**,瞄准点瞬移 ~284cm,俯仰一帧跳 ~22°,之后又随枢轴慢慢飘回。**判据:任何进入相机姿态计算的量,只要其源会瞬间换边,就会造成瞬时跳变。**

**修复**:瞄准点/判别项改用**已平滑的抬升方向** `CameraLiftDirection`(收敛后与 SupportUp 等价,过程中连续)。**验证相机"突变"必须同时采位置和旋转**——只采位置会漏掉俯仰跳变(本项目第一轮就是这么漏的)。
