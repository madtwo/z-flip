// GravityShift v7 - gravity detectors gate which blocks a gravity field acts on.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSGravityZones.generated.h"

class AGSBlockBase;
class AGSRollingBallPawn;
class UBoxComponent;
class UStaticMeshComponent;

// 全场唯一的重力区域协调者:同一时刻最多一个区域受重力,其余一律禁用。
// 关卡里放一个即可(不会自动 spawn);没有它检测器只闪灯、不切区域。
UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "GS Gravity Zone Manager"))
class GRAVITYSHIFT_API AGSGravityZoneManager : public AActor
{
	GENERATED_BODY()

public:
	AGSGravityZoneManager();

	// 开局是否全图禁用方块重力(检测器没触发前谁都不掉)。false = 沿用方块各自的
	// bAffectedByGravity 初始值,检测器只做"切换"而不做"起步禁用"。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gravity Zone")
	bool bStartWithGravityDisabled = true;

	// 先全图禁用,再单独启用 Blocks 里的方块。传空数组 = 全场无重力。
	// 参数类型是裸指针数组而不是 TObjectPtr 数组:UHT 不收 TObjectPtr 作 UFUNCTION
	// 参数,而两个 Zone 数组要能直接喂进来。
	UFUNCTION(BlueprintCallable, Category = "Gravity Zone")
	void SetActiveZone(const TArray<AGSBlockBase*>& Blocks);

	// 全图禁用 + 定住:遍历所有 AGSBlockBase 调 SetAffectedByGravity(false) 并清零速度
	// (FreezeMotion)。方块会停在原地,不是继续滑行。
	UFUNCTION(BlueprintCallable, Category = "Gravity Zone")
	void DisableAllGravity();

	// 回到初始状态(全禁用)。
	UFUNCTION(BlueprintCallable, Category = "Gravity Zone")
	void ResetAllZones();

	// 供 AGSWorldStateManager::ResetWorld 之类的重置流程调用(目前未接线,留给关卡
	// 蓝图或后续集成)。等价于 ResetAllZones。
	UFUNCTION(BlueprintCallable, Category = "Gravity Zone")
	void OnResetWorld();

	UFUNCTION(BlueprintCallable, Category = "Gravity Zone", meta = (WorldContext = "WorldContextObject"))
	static AGSGravityZoneManager* FindZoneManager(UObject* WorldContextObject);

	virtual void BeginPlay() override;

private:
	// 禁用一个方块的加重力并定住它。所有"禁用"路径都走这里,别在多处各写一遍。
	static void DisableBlockGravity(AGSBlockBase* Block);
};

// 门口/分界线上的一层薄触发面:小球穿过时把重力交给某一侧的方块。
// 朝向约定:前向箭头指着 ZoneB 那一侧,背后是 ZoneA。球朝前向穿过 → 激活 ZoneB;
// 反向穿过 → 激活 ZoneA。
UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "GS Gravity Detector"))
class GRAVITYSHIFT_API AGSGravityDetector : public AActor
{
	GENERATED_BODY()

public:
	AGSGravityDetector();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UBoxComponent> TriggerPlane = nullptr;

	// 闪光反馈用。编辑器里可见(方便挂发光材质),游戏里默认隐身,只在触发时亮一下。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UStaticMeshComponent> FlashMesh = nullptr;

	// 前向箭头指向那一侧的方块。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gravity Detector")
	TArray<AGSBlockBase*> ZoneB_Blocks;

	// 背后那一侧的方块。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gravity Detector")
	TArray<AGSBlockBase*> ZoneA_Blocks;

	// 触发盒半尺寸(SetBoxExtent 语义,不是全长)。默认 X=5 → 10cm 厚。
	// 球开了 CCD,薄面不会被高速穿越跳过;真要加厚直接改这里。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gravity Detector")
	FVector TriggerExtent = FVector(5.0, 200.0, 200.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gravity Detector", meta = (ClampMin = "0.0"))
	float FlashDuration = 0.3f;

	// 低于此速度不触发:球停在触发面上来回蹭时不该反复切区域。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gravity Detector", meta = (ClampMin = "0.0"))
	float MinTriggerSpeed = 50.0f;

	// 速度方向与前向夹角的最小余弦值。球侧向擦过触发盒时方向读不出来,低于此值
	// 不切区域(否则 Dot 的正负会把区域随机切到一侧)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gravity Detector", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinDirectionalDot = 0.2f;

	// 闪一次(重复调用会把计时重新拉满)。
	UFUNCTION(BlueprintCallable, Category = "Gravity Detector")
	void PlayFlash();

	// 灭灯 + 取消未到期的闪光计时。区域状态不归它管,要复位区域调 ZoneManager。
	UFUNCTION(BlueprintCallable, Category = "Gravity Detector")
	void ResetDetector();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void EndFlash();

	FTimerHandle FlashTimerHandle;
};
