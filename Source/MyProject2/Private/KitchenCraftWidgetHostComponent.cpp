#include "KitchenCraftWidgetHostComponent.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetComponent.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "KitchenCraftBlueprintLibrary.h"
#include "KitchenCraftStationComponent.h"
#include "KitchenIngredientActor.h"
#include "KitchenInventoryProvider.h"
#include "Kismet/GameplayStatics.h"
#include "ProximityOutlineComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogKitchenCraftUI, Log, All);

namespace
{
template <typename WidgetType>
WidgetType* FindKitchenWidgetByName(UUserWidget* RootWidget, FName WidgetName)
{
	if (!RootWidget || !RootWidget->WidgetTree || WidgetName.IsNone())
	{
		return nullptr;
	}

	return Cast<WidgetType>(RootWidget->WidgetTree->FindWidget(WidgetName));
}

FText MakeKitchenEntryTooltipText(const FKitchenIngredientOption& Option)
{
	const FString DisplayName = Option.ItemData.DisplayName.ToString();
	return FText::FromString(DisplayName.IsEmpty() ? Option.ItemId.ToString() : DisplayName);
}

void SetKitchenOwnerOutlineSuppressed(UActorComponent* Component, bool bSuppressed)
{
	AActor* Owner = Component ? Component->GetOwner() : nullptr;
	if (!Owner)
	{
		return;
	}

	TInlineComponentArray<UProximityOutlineComponent*> OutlineComponents;
	Owner->GetComponents(OutlineComponents);
	for (UProximityOutlineComponent* OutlineComponent : OutlineComponents)
	{
		if (OutlineComponent)
		{
			OutlineComponent->SetHighlightSuppressed(bSuppressed);
		}
	}
}

FVector2D ScaleKitchenVector(const FVector2D& Value, float Scale)
{
	return Value * FMath::Max(1.0f, Scale);
}

FMargin ScaleKitchenMargin(const FMargin& Value, float Scale)
{
	const float SafeScale = FMath::Max(1.0f, Scale);
	return FMargin(
		Value.Left * SafeScale,
		Value.Top * SafeScale,
		Value.Right * SafeScale,
		Value.Bottom * SafeScale);
}

const FName KitchenFixedTooltipBorderName(TEXT("Native_KitchenFixedEntryTooltip"));
const FName KitchenFixedTooltipTextName(TEXT("Native_KitchenFixedEntryTooltipText"));

UBorder* FindOrCreateKitchenFixedTooltip(
	UUserWidget* OwnerWidget,
	const FVector2D& Position,
	const FVector2D& Size,
	int32 FontSize,
	float RenderScale)
{
	if (!OwnerWidget || !OwnerWidget->WidgetTree)
	{
		return nullptr;
	}

	UBorder* TooltipBorder = Cast<UBorder>(OwnerWidget->WidgetTree->FindWidget(KitchenFixedTooltipBorderName));
	UTextBlock* TooltipLabel = Cast<UTextBlock>(OwnerWidget->WidgetTree->FindWidget(KitchenFixedTooltipTextName));
	if (!TooltipBorder)
	{
		TooltipBorder = OwnerWidget->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), KitchenFixedTooltipBorderName);
	}
	if (!TooltipLabel)
	{
		TooltipLabel = OwnerWidget->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), KitchenFixedTooltipTextName);
	}
	if (!TooltipBorder || !TooltipLabel)
	{
		return nullptr;
	}

	if (!TooltipBorder->GetParent())
	{
		if (UPanelWidget* RootPanel = Cast<UPanelWidget>(OwnerWidget->WidgetTree->RootWidget))
		{
			RootPanel->AddChild(TooltipBorder);
		}
	}

	const float SafeRenderScale = FMath::Max(1.0f, RenderScale);
	const FVector2D ScaledPosition = ScaleKitchenVector(Position, SafeRenderScale);
	const FVector2D ScaledSize = ScaleKitchenVector(Size, SafeRenderScale);

	TooltipBorder->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.88f));
	TooltipBorder->SetHorizontalAlignment(HAlign_Center);
	TooltipBorder->SetVerticalAlignment(VAlign_Center);
	TooltipBorder->SetPadding(ScaleKitchenMargin(FMargin(4.0f, 2.0f), SafeRenderScale));
	TooltipBorder->SetVisibility(ESlateVisibility::Collapsed);

	TooltipLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	TooltipLabel->SetJustification(ETextJustify::Center);
	TooltipLabel->SetAutoWrapText(false);

	FSlateFontInfo Font = TooltipLabel->GetFont();
	Font.Size = FMath::RoundToInt(static_cast<float>(FMath::Max(FontSize, 10)) * SafeRenderScale);
	Font.OutlineSettings.OutlineSize = 1;
	Font.OutlineSettings.OutlineColor = FLinearColor::Black;
	TooltipLabel->SetFont(Font);

	TooltipBorder->SetContent(TooltipLabel);

	const FVector2D SafePosition(
		FMath::Max(ScaledPosition.X + ScaledSize.X, 12.0 * SafeRenderScale),
		FMath::Max(ScaledPosition.Y, 30.0 * SafeRenderScale));
	if (UCanvasPanelSlot* TooltipCanvasSlot = Cast<UCanvasPanelSlot>(TooltipBorder->Slot))
	{
		TooltipCanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		TooltipCanvasSlot->SetAlignment(FVector2D(1.0f, 0.0f));
		TooltipCanvasSlot->SetAutoSize(true);
		TooltipCanvasSlot->SetPosition(SafePosition);
		TooltipCanvasSlot->SetZOrder(1000);
	}
	else if (UOverlaySlot* TooltipOverlaySlot = Cast<UOverlaySlot>(TooltipBorder->Slot))
	{
		TooltipOverlaySlot->SetHorizontalAlignment(HAlign_Left);
		TooltipOverlaySlot->SetVerticalAlignment(VAlign_Top);
		TooltipOverlaySlot->SetPadding(FMargin(SafePosition.X - ScaledSize.X, SafePosition.Y, 0.0f, 0.0f));
	}

	return TooltipBorder;
}
}

void UKitchenIngredientEntryClickHandler::Initialize(UKitchenCraftStationComponent* InStation, UKitchenCraftWidgetHostComponent* InHost, const FKitchenIngredientOption& InOption)
{
	Station = InStation;
	Host = InHost;
	Option = InOption;
}

void UKitchenIngredientEntryClickHandler::HandleClicked()
{
	UKitchenCraftStationComponent* StationPtr = Station.Get();
	UKitchenCraftWidgetHostComponent* HostPtr = Host.Get();
	if (!StationPtr || Option.ItemId.IsNone())
	{
		return;
	}

	AKitchenIngredientActor* SpawnedActor = nullptr;
	if (StationPtr->TrySelectIngredient(Option.ItemId, 1, SpawnedActor) && HostPtr)
	{
		HostPtr->RefreshKitchenPicker();
	}
}

void UKitchenIngredientEntryClickHandler::HandleHovered()
{
	if (UKitchenCraftWidgetHostComponent* HostPtr = Host.Get())
	{
		HostPtr->ShowEntryTooltip(MakeKitchenEntryTooltipText(Option));
	}
}

void UKitchenIngredientEntryClickHandler::HandleUnhovered()
{
	if (UKitchenCraftWidgetHostComponent* HostPtr = Host.Get())
	{
		HostPtr->HideEntryTooltip();
	}
}

UKitchenCraftWidgetHostComponent::UKitchenCraftWidgetHostComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	static ConstructorHelpers::FClassFinder<UUserWidget> EntryWidgetFinder(TEXT("/Game/BluePrint/UI/WBP_KitchenIngredientEntry"));
	if (EntryWidgetFinder.Succeeded())
	{
		IngredientEntryWidgetClass = EntryWidgetFinder.Class;
	}
}

void UKitchenCraftWidgetHostComponent::BeginPlay()
{
	Super::BeginPlay();

	ResolveReferences();
	ConfigurePickerLayout();
	BindCloseButton();
	BindSelectionChanged();
	ConfigureIngredientScrollBox();

	if (bHidePickerOnBeginPlay)
	{
		SetPickerVisible(false);
	}
}

void UKitchenCraftWidgetHostComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetKitchenOwnerOutlineSuppressed(this, false);
	RestoreWorldClickEvents();
	RestorePlayerPawnVisibility();
	UnbindSelectionChanged();
	EntryClickHandlers.Reset();
	Super::EndPlay(EndPlayReason);
}

bool UKitchenCraftWidgetHostComponent::OpenKitchenPicker(APlayerController* RequestingController, UObject* InventoryProvider)
{
	if (!ResolveReferences())
	{
		return false;
	}

	APlayerController* Controller = ResolveController(RequestingController);
	UObject* ResolvedInventoryProvider = ResolveInventoryProvider(Controller, InventoryProvider);
	if (!ResolvedInventoryProvider)
	{
		return false;
	}

	if (!KitchenCraftStation->BeginKitchenSession(ResolvedInventoryProvider, OpenMode, Controller, bUseKitchenStationCameraOnOpen))
	{
		return false;
	}

	ActiveController = Controller;
	ActiveInventoryProvider = ResolvedInventoryProvider;
	bPickerOpen = true;

	SetKitchenOwnerOutlineSuppressed(this, true);
	EnableWorldClickEventsForSession(Controller);
	HidePlayerPawnForSession(Controller);
	ConfigurePickerLayout();
	SetPickerVisible(true);
	BindCloseButton();
	BindSelectionChanged();
	UScrollBox* IngredientScrollBox = ConfigureIngredientScrollBox();

	if (UUserWidget* PickerWidget = GetPickerWidget())
	{
		PickerWidget->SetIsFocusable(true);
		TryCallBlueprintInit(PickerWidget, ResolvedInventoryProvider);
	}

	RefreshKitchenPicker();
	if (IngredientScrollBox)
	{
		IngredientScrollBox->ScrollToStart();
	}

	if (Controller && bSetGameAndUIInputOnOpen)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		if (UUserWidget* PickerWidget = GetPickerWidget())
		{
			InputMode.SetWidgetToFocus(PickerWidget->TakeWidget());
		}
		Controller->SetInputMode(InputMode);
		Controller->bShowMouseCursor = true;
	}

	return true;
}

bool UKitchenCraftWidgetHostComponent::OpenKitchenPickerForMode(EKitchenCraftMode Mode, APlayerController* RequestingController, UObject* InventoryProvider)
{
	if (!bPickerOpen)
	{
		const EKitchenCraftMode PreviousOpenMode = OpenMode;
		OpenMode = Mode;
		const bool bOpened = OpenKitchenPicker(RequestingController, InventoryProvider);
		OpenMode = PreviousOpenMode;
		return bOpened;
	}

	if (!ResolveReferences() || !KitchenCraftStation)
	{
		return false;
	}

	APlayerController* Controller = ResolveController(RequestingController);
	if (KitchenCraftStation->GetCraftMode() != Mode && IsValid(KitchenCraftStation->GetPendingCraftResultActor()))
	{
		return false;
	}

	KitchenCraftStation->SetCraftMode(Mode, true, Controller, bUseKitchenStationCameraOnOpen);
	if (KitchenCraftStation->GetCraftMode() != Mode)
	{
		return false;
	}

	ActiveController = Controller;
	if (InventoryProvider)
	{
		ActiveInventoryProvider = InventoryProvider;
	}

	SetKitchenOwnerOutlineSuppressed(this, true);
	ConfigurePickerLayout();
	SetPickerVisible(true);
	BindCloseButton();
	BindSelectionChanged();
	UScrollBox* IngredientScrollBox = ConfigureIngredientScrollBox();

	if (UUserWidget* PickerWidget = GetPickerWidget())
	{
		PickerWidget->SetIsFocusable(true);
		if (ActiveInventoryProvider)
		{
			TryCallBlueprintInit(PickerWidget, ActiveInventoryProvider);
		}
	}

	const bool bRefreshed = RefreshKitchenPicker();
	if (IngredientScrollBox)
	{
		IngredientScrollBox->ScrollToStart();
	}

	return bRefreshed;
}

void UKitchenCraftWidgetHostComponent::CloseKitchenPicker(APlayerController* RequestingController, bool bReturnReservedIngredients)
{
	if (!ResolveReferences())
	{
		RestorePlayerPawnVisibility();
		SetKitchenOwnerOutlineSuppressed(this, false);
		return;
	}

	APlayerController* Controller = ResolveController(RequestingController);
	if (!Controller)
	{
		Controller = ActiveController.Get();
	}

	if (bPreventCloseWhileResultWaiting && KitchenCraftStation && IsValid(KitchenCraftStation->GetPendingCraftResultActor()))
	{
		UE_LOG(LogKitchenCraftUI, Warning, TEXT("CloseKitchenPicker blocked: pending craft result is waiting. Owner=%s ResultActor=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(KitchenCraftStation->GetPendingCraftResultActor()));
		RefreshKitchenPicker();
		return;
	}

	if (KitchenCraftStation)
	{
		KitchenCraftStation->EndKitchenSession(bReturnReservedIngredients, Controller, bUseKitchenStationCameraOnClose);
	}

	RestorePlayerPawnVisibility();

	if (!bUseKitchenStationCameraOnClose)
	{
		TryCallOwnerReturnCameraFunction();
	}

	EntryClickHandlers.Reset();
	ActiveInventoryProvider = nullptr;
	bPickerOpen = false;
	SetPickerVisible(false);
	SetKitchenOwnerOutlineSuppressed(this, false);
	RestoreWorldClickEvents();

	if (Controller && bSetGameOnlyInputOnClose)
	{
		FInputModeGameOnly InputMode;
		Controller->SetInputMode(InputMode);
		Controller->bShowMouseCursor = false;
	}
}

bool UKitchenCraftWidgetHostComponent::RefreshKitchenPicker()
{
	if (!ResolveReferences() || !KitchenCraftStation || !ActiveInventoryProvider)
	{
		return false;
	}

	UUserWidget* PickerWidget = GetPickerWidget();
	UPanelWidget* GridPanel = FindKitchenWidgetByName<UPanelWidget>(PickerWidget, GridWidgetName);
	if (!PickerWidget || !GridPanel)
	{
		return false;
	}
	ConfigurePickerLayout();
	ConfigureIngredientScrollBox();

	TArray<FKitchenIngredientOption> Options;
	if (!UKitchenCraftBlueprintLibrary::GetKitchenIngredientOptions(KitchenCraftStation, ActiveInventoryProvider, Options, true))
	{
		GridPanel->ClearChildren();
		EntryClickHandlers.Reset();
		return false;
	}

	GridPanel->ClearChildren();
	EntryClickHandlers.Reset();

	UE_LOG(LogKitchenCraftUI, Log, TEXT("Kitchen picker refresh: %d available option(s), provider=%s"),
		Options.Num(),
		*GetNameSafe(ActiveInventoryProvider));

	APlayerController* Controller = ResolveController(ActiveController.Get());
	for (const FKitchenIngredientOption& Option : Options)
	{
		if (!IngredientEntryWidgetClass)
		{
			break;
		}

		UUserWidget* EntryWidget = Controller
			? CreateWidget<UUserWidget>(Controller, IngredientEntryWidgetClass)
			: CreateWidget<UUserWidget>(GetWorld(), IngredientEntryWidgetClass);
		if (!EntryWidget)
		{
			continue;
		}

		ConfigureEntryWidget(EntryWidget, Option);
		UWidget* EntryContent = EntryWidget;
		if (USizeBox* SizedEntryWidget = CreateSizedEntryWidget(PickerWidget, EntryWidget))
		{
			EntryContent = SizedEntryWidget;
		}

		UPanelSlot* AddedSlot = GridPanel->AddChild(EntryContent);
		if (UWrapBoxSlot* WrapBoxSlot = Cast<UWrapBoxSlot>(AddedSlot))
		{
			WrapBoxSlot->SetPadding(ScaleKitchenMargin(EntryPadding, GetPickerRenderScale()));
			WrapBoxSlot->SetFillEmptySpace(true);
			WrapBoxSlot->SetHorizontalAlignment(HAlign_Center);
			WrapBoxSlot->SetVerticalAlignment(VAlign_Center);
		}
	}

	return true;
}

void UKitchenCraftWidgetHostComponent::SetPickerVisible(bool bVisible)
{
	if (!ResolveReferences())
	{
		return;
	}

	UWidgetComponent* ActivePickerComponent = GetActivePickerComponent();
	if (!ActivePickerComponent)
	{
		return;
	}

	TArray<UWidgetComponent*> PickerComponents;
	auto AddUniquePickerComponent = [&PickerComponents](UWidgetComponent* PickerComponent)
	{
		if (PickerComponent)
		{
			PickerComponents.AddUnique(PickerComponent);
		}
	};

	AddUniquePickerComponent(IngredientPickerComponent);
	AddUniquePickerComponent(PanIngredientPickerComponent);
	AddUniquePickerComponent(IntermediateIngredientPickerComponent);

	for (UWidgetComponent* PickerComponent : PickerComponents)
	{
		const bool bShowThisPicker = bVisible && PickerComponent == ActivePickerComponent;
		PickerComponent->SetVisibility(bShowThisPicker, true);
		PickerComponent->SetHiddenInGame(!bShowThisPicker, true);
	}

	if (!bVisible)
	{
		HideEntryTooltip();
	}
}

void UKitchenCraftWidgetHostComponent::ShowEntryTooltip(const FText& TooltipText)
{
	if (!bUseFixedEntryTooltip)
	{
		return;
	}

	UUserWidget* PickerWidget = GetPickerWidget();
	UBorder* TooltipBorder = FindOrCreateKitchenFixedTooltip(
		PickerWidget,
		FixedEntryTooltipPosition,
		FixedEntryTooltipSize,
		FixedEntryTooltipFontSize,
		GetPickerRenderScale());
	UTextBlock* TooltipLabel = PickerWidget && PickerWidget->WidgetTree
		? Cast<UTextBlock>(PickerWidget->WidgetTree->FindWidget(KitchenFixedTooltipTextName))
		: nullptr;
	if (!TooltipBorder || !TooltipLabel)
	{
		return;
	}

	TooltipLabel->SetText(TooltipText);
	TooltipBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UKitchenCraftWidgetHostComponent::HideEntryTooltip()
{
	UUserWidget* PickerWidget = GetPickerWidget();
	UBorder* TooltipBorder = PickerWidget && PickerWidget->WidgetTree
		? Cast<UBorder>(PickerWidget->WidgetTree->FindWidget(KitchenFixedTooltipBorderName))
		: nullptr;
	if (TooltipBorder)
	{
		TooltipBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UKitchenCraftWidgetHostComponent::HandleCloseClicked()
{
	CloseKitchenPicker(ActiveController.Get(), true);
}

void UKitchenCraftWidgetHostComponent::HandleSelectionChanged()
{
	if (bPickerOpen && bRefreshOnSelectionChanged)
	{
		RefreshKitchenPicker();
	}
}

bool UKitchenCraftWidgetHostComponent::ResolveReferences()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	if (!KitchenCraftStation)
	{
		KitchenCraftStation = Owner->FindComponentByClass<UKitchenCraftStationComponent>();
	}

	ResolvePickerComponents(Owner);
	ConfigurePickerComponent(IngredientPickerComponent);
	ConfigurePickerComponent(PanIngredientPickerComponent);
	ConfigurePickerComponent(IntermediateIngredientPickerComponent);

	return KitchenCraftStation && GetActivePickerComponent();
}

void UKitchenCraftWidgetHostComponent::ResolvePickerComponents(AActor* Owner)
{
	if (!Owner)
	{
		return;
	}

	TArray<UWidgetComponent*> WidgetComponents;
	Owner->GetComponents<UWidgetComponent>(WidgetComponents);

	auto FindWidgetComponent = [&WidgetComponents](FName ComponentName) -> UWidgetComponent*
	{
		if (ComponentName.IsNone())
		{
			return nullptr;
		}

		for (UWidgetComponent* WidgetComponent : WidgetComponents)
		{
			if (WidgetComponent && WidgetComponent->GetFName() == ComponentName)
			{
				return WidgetComponent;
			}
		}

		return nullptr;
	};

	if (UWidgetComponent* NamedLegacyPicker = FindWidgetComponent(IngredientPickerComponentName))
	{
		IngredientPickerComponent = NamedLegacyPicker;
	}
	else if (!IngredientPickerComponent && WidgetComponents.Num() > 0)
	{
		IngredientPickerComponent = WidgetComponents[0];
	}

	if (UWidgetComponent* NamedPanPicker = FindWidgetComponent(PanIngredientPickerComponentName))
	{
		PanIngredientPickerComponent = NamedPanPicker;
	}
	else if (!PanIngredientPickerComponent)
	{
		PanIngredientPickerComponent = IngredientPickerComponent;
	}

	if (UWidgetComponent* NamedIntermediatePicker = FindWidgetComponent(IntermediateIngredientPickerComponentName))
	{
		IntermediateIngredientPickerComponent = NamedIntermediatePicker;
	}
	else if (!IntermediateIngredientPickerComponent)
	{
		IntermediateIngredientPickerComponent = IngredientPickerComponent;
	}

	if (!IngredientPickerComponent)
	{
		IngredientPickerComponent = PanIngredientPickerComponent ? PanIngredientPickerComponent : IntermediateIngredientPickerComponent;
	}
}

APlayerController* UKitchenCraftWidgetHostComponent::ResolveController(APlayerController* RequestingController) const
{
	if (RequestingController)
	{
		return RequestingController;
	}

	if (ActiveController)
	{
		return ActiveController.Get();
	}

	return UGameplayStatics::GetPlayerController(this, 0);
}

UObject* UKitchenCraftWidgetHostComponent::ResolveInventoryProvider(APlayerController* RequestingController, UObject* ProvidedInventoryProvider) const
{
	if (ProvidedInventoryProvider && ProvidedInventoryProvider->GetClass()->ImplementsInterface(UKitchenInventoryProvider::StaticClass()))
	{
		return ProvidedInventoryProvider;
	}

	APlayerController* Controller = ResolveController(RequestingController);
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Pawn)
	{
		return nullptr;
	}

	if (Pawn->GetClass()->ImplementsInterface(UKitchenInventoryProvider::StaticClass()))
	{
		return Pawn;
	}

	TArray<UActorComponent*> Components;
	Pawn->GetComponents(Components);
	for (UActorComponent* Component : Components)
	{
		if (Component && Component->GetClass()->ImplementsInterface(UKitchenInventoryProvider::StaticClass()))
		{
			return Component;
		}
	}

	return nullptr;
}

UWidgetComponent* UKitchenCraftWidgetHostComponent::GetPickerComponentForMode(EKitchenCraftMode Mode) const
{
	return Mode == EKitchenCraftMode::IntermediatePrep
		? (IntermediateIngredientPickerComponent ? IntermediateIngredientPickerComponent.Get() : IngredientPickerComponent.Get())
		: (PanIngredientPickerComponent ? PanIngredientPickerComponent.Get() : IngredientPickerComponent.Get());
}

UWidgetComponent* UKitchenCraftWidgetHostComponent::GetActivePickerComponent() const
{
	const EKitchenCraftMode ActiveMode = KitchenCraftStation ? KitchenCraftStation->GetCraftMode() : OpenMode;
	return GetPickerComponentForMode(ActiveMode);
}

void UKitchenCraftWidgetHostComponent::ConfigurePickerComponent(UWidgetComponent* PickerComponent)
{
	if (!PickerComponent)
	{
		return;
	}

	if (bApplyPickerComponentOffset && !PickerComponentOffset.IsNearlyZero() && !OffsetAppliedPickerComponents.Contains(PickerComponent))
	{
		PickerComponent->AddRelativeLocation(PickerComponentOffset);
		OffsetAppliedPickerComponents.Add(PickerComponent);
	}

	if (bForcePickerDrawSize)
	{
		const float TooltipAreaWidth = bUseFixedEntryTooltip
			? FMath::Max(0.0f, static_cast<float>(FixedEntryTooltipPosition.X + FixedEntryTooltipSize.X + 2.0))
			: 0.0f;
		const float RenderScale = GetPickerRenderScale();
		const FVector2D EffectiveDrawSize(PickerDrawSize.X + TooltipAreaWidth, PickerDrawSize.Y);
		PickerComponent->SetDrawSize(EffectiveDrawSize * RenderScale);
		ApplyPickerComponentRenderScale(PickerComponent);
		if (bUseFixedEntryTooltip && EffectiveDrawSize.X > KINDA_SMALL_NUMBER)
		{
			const float PivotX = (TooltipAreaWidth + PickerDrawSize.X * 0.5f) / EffectiveDrawSize.X;
			PickerComponent->SetPivot(FVector2D(PivotX, 0.5f));
		}
	}

	if (FBoolProperty* ReceiveHardwareInputProperty = FindFProperty<FBoolProperty>(PickerComponent->GetClass(), TEXT("bReceiveHardwareInput")))
	{
		ReceiveHardwareInputProperty->SetPropertyValue_InContainer(PickerComponent, true);
	}

	if (FBoolProperty* WindowFocusableProperty = FindFProperty<FBoolProperty>(PickerComponent->GetClass(), TEXT("bWindowFocusable")))
	{
		WindowFocusableProperty->SetPropertyValue_InContainer(PickerComponent, true);
	}
}

float UKitchenCraftWidgetHostComponent::GetPickerRenderScale() const
{
	return bForcePickerDrawSize ? FMath::Max(1.0f, PickerRenderScale) : 1.0f;
}

void UKitchenCraftWidgetHostComponent::ApplyPickerComponentRenderScale(UWidgetComponent* PickerComponent)
{
	if (!PickerComponent)
	{
		return;
	}

	const float RenderScale = GetPickerRenderScale();
	if (!OriginalPickerComponentScales.Contains(PickerComponent))
	{
		OriginalPickerComponentScales.Add(PickerComponent, PickerComponent->GetRelativeScale3D());
	}

	if (const FVector* OriginalScale = OriginalPickerComponentScales.Find(PickerComponent))
	{
		PickerComponent->SetRelativeScale3D(*OriginalScale / RenderScale);
	}
}

void UKitchenCraftWidgetHostComponent::ConfigurePickerRootRenderScale(UUserWidget* PickerWidget) const
{
	if (!PickerWidget)
	{
		return;
	}

	PickerWidget->SetRenderTransformPivot(FVector2D::ZeroVector);
	PickerWidget->SetRenderScale(FVector2D(1.0f, 1.0f));
}

UUserWidget* UKitchenCraftWidgetHostComponent::GetPickerWidget() const
{
	UWidgetComponent* ActivePickerComponent = GetActivePickerComponent();
	if (!ActivePickerComponent)
	{
		return nullptr;
	}

	ActivePickerComponent->InitWidget();
	UUserWidget* PickerWidget = ActivePickerComponent->GetUserWidgetObject();
	ConfigurePickerRootRenderScale(PickerWidget);
	return PickerWidget;
}

bool UKitchenCraftWidgetHostComponent::BindCloseButton()
{
	UButton* CloseButton = FindKitchenWidgetByName<UButton>(GetPickerWidget(), CloseButtonName);
	if (!CloseButton)
	{
		return false;
	}

	ConfigureCloseButtonVisuals();
	CloseButton->OnClicked.RemoveDynamic(this, &UKitchenCraftWidgetHostComponent::HandleCloseClicked);
	CloseButton->OnClicked.AddDynamic(this, &UKitchenCraftWidgetHostComponent::HandleCloseClicked);
	return true;
}

void UKitchenCraftWidgetHostComponent::BindSelectionChanged()
{
	if (!bRefreshOnSelectionChanged || !KitchenCraftStation || bSelectionBound)
	{
		return;
	}

	KitchenCraftStation->OnSelectionChanged.AddUniqueDynamic(this, &UKitchenCraftWidgetHostComponent::HandleSelectionChanged);
	bSelectionBound = true;
}

void UKitchenCraftWidgetHostComponent::UnbindSelectionChanged()
{
	if (KitchenCraftStation && bSelectionBound)
	{
		KitchenCraftStation->OnSelectionChanged.RemoveDynamic(this, &UKitchenCraftWidgetHostComponent::HandleSelectionChanged);
	}

	bSelectionBound = false;
}

void UKitchenCraftWidgetHostComponent::ConfigurePickerLayout()
{
	if (!bForcePickerLayout)
	{
		return;
	}

	UUserWidget* PickerWidget = GetPickerWidget();
	if (!PickerWidget)
	{
		return;
	}

	const float RenderScale = GetPickerRenderScale();
	const float TooltipAreaWidth = bUseFixedEntryTooltip
		? FMath::Max(0.0f, static_cast<float>(FixedEntryTooltipPosition.X + FixedEntryTooltipSize.X + 2.0))
		: 0.0f;
	const FVector2D ContentOffset(TooltipAreaWidth, 0.0f);

	if (UWidget* PanelBackground = FindKitchenWidgetByName<UWidget>(PickerWidget, PanelBackgroundName))
	{
		PanelBackground->SetClipping(EWidgetClipping::ClipToBounds);

		if (UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(PanelBackground->Slot))
		{
			PanelSlot->SetAnchors(FAnchors(0.0f, 0.0f));
			PanelSlot->SetAlignment(FVector2D::ZeroVector);
			PanelSlot->SetAutoSize(false);
			PanelSlot->SetPosition(ScaleKitchenVector(PickerPanelPosition + ContentOffset, RenderScale));
			PanelSlot->SetSize(ScaleKitchenVector(PickerPanelSize, RenderScale));
			PanelSlot->SetZOrder(0);
		}
	}

	if (bUseFixedEntryTooltip)
	{
		FindOrCreateKitchenFixedTooltip(
			PickerWidget,
			FixedEntryTooltipPosition,
			FixedEntryTooltipSize,
			FixedEntryTooltipFontSize,
			RenderScale);
	}

	ConfigureIngredientScrollBox();
	ConfigureCloseButtonVisuals();
}

UScrollBox* UKitchenCraftWidgetHostComponent::ConfigureIngredientScrollBox()
{
	UScrollBox* IngredientScrollBox = FindKitchenWidgetByName<UScrollBox>(GetPickerWidget(), IngredientScrollBoxName);
	if (!IngredientScrollBox)
	{
		return nullptr;
	}

	const float ScrollBoxWidth = FMath::Max(1.0f, PickerPanelSize.X - IngredientScrollBoxPadding.Left - IngredientScrollBoxPadding.Right);
	const float ScrollBoxHeight = FMath::Max(1.0f, PickerPanelSize.Y - IngredientScrollBoxPadding.Top - IngredientScrollBoxPadding.Bottom);
	const float RenderScale = GetPickerRenderScale();
	const float TooltipAreaWidth = bUseFixedEntryTooltip
		? FMath::Max(0.0f, static_cast<float>(FixedEntryTooltipPosition.X + FixedEntryTooltipSize.X + 2.0))
		: 0.0f;
	const FVector2D ScrollBoxPosition(TooltipAreaWidth + IngredientScrollBoxPadding.Left, IngredientScrollBoxPadding.Top);
	const FVector2D ScrollBoxSize(ScrollBoxWidth, ScrollBoxHeight);

	IngredientScrollBox->SetClipping(EWidgetClipping::ClipToBounds);
	IngredientScrollBox->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
	IngredientScrollBox->SetScrollBarVisibility(bShowIngredientScrollBar ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	IngredientScrollBox->SetAlwaysShowScrollbar(bShowIngredientScrollBar);
	IngredientScrollBox->SetAllowOverscroll(false);
	IngredientScrollBox->SetAnimateWheelScrolling(true);
	IngredientScrollBox->SetWheelScrollMultiplier(IngredientWheelScrollMultiplier);

	if (UPanelWidget* GridPanel = FindKitchenWidgetByName<UPanelWidget>(GetPickerWidget(), GridWidgetName))
	{
		GridPanel->SetClipping(EWidgetClipping::ClipToBounds);
	}

	if (UCanvasPanelSlot* ScrollCanvasSlot = Cast<UCanvasPanelSlot>(IngredientScrollBox->Slot))
	{
		ScrollCanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		ScrollCanvasSlot->SetAlignment(FVector2D::ZeroVector);
		ScrollCanvasSlot->SetAutoSize(false);
		ScrollCanvasSlot->SetPosition(ScaleKitchenVector(ScrollBoxPosition, RenderScale));
		ScrollCanvasSlot->SetSize(ScaleKitchenVector(ScrollBoxSize, RenderScale));
	}
	else if (UOverlaySlot* ScrollOverlaySlot = Cast<UOverlaySlot>(IngredientScrollBox->Slot))
	{
		ScrollOverlaySlot->SetHorizontalAlignment(HAlign_Fill);
		ScrollOverlaySlot->SetVerticalAlignment(VAlign_Fill);
		ScrollOverlaySlot->SetPadding(ScaleKitchenMargin(IngredientScrollBoxPadding, RenderScale));
	}
	else if (UVerticalBoxSlot* ScrollVerticalSlot = Cast<UVerticalBoxSlot>(IngredientScrollBox->Slot))
	{
		ScrollVerticalSlot->SetHorizontalAlignment(HAlign_Fill);
		ScrollVerticalSlot->SetVerticalAlignment(VAlign_Fill);
		ScrollVerticalSlot->SetPadding(ScaleKitchenMargin(IngredientScrollBoxPadding, RenderScale));
		ScrollVerticalSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	else if (UBorderSlot* ScrollBorderSlot = Cast<UBorderSlot>(IngredientScrollBox->Slot))
	{
		ScrollBorderSlot->SetHorizontalAlignment(HAlign_Fill);
		ScrollBorderSlot->SetVerticalAlignment(VAlign_Fill);
		ScrollBorderSlot->SetPadding(ScaleKitchenMargin(IngredientScrollBoxPadding, RenderScale));
	}

	return IngredientScrollBox;
}

void UKitchenCraftWidgetHostComponent::ConfigureCloseButtonVisuals()
{
	UUserWidget* PickerWidget = GetPickerWidget();
	UButton* CloseButton = FindKitchenWidgetByName<UButton>(PickerWidget, CloseButtonName);
	if (!PickerWidget || !CloseButton)
	{
		return;
	}

	CloseButton->SetVisibility(ESlateVisibility::Visible);
	CloseButton->SetIsEnabled(true);
	CloseButton->SetRenderOpacity(1.0f);

	if (UCanvasPanelSlot* CloseSlot = Cast<UCanvasPanelSlot>(CloseButton->Slot))
	{
		const float RenderScale = GetPickerRenderScale();
		CloseSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		CloseSlot->SetAlignment(FVector2D::ZeroVector);
		CloseSlot->SetAutoSize(false);
		const float TooltipAreaWidth = bUseFixedEntryTooltip
			? FMath::Max(0.0f, static_cast<float>(FixedEntryTooltipPosition.X + FixedEntryTooltipSize.X + 2.0))
			: 0.0f;
		CloseSlot->SetPosition(ScaleKitchenVector(CloseButtonPosition + FVector2D(TooltipAreaWidth, 0.0f), RenderScale));
		CloseSlot->SetSize(ScaleKitchenVector(CloseButtonSize, RenderScale));
		CloseSlot->SetZOrder(100);
	}

	if (UTextBlock* ExistingLabel = Cast<UTextBlock>(CloseButton->GetContent()))
	{
		ExistingLabel->SetText(CloseButtonText);
		ExistingLabel->SetJustification(ETextJustify::Center);
		ExistingLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		return;
	}

	if (PickerWidget->WidgetTree)
	{
		if (CloseButtonText.IsEmpty())
		{
			return;
		}

		UTextBlock* CloseLabel = PickerWidget->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		if (CloseLabel)
		{
			CloseLabel->SetText(CloseButtonText);
			CloseLabel->SetJustification(ETextJustify::Center);
			CloseLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			CloseButton->SetContent(CloseLabel);
		}
	}
}

USizeBox* UKitchenCraftWidgetHostComponent::CreateSizedEntryWidget(UUserWidget* PickerWidget, UUserWidget* EntryWidget)
{
	if (!PickerWidget || !PickerWidget->WidgetTree || !EntryWidget)
	{
		return nullptr;
	}

	USizeBox* EntrySizeBox = PickerWidget->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	if (!EntrySizeBox)
	{
		return nullptr;
	}

	const FVector2D ScaledEntryWidgetSize = ScaleKitchenVector(EntryWidgetSize, GetPickerRenderScale());
	EntrySizeBox->SetWidthOverride(ScaledEntryWidgetSize.X);
	EntrySizeBox->SetHeightOverride(ScaledEntryWidgetSize.Y);
	EntrySizeBox->SetMinDesiredWidth(ScaledEntryWidgetSize.X);
	EntrySizeBox->SetMinDesiredHeight(ScaledEntryWidgetSize.Y);
	EntrySizeBox->SetClipping(EWidgetClipping::ClipToBounds);
	EntryWidget->SetClipping(EWidgetClipping::ClipToBounds);
	EntrySizeBox->AddChild(EntryWidget);
	return EntrySizeBox;
}

bool UKitchenCraftWidgetHostComponent::ConfigureEntryWidget(UUserWidget* EntryWidget, const FKitchenIngredientOption& Option)
{
	if (!EntryWidget || !KitchenCraftStation)
	{
		return false;
	}

	const float RenderScale = GetPickerRenderScale();

	if (UImage* IconImage = FindKitchenWidgetByName<UImage>(EntryWidget, EntryIconImageName))
	{
		IconImage->SetBrushFromTexture(Option.ItemData.IconTexture, false);
		IconImage->SetDesiredSizeOverride(EntryIconSize);
		IconImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		IconImage->SetRenderScale(FVector2D(RenderScale, RenderScale));

		if (UOverlaySlot* IconOverlaySlot = Cast<UOverlaySlot>(IconImage->Slot))
		{
			IconOverlaySlot->SetHorizontalAlignment(HAlign_Center);
			IconOverlaySlot->SetVerticalAlignment(VAlign_Center);
			IconOverlaySlot->SetPadding(FMargin(0.0f));
		}
		else if (UCanvasPanelSlot* IconCanvasSlot = Cast<UCanvasPanelSlot>(IconImage->Slot))
		{
			IconCanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			IconCanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			IconCanvasSlot->SetAutoSize(false);
			IconCanvasSlot->SetPosition(FVector2D::ZeroVector);
			IconCanvasSlot->SetSize(EntryIconSize);
			IconCanvasSlot->SetZOrder(0);
		}
	}

	if (UTextBlock* CountText = FindKitchenWidgetByName<UTextBlock>(EntryWidget, EntryCountTextName))
	{
		CountText->SetText(FText::AsNumber(Option.AvailableCount));
		CountText->SetJustification(ETextJustify::Center);
		CountText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		CountText->SetShadowOffset(FVector2D::ZeroVector);
		CountText->SetShadowColorAndOpacity(FLinearColor::Black);

		FSlateFontInfo CountFont = CountText->GetFont();
		CountFont.Size = FMath::RoundToInt(static_cast<float>(FMath::Clamp(EntryCountFontSize, 8, 10)) * RenderScale);
		CountFont.OutlineSettings.OutlineSize = 1;
		CountFont.OutlineSettings.OutlineColor = FLinearColor::Black;
		CountText->SetFont(CountFont);

		FMargin EffectiveCountPadding = EntryCountPadding;
		EffectiveCountPadding.Right = FMath::Max(0.0f, EntryCountPadding.Right - 3.0f);
		const FMargin ScaledCountPadding = ScaleKitchenMargin(EffectiveCountPadding, RenderScale);

		if (UOverlaySlot* CountOverlaySlot = Cast<UOverlaySlot>(CountText->Slot))
		{
			CountOverlaySlot->SetHorizontalAlignment(HAlign_Right);
			CountOverlaySlot->SetVerticalAlignment(VAlign_Bottom);
			CountOverlaySlot->SetPadding(ScaledCountPadding);
		}
		else if (UCanvasPanelSlot* CountCanvasSlot = Cast<UCanvasPanelSlot>(CountText->Slot))
		{
			CountCanvasSlot->SetAnchors(FAnchors(1.0f, 1.0f));
			CountCanvasSlot->SetAlignment(FVector2D(1.0f, 1.0f));
			CountCanvasSlot->SetAutoSize(true);
			CountCanvasSlot->SetPosition(FVector2D(-ScaledCountPadding.Right, -ScaledCountPadding.Bottom));
			CountCanvasSlot->SetZOrder(10);
		}
	}

	if (UButton* SelectButton = FindKitchenWidgetByName<UButton>(EntryWidget, EntrySelectButtonName))
	{
		UE_LOG(LogKitchenCraftUI, Verbose, TEXT("Kitchen picker entry: %s count=%d icon=%s"),
			*Option.ItemId.ToString(),
			Option.AvailableCount,
			*GetNameSafe(Option.ItemData.IconTexture));

		UKitchenIngredientEntryClickHandler* Handler = NewObject<UKitchenIngredientEntryClickHandler>(this);
		Handler->Initialize(KitchenCraftStation, this, Option);
		SelectButton->OnHovered.AddDynamic(Handler, &UKitchenIngredientEntryClickHandler::HandleHovered);
		SelectButton->OnUnhovered.AddDynamic(Handler, &UKitchenIngredientEntryClickHandler::HandleUnhovered);
		SelectButton->OnClicked.AddDynamic(Handler, &UKitchenIngredientEntryClickHandler::HandleClicked);
		EntryClickHandlers.Add(Handler);
	}

	return true;
}

bool UKitchenCraftWidgetHostComponent::TryCallBlueprintInit(UUserWidget* PickerWidget, UObject* InventoryProvider)
{
	if (!PickerWidget)
	{
		return false;
	}

	UFunction* InitFunction = PickerWidget->FindFunction(TEXT("InitKitchenIngredientPicker"));
	if (!InitFunction)
	{
		return false;
	}

	struct FInitKitchenIngredientPickerParams
	{
		UKitchenCraftStationComponent* InKitchenCraftStation;
		UObject* InInventoryProvider;
		UWidgetComponent* InOwnerWidgetComponent;
	};

	FInitKitchenIngredientPickerParams Params;
	Params.InKitchenCraftStation = KitchenCraftStation;
	Params.InInventoryProvider = InventoryProvider;
	Params.InOwnerWidgetComponent = IngredientPickerComponent;
	PickerWidget->ProcessEvent(InitFunction, &Params);
	return true;
}

bool UKitchenCraftWidgetHostComponent::TryCallOwnerReturnCameraFunction()
{
	AActor* Owner = GetOwner();
	if (!Owner || OwnerReturnCameraFunctionName.IsNone())
	{
		return false;
	}

	UFunction* ReturnCameraFunction = Owner->FindFunction(OwnerReturnCameraFunctionName);
	if (!ReturnCameraFunction)
	{
		return false;
	}

	Owner->ProcessEvent(ReturnCameraFunction, nullptr);
	return true;
}

void UKitchenCraftWidgetHostComponent::EnableWorldClickEventsForSession(APlayerController* RequestingController)
{
	if (!bEnableIngredientActorClickEvents || !RequestingController)
	{
		return;
	}

	if (!bHasCachedControllerClickSettings)
	{
		bCachedControllerClickEvents = RequestingController->bEnableClickEvents;
		bCachedControllerMouseOverEvents = RequestingController->bEnableMouseOverEvents;
		bHasCachedControllerClickSettings = true;
	}

	RequestingController->bEnableClickEvents = true;
	RequestingController->bEnableMouseOverEvents = true;
}

void UKitchenCraftWidgetHostComponent::RestoreWorldClickEvents()
{
	APlayerController* Controller = ActiveController.Get();
	if (!Controller || !bHasCachedControllerClickSettings)
	{
		bHasCachedControllerClickSettings = false;
		return;
	}

	Controller->bEnableClickEvents = bCachedControllerClickEvents;
	Controller->bEnableMouseOverEvents = bCachedControllerMouseOverEvents;
	bHasCachedControllerClickSettings = false;
}

void UKitchenCraftWidgetHostComponent::HidePlayerPawnForSession(APlayerController* RequestingController)
{
	if (!bHidePlayerPawnOnOpen)
	{
		return;
	}

	APlayerController* Controller = ResolveController(RequestingController);
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}

	if (bHasHiddenPlayerPawn && HiddenPlayerPawn.Get() == Pawn)
	{
		Pawn->SetActorHiddenInGame(true);
		return;
	}

	RestorePlayerPawnVisibility();
	HiddenPlayerPawn = Pawn;
	bCachedPlayerPawnHiddenState = Pawn->IsHidden();
	bHasHiddenPlayerPawn = true;
	Pawn->SetActorHiddenInGame(true);
}

void UKitchenCraftWidgetHostComponent::RestorePlayerPawnVisibility()
{
	if (!bHasHiddenPlayerPawn)
	{
		return;
	}

	if (AActor* PawnActor = HiddenPlayerPawn.Get())
	{
		PawnActor->SetActorHiddenInGame(bCachedPlayerPawnHiddenState);
	}

	HiddenPlayerPawn.Reset();
	bHasHiddenPlayerPawn = false;
	bCachedPlayerPawnHiddenState = false;
}
