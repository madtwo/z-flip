// GravityShift v5 - the single authority for gravity state (Z-only).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GravityShiftTypes.h"
#include "GSGravityManager.generated.h"

class UGSGravityBodyComponent;
class UGSGravityProfile;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FGSOnGravityChanged,
	EGSGravityPolarity, NewPolarity,
	FVector, GravityDirection,
	int32, Revision,
	EGSGravityChangeReason, Reason);

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "GS Gravity Manager"))
class GRAVITYSHIFT_API AGSGravityManager : public AActor
{
	GENERATED_BODY()

public:
	AGSGravityManager();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	EGSGravityPolarity DefaultPolarity = EGSGravityPolarity::NEGATIVE_Z;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift")
	EGSGravityPolarity CurrentPolarity = EGSGravityPolarity::NEGATIVE_Z;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float GravityAccelerationCm = 1600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float ManualCooldownSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float AutomaticCooldownSeconds = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bGravityLocked = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift")
	int32 GravityRevision = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift")
	EGSGravityChangeReason LastChangeReason = EGSGravityChangeReason::RESET;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<UGSGravityProfile> GravityProfile = nullptr;

	UPROPERTY(BlueprintAssignable, Category = "GravityShift")
	FGSOnGravityChanged OnGravityChanged;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	EGSGravityRequestResult RequestToggleGravity(AActor* Requester, EGSGravityChangeReason Reason, bool bForce);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	EGSGravityRequestResult RequestGravityPolarity(EGSGravityPolarity NewPolarity, AActor* Requester, EGSGravityChangeReason Reason, bool bForce);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	EGSGravityRequestResult ResetGravity(bool bForce);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetGravityLocked(bool bLocked);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ApplyGravityProfile(UGSGravityProfile* NewProfile);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FVector GetGravityDirection() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FVector GetGravityAccelerationVector() const;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	float GetCooldownRemaining(EGSGravityChangeReason Reason) const;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void RegisterGravityBody(UGSGravityBodyComponent* GravityBody);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void UnregisterGravityBody(UGSGravityBodyComponent* GravityBody);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	int32 GetRegisteredBodyCount() const;

	UFUNCTION(BlueprintCallable, Category = "GravityShift", meta = (WorldContext = "WorldContextObject"))
	static AGSGravityManager* FindGravityManager(UObject* WorldContextObject);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY()
	TArray<TObjectPtr<UGSGravityBodyComponent>> RegisteredBodies;

	TMap<uint8, double> LastChangeTimeByReason;

	bool IsAutomaticReason(EGSGravityChangeReason Reason) const;
	void CommitPolarity(EGSGravityPolarity NewPolarity, EGSGravityChangeReason Reason);
};
