// GravityShift v5 - shared interaction interface.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GSInteractable.generated.h"

UINTERFACE(BlueprintType, MinimalAPI)
class UGSInteractable : public UInterface
{
	GENERATED_BODY()
};

class IGSInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "GravityShift|Interaction")
	bool CanInteract(APawn* InstigatorPawn);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "GravityShift|Interaction")
	bool Interact(APawn* InstigatorPawn);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "GravityShift|Interaction")
	FText GetInteractionText(APawn* InstigatorPawn);
};
