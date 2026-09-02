// GravityShift v5 - game mode and debug HUD.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GameFramework/GameModeBase.h"
#include "GSFramework.generated.h"

class AGSGravityManager;
class AGSWorldStateManager;
class UGSBallProfile;
class UGSLandingProfile;

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "GS Gravity Game Mode"))
class GRAVITYSHIFT_API AGSGravityGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGSGravityGameMode();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TSubclassOf<AGSGravityManager> GravityManagerClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TSubclassOf<AGSWorldStateManager> WorldStateManagerClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bAutoSpawnCoreManagers = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<UGSBallProfile> DefaultBallProfile = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<UGSLandingProfile> DefaultLandingProfile = nullptr;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void EnsureCoreManagers();

	virtual void BeginPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
};

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "GS Gravity HUD"))
class GRAVITYSHIFT_API AGSGravityHUD : public AHUD
{
	GENERATED_BODY()

public:
	AGSGravityHUD();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bShowGravityStatus = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bShowFallStatus = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bShowCollectibleStatus = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bShowInteractionPrompt = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bShowControls = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FVector2D StartPosition = FVector2D(24.0f, 24.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "1.0"))
	float LineHeight = 22.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FLinearColor TextColor = FLinearColor::White;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetHUDVisible(bool bVisible);

	virtual void DrawHUD() override;

protected:
	bool bHUDVisible = true;
};
