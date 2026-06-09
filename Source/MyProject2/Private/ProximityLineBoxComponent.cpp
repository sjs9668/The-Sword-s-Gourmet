#include "ProximityLineBoxComponent.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

UProximityLineBoxComponent::UProximityLineBoxComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = CheckInterval;
}

void UProximityLineBoxComponent::BeginPlay()
{
	Super::BeginPlay();

	PrimaryComponentTick.TickInterval = CheckInterval;
	RefreshSourceComponent();
}

void UProximityLineBoxComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateVisibilityForLocalPlayer();
}

void UProximityLineBoxComponent::RefreshSourceComponent()
{
	if (AActor* Owner = GetOwner())
	{
		CachedSourceComponent = Cast<USceneComponent>(BoundsSourceComponent.GetComponent(Owner));
	}
	else
	{
		CachedSourceComponent.Reset();
	}
}

void UProximityLineBoxComponent::SetForceVisible(bool bVisible)
{
	bForceVisible = bVisible;
	UpdateVisibilityForLocalPlayer();
}

void UProximityLineBoxComponent::UpdateVisibilityForLocalPlayer()
{
	if (!GetWorld())
	{
		return;
	}

	if (!CachedSourceComponent.IsValid())
	{
		RefreshSourceComponent();
	}

	bool bShouldDraw = bForceVisible;
	if (!bShouldDraw)
	{
		APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
		bShouldDraw = ShouldDraw(PlayerPawn);
	}

	bIsVisible = bShouldDraw;

	if (!bShouldDraw)
	{
		return;
	}

	FTransform BoxTransform;
	FVector BoxExtent;
	if (ResolveBoxData(BoxTransform, BoxExtent))
	{
		DrawLineBox(BoxTransform, BoxExtent);
	}
}

bool UProximityLineBoxComponent::ShouldDraw(APawn* PlayerPawn) const
{
	if (!PlayerPawn || !GetOwner())
	{
		return false;
	}

	const float MaxDistanceSq = FMath::Square(HighlightRadius);
	return FVector::DistSquared(PlayerPawn->GetActorLocation(), GetDistanceOrigin()) <= MaxDistanceSq;
}

FVector UProximityLineBoxComponent::GetDistanceOrigin() const
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

bool UProximityLineBoxComponent::ResolveBoxData(FTransform& OutTransform, FVector& OutExtent) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	USceneComponent* SourceComponent = CachedSourceComponent.Get();
	if (!SourceComponent)
	{
		SourceComponent = Owner->GetRootComponent();
	}

	if (!SourceComponent)
	{
		return false;
	}

	const FTransform SourceTransform = SourceComponent->GetComponentTransform();
	FVector WorldLocation = SourceTransform.TransformPosition(LocalBoxOffset);
	FVector ResolvedExtent = ManualBoxExtent.GetAbs();

	if (bUseSourceComponentBounds)
	{
		if (const UBoxComponent* BoxComponent = Cast<UBoxComponent>(SourceComponent))
		{
			ResolvedExtent = BoxComponent->GetScaledBoxExtent().GetAbs();
			WorldLocation = SourceTransform.TransformPosition(LocalBoxOffset);
		}
		else if (const UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(SourceComponent))
		{
			FVector LocalMin(ForceInitToZero);
			FVector LocalMax(ForceInitToZero);
			StaticMeshComponent->GetLocalBounds(LocalMin, LocalMax);

			const FVector LocalCenter = ((LocalMin + LocalMax) * 0.5f) + LocalBoxOffset;
			const FVector LocalExtent = ((LocalMax - LocalMin) * 0.5f).GetAbs();
			const FVector WorldScale = SourceTransform.GetScale3D().GetAbs();

			ResolvedExtent = LocalExtent * WorldScale;
			WorldLocation = SourceTransform.TransformPosition(LocalCenter);
		}
		else
		{
			const FBoxSphereBounds LocalBounds = SourceComponent->GetLocalBounds();
			if (!LocalBounds.BoxExtent.IsNearlyZero())
			{
				ResolvedExtent = LocalBounds.BoxExtent.GetAbs() * SourceTransform.GetScale3D().GetAbs();
				WorldLocation = SourceTransform.TransformPosition(LocalBounds.Origin + LocalBoxOffset);
			}
		}
	}

	if (ResolvedExtent.GetMin() <= KINDA_SMALL_NUMBER)
	{
		ResolvedExtent = ManualBoxExtent.GetAbs();
	}

	OutTransform = FTransform(SourceTransform.GetRotation(), WorldLocation);
	OutExtent = ResolvedExtent;
	return true;
}

void UProximityLineBoxComponent::DrawLineBox(const FTransform& BoxTransform, const FVector& BoxExtent) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Extent = BoxExtent.GetAbs();

	const FVector LocalCorners[8] =
	{
		FVector(-Extent.X, -Extent.Y, -Extent.Z),
		FVector( Extent.X, -Extent.Y, -Extent.Z),
		FVector(-Extent.X,  Extent.Y, -Extent.Z),
		FVector( Extent.X,  Extent.Y, -Extent.Z),
		FVector(-Extent.X, -Extent.Y,  Extent.Z),
		FVector( Extent.X, -Extent.Y,  Extent.Z),
		FVector(-Extent.X,  Extent.Y,  Extent.Z),
		FVector( Extent.X,  Extent.Y,  Extent.Z)
	};

	const int32 EdgePairs[12][2] =
	{
		{0, 1}, {1, 3}, {3, 2}, {2, 0},
		{4, 5}, {5, 7}, {7, 6}, {6, 4},
		{0, 4}, {1, 5}, {2, 6}, {3, 7}
	};

	FVector WorldCorners[8];
	for (int32 CornerIndex = 0; CornerIndex < UE_ARRAY_COUNT(LocalCorners); ++CornerIndex)
	{
		WorldCorners[CornerIndex] = BoxTransform.TransformPosition(LocalCorners[CornerIndex]);
	}

	for (int32 EdgeIndex = 0; EdgeIndex < UE_ARRAY_COUNT(EdgePairs); ++EdgeIndex)
	{
		const int32 StartIndex = EdgePairs[EdgeIndex][0];
		const int32 EndIndex = EdgePairs[EdgeIndex][1];

		DrawDebugLine(
			World,
			WorldCorners[StartIndex],
			WorldCorners[EndIndex],
			LineColor.ToFColor(true),
			false,
			LineLifetime,
			static_cast<uint8>(DepthPriority),
			LineThickness);
	}
}
