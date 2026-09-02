// GravityShift v5 - surface volumes, landing volumes, gravity switch and surface controller.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GravityShiftTypes.h"
#include "GSInteractable.h"
#include "GSInteractables.generated.h"

class AGSGravityManager;
class UBoxComponent;
class UGSSurfaceProfile;
class UGSLandingProfile;
class UStaticMeshComponent;

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "GS Surface Modifier Volume"))
class GRAVITYSHIFT_API AGSSurfaceModifierVolume : public AActor
{
	GENERATED_BODY()

public:
	AGSSurfaceModifierVolume();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UBoxComponent> Volume = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UStaticMeshComponent> VisualMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<UGSSurfaceProfile> ProfileA = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<UGSSurfaceProfile> ProfileB = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FGSSurfaceModifierSpec FallbackProfileA;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FGSSurfaceModifierSpec FallbackProfileB;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	int32 ActiveProfileIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FVector VolumeExtent = FVector(400.0, 400.0, 60.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bShowVisualMesh = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FName BindingSourceWhiteboxId = NAME_None;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetActiveProfileIndex(int32 NewIndex);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ToggleProfile();

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetVolumeExtent(FVector NewExtent);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ReapplyToOverlappingActors();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FGSSurfaceModifierSpec GetActiveSurfaceModifier() const;

	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "GS Landing Response Volume"))
class GRAVITYSHIFT_API AGSLandingResponseVolume : public AActor
{
	GENERATED_BODY()

public:
	AGSLandingResponseVolume();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UBoxComponent> Volume = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UStaticMeshComponent> VisualMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<UGSLandingProfile> ProfileA = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<UGSLandingProfile> ProfileB = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FGSLandingModifierSpec FallbackProfileA;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FGSLandingModifierSpec FallbackProfileB;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	int32 ActiveProfileIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FVector VolumeExtent = FVector(400.0, 400.0, 60.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bShowVisualMesh = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FName BindingSourceWhiteboxId = NAME_None;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetActiveProfileIndex(int32 NewIndex);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ToggleProfile();

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetVolumeExtent(FVector NewExtent);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ReapplyToOverlappingActors();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FGSLandingModifierSpec GetActiveLandingModifier() const;

	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "GS Gravity Switch"))
class GRAVITYSHIFT_API AGSGravitySwitch : public AActor, public IGSInteractable
{
	GENERATED_BODY()

public:
	AGSGravitySwitch();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UStaticMeshComponent> Mesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UBoxComponent> Trigger = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<AGSGravityManager> GravityManager = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	EGSGravitySwitchMode SwitchMode = EGSGravitySwitchMode::TOGGLE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bTriggerOnOverlap = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bForceGravityRequest = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	int32 RemainingUses = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float TriggerCooldownSeconds = 0.40f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FName BindingSourceWhiteboxId = NAME_None;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	EGSGravityRequestResult TriggerSwitch(AActor* RequestInstigator);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ResetUses(int32 NewUses);

	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) override;
	virtual bool Interact_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractionText_Implementation(APawn* InstigatorPawn) override;

	virtual void BeginPlay() override;

protected:
	double LastTriggerTime = -1000.0;

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "GS Surface Controller Device"))
class GRAVITYSHIFT_API AGSSurfaceControllerDevice : public AActor, public IGSInteractable
{
	GENERATED_BODY()

public:
	AGSSurfaceControllerDevice();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UStaticMeshComponent> Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TArray<TObjectPtr<AGSSurfaceModifierVolume>> TargetSurfaceVolumes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TArray<TObjectPtr<AGSLandingResponseVolume>> TargetLandingVolumes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	int32 RemainingUses = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FName BindingSourceWhiteboxId = NAME_None;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void TriggerTargets();

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ResetUses(int32 NewUses);

	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) override;
	virtual bool Interact_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractionText_Implementation(APawn* InstigatorPawn) override;
};
