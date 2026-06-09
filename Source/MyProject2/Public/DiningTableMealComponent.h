#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "KitchenCraftTypes.h"
#include "DiningTableMealComponent.generated.h"

class AKitchenIngredientActor;
class APlayerController;
class UButton;
class UDataTable;
class UPrimitiveComponent;
class USceneComponent;
class UScrollBox;
class USizeBox;
class UUserWidget;
class UWidgetComponent;
class UDiningTableMealComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDiningMealFoodSelectedEvent, FName, ItemId, FKitchenItemRow, ItemData);

UCLASS()
class MYPROJECT2_API UDiningMealEntryClickHandler : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(class UDiningTableMealComponent* InMealComponent, const FKitchenIngredientOption& InOption);

	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandlePressed();

	UFUNCTION()
	void HandleHovered();

	UFUNCTION()
	void HandleUnhovered();

private:
	void Activate(const TCHAR* SourceEventName);

	UPROPERTY(Transient)
	TObjectPtr<UDiningTableMealComponent> MealComponent = nullptr;

	UPROPERTY(Transient)
	FKitchenIngredientOption Option;
};

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Dining), meta=(BlueprintSpawnableComponent, DisplayName="Dining Table Meal"))
class MYPROJECT2_API UDiningTableMealComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDiningTableMealComponent();

	UFUNCTION(BlueprintCallable, Category = "Dining|Meal", meta = (ExpandBoolAsExecs = "ReturnValue", AdvancedDisplay = "RequestingController,InventoryProvider"))
	bool OpenMealPicker(APlayerController* RequestingController = nullptr, UObject* InventoryProvider = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Dining|Meal")
	void CloseMealPicker(APlayerController* RequestingController = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Dining|Meal", meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool RefreshMealPicker();

	UFUNCTION(BlueprintCallable, Category = "Dining|Meal", meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool SelectMealFood(FName ItemId);

	bool SelectMealFoodOption(const FKitchenIngredientOption& Option);

	void ShowEntryTooltip(const FText& TooltipText);
	void HideEntryTooltip();

	UFUNCTION(BlueprintCallable, Category = "Dining|Meal")
	void ClearSelectedMealFood();

	UFUNCTION(BlueprintCallable, Category = "Dining|Meal")
	void SetMealPickerVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "Dining|Meal")
	bool IsMealPickerOpen() const { return bPickerOpen; }

	UFUNCTION(BlueprintPure, Category = "Dining|Meal")
	FName GetSelectedFoodItemId() const { return SelectedFoodItemId; }

	UPROPERTY(BlueprintAssignable, Category = "Dining|Meal")
	FDiningMealFoodSelectedEvent OnMealFoodSelected;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|Data")
	TObjectPtr<UDataTable> ItemDataTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|References")
	TObjectPtr<UWidgetComponent> MealPickerComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|References")
	FName MealPickerComponentName = TEXT("WC_MealPicker");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|Food Preview")
	TSubclassOf<AKitchenIngredientActor> DefaultFoodActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|Food Preview")
	TObjectPtr<USceneComponent> FoodPlacementComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|Food Preview")
	FName FoodPlacementComponentName = TEXT("FoodPlacement");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|Food Preview")
	FVector FoodScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|Food Preview")
	bool bUseFoodPreviewClickBox = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|Food Preview", meta = (EditCondition = "bUseFoodPreviewClickBox", ClampMin = "0.1", UIMin = "1.0", UIMax = "200.0"))
	FVector FoodPreviewClickBoxExtent = FVector(55.0f, 55.0f, 12.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|Food Preview", meta = (EditCondition = "bUseFoodPreviewClickBox"))
	FVector FoodPreviewClickBoxOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|Food Preview")
	bool bAttachPreviewToTable = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|Food Preview")
	bool bClearPreviewOnClose = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|Clatter")
	bool bClatterOnSelect = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|Clatter", meta = (ClampMin = "0.01"))
	float ClatterDuration = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|Clatter", meta = (ClampMin = "0.0"))
	float ClatterLocationAmplitude = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|Clatter", meta = (ClampMin = "0.0"))
	float ClatterRotationAmplitude = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|Clatter", meta = (ClampMin = "0.1"))
	float ClatterFrequency = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|Eat")
	bool bConsumeFoodOnPreviewClick = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|Eat")
	FName ApplyFoodStatBonusFunctionName = TEXT("Apply Food Stat Bonus");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|Eat")
	bool bClatterOnConsumeClick = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|Eat")
	bool bClearPreviewAfterConsume = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|Eat")
	bool bRefreshPickerAfterConsume = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI")
	TSubclassOf<UUserWidget> IngredientEntryWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI")
	FName GridWidgetName = TEXT("WB_IngredientGrid");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI")
	FName IngredientScrollBoxName = TEXT("SB_Ingredients");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI")
	FName PanelBackgroundName = TEXT("Panel_Background");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI")
	FName CloseButtonName = TEXT("Btn_Close");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI")
	FName EntrySelectButtonName = TEXT("Btn_Select");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI")
	FName EntryIconImageName = TEXT("Img_Icon");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI")
	FName EntryCountTextName = TEXT("Txt_Count");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI")
	FMargin EntryPadding = FMargin(1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI")
	FVector2D EntryWidgetSize = FVector2D(78.0f, 78.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI")
	FVector2D EntryIconSize = FVector2D(62.0f, 68.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI", meta = (ClampMin = "6", ClampMax = "48"))
	int32 EntryCountFontSize = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI")
	FMargin EntryCountPadding = FMargin(0.0f, 0.0f, 4.0f, 3.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI")
	bool bShowIngredientScrollBar = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI", meta = (ClampMin = "0.1"))
	float IngredientWheelScrollMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI")
	bool bForcePickerDrawSize = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI", meta = (EditCondition = "bForcePickerDrawSize"))
	FVector2D PickerDrawSize = FVector2D(92.0f, 238.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI", meta = (EditCondition = "bForcePickerDrawSize", ClampMin = "1.0", ClampMax = "6.0"))
	float PickerRenderScale = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI")
	bool bForcePickerLayout = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI", meta = (EditCondition = "bForcePickerLayout"))
	FVector2D PickerPanelPosition = FVector2D(0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI", meta = (EditCondition = "bForcePickerLayout"))
	FVector2D PickerPanelSize = FVector2D(92.0f, 214.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI", meta = (EditCondition = "bForcePickerLayout"))
	FMargin IngredientScrollBoxPadding = FMargin(4.0f, 4.0f, 4.0f, 4.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI", meta = (EditCondition = "bForcePickerLayout"))
	FVector2D CloseButtonPosition = FVector2D(6.0f, 218.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI", meta = (EditCondition = "bForcePickerLayout"))
	FVector2D CloseButtonSize = FVector2D(80.0f, 16.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI")
	FText CloseButtonText = FText::GetEmpty();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI|Tooltip")
	bool bUseFixedEntryTooltip = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI|Tooltip", meta = (EditCondition = "bUseFixedEntryTooltip"))
	FVector2D FixedEntryTooltipPosition = FVector2D(2.0f, 42.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI|Tooltip", meta = (EditCondition = "bUseFixedEntryTooltip"))
	FVector2D FixedEntryTooltipSize = FVector2D(124.0f, 70.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|UI|Tooltip", meta = (EditCondition = "bUseFixedEntryTooltip", ClampMin = "6", ClampMax = "24"))
	int32 FixedEntryTooltipFontSize = 9;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|Session")
	bool bHidePickerOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|Session")
	bool bSetGameAndUIInputOnOpen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|Session")
	bool bSetGameOnlyInputOnClose = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|Session")
	bool bEnableWorldClickEventsForSession = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|Camera")
	bool bCallOwnerReturnCameraOnClose = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dining|Camera")
	FName OwnerReturnCameraFunctionName = TEXT("ReturnToPlayerCamera");

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleClosePressed();

	UFUNCTION()
	void HandleSelectedFoodClicked(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed);

	void HandleCloseButtonActivated(const TCHAR* SourceEventName);
	bool ResolveReferences();
	void ResolvePickerComponent(AActor* Owner);
	void ResolveFoodPlacementComponent(AActor* Owner);
	APlayerController* ResolveController(APlayerController* RequestingController) const;
	UObject* ResolveInventoryProvider(APlayerController* RequestingController, UObject* ProvidedInventoryProvider) const;
	void ConfigurePickerComponent();
	float GetPickerRenderScale() const;
	void ApplyPickerComponentRenderScale();
	void ConfigurePickerRootRenderScale(UUserWidget* PickerWidget) const;
	UUserWidget* GetPickerWidget() const;
	bool BindCloseButton();
	void ConfigurePickerLayout();
	UScrollBox* ConfigureIngredientScrollBox();
	void ConfigureCloseButtonVisuals();
	USizeBox* CreateSizedEntryWidget(UUserWidget* PickerWidget, UUserWidget* EntryWidget);
	bool ConfigureEntryWidget(UUserWidget* EntryWidget, const FKitchenIngredientOption& Option);
	bool ResolveFoodData(FName ItemId, FKitchenIngredientOption& OutOption) const;
	bool SpawnSelectedFoodPreview(const FKitchenIngredientOption& Option);
	FTransform GetFoodPlacementTransform() const;
	void ConfigureSelectedFoodClickBox(AKitchenIngredientActor* FoodActor);
	void StartClatterMotion(bool bInConsumeAfterClatter = false);
	void StopClatterMotion(bool bSnapToBase);
	bool ConsumeSelectedMealFood();
	bool TryConsumeMealThroughInventory(UObject* InventoryProvider, const FKitchenIngredientOption& Option, APlayerController* RequestingController) const;
	bool TryApplyFoodStatBonus(APlayerController* RequestingController, const FKitchenStatBonus& StatBonus) const;
	bool TryCallOwnerReturnCameraFunction(APlayerController* RequestingController = nullptr);
	void EnableWorldClickEventsForSession(APlayerController* RequestingController);
	void RestoreWorldClickEvents();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> EntryClickHandlers;

	UPROPERTY()
	bool bShowDebugMessages = false;

	UPROPERTY(Transient)
	TObjectPtr<UObject> ActiveInventoryProvider = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<APlayerController> ActiveController = nullptr;

	TMap<TWeakObjectPtr<UWidgetComponent>, FVector> OriginalPickerComponentScales;

	UPROPERTY(Transient)
	TObjectPtr<AKitchenIngredientActor> SelectedFoodActor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> SelectedFoodClickBoxComponent = nullptr;

	UPROPERTY(Transient)
	FKitchenIngredientOption SelectedFoodOption;

	FName SelectedFoodItemId = NAME_None;
	FTransform ClatterBaseTransform = FTransform::Identity;
	float ClatterElapsed = 0.0f;
	bool bPickerOpen = false;
	bool bClatterActive = false;
	bool bConsumeAfterClatter = false;
	bool bHasCachedControllerClickSettings = false;
	bool bCachedControllerClickEvents = false;
	bool bCachedControllerMouseOverEvents = false;
};
