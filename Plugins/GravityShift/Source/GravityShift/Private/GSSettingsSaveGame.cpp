#include "GSSettingsSaveGame.h"

#include "Kismet/GameplayStatics.h"

const FString UGSSettingsSaveGame::SaveSlotName = TEXT("ZFlipSettings");

UGSSettingsSaveGame* UGSSettingsSaveGame::LoadOrCreate()
{
	UGSSettingsSaveGame* Save = Cast<UGSSettingsSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SaveSlotName, UserIndex));

	if (!Save)
	{
		Save = NewObject<UGSSettingsSaveGame>();
	}

	Save->MouseSensitivityMultiplier = FMath::Clamp(
		Save->MouseSensitivityMultiplier,
		GSSettingsLimits::MinSensitivity,
		GSSettingsLimits::MaxSensitivity);

	return Save;
}

void UGSSettingsSaveGame::SaveMultiplier(float NewMultiplier)
{
	UGSSettingsSaveGame* Save = LoadOrCreate();
	Save->MouseSensitivityMultiplier = FMath::Clamp(
		NewMultiplier,
		GSSettingsLimits::MinSensitivity,
		GSSettingsLimits::MaxSensitivity);

	UGameplayStatics::SaveGameToSlot(Save, SaveSlotName, UserIndex);
}
