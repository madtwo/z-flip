#include "GSSurfaceReceiverComponent.h"

#include "GSProfiles.h"

UGSSurfaceReceiverComponent::UGSSurfaceReceiverComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGSSurfaceReceiverComponent::ApplySurfaceModifier(AActor* Source, FGSSurfaceModifierSpec Spec)
{
	if (!bEnabled || !Source)
	{
		return;
	}

	ActiveModifiers.Add(Source, Spec);
}

void UGSSurfaceReceiverComponent::ApplySurfaceProfile(AActor* Source, UGSSurfaceProfile* Profile)
{
	if (!Profile)
	{
		return;
	}

	ApplySurfaceModifier(Source, Profile->Spec);
}

void UGSSurfaceReceiverComponent::RemoveSurfaceModifier(AActor* Source)
{
	if (Source)
	{
		ActiveModifiers.Remove(Source);
	}
}

void UGSSurfaceReceiverComponent::ClearSurfaceModifiers()
{
	ActiveModifiers.Empty();
}

FGSSurfaceModifierSpec UGSSurfaceReceiverComponent::GetEffectiveSurfaceModifier() const
{
	if (!bEnabled || ActiveModifiers.Num() == 0)
	{
		return DefaultModifier;
	}

	const FGSSurfaceModifierSpec* Best = nullptr;
	for (const TPair<TObjectPtr<AActor>, FGSSurfaceModifierSpec>& Pair : ActiveModifiers)
	{
		if (!Pair.Key)
		{
			continue;
		}

		if (!Best || Pair.Value.Priority > Best->Priority)
		{
			Best = &Pair.Value;
		}
	}

	return Best ? *Best : DefaultModifier;
}

int32 UGSSurfaceReceiverComponent::GetActiveModifierCount() const
{
	int32 Count = 0;
	for (const TPair<TObjectPtr<AActor>, FGSSurfaceModifierSpec>& Pair : ActiveModifiers)
	{
		if (Pair.Key)
		{
			++Count;
		}
	}
	return Count;
}
