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

	// 本组件是否正在驱动一次滑行(滑行期间不做重触发;球离开触发盒才释放)。
	bool bRiding = false;

	double LastFireTime = -1.0;
};
