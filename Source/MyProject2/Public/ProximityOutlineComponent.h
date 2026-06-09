#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "ProximityOutlineComponent.generated.h"

class APawn;
class UPrimitiveComponent;

UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class MYPROJECT2_API UProximityOutlineComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProximityOutlineComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// Rebuilds the list of meshes that should receive the outline.
	UFUNCTION(BlueprintCallable, Category = "Interaction|Outline")
	void RefreshOutlineTargets();

	// Forces the outline on/off regardless of distance. Useful for debugging.
	UFUNCTION(BlueprintCallable, Category = "Interaction|Outline")
	void SetForceHighlighted(bool bHighlighted);

	UFUNCTION(BlueprintCallable, Category = "Interaction|Outline")
	void SetHighlightSuppressed(bool bSuppressed);

	UFUNCTION(BlueprintPure, Category = "Interaction|Outline")
	bool IsHighlighted() const { return bIsHighlighted; }

	UFUNCTION(BlueprintPure, Category = "Interaction|Outline")
	bool IsHighlightSuppressed() const { return bHighlightSuppressed; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Proximity", meta = (ClampMin = "0.0"))
	float HighlightRadius = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Proximity", meta = (ClampMin = "0.02"))
	float CheckInterval = 0.1f;

	// Optional component to use as the distance origin instead of the actor location.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Proximity")
	FComponentReference DistanceCheckOrigin;

	// Usually false when the owner is an invisible trigger actor and you want to highlight another mesh.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Outline")
	bool bIncludeOwnerPrimitiveComponents = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Outline")
	bool bIgnoreHiddenInGamePrimitives = true;

	// Explicit component references to highlight.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Outline")
	TArray<FComponentReference> OutlineComponentReferences;

	// Additional actors whose primitive components should be highlighted.
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Interaction|Outline")
	TArray<TObjectPtr<AActor>> AdditionalActorsToOutline;

private:
	struct FOutlineTargetState
	{
		TWeakObjectPtr<UPrimitiveComponent> Primitive;
		bool bOriginalRenderCustomDepth = false;
		int32 OriginalStencilValue = 0;
	};

	TArray<FOutlineTargetState> OutlineTargetStates;
	bool bForceHighlighted = false;
	bool bHighlightSuppressed = false;
	bool bIsHighlighted = false;

	void UpdateHighlightForLocalPlayer();
	bool ShouldHighlight(APawn* PlayerPawn) const;
	void SetHighlightEnabled(bool bEnabled);
	FVector GetDistanceOrigin() const;
	void AddTargetPrimitive(UPrimitiveComponent* Primitive);
	void GatherPrimitivesFromActor(AActor* Actor);
};
