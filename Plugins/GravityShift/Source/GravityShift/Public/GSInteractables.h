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
class USceneComponent;
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

// ---------------------------------------------------------------------------------
// Pickup item - F-interact pickup. Shows a center-screen message and pauses input
// (ShowMessageAndLock) until the player presses Space. Stays alive but hidden when
// collected so the world reset can re-arm it (mirrors AGSCollectible; no Destroy).
// ---------------------------------------------------------------------------------

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "GS Pickup Item"))
class GRAVITYSHIFT_API AGSPickupItem : public AActor, public IGSInteractable
{
	GENERATED_BODY()

public:
	AGSPickupItem();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UStaticMeshComponent> Mesh = nullptr;

	// Center-screen text shown on pickup. Empty = collect silently, no input pause.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FText PickupMessage;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift")
	bool bIsCollected = false;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	bool Collect(APawn* Collector);

	// World-reset hook: re-arms the pickup so it can be collected again.
	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void RestoreInitialState();

	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) override;
	virtual bool Interact_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractionText_Implementation(APawn* InstigatorPawn) override;

	virtual void BeginPlay() override;

protected:
	bool bInitialCollected = false;
};

// ---------------------------------------------------------------------------------
// Key - a pickup that unlocks every door whose RequiredKeyID matches this KeyID.
// ---------------------------------------------------------------------------------

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "GS Key"))
class GRAVITYSHIFT_API AGSKey : public AGSPickupItem
{
	GENERATED_BODY()

public:
	AGSKey();

	// Doors with RequiredKeyID == this KeyID open the moment the key is picked up.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Key")
	FName KeyID = NAME_None;

	virtual bool Interact_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractionText_Implementation(APawn* InstigatorPawn) override;
};

// ---------------------------------------------------------------------------------
// Door - blocks the way while locked; a matching key (AGSKey) opens it by sliding
// DoorMesh by SlideOffset. Interactable only while locked, to show a hint. A world
// reset re-locks it (RestoreInitialState) so every keyed gate rewinds on death.
// ---------------------------------------------------------------------------------

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "GS Key Door"))
class GRAVITYSHIFT_API AGSDoor : public AActor, public IGSInteractable
{
	GENERATED_BODY()

public:
	AGSDoor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<USceneComponent> DoorRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UStaticMeshComponent> DoorMesh = nullptr;

	// Doors whose RequiredKeyID matches the picked key's KeyID open.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Door")
	FName RequiredKeyID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Door")
	bool bIsLocked = true;

	// Mesh slides from its local origin to this offset while opening (direction+distance).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Door")
	FVector SlideOffset = FVector(0.0f, 0.0f, 240.0f);

	// Seconds for the slide to complete in either direction.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Door", meta = (ClampMin = "0.05"))
	float SlideDuration = 0.8f;

	// Center-screen hint shown when the player presses F on a still-locked door.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Door")
	FText LockedMessage;

	// Opens the door if this is the matching key. Returns true when the door ends up open.
	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	bool TryUnlock(FName KeyID);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetLocked(bool bNowLocked);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void RestoreInitialState();

	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) override;
	virtual bool Interact_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractionText_Implementation(APawn* InstigatorPawn) override;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	bool bInitialLocked = true;

	// 0 = closed (mesh at local origin), 1 = open (mesh at SlideOffset).
	float SlideAlpha = 0.0f;
};
