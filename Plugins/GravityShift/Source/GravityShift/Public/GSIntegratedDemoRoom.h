// GravityShift v5 - six-sided test room. Test asset, not the preferred production architecture.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSIntegratedDemoRoom.generated.h"

class UStaticMeshComponent;

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "GS Integrated Demo Room"))
class GRAVITYSHIFT_API AGSIntegratedDemoRoom : public AActor
{
	GENERATED_BODY()

public:
	AGSIntegratedDemoRoom();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UStaticMeshComponent> FloorPanel = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UStaticMeshComponent> CeilingPanel = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UStaticMeshComponent> WallPosX = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UStaticMeshComponent> WallNegX = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UStaticMeshComponent> WallPosY = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UStaticMeshComponent> WallNegY = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FVector RoomExtent = FVector(1600.0, 1600.0, 500.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	float FloorLocalZ = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	float CeilingLocalZ = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "1.0"))
	float PanelThickness = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bSpawnOnBeginPlay = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift")
	bool bDemoSpawned = false;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void RefreshRoomGeometry();

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SpawnDemoActors();

	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
};
