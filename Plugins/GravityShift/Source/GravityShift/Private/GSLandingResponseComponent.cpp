#include "GSLandingResponseComponent.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "WorldCollision.h"
#include "GSGravityBodyComponent.h"
#include "GSGravityManager.h"
#include "GSProfiles.h"

UGSLandingResponseComponent::UGSLandingResponseComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UGSLandingResponseComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshReferences();
}

void UGSLandingResponseComponent::RefreshReferences()
{
	AActor* OwnerActor = GetOwner();

	if (!TargetPrimitive && OwnerActor)
	{
		UPrimitiveComponent* Found = Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent());
		if (!Found)
		{
			Found = OwnerActor->FindComponentByClass<UStaticMeshComponent>();
		}
		SetTargetPrimitive(Found);
	}

	if (!GravityManager)
	{
		GravityManager = AGSGravityManager::FindGravityManager(this);
	}

	if (!GravityBody && OwnerActor)
	{
		GravityBody = OwnerActor->FindComponentByClass<UGSGravityBodyComponent>();
	}
}

void UGSLandingResponseComponent::SetTargetPrimitive(UPrimitiveComponent* NewTarget)
{
	TargetPrimitive = NewTarget;
}

void UGSLandingResponseComponent::ApplyLandingModifier(AActor* Source, FGSLandingModifierSpec Spec)
{
	if (Source)
	{
		ActiveModifiers.Add(Source, Spec);
	}
}

void UGSLandingResponseComponent::ApplyLandingProfile(AActor* Source, UGSLandingProfile* Profile)
{
	if (Source && Profile)
	{
		ApplyLandingModifier(Source, Profile->Spec);
	}
}

void UGSLandingResponseComponent::RemoveLandingModifier(AActor* Source)
{
	if (Source)
	{
		ActiveModifiers.Remove(Source);
	}
}

void UGSLandingResponseComponent::ClearLandingModifiers()
{
	ActiveModifiers.Empty();
}

FGSLandingModifierSpec UGSLandingResponseComponent::GetEffectiveLandingModifier() const
{
	FGSLandingModifierSpec Spec;
	Spec.NoResponseBelowImpactSpeedCm = NoResponseBelowImpactSpeedCm;
	Spec.BounceSpeedCm = BounceSpeedCm;
	Spec.AutoReverseAtSpeedCm = LandingAutoReverseAtSpeedCm;
	Spec.BounceTangentialRetention = BounceTangentialRetention;

	if (ActiveModifiers.Num() == 0)
	{
		return Spec;
	}

	const FGSLandingModifierSpec* Best = nullptr;
	for (const TPair<TObjectPtr<AActor>, FGSLandingModifierSpec>& Pair : ActiveModifiers)
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

	return Best ? *Best : Spec;
}

int32 UGSLandingResponseComponent::GetActiveLandingModifierCount() const
{
	int32 Count = 0;
	for (const TPair<TObjectPtr<AActor>, FGSLandingModifierSpec>& Pair : ActiveModifiers)
	{
		if (Pair.Key)
		{
			++Count;
		}
	}
	return Count;
}

float UGSLandingResponseComponent::GetCurrentFallSpeedCm() const { return CurrentFallSpeedCm; }
float UGSLandingResponseComponent::GetCurrentFallDistanceCm() const { return CurrentFallDistanceCm; }
float UGSLandingResponseComponent::GetAirborneSeconds() const { return AirborneSeconds; }
bool UGSLandingResponseComponent::IsSupported() const { return bWasSupported; }
FGSLandingReport UGSLandingResponseComponent::GetLastLandingReport() const { return LastLandingReport; }

void UGSLandingResponseComponent::ResetFlightState()
{
	AirborneSeconds = 0.0f;
	CurrentFallSpeedCm = 0.0f;
	CurrentFallDistanceCm = 0.0f;
	bReverseConsumedThisFlight = false;
	bBouncedSinceQuietLanding = false;
	bWasSupported = true;
}

bool UGSLandingResponseComponent::ProbeGround() const
{
	if (!TargetPrimitive)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector Dir = GravityManager ? GravityManager->GetGravityDirection() : FVector(0.0, 0.0, -1.0);
	const float Radius = FMath::Max(TargetPrimitive->Bounds.SphereRadius * GroundProbeRadiusScale, 1.0f);
	const FVector Start = TargetPrimitive->GetComponentLocation();
	const FVector End = Start + Dir * (GroundProbeDistanceCm + Radius);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSGroundProbe), false, GetOwner());
	FHitResult Hit;
	const bool bHit = World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, GroundProbeChannel, FCollisionShape::MakeSphere(Radius), Params);
	if (!bHit)
	{
		return false;
	}

	AActor* HitActor = Hit.GetActor();
	if (HitActor && HitActor == GetOwner())
	{
		return false;
	}

	const float NormalDot = FVector::DotProduct(Hit.ImpactNormal, -Dir);
	return NormalDot >= MinimumLandingNormalDot;
}

void UGSLandingResponseComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEnabled || !TargetPrimitive || !TargetPrimitive->IsSimulatingPhysics() || DeltaTime <= 0.0f)
	{
		return;
	}

	const FVector Dir = GravityManager ? GravityManager->GetGravityDirection() : FVector(0.0, 0.0, -1.0);
	const FVector Velocity = TargetPrimitive->GetPhysicsLinearVelocity();
	const float FallSpeed = FVector::DotProduct(Velocity, Dir);

	const bool bSupported = ProbeGround();

	if (bSupported)
	{
		if (!bWasSupported)
		{
			HandleLanding(FMath::Abs(CurrentFallSpeedCm));
		}

		AirborneSeconds = 0.0f;
		CurrentFallSpeedCm = 0.0f;
		CurrentFallDistanceCm = 0.0f;
		bReverseConsumedThisFlight = false;
	}
	else
	{
		AirborneSeconds += DeltaTime;
		CurrentFallSpeedCm = FallSpeed;
		if (FallSpeed > 0.0f)
		{
			CurrentFallDistanceCm += FallSpeed * DeltaTime;
		}

		const bool bModeAllowsMidAir = AutoReverseMode == EGSAutoReverseMode::MID_AIR_THRESHOLD
			|| AutoReverseMode == EGSAutoReverseMode::MID_AIR_OR_LANDING;

		const bool bSpeedOk = FallSpeed >= AutoReverseFallSpeedCm;
		const bool bDistanceOk = CurrentFallDistanceCm >= AutoReverseFallDistanceCm;
		const bool bThresholdHit = bRequireSpeedAndDistanceForMidairReverse ? (bSpeedOk && bDistanceOk) : (bSpeedOk || bDistanceOk);

		if (bModeAllowsMidAir && !bReverseConsumedThisFlight && AirborneSeconds >= MinimumAirborneSeconds && bThresholdHit)
		{
			if (RequestAutomaticReverse(EGSGravityChangeReason::FALL_THRESHOLD))
			{
				ClampCarriedVelocity(AutomaticVelocityRetention, AutomaticMaxCarrySpeedCm);
			}
		}
	}

	bWasSupported = bSupported;
}

void UGSLandingResponseComponent::HandleLanding(float ImpactSpeedCm)
{
	UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0f;

	FGSLandingReport Report;
	Report.ImpactSpeedCm = ImpactSpeedCm;
	Report.FallSpeedCm = CurrentFallSpeedCm;
	Report.FallDistanceCm = CurrentFallDistanceCm;
	Report.TimeSeconds = Now;

	const FGSLandingModifierSpec Mod = GetEffectiveLandingModifier();

	if (!bEnabled || Mod.bSuppressResponse)
	{
		Report.Action = EGSLandingResponseAction::SUPPRESSED;
		LastLandingReport = Report;
		return;
	}

	if (Now - LastResponseTime < LocalResponseCooldownSeconds)
	{
		Report.Action = EGSLandingResponseAction::NONE;
		LastLandingReport = Report;
		return;
	}

	if (ImpactSpeedCm < Mod.NoResponseBelowImpactSpeedCm)
	{
		Report.Action = EGSLandingResponseAction::NONE;
		// A quiet landing resets the one-bounce cycle: the next bounce-band
		// landing is allowed to bounce again.
		bBouncedSinceQuietLanding = false;
		LastLandingReport = Report;
		return;
	}

	const bool bModeAllowsLanding = AutoReverseMode == EGSAutoReverseMode::LANDING_IMPACT
		|| AutoReverseMode == EGSAutoReverseMode::MID_AIR_OR_LANDING;

	if (bModeAllowsLanding && ImpactSpeedCm >= Mod.AutoReverseAtSpeedCm)
	{
		if (RequestAutomaticReverse(EGSGravityChangeReason::LANDING_RESPONSE))
		{
			ClampCarriedVelocity(AutomaticVelocityRetention, AutomaticMaxCarrySpeedCm);
			Report.Action = EGSLandingResponseAction::GRAVITY_REVERSED;
			Report.bGravityReversed = true;
			bBouncedSinceQuietLanding = false;
			LastResponseTime = Now;
			LastLandingReport = Report;
			return;
		}
	}

	// One bounce per cycle: after bouncing once, a further bounce-band landing
	// settles instead — the fixed bounce speed would otherwise relaunch forever.
	if (bBouncedSinceQuietLanding)
	{
		Report.Action = EGSLandingResponseAction::NONE;
		LastLandingReport = Report;
		return;
	}

	// Bounce: replace the gravity-axis velocity, optionally keeping tangential motion.
	const FVector Dir = GravityManager ? GravityManager->GetGravityDirection() : FVector(0.0, 0.0, -1.0);
	UPrimitiveComponent* Prim = TargetPrimitive;
	FVector Velocity = Prim->GetPhysicsLinearVelocity();
	const FVector NormalPart = Dir * FVector::DotProduct(Velocity, Dir);
	const FVector TangentPart = Velocity - NormalPart;

	const float Retention = bPreserveTangentialVelocityOnBounce ? Mod.BounceTangentialRetention : 0.0f;
	Velocity = -Dir * Mod.BounceSpeedCm + TangentPart * Retention;
	Prim->SetPhysicsLinearVelocity(Velocity);

	Report.Action = EGSLandingResponseAction::BOUNCE;
	bBouncedSinceQuietLanding = true;
	LastResponseTime = Now;
	LastLandingReport = Report;
}

bool UGSLandingResponseComponent::RequestAutomaticReverse(EGSGravityChangeReason Reason)
{
	if (!GravityManager)
	{
		return false;
	}

	AActor* OwnerActor = GetOwner();
	if (OwnerActor && !bReverseConsumedThisFlight)
	{
		const EGSGravityRequestResult Result = GravityManager->RequestToggleGravity(OwnerActor, Reason, false);
		if (Result == EGSGravityRequestResult::ACCEPTED)
		{
			bReverseConsumedThisFlight = true;
			UE_LOG(LogTemp, Log, TEXT("[GravityShift] automatic reverse (%s) by %s"),
				Reason == EGSGravityChangeReason::FALL_THRESHOLD ? TEXT("fall") : TEXT("landing"),
				*GetNameSafe(OwnerActor));
			return true;
		}
	}

	return false;
}

void UGSLandingResponseComponent::ClampCarriedVelocity(float Retention, float MaxCarrySpeedCm)
{
	if (!TargetPrimitive)
	{
		return;
	}

	const FVector Dir = GravityManager ? GravityManager->GetGravityDirection() : FVector(0.0, 0.0, -1.0);
	FVector Velocity = TargetPrimitive->GetPhysicsLinearVelocity();
	const FVector NormalPart = Dir * FVector::DotProduct(Velocity, Dir);
	const FVector TangentPart = Velocity - NormalPart;

	float Carry = NormalPart.Size();
	if (MaxCarrySpeedCm > 0.0f && Carry > MaxCarrySpeedCm)
	{
		Carry = MaxCarrySpeedCm;
	}

	Velocity = TangentPart * Retention + Dir.GetSafeNormal() * Carry;
	TargetPrimitive->SetPhysicsLinearVelocity(Velocity);
}

bool UGSLandingResponseComponent::ForceEvaluateAutomaticReverse()
{
	return RequestAutomaticReverse(EGSGravityChangeReason::FALL_THRESHOLD);
}
