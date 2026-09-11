// GravityShift v5 - one block class covering fixed, gravity, breaker and breakable recipes.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSBlockBase.generated.h"

class UGSBlockProfile;
class UGSBreakableComponent;
class UGSGravityBodyComponent;
class UGSGridSnapComponent;
class UGSResettableComponent;
class UGSSurfaceReceiverComponent;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "GS Block Base"))
class GRAVITYSHIFT_API AGSBlockBase : public AActor
{
	GENERATED_BODY()

public:
	AGSBlockBase();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UStaticMeshComponent> Mesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UGSGravityBodyComponent> GravityBody = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UGSSurfaceReceiverComponent> SurfaceReceiver = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UGSBreakableComponent> BreakableComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UGSResettableComponent> Resettable = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UGSGridSnapComponent> GridSnapComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<UGSBlockProfile> BlockProfile = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bStartSimulatingPhysics = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bAffectedByGravity = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bCanBreakTargets = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bBreakable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bUseContinuousCollisionDetection = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float GravityScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float MassOverrideKg = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float MaximumSpeedCm = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float ImpactEnergyMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FName ImpactSourceTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FName BindingSourceWhiteboxId = NAME_None;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ApplyBlockProfile(UGSBlockProfile* NewProfile);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetBlockMesh(UStaticMesh* NewMesh);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetSimulatingPhysics(bool bSimulate);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetAffectedByGravity(bool bAffected);

	// 清零线速度 + 角速度。配合 SetAffectedByGravity(false) 把方块"瞬间定住",而不是
	// 保留速度继续滑行。方块仍在模拟物理——重新启用重力后立刻恢复响应。
	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void FreezeMotion();

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetCanBreakTargets(bool bCanBreak);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetBreakable(bool bIsBreakable);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ApplyCurrentConfiguration();

	virtual void BeginPlay() override;
};
