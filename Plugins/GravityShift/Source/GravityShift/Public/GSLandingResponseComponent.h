// GravityShift v5 - support probe, fall tracking, bounce and automatic gravity reverse.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GravityShiftTypes.h"
#include "GSLandingResponseComponent.generated.h"

class AGSGravityManager;
class UGSGravityBodyComponent;
class UGSLandingProfile;
class UPrimitiveComponent;

UCLASS(ClassGroup = (GravityShift), meta = (BlueprintSpawnableComponent, DisplayName = "GS Landing Response"))
class GRAVITYSHIFT_API UGSLandingResponseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSLandingResponseComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<UPrimitiveComponent> TargetPrimitive = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<AGSGravityManager> GravityManager = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<UGSGravityBodyComponent> GravityBody = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	EGSAutoReverseMode AutoReverseMode = EGSAutoReverseMode::LANDING_IMPACT;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bRequireSpeedAndDistanceForMidairReverse = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bPreserveTangentialVelocityOnBounce = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float NoResponseBelowImpactSpeedCm = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float LandingAutoReverseAtSpeedCm = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float BounceSpeedCm = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float AutoReverseFallSpeedCm = 1400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float AutoReverseFallDistanceCm = 2200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float MinimumAirborneSeconds = 0.20f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float LocalResponseCooldownSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float AutomaticRetrySeconds = 0.50f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float GroundProbeDistanceCm = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.01"))
	float GroundProbeRadiusScale = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumLandingNormalDot = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TEnumAsByte<ECollisionChannel> GroundProbeChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	float BounceTangentialRetention = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	float AutomaticVelocityRetention = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float AutomaticMaxCarrySpeedCm = 700.0f;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetTargetPrimitive(UPrimitiveComponent* NewTarget);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void RefreshReferences();

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ApplyLandingModifier(AActor* Source, FGSLandingModifierSpec Spec);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ApplyLandingProfile(AActor* Source, UGSLandingProfile* Profile);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void RemoveLandingModifier(AActor* Source);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ClearLandingModifiers();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FGSLandingModifierSpec GetEffectiveLandingModifier() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	int32 GetActiveLandingModifierCount() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	float GetCurrentFallSpeedCm() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	float GetCurrentFallDistanceCm() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	float GetAirborneSeconds() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	bool IsSupported() const;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	bool ForceEvaluateAutomaticReverse();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FGSLandingReport GetLastLandingReport() const;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ResetFlightState();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	bool bWasSupported = false;
	bool bReverseConsumedThisFlight = false;
	bool bBouncedSinceQuietLanding = false;
	float AirborneSeconds = 0.0f;
	float CurrentFallSpeedCm = 0.0f;
	float CurrentFallDistanceCm = 0.0f;
	float LastResponseTime = -1000.0f;
	FGSLandingReport LastLandingReport;

	UPROPERTY()
	TMap<TObjectPtr<AActor>, FGSLandingModifierSpec> ActiveModifiers;

	bool ProbeGround() const;
	void HandleLanding(float ImpactSpeedCm);
	bool RequestAutomaticReverse(EGSGravityChangeReason Reason);
	void ClampCarriedVelocity(float Retention, float MaxCarrySpeedCm);
};
