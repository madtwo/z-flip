// GravityShift v5 - impact energy driven breakable state (restorable, never destroyed on reset).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSBreakableComponent.generated.h"

class UGSBreakProfile;
class UPrimitiveComponent;
class UStaticMesh;

UCLASS(ClassGroup = (GravityShift), meta = (BlueprintSpawnableComponent, DisplayName = "GS Breakable"))
class GRAVITYSHIFT_API UGSBreakableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSBreakableComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<UPrimitiveComponent> TargetPrimitive = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<UGSBreakProfile> BreakProfile = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bBreakable = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift")
	bool bBroken = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bOneHitBreakAboveThreshold = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bHideOwnerWhenBroken = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bDisableCollisionWhenBroken = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bDisablePhysicsWhenBroken = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FName RequiredSourceTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float MinimumImpactEnergyJ = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.01"))
	float MaximumHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float CurrentHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float DamageScalePerJ = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<UStaticMesh> BrokenMesh = nullptr;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetTargetPrimitive(UPrimitiveComponent* NewTarget);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ApplyBreakProfile(UGSBreakProfile* NewProfile);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	float ApplyImpactEnergy(float EnergyJ, AActor* Instigator);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	bool BreakNow(AActor* Instigator, float EnergyJ);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	bool Repair();

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void CaptureInitialState();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	bool IsBroken() const;

	virtual void BeginPlay() override;

protected:
	bool bHasCapturedState = false;
	float InitialHealth = 100.0f;

	bool SourceTagMatches(AActor* Instigator) const;
};
