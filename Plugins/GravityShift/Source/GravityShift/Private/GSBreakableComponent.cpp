#include "GSBreakableComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "GSGravityBodyComponent.h"
#include "GSProfiles.h"

UGSBreakableComponent::UGSBreakableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGSBreakableComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!TargetPrimitive)
	{
		AActor* OwnerActor = GetOwner();
		UPrimitiveComponent* Found = OwnerActor ? Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent()) : nullptr;
		if (!Found && OwnerActor)
		{
			Found = OwnerActor->FindComponentByClass<UStaticMeshComponent>();
		}
		SetTargetPrimitive(Found);
	}

	CaptureInitialState();
}

void UGSBreakableComponent::SetTargetPrimitive(UPrimitiveComponent* NewTarget)
{
	TargetPrimitive = NewTarget;
}

void UGSBreakableComponent::CaptureInitialState()
{
	InitialHealth = MaximumHealth;
	CurrentHealth = MaximumHealth;
	bHasCapturedState = true;
}

void UGSBreakableComponent::ApplyBreakProfile(UGSBreakProfile* NewProfile)
{
	if (!NewProfile)
	{
		return;
	}

	BreakProfile = NewProfile;
	bBreakable = NewProfile->bBreakable;
	bOneHitBreakAboveThreshold = NewProfile->bOneHitBreakAboveThreshold;
	MinimumImpactEnergyJ = NewProfile->MinimumImpactEnergyJ;
	MaximumHealth = NewProfile->MaximumHealth;
	DamageScalePerJ = NewProfile->DamageScalePerJ;
	RequiredSourceTag = NewProfile->RequiredSourceTag;
	bHideOwnerWhenBroken = NewProfile->bHideOwnerWhenBroken;
	bDisableCollisionWhenBroken = NewProfile->bDisableCollisionWhenBroken;
	bDisablePhysicsWhenBroken = NewProfile->bDisablePhysicsWhenBroken;
	BrokenMesh = NewProfile->BrokenMesh;
	CurrentHealth = MaximumHealth;
}

bool UGSBreakableComponent::SourceTagMatches(AActor* Instigator) const
{
	if (RequiredSourceTag == NAME_None)
	{
		return true;
	}

	UGSGravityBodyComponent* Body = Instigator ? Instigator->FindComponentByClass<UGSGravityBodyComponent>() : nullptr;
	if (Body && Body->ImpactSourceTag == RequiredSourceTag)
	{
		return true;
	}

	return Instigator && Instigator->ActorHasTag(RequiredSourceTag);
}

float UGSBreakableComponent::ApplyImpactEnergy(float EnergyJ, AActor* Instigator)
{
	if (!bBreakable || bBroken || EnergyJ <= 0.0f)
	{
		return 0.0f;
	}

	if (!SourceTagMatches(Instigator))
	{
		return 0.0f;
	}

	if (EnergyJ < MinimumImpactEnergyJ)
	{
		return 0.0f;
	}

	const float Damage = EnergyJ * DamageScalePerJ;
	CurrentHealth = FMath::Max(0.0f, CurrentHealth - Damage);

	UE_LOG(LogTemp, Log, TEXT("[GravityShift] impact %.2fJ on %s -> health %.2f"), EnergyJ, *GetNameSafe(GetOwner()), CurrentHealth);

	if (bOneHitBreakAboveThreshold || CurrentHealth <= 0.0f)
	{
		BreakNow(Instigator, EnergyJ);
	}

	return Damage;
}

bool UGSBreakableComponent::BreakNow(AActor* Instigator, float EnergyJ)
{
	if (!bBreakable || bBroken)
	{
		return false;
	}

	bBroken = true;
	CurrentHealth = 0.0f;

	AActor* OwnerActor = GetOwner();
	UPrimitiveComponent* Prim = TargetPrimitive;

	if (bDisablePhysicsWhenBroken && Prim)
	{
		Prim->SetSimulatePhysics(false);
	}
	if (bDisableCollisionWhenBroken && Prim)
	{
		Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (bHideOwnerWhenBroken && OwnerActor)
	{
		OwnerActor->SetActorHiddenInGame(true);
	}

	UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(Prim);
	if (MeshComp && BrokenMesh)
	{
		MeshComp->SetStaticMesh(BrokenMesh);
	}

	UE_LOG(LogTemp, Log, TEXT("[GravityShift] %s BROKEN by %s (%.2fJ)"), *GetNameSafe(OwnerActor), *GetNameSafe(Instigator), EnergyJ);
	return true;
}

bool UGSBreakableComponent::Repair()
{
	if (!bBroken)
	{
		return false;
	}

	bBroken = false;
	CurrentHealth = bHasCapturedState ? InitialHealth : MaximumHealth;

	AActor* OwnerActor = GetOwner();
	UPrimitiveComponent* Prim = TargetPrimitive;

	if (OwnerActor)
	{
		OwnerActor->SetActorHiddenInGame(false);
	}
	if (Prim)
	{
		Prim->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	UE_LOG(LogTemp, Log, TEXT("[GravityShift] %s repaired"), *GetNameSafe(OwnerActor));
	return true;
}

bool UGSBreakableComponent::IsBroken() const
{
	return bBroken;
}
