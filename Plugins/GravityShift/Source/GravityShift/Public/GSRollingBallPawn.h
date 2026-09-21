// GravityShift v5 - physics rolling ball. No CharacterMovement, no capsule.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "GravityShiftTypes.h"
#include "GSRollingBallPawn.generated.h"

class AGSGravityManager;
class AGSWorldStateManager;
class UCameraComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UGSBallProfile;
class AGSBlockBase;
class UGSGravityBodyComponent;
class UGSLandingProfile;
class UGSLandingResponseComponent;
class UGSResettableComponent;
class UGSRailCameraComponent;
class UGSSurfaceReceiverComponent;
class USceneComponent;
class USphereComponent;
class USpringArmComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "GS Rolling Ball Pawn"))
class GRAVITYSHIFT_API AGSRollingBallPawn : public APawn
{
	GENERATED_BODY()

public:
	AGSRollingBallPawn();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<USphereComponent> BallCollision = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UStaticMeshComponent> BallMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<USceneComponent> CameraPivot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<USpringArmComponent> CameraArm = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UCameraComponent> Camera = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UGSGravityBodyComponent> GravityBody = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UGSSurfaceReceiverComponent> SurfaceReceiver = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UGSLandingResponseComponent> LandingResponse = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UGSResettableComponent> Resettable = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UGSRailCameraComponent> RailCamera = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<UGSBallProfile> BallProfile = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float StopTorqueAcceleration = 160.0f;

	// WASD 驱动的平面加速度(cm/s²,质量无关)。终端速度 ≈ 此值/切向拖拽(0.35)。
	// 只作用于支撑面上的 WASD 路径,重力/掉落/反弹不受影响。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float DriveAccelerationCm = 400.0f;

	// 松键刹车的平面速度衰减率(Hz):e^{-Hz·dt},~3 = 1 秒内基本停稳。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float ReleaseBrakeHz = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<AGSGravityManager> GravityManager = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<AGSWorldStateManager> WorldStateManager = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<UGSLandingResponseComponent> LandingResponseRef = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	bool bEnableNativePollingInput = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	bool bAutoPossessFirstPlayer = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey ForwardKey = EKeys::W;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey BackwardKey = EKeys::S;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey LeftKey = EKeys::A;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey RightKey = EKeys::D;

	// 旧 G 翻转已移除(2026-09-12 新机制):玩家重力只由转向器圆弧控制。

	// CameraPivot location is written in world space and does NOT inherit the
	// physics ball's intra-frame displacement (that bypasses all camera smoothing
	// and shows as small instant jitter). Turn off only to A/B the old behaviour.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera")
	bool bUseAbsoluteCameraLocation = true;

	// Per-frame rail-camera debug log (pivot pre-write position / ball / target).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Debug")
	bool bRailCamDebugLog = false;

	// Per-frame fallback-camera debug log (ball / pivot / actual arm length / lift
	// scale / aim pitch) — for diagnosing stair-climb camera shake.
	// 逐帧相机日志(默认关;诊断"相机怪怪的"时打开:arm/raw/hit/pitch/camup/g 全都有)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Debug")
	bool bFallbackCamDebugLog = false;

	// Debug: hold full forward drive every tick (stair-climb camera shake repro).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Debug")
	bool bDebugAutoDriveForward = false;

	// Debug: 世界坐标强制驱动方向(零向量 = 关闭)。用来**不靠输入**验证场景机制(例如
	// "球在高台上朝圆弧滚过去"):配合 bDebugAutoDriveForward 使用——后者让 ApplyMovement
	// 走驱动分支,前者把驱动方向从"相机相对"换成这个世界方向。测完清成零向量。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Debug")
	FVector DebugAutoDriveWorldDir = FVector::ZeroVector;

	// Debug: 强制进入瞄准态,不需要按右键。用途:**瞄准相机的穿模/贴脸淡出只能在瞄准态复现**,
	// 而本机 PIE 收不到注入的鼠标事件(UE 用 raw input,注入事件被忽略)→ 用脚本设这个开关,
	// 就能纯 Python 读取 FOV/SocketOffset/臂长/相机位来自动验收(2026-09-15 立)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Debug")
	bool bForceAimingDebug = false;

	// Debug: 转向器滑行逐帧日志(球位/实际速度/指令速度/接触法线/旋转进度)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Debug")
	bool bRedirectDebugLog = false;

	// Interact moved from E to F: E is now the camera-distance key (Q/E).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey InteractKey = EKeys::F;

	// Dismisses a center-screen pickup message (see ShowMessageAndLock).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey DismissMessageKey = EKeys::SpaceBar;

	// Player camera-distance keys: Q pulls the rail camera closer, E sends it back.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey TrailCloserKey = EKeys::Q;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey TrailFartherKey = EKeys::E;

	// Player speed keys: O slows the WASD drive down, P speeds it up (live).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey SpeedDownKey = EKeys::O;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey SpeedUpKey = EKeys::P;

	// O/P 每次按的步长与范围(终端速度 ≈ DriveAccelerationCm×0.28)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "1.0"))
	float DriveAdjustStepCm = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float DriveMinCm = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float DriveMaxCm = 7200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey ResetKey = EKeys::R;

	// —— 新瞄准机制(2026-09-12):按住右键出现准星,可改变重力的物体边缘发光,
	// 左键把它的重力在 掉下来/升起来(±Z) 之间切换。取代旧的 G/1/2/3。 ——
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey AimKey = EKeys::RightMouseButton;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey AimFireKey = EKeys::LeftMouseButton;

	// 准星射线最长距离(cm),超出的物体瞄不到。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input", meta = (ClampMin = "100.0"))
	float AimRangeCm = 2500.0f;

	// 是否正按住右键瞄准(HUD 据此画准星)。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Input")
	bool bAiming = false;

	// 当前准星锁定的可改变重力方块(未锁定为 null)。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Input")
	TObjectPtr<AGSBlockBase> AimedBlock = nullptr;

	// 无导轨相机的 Q/E / 滚轮调距:每按一次(滚一格)的步长。范围 220~900 见 AdjustCameraDistance。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "10.0"))
	float CameraDistanceStepCm = 60.0f;

	// 调距的平滑速度(越大越快到位)。调距写的是"期望臂长",UpdateCamera 每帧把
	// CameraArmLengthCm 朝它插值 → 拉近不再瞬跳(旧行为:探针立即压入,往近滚一格画面
	// 就"啪"地跳一个 CameraDistanceStepCm)。0 = 关闭平滑(退回瞬跳的旧手感)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float CameraZoomInterpSpeed = 10.0f;

	// —— 瞄准聚焦(TPS ADS 手感,2026-09-12) ——
	// 瞄准时相机 FOV 收到这个值(越小越"聚焦")。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Aim", meta = (ClampMin = "30.0", ClampMax = "110.0"))
	float AimTargetFOV = 55.0f;

	// 瞄准时相机臂长收到这个值(贴近肩后),松开右键恢复 Q/E 设定的距离。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Aim", meta = (ClampMin = "120.0"))
	float AimArmLengthCm = 250.0f;

	// FOV/臂长向目标收敛的速度(每秒插值系数)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Aim", meta = (ClampMin = "1.0"))
	float AimZoomSpeed = 8.0f;

	// 瞄准时 WASD 驱动力缩放(聚焦时移动放慢,更好瞄准)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Aim", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float AimDriveScale = 0.6f;

	// 越肩偏移(cm):瞄准时相机沿屏幕右方向让开,视线绕过球本体
	// (否则抬头瞄天花板上的方块会被球挡住)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Aim", meta = (ClampMin = "0.0"))
	float AimShoulderOffsetCm = 65.0f;

	// How long the "X轴不可用" hint stays on screen after a disallowed 1/2/3 press.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input", meta = (ClampMin = "0.0"))
	float AxisHintLifetimeSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement")
	bool bAllowAirControl = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement")
	bool bClampPlanarSpeed = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float RollTorqueAcceleration = 44.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float MaximumPlanarSpeedCm = 1600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float AirControlAccelerationCm = 900.0f;

	// 楼梯吸力(2026-09-13 用户需求):踩在楼梯上时给一个朝支撑面的小加速度,爬楼时把球
	// "摁"在台阶上、不从台阶棱角弹飞。**只对楼梯生效**——向下探针命中的 actor 名字/类名
	// 含 StairStickNameTag 才施力(默认 "Stairs" 匹配 Blockout_Stairs_Linear 等);
	// 其他任何表面(地板/墙/方块)完全不受影响。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement")
	bool bStairStickEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float StairStickAccelCm = 1500.0f;

	// 吸力探针长度(cm,球面之外):球被弹起后仍在其下方该距离内探到楼梯就继续吸。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float StairStickProbeReachCm = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement")
	FString StairStickNameTag = TEXT("Stairs");

	// 上坡时的吸力倍率(2026-09-15 用户反馈"吸力让上坡变难,应该只有下坡吸、上坡弱很多"):
	// 吸力按"球速沿重力方向的分量"插值——下坡/停着接近 1.0,上坡压到该倍率(默认 0.2)。
	// 1.0 = 恢复"上下坡一样吸"的旧行为;0.0 = 上坡完全不吸。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StairStickUphillScale = 0.2f;

	// ---- 楼梯"端点延续吸附"(2026-09-15 用户需求:上下端点往前一小块也要吸,不然还会飞出去)----
	// 原理:记住最后一次**确实踩在楼梯上**的位置与时刻;离开楼梯后,只要球还在
	// StairStickEndMarginCm 之内、且距上次踩楼梯不超过 StairStickEndSeconds,就继续施加
	// 一个**固定小**吸附(不乘上下坡系数——飞出去恰恰多发生在上坡末段最后一级台阶)。
	// 只由"真实命中 Stairs"点亮,所以其它表面依旧零影响;走远/超时自动失效。
	// ---- 楼梯"离面瞬间/端点"防飞出去(2026-09-15 用户二次反馈:上坡出楼梯末端还是飞)----
	// 空中(刚被台阶棱角弹起、或冲出最后一级)时球没有接触摩擦,此时朝支撑面拉一把只赚不亏
	// ——所以**不受上下坡系数削弱**(上坡按 0.2 算几乎等于没有,正是"出末端飞出去"的成因)。
	// 骑在台阶上(有接触)时才走分坡吸力:上坡弱(不给爬坡添阻),下坡强(抓地不弹)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float StairStickAirborneAccelCm = 1400.0f;

	// ---- **强化楼梯**:只对点名的那一段生效(2026-09-15 用户:"我让你只改那一段楼梯")----
	// 逗号分隔的名字片段;actor 名字/标签 或 类名 含任一片段才算"点名楼梯"。命中后:
	//   · 吸力/影响区用下面这组 Boost* 值(更大更猛);
	//   · 且**只在下坡趋势时**吃两道削速度硬约束(防飞);
	// 没命中的楼梯一切照基础值走,且**永不削速度**——爬坡要靠"向上的速度"爬上台阶,
	// 削了就直接爬不动(用户实测"其他楼梯都上坡上不了了")。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement")
	FString StairStickBoostNameTag = TEXT("Linear3,Linear4,Linear5,Linear6");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float StairStickBoostAccelCm = 3200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float StairStickBoostAirborneAccelCm = 2200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float StairStickBoostEndAccelCm = 1400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float StairStickBoostProbeReachCm = 280.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float StairStickBoostEndMarginCm = 280.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float StairStickBoostEndSeconds = 0.8f;

	// ③ 下坡限速(仅点名楼梯 + 下坡趋势):台阶是**不连续**的,球速一快就会在每级边缘起跳、
	//    连续腾空"飞"下楼梯 —— 抛物线靠"朝下的力"拉不回来(力只能让它落得更快,落点还是
	//    跳过的那些台阶),必须限速:限速后单位时间跨越的台阶少、下坠量远小于台阶高,球基本
	//    贴着台阶滚。0 = 不限速。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float StairStickBoostMaxSpeedCm = 700.0f;

	// 防飞硬约束(仅点名楼梯 + **下坡趋势**时生效):
	//   ① 骑在台阶上:削掉"朝离开支撑面方向"的法向分量(沿面滚可以、被棱角顶飞不行);
	//   ② 空中仍探到台阶:把"逆重力(上抛)"分量削掉(上抛当场归零)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StairStickBoostNormalKill = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StairStickBoostLiftKill = 1.0f;

	// 逐帧楼梯诊断日志(默认关):打印 在楼梯上/空中/位置/速度 —— 楼梯"飞"是**亚秒级**弹跳,
	// 0.5s 级采样看不到,只能靠逐帧日志定位(见 PIE_TESTING 的"逐帧数据"章)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Debug")
	bool bStairDebugLog = false;

	// 离开楼梯后"端点外一小块"的固定小吸附(见下方端点延续说明)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float StairStickEndAccelCm = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float StairStickEndMarginCm = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float StairStickEndSeconds = 0.5f;

	// 控制基向量随相机角度自适应(2026-09-13 用户定则):视线与支撑面越"正面相对"
	// (FaceOn=|视线·支撑上|→1),移动基越向"屏幕相对"过渡——W 从"视线在面内的投影"
	// 渐变到"屏幕上方向在面内的投影"(正对墙面时 W=向上爬、A/D=沿墙),视线与面平行
	// 时完全沿用旧行为。中间角度在 [Min,Max] 区间线性混合,避免换映射的突跳。
	// 设 false 恢复旧行为(墙上 W=沿墙横滚、A/D=攀爬)供 A/B 对照。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement")
	bool bAdaptiveDriveBasis = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DriveBasisFaceOnMin = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DriveBasisFaceOnMax = 0.85f;

	// 无导轨相机滚转锁世界竖直(与导轨相机同一原则):G 翻转只平移跟球、画面不颠倒
	//(颠倒视角会晕 3D,用户定案)。设 true 恢复旧的随重力翻转行为。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera")
	bool bCameraFlipsWithGravity = false;

	// 角度微调:轴向瞄准"球 + 支撑面上方 AimUp"——抬升侧翻转时自动把球压回画面
	// 下三分之一(地板态微俯/天花板态微抬);AimUp = Base + Swing×dot(抬升方向,支撑上)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float CameraAimUpBaseCm = 142.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera")
	float CameraAimUpSwingCm = -7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.01"))
	float CameraFlipDurationSeconds = 0.35f;

	// 无导轨第三人称:枢轴沿当前"上"轴抬高,让球出现在画面下三分之一(常见第三人称
	// 取景,贴近后期导轨相机的观感);0 = 球居中(旧行为)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float CameraPivotLiftHeightCm = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float CameraFollowInterpSpeed = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float CameraYawDegreesPerMouseUnit = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float CameraPitchDegreesPerMouseUnit = 0.25f;

	// 用户设置里的灵敏度倍率(UGSSettingsSaveGame)。BeginPlay 读一次存档缓存,
	// 不每帧读盘;设置面板拖动时会直接改这里,当场生效。
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float MouseSensitivityMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float MaximumCameraPitchDegrees = 70.0f;

	// 俯仰默认 0:配合枢轴抬高,球落在画面下三分之一;鼠标仍可自由俯仰。
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera")
	float CameraPitchDegrees = 0.0f;

	// 已被 CameraAimHeading 取代(保留是为了兼容旧序列化实例,不再参与计算)。
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera")
	float CameraYawDegrees = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float CameraArmLengthCm = 700.0f;

	// 相机臂长放长速度(自建探针命中→立即收短;连续无命中 ArmExtendHoldSeconds 后才按
	// 此速度放长,台阶边缘探针逐帧翻转被去弹,相机不再弹跳)。越大回位越快。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.1"))
	float ArmLengthInterpSpeed = 5.0f;

	// "连续无命中"需要保持多久才允许放长臂(去弹窗口)。爬楼梯时探针命中/无命中以
	// ~1Hz 交替,窗口短于无命中周期相机就会来回抽——0.7s 足够盖住,又不会让离墙后的
	// 拉远显得拖沓。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float ArmExtendHoldSeconds = 0.7f;

	// 自建探针的球半径(cm):用球扫掠替代单线,斜擦棱边/贴面时提前收短,防近裁剪面穿透。
	// ⚠ 别调太大(2026-09-15 教训:18 太大):球半径一大,**只是从相机侧边擦过、根本不在
	// 视野里**的墙面也会立刻命中探针 → 臂长被钉在最短,相机"转不动"。9 ≈ 覆盖近裁剪面
	// 需要的余量(相机不会穿进去),又不会把侧边墙当成正前方障碍。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "1.0"))
	float CameraProbeRadiusCm = 9.0f;

	// 被挤窄时的取景兜底(cm,2026-09-15 用户"相机转向很困难"):臂被探针压到很短时,球会
	// 糊住整个画面(相机几乎贴在球上,球半径 50 > 臂长)。此时**额外抬高枢轴**,让相机稍微
	// 从球上方看——球不再糊屏,而且抬高后常常正好绕开原来挡住臂的那面墙,臂还能自己长回来。
	// ⚠ **默认 0(关)**:实测抬枢轴会把"瞄准球"的俯仰一起压下去(80cm→-52°、150cm→-60°),
	// 变成盯着地面、看不见前方,反而更像"转不动"。所以默认不开;真要试就填 60~80,并接受
	// 俯仰变化。贴脸时球本身是半透明 + 单面材质(相机进到球里也看不到背面),不挡视线。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float CameraSqueezeLiftCm = 0.0f;

	// 挤窄时的**视场补偿**(deg,2026-09-15 用户"有些地方转不了"):楼梯槽这种地方球身后只有
	// 几十厘米,相机物理上退不出去 → 画面里几乎只剩球。这时随"挤窄程度"把 FOV 拉宽(最多
	// +该值),让玩家还能看见球周围的地形。比"抬高枢轴"温和:不动俯仰、也不穿模。
	// 0 = 关掉(回到固定 FOV)。
	// ⚠ 默认 0(关):2026-09-15 实测"开了反而更难转"(FOV 一变宽,近处观感更差),所以默认关。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0", ClampMax = "40.0"))
	float CameraSqueezeFovBoostDeg = 0.0f;

	// 挤窄时"瞄准俯仰"的保留比例(2026-09-15 用户:"狭窄有阻挡的地方想对特定方向转相机转不过去")。
	// 相机臂被压短时几乎贴在球上,"把球压到画面下三分"已经没有意义,只会让视线一直往下扎 -
	// 玩家想平视就得全程顶着鼠标拖、一松手又被拉回低头。按挤窄程度把这段俯仰衰减到该比例
	// (满臂时 =1.0 完全不衰减,最挤时约等于该值)。0.3 = 最挤时只保留三成。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CameraSqueezeAimPitchScale = 0.3f;

	// 探针命中后额外收短的余量(cm):相机与墙面之间保留的安全空隙。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float CameraProbeMarginCm = 6.0f;

	// 球贴脸遮挡:臂塌缩到 Hide 以下(球占满画面)时把球调成半透明,回到 Show 以上恢复。
	// 纯视觉,不影响物理;瞄准中(有越肩偏移让开视线)不处理。滞回防阈值附近闪烁。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float BallMeshHideBelowArmCm = 230.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float BallMeshShowAboveArmCm = 300.0f;

	// 贴脸遮挡的**屏幕占比触发**(2026-09-15 用户反馈"挡住屏幕大块却不变半透明"):
	// 光看臂长不够——球大/视场窄时臂还很长就已经糊住半屏。改判"球在画面上的角半径":
	// asin(球半径 / 相机到球距离) ≥ 起始角就淡出,回到结束角以下才恢复(滞回防闪烁)。
	// 与上面臂长阈值是"或"关系,任一满足即淡出。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "1.0", ClampMax = "60.0"))
	float BallMeshFadeStartAngleDeg = 13.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0", ClampMax = "60.0"))
	float BallMeshFadeEndAngleDeg = 9.0f;

	// 贴脸时球的不透明度(0.5 = 50% 透明)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BallMeshFadeOpacity = 0.5f;

	// 瞄准(RMB)时是否也做贴脸淡出(2026-09-15 用户反馈后默认打开)。
	// 旧行为是"瞄准态不处理",但瞄准臂只有 AimArmLengthCm(250)且 FOV 收窄——球又大又近,
	// 正是最挡视野的时机,于是变成"挡住大半屏却不淡出"。要回到旧行为把这个开关关掉。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera")
	bool bFadeBallWhileAiming = true;

	// 贴脸时换上的半透明材质(需带 Opacity 标量参数)。为空则退回"整球隐藏"的老行为。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera")
	TObjectPtr<UMaterialInterface> BallMeshFadeMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Interaction", meta = (ClampMin = "0.0"))
	float InteractionRadiusCm = 320.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Velocity")
	float ManualVelocityRetention = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Velocity")
	float AutomaticVelocityRetention = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Velocity", meta = (ClampMin = "0.0"))
	float AutomaticMaxCarrySpeedCm = 700.0f;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ApplyBallProfile(UGSBallProfile* NewProfile);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void RefreshSystemReferences();

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetMoveInput(FVector2D NewMoveInput);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void AddCameraLookInput(float YawDeltaDegrees, float PitchDeltaDegrees);

	// 相机航向(世界方向,只受鼠标偏航影响)。**每帧都会被重新投影到"⊥ 支撑上轴"平面并存回**:
	// 它由 AddCameraLookInput 绕"当前相机上轴"旋转,而俯仰大 / 重力翻转 / 过渡期间那个上轴是倾斜的,
	// 航向会被越转越"竖",最终与上轴近乎平行 → 偏航退化(2026-09-15 用户:"往右拽没用、只能往左,
	// 完全转不动")。存回投影向量即可永久消除这种竖直漂移。

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	EGSGravityRequestResult RequestManualGravityFlip();

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	EGSGravityRequestResult RequestGravityPolarity(EGSGravityPolarity NewPolarity, bool bForce);

	// Requests an arbitrary gravity direction (script/level use).
	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	EGSGravityRequestResult RequestGravityDirection(EGSGravityDirection NewDirection, bool bForce);

	UFUNCTION(BlueprintPure, Category = "GravityShift")
	EGSGravityDirection GetCurrentGravityDirection() const;

	// 当前生效的重力方向(单位向量)。转向器过渡期间返回过渡中的中间方向(≠管理器方向),
	// 相机的"支撑上"、驱动平面、物理重力全部读这一个源,滑行全程连续不跳变。
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FVector GetActiveGravityDirection() const;

	// ---- 转向器:定向重力过渡 + 沿弯道滑行 ------------------------------------
	// 球碰到转向器后由本状态机接管:重力方向沿弯道平滑旋转(按滑行距离推进,球滚得
	// 快转得快,不会"球还在坡上重力已转完"),期间速度锁在弯道当前切向(球被重力压在
	// 滑梯面上滑过去),玩家输入被抑制。旋转走完提交管理器;滑行持续到球离开滑梯
	// (由转向器调 EndGravityRedirect 释放)。
	//   TargetGravityDirection 滑出后的重力方向(连接面的"下")
	//   RideSpeedCm             滑行速度
	//   BendAxis                弯道旋转轴(≈ 转向器网格挤出方向)
	//   RidePathLengthCm        滑过多少厘米完成重力旋转
	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void BeginGravityRedirect(FVector TargetGravityDirection, float RideSpeedCm, FVector BendAxis, float RidePathLengthCm);

	// 释放滑行(球已离开滑梯):若旋转尚未走完则补完并提交,然后交回玩家控制。
	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void EndGravityRedirect();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	bool IsGravityRedirecting() const { return bGravityRedirectActive; }

	// 转向器滑行进度(0..1):按"实际滑行距离 / RidePathLengthCm"推进,1 = 重力旋转已走完
	// (未在滑行时返回 1)。转向器用它在释放前确认旋转已走完,避免留下半途的重力。
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	float GetGravityRedirectProgress() const;

	// ---- 特殊滑梯:面吸附(2026-09-15 用户需求,只给关卡里那条独一无二的滑梯)------
	// 与上面的"弯道滑行"是两套算法:弯道滑行要求球已经骑在某一面上、沿 90° 圆弧滚出去;
	// 面吸附解决的是"球**碰到竖直面(垂直于地面的那一面)**"这一下——碰到就把球吸附在
	// 面上,沿曲面把它自然地带到平面(平行地面的那一面),走完时重力正好转成该面的重力
	// (通常=竖直向下)。方向推导按"入口面法线→出口面法线"算旋转轴,不依赖网格朝向。
	//   ChuteActor      滑梯本体(吸附期间只认它的面,不吸别的物体)
	//   ContactNormal   进入瞬间的接触面法线(竖直面,例如 +Y)
	//   ExitGravity     走完后要转到的重力方向(例如 (0,0,-1) = 向下)
	//   DriveSpeedCm    沿面驱动速度;StickAccelCm 压向面的加速度
	//   ExitNormalDot   接触法线与出口面法线的余弦达标即释放(0.85 ≈ 32°)
	//   bGrounded       **温和吸附**(2026-09-16 双向需求):true = 球是自己在地面/高台上
	//                   滚到圆弧的,保留它自己的切向速度(只钳进 [FloorSpeedCm, CeilSpeedCm]),
	//                   法向也只切掉"正在离开面"的那一半——玩家主动滚过去时不会被定速拽一下。
	//                   false = 原硬吸附:进入瞬间换成定速沿面驱动(给"弹起来擦到面"的球)。
	//   FloorSpeedCm / CeilSpeedCm  温和吸附的切向速度下限/上限(硬吸附忽略这两个值)。
	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void BeginFaceCapture(AActor* ChuteActor, FVector ContactNormal, FVector ExitGravity,
		float DriveSpeedCm, float StickAccelCm, float ExitNormalDot,
		bool bGrounded, float FloorSpeedCm, float CeilSpeedCm);

	// 放弃吸附(超时/脱面/出口判定已到时调用):补完重力旋转并交回玩家控制。
	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void EndFaceCapture();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	bool IsFaceCapturing() const { return bFaceCaptureActive; }

	// 刚结束一次面吸附后的冷却(2026-09-16 双向必需品):A→B 把球送上墙的那一帧,球还贴着
	// 同一段圆弧、重力已经是墙的重力——本关卡三个滑梯件叠着摆,触发盒互相重叠,不设冷却
	// 会被另一个件立刻反向吸回高台(来回弹)。冷却期内球已沿墙走开,探针打不到圆弧了。
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	bool IsFaceCaptureCoolingDown() const;

	// 吸附中的实时接触面法线(转向器日志用;未吸附时返回零向量)。
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FVector GetFaceCaptureNormal() const;

	// Transient "X axis unavailable" feedback (driven by 1/2/3 on disallowed axes).
	UFUNCTION(BlueprintPure, Category = "GravityShift")
	bool IsAxisHintActive() const;

	// 无导轨相机枢轴抬升方向(瞬时状态,非反射):跟随"支撑面外侧"(=−重力)平滑摆动,
	// 让天花板/墙面态的相机始终在球靠房间的一侧、球保持下三分之一取景。
	FVector CameraLiftDirection = FVector::UpVector;

	// 无导轨相机枢轴位置平滑(瞬时状态,非反射;速率=Profile 的 CameraFollowInterpSpeed)。
	FVector SmoothedPivotLocation = FVector::ZeroVector;
	bool bPivotSmoothed = false;

	// 平滑后的实测臂长(探针塌缩会逐帧翻转,直接喂 LiftScale 会让枢轴高度抽动)。
	float SmoothedArmLengthCm = -1.0f;

	// 探针"连续无命中"计时:命中立即压入、连续无命中 ArmExtendHoldSeconds 后才放长(去弹翻转)。
	float ProbeClearSeconds = 0.0f;

	// 探针朝向用的"上一帧瞄准俯仰":探针必须沿相机真实最终朝向(含 AimPitchDeg 微调)
	// 打,否则视线偏最多 ~17°,探针判"安全"而相机已进墙("容易穿模"根因之一)。
	// 取上一帧值避免与 LiftScale→AimPitchDeg 形成循环依赖。
	float LastAimPitchDeg = 0.0f;

	// 球网格隐藏滞回状态(瞬时,非反射)。
	bool bBallMeshHidden = false;

	// 半透明用的动态材质实例 + 球原本的材质(退出贴脸时还原)。
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BallMeshFadeMID = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> BallMeshOriginalMaterial = nullptr;

	// ---- 转向器过渡状态(瞬时,非反射) ----
	bool bGravityRedirectActive = false;
	bool bGravityRedirectRotationCommitted = false;
	FVector GravityRedirectFrom = FVector(0.0, 0.0, -1.0);
	FVector GravityRedirectTo = FVector(0.0, 0.0, -1.0);
	FVector GravityRedirectCurrent = FVector(0.0, 0.0, -1.0);
	FVector GravityRedirectAxis = FVector(1.0, 0.0, 0.0);
	float GravityRedirectSpeed = 700.0f;
	float GravityRedirectPathLength = 320.0f;
	float GravityRedirectPathCm = 0.0f;
	float GravityRedirectHoldElapsed = 0.0f;
	// 贴面吸附:探针长度 = 球半径 + 该值;法向吸附速度 = 空隙 × 该增益(限速,不瞬移)。
	float SurfaceFollowRangeCm = 120.0f;
	float SurfaceFollowGain = 6.0f;
	// 保险上限:球卡住/组件异常没释放时也不会永久夺走控制。
	float GravityRedirectMaxSeconds = 5.0f;
	void UpdateGravityRedirect(float DeltaSeconds);

	// ---- 特殊滑梯:面吸附状态(瞬时,非反射) ----
	bool bFaceCaptureActive = false;
	// 滑梯本体:必须 UPROPERTY 持有,否则 GC 可能在 PIE 重启间隙回收它,下局变悬空指针。
	UPROPERTY(Transient)
	TObjectPtr<AActor> FaceCaptureActor = nullptr;
	FVector FaceCaptureEntryNormal = FVector(0.0, 1.0, 0.0);
	FVector FaceCaptureExitNormal = FVector(0.0, 0.0, 1.0);
	FVector FaceCaptureAxis = FVector(1.0, 0.0, 0.0);
	FVector FaceCaptureCurrentNormal = FVector(0.0, 1.0, 0.0);
	FVector FaceCaptureGravityFrom = FVector(0.0, 0.0, -1.0);
	FVector FaceCaptureGravityTo = FVector(0.0, 0.0, -1.0);
	FVector FaceCaptureGravityCurrent = FVector(0.0, 0.0, -1.0);
	float FaceCaptureSpeedCm = 700.0f;
	// 温和吸附(=球自己在地面/高台上滚进圆弧那一侧)用:切向速度不再定速,而是把球自己的
	// 速度钳进 [Floor, Ceil];下限保证球慢也能走完圆弧,上限防"高速冲进来被加重力转出来的
	// 额外动能"顶飞。
	bool bFaceCaptureGrounded = false;
	float FaceCaptureFloorSpeedCm = 300.0f;
	float FaceCaptureCeilSpeedCm = 900.0f;
	// 探针连续探不到面的累计时长(见 FaceCaptureSurfaceLostSeconds)。
	float FaceCaptureSurfaceLostAcc = 0.0f;
	// 释放后的冷却时长(s)与上次释放时刻(见 IsFaceCaptureCoolingDown)。
	float FaceCaptureReleaseCooldownSeconds = 0.5f;
	float FaceCaptureReleaseTime = -1000.0f;

	// ---- 楼梯强化状态(瞬时,非反射) ----
	// 垂直速度的指数均值:用来判"当前是不是在下坡"。硬约束只在下坡趋势时生效——
	// 光看瞬时速度方向区分不了"爬台阶(需要向上速度)"和"被顶飞",趋势能区分。
	float StairVzTrendCm = 0.0f;
	// 上次踩到的楼梯是不是"点名强化"的那一类(离开楼梯后的端点延续要靠它决定强度)。
	bool bLastStairBoosted = false;

	// ---- 楼梯端点延续吸附状态(瞬时,非反射) ----
	// 最后一次"确实踩在楼梯上"的球位置与时刻;离开楼梯后的一小块里靠它判断还在端点区。
	FVector LastStairContactLocation = FVector::ZeroVector;
	float LastStairContactTime = -1000.0f;
	bool bHasStairContact = false;
	float FaceCaptureStickAccelCm = 2500.0f;
	float FaceCaptureExitNormalDot = 0.85f;
	float FaceCapturePathCm = 0.0f;
	float FaceCaptureElapsedSeconds = 0.0f;
	bool bFaceCaptureGravityCommitted = false;
	// 吸附参数(实例级,可在蓝图层调):探针长度 = 球半径 + Reach;法向合拢增益;转完路程;超时。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|FaceCapture", meta = (ClampMin = "10.0"))
	float FaceCaptureProbeReachCm = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|FaceCapture", meta = (ClampMin = "0.5"))
	float FaceCaptureGapGain = 16.0f;

	// 探针打到的**不是本滑梯件**时的连续性兜底:命中面的法线跟当前法线余弦 ≥ 该值就认它
	// 是"同一张连续曲面"照常采用。必要性(2026-09-16):球绕到墙角后探针打到的是墙体网格
	// (墙和滑梯本来就拼在一起)而不是滑梯件 → 旧逻辑判"脱面":法线冻结、切向恒住,出口判定
	// 永远不满足,球被恒定切向速度甩出去(用户反馈"跑太快会飞出去"的机制之一)。
	// 有了它,墙面法线能正常更新 → 出口判定当场满足 → 正常落到墙上。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|FaceCapture", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FaceCaptureSurfaceNormalMinDot = 0.5f;

	// 连续多少秒完全探不到面就放开(补完重力、交回控制):保险,不让"法线冻结+恒速"把球甩飞。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|FaceCapture", meta = (ClampMin = "0.05"))
	float FaceCaptureSurfaceLostSeconds = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|FaceCapture", meta = (ClampMin = "10.0"))
	float FaceCaptureRotateOverCm = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|FaceCapture", meta = (ClampMin = "0.2"))
	float FaceCaptureMaxSeconds = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|FaceCapture")
	bool bFaceCaptureDebugLog = false;

	void UpdateFaceCapture(float DeltaSeconds);

	// 名字/标签 或 类名 是否含 TagsCsv(逗号分隔)里任一片段:用于楼梯的"点名强化"判定。
	bool MatchesStairTag(const AActor* Actor, const FString& TagsCsv) const;

	UFUNCTION(BlueprintPure, Category = "GravityShift")
	FString GetAxisHintText() const;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	bool TryInteract();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FText GetCurrentInteractionText() const;

	// Shows Message center-screen and suppresses gameplay input until the player
	// presses DismissMessageKey. Used by pickups/key hints.
	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ShowMessageAndLock(const FText& Message);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void DismissPendingMessage();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	bool IsMessageLocked() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FText GetPendingMessage() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FKey GetMessageDismissKey() const;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ResetToCheckpoint();

	// O/P 键:游戏内实时调 WASD 驱动力(Direction −1=减速 / +1=加速,步长/范围见
	// DriveAdjustStepCm 等参数;终端速度 ≈ DriveAccelerationCm×0.28)。
	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void AdjustDriveSpeed(float Direction);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FVector GetBallLinearVelocity() const;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetBallLinearVelocity(FVector NewVelocity, bool bAddToCurrent);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FVector GetCameraUpVector() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FVector GetTargetCameraUpVector() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	EGSGravityPolarity GetCurrentGravityPolarity() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	bool DoesGravityFlipRotateBall() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	USphereComponent* GetBallCollisionComponent() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	UCameraComponent* GetBallCameraComponent() const;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
	FVector2D MoveInput = FVector2D::ZeroVector;
	FVector CurrentCameraUp = FVector::UpVector;
	FVector TargetCameraUp = FVector::UpVector;
	FQuat CurrentCameraRotation = FQuat::Identity;
	bool bCameraRotationReady = false;
	bool bRailCamActive = false;
	bool bInteractKeyWasDown = false;
	bool bResetKeyWasDown = false;
	bool bTrailCloserKeyWasDown = false;
	bool bTrailFartherKeyWasDown = false;
	bool bSpeedDownKeyWasDown = false;
	bool bSpeedUpKeyWasDown = false;
	bool bAimKeyWasDown = false;
	bool bAimFireKeyWasDown = false;
	bool bDismissKeyWasDown = false;

	bool bInputLocked = false;
	FText PendingMessage;

	float AxisHintExpireTime = -1.0f;
	FString AxisHintText;

	// 调距的**期望**臂长(平滑目标,由 Q/E / 滚轮写)。<0 = 未初始化,首次使用时取当前
	// CameraArmLengthCm 再开始插值。
	float ZoomDesiredArmCm = -1.0f;

	// ADS 状态:非瞄准时的臂长基线(Q/E 改动实时反映进来)与开局 FOV 基线。
	float NonAimArmLengthCm = 0.0f;
	float DefaultCameraFOV = 0.0f;
	// 瞄准解除后的"快速回弹窗口"(世界秒):窗口内探针放长不等驻留、速度×3,
	// 解决"抬头瞄准松开后仍贴着拉近状态"的问题。
	float FastArmExtendUntilSeconds = -1.0f;

	FQuat BuildCameraRotation(const FVector& UpVector, float AdditionalPitchDegrees = 0.0f) const;
	// 世界航向(单位向量):无导轨相机的水平瞄准方向。鼠标转向绕当前重力上轴旋转它,
	// G 翻转(上轴 ±Z 互换)不改变航向 → 视角翻转后仍对准同一个世界方向。
	FVector CameraAimHeading = FVector::ForwardVector;
	void UpdateCamera(float DeltaSeconds);

	// 贴脸遮挡:按开关换/还原球网格的半透明材质(没配材质则退回整球隐藏)。
	void SetBallMeshFaded(bool bFaded);
	void ApplyMovement(float DeltaSeconds);
	void PollNativeInput();
	// 新瞄准机制:每 tick 更新 RMB 瞄准状态/中心射线/高亮切换;锁定时左键翻转方块重力。
	void UpdateAiming();
	// 无导轨相机的 Q/E / 滚轮调距:写的是期望臂长 ZoomDesiredArmCm(无轨取景的实际臂长源
	// 是 CameraArmLengthCm,由 UpdateCamera 的平滑层追上来),不是弹簧臂的 TargetArmLength
	// ——后者每帧被探针覆写(Fraction 可为小数,滚轮会传小数)。
	void AdjustCameraDistance(float Fraction);
	// Q/E 与鼠标滚轮共用的调距入口:轨相机在驱动→调轨距,否则调无轨臂长。
	void StepCameraDistance(float Fraction);
	void ShowAxisDisabledHint(EGSGravityAxis Axis);
	AActor* FindBestInteractable() const;

	UFUNCTION()
	void HandleGravityChanged(EGSGravityPolarity NewPolarity, FVector GravityDirection, int32 Revision, EGSGravityChangeReason Reason);
};
