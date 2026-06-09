#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "KitchenCraftTypes.h"
#include "KitchenCraftWidgetHostComponent.generated.h"

class APlayerController;
class AActor;
class UButton;
class UKitchenCraftStationComponent;
class UKitchenCraftWidgetHostComponent;
class UScrollBox;
class USizeBox;
class UUserWidget;
class UWidgetComponent;

UCLASS()
class MYPROJECT2_API UKitchenIngredientEntryClickHandler : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UKitchenCraftStationComponent* InStation, UKitchenCraftWidgetHostComponent* InHost, const FKitchenIngredientOption& InOption);

	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandleHovered();

	UFUNCTION()
	void HandleUnhovered();

private:
	UPROPERTY(Transient)
	TObjectPtr<UKitchenCraftStationComponent> Station = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UKitchenCraftWidgetHostComponent> Host = nullptr;

	UPROPERTY(Transient)
	FKitchenIngredientOption Option;
};

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Kitchen), meta=(BlueprintSpawnableComponent, DisplayName="Kitchen Craft Widget Host"))
class MYPROJECT2_API UKitchenCraftWidgetHostComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKitchenCraftWidgetHostComponent();

	UFUNCTION(BlueprintCallable, Category = "Kitchen|UI", meta = (ExpandBoolAsExecs = "ReturnValue", AdvancedDisplay = "RequestingController,InventoryProvider"))
	bool OpenKitchenPicker(APlayerController* RequestingController = nullptr, UObject* InventoryProvider = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Kitchen|UI", meta = (ExpandBoolAsExecs = "ReturnValue", AdvancedDisplay = "RequestingController,InventoryProvider"))
	bool OpenKitchenPickerForMode(EKitchenCraftMode Mode, APlayerController* RequestingController = nullptr, UObject* InventoryProvider = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Kitchen|UI")
	void CloseKitchenPicker(APlayerController* RequestingController = nullptr, bool bReturnReservedIngredients = true);

	UFUNCTION(BlueprintCallable, Category = "Kitchen|UI", meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool RefreshKitchenPicker();

	UFUNCTION(BlueprintCallable, Category = "Kitchen|UI")
	void SetPickerVisible(bool bVisible);

	void ShowEntryTooltip(const FText& TooltipText);
	void HideEntryTooltip();

	UFUNCTION(BlueprintPure, Category = "Kitchen|UI")
	bool IsKitchenPickerOpen() const { return bPickerOpen; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|References")
	TObjectPtr<UKitchenCraftStationComponent> KitchenCraftStation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|References")
	TObjectPtr<UWidgetComponent> IngredientPickerComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|References")
	FName IngredientPickerComponentName = TEXT("WC_IngredientPicker");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|References")
	TObjectPtr<UWidgetComponent> PanIngredientPickerComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|References")
	FName PanIngredientPickerComponentName = TEXT("WC_IngredientPicker");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|References")
	TObjectPtr<UWidgetComponent> IntermediateIngredientPickerComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|References")
	FName IntermediateIngredientPickerComponentName = TEXT("WC_IntermediateIngredientPicker");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI")
	TSubclassOf<UUserWidget> IngredientEntryWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI")
	FName GridWidgetName = TEXT("WB_IngredientGrid");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI")
	FName IngredientScrollBoxName = TEXT("SB_Ingredients");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI")
	FName PanelBackgroundName = TEXT("Panel_Background");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI")
	FName CloseButtonName = TEXT("Btn_Close");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI")
	FName EntrySelectButtonName = TEXT("Btn_Select");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI")
	FName EntryIconImageName = TEXT("Img_Icon");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI")
	FName EntryCountTextName = TEXT("Txt_Count");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI")
	FMargin EntryPadding = FMargin(1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI")
	FVector2D EntryWidgetSize = FVector2D(78.0f, 78.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI")
	FVector2D EntryIconSize = FVector2D(62.0f, 68.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI", meta = (ClampMin = "6", ClampMax = "48"))
	int32 EntryCountFontSize = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI")
	FMargin EntryCountPadding = FMargin(0.0f, 0.0f, 4.0f, 3.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI")
	bool bShowIngredientScrollBar = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI", meta = (ClampMin = "0.1"))
	float IngredientWheelScrollMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI")
	bool bForcePickerDrawSize = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI", meta = (EditCondition = "bForcePickerDrawSize"))
	FVector2D PickerDrawSize = FVector2D(92.0f, 238.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI", meta = (EditCondition = "bForcePickerDrawSize", ClampMin = "1.0", ClampMax = "6.0"))
	float PickerRenderScale = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI")
	bool bForcePickerLayout = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI", meta = (EditCondition = "bForcePickerLayout"))
	FVector2D PickerPanelPosition = FVector2D(0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI", meta = (EditCondition = "bForcePickerLayout"))
	FVector2D PickerPanelSize = FVector2D(92.0f, 214.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI", meta = (EditCondition = "bForcePickerLayout"))
	FMargin IngredientScrollBoxPadding = FMargin(4.0f, 4.0f, 4.0f, 4.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI", meta = (EditCondition = "bForcePickerLayout"))
	FVector2D CloseButtonPosition = FVector2D(6.0f, 218.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI", meta = (EditCondition = "bForcePickerLayout"))
	FVector2D CloseButtonSize = FVector2D(80.0f, 16.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI")
	bool bApplyPickerComponentOffset = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI", meta = (EditCondition = "bApplyPickerComponentOffset"))
	FVector PickerComponentOffset = FVector(0.0f, 3.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI")
	FText CloseButtonText = FText::GetEmpty();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI|Tooltip")
	bool bUseFixedEntryTooltip = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI|Tooltip", meta = (EditCondition = "bUseFixedEntryTooltip"))
	FVector2D FixedEntryTooltipPosition = FVector2D(14.0f, 50.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI|Tooltip", meta = (EditCondition = "bUseFixedEntryTooltip"))
	FVector2D FixedEntryTooltipSize = FVector2D(74.0f, 20.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|UI|Tooltip", meta = (EditCondition = "bUseFixedEntryTooltip", ClampMin = "6", ClampMax = "24"))
	int32 FixedEntryTooltipFontSize = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Session")
	EKitchenCraftMode OpenMode = EKitchenCraftMode::PanCook;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Session")
	bool bHidePickerOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Session")
	bool bSetGameAndUIInputOnOpen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Session")
	bool bSetGameOnlyInputOnClose = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Session")
	bool bRefreshOnSelectionChanged = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Session")
	bool bEnableIngredientActorClickEvents = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Session")
	bool bHidePlayerPawnOnOpen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Session")
	bool bPreventCloseWhileResultWaiting = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Camera")
	bool bUseKitchenStationCameraOnOpen = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Camera")
	bool bUseKitchenStationCameraOnClose = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Camera")
	FName OwnerReturnCameraFunctionName = TEXT("ReturnToPlayerCamera");

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleSelectionChanged();

	bool ResolveReferences();
	void ResolvePickerComponents(AActor* Owner);
	APlayerController* ResolveController(APlayerController* RequestingController) const;
	UObject* ResolveInventoryProvider(APlayerController* RequestingController, UObject* ProvidedInventoryProvider) const;
	UWidgetComponent* GetPickerComponentForMode(EKitchenCraftMode Mode) const;
	UWidgetComponent* GetActivePickerComponent() const;
	void ConfigurePickerComponent(UWidgetComponent* PickerComponent);
	float GetPickerRenderScale() const;
	void ApplyPickerComponentRenderScale(UWidgetComponent* PickerComponent);
	void ConfigurePickerRootRenderScale(UUserWidget* PickerWidget) const;
	UUserWidget* GetPickerWidget() const;
	bool BindCloseButton();
	void BindSelectionChanged();
	void UnbindSelectionChanged();
	void ConfigurePickerLayout();
	UScrollBox* ConfigureIngredientScrollBox();
	void ConfigureCloseButtonVisuals();
	USizeBox* CreateSizedEntryWidget(UUserWidget* PickerWidget, UUserWidget* EntryWidget);
	bool ConfigureEntryWidget(UUserWidget* EntryWidget, const FKitchenIngredientOption& Option);
	bool TryCallBlueprintInit(UUserWidget* PickerWidget, UObject* InventoryProvider);
	bool TryCallOwnerReturnCameraFunction();
	void EnableWorldClickEventsForSession(APlayerController* RequestingController);
	void RestoreWorldClickEvents();
	void HidePlayerPawnForSession(APlayerController* RequestingController);
	void RestorePlayerPawnVisibility();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> EntryClickHandlers;

	UPROPERTY(Transient)
	TObjectPtr<UObject> ActiveInventoryProvider = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<APlayerController> ActiveController = nullptr;

	TMap<TWeakObjectPtr<UWidgetComponent>, FVector> OriginalPickerComponentScales;

	bool bPickerOpen = false;
	bool bSelectionBound = false;
	bool bHasHiddenPlayerPawn = false;
	bool bCachedPlayerPawnHiddenState = false;
	bool bHasCachedControllerClickSettings = false;
	bool bCachedControllerClickEvents = false;
	bool bCachedControllerMouseOverEvents = false;
	TWeakObjectPtr<AActor> HiddenPlayerPawn;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidgetComponent>> OffsetAppliedPickerComponents;
};
