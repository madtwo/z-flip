#include "GSMenuGameMode.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#include "GSMenuWidgets.h"

AGSMenuGameMode::AGSMenuGameMode()
{
	// 菜单关卡不要游戏 Pawn(球会掉下去、会被 WASD 推),也不要 AGSGravityHUD。
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;

	// 但视图目标得有一个,否则关卡不渲染。观察者 Pawn 是引擎给"无玩法 Pawn"关卡的
	// 正规做法:相机存在、不接受玩法输入。
	bStartPlayersAsSpectators = true;

	MenuWidgetClass = UGSMainMenuWidget::StaticClass();
}

void AGSMenuGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!MenuWidgetClass)
	{
		return;
	}

	// GameMode 的 BeginPlay 在 World::BeginPlay 阶段,本地 PlayerController 此时已经
	// 由 UGameInstance::CreateLocalPlayer 建好。取不到就说明这局没有本地玩家,不该有菜单。
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GSMenu] 没有本地 PlayerController,菜单未创建"));
		return;
	}

	UGSMainMenuWidget* Menu = CreateWidget<UGSMainMenuWidget>(PC, MenuWidgetClass);
	if (Menu)
	{
		Menu->AddToViewport();
		UE_LOG(LogTemp, Log, TEXT("[GSMenu] 主菜单已创建"));
	}
}
