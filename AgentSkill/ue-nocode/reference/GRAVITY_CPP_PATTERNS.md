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

## 旋转重力式"转向器"(滑梯换面)实现模式(2026-09-11 转向器轮,PIE 正反 8 例全过)

**需求**:球碰滑梯 → 自动滑到它连接的墙面上、重力随之连续旋转(替代 G 键直接换向),**且双向生效**。

**六条定式(都是先踩坑后定的)**:
1. **双向:按"一对连接面"建模,不要按"出口方向"建模**。只给出口方向的设备天然只能单向(用户第二版反馈的原话:"你这个只有单项生效")。正确姿势:`GravityDirectionA/B`(两面必须垂直)= 转向器连接的两个面,**入口面由球当前重力识别**(与哪面更接近就是哪面),出口取另一面。90° 弯道下"入口行进方向恒等于出口重力方向",于是**进入判定(速度·出口方向≥min)一条规则覆盖正反双向**;先判"球是否在某个面上"(dot≥0.5)再判方向,背面/天花板路过不会误触发。`cross(入口上,出口上)` 得到的弯道轴在反向时自动反向,整套滑行/旋转机制不用改。
2. **重力按"滑行距离"旋转,不要按时间**:`PathCm += |v_actual|·dt; Alpha = PathCm/PathLength`——球被挡住/变慢时重力先不转;按时间会出现"球还在坡上、重力已转完",球被压死在半坡(实测:0.8s 转完 vs 球要 7s 才出坡)。
3. **滑行速度锁在"弯道当前切向"**:`forward = cross(当前上, 弯道轴)`,每帧 `SetPhysicsLinearVelocity(forward*RideSpeed)` 而非 AddForce——摩擦/坡面阻力全绕过,球快转快、球慢转慢。
4. **球面探针贴面(不然布尔网格会把球弹飞)**:每帧从球心沿 -Up 打 `Radius+120cm` 线,命中则 `指令速度 = VectorPlaneProject(forward*Speed, 命中法线) + 命中法线 × clamp(-空隙×6, ±Speed)`——指令速度永远切于**实际**接触面并按空隙吸回面上;理想弧线只作方向参考(实测:不做投影时碰撞解算吃掉 92% 位移)。
5. **状态机与会话**:Pawn 上 `bGravityRedirectActive` + `From/To/Current/Axis/Speed/PathCm`;`GetActiveGravityDirection()` 在过渡期返回**旋转中的中间方向** → 相机抬升、驱动平面、物理重力(经 GravityBody 的 `GravityDirectionOverride`)读同一个源,全程连续;**旋转走完才提交管理器**(提交瞬间两者一致,零跳变);滑行期间抑制 WASD/刹车/G/1-2-3,落地响应与自动反转直接 return(防半路弹跳/反转把球扯下来);释放由触发它的组件负责(球离开触发盒),另设 5s 超时保险。
6. 网格侧:建模工具布尔网格**没有简单碰撞体**,把资产 collision_trace_flag 改 `CTF_USE_COMPLEX_AS_SIMPLE` 球才真的被滑梯接住(多实例共用资产 → 一次改全生效)。

**逐帧诊断**:Pawn 加 `bRedirectDebugLog`(球位/实际速度/指令速度/接触体/空隙/重力上/进度),组件加 `bDebugLog`(armed/fired/released,含 A→B)→ 比盲调参数快一个数量级。

**测试运动状态的坑**:见 PIE_TESTING.md"注入速度活不过暂停帧"(自动驱动方向不同向时会把速度方向盖掉)与"相机航向是实例级残留状态"(绝对角度重设法)。

## 转向器触发与释放的两条补充定式(2026-09-11 第十五轮,PIE 实测)

**补 1:触发必须由"真碰到圆弧"决定,不能只看触发盒**
- 触发盒外扩只是**廉价先筛**;真正的门是"球面到滑梯表面距离 ≤ 半径+容差(默认 20cm)"。只按盒触发 → 球在半空就被旋转重力,手感诡异(用户反馈)。
- 距离怎么测(踩过一轮):
  - **球面扫掠(SweepSingleByChannel)不行**:球静止在地面上时扫掠起点已与地面重叠,扫掠立刻返回地面、永远打不到滑梯(所有触发全失效)。
  - `GetClosestPointOnCollision` 对"复杂碰撞当简单用"的网格**返回 -1**(文档要求 simple collision)。
  - **可行解:多方向线探针**——从球心沿 {朝网格包围盒最近点, 入口"下", 出口"下"} 打线,命中自身网格且距离 ≤ 半径+容差 → 碰到。线从球心出发不与地面重叠 ✓,实测正向进入正好在"球碰到坡面那一帧"触发。
- **贴住滑梯时要放宽速度/方向门**:反向(从墙上/天花板上下来)时球常被滑梯几何**顶停在坡面**(速度掉到几十),若仍要求"速度≥50 且方向 align≥0.35"就会被拒 → 表现为"逆不回去"。规则改成:接触为真 → 速度门不生效、方向门放宽到 -0.5(不是明显往外走就带走);没接触 → 按原门限。

**补 2:释放条件 = 离开触发盒 **且** 重力旋转已走完**
- 只按"离开盒"释放,会在球还没滑完弯道时留下一个"介于两个面之间"的重力,看着像"没转过去"。Pawn 暴露 `GetGravityRedirectProgress()`,组件在 `离开盒 && progress>=1` 才释放(另有 5s 超时保险)。

**调试法(一次到位)**:组件 `bDebugLog` 打开后,球在触发盒内**每帧**打 `gate[speed|face|align|touch]` + 球位/速度/align/touch;再加每 30 帧心跳 `tick ball=... inBox=... riding=...` 确认 tick 在跑。定位"为什么不触发"不再靠猜。

**测试运动状态的三个坑(本轮新增)**
- **导轨相机让 move_input 方向不可靠**:本关(测试案例)走导轨相机,驱动方向由导轨决定且**随球沿导轨的位置变化**——同一 move_input 在不同位置驱动方向不同。测运动改用"**注入世界速度 + 极小驱动力(50)只为关掉松键刹车**",并在多次脚本调用间**反复重设速度**(每次调用世界推进 ~0.4s)模拟持续驱动。
- **用例串场**:上一个滑行没释放就摆下一个用例,新设的重力会被旧 `GravityRedirectCurrent` 覆盖 → 用例间留足时间或重启 PIE。
- **测试坐标按 actor 实际位置推导**:关卡里的转向器可能被拖动(本轮就遇到 1/3 号沿 X 挪了 250~300);滑梯沿挤出方向平移是无害的,但写死坐标的用例会打空。

**补 3:撞侧面不触发——看"接触面法线",不要看速度**
- 需求:只有"正面圆弧接地那一块"才触发;撞滑梯侧壁(平直面)一律不动(用户反馈:撞侧面还是会动)。
- **只看速度方向不够**:球撞侧面后**弹开**,速度转一下 align 就变正,又被放行触发(实测:先按速度拒了两次,弹开后 align≈0 却触发)。
- **正解**:接触时取回**接触面法线**,`|dot(法线, 弯道轴)| > 0.7` → 撞的是侧壁(法线沿滑梯宽度方向=弯道轴)→ 不触发。球只要还贴着侧壁就永远不触发,与瞬时速度无关。
- 另加"**正面接地那块**"位置门:球心到**入口侧面**(地面/墙/天花板那一侧的面,= 网格包围盒在入口重力方向上的支撑面)的距离 ≤ 球半径 + 60cm。⚠ **符号坑**:`Center - Sign(N)*Extent` 会取到**对面**(顶面),所有正常触发会被拒;正确是 `Center + Sign(N)*Extent`。
- 方向门保留但加**低速豁免**:`align ≥ 0.35`,速度 < 100cm/s 时豁免(反向下滑常被顶停在坡面,方向已无意义)。
- 触发判定顺序:接触 → 侧壁(法线)→ 接地(位置)→ 最小速度 → 方向。调试时逐门打 `gate[touch|side|lip|speed|align]` 一目了然。
