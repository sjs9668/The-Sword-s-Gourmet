#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "KitchenCraftTypes.h"
#include "KitchenCraftStationComponent.generated.h"

class AKitchenIngredientActor;
class APlayerController;
class UDataTable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FKitchenSelectionChangedEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKitchenIngredientActorChangedEvent, AKitchenIngredientActor*, IngredientActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKitchenCraftCompletedEvent, FKitchenCraftResult, Result);

UCLASS(Blueprintable, BlueprintType, ClassGroup=(Kitchen), meta=(BlueprintSpawnableComponent))
class MYPROJECT2_API UKitchenCraftStationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKitchenCraftStationComponent();

	UFUNCTION(BlueprintCallable, Category = "Kitchen|Session")
	bool BeginKitchenSession(UObject* InInventoryProvider, EKitchenCraftMode InMode, APlayerController* RequestingController = nullptr, bool bBlendCamera = true);

	UFUNCTION(BlueprintCallable, Category = "Kitchen|Session")
	void EndKitchenSession(bool bReturnReservedIngredients = true, APlayerController* RequestingController = nullptr, bool bBlendBack = true);

	UFUNCTION(BlueprintCallable, Category = "Kitchen|Mode")
	void SetCraftMode(EKitchenCraftMode NewMode, bool bReturnCurrentSelection = true, APlayerController* RequestingController = nullptr, bool bBlendCamera = true);

	UFUNCTION(BlueprintPure, Category = "Kitchen|Mode")
	EKitchenCraftMode GetCraftMode() const { return CurrentMode; }

	UFUNCTION(BlueprintCallable, Category = "Kitchen|Selection", meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool TrySelectIngredient(FName ItemId, int32 Count, AKitchenIngredientActor*& SpawnedActor);

	UFUNCTION(BlueprintCallable, Category = "Kitchen|Selection")
	bool ReturnDroppedIngredient(AKitchenIngredientActor* IngredientActor);

	UFUNCTION(BlueprintCallable, Category = "Kitchen|Selection")
	bool ReturnIngredientAtIndex(int32 IngredientIndex);

	UFUNCTION(BlueprintCallable, Category = "Kitchen|Selection")
	void ReturnAllIngredients();

	UFUNCTION(BlueprintCallable, Category = "Kitchen|Craft", meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool TryExecuteCurrentCraft(FKitchenCraftResult& Result);

	UFUNCTION(BlueprintCallable, Category = "Kitchen|Craft", meta = (ExpandBoolAsExecs = "ReturnValue", AdvancedDisplay = "RequestingController,bReturnCurrentSelection,bBlendCamera"))
	bool TryExecuteCraftForMode(EKitchenCraftMode Mode, FKitchenCraftResult& Result, APlayerController* RequestingController = nullptr, bool bReturnCurrentSelection = false, bool bBlendCamera = false);

	UFUNCTION(BlueprintCallable, Category = "Kitchen|Craft", meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool CollectCraftResult(AKitchenIngredientActor* ResultActor);

	UFUNCTION(BlueprintCallable, Category = "Kitchen|Result")
	void ApplyCraftResultVisualScale();

	UFUNCTION(BlueprintCallable, Category = "Kitchen|Craft", meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool FindMatchingRecipe(EKitchenCraftMode Mode, FKitchenRecipeRow& OutRecipe, FName& OutRecipeRowName) const;

	UFUNCTION(BlueprintCallable, Category = "Kitchen|Data", meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool GetItemData(FName ItemId, FKitchenItemRow& OutItemData) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Kitchen|Data", meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool ResolveKitchenItemData(FName ItemId, FKitchenItemRow& OutItemData) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Kitchen|Data", meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool ResolveKitchenRecipe(EKitchenCraftMode Mode, const TArray<FKitchenItemStack>& Ingredients, FKitchenRecipeRow& OutRecipe, FName& OutRecipeRowName) const;

	UFUNCTION(BlueprintPure, Category = "Kitchen|Selection")
	TArray<FKitchenItemStack> GetSelectedIngredients() const { return SelectedIngredients; }

	UFUNCTION(BlueprintPure, Category = "Kitchen|Selection")
	TArray<AKitchenIngredientActor*> GetSpawnedIngredientActors() const { return SpawnedIngredientActors; }

	UFUNCTION(BlueprintPure, Category = "Kitchen|Craft")
	AKitchenIngredientActor* GetPendingCraftResultActor() const { return PendingCraftResultActor; }

	UFUNCTION(BlueprintCallable, Category = "Kitchen|Camera", meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool BlendToCurrentModeView(APlayerController* RequestingController = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Kitchen|Camera", meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool BlendBackToPreviousView(APlayerController* RequestingController = nullptr);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Data")
	TObjectPtr<UDataTable> ItemDataTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Data")
	TObjectPtr<UDataTable> PanRecipeDataTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Data")
	TObjectPtr<UDataTable> IntermediateRecipeDataTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Spawn")
	TSubclassOf<AKitchenIngredientActor> DefaultIngredientActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Spawn")
	TObjectPtr<AActor> PanSpawnSource = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Spawn")
	TObjectPtr<AActor> PanDropTarget = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Spawn")
	FVector PanDropTargetOffset = FVector(0.0f, 0.0f, 8.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Spawn")
	TObjectPtr<AActor> IntermediateSpawnSource = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Spawn")
	TObjectPtr<AActor> IntermediateDropTarget = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Spawn")
	FVector IntermediateDropTargetOffset = FVector(0.0f, 0.0f, 8.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Spawn")
	bool bUseDropTargetLocalOffset = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Spawn", meta = (ClampMin = "0.0"))
	float FallbackSpawnHeight = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Spawn")
	bool bUsePhysicsIngredientDrop = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Spawn", meta = (EditCondition = "bUsePhysicsIngredientDrop", ClampMin = "0.0"))
	float PhysicsIngredientDropHeight = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Spawn", meta = (EditCondition = "bUsePhysicsIngredientDrop", ClampMin = "0.0"))
	float PhysicsIngredientDropLinearDamping = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Spawn", meta = (EditCondition = "bUsePhysicsIngredientDrop", ClampMin = "0.0"))
	float PhysicsIngredientDropAngularDamping = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Spawn", meta = (EditCondition = "bUsePhysicsIngredientDrop", ClampMin = "0.0"))
	float PhysicsIngredientDropHorizontalImpulseScale = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Spawn", meta = (EditCondition = "bUsePhysicsIngredientDrop", ClampMin = "0.0"))
	float PhysicsIngredientDropAngularImpulseScale = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Spawn", meta = (ClampMin = "0.01"))
	float DropDuration = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Spawn", meta = (ClampMin = "0.0"))
	float DropArcHeight = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Spawn", meta = (ClampMin = "0.0"))
	float PanDropScatterRadius = 28.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Spawn", meta = (ClampMin = "0.0"))
	float IntermediateDropScatterRadius = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Recipe")
	bool bRequireExactRecipeMatch = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Recipe")
	bool bCreateFailedPanDishOnRecipeMismatch = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Recipe")
	FName FailedPanDishItemId = TEXT("???");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Recipe", meta = (ClampMin = "1"))
	int32 FailedPanDishCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Recipe")
	FKitchenStatBonus FailedPanDishStatBonus;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Result")
	bool bSpawnCraftResultOnPan = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Result")
	bool bPreventCraftWhileResultWaiting = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Result")
	FVector CraftResultTargetOffset = FVector(0.0f, 0.0f, 3.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Result", meta = (ClampMin = "0.01", UIMin = "0.1", UIMax = "3.0"))
	float CraftResultVisualScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Inventory")
	bool bAllowSelectionWithoutInventoryProvider = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Inventory")
	bool bReturnIngredientsIfOutputAddFails = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Camera")
	TObjectPtr<AActor> PanViewActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Camera")
	TObjectPtr<AActor> IntermediateViewActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Camera", meta = (ClampMin = "0.0"))
	float CameraBlendTime = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Camera")
	TEnumAsByte<EViewTargetBlendFunction> CameraBlendFunction = VTBlend_Cubic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Camera", meta = (ClampMin = "0.0"))
	float CameraBlendExp = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Camera")
	bool bCameraLockOutgoing = false;

	UPROPERTY(BlueprintAssignable, Category = "Kitchen|Events")
	FKitchenSelectionChangedEvent OnSelectionChanged;

	UPROPERTY(BlueprintAssignable, Category = "Kitchen|Events")
	FKitchenIngredientActorChangedEvent OnIngredientDropped;

	UPROPERTY(BlueprintAssignable, Category = "Kitchen|Events")
	FKitchenIngredientActorChangedEvent OnIngredientReturned;

	UPROPERTY(BlueprintAssignable, Category = "Kitchen|Events")
	FKitchenCraftCompletedEvent OnCraftCompleted;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(BlueprintReadOnly, Category = "Kitchen|Mode")
	EKitchenCraftMode CurrentMode = EKitchenCraftMode::PanCook;

	UPROPERTY(BlueprintReadOnly, Category = "Kitchen|Inventory")
	TObjectPtr<UObject> ActiveInventoryProvider = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Kitchen|Selection")
	TArray<FKitchenItemStack> SelectedIngredients;

private:
	UDataTable* GetRecipeTableForMode(EKitchenCraftMode Mode) const;
	AActor* GetDropTargetForMode(EKitchenCraftMode Mode) const;
	AActor* GetSpawnSourceForMode(EKitchenCraftMode Mode) const;
	AActor* GetViewActorForMode(EKitchenCraftMode Mode) const;
	FVector GetDropTargetOffsetForMode(EKitchenCraftMode Mode) const;
	FVector GetRandomizedDropTargetLocation(EKitchenCraftMode Mode, const FVector& BaseLocation) const;
	APlayerController* ResolveController(APlayerController* RequestingController) const;

	bool ReserveIngredient(FName ItemId, int32 Count) const;
	void ReturnIngredientToProvider(const FKitchenItemStack& Ingredient) const;
	bool CommitReservedIngredients() const;
	bool AddCraftedItemToProvider(const FKitchenCraftedItem& Item, UObject* InventoryProviderOverride = nullptr) const;

	void AddIngredientStack(const FKitchenItemStack& Ingredient);
	bool RemoveIngredientStack(const FKitchenItemStack& Ingredient);
	static TArray<FKitchenItemStack> NormalizeStacks(const TArray<FKitchenItemStack>& Stacks);
	static bool DoStacksMatch(const TArray<FKitchenItemStack>& Selected, const TArray<FKitchenItemStack>& Required, bool bExactMatch);

	AKitchenIngredientActor* SpawnIngredientActor(const FKitchenItemStack& Ingredient, const FKitchenItemRow& ItemData);
	AKitchenIngredientActor* SpawnCraftResultActor(const FKitchenCraftedItem& CraftedItem);
	void DestroySpawnedIngredientActors();
	void DestroyPendingCraftResultActor();
	FKitchenCraftResult MakeFailureResult(const FText& Reason) const;

	UPROPERTY()
	TArray<AKitchenIngredientActor*> SpawnedIngredientActors;

	UPROPERTY()
	TObjectPtr<AKitchenIngredientActor> PendingCraftResultActor = nullptr;

	UPROPERTY()
	FKitchenCraftedItem PendingCraftResultItem;

	UPROPERTY()
	TObjectPtr<UObject> PendingCraftResultInventoryProvider = nullptr;

	FVector PendingCraftResultBaseMeshScale = FVector::OneVector;

	TWeakObjectPtr<APlayerController> CachedController;
	TWeakObjectPtr<AActor> CachedPreviousViewTarget;
};
