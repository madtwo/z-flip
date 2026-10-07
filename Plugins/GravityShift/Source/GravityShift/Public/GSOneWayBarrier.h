// GravityShift - 单向空气墙(2026-09-17 用户需求:"给这个加个单向空气墙防止滚下去")

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSOneWayBarrier.generated.h"

class UBoxComponent;

/**
 * 单向空气墙:球**往外**走被挡住,往内走(或横穿)完全不受影响。
 *
 * 为什么不用真碰撞:UE 的物理碰撞没有"只挡一个方向"这回事,靠每帧开关碰撞会抖。
 * 这里改成**速度钳制** —— 球在盒内、且速度在外向方向上有正分量时,把这个分量清零,
 * 于是"想出去"的球被钉在墙内侧,而"想进来"的球(外向分量为负)一点都不受影响。
 *
 * 用法:拖到平台边缘,把 OutwardDirection 指向"掉下去的那一侧",盒子盖住那一条边即可
 * (盒子本身无碰撞,只在编辑器里显示线框)。
 */
UCLASS(Blueprintable, BlueprintType, DisplayName = "GS One Way Barrier", Category = "GravityShift")
class GRAVITYSHIFT_API AGSOneWayBarrier : public AActor
{
	GENERATED_BODY()

public:
	AGSOneWayBarrier();

	// 无碰撞的线框盒:只用来划定"空气墙"的范围,真正的阻挡靠速度钳制。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Barrier")
	TObjectPtr<UBoxComponent> Barrier;

	// 外向方向(世界坐标):球往这边走 = 想掉下去 → 被挡住。默认 -Y。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Barrier")
	FVector OutwardDirection = FVector(0.0, -1.0, 0.0);

	// 生效厚度(cm):盒子在三个方向上都再外扩这么多,给高速球留一点提前量。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Barrier", meta = (ClampMin = "0.0"))
	float BarrierThicknessCm = 40.0f;

	// 被挡时额外朝内的回推速度(cm/s):0 = 只清零,不推。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Barrier", meta = (ClampMin = "0.0"))
	float PushBackCm = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Barrier")
	bool bDebugLog = false;

	// 位置拽回的最大位移(cm):停止线外超过这个距离的球不拽(从很外面掉进来的不硬拉)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Barrier", meta = (ClampMin = "0.0"))
	float MaxSnapCm = 80.0f;

	// 拦截日志的节流(每 0.2s 一条),避免顶着墙推时刷屏。
	double LastBlockLogTime = -1.0;

	virtual void Tick(float DeltaSeconds) override;
};
