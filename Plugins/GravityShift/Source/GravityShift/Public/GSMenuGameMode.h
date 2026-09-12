// GravityShift v5 - 菜单关卡的 GameMode。
//
// 故意继承 AGameModeBase 而不是 AGSGravityGameMode:菜单关卡不生成球、不生成
// AGSGravityManager / AGSWorldStateManager、不要 HUD(AGSGravityHUD 是游戏内 HUD,
// 和菜单无关)。菜单本体是 UGSMainMenuWidget,叠加在空关卡之上。

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GSMenuGameMode.generated.h"

class UGSMainMenuWidget;

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "GS Menu Game Mode"))
class GRAVITYSHIFT_API AGSMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGSMenuGameMode();

	// 留空则用 UGSMainMenuWidget。想做蓝图子类换皮时在这里挂。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Menu")
	TSubclassOf<UGSMainMenuWidget> MenuWidgetClass;

	virtual void BeginPlay() override;
};
