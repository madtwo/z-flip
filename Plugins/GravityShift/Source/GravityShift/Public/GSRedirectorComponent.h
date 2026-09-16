// GravityShift v7 - redirector chute: touching it slides the ball onto the connected wall
// and rotates gravity to that wall (replaces the direct G-key flip for level design).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GravityShiftTypes.h"
#include "GSRedirectorComponent.generated.h"

class AGSRollingBallPawn;

UCLASS(ClassGroup = (GravityShift), meta = (BlueprintSpawnableComponent, DisplayName = "GS Redirector"))
class GRAVITYSHIFT_API UGSRedirectorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSRedirectorComponent();

	// 转向器连接的两个面(必须互相垂直,构成 90° 弯道)各自的重力方向。**双向生效**:
	// 球从任一面进入都会滑到另一面——入口面由"球当前重力"自动识别,出口就是另一面;
	// 入口行进方向恰好等于"出口重力方向",所以进入判定用一个规则就覆盖两个方向。
	// 例:地面↔y=2400 墙 → A=地面(-Z)、B=墙(-Y);从地面滚来滑上墙,从墙上滚下来滑回地面。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector")
	EGSGravityDirection GravityDirectionA = EGSGravityDirection::NEGATIVE_Z;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector")
	EGSGravityDirection GravityDirectionB = EGSGravityDirection::POSITIVE_Y;

	// 滑行速度(cm/s):球在滑梯里的速度。入口比它快就沿用入口速度(封顶 MaxRideSpeedCm)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector", meta = (ClampMin = "50.0"))
	float RideSpeedCm = 800.0f;

	// 滑过多少厘米(cm)完成重力旋转 ≈ 弯道弧长(碰到圆弧才触发,所以这里就是弧长)。
	// 球滚得快就转得快,不会出现"球还在坡上重力已转完"。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector", meta = (ClampMin = "10.0"))
	float RidePathLengthCm = 260.0f;

	// 触发盒在自身网格包围盒基础上的外扩(cm):球还没真正贴上滑梯就开始过渡,
	// 而不是撞上去才起步。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector", meta = (ClampMin = "0.0"))
	float TriggerInflateCm = 120.0f;

	// 最小触发速度(cm/s):球几乎停着不触发(默认 20,防停在坡口的球被反复"进入")。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector", meta = (ClampMin = "0.0"))
	float MinTriggerSpeedCm = 20.0f;

	// "正面接地那块"的判定带(cm):球心到**入口侧面**(地面/墙/天花板那一侧的面)的距离
	// ≤ 球半径 + 该值,才算碰在"圆弧接地的那一块"上。碰在滑梯其他位置(顶面/背面)不触发。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector", meta = (ClampMin = "0.0"))
	float EntryLipBandCm = 60.0f;

	// 撞侧面不触发:接触面法线与"滑梯宽度方向(弯道轴)"的夹角余弦超过该值 → 判定为
	// 撞在滑梯的侧壁(平直面)而不是正面圆弧,不转重力。0.7 ≈ 法线偏出 45° 就算侧面。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxLateralNormalDot = 0.7f;

	// "碰到圆弧"判定容差(cm):球面到滑梯表面的最近距离 ≤ 该值才算碰到。默认 20 ≈ 一帧
	// 的行进量,表现为"球面贴到坡面那一刻"开始转重力(用户反馈:提前转很诡异)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector", meta = (ClampMin = "-100.0"))
	float ContactTouchMarginCm = 20.0f;

	// 进入判定:球速方向与"出口重力方向"的余弦下限。90° 弯道下入口行进方向恒等于
	// 出口重力方向,所以这条规则天然双面通用(从 A 面进就要求朝 B 面走,反之亦然),
	// 同时排除"侧面撞进来 / 在滑梯里弹跳"的误触发。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float EntryAlignmentMin = 0.35f;

	// 球速低于该值(cm/s)时豁免方向判定:反向进入时球常被滑梯几何顶停在坡面上,
	// 此时速度方向已无意义,仍应按"碰到圆弧"触发。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector", meta = (ClampMin = "0.0"))
	float SlowEntrySpeedCm = 100.0f;

	// 入口面识别容差:球当前重力与入口面重力的余弦下限(球必须在其中一个面上,
	// 否则不触发)。正常进入时 ≈1。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EntryFaceGravityMin = 0.5f;

	// 进入瞬间的保底前向速度(cm/s):球滚得太慢时补到该速度,保证能滑过弯道。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector", meta = (ClampMin = "0.0"))
	float MinEntrySpeedCm = 500.0f;

	// 滑行速度上限(cm/s):入口很快的球也压到这个速度,避免高速飞出滑梯出口。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector", meta = (ClampMin = "50.0"))
	float MaxRideSpeedCm = 1200.0f;

	// 进入时清掉侧向速度(垂直弯道平面方向的)比例:1=全清。防球斜着插进滑梯蹭墙。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LateralKillFraction = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector")
	bool bEnabled = true;

	// 冷却(秒):滑出后短时间内不再重复触发(短冷却即可,反向回来要能立刻再触发)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector", meta = (ClampMin = "0.0"))
	float CooldownSeconds = 0.3f;

	// **"真的骑在面上"加固(2026-09-13 用户反馈 LDI_Gravityshift2 没上圆弧也触发)**:
	// ①支撑门:触发瞬间球必须在支撑态、且悬空时长 ≤ MaxAirborneSecondsForTrigger。
	//   滑地/滑墙进入的球是"骑在面上"的;从墙沿掉下来、空中飞过时擦到滑梯的球不算。
	// ②分离门:球相对接触面的速度不能朝"离开滑梯"方向超过 SeparationRejectSpeedCm
	//   (刚被滑梯边缘弹开的球,速度沿接触法线朝外)。两个门都留开关便于 A/B 对照。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector")
	bool bRequireSupportToTrigger = true;

	// 单面进入开关(2026-09-13):可禁止从某一面进入(默认两面都开=原双向行为)。
	// 例:只让球从地面滚入、从墙上滑下时不触发 → 关掉 A 那面。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector")
	bool bAllowEntryFromA = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector")
	bool bAllowEntryFromB = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector", meta = (ClampMin = "0.0"))
	float MaxAirborneSecondsForTrigger = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector")
	bool bRejectSeparatingContact = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector", meta = (ClampMin = "0.0"))
	float SeparationRejectSpeedCm = 150.0f;

	// ---- 特殊滑梯:面吸附模式(2026-09-15 用户需求,给关卡里那条独一无二的滑梯) ----
	// 勾上后本转向器不再走"90° 弯道滑行",而是:球**碰到竖直面(垂直于地面的那一面)**
	// → 吸附 → 沿曲面被自然带到平面(平行地面的那一面)→ 重力转成该面的重力(通常向下)。
	// 与弯道滑行的区别:弯道滑行要求球已经骑在某一面上沿弧滚出;面吸附专门处理
	// "从侧面撞上去"这件事——那一下在旧算法里恰恰被 gate[side] 拒掉。
	// 只给特殊滑梯用:普通滑梯保持默认(不勾)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector")
	bool bFaceCaptureMode = false;

	// 允许从哪一面"吸附进入"。2026-09-16 起 A 面(平面/高台)也是**正经可用**的一侧:
	// 球在高台上朝圆弧滚过去 → 温和吸附 → 重力贴到墙上(骑竖直面)。两侧都开即双向。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector")
	bool bCaptureEntryFromA = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector")
	bool bCaptureEntryFromB = true;

	// 吸附后沿面驱动速度(cm/s)。只作用于**硬吸附**(入口面 = B);
	// A 面进入走温和吸附,速度由 FaceCaptureGroundMin/MaxSpeedCm 决定。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector", meta = (ClampMin = "50.0"))
	float FaceCaptureSpeedCm = 700.0f;

	// 压向面的加速度(cm/s²):凸圆弧上球有被甩开的趋势,靠它摁住。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector", meta = (ClampMin = "0.0"))
	float FaceCaptureStickAccelCm = 2500.0f;

	// 重力转完所需路程(cm) ≈ 圆弧弧长(默认 180 ≈ R100 的四分之一弧)。走到平面正好转到位。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector", meta = (ClampMin = "10.0"))
	float FaceCaptureRotateOverCm = 180.0f;

	// 释放判定:接触法线与**出口面**法线的余弦 ≥ 该值(0.85 ≈ 32°)即认为已到平面。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector", meta = (ClampMin = "0.1", ClampMax = "0.999"))
	float FaceCaptureExitNormalDot = 0.85f;

	// 进入判定:接触法线与**入口面**法线的余弦下限(确认球确实贴在这一面上)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FaceCaptureEntryNormalMin = 0.6f;

	// 进入判定:球速沿"沿面前进方向"的分量下限(cm/s)。默认 0 = 碰到就吸附
	// (用户原话"碰到了就会被吸附上去");调到数百可要求"真的在往坡上滚"才吸附。
	// 只作用于 **B 面(竖直面)进入** 那一侧;A 面(高台)进入用下面的 Ground 版本。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector")
	float FaceCaptureMinApproachSpeedCm = 0.0f;

	// ---- 双向:从 A 面(平面/高台)滚进圆弧那一侧(2026-09-16 队友需求) ----
	// 上一轮只做了"球碰到竖直面 → 被吸上平面";这一轮要反过来也成立:球已经到了高台上,
	// 再对着圆弧滚过去,重力要能贴到墙上(变成骑竖直面)。用户的话是"碰到圆弧的一面后,
	// 只要加速度符合方向要求就能触发转重力",并且"看人下菜":
	//   · 弹起来擦到墙 / 已经骑在墙上 → 仍走原来的**硬吸附**(入口面 = B,定速沿面驱动)
	//   · 已经在地面(高台)上自己滚过去 → 走新的**温和吸附**(入口面 = A,保留球自己的速度)
	// 所以这里不再按"哪个面"写死开关:两面都可进,吸附方式由入口面决定。
	// (关卡侧:把 bCaptureEntryFromA 打开即启用这个新方向。)

	// A 面(高台)进入的方向门(cm/s):球沿"朝着圆弧"的切向速度下限。这一条是用户要求的
	// "方向符合要求才触发"的落点——球必须真的在往圆弧那边滚,而不是停在拐角上乱撞。
	// 也是防止刚被温和吸附送到墙上的球在盒内被反方向再吸回去的第二道保险。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector")
	float FaceCaptureGroundMinApproachSpeedCm = 150.0f;

	// 温和吸附的切向速度下限/上限(cm/s):球滚得慢由下限兜底(保证走完圆弧),滚得快由
	// 上限封顶(不和重力转出来的额外动能叠加成"飞出去")。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector", meta = (ClampMin = "20.0"))
	float FaceCaptureGroundMinSpeedCm = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector", meta = (ClampMin = "50.0"))
	float FaceCaptureGroundMaxSpeedCm = 900.0f;

	// 面吸附模式下是否**同时**保留旧的 90° 弯道逻辑(默认关)。
	// 必须默认关的原因(2026-09-15 实测):吸附把球送到平面的那一帧,球正好贴着圆弧、
	// 重力已转成 -Z,旧逻辑立刻判定"从平面进入"并又来一次 -Z→-Y 滑行,把刚送上平面的球
	// 原路甩回墙上——用户要的"被带到平面上"当场被抵消。关掉后本滑梯只有面吸附这一套行为。
	// (若哪天想让球从平面滚下来时也能贴到墙上,再把这个开关打开。)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Redirector")
	bool bFaceCaptureAlsoClassic = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Debug")
	bool bDebugLog = false;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	// 由拥有者的渲染/碰撞包围盒外扩得到(静态摆件,开局算一次)。
	FBox TriggerBounds = FBox(ForceInit);

	// 网格本身的包围盒(不外扩)。
	FBox MeshBounds = FBox(ForceInit);

	// 球面到滑梯表面的最近距离是否已进入容差(三个方向线探针取最小命中距离:
	// 朝包围盒最近点 / 入口"下" / 出口"下");OutNormal 回传该接触面的法线。
	bool IsBallTouchingChute(const class USphereComponent& BallSphere, const FVector& EntryUp, const FVector& ExitUp, FVector& OutNormal) const;

	// 特殊滑梯:面吸附触发判定(bFaceCaptureMode 生效时走这里,不走上面的 90° 弯道逻辑)。
	// 球贴在本滑梯的**入口面**上(A=平面/高台 或 B=竖直面,由球当前重力识别)即命中;
	// 命中则调 Pawn 的 BeginFaceCapture,并把重力交给它逐帧旋转到另一面。吸附方式(硬/温和)
	// 由入口面决定,见头文件上方 bCaptureEntryFromA 的说明。
	bool TryBeginFaceCapture(class AGSRollingBallPawn& Ball, const class USphereComponent& BallSphere,
		const FVector& BallLoc, const FVector& Velocity, float Speed);

	// 本组件是否正在驱动一次滑行(滑行期间不做重触发;球离开触发盒才释放)。
	bool bRiding = false;

	double LastFireTime = -1.0;
};
