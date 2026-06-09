#include "DiningTableMealComponent.h"

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
#include "Components/PrimitiveComponent.h"
#include "Components/ScrollBox.h"
#include "Components/SceneComponent.h"
#include "Components/SizeBox.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetComponent.h"
#include "Components/WrapBoxSlot.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InteractionCameraBlendComponent.h"
#include "KitchenCraftBlueprintLibrary.h"
#include "KitchenIngredientActor.h"
#include "KitchenInventoryProvider.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "ProximityOutlineComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogDiningTableMeal, Log, All);

namespace
{
constexpr float DiningEntryIconVisualScale = 2.5f;

template <typename WidgetType>
WidgetType* FindDiningWidgetByName(UUserWidget* RootWidget, FName WidgetName)
{
	if (!RootWidget || !RootWidget->WidgetTree || WidgetName.IsNone())
	{
		return nullptr;
	}

	return Cast<WidgetType>(RootWidget->WidgetTree->FindWidget(WidgetName));
}

void SetDiningOwnerOutlineSuppressed(UActorComponent* Component, bool bSuppressed)
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

FVector2D ScaleDiningVector(const FVector2D& Value, float Scale)
{
	return Value * FMath::Max(1.0f, Scale);
}

FMargin ScaleDiningMargin(const FMargin& Value, float Scale)
{
	const float SafeScale = FMath::Max(1.0f, Scale);
	return FMargin(
		Value.Left * SafeScale,
		Value.Top * SafeScale,
		Value.Right * SafeScale,
		Value.Bottom * SafeScale);
}

TSubclassOf<AKitchenIngredientActor> GetFallbackDiningFoodActorClass()
{
	static TWeakObjectPtr<UClass> CachedClass;
	if (!CachedClass.IsValid())
	{
		UClass* LoadedClass = StaticLoadClass(AKitchenIngredientActor::StaticClass(), nullptr, TEXT("/Game/BluePrint/Object/BP_KitchenIngredientActor.BP_KitchenIngredientActor_C"));
		if (LoadedClass && LoadedClass->IsChildOf(AKitchenIngredientActor::StaticClass()))
		{
			CachedClass = LoadedClass;
		}
	}

	return CachedClass.Get();
}

float GetDiningFixedTooltipGap(const FVector2D& Position)
{
	return FMath::Max(0.0f, Position.X);
}

float GetDiningFixedTooltipAreaWidth(const FVector2D& Position, const FVector2D& Size)
{
	return GetDiningFixedTooltipGap(Position) + FMath::Max(0.0f, Size.X);
}

FVector2D GetDiningFixedTooltipRightPosition(
	const FVector2D& PanelPosition,
	const FVector2D& PanelSize,
	const FVector2D& TooltipPosition)
{
	return FVector2D(
		PanelPosition.X + PanelSize.X + GetDiningFixedTooltipGap(TooltipPosition),
		TooltipPosition.Y);
}

UStaticMesh* GetFallbackDiningIconMesh()
{
	static TWeakObjectPtr<UStaticMesh> CachedMesh;
	if (!CachedMesh.IsValid())
	{
		CachedMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	}

	return CachedMesh.Get();
}

UMaterialInterface* GetFallbackDiningIconMaterial()
{
	static TWeakObjectPtr<UMaterialInterface> CachedMaterial;
	if (!CachedMaterial.IsValid())
	{
		CachedMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Resource/Material/M_KitchenIngredientIcon.M_KitchenIngredientIcon"));
	}

	return CachedMaterial.Get();
}

FProperty* FindDiningStructProperty(const UStruct* Struct, const TCHAR* FieldName)
{
	if (!Struct || !FieldName)
	{
		return nullptr;
	}

	const FString DesiredName(FieldName);
	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		FProperty* Property = *It;
		if (!Property)
		{
			continue;
		}

		const FString PropertyName = Property->GetName();
		if (PropertyName == DesiredName || PropertyName.StartsWith(DesiredName + TEXT("_")))
		{
			return Property;
		}
	}

	return nullptr;
}

bool SetDiningNumericField(const UStruct* Struct, void* Container, const TCHAR* FieldName, double Value)
{
	FNumericProperty* NumericProperty = CastField<FNumericProperty>(FindDiningStructProperty(Struct, FieldName));
	if (!NumericProperty || !Container)
	{
		return false;
	}

	void* ValuePtr = NumericProperty->ContainerPtrToValuePtr<void>(Container);
	if (NumericProperty->IsInteger())
	{
		NumericProperty->SetIntPropertyValue(ValuePtr, static_cast<int64>(Value));
	}
	else
	{
		NumericProperty->SetFloatingPointPropertyValue(ValuePtr, Value);
	}
	return true;
}

bool SetDiningStatBonusLikeStruct(const UStruct* Struct, void* Container, const FKitchenStatBonus& Bonus)
{
	if (!Struct || !Container)
	{
		return false;
	}

	bool bSetAny = false;
	bSetAny |= SetDiningNumericField(Struct, Container, TEXT("Quality"), Bonus.Quality);
	bSetAny |= SetDiningNumericField(Struct, Container, TEXT("HPBonus"), Bonus.HPBonus);
	bSetAny |= SetDiningNumericField(Struct, Container, TEXT("AttackTierBonus"), Bonus.AttackTierBonus);
	bSetAny |= SetDiningNumericField(Struct, Container, TEXT("AttackSpeedTierBonus"), Bonus.AttackSpeedTierBonus);
	bSetAny |= SetDiningNumericField(Struct, Container, TEXT("DashTierBonus"), Bonus.DashTierBonus);
	bSetAny |= SetDiningNumericField(Struct, Container, TEXT("DashCountBonus"), Bonus.DashCountBonus);
	bSetAny |= SetDiningNumericField(Struct, Container, TEXT("MoveSpeedBonus"), Bonus.MoveSpeedBonus);
	bSetAny |= SetDiningNumericField(Struct, Container, TEXT("DropRateBonus"), Bonus.DropRateBonus);
	return bSetAny;
}

bool SetDiningStatBonusInputParam(FProperty* Property, uint8* Params, const FKitchenStatBonus& Bonus)
{
	if (!Property || !Params)
	{
		return false;
	}

	if (FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
	{
		const FString PropertyName = Property->GetName();
		TOptional<int32> Value;
		if (PropertyName.Contains(TEXT("Quality"), ESearchCase::IgnoreCase)) { Value = Bonus.Quality; }
		else if (PropertyName.Contains(TEXT("HP"), ESearchCase::IgnoreCase)) { Value = Bonus.HPBonus; }
		else if (PropertyName.Contains(TEXT("AttackSpeed"), ESearchCase::IgnoreCase)) { Value = Bonus.AttackSpeedTierBonus; }
		else if (PropertyName.Contains(TEXT("Attack"), ESearchCase::IgnoreCase)) { Value = Bonus.AttackTierBonus; }
		else if (PropertyName.Contains(TEXT("DashCount"), ESearchCase::IgnoreCase)) { Value = Bonus.DashCountBonus; }
		else if (PropertyName.Contains(TEXT("Dash"), ESearchCase::IgnoreCase)) { Value = Bonus.DashTierBonus; }
		else if (PropertyName.Contains(TEXT("MoveSpeed"), ESearchCase::IgnoreCase)) { Value = Bonus.MoveSpeedBonus; }
		else if (PropertyName.Contains(TEXT("DropRate"), ESearchCase::IgnoreCase)) { Value = Bonus.DropRateBonus; }
		else { return false; }

		void* ValuePtr = NumericProperty->ContainerPtrToValuePtr<void>(Params);
		if (NumericProperty->IsInteger())
		{
			NumericProperty->SetIntPropertyValue(ValuePtr, static_cast<int64>(Value.GetValue()));
		}
		else
		{
			NumericProperty->SetFloatingPointPropertyValue(ValuePtr, static_cast<double>(Value.GetValue()));
		}
		return true;
	}

	if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		void* StructValue = StructProperty->ContainerPtrToValuePtr<void>(Params);
		if (StructProperty->Struct == FKitchenStatBonus::StaticStruct())
		{
			*static_cast<FKitchenStatBonus*>(StructValue) = Bonus;
			return true;
		}

		return SetDiningStatBonusLikeStruct(StructProperty->Struct, StructValue, Bonus);
	}

	return false;
}

FString DescribeDiningStatBonus(const FKitchenStatBonus& Bonus)
{
	return FString::Printf(TEXT("Quality=%d HP=%d Attack=%d AttackSpeed=%d Dash=%d DashCount=%d MoveSpeed=%d DropRate=%d"),
		Bonus.Quality,
		Bonus.HPBonus,
		Bonus.AttackTierBonus,
		Bonus.AttackSpeedTierBonus,
		Bonus.DashTierBonus,
		Bonus.DashCountBonus,
		Bonus.MoveSpeedBonus,
		Bonus.DropRateBonus);
}

void AppendDiningTooltipStatLine(FString& InOutText, const TCHAR* Label, int32 Value)
{
	if (!Label || Value == 0)
	{
		return;
	}

	InOutText += LINE_TERMINATOR;
	InOutText += FString::Printf(TEXT("%s %+d"), Label, Value);
}

FText MakeDiningMealTooltipText(const FKitchenIngredientOption& Option)
{
	FString TooltipText = Option.ItemData.DisplayName.ToString();
	if (TooltipText.IsEmpty())
	{
		TooltipText = Option.ItemId.ToString();
	}

	const FKitchenStatBonus& StatBonus = Option.bHasInventoryStatBonus
		? Option.InventoryStatBonus
		: Option.ItemData.BaseStatBonus;

	AppendDiningTooltipStatLine(TooltipText, TEXT("Quality"), StatBonus.Quality);
	AppendDiningTooltipStatLine(TooltipText, TEXT("HP"), StatBonus.HPBonus);
	AppendDiningTooltipStatLine(TooltipText, TEXT("Attack"), StatBonus.AttackTierBonus);
	AppendDiningTooltipStatLine(TooltipText, TEXT("Attack Speed"), StatBonus.AttackSpeedTierBonus);
	AppendDiningTooltipStatLine(TooltipText, TEXT("Dash"), StatBonus.DashTierBonus);
	AppendDiningTooltipStatLine(TooltipText, TEXT("Dash Count"), StatBonus.DashCountBonus);
	AppendDiningTooltipStatLine(TooltipText, TEXT("Move Speed"), StatBonus.MoveSpeedBonus);
	AppendDiningTooltipStatLine(TooltipText, TEXT("Drop Rate"), StatBonus.DropRateBonus);
	return FText::FromString(TooltipText);
}

const FName DiningFixedTooltipBorderName(TEXT("Native_DiningFixedEntryTooltip"));
const FName DiningFixedTooltipTextName(TEXT("Native_DiningFixedEntryTooltipText"));

UBorder* FindOrCreateDiningFixedTooltip(
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

	UBorder* TooltipBorder = Cast<UBorder>(OwnerWidget->WidgetTree->FindWidget(DiningFixedTooltipBorderName));
	UTextBlock* TooltipLabel = Cast<UTextBlock>(OwnerWidget->WidgetTree->FindWidget(DiningFixedTooltipTextName));
	if (!TooltipBorder)
	{
		TooltipBorder = OwnerWidget->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), DiningFixedTooltipBorderName);
	}
	if (!TooltipLabel)
	{
		TooltipLabel = OwnerWidget->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), DiningFixedTooltipTextName);
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
	const FVector2D ScaledPosition = ScaleDiningVector(Position, SafeRenderScale);

	TooltipBorder->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.88f));
	TooltipBorder->SetPadding(ScaleDiningMargin(FMargin(4.0f, 2.0f), SafeRenderScale));
	TooltipBorder->SetHorizontalAlignment(HAlign_Center);
	TooltipBorder->SetVerticalAlignment(VAlign_Center);
	TooltipBorder->SetVisibility(ESlateVisibility::Collapsed);

	TooltipLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	TooltipLabel->SetJustification(ETextJustify::Center);
	TooltipLabel->SetAutoWrapText(false);

	FSlateFontInfo Font = TooltipLabel->GetFont();
	Font.Size = FMath::RoundToInt(static_cast<float>(FMath::Max(FontSize, 9)) * SafeRenderScale);
	Font.OutlineSettings.OutlineSize = 1;
	Font.OutlineSettings.OutlineColor = FLinearColor::Black;
	TooltipLabel->SetFont(Font);

	TooltipBorder->SetContent(TooltipLabel);

	const FVector2D SafePosition(
		FMath::Max(ScaledPosition.X, 12.0 * SafeRenderScale),
		FMath::Max(ScaledPosition.Y, 30.0 * SafeRenderScale));
	if (UCanvasPanelSlot* TooltipCanvasSlot = Cast<UCanvasPanelSlot>(TooltipBorder->Slot))
	{
		TooltipCanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		TooltipCanvasSlot->SetAlignment(FVector2D::ZeroVector);
		TooltipCanvasSlot->SetAutoSize(true);
		TooltipCanvasSlot->SetPosition(SafePosition);
		TooltipCanvasSlot->SetZOrder(1000);
	}
	else if (UOverlaySlot* TooltipOverlaySlot = Cast<UOverlaySlot>(TooltipBorder->Slot))
	{
		TooltipOverlaySlot->SetHorizontalAlignment(HAlign_Left);
		TooltipOverlaySlot->SetVerticalAlignment(VAlign_Top);
		TooltipOverlaySlot->SetPadding(FMargin(SafePosition.X, SafePosition.Y, 0.0f, 0.0f));
	}

	return TooltipBorder;
}

FString DescribeDiningFunctionParams(const TArray<FProperty*>& ParamProperties)
{
	FString ParamLayout;
	for (FProperty* Property : ParamProperties)
	{
		if (!Property)
		{
			continue;
		}

		ParamLayout += FString::Printf(TEXT("%s:%s Flags=%llu; "),
			*Property->GetName(),
			*Property->GetClass()->GetName(),
			static_cast<uint64>(Property->PropertyFlags));
	}

	return ParamLayout;
}
}

void UDiningMealEntryClickHandler::Initialize(UDiningTableMealComponent* InMealComponent, const FKitchenIngredientOption& InOption)
{
	MealComponent = InMealComponent;
	Option = InOption;
}

void UDiningMealEntryClickHandler::HandleClicked()
{
	Activate(TEXT("OnClicked"));
}

void UDiningMealEntryClickHandler::HandlePressed()
{
	Activate(TEXT("OnPressed"));
}

void UDiningMealEntryClickHandler::HandleHovered()
{
	if (UDiningTableMealComponent* MealComponentPtr = MealComponent.Get())
	{
		MealComponentPtr->ShowEntryTooltip(MakeDiningMealTooltipText(Option));
	}
}

void UDiningMealEntryClickHandler::HandleUnhovered()
{
	if (UDiningTableMealComponent* MealComponentPtr = MealComponent.Get())
	{
		MealComponentPtr->HideEntryTooltip();
	}
}

void UDiningMealEntryClickHandler::Activate(const TCHAR* SourceEventName)
{
	if (UDiningTableMealComponent* MealComponentPtr = MealComponent.Get())
	{
		UE_LOG(LogDiningTableMeal, Warning, TEXT("Meal entry activated: %s. ItemId=%s StackIndex=%d Count=%d Component=%s Icon=%s Mesh=%s"),
			SourceEventName ? SourceEventName : TEXT("Unknown"),
			*Option.ItemId.ToString(),
			Option.InventoryStackIndex,
			Option.AvailableCount,
			*GetNameSafe(MealComponentPtr),
			*GetNameSafe(Option.ItemData.IconTexture),
			*GetNameSafe(Option.ItemData.IngredientMesh));

		const bool bSelected = MealComponentPtr->SelectMealFoodOption(Option);
		if (!bSelected)
		{
			UE_LOG(LogDiningTableMeal, Warning, TEXT("Meal entry click did not select food. ItemId=%s Component=%s"),
				*Option.ItemId.ToString(),
				*GetNameSafe(MealComponentPtr));
		}
	}
}

UDiningTableMealComponent::UDiningTableMealComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	static ConstructorHelpers::FClassFinder<UUserWidget> EntryWidgetFinder(TEXT("/Game/BluePrint/UI/WBP_KitchenIngredientEntry"));
	if (EntryWidgetFinder.Succeeded())
	{
		IngredientEntryWidgetClass = EntryWidgetFinder.Class;
	}

	static ConstructorHelpers::FClassFinder<AKitchenIngredientActor> FoodActorFinder(TEXT("/Game/BluePrint/Object/BP_KitchenIngredientActor"));
	if (FoodActorFinder.Succeeded())
	{
		DefaultFoodActorClass = FoodActorFinder.Class;
	}
}

void UDiningTableMealComponent::BeginPlay()
{
	Super::BeginPlay();

	SetComponentTickEnabled(false);
	ResolveReferences();
	ConfigurePickerLayout();
	BindCloseButton();
	ConfigureIngredientScrollBox();

	if (bHidePickerOnBeginPlay)
	{
		SetMealPickerVisible(false);
	}
}

void UDiningTableMealComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetDiningOwnerOutlineSuppressed(this, false);
	StopClatterMotion(false);
	RestoreWorldClickEvents();
	EntryClickHandlers.Reset();

	if (SelectedFoodActor)
	{
		SelectedFoodActor->Destroy();
		SelectedFoodActor = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UDiningTableMealComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bClatterActive || !SelectedFoodActor)
	{
		StopClatterMotion(false);
		return;
	}

	ClatterElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(ClatterElapsed / FMath::Max(ClatterDuration, 0.01f), 0.0f, 1.0f);
	const float Decay = 1.0f - Alpha;
	const float Wave = FMath::Sin(Alpha * ClatterFrequency * 2.0f * UE_PI);
	const float SecondaryWave = FMath::Sin(Alpha * ClatterFrequency * 3.1f * UE_PI + UE_PI * 0.25f);

	const AActor* Owner = GetOwner();
	const FVector RightVector = Owner ? Owner->GetActorRightVector() : FVector::RightVector;
	const FVector ForwardVector = Owner ? Owner->GetActorForwardVector() : FVector::ForwardVector;
	const FVector UpVector = Owner ? Owner->GetActorUpVector() : FVector::UpVector;

	FVector Location = ClatterBaseTransform.GetLocation();
	Location += RightVector * (Wave * ClatterLocationAmplitude * Decay);
	Location += ForwardVector * (SecondaryWave * ClatterLocationAmplitude * 0.35f * Decay);
	Location += UpVector * (FMath::Abs(SecondaryWave) * ClatterLocationAmplitude * 0.25f * Decay);

	FRotator Rotation = ClatterBaseTransform.GetRotation().Rotator();
	Rotation.Roll += Wave * ClatterRotationAmplitude * Decay;
	Rotation.Pitch += SecondaryWave * ClatterRotationAmplitude * 0.35f * Decay;
	Rotation.Yaw += SecondaryWave * ClatterRotationAmplitude * 0.25f * Decay;

	SelectedFoodActor->SetActorLocationAndRotation(Location, Rotation);

	if (Alpha >= 1.0f)
	{
		StopClatterMotion(true);
	}
}

bool UDiningTableMealComponent::OpenMealPicker(APlayerController* RequestingController, UObject* InventoryProvider)
{
	UE_LOG(LogDiningTableMeal, Log, TEXT("OpenMealPicker called. Owner=%s Component=%s RequestingController=%s InventoryProvider=%s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(this),
		*GetNameSafe(RequestingController),
		*GetNameSafe(InventoryProvider));

	if (!ResolveReferences())
	{
		UE_LOG(LogDiningTableMeal, Warning, TEXT("OpenMealPicker failed: references not resolved. Owner=%s ItemDataTable=%s MealPickerComponent=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(ItemDataTable),
			*GetNameSafe(MealPickerComponent));
		return false;
	}

	APlayerController* Controller = ResolveController(RequestingController);
	UObject* ResolvedInventoryProvider = ResolveInventoryProvider(Controller, InventoryProvider);
	if (!ResolvedInventoryProvider)
	{
		UE_LOG(LogDiningTableMeal, Warning, TEXT("OpenMealPicker failed: inventory provider missing. Owner=%s Controller=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Controller));
		return false;
	}

	ActiveController = Controller;
	ActiveInventoryProvider = ResolvedInventoryProvider;
	bPickerOpen = true;

	SetDiningOwnerOutlineSuppressed(this, true);
	EnableWorldClickEventsForSession(Controller);
	ConfigurePickerLayout();
	SetMealPickerVisible(true);
	BindCloseButton();
	UScrollBox* IngredientScrollBox = ConfigureIngredientScrollBox();

	if (UUserWidget* PickerWidget = GetPickerWidget())
	{
		PickerWidget->SetIsFocusable(true);
	}

	RefreshMealPicker();
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

void UDiningTableMealComponent::CloseMealPicker(APlayerController* RequestingController)
{
	if (!bPickerOpen)
	{
		SetDiningOwnerOutlineSuppressed(this, false);
		UE_LOG(LogDiningTableMeal, Warning, TEXT("CloseMealPicker ignored: picker is already closed. Owner=%s Controller=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(RequestingController));
		return;
	}

	APlayerController* Controller = ResolveController(RequestingController);
	if (!Controller)
	{
		Controller = ActiveController.Get();
	}

	bool bCameraReturned = false;
	if (bCallOwnerReturnCameraOnClose)
	{
		bCameraReturned = TryCallOwnerReturnCameraFunction(Controller);
	}

	EntryClickHandlers.Reset();
	ActiveInventoryProvider = nullptr;
	bPickerOpen = false;
	SetMealPickerVisible(false);
	SetDiningOwnerOutlineSuppressed(this, false);
	RestoreWorldClickEvents();

	if (bClearPreviewOnClose)
	{
		ClearSelectedMealFood();
	}

	if (Controller && bSetGameOnlyInputOnClose)
	{
		FInputModeGameOnly InputMode;
		Controller->SetInputMode(InputMode);
		Controller->bShowMouseCursor = false;
	}

	if (bCallOwnerReturnCameraOnClose)
	{
		UE_LOG(LogDiningTableMeal, Log, TEXT("Meal close camera return: %s. Owner=%s Controller=%s"),
			bCameraReturned ? TEXT("ok") : TEXT("failed"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Controller));
	}

	ActiveController = nullptr;
}

bool UDiningTableMealComponent::RefreshMealPicker()
{
	if (!ResolveReferences() || !ActiveInventoryProvider)
	{
		return false;
	}

	UUserWidget* PickerWidget = GetPickerWidget();
	UPanelWidget* GridPanel = FindDiningWidgetByName<UPanelWidget>(PickerWidget, GridWidgetName);
	if (!PickerWidget || !GridPanel)
	{
		return false;
	}

	ConfigurePickerLayout();
	ConfigureIngredientScrollBox();

	TArray<FKitchenIngredientOption> Options;
	if (!UKitchenCraftBlueprintLibrary::GetKitchenFoodOptions(ItemDataTable, ActiveInventoryProvider, Options, true))
	{
		GridPanel->ClearChildren();
		EntryClickHandlers.Reset();
		return false;
	}

	GridPanel->ClearChildren();
	EntryClickHandlers.Reset();

	UE_LOG(LogDiningTableMeal, Warning, TEXT("Meal picker refresh: %d available food option(s), provider=%s"),
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
			WrapBoxSlot->SetPadding(ScaleDiningMargin(EntryPadding, GetPickerRenderScale()));
			WrapBoxSlot->SetFillEmptySpace(true);
			WrapBoxSlot->SetHorizontalAlignment(HAlign_Center);
			WrapBoxSlot->SetVerticalAlignment(VAlign_Center);
		}
	}

	return true;
}

bool UDiningTableMealComponent::SelectMealFood(FName ItemId)
{
	UE_LOG(LogDiningTableMeal, Warning, TEXT("SelectMealFood requested. Owner=%s ItemId=%s"),
		*GetNameSafe(GetOwner()),
		*ItemId.ToString());

	FKitchenIngredientOption Option;
	if (!ResolveFoodData(ItemId, Option))
	{
		UE_LOG(LogDiningTableMeal, Warning, TEXT("Food select failed: %s not in inventory food options. Owner=%s"),
			*ItemId.ToString(),
			*GetNameSafe(GetOwner()));
		return false;
	}

	return SelectMealFoodOption(Option);
}

bool UDiningTableMealComponent::SelectMealFoodOption(const FKitchenIngredientOption& Option)
{
	if (Option.ItemId.IsNone())
	{
		return false;
	}

	if (!SpawnSelectedFoodPreview(Option))
	{
		UE_LOG(LogDiningTableMeal, Warning, TEXT("Food spawn failed: check %s. Owner=%s ItemId=%s"),
			*FoodPlacementComponentName.ToString(),
			*GetNameSafe(GetOwner()),
			*Option.ItemId.ToString());
		return false;
	}

	SelectedFoodItemId = Option.ItemId;
	SelectedFoodOption = Option;
	OnMealFoodSelected.Broadcast(Option.ItemId, Option.ItemData);
	UE_LOG(LogDiningTableMeal, Warning, TEXT("Food selected: %s. Owner=%s Actor=%s StackIndex=%d Count=%d"),
		*Option.ItemId.ToString(),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(SelectedFoodActor),
		Option.InventoryStackIndex,
		Option.AvailableCount);
	return true;
}

void UDiningTableMealComponent::ClearSelectedMealFood()
{
	StopClatterMotion(false);
	SelectedFoodItemId = NAME_None;
	SelectedFoodOption = FKitchenIngredientOption();

	if (SelectedFoodActor)
	{
		SelectedFoodActor->Destroy();
		SelectedFoodActor = nullptr;
	}
	SelectedFoodClickBoxComponent = nullptr;
}

void UDiningTableMealComponent::SetMealPickerVisible(bool bVisible)
{
	if (AActor* Owner = GetOwner())
	{
		ResolvePickerComponent(Owner);
		ConfigurePickerComponent();
	}

	if (!MealPickerComponent)
	{
		return;
	}

	MealPickerComponent->SetVisibility(bVisible, true);
	MealPickerComponent->SetHiddenInGame(!bVisible, true);

	if (!bVisible)
	{
		HideEntryTooltip();
	}
}

void UDiningTableMealComponent::ShowEntryTooltip(const FText& TooltipText)
{
	if (!bUseFixedEntryTooltip)
	{
		return;
	}

	UUserWidget* PickerWidget = GetPickerWidget();
	const FVector2D TooltipPosition = GetDiningFixedTooltipRightPosition(
		PickerPanelPosition,
		PickerPanelSize,
		FixedEntryTooltipPosition);
	UBorder* TooltipBorder = FindOrCreateDiningFixedTooltip(
		PickerWidget,
		TooltipPosition,
		FixedEntryTooltipSize,
		FixedEntryTooltipFontSize,
		GetPickerRenderScale());
	UTextBlock* TooltipLabel = PickerWidget && PickerWidget->WidgetTree
		? Cast<UTextBlock>(PickerWidget->WidgetTree->FindWidget(DiningFixedTooltipTextName))
		: nullptr;
	if (!TooltipBorder || !TooltipLabel)
	{
		return;
	}

	TooltipLabel->SetText(TooltipText);
	TooltipBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UDiningTableMealComponent::HideEntryTooltip()
{
	UUserWidget* PickerWidget = GetPickerWidget();
	UBorder* TooltipBorder = PickerWidget && PickerWidget->WidgetTree
		? Cast<UBorder>(PickerWidget->WidgetTree->FindWidget(DiningFixedTooltipBorderName))
		: nullptr;
	if (TooltipBorder)
	{
		TooltipBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UDiningTableMealComponent::HandleCloseClicked()
{
	HandleCloseButtonActivated(TEXT("OnClicked"));
}

void UDiningTableMealComponent::HandleClosePressed()
{
	HandleCloseButtonActivated(TEXT("OnPressed"));
}

void UDiningTableMealComponent::HandleCloseButtonActivated(const TCHAR* SourceEventName)
{
	UE_LOG(LogDiningTableMeal, Warning, TEXT("Meal close button activated: %s. Owner=%s Controller=%s bPickerOpen=%s"),
		SourceEventName ? SourceEventName : TEXT("Unknown"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(ActiveController.Get()),
		bPickerOpen ? TEXT("true") : TEXT("false"));
	CloseMealPicker(ActiveController.Get());
}

void UDiningTableMealComponent::HandleSelectedFoodClicked(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed)
{
	if (!bConsumeFoodOnPreviewClick || !SelectedFoodActor || SelectedFoodItemId.IsNone() || bConsumeAfterClatter)
	{
		return;
	}

	APlayerController* Controller = ResolveController(ActiveController.Get());
	UObject* ResolvedInventoryProvider = ResolveInventoryProvider(Controller, ActiveInventoryProvider.Get());
	if (!ResolvedInventoryProvider)
	{
		UE_LOG(LogDiningTableMeal, Warning, TEXT("Could not consume selected meal: inventory provider not found. ItemId=%s"),
			*SelectedFoodItemId.ToString());
		return;
	}

	ActiveController = Controller;
	ActiveInventoryProvider = ResolvedInventoryProvider;

	if (bClatterOnConsumeClick)
	{
		StartClatterMotion(true);
		return;
	}

	ConsumeSelectedMealFood();
}

bool UDiningTableMealComponent::ResolveReferences()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	ResolvePickerComponent(Owner);
	ResolveFoodPlacementComponent(Owner);
	ConfigurePickerComponent();
	return ItemDataTable && MealPickerComponent;
}

void UDiningTableMealComponent::ResolvePickerComponent(AActor* Owner)
{
	if (!Owner || MealPickerComponent)
	{
		return;
	}

	TArray<UWidgetComponent*> WidgetComponents;
	Owner->GetComponents<UWidgetComponent>(WidgetComponents);

	for (UWidgetComponent* WidgetComponent : WidgetComponents)
	{
		if (WidgetComponent && WidgetComponent->GetFName() == MealPickerComponentName)
		{
			MealPickerComponent = WidgetComponent;
			return;
		}
	}

	if (WidgetComponents.Num() == 1)
	{
		MealPickerComponent = WidgetComponents[0];
	}
}

void UDiningTableMealComponent::ResolveFoodPlacementComponent(AActor* Owner)
{
	if (!Owner || FoodPlacementComponent || FoodPlacementComponentName.IsNone())
	{
		return;
	}

	TArray<USceneComponent*> SceneComponents;
	Owner->GetComponents<USceneComponent>(SceneComponents);
	for (USceneComponent* SceneComponent : SceneComponents)
	{
		if (SceneComponent && SceneComponent->GetFName() == FoodPlacementComponentName)
		{
			FoodPlacementComponent = SceneComponent;
			return;
		}
	}
}

APlayerController* UDiningTableMealComponent::ResolveController(APlayerController* RequestingController) const
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

UObject* UDiningTableMealComponent::ResolveInventoryProvider(APlayerController* RequestingController, UObject* ProvidedInventoryProvider) const
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

void UDiningTableMealComponent::ConfigurePickerComponent()
{
	if (!MealPickerComponent)
	{
		return;
	}

	if (bForcePickerDrawSize)
	{
		const float TooltipAreaWidth = bUseFixedEntryTooltip
			? GetDiningFixedTooltipAreaWidth(FixedEntryTooltipPosition, FixedEntryTooltipSize)
			: 0.0f;
		const float RenderScale = GetPickerRenderScale();
		const FVector2D EffectiveDrawSize(PickerDrawSize.X + TooltipAreaWidth, PickerDrawSize.Y);
		MealPickerComponent->SetDrawSize(EffectiveDrawSize * RenderScale);
		ApplyPickerComponentRenderScale();
		if (bUseFixedEntryTooltip && EffectiveDrawSize.X > KINDA_SMALL_NUMBER)
		{
			const float PivotX = (PickerDrawSize.X * 0.5f) / EffectiveDrawSize.X;
			MealPickerComponent->SetPivot(FVector2D(PivotX, 0.5f));
		}
	}

	if (FBoolProperty* ReceiveHardwareInputProperty = FindFProperty<FBoolProperty>(MealPickerComponent->GetClass(), TEXT("bReceiveHardwareInput")))
	{
		ReceiveHardwareInputProperty->SetPropertyValue_InContainer(MealPickerComponent, true);
	}

	if (FBoolProperty* WindowFocusableProperty = FindFProperty<FBoolProperty>(MealPickerComponent->GetClass(), TEXT("bWindowFocusable")))
	{
		WindowFocusableProperty->SetPropertyValue_InContainer(MealPickerComponent, true);
	}
}

float UDiningTableMealComponent::GetPickerRenderScale() const
{
	return bForcePickerDrawSize ? FMath::Max(1.0f, PickerRenderScale) : 1.0f;
}

void UDiningTableMealComponent::ApplyPickerComponentRenderScale()
{
	UWidgetComponent* PickerComponent = MealPickerComponent.Get();
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

void UDiningTableMealComponent::ConfigurePickerRootRenderScale(UUserWidget* PickerWidget) const
{
	if (!PickerWidget)
	{
		return;
	}

	PickerWidget->SetRenderTransformPivot(FVector2D::ZeroVector);
	PickerWidget->SetRenderScale(FVector2D(1.0f, 1.0f));
}

UUserWidget* UDiningTableMealComponent::GetPickerWidget() const
{
	if (!MealPickerComponent)
	{
		return nullptr;
	}

	MealPickerComponent->InitWidget();
	UUserWidget* PickerWidget = MealPickerComponent->GetUserWidgetObject();
	ConfigurePickerRootRenderScale(PickerWidget);
	return PickerWidget;
}

bool UDiningTableMealComponent::BindCloseButton()
{
	UButton* CloseButton = FindDiningWidgetByName<UButton>(GetPickerWidget(), CloseButtonName);
	if (!CloseButton)
	{
		UE_LOG(LogDiningTableMeal, Warning, TEXT("Could not bind meal close button. Owner=%s Widget=%s CloseButtonName=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(GetPickerWidget()),
			*CloseButtonName.ToString());
		return false;
	}

	ConfigureCloseButtonVisuals();
	CloseButton->OnClicked.RemoveDynamic(this, &UDiningTableMealComponent::HandleCloseClicked);
	CloseButton->OnPressed.RemoveDynamic(this, &UDiningTableMealComponent::HandleClosePressed);
	CloseButton->OnClicked.AddDynamic(this, &UDiningTableMealComponent::HandleCloseClicked);
	CloseButton->OnPressed.AddDynamic(this, &UDiningTableMealComponent::HandleClosePressed);
	UE_LOG(LogDiningTableMeal, Log, TEXT("Bound meal close button. Owner=%s Widget=%s Button=%s Visibility=%d IsEnabled=%s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(GetPickerWidget()),
		*GetNameSafe(CloseButton),
		static_cast<int32>(CloseButton->GetVisibility()),
		CloseButton->GetIsEnabled() ? TEXT("true") : TEXT("false"));
	return true;
}

void UDiningTableMealComponent::ConfigurePickerLayout()
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
	if (UWidget* PanelBackground = FindDiningWidgetByName<UWidget>(PickerWidget, PanelBackgroundName))
	{
		PanelBackground->SetClipping(EWidgetClipping::ClipToBounds);

		if (UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(PanelBackground->Slot))
		{
			PanelSlot->SetAnchors(FAnchors(0.0f, 0.0f));
			PanelSlot->SetAlignment(FVector2D::ZeroVector);
			PanelSlot->SetAutoSize(false);
			PanelSlot->SetPosition(ScaleDiningVector(PickerPanelPosition, RenderScale));
			PanelSlot->SetSize(ScaleDiningVector(PickerPanelSize, RenderScale));
			PanelSlot->SetZOrder(0);
		}
	}

	if (bUseFixedEntryTooltip)
	{
		const FVector2D TooltipPosition = GetDiningFixedTooltipRightPosition(
			PickerPanelPosition,
			PickerPanelSize,
			FixedEntryTooltipPosition);
		FindOrCreateDiningFixedTooltip(
			PickerWidget,
			TooltipPosition,
			FixedEntryTooltipSize,
			FixedEntryTooltipFontSize,
			RenderScale);
	}

	ConfigureIngredientScrollBox();
	ConfigureCloseButtonVisuals();
}

UScrollBox* UDiningTableMealComponent::ConfigureIngredientScrollBox()
{
	UScrollBox* IngredientScrollBox = FindDiningWidgetByName<UScrollBox>(GetPickerWidget(), IngredientScrollBoxName);
	if (!IngredientScrollBox)
	{
		return nullptr;
	}

	const float ScrollBoxWidth = FMath::Max(1.0f, PickerPanelSize.X - IngredientScrollBoxPadding.Left - IngredientScrollBoxPadding.Right);
	const float ScrollBoxHeight = FMath::Max(1.0f, PickerPanelSize.Y - IngredientScrollBoxPadding.Top - IngredientScrollBoxPadding.Bottom);
	const float RenderScale = GetPickerRenderScale();
	const FVector2D ScrollBoxPosition(IngredientScrollBoxPadding.Left, IngredientScrollBoxPadding.Top);
	const FVector2D ScrollBoxSize(ScrollBoxWidth, ScrollBoxHeight);

	IngredientScrollBox->SetClipping(EWidgetClipping::ClipToBounds);
	IngredientScrollBox->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
	IngredientScrollBox->SetScrollBarVisibility(bShowIngredientScrollBar ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	IngredientScrollBox->SetAlwaysShowScrollbar(bShowIngredientScrollBar);
	IngredientScrollBox->SetAllowOverscroll(false);
	IngredientScrollBox->SetAnimateWheelScrolling(true);
	IngredientScrollBox->SetWheelScrollMultiplier(IngredientWheelScrollMultiplier);

	if (UPanelWidget* GridPanel = FindDiningWidgetByName<UPanelWidget>(GetPickerWidget(), GridWidgetName))
	{
		GridPanel->SetClipping(EWidgetClipping::ClipToBounds);
	}

	if (UCanvasPanelSlot* ScrollCanvasSlot = Cast<UCanvasPanelSlot>(IngredientScrollBox->Slot))
	{
		ScrollCanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		ScrollCanvasSlot->SetAlignment(FVector2D::ZeroVector);
		ScrollCanvasSlot->SetAutoSize(false);
		ScrollCanvasSlot->SetPosition(ScaleDiningVector(ScrollBoxPosition, RenderScale));
		ScrollCanvasSlot->SetSize(ScaleDiningVector(ScrollBoxSize, RenderScale));
	}
	else if (UOverlaySlot* ScrollOverlaySlot = Cast<UOverlaySlot>(IngredientScrollBox->Slot))
	{
		ScrollOverlaySlot->SetHorizontalAlignment(HAlign_Fill);
		ScrollOverlaySlot->SetVerticalAlignment(VAlign_Fill);
		ScrollOverlaySlot->SetPadding(ScaleDiningMargin(IngredientScrollBoxPadding, RenderScale));
	}
	else if (UVerticalBoxSlot* ScrollVerticalSlot = Cast<UVerticalBoxSlot>(IngredientScrollBox->Slot))
	{
		ScrollVerticalSlot->SetHorizontalAlignment(HAlign_Fill);
		ScrollVerticalSlot->SetVerticalAlignment(VAlign_Fill);
		ScrollVerticalSlot->SetPadding(ScaleDiningMargin(IngredientScrollBoxPadding, RenderScale));
		ScrollVerticalSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	else if (UBorderSlot* ScrollBorderSlot = Cast<UBorderSlot>(IngredientScrollBox->Slot))
	{
		ScrollBorderSlot->SetHorizontalAlignment(HAlign_Fill);
		ScrollBorderSlot->SetVerticalAlignment(VAlign_Fill);
		ScrollBorderSlot->SetPadding(ScaleDiningMargin(IngredientScrollBoxPadding, RenderScale));
	}

	return IngredientScrollBox;
}

void UDiningTableMealComponent::ConfigureCloseButtonVisuals()
{
	UUserWidget* PickerWidget = GetPickerWidget();
	UButton* CloseButton = FindDiningWidgetByName<UButton>(PickerWidget, CloseButtonName);
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
		CloseSlot->SetPosition(ScaleDiningVector(CloseButtonPosition, RenderScale));
		CloseSlot->SetSize(ScaleDiningVector(CloseButtonSize, RenderScale));
		CloseSlot->SetZOrder(100);
	}

	if (UTextBlock* ExistingLabel = Cast<UTextBlock>(CloseButton->GetContent()))
	{
		ExistingLabel->SetText(CloseButtonText);
		ExistingLabel->SetJustification(ETextJustify::Center);
		ExistingLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		return;
	}

	if (PickerWidget->WidgetTree && !CloseButtonText.IsEmpty())
	{
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

USizeBox* UDiningTableMealComponent::CreateSizedEntryWidget(UUserWidget* PickerWidget, UUserWidget* EntryWidget)
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

	const FVector2D ScaledEntryWidgetSize = ScaleDiningVector(EntryWidgetSize, GetPickerRenderScale());
	EntrySizeBox->SetWidthOverride(ScaledEntryWidgetSize.X);
	EntrySizeBox->SetHeightOverride(ScaledEntryWidgetSize.Y);
	EntrySizeBox->SetMinDesiredWidth(ScaledEntryWidgetSize.X);
	EntrySizeBox->SetMinDesiredHeight(ScaledEntryWidgetSize.Y);
	EntrySizeBox->SetClipping(EWidgetClipping::ClipToBounds);
	EntryWidget->SetClipping(EWidgetClipping::ClipToBounds);
	EntrySizeBox->AddChild(EntryWidget);
	return EntrySizeBox;
}

bool UDiningTableMealComponent::ConfigureEntryWidget(UUserWidget* EntryWidget, const FKitchenIngredientOption& Option)
{
	if (!EntryWidget)
	{
		return false;
	}

	const float RenderScale = GetPickerRenderScale();

	if (UImage* IconImage = FindDiningWidgetByName<UImage>(EntryWidget, EntryIconImageName))
	{
		IconImage->SetBrushFromTexture(Option.ItemData.IconTexture, false);
		IconImage->SetDesiredSizeOverride(EntryIconSize);
		IconImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		IconImage->SetRenderScale(FVector2D(RenderScale * DiningEntryIconVisualScale, RenderScale * DiningEntryIconVisualScale));

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

	if (UTextBlock* CountText = FindDiningWidgetByName<UTextBlock>(EntryWidget, EntryCountTextName))
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
		const FMargin ScaledCountPadding = ScaleDiningMargin(EffectiveCountPadding, RenderScale);

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

	if (UButton* SelectButton = FindDiningWidgetByName<UButton>(EntryWidget, EntrySelectButtonName))
	{
		UDiningMealEntryClickHandler* Handler = NewObject<UDiningMealEntryClickHandler>(this);
		Handler->Initialize(this, Option);
		SelectButton->OnHovered.RemoveDynamic(Handler, &UDiningMealEntryClickHandler::HandleHovered);
		SelectButton->OnUnhovered.RemoveDynamic(Handler, &UDiningMealEntryClickHandler::HandleUnhovered);
		SelectButton->OnClicked.RemoveDynamic(Handler, &UDiningMealEntryClickHandler::HandleClicked);
		SelectButton->OnPressed.RemoveDynamic(Handler, &UDiningMealEntryClickHandler::HandlePressed);
		SelectButton->OnHovered.AddDynamic(Handler, &UDiningMealEntryClickHandler::HandleHovered);
		SelectButton->OnUnhovered.AddDynamic(Handler, &UDiningMealEntryClickHandler::HandleUnhovered);
		SelectButton->OnClicked.AddDynamic(Handler, &UDiningMealEntryClickHandler::HandleClicked);
		SelectButton->OnPressed.AddDynamic(Handler, &UDiningMealEntryClickHandler::HandlePressed);
		EntryClickHandlers.Add(Handler);
		UE_LOG(LogDiningTableMeal, Warning, TEXT("Bound meal entry button. ItemId=%s StackIndex=%d Count=%d Button=%s Visibility=%d IsEnabled=%s Icon=%s Mesh=%s"),
			*Option.ItemId.ToString(),
			Option.InventoryStackIndex,
			Option.AvailableCount,
			*GetNameSafe(SelectButton),
			static_cast<int32>(SelectButton->GetVisibility()),
			SelectButton->GetIsEnabled() ? TEXT("true") : TEXT("false"),
			*GetNameSafe(Option.ItemData.IconTexture),
			*GetNameSafe(Option.ItemData.IngredientMesh));
	}
	else
	{
		UE_LOG(LogDiningTableMeal, Warning, TEXT("Could not bind meal entry button. ItemId=%s EntryWidget=%s ButtonName=%s"),
			*Option.ItemId.ToString(),
			*GetNameSafe(EntryWidget),
			*EntrySelectButtonName.ToString());
	}

	return true;
}

bool UDiningTableMealComponent::ResolveFoodData(FName ItemId, FKitchenIngredientOption& OutOption) const
{
	if (ItemId.IsNone() || !ItemDataTable || !ActiveInventoryProvider)
	{
		UE_LOG(LogDiningTableMeal, Warning, TEXT("Could not resolve food data. ItemId=%s ItemDataTable=%s InventoryProvider=%s"),
			*ItemId.ToString(),
			*GetNameSafe(ItemDataTable),
			*GetNameSafe(ActiveInventoryProvider.Get()));
		return false;
	}

	TArray<FKitchenIngredientOption> Options;
	if (!UKitchenCraftBlueprintLibrary::GetKitchenFoodOptions(ItemDataTable, ActiveInventoryProvider, Options, true))
	{
		UE_LOG(LogDiningTableMeal, Warning, TEXT("Could not resolve food data: GetKitchenFoodOptions failed. ItemId=%s Table=%s Provider=%s"),
			*ItemId.ToString(),
			*GetNameSafe(ItemDataTable),
			*GetNameSafe(ActiveInventoryProvider.Get()));
		return false;
	}

	for (const FKitchenIngredientOption& Option : Options)
	{
		if (Option.ItemId == ItemId)
		{
			OutOption = Option;
			return true;
		}
	}

	UE_LOG(LogDiningTableMeal, Warning, TEXT("Could not resolve food data: selected item is no longer available. ItemId=%s Options=%d"),
		*ItemId.ToString(),
		Options.Num());
	return false;
}

bool UDiningTableMealComponent::SpawnSelectedFoodPreview(const FKitchenIngredientOption& Option)
{
	UWorld* World = GetWorld();
	if (!World || Option.ItemId.IsNone())
	{
		UE_LOG(LogDiningTableMeal, Warning, TEXT("Could not spawn selected food preview: invalid world or item id. World=%s ItemId=%s"),
			*GetNameSafe(World),
			*Option.ItemId.ToString());
		return false;
	}

	ResolveFoodPlacementComponent(GetOwner());
	if (!FoodPlacementComponent)
	{
		UE_LOG(LogDiningTableMeal, Warning, TEXT("Could not spawn selected food preview: FoodPlacementComponent not found. Owner=%s ComponentName=%s"),
			*GetNameSafe(GetOwner()),
			*FoodPlacementComponentName.ToString());
		return false;
	}

	TSubclassOf<AKitchenIngredientActor> ActorClass = Option.ItemData.IngredientActorClass ? Option.ItemData.IngredientActorClass : DefaultFoodActorClass;
	if (!ActorClass)
	{
		ActorClass = GetFallbackDiningFoodActorClass();
	}
	if (!ActorClass)
	{
		ActorClass = AKitchenIngredientActor::StaticClass();
	}

	StopClatterMotion(false);
	if (SelectedFoodActor)
	{
		SelectedFoodActor->Destroy();
		SelectedFoodActor = nullptr;
	}
	SelectedFoodClickBoxComponent = nullptr;

	const FTransform SpawnTransform = GetFoodPlacementTransform();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AKitchenIngredientActor* FoodActor = World->SpawnActor<AKitchenIngredientActor>(ActorClass, SpawnTransform, SpawnParams);
	if (!FoodActor)
	{
		UE_LOG(LogDiningTableMeal, Warning, TEXT("Could not spawn selected food preview actor. ItemId=%s Class=%s Location=%s"),
			*Option.ItemId.ToString(),
			*GetNameSafe(ActorClass.Get()),
			*SpawnTransform.GetLocation().ToString());
		return false;
	}

	FKitchenCraftedItem CraftedItem;
	CraftedItem.ItemId = Option.ItemId;
	CraftedItem.Count = 1;
	CraftedItem.StatBonus = Option.bHasInventoryStatBonus ? Option.InventoryStatBonus : Option.ItemData.BaseStatBonus;
	FoodActor->InitializeCraftResult(nullptr, CraftedItem, Option.ItemData.IconTexture, Option.ItemData.IngredientMesh);

	if (FoodActor->MeshComponent)
	{
		if (!FoodActor->MeshComponent->GetStaticMesh())
		{
			if (UStaticMesh* FallbackMesh = GetFallbackDiningIconMesh())
			{
				FoodActor->MeshComponent->SetStaticMesh(FallbackMesh);
			}
		}

		if (!FoodActor->MeshComponent->GetMaterial(0))
		{
			if (UMaterialInterface* FallbackMaterial = GetFallbackDiningIconMaterial())
			{
				FoodActor->MeshComponent->SetMaterial(0, FallbackMaterial);
			}
		}

		FoodActor->ApplyIconTextureToMeshMaterial();
		FoodActor->MeshComponent->SetVisibility(true, true);
		FoodActor->MeshComponent->SetHiddenInGame(false, true);
		FoodActor->MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		FoodActor->MeshComponent->OnClicked.RemoveDynamic(this, &UDiningTableMealComponent::HandleSelectedFoodClicked);
		FoodActor->MeshComponent->OnClicked.AddDynamic(this, &UDiningTableMealComponent::HandleSelectedFoodClicked);
	}

	if (FoodActor->ClickCollisionComponent)
	{
		FoodActor->ClickCollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	FoodActor->SetActorTransform(SpawnTransform);
	FoodActor->SetActorHiddenInGame(false);
	FoodActor->SetActorEnableCollision(true);
	ConfigureSelectedFoodClickBox(FoodActor);

	if (bAttachPreviewToTable && GetOwner())
	{
		FoodActor->AttachToActor(GetOwner(), FAttachmentTransformRules::KeepWorldTransform);
	}

	SelectedFoodActor = FoodActor;
	StartClatterMotion();
	UE_LOG(LogDiningTableMeal, Warning, TEXT("Spawned table meal preview. ItemId=%s Actor=%s Class=%s Placement=%s Location=%s Scale=%s Icon=%s Mesh=%s Material=%s"),
		*Option.ItemId.ToString(),
		*GetNameSafe(FoodActor),
		*GetNameSafe(ActorClass.Get()),
		*GetNameSafe(FoodPlacementComponent.Get()),
		*SpawnTransform.GetLocation().ToString(),
		*SpawnTransform.GetScale3D().ToString(),
		*GetNameSafe(Option.ItemData.IconTexture),
		*GetNameSafe(FoodActor->MeshComponent ? FoodActor->MeshComponent->GetStaticMesh() : nullptr),
		*GetNameSafe(FoodActor->MeshComponent ? FoodActor->MeshComponent->GetMaterial(0) : nullptr));
	return true;
}

FTransform UDiningTableMealComponent::GetFoodPlacementTransform() const
{
	if (FoodPlacementComponent)
	{
		return FTransform(
			FoodPlacementComponent->GetComponentRotation(),
			FoodPlacementComponent->GetComponentLocation(),
			FoodPlacementComponent->GetComponentScale() * FoodScale);
	}

	return FTransform::Identity;
}

void UDiningTableMealComponent::ConfigureSelectedFoodClickBox(AKitchenIngredientActor* FoodActor)
{
	SelectedFoodClickBoxComponent = nullptr;

	if (!bUseFoodPreviewClickBox || !FoodActor)
	{
		return;
	}

	USceneComponent* AttachParent = FoodActor->MeshComponent ? FoodActor->MeshComponent.Get() : FoodActor->GetRootComponent();
	if (!AttachParent)
	{
		return;
	}

	UKitchenClickCollisionComponent* ClickBox = NewObject<UKitchenClickCollisionComponent>(FoodActor, TEXT("DiningFoodPreviewClickBox"));
	if (!ClickBox)
	{
		return;
	}

	ClickBox->SetupAttachment(AttachParent);
	FoodActor->AddInstanceComponent(ClickBox);
	ClickBox->SetRelativeLocation(FoodPreviewClickBoxOffset, false, nullptr, ETeleportType::TeleportPhysics);
	ClickBox->SetRelativeRotation(FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
	ClickBox->SetBoxExtent(FVector(
		FMath::Max(0.1f, FoodPreviewClickBoxExtent.X),
		FMath::Max(0.1f, FoodPreviewClickBoxExtent.Y),
		FMath::Max(0.1f, FoodPreviewClickBoxExtent.Z)), false);
	ClickBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ClickBox->SetCollisionObjectType(ECC_WorldDynamic);
	ClickBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	ClickBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ClickBox->SetGenerateOverlapEvents(false);
	ClickBox->SetVisibility(false, true);
	ClickBox->SetHiddenInGame(true);
	ClickBox->RegisterComponent();
	ClickBox->OnClicked.AddDynamic(this, &UDiningTableMealComponent::HandleSelectedFoodClicked);

	SelectedFoodClickBoxComponent = ClickBox;
}

void UDiningTableMealComponent::StartClatterMotion(bool bInConsumeAfterClatter)
{
	const bool bShouldPlayClatter = bInConsumeAfterClatter ? bClatterOnConsumeClick : bClatterOnSelect;
	if (!bShouldPlayClatter || !SelectedFoodActor)
	{
		StopClatterMotion(false);
		if (bInConsumeAfterClatter)
		{
			ConsumeSelectedMealFood();
		}
		return;
	}

	ClatterBaseTransform = SelectedFoodActor->GetActorTransform();
	ClatterElapsed = 0.0f;
	bClatterActive = true;
	bConsumeAfterClatter = bInConsumeAfterClatter;
	SetComponentTickEnabled(true);
}

void UDiningTableMealComponent::StopClatterMotion(bool bSnapToBase)
{
	const bool bShouldConsume = bConsumeAfterClatter && bSnapToBase;

	if (bSnapToBase && SelectedFoodActor)
	{
		SelectedFoodActor->SetActorTransform(ClatterBaseTransform);
	}

	bClatterActive = false;
	bConsumeAfterClatter = false;
	ClatterElapsed = 0.0f;
	SetComponentTickEnabled(false);

	if (bShouldConsume)
	{
		ConsumeSelectedMealFood();
	}
}

bool UDiningTableMealComponent::ConsumeSelectedMealFood()
{
	if (SelectedFoodItemId.IsNone())
	{
		return false;
	}

	APlayerController* Controller = ResolveController(ActiveController.Get());
	UObject* ResolvedInventoryProvider = ResolveInventoryProvider(Controller, ActiveInventoryProvider.Get());
	if (!ResolvedInventoryProvider)
	{
		UE_LOG(LogDiningTableMeal, Warning, TEXT("Could not consume selected meal: inventory provider not found. ItemId=%s"),
			*SelectedFoodItemId.ToString());
		return false;
	}

	ActiveController = Controller;
	ActiveInventoryProvider = ResolvedInventoryProvider;

	const FKitchenIngredientOption OptionToConsume = SelectedFoodOption;
	if (!TryConsumeMealThroughInventory(ResolvedInventoryProvider, OptionToConsume, Controller))
	{
		UE_LOG(LogDiningTableMeal, Warning, TEXT("Table meal consume failed. Provider=%s ItemId=%s"),
			*GetNameSafe(ResolvedInventoryProvider),
			*SelectedFoodItemId.ToString());
		return false;
	}

	UE_LOG(LogDiningTableMeal, Log, TEXT("Consumed table meal. Provider=%s ItemId=%s"),
		*GetNameSafe(ResolvedInventoryProvider),
		*SelectedFoodItemId.ToString());

	if (bClearPreviewAfterConsume)
	{
		ClearSelectedMealFood();
	}

	if (bRefreshPickerAfterConsume && bPickerOpen)
	{
		RefreshMealPicker();
	}

	return true;
}

bool UDiningTableMealComponent::TryConsumeMealThroughInventory(UObject* InventoryProvider, const FKitchenIngredientOption& Option, APlayerController* RequestingController) const
{
	if (!InventoryProvider || Option.ItemId.IsNone())
	{
		return false;
	}

	FKitchenStatBonus StatBonusToApply;
	if (!UKitchenCraftBlueprintLibrary::ConsumeKitchenFoodStackForTable(InventoryProvider, Option.ItemId, Option.InventoryStackIndex, 1, Option.ItemData.BaseStatBonus, StatBonusToApply))
	{
		UE_LOG(LogDiningTableMeal, Warning, TEXT("Could not consume table meal through table inventory function. Provider=%s ItemId=%s"),
			*GetNameSafe(InventoryProvider),
			*Option.ItemId.ToString());
		return false;
	}

	UE_LOG(LogDiningTableMeal, Log, TEXT("Applying table meal stat bonus. Provider=%s ItemId=%s %s"),
		*GetNameSafe(InventoryProvider),
		*Option.ItemId.ToString(),
		*DescribeDiningStatBonus(StatBonusToApply));

	if (!TryApplyFoodStatBonus(RequestingController, StatBonusToApply))
	{
		FKitchenCraftedItem RestoreItem;
		RestoreItem.ItemId = Option.ItemId;
		RestoreItem.Count = 1;
		RestoreItem.StatBonus = StatBonusToApply;
		IKitchenInventoryProvider::Execute_AddKitchenCraftedItem(InventoryProvider, RestoreItem);
		UE_LOG(LogDiningTableMeal, Warning, TEXT("Could not consume table meal: stat application failed, restored item. Provider=%s ItemId=%s"),
			*GetNameSafe(InventoryProvider),
			*Option.ItemId.ToString());
		return false;
	}

	UE_LOG(LogDiningTableMeal, Log, TEXT("Consumed table meal through inventory interface. Provider=%s ItemId=%s Count=%d"),
		*GetNameSafe(InventoryProvider),
		*Option.ItemId.ToString(),
		1);
	return true;
}

bool UDiningTableMealComponent::TryApplyFoodStatBonus(APlayerController* RequestingController, const FKitchenStatBonus& StatBonus) const
{
	APlayerController* Controller = ResolveController(RequestingController);
	APawn* ConsumerPawn = Controller ? Controller->GetPawn() : nullptr;
	if (!ConsumerPawn)
	{
		UE_LOG(LogDiningTableMeal, Warning, TEXT("Could not apply food stat bonus: consumer pawn missing. Controller=%s"),
			*GetNameSafe(Controller));
		return false;
	}

	UFunction* ApplyFunction = nullptr;
	const FName CandidateNames[] =
	{
		ApplyFoodStatBonusFunctionName,
		FName(TEXT("ApplyFoodStatBonus")),
		FName(TEXT("Apply Food Stat Bonus"))
	};

	for (const FName CandidateName : CandidateNames)
	{
		if (CandidateName.IsNone())
		{
			continue;
		}

		ApplyFunction = ConsumerPawn->FindFunction(CandidateName);
		if (ApplyFunction)
		{
			break;
		}
	}

	if (!ApplyFunction)
	{
		UE_LOG(LogDiningTableMeal, Warning, TEXT("Could not apply food stat bonus: function not found. Pawn=%s Function=%s"),
			*GetNameSafe(ConsumerPawn),
			*ApplyFoodStatBonusFunctionName.ToString());
		return false;
	}

	TArray<FProperty*> ParamProperties;
	int32 InputParamCount = 0;
	bool bWroteInput = false;
	for (TFieldIterator<FProperty> It(ApplyFunction); It; ++It)
	{
		FProperty* Property = *It;
		if (Property && Property->HasAnyPropertyFlags(CPF_Parm))
		{
			ParamProperties.Add(Property);
		}
	}

	uint8* Params = static_cast<uint8*>(FMemory_Alloca(ApplyFunction->ParmsSize));
	FMemory::Memzero(Params, ApplyFunction->ParmsSize);
	for (FProperty* Property : ParamProperties)
	{
		Property->InitializeValue_InContainer(Params);
	}

	for (FProperty* Property : ParamProperties)
	{
		const bool bReturnParam = Property->HasAnyPropertyFlags(CPF_ReturnParm);
		const bool bOutputOnlyParam = Property->HasAnyPropertyFlags(CPF_OutParm)
			&& !Property->HasAnyPropertyFlags(CPF_ReferenceParm);
		if (bReturnParam || bOutputOnlyParam)
		{
			continue;
		}

		++InputParamCount;
		bWroteInput |= SetDiningStatBonusInputParam(Property, Params, StatBonus);
	}

	if (InputParamCount > 0 && !bWroteInput)
	{
		for (FProperty* Property : ParamProperties)
		{
			Property->DestroyValue_InContainer(Params);
		}

		UE_LOG(LogDiningTableMeal, Warning, TEXT("Could not apply food stat bonus: unsupported input layout. Pawn=%s Function=%s Params=%s"),
			*GetNameSafe(ConsumerPawn),
			*ApplyFunction->GetName(),
			*DescribeDiningFunctionParams(ParamProperties));
		return false;
	}

	if (InputParamCount == 0)
	{
		for (FProperty* Property : ParamProperties)
		{
			Property->DestroyValue_InContainer(Params);
		}

		UE_LOG(LogDiningTableMeal, Warning, TEXT("Could not apply food stat bonus: function has no input params. Pawn=%s Function=%s"),
			*GetNameSafe(ConsumerPawn),
			*ApplyFunction->GetName());
		return false;
	}

	UE_LOG(LogDiningTableMeal, Log, TEXT("Calling character food stat function. Pawn=%s Function=%s %s"),
		*GetNameSafe(ConsumerPawn),
		*ApplyFunction->GetName(),
		*DescribeDiningStatBonus(StatBonus));

	ConsumerPawn->ProcessEvent(ApplyFunction, Params);

	bool bHasBoolResult = false;
	bool bBoolResult = true;
	for (FProperty* Property : ParamProperties)
	{
		const bool bIsReturnOrOut = Property->HasAnyPropertyFlags(CPF_ReturnParm) || Property->HasAnyPropertyFlags(CPF_OutParm);
		const bool bLooksLikeResult = Property->HasAnyPropertyFlags(CPF_ReturnParm)
			|| Property->GetName().Contains(TEXT("Success"), ESearchCase::IgnoreCase)
			|| Property->GetName().Contains(TEXT("Result"), ESearchCase::IgnoreCase);
		if (bIsReturnOrOut && bLooksLikeResult)
		{
			if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
			{
				bHasBoolResult = true;
				bBoolResult = BoolProperty->GetPropertyValue_InContainer(Params);
				break;
			}
		}
	}

	for (FProperty* Property : ParamProperties)
	{
		Property->DestroyValue_InContainer(Params);
	}

	const bool bApplied = bHasBoolResult ? bBoolResult : true;
	if (bApplied)
	{
		UE_LOG(LogDiningTableMeal, Log, TEXT("Applied food stat bonus to character. Pawn=%s Function=%s Result=%s HasBoolResult=%s %s"),
			*GetNameSafe(ConsumerPawn),
			*ApplyFunction->GetName(),
			bApplied ? TEXT("Success") : TEXT("Failure"),
			bHasBoolResult ? TEXT("true") : TEXT("false"),
			*DescribeDiningStatBonus(StatBonus));
	}
	else
	{
		UE_LOG(LogDiningTableMeal, Warning, TEXT("Food stat bonus function returned failure. Pawn=%s Function=%s %s"),
			*GetNameSafe(ConsumerPawn),
			*ApplyFunction->GetName(),
			*DescribeDiningStatBonus(StatBonus));
	}
	return bApplied;
}

bool UDiningTableMealComponent::TryCallOwnerReturnCameraFunction(APlayerController* RequestingController)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		UE_LOG(LogDiningTableMeal, Warning, TEXT("Could not return table meal camera: owner is missing."));
		return false;
	}

	APlayerController* Controller = ResolveController(RequestingController);
	if (UInteractionCameraBlendComponent* CameraBlendComponent = Owner->FindComponentByClass<UInteractionCameraBlendComponent>())
	{
		if (CameraBlendComponent->BlendBackToPlayer(Controller))
		{
			return true;
		}

		UE_LOG(LogDiningTableMeal, Warning, TEXT("Interaction camera blend back failed for table meal. Owner=%s Controller=%s Component=%s"),
			*GetNameSafe(Owner),
			*GetNameSafe(Controller),
			*GetNameSafe(CameraBlendComponent));
	}
	else
	{
		UE_LOG(LogDiningTableMeal, Warning, TEXT("Could not return table meal camera: InteractionCameraBlendComponent not found. Owner=%s"),
			*GetNameSafe(Owner));
	}

	if (!OwnerReturnCameraFunctionName.IsNone())
	{
		UFunction* ReturnCameraFunction = Owner->FindFunction(OwnerReturnCameraFunctionName);
		if (ReturnCameraFunction)
		{
			Owner->ProcessEvent(ReturnCameraFunction, nullptr);
			return true;
		}

		UE_LOG(LogDiningTableMeal, Warning, TEXT("Could not return table meal camera: function %s not found on owner %s"),
			*OwnerReturnCameraFunctionName.ToString(),
			*GetNameSafe(Owner));
	}

	return false;
}

void UDiningTableMealComponent::EnableWorldClickEventsForSession(APlayerController* RequestingController)
{
	if (!bEnableWorldClickEventsForSession || !RequestingController)
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

void UDiningTableMealComponent::RestoreWorldClickEvents()
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
