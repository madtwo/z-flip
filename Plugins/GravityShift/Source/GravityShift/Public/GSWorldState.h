// GravityShift v5 - world state, checkpoints, kill volumes, collectibles and the goal.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSWorldState.generated.h"

class AGSCollectible;
class AGSGravityManager;
class APawn;
class UBoxComponent;
class UGSCollectibleProfile;
class UStaticMeshComponent;

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "GS World State Manager"))
class GRAVITYSHIFT_API AGSWorldStateManager : public AActor
{
	GENERATED_BODY()

public:
	AGSWorldStateManager();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FTransform ActiveCheckpointTransform = FTransform::Identity;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool HasActiveCheckpoint = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bUseCapturedCheckpointState = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift")
	float TotalCollectibleValue = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift")
	float CollectedValue = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift")
	float TotalRequiredCollectibleValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	float RequiredValueOverride = -1.0f;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void RegisterPlayer(APawn* NewPlayer);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	APawn* GetRegisteredPlayer() const;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetCheckpoint(FTransform NewCheckpointTransform);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void CaptureWorldCheckpointState();

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ResetWorld();

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void RegisterCollectible(AGSCollectible* Collectible);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void NotifyCollectibleStateChanged(AGSCollectible* Collectible, bool bCollected);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void RecalculateCollectibleCounts();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	bool CanCompleteGoal() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	float GetRequiredGoalValue() const;

	UFUNCTION(BlueprintCallable, Category = "GravityShift", meta = (WorldContext = "WorldContextObject"))
	static AGSWorldStateManager* FindWorldStateManager(UObject* WorldContextObject);

protected:
	UPROPERTY()
	TObjectPtr<APawn> RegisteredPlayer = nullptr;

	UPROPERTY()
	TArray<TObjectPtr<AGSCollectible>> RegisteredCollectibles;
};

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "GS Checkpoint"))
class GRAVITYSHIFT_API AGSCheckpoint : public AActor
{
	GENERATED_BODY()

public:
	AGSCheckpoint();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UBoxComponent> Trigger = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<AGSWorldStateManager> WorldStateManager = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FTransform RespawnOffset = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bCaptureWorldState = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bOneShot = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift")
	bool bActivated = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FName BindingSourceWhiteboxId = NAME_None;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ActivateCheckpoint(AActor* Activator);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FTransform GetRespawnTransform() const;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ResetCheckpoint();

	virtual void BeginPlay() override;

protected:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "GS Kill Volume"))
class GRAVITYSHIFT_API AGSKillVolume : public AActor
{
	GENERATED_BODY()

public:
	AGSKillVolume();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UBoxComponent> Volume = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<AGSWorldStateManager> WorldStateManager = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FVector VolumeExtent = FVector(5000.0, 5000.0, 300.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float TriggerCooldownSeconds = 0.50f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FName BindingSourceWhiteboxId = NAME_None;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void TriggerReset();

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetVolumeExtent(FVector NewExtent);

	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	double LastTriggerTime = -1000.0;

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "GS Collectible"))
class GRAVITYSHIFT_API AGSCollectible : public AActor
{
	GENERATED_BODY()

public:
	AGSCollectible();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UStaticMeshComponent> Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<UGSCollectibleProfile> CollectibleProfile = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<AGSWorldStateManager> WorldStateManager = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	float Value = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bPersistThroughReset = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bRequiredForGoal = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift")
	bool bCollected = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FName BindingSourceWhiteboxId = NAME_None;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ApplyCollectibleProfile(UGSCollectibleProfile* NewProfile);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	bool Collect(APawn* Collector);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetCollectedState(bool bNewCollected);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void CaptureCheckpointState();

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void RestoreCheckpointState();

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void RestoreInitialState();

	virtual void BeginPlay() override;

protected:
	bool bCheckpointCollected = false;
	bool bInitialCollected = false;

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "GS Finish Goal"))
class GRAVITYSHIFT_API AGSFinishGoal : public AActor
{
	GENERATED_BODY()

public:
	AGSFinishGoal();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UBoxComponent> Trigger = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<AGSWorldStateManager> WorldStateManager = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bRequireCollectibles = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift")
	bool bCompleted = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	float RequiredValueOverride = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FName BindingSourceWhiteboxId = NAME_None;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	bool TryComplete();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	bool CanComplete() const;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ResetGoal();

	virtual void BeginPlay() override;

protected:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
