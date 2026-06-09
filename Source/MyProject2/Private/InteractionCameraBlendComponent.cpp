#include "InteractionCameraBlendComponent.h"

#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogInteractionCameraBlend, Log, All);

namespace
{
	const FName DiningTableFallbackCameraActorName(TEXT("CineCameraActor3"));

	bool IsDiningTableOwner(const AActor* Owner)
	{
		return IsValid(Owner) && Owner->GetClass()->GetName().Contains(TEXT("BP_table"));
	}

	FString BuildRuntimeActorNameFromEditorLabel(const FString& TargetName)
	{
		if (TargetName.IsEmpty())
		{
			return FString();
		}

		int32 DigitStartIndex = TargetName.Len();
		while (DigitStartIndex > 0 && FChar::IsDigit(TargetName[DigitStartIndex - 1]))
		{
			--DigitStartIndex;
		}

		if (DigitStartIndex == TargetName.Len())
		{
			return FString::Printf(TEXT("%s_0"), *TargetName);
		}

		const FString Prefix = TargetName.Left(DigitStartIndex);
		const int32 LabelIndex = FCString::Atoi(*TargetName.Mid(DigitStartIndex));
		if (Prefix.IsEmpty() || LabelIndex <= 1)
		{
			return FString();
		}

		return FString::Printf(TEXT("%s_%d"), *Prefix, LabelIndex - 1);
	}

	bool DoesActorMatchTargetName(const AActor* Actor, const FName TargetActorName, const FString& TargetName)
	{
		if (!IsValid(Actor))
		{
			return false;
		}

		const FString ActorName = Actor->GetName();
		if (Actor->GetFName() == TargetActorName || ActorName == TargetName)
		{
			return true;
		}

		if (!TargetName.IsEmpty())
		{
			const FString NamePrefix = FString::Printf(TEXT("%s_"), *TargetName);
			if (ActorName.StartsWith(NamePrefix))
			{
				return true;
			}

			const FString RuntimeNameFromEditorLabel = BuildRuntimeActorNameFromEditorLabel(TargetName);
			if (!RuntimeNameFromEditorLabel.IsEmpty())
			{
				const FString RuntimeNamePrefix = FString::Printf(TEXT("%s_"), *RuntimeNameFromEditorLabel);
				if (ActorName == RuntimeNameFromEditorLabel || ActorName.StartsWith(RuntimeNamePrefix))
				{
					return true;
				}
			}

			const FString ActorPathName = Actor->GetPathName();
			const FString ExactPathSuffix = FString::Printf(TEXT(".%s"), *TargetName);
			const FString PrefixedPathSegment = FString::Printf(TEXT(".%s_"), *TargetName);
			if (ActorPathName.EndsWith(ExactPathSuffix) || ActorPathName.Contains(PrefixedPathSegment))
			{
				return true;
			}

			if (!RuntimeNameFromEditorLabel.IsEmpty())
			{
				const FString RuntimeExactPathSuffix = FString::Printf(TEXT(".%s"), *RuntimeNameFromEditorLabel);
				const FString RuntimePrefixedPathSegment = FString::Printf(TEXT(".%s_"), *RuntimeNameFromEditorLabel);
				if (ActorPathName.EndsWith(RuntimeExactPathSuffix) || ActorPathName.Contains(RuntimePrefixedPathSegment))
				{
					return true;
				}
			}
		}

#if WITH_EDITOR
		if (Actor->GetActorLabel() == TargetName)
		{
			return true;
		}
#endif

		return false;
	}
}

UInteractionCameraBlendComponent::UInteractionCameraBlendComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UInteractionCameraBlendComponent::BlendToInteractionCamera(APlayerController* RequestingController)
{
	APlayerController* Controller = ResolveController(RequestingController);
	AActor* ViewTarget = ResolveTargetViewActor();

	if (!Controller || !ViewTarget)
	{
		UE_LOG(LogInteractionCameraBlend, Warning, TEXT("BlendToInteractionCamera failed. Owner=%s Controller=%s TargetViewActor=%s TargetActorTag=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Controller),
			*GetNameSafe(TargetViewActor),
			*TargetActorTag.ToString());
		return false;
	}

	AActor* CurrentViewTarget = Controller->GetViewTarget();
	if (CurrentViewTarget && CurrentViewTarget != ViewTarget)
	{
		CachedPreviousViewTarget = CurrentViewTarget;
	}

	CachedController = Controller;
	Controller->SetViewTargetWithBlend(ViewTarget, BlendTime, BlendFunction, BlendExp, bLockOutgoing);
	UE_LOG(LogInteractionCameraBlend, Log, TEXT("BlendToInteractionCamera succeeded. Owner=%s ViewTarget=%s Controller=%s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(ViewTarget),
		*GetNameSafe(Controller));
	return true;
}

bool UInteractionCameraBlendComponent::BlendBackToPlayer(APlayerController* RequestingController)
{
	APlayerController* Controller = ResolveController(RequestingController);
	if (!Controller)
	{
		Controller = CachedController.Get();
	}

	if (!Controller)
	{
		UE_LOG(LogInteractionCameraBlend, Warning, TEXT("BlendBackToPlayer failed: controller missing. Owner=%s CachedController=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(CachedController.Get()));
		return false;
	}

	AActor* ReturnTarget = nullptr;
	if (bRestorePreviousViewTarget && CachedPreviousViewTarget.IsValid())
	{
		ReturnTarget = CachedPreviousViewTarget.Get();
	}

	if (!ReturnTarget)
	{
		UE_LOG(LogInteractionCameraBlend, Warning, TEXT("BlendBackToPlayer failed: previous view target missing. Owner=%s Controller=%s bRestorePreviousViewTarget=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Controller),
			bRestorePreviousViewTarget ? TEXT("true") : TEXT("false"));
		return false;
	}

	Controller->SetViewTargetWithBlend(ReturnTarget, BlendTime, BlendFunction, BlendExp, bLockOutgoing);
	UE_LOG(LogInteractionCameraBlend, Log, TEXT("BlendBackToPlayer succeeded. Owner=%s ReturnTarget=%s Controller=%s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(ReturnTarget),
		*GetNameSafe(Controller));
	return true;
}

AActor* UInteractionCameraBlendComponent::GetResolvedTargetViewActor() const
{
	return ResolveTargetViewActor();
}

void UInteractionCameraBlendComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bAutoBlendBackOnEndPlay && CachedController.IsValid())
	{
		BlendBackToPlayer(CachedController.Get());
	}

	Super::EndPlay(EndPlayReason);
}

APlayerController* UInteractionCameraBlendComponent::ResolveController(APlayerController* RequestingController) const
{
	if (RequestingController)
	{
		return RequestingController;
	}

	UWorld* World = GetWorld();
	return World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
}

AActor* UInteractionCameraBlendComponent::ResolveTargetViewActor() const
{
	if (IsValid(TargetViewActor))
	{
		return TargetViewActor;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FName EffectiveTargetActorTag = TargetActorTag;
	if (EffectiveTargetActorTag.IsNone() && IsDiningTableOwner(GetOwner()))
	{
		EffectiveTargetActorTag = DiningTableFallbackCameraActorName;
	}

	if (EffectiveTargetActorTag.IsNone())
	{
		return nullptr;
	}

	TArray<AActor*> TaggedActors;
	UGameplayStatics::GetAllActorsWithTag(World, EffectiveTargetActorTag, TaggedActors);
	if (TaggedActors.Num() > 0)
	{
		return TaggedActors[0];
	}

	const FString TargetName = EffectiveTargetActorTag.ToString();
	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
	for (AActor* Actor : AllActors)
	{
		if (DoesActorMatchTargetName(Actor, EffectiveTargetActorTag, TargetName))
		{
			return Actor;
		}
	}

	return nullptr;
}
