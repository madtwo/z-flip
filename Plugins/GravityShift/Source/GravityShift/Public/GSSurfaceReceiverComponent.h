// GravityShift v5 - receives surface modifiers from volumes (deterministic priority, no stacking).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GravityShiftTypes.h"
#include "GSSurfaceReceiverComponent.generated.h"

class UGSSurfaceProfile;

UCLASS(ClassGroup = (GravityShift), meta = (BlueprintSpawnableComponent, DisplayName = "GS Surface Receiver"))
class GRAVITYSHIFT_API UGSSurfaceReceiverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSSurfaceReceiverComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FGSSurfaceModifierSpec DefaultModifier;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ApplySurfaceModifier(AActor* Source, FGSSurfaceModifierSpec Spec);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ApplySurfaceProfile(AActor* Source, UGSSurfaceProfile* Profile);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void RemoveSurfaceModifier(AActor* Source);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ClearSurfaceModifiers();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FGSSurfaceModifierSpec GetEffectiveSurfaceModifier() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	int32 GetActiveModifierCount() const;

protected:
	UPROPERTY()
	TMap<TObjectPtr<AActor>, FGSSurfaceModifierSpec> ActiveModifiers;
};
