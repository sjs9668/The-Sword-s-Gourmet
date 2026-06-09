#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "ProximityLineBoxComponent.generated.h"

class APawn;
class USceneComponent;

UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class MYPROJECT2_API UProximityLineBoxComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProximityLineBoxComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// Refreshes the cached component used for box bounds.
	UFUNCTION(BlueprintCallable, Category = "Interaction|LineBox")
	void RefreshSourceComponent();

	// Forces the line box on or off regardless of player distance.
	UFUNCTION(BlueprintCallable, Category = "Interaction|LineBox")
	void SetForceVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "Interaction|LineBox")
	bool IsVisible() const { return bIsVisible; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Proximity", meta = (ClampMin = "0.0"))
	float HighlightRadius = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Proximity", meta = (ClampMin = "0.01"))
	float CheckInterval = 0.02f;

	// Optional component to use as the distance origin instead of the actor location.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Proximity")
	FComponentReference DistanceCheckOrigin;

	// Optional component whose transform and bounds should drive the box line shape.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|LineBox")
	FComponentReference BoundsSourceComponent;

	// If true, tries to read the source component's bounds automatically.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|LineBox")
	bool bUseSourceComponentBounds = true;

	// Extra local offset from the chosen source component center.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|LineBox")
	FVector LocalBoxOffset = FVector::ZeroVector;

	// Used when no valid source bounds can be resolved.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|LineBox")
	FVector ManualBoxExtent = FVector(50.0f, 50.0f, 50.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|LineBox")
	FLinearColor LineColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|LineBox", meta = (ClampMin = "0.1"))
	float LineThickness = 3.0f;

	// Keep lifetime slightly above the tick interval so the box looks continuous.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|LineBox", meta = (ClampMin = "0.02"))
	float LineLifetime = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|LineBox")
	TEnumAsByte<ESceneDepthPriorityGroup> DepthPriority = SDPG_World;

private:
	TWeakObjectPtr<USceneComponent> CachedSourceComponent;
	bool bForceVisible = false;
	bool bIsVisible = false;

	void UpdateVisibilityForLocalPlayer();
	bool ShouldDraw(APawn* PlayerPawn) const;
	FVector GetDistanceOrigin() const;
	bool ResolveBoxData(FTransform& OutTransform, FVector& OutExtent) const;
	void DrawLineBox(const FTransform& BoxTransform, const FVector& BoxExtent) const;
};
