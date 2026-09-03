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

	CurrentDirection = DefaultDirection;
	CurrentPolarity = GSGravity::DirectionToLegacyPolarity(CurrentDirection);
	DefaultPolarity = GSGravity::DirectionToLegacyPolarity(DefaultDirection);
	LastChangeReason = EGSGravityChangeReason::RESET;

	UE_LOG(LogTemp, Log, TEXT("[GravityShift] Manager online: dir=%s accel=%.1f bodies=%d"),
		*GSGravity::GetDirectionDisplayName(CurrentDirection),
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
	return GSGravity::DirectionToVector(CurrentDirection);
}

FVector AGSGravityManager::GetGravityAccelerationVector() const
{
	return GetGravityDirection() * GravityAccelerationCm;
}

EGSGravityDirection AGSGravityManager::GetCurrentDirection() const
{
	return CurrentDirection;
}

TArray<EGSGravityAxis> AGSGravityManager::GetAllowedAxes() const
{
	if (AllowedAxes.Num() == 0)
	{
		TArray<EGSGravityAxis> All;
		All.Add(EGSGravityAxis::X);
		All.Add(EGSGravityAxis::Y);
		All.Add(EGSGravityAxis::Z);
		return All;
	}
	return AllowedAxes;
}

void AGSGravityManager::SetAllowedAxes(const TArray<EGSGravityAxis>& InAllowedAxes)
{
	AllowedAxes = InAllowedAxes;
}

bool AGSGravityManager::IsDirectionAllowed(EGSGravityDirection Dir) const
{
	return GSGravity::IsAxisAllowed(GSGravity::GetAxisFromDirection(Dir), AllowedAxes);
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

EGSGravityRequestResult AGSGravityManager::RequestGravityDirection(EGSGravityDirection NewDirection, AActor* Requester,
	EGSGravityChangeReason Reason, bool bForce)
{
	if (NewDirection == CurrentDirection)
	{
		return EGSGravityRequestResult::NO_CHANGE;
	}

	if (!IsDirectionAllowed(NewDirection))
	{
		UE_LOG(LogTemp, Verbose, TEXT("[GravityShift] request rejected: %s not in allowed axes"),
			*GSGravity::GetDirectionDisplayName(NewDirection));
		return EGSGravityRequestResult::REJECTED_DISABLED;
	}

	if (bGravityLocked && !bForce)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[GravityShift] request rejected: locked"));
		return EGSGravityRequestResult::REJECTED_LOCKED;
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

	CommitDirection(NewDirection, Reason, Requester);
	return EGSGravityRequestResult::ACCEPTED;
}

EGSGravityRequestResult AGSGravityManager::SetGravityAxis(EGSGravityAxis Axis, AActor* Requester, bool bForce)
{
	if (!GSGravity::IsAxisAllowed(Axis, AllowedAxes))
	{
		return EGSGravityRequestResult::REJECTED_DISABLED;
	}

	return RequestGravityDirection(GSGravity::GetPositiveDirection(Axis), Requester,
		EGSGravityChangeReason::MANUAL, bForce);
}

EGSGravityRequestResult AGSGravityManager::ToggleCurrentAxis(AActor* Requester, EGSGravityChangeReason Reason, bool bForce)
{
	const EGSGravityDirection Target = GSGravity::FlipDirection(CurrentDirection);
	if (!IsDirectionAllowed(Target))
	{
		return EGSGravityRequestResult::REJECTED_DISABLED;
	}

	return RequestGravityDirection(Target, Requester, Reason, bForce);
}

// ---- legacy Z-only entry points ----------------------------------------------------

EGSGravityRequestResult AGSGravityManager::RequestToggleGravity(AActor* Requester, EGSGravityChangeReason Reason, bool bForce)
{
	return ToggleCurrentAxis(Requester, Reason, bForce);
}

EGSGravityRequestResult AGSGravityManager::RequestGravityPolarity(EGSGravityPolarity NewPolarity, AActor* Requester,
	EGSGravityChangeReason Reason, bool bForce)
{
	return RequestGravityDirection(GSGravity::VectorToDirection(GSGravity::PolarityToDirection(NewPolarity)),
		Requester, Reason, bForce);
}

EGSGravityRequestResult AGSGravityManager::ResetGravity(bool bForce)
{
	return RequestGravityDirection(DefaultDirection, this, EGSGravityChangeReason::RESET, bForce);
}

void AGSGravityManager::CommitDirection(EGSGravityDirection NewDirection, EGSGravityChangeReason Reason, AActor* Requester)
{
	CurrentDirection = NewDirection;
	CurrentPolarity = GSGravity::DirectionToLegacyPolarity(CurrentDirection);
	LastChangeReason = Reason;
	++GravityRevision;

	UWorld* World = GetWorld();
	if (World)
	{
		LastChangeTimeByReason.Add(static_cast<uint8>(Reason), World->GetTimeSeconds());
	}

	const FVector DirVec = GetGravityDirection();
	UE_LOG(LogTemp, Log, TEXT("[GravityShift] dir=%s (%s) revision=%d reason=%d requester=%s"),
		*GSGravity::GetDirectionDisplayName(CurrentDirection),
		*DirVec.ToString(),
		GravityRevision,
		static_cast<int32>(Reason),
		*GetNameSafe(Requester));

	// Six-direction primary broadcast, then the legacy Z-only broadcast (compat).
	OnGravityDirectionChanged.Broadcast(CurrentDirection, DirVec, GravityRevision, Reason, Requester);
	OnGravityChanged.Broadcast(CurrentPolarity, DirVec, GravityRevision, Reason);
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
	// The profile only speaks +/-Z; the level's per-direction default (if any) is
	// applied afterwards by AGSWorldStateManager and wins.
	DefaultDirection = NewProfile->DefaultPolarity == EGSGravityPolarity::POSITIVE_Z
		? EGSGravityDirection::POSITIVE_Z
		: EGSGravityDirection::NEGATIVE_Z;
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
