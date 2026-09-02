#include "GSGravityManager.h"

#include "EngineUtils.h"
#include "GSGravityBodyComponent.h"
#include "GSProfiles.h"

AGSGravityManager::AGSGravityManager()
{
	PrimaryActorTick.bCanEverTick = false;
	bNetLoadOnClient = false;
}

void AGSGravityManager::BeginPlay()
{
	Super::BeginPlay();

	if (GravityProfile)
	{
		ApplyGravityProfile(GravityProfile);
	}

	CurrentPolarity = DefaultPolarity;
	LastChangeReason = EGSGravityChangeReason::RESET;

	UE_LOG(LogTemp, Log, TEXT("[GravityShift] Manager online: polarity=%s accel=%.1f bodies=%d"),
		CurrentPolarity == EGSGravityPolarity::POSITIVE_Z ? TEXT("+Z") : TEXT("-Z"),
		GravityAccelerationCm,
		RegisteredBodies.Num());
}

void AGSGravityManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

bool AGSGravityManager::IsAutomaticReason(EGSGravityChangeReason Reason) const
{
	return Reason == EGSGravityChangeReason::FALL_THRESHOLD || Reason == EGSGravityChangeReason::LANDING_RESPONSE;
}

FVector AGSGravityManager::GetGravityDirection() const
{
	return GSGravity::PolarityToDirection(CurrentPolarity);
}

FVector AGSGravityManager::GetGravityAccelerationVector() const
{
	return GetGravityDirection() * GravityAccelerationCm;
}

float AGSGravityManager::GetCooldownRemaining(EGSGravityChangeReason Reason) const
{
	const double* Last = LastChangeTimeByReason.Find(static_cast<uint8>(Reason));
	const UWorld* World = GetWorld();
	if (!Last || !World)
	{
		return 0.0f;
	}

	const float Cooldown = IsAutomaticReason(Reason) ? AutomaticCooldownSeconds : ManualCooldownSeconds;
	return FMath::Max(0.0f, Cooldown - static_cast<float>(World->GetTimeSeconds() - *Last));
}

EGSGravityRequestResult AGSGravityManager::RequestToggleGravity(AActor* Requester, EGSGravityChangeReason Reason, bool bForce)
{
	const EGSGravityPolarity Wanted = CurrentPolarity == EGSGravityPolarity::NEGATIVE_Z
		? EGSGravityPolarity::POSITIVE_Z
		: EGSGravityPolarity::NEGATIVE_Z;

	return RequestGravityPolarity(Wanted, Requester, Reason, bForce);
}

EGSGravityRequestResult AGSGravityManager::RequestGravityPolarity(EGSGravityPolarity NewPolarity, AActor* Requester, EGSGravityChangeReason Reason, bool bForce)
{
	if (bGravityLocked && !bForce)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[GravityShift] request rejected: locked"));
		return EGSGravityRequestResult::REJECTED_LOCKED;
	}

	if (NewPolarity == CurrentPolarity)
	{
		return EGSGravityRequestResult::NO_CHANGE;
	}

	if (!bForce)
	{
		const float Remaining = GetCooldownRemaining(Reason);
		if (Remaining > 0.0f)
		{
			UE_LOG(LogTemp, Verbose, TEXT("[GravityShift] request rejected: cooldown %.3fs"), Remaining);
			return EGSGravityRequestResult::REJECTED_COOLDOWN;
		}
	}

	CommitPolarity(NewPolarity, Reason);
	return EGSGravityRequestResult::ACCEPTED;
}

EGSGravityRequestResult AGSGravityManager::ResetGravity(bool bForce)
{
	return RequestGravityPolarity(DefaultPolarity, this, EGSGravityChangeReason::RESET, bForce);
}

void AGSGravityManager::CommitPolarity(EGSGravityPolarity NewPolarity, EGSGravityChangeReason Reason)
{
	CurrentPolarity = NewPolarity;
	LastChangeReason = Reason;
	++GravityRevision;

	UWorld* World = GetWorld();
	if (World)
	{
		LastChangeTimeByReason.Add(static_cast<uint8>(Reason), World->GetTimeSeconds());
	}

	UE_LOG(LogTemp, Log, TEXT("[GravityShift] polarity=%s revision=%d reason=%d requester=%s"),
		CurrentPolarity == EGSGravityPolarity::POSITIVE_Z ? TEXT("+Z") : TEXT("-Z"),
		GravityRevision,
		static_cast<int32>(Reason),
		*GetNameSafe(this));

	OnGravityChanged.Broadcast(CurrentPolarity, GetGravityDirection(), GravityRevision, Reason);
}

void AGSGravityManager::SetGravityLocked(bool bLocked)
{
	bGravityLocked = bLocked;
}

void AGSGravityManager::ApplyGravityProfile(UGSGravityProfile* NewProfile)
{
	if (!NewProfile)
	{
		return;
	}

	GravityProfile = NewProfile;
	GravityAccelerationCm = NewProfile->GravityAccelerationCm;
	ManualCooldownSeconds = NewProfile->ManualCooldownSeconds;
	AutomaticCooldownSeconds = NewProfile->AutomaticCooldownSeconds;
	DefaultPolarity = NewProfile->DefaultPolarity;
}

void AGSGravityManager::RegisterGravityBody(UGSGravityBodyComponent* GravityBody)
{
	if (!GravityBody || RegisteredBodies.Contains(GravityBody))
	{
		return;
	}

	RegisteredBodies.Add(GravityBody);
}

void AGSGravityManager::UnregisterGravityBody(UGSGravityBodyComponent* GravityBody)
{
	RegisteredBodies.Remove(GravityBody);
}

int32 AGSGravityManager::GetRegisteredBodyCount() const
{
	int32 Count = 0;
	for (const TObjectPtr<UGSGravityBodyComponent>& Body : RegisteredBodies)
	{
		if (Body)
		{
			++Count;
		}
	}
	return Count;
}

AGSGravityManager* AGSGravityManager::FindGravityManager(UObject* WorldContextObject)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AGSGravityManager> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}
