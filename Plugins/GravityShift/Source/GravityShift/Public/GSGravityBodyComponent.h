// GravityShift v5 - applies custom Z gravity, drag and speed clamping to one primitive.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GravityShiftTypes.h"
#include "GSGravityBodyComponent.generated.h"

class AGSGravityManager;
class UGSSurfaceReceiverComponent;
class UPrimitiveComponent;

UCLASS(ClassGroup = (GravityShift), meta = (BlueprintSpawnableComponent, DisplayName = "GS Gravity Body"))
class GRAVITYSHIFT_API UGSGravityBodyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSGravityBodyComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<UPrimitiveComponent> TargetPrimitive = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<AGSGravityManager> GravityManager = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<UGSSurfaceReceiverComponent> SurfaceReceiver = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bGravityEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bAutoResolveTarget = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bAutoFindManager = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bCanBreakTargets = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bUseContinuousCollisionDetection = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float GravityScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float GravityAxisDragHz = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float TangentDragHz = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float MaximumSpeedCm = 4000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float BaseImpactEnergyMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float RepeatedImpactCooldownSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FName ImpactSourceTag = NAME_None;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetTargetPrimitive(UPrimitiveComponent* NewTarget);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetGravityEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void RefreshReferences();

	// 转向器过渡用:非零时取代管理器方向(单位向量),让重力方向可以连续旋转。
	// 由转向器经 Pawn 逐帧写入,过渡结束清零恢复。相机/驱动平面读 Pawn 的同一份值。
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift")
	FVector GravityDirectionOverride = FVector::ZeroVector;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetGravityDirectionOverride(FVector NewDirection);

	// 新机制(2026-09-12):物体重力只剩 ±Z,由玩家瞄准点击切换。启用后本物体
	// 不再跟随管理器提交的全局方向——玩家重力完全归转向器/关卡配置管,
	// 物体重力只由准星机制改变,两者互不干扰。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bUseOwnGravityDirection = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FVector OwnGravityDirection = FVector(0.0, 0.0, -1.0);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetOwnGravityDirection(FVector NewDirection, bool bEnable = true);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FVector GetGravityDirection() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FVector GetLinearVelocity() const;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetLinearVelocity(FVector NewVelocity, bool bAddToCurrent);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FVector GetCachedPrePhysicsVelocity() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FGSImpactReport GetLastImpactReport() const;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	FGSImpactReport EvaluateImpact(AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector OtherVelocity, FVector HitNormal, FVector ImpactPoint);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	FVector CachedPrePhysicsVelocity = FVector::ZeroVector;
	FGSImpactReport LastImpactReport;

	// Velocity-driven bodies under the custom gravity model do not reliably receive
	// OnComponentHit notifies from the solver, so impacts are self-detected instead:
	// while the body is moving fast we track its peak approach speed and direction;
	// when it then settles to ~rest that frame is an impact against whatever it landed on.
	float TickImpactApproachSpeedCm = 0.0f;
	FVector TickImpactApproachDirection = FVector::ZeroVector;

	UPROPERTY()
	TMap<TObjectPtr<AActor>, double> LastImpactTimeByActor;

	UFUNCTION()
	void HandleTargetHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void DetectTickImpact(const FVector& CurrentVelocityCm);
	void ResolveTickImpact(float ApproachSpeedCm);

	UFUNCTION()
	void HandleGravityChanged(EGSGravityPolarity NewPolarity, FVector GravityDirection, int32 Revision, EGSGravityChangeReason Reason);

	bool IsSimulatingTarget() const;
};
