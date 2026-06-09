#include "ProximityOutlineComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

UProximityOutlineComponent::UProximityOutlineComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = CheckInterval;
}

void UProximityOutlineComponent::BeginPlay()
{
	Super::BeginPlay();

	PrimaryComponentTick.TickInterval = CheckInterval;
	RefreshOutlineTargets();
	SetHighlightEnabled(false);
}

void UProximityOutlineComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetHighlightEnabled(false);
	Super::EndPlay(EndPlayReason);
}

void UProximityOutlineComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateHighlightForLocalPlayer();
}

void UProximityOutlineComponent::RefreshOutlineTargets()
{
	SetHighlightEnabled(false);
	OutlineTargetStates.Reset();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (bIncludeOwnerPrimitiveComponents)
	{
		GatherPrimitivesFromActor(Owner);
	}

	for (const FComponentReference& ComponentReference : OutlineComponentReferences)
	{
		if (UPrimitiveComponent* Primitive =
			Cast<UPrimitiveComponent>(ComponentReference.GetComponent(Owner)))
		{
			AddTargetPrimitive(Primitive);
		}
	}

	for (AActor* ActorToOutline : AdditionalActorsToOutline)
	{
		GatherPrimitivesFromActor(ActorToOutline);
	}

	// Fallback for the common case where the owner itself is the outline target
	// and no explicit component references were configured in the editor yet.
	if (!bIncludeOwnerPrimitiveComponents
		&& OutlineComponentReferences.Num() == 0
		&& AdditionalActorsToOutline.Num() == 0
		&& OutlineTargetStates.Num() == 0)
	{
		GatherPrimitivesFromActor(Owner);
		UE_LOG(
			LogTemp,
			Log,
			TEXT("ProximityOutlineComponent on '%s' fell back to owner primitive components because no outline targets were configured."),
			*Owner->GetName());
	}

	if (OutlineTargetStates.Num() == 0)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("ProximityOutlineComponent on '%s' found no primitive components to outline. Check OutlineComponentReferences, AdditionalActorsToOutline, and Hidden In Game settings."),
			*Owner->GetName());
	}
}

void UProximityOutlineComponent::SetForceHighlighted(bool bHighlighted)
{
	bForceHighlighted = bHighlighted;
	UpdateHighlightForLocalPlayer();
}

void UProximityOutlineComponent::SetHighlightSuppressed(bool bSuppressed)
{
	bHighlightSuppressed = bSuppressed;
	UpdateHighlightForLocalPlayer();
}

void UProximityOutlineComponent::UpdateHighlightForLocalPlayer()
{
	if (bHighlightSuppressed)
	{
		SetHighlightEnabled(false);
		return;
	}

	if (bForceHighlighted)
	{
		SetHighlightEnabled(true);
		return;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	SetHighlightEnabled(ShouldHighlight(PlayerPawn));
}

bool UProximityOutlineComponent::ShouldHighlight(APawn* PlayerPawn) const
{
	if (!PlayerPawn || !GetOwner())
	{
		return false;
	}

	const float MaxDistanceSq = FMath::Square(HighlightRadius);
	return FVector::DistSquared(PlayerPawn->GetActorLocation(), GetDistanceOrigin()) <= MaxDistanceSq;
}

void UProximityOutlineComponent::SetHighlightEnabled(bool bEnabled)
{
	if (bIsHighlighted == bEnabled)
	{
		return;
	}

	for (FOutlineTargetState& TargetState : OutlineTargetStates)
	{
		if (UPrimitiveComponent* Primitive = TargetState.Primitive.Get())
		{
			if (bEnabled)
			{
				Primitive->SetRenderCustomDepth(true);
			}
			else
			{
				Primitive->SetRenderCustomDepth(TargetState.bOriginalRenderCustomDepth);
				Primitive->SetCustomDepthStencilValue(TargetState.OriginalStencilValue);
			}

			Primitive->MarkRenderStateDirty();
		}
	}

	bIsHighlighted = bEnabled;
}

FVector UProximityOutlineComponent::GetDistanceOrigin() const
{
	if (const AActor* Owner = GetOwner())
	{
		if (USceneComponent* OriginComponent =
			Cast<USceneComponent>(DistanceCheckOrigin.GetComponent(const_cast<AActor*>(Owner))))
		{
			return OriginComponent->GetComponentLocation();
		}

		return Owner->GetActorLocation();
	}

	return FVector::ZeroVector;
}

void UProximityOutlineComponent::AddTargetPrimitive(UPrimitiveComponent* Primitive)
{
	if (!Primitive || !Primitive->IsRegistered())
	{
		return;
	}

	if (bIgnoreHiddenInGamePrimitives && Primitive->bHiddenInGame)
	{
		return;
	}

	for (const FOutlineTargetState& ExistingTarget : OutlineTargetStates)
	{
		if (ExistingTarget.Primitive.Get() == Primitive)
		{
			return;
		}
	}

	FOutlineTargetState NewTarget;
	NewTarget.Primitive = Primitive;
	NewTarget.bOriginalRenderCustomDepth = Primitive->bRenderCustomDepth;
	NewTarget.OriginalStencilValue = Primitive->CustomDepthStencilValue;
	OutlineTargetStates.Add(NewTarget);
}

void UProximityOutlineComponent::GatherPrimitivesFromActor(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
	Actor->GetComponents(PrimitiveComponents);

	for (UPrimitiveComponent* Primitive : PrimitiveComponents)
	{
		AddTargetPrimitive(Primitive);
	}
}
