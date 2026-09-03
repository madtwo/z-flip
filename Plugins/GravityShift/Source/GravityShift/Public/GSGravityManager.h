// GravityShift v6 - the single authority for gravity state (any of six directions).

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

// Primary six-direction change notification. Requester is the actor that asked
// for the change (null for boot-time/internal commits).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FOnGravityDirectionChanged,
	EGSGravityDirection, NewDirection,
	FVector, DirectionVector,
	int32, Revision,
	EGSGravityChangeReason, Reason,
	AActor*, Requester);

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "GS Gravity Manager"))
class GRAVITYSHIFT_API AGSGravityManager : public AActor
{
	GENERATED_BODY()

public:
	AGSGravityManager();

	// ------------------------------------------------------------------ six-direction state
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Direction")
	EGSGravityDirection DefaultDirection = EGSGravityDirection::NEGATIVE_Z;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Direction")
	EGSGravityDirection CurrentDirection = EGSGravityDirection::NEGATIVE_Z;

	// Axes the current level allows the player to flip to. Empty = all axes
	// allowed (X/Y/Z). Set by AGSWorldStateManager from level config.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "GravityShift|Direction")
	TArray<EGSGravityAxis> AllowedAxes;

	// ---- legacy Z-only mirrors (deprecated; kept for backward compatibility) --------
	// Only meaningful while CurrentDirection is on the Z axis.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Legacy")
	EGSGravityPolarity CurrentPolarity = EGSGravityPolarity::NEGATIVE_Z;

	// ------------------------------------------------------------------ classic tuning
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Legacy")
	EGSGravityPolarity DefaultPolarity = EGSGravityPolarity::NEGATIVE_Z;

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
	FOnGravityDirectionChanged OnGravityDirectionChanged;

	// Legacy broadcast (Z-only polarity); kept for existing binders.
	UPROPERTY(BlueprintAssignable, Category = "GravityShift")
	FGSOnGravityChanged OnGravityChanged;

	// ---- requests (six-direction) ------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	EGSGravityRequestResult RequestGravityDirection(EGSGravityDirection NewDirection, AActor* Requester,
		EGSGravityChangeReason Reason = EGSGravityChangeReason::SCRIPTED, bool bForce = false);

	// Requests the positive direction of the given axis (+X for X, etc.).
	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	EGSGravityRequestResult SetGravityAxis(EGSGravityAxis Axis, AActor* Requester, bool bForce = false);

	// Flips the current direction on its own axis (+X <-> -X).
	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	EGSGravityRequestResult ToggleCurrentAxis(AActor* Requester,
		EGSGravityChangeReason Reason = EGSGravityChangeReason::MANUAL, bool bForce = false);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetAllowedAxes(const TArray<EGSGravityAxis>& InAllowedAxes);

	UFUNCTION(BlueprintPure, Category = "GravityShift")
	bool IsDirectionAllowed(EGSGravityDirection Dir) const;

	// ---- legacy Z-only request entry points (deprecated) ---------------------------
	// Kept public so existing callers (gravity switch, landing response, older BP)
	// keep working. Toggle flips the current axis; the polarity methods target +Z/-Z.
	UFUNCTION(BlueprintCallable, Category = "GravityShift|Legacy")
	EGSGravityRequestResult RequestToggleGravity(AActor* Requester, EGSGravityChangeReason Reason, bool bForce);

	UFUNCTION(BlueprintCallable, Category = "GravityShift|Legacy")
	EGSGravityRequestResult RequestGravityPolarity(EGSGravityPolarity NewPolarity, AActor* Requester,
		EGSGravityChangeReason Reason, bool bForce);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	EGSGravityRequestResult ResetGravity(bool bForce);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetGravityLocked(bool bLocked);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ApplyGravityProfile(UGSGravityProfile* NewProfile);

	// ---- queries -------------------------------------------------------------------
	UFUNCTION(BlueprintPure, Category = "GravityShift")
	EGSGravityDirection GetCurrentDirection() const;

	// Normalized allowed list; empty member list is reported as all three axes.
	UFUNCTION(BlueprintPure, Category = "GravityShift")
	TArray<EGSGravityAxis> GetAllowedAxes() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FVector GetGravityDirection() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FVector GetGravityAccelerationVector() const;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	float GetCooldownRemaining(EGSGravityChangeReason Reason) const;

	// ---- body registry ---------------------------------------------------------------
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
	void CommitDirection(EGSGravityDirection NewDirection, EGSGravityChangeReason Reason, AActor* Requester);
};
