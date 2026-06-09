#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "InteractionCameraBlendComponent.generated.h"

class APlayerController;

UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class MYPROJECT2_API UInteractionCameraBlendComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractionCameraBlendComponent();

	UFUNCTION(BlueprintCallable, Category = "Interaction|Camera")
	bool BlendToInteractionCamera(APlayerController* RequestingController = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Interaction|Camera")
	bool BlendBackToPlayer(APlayerController* RequestingController = nullptr);

	UFUNCTION(BlueprintPure, Category = "Interaction|Camera")
	AActor* GetResolvedTargetViewActor() const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Interaction|Camera")
	AActor* TargetViewActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Camera")
	FName TargetActorTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Camera", meta = (ClampMin = "0.0"))
	float BlendTime = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Camera")
	TEnumAsByte<EViewTargetBlendFunction> BlendFunction = VTBlend_Cubic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Camera", meta = (ClampMin = "0.0"))
	float BlendExp = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Camera")
	bool bLockOutgoing = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Camera")
	bool bRestorePreviousViewTarget = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction|Camera")
	bool bAutoBlendBackOnEndPlay = false;

private:
	APlayerController* ResolveController(APlayerController* RequestingController) const;
	AActor* ResolveTargetViewActor() const;

	TWeakObjectPtr<APlayerController> CachedController;
	TWeakObjectPtr<AActor> CachedPreviousViewTarget;
};
