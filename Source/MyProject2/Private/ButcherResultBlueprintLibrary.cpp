#include "ButcherResultBlueprintLibrary.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateColor.h"
#include "UObject/UnrealType.h"

namespace
{
constexpr float ButcherResultEntrySize = 76.0f;
constexpr float ButcherResultGridWidth = 560.0f;
constexpr float ButcherResultGridHeight = 260.0f;
const FName NativeButcherGridName(TEXT("Native_ButcherDropGrid"));

struct FButcherResultDropItem
{
	FName ItemId = NAME_None;
	int32 Count = 0;
	FText DisplayName;
	TObjectPtr<UTexture2D> IconTexture = nullptr;

	bool IsValid() const
	{
		return !ItemId.IsNone() && Count > 0;
	}
};

FProperty* FindButcherProperty(const UStruct* Struct, const TCHAR* FieldName)
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

bool ReadButcherNameField(const UStruct* Struct, const void* Container, const TCHAR* FieldName, FName& OutValue)
{
	const FNameProperty* NameProperty = CastField<FNameProperty>(FindButcherProperty(Struct, FieldName));
	if (!NameProperty)
	{
		return false;
	}

	OutValue = NameProperty->GetPropertyValue_InContainer(Container);
	return true;
}

bool ReadButcherIntField(const UStruct* Struct, const void* Container, const TCHAR* FieldName, int32& OutValue)
{
	const FNumericProperty* NumericProperty = CastField<FNumericProperty>(FindButcherProperty(Struct, FieldName));
	if (!NumericProperty || !NumericProperty->IsInteger())
	{
		return false;
	}

	const void* ValuePtr = NumericProperty->ContainerPtrToValuePtr<void>(Container);
	OutValue = static_cast<int32>(NumericProperty->GetSignedIntPropertyValue(ValuePtr));
	return true;
}

bool ReadButcherTextField(const UStruct* Struct, const void* Container, const TCHAR* FieldName, FText& OutValue)
{
	const FTextProperty* TextProperty = CastField<FTextProperty>(FindButcherProperty(Struct, FieldName));
	if (!TextProperty)
	{
		return false;
	}

	const FText* TextValue = TextProperty->ContainerPtrToValuePtr<FText>(Container);
	if (!TextValue)
	{
		return false;
	}

	OutValue = *TextValue;
	return true;
}

template <typename ObjectType>
bool ReadButcherObjectField(const UStruct* Struct, const void* Container, const TCHAR* FieldName, TObjectPtr<ObjectType>& OutValue)
{
	const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(FindButcherProperty(Struct, FieldName));
	if (!ObjectProperty)
	{
		return false;
	}

	OutValue = Cast<ObjectType>(ObjectProperty->GetObjectPropertyValue_InContainer(Container));
	return true;
}

const uint8* FindButcherDataTableRow(const UDataTable* DataTable, FName RowName)
{
	if (!DataTable || RowName.IsNone())
	{
		return nullptr;
	}

	if (const uint8* const* RowPtr = DataTable->GetRowMap().Find(RowName))
	{
		return *RowPtr;
	}

	return nullptr;
}

UDataTable* ResolveButcherItemDataTable(UUserWidget* ResultWidget, UDataTable* ProvidedDataTable)
{
	if (ProvidedDataTable)
	{
		return ProvidedDataTable;
	}

	if (ResultWidget)
	{
		TObjectPtr<UDataTable> WidgetDataTable = nullptr;
		if (ReadButcherObjectField(ResultWidget->GetClass(), ResultWidget, TEXT("DT_ItemsRef"), WidgetDataTable) && WidgetDataTable)
		{
			return WidgetDataTable;
		}
	}

	return LoadObject<UDataTable>(nullptr, TEXT("/Game/Resource/DataTable/DT_Items.DT_Items"));
}

bool ReadButcherItemDefinition(UDataTable* ItemDataTable, FButcherResultDropItem& Item)
{
	const uint8* RowData = FindButcherDataTableRow(ItemDataTable, Item.ItemId);
	const UScriptStruct* RowStruct = ItemDataTable ? ItemDataTable->GetRowStruct() : nullptr;
	if (!RowData || !RowStruct)
	{
		Item.DisplayName = FText::FromName(Item.ItemId);
		return false;
	}

	if (!ReadButcherTextField(RowStruct, RowData, TEXT("DisplayName"), Item.DisplayName))
	{
		FName NameField;
		Item.DisplayName = ReadButcherNameField(RowStruct, RowData, TEXT("Name"), NameField)
			? FText::FromName(NameField)
			: FText::FromName(Item.ItemId);
	}

	ReadButcherObjectField(RowStruct, RowData, TEXT("IconTexture"), Item.IconTexture);
	return true;
}

bool ReadButcherDrops(UUserWidget* ResultWidget, UDataTable* ItemDataTable, TArray<FButcherResultDropItem>& OutItems)
{
	OutItems.Reset();
	if (!ResultWidget)
	{
		return false;
	}

	const FArrayProperty* DropsProperty = CastField<FArrayProperty>(FindButcherProperty(ResultWidget->GetClass(), TEXT("Drops")));
	const FStructProperty* DropStructProperty = DropsProperty ? CastField<FStructProperty>(DropsProperty->Inner) : nullptr;
	if (!DropsProperty || !DropStructProperty || !DropStructProperty->Struct)
	{
		return false;
	}

	const void* ArrayContainer = DropsProperty->ContainerPtrToValuePtr<void>(ResultWidget);
	FScriptArrayHelper ArrayHelper(DropsProperty, ArrayContainer);
	for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
	{
		const void* ElementContainer = ArrayHelper.GetRawPtr(Index);

		FButcherResultDropItem Item;
		ReadButcherNameField(DropStructProperty->Struct, ElementContainer, TEXT("ItemId"), Item.ItemId);
		if (!ReadButcherIntField(DropStructProperty->Struct, ElementContainer, TEXT("Count"), Item.Count))
		{
			Item.Count = 1;
		}

		if (Item.IsValid())
		{
			ReadButcherItemDefinition(ItemDataTable, Item);
			OutItems.Add(Item);
		}
	}

	return OutItems.Num() > 0;
}

template <typename WidgetType>
WidgetType* FindButcherWidgetByName(UUserWidget* RootWidget, FName WidgetName)
{
	if (!RootWidget || !RootWidget->WidgetTree || WidgetName.IsNone())
	{
		return nullptr;
	}

	return Cast<WidgetType>(RootWidget->WidgetTree->FindWidget(WidgetName));
}

UWrapBox* CreateNativeButcherGrid(UUserWidget* ResultWidget)
{
	if (!ResultWidget || !ResultWidget->WidgetTree)
	{
		return nullptr;
	}

	if (UWidget* ExistingGrid = ResultWidget->WidgetTree->FindWidget(NativeButcherGridName))
	{
		ExistingGrid->RemoveFromParent();
	}

	UPanelWidget* RootPanel = Cast<UPanelWidget>(ResultWidget->WidgetTree->RootWidget);
	if (!RootPanel)
	{
		return nullptr;
	}

	UWrapBox* Grid = ResultWidget->WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), NativeButcherGridName);
	if (!Grid)
	{
		return nullptr;
	}

	Grid->SetInnerSlotPadding(FVector2D(8.0f, 8.0f));
	Grid->SetHorizontalAlignment(HAlign_Center);

	if (UCanvasPanel* CanvasPanel = Cast<UCanvasPanel>(RootPanel))
	{
		UCanvasPanelSlot* CanvasSlot = CanvasPanel->AddChildToCanvas(Grid);
		if (CanvasSlot)
		{
			CanvasSlot->SetAnchors(FAnchors(0.5f, 0.12f, 0.5f, 0.12f));
			CanvasSlot->SetOffsets(FMargin(
				-ButcherResultGridWidth * 0.5f,
				0.0f,
				ButcherResultGridWidth,
				ButcherResultGridHeight));
			CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		}
	}
	else
	{
		RootPanel->AddChild(Grid);
	}

	return Grid;
}

UPanelWidget* ResolveButcherDropPanel(UUserWidget* ResultWidget)
{
	if (UPanelWidget* ExistingNativePanel = FindButcherWidgetByName<UPanelWidget>(ResultWidget, NativeButcherGridName))
	{
		ExistingNativePanel->ClearChildren();
		return ExistingNativePanel;
	}

	return CreateNativeButcherGrid(ResultWidget);
}

UWidget* CreateButcherDropEntry(UUserWidget* ResultWidget, const FButcherResultDropItem& Item)
{
	if (!ResultWidget || !ResultWidget->WidgetTree)
	{
		return nullptr;
	}

	USizeBox* EntrySize = ResultWidget->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	UOverlay* Overlay = ResultWidget->WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	if (!EntrySize || !Overlay)
	{
		return nullptr;
	}

	EntrySize->SetWidthOverride(ButcherResultEntrySize);
	EntrySize->SetHeightOverride(ButcherResultEntrySize);
	EntrySize->SetToolTipText(Item.DisplayName);
	EntrySize->AddChild(Overlay);

	UImage* IconImage = ResultWidget->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	if (IconImage)
	{
		if (Item.IconTexture)
		{
			IconImage->SetBrushFromTexture(Item.IconTexture, false);
		}
		IconImage->SetColorAndOpacity(FLinearColor::White);

		if (UOverlaySlot* IconSlot = Overlay->AddChildToOverlay(IconImage))
		{
			IconSlot->SetHorizontalAlignment(HAlign_Fill);
			IconSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	UBorder* CountBadge = ResultWidget->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	UTextBlock* CountText = ResultWidget->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	if (CountBadge && CountText)
	{
		CountBadge->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.82f));
		CountBadge->SetPadding(FMargin(5.0f, 1.0f));

		CountText->SetText(FText::AsNumber(Item.Count));
		CountText->SetColorAndOpacity(FSlateColor(FLinearColor::White));

		FSlateFontInfo Font = CountText->GetFont();
		Font.Size = 16;
		CountText->SetFont(Font);

		CountBadge->SetContent(CountText);
		if (UOverlaySlot* BadgeSlot = Overlay->AddChildToOverlay(CountBadge))
		{
			BadgeSlot->SetHorizontalAlignment(HAlign_Right);
			BadgeSlot->SetVerticalAlignment(VAlign_Bottom);
			BadgeSlot->SetPadding(FMargin(0.0f, 0.0f, 3.0f, 3.0f));
		}
	}

	return EntrySize;
}

void HideLegacyButcherResultText(UUserWidget* ResultWidget)
{
	if (UTextBlock* ResultText = FindButcherWidgetByName<UTextBlock>(ResultWidget, TEXT("Txt_Result")))
	{
		ResultText->SetVisibility(ESlateVisibility::Collapsed);
	}
}
}

bool UButcherResultBlueprintLibrary::ApplyButcherResultIconLayout(UUserWidget* ResultWidget, UDataTable* ItemDataTable)
{
	if (!ResultWidget || !ResultWidget->WidgetTree)
	{
		return false;
	}

	HideLegacyButcherResultText(ResultWidget);

	UDataTable* ResolvedItemDataTable = ResolveButcherItemDataTable(ResultWidget, ItemDataTable);

	TArray<FButcherResultDropItem> Drops;
	if (!ReadButcherDrops(ResultWidget, ResolvedItemDataTable, Drops))
	{
		return false;
	}

	UPanelWidget* ResultPanel = ResolveButcherDropPanel(ResultWidget);
	if (!ResultPanel)
	{
		return false;
	}

	ResultPanel->ClearChildren();

	for (const FButcherResultDropItem& Drop : Drops)
	{
		if (UWidget* EntryWidget = CreateButcherDropEntry(ResultWidget, Drop))
		{
			UPanelSlot* AddedSlot = ResultPanel->AddChild(EntryWidget);
			if (UWrapBoxSlot* WrapBoxSlot = Cast<UWrapBoxSlot>(AddedSlot))
			{
				WrapBoxSlot->SetPadding(FMargin(4.0f));
			}
		}
	}

	return true;
}
