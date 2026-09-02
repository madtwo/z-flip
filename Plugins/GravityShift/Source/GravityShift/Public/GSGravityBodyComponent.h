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

	UPROPERTY()
	TMap<TObjectPtr<AActor>, double> LastImpactTimeByActor;

	UFUNCTION()
	void HandleTargetHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void HandleGravityChanged(EGSGravityPolarity NewPolarity, FVector GravityDirection, int32 Revision, EGSGravityChangeReason Reason);

	bool IsSimulatingTarget() const;
};
