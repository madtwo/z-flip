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
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
};

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "GS Gravity HUD"))
class GRAVITYSHIFT_API AGSGravityHUD : public AHUD
{
	GENERATED_BODY()

public:
	AGSGravityHUD();

	// 2026-10-11 收尾:正式演示不再显示左上角调试块。四个开发用开关全部默认关
	// (Gravity/Fall/Collectible 状态、英文操作行、编译时间戳);游戏自己的
	// HUD 提示(互动悬停文字 / 拾取线索)与 UMG 不受影响。
	// 排查时在关卡 HUD 的 Details 里把对应开关勾回来即可。

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bShowGravityStatus = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bShowFallStatus = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bShowCollectibleStatus = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bShowInteractionPrompt = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bShowControls = false;

	// 编译时间戳(多人协作仲裁"跑的 dll 是哪次编的"用),正式演示默认关。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bShowBuildStamp = false;

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
