#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "KitchenCraftTypes.h"
#include "TimerManager.h"
#include "KitchenIngredientActor.generated.h"

class UStaticMeshComponent;
class UPrimitiveComponent;
class UKitchenCraftStationComponent;
class AKitchenIngredientActor;

UCLASS()
class MYPROJECT2_API UKitchenClickCollisionComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKitchenIngredientSimpleEvent, AKitchenIngredientActor*, IngredientActor);

UCLASS(Blueprintable, BlueprintType)
class MYPROJECT2_API AKitchenIngredientActor : public AActor
{
	GENERATED_BODY()

public:
	AKitchenIngredientActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void NotifyActorOnClicked(FKey ButtonPressed = EKeys::LeftMouseButton) override;

	UFUNCTION(BlueprintCallable, Category = "Kitchen|Ingredient")
	void InitializeDroppedIngredient(UKitchenCraftStationComponent* InOwnerStation, const FKitchenItemStack& InIngredientStack, UTexture2D* InIconTexture, UStaticMesh* InIngredientMesh);

	UFUNCTION(BlueprintCallable, Category = "Kitchen|Result")
	void InitializeCraftResult(UKitchenCraftStationComponent* InOwnerStation, const FKitchenCraftedItem& InCraftedItem, UTexture2D* InIconTexture, UStaticMesh* InIngredientMesh);

	UFUNCTION(BlueprintCallable, Category = "Kitchen|Ingredient")
	void StartDropAnimation(FVector InStartLocation, FVector InTargetLocation, float InDuration, float InArcHeight);

	UFUNCTION(BlueprintCallable, Category = "Kitchen|Ingredient")
	void StartPhysicsDrop(FVector InStartLocation, float DropLinearDamping = 0.05f, float DropAngularDamping = 6.0f, float HorizontalImpulseScale = 0.0f, float AngularImpulseScale = 0.0f);

	UFUNCTION(BlueprintCallable, Category = "Kitchen|Ingredient")
	void StartLandingPhysics();

	UFUNCTION(BlueprintCallable, Category = "Kitchen|Physics")
	void SettlePhysicsNow();

	UFUNCTION(BlueprintCallable, Category = "Kitchen|Ingredient")
	void ApplyIconTextureToMeshMaterial();

	UFUNCTION(BlueprintCallable, Category = "Kitchen|Ingredient")
	void ReturnToInventory();

	UFUNCTION(BlueprintPure, Category = "Kitchen|Ingredient")
	bool IsDropAnimationActive() const { return bIsDropping; }

	UFUNCTION(BlueprintPure, Category = "Kitchen|Ingredient")
	const FKitchenItemStack& GetIngredientStack() const { return IngredientStack; }

	UFUNCTION(BlueprintPure, Category = "Kitchen|Result")
	const FKitchenCraftedItem& GetCraftedItem() const { return CraftedItem; }

	UFUNCTION(BlueprintPure, Category = "Kitchen|Result")
	bool IsCraftResultActor() const { return bIsCraftResult; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kitchen|Ingredient")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kitchen|Ingredient")
	TObjectPtr<UStaticMeshComponent> MeshComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kitchen|Ingredient")
	TObjectPtr<UKitchenClickCollisionComponent> ClickCollisionComponent = nullptr;

	UPROPERTY(BlueprintAssignable, Category = "Kitchen|Ingredient")
	FKitchenIngredientSimpleEvent OnDropLanded;

	UPROPERTY(BlueprintAssignable, Category = "Kitchen|Ingredient")
	FKitchenIngredientSimpleEvent OnReturnedToInventory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Visual")
	bool bApplyIconTextureToMaterial = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Visual")
	FName IconTextureParameterName = TEXT("IconTexture");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Click")
	bool bUseClickCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Click", meta = (ClampMin = "0.1", UIMin = "1.0", UIMax = "120.0"))
	FVector ClickCollisionExtent = FVector(42.0f, 42.0f, 12.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Physics")
	bool bEnablePhysicsAfterLanding = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Physics", meta = (ClampMin = "0.0"))
	float LandingHorizontalImpulse = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Physics", meta = (ClampMin = "0.0"))
	float LandingUpImpulse = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Physics", meta = (ClampMin = "0.0"))
	float LandingDropVelocityScale = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Physics", meta = (ClampMin = "0.0"))
	float LandingBounceFromFallScale = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Physics", meta = (ClampMin = "0.0"))
	float LandingAngularImpulse = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Physics", meta = (ClampMin = "0.0"))
	float PhysicsLinearDamping = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Physics", meta = (ClampMin = "0.0"))
	float PhysicsAngularDamping = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Physics")
	bool bAutoSettlePhysicsAfterDrop = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Physics", meta = (ClampMin = "0.0"))
	float PhysicsSettleDelay = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Physics", meta = (ClampMin = "0.0"))
	float SettledPhysicsLinearDamping = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Physics", meta = (ClampMin = "0.0"))
	float SettledPhysicsAngularDamping = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Physics")
	bool bFreezePhysicsAfterSettle = true;

protected:
	UFUNCTION()
	void HandleMeshClicked(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed);

	UFUNCTION(BlueprintImplementableEvent, Category = "Kitchen|Ingredient")
	void OnIngredientVisualDataApplied(UTexture2D* InIconTexture, UStaticMesh* InIngredientMesh);

	UPROPERTY(BlueprintReadOnly, Category = "Kitchen|Ingredient")
	FKitchenItemStack IngredientStack;

	UPROPERTY(BlueprintReadOnly, Category = "Kitchen|Result")
	FKitchenCraftedItem CraftedItem;

	UPROPERTY(BlueprintReadOnly, Category = "Kitchen|Ingredient")
	TObjectPtr<UTexture2D> IconTexture = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Kitchen|Ingredient")
	TObjectPtr<UStaticMesh> IngredientMesh = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Kitchen|Ingredient")
	TObjectPtr<UKitchenCraftStationComponent> OwnerStation = nullptr;

private:
	UPrimitiveComponent* ConfigurePhysicsDropBody(float LinearDamping, float AngularDamping);
	void ConfigureMeshForClickOnly();
	void ConfigureMeshAsPhysicsBody(float LinearDamping, float AngularDamping);
	FVector GetSafeClickCollisionExtent() const;
	void ConfigureClickCollision();
	void SchedulePhysicsSettle();
	void SettlePhysicsBody();
	void SetMeshPhysicsLocked(bool bLocked);

	bool bIsCraftResult = false;
	bool bIsDropping = false;
	float DropElapsed = 0.0f;
	float DropDuration = 0.25f;
	float DropArcHeight = 60.0f;
	FVector DropStartLocation = FVector::ZeroVector;
	FVector DropTargetLocation = FVector::ZeroVector;
	FVector PendingLandingVelocity = FVector::ZeroVector;
	FTimerHandle PhysicsSettleTimerHandle;
};
