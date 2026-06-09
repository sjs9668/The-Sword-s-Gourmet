#include "InventorySelectBlueprintLibrary.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ActorComponent.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/DataTable.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogInventorySelectUI, Log, All);

namespace
{
FProperty* FindInventorySelectProperty(const UStruct* Struct, const TCHAR* FieldName)
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

bool ReadInventorySelectNameField(const UStruct* Struct, const void* Container, const TCHAR* FieldName, FName& OutValue)
{
	const FNameProperty* NameProperty = CastField<FNameProperty>(FindInventorySelectProperty(Struct, FieldName));
	if (!NameProperty)
	{
		return false;
	}

	OutValue = NameProperty->GetPropertyValue_InContainer(Container);
	return true;
}

bool ReadInventorySelectIntField(const UStruct* Struct, const void* Container, const TCHAR* FieldName, int32& OutValue)
{
	const FNumericProperty* NumericProperty = CastField<FNumericProperty>(FindInventorySelectProperty(Struct, FieldName));
	if (!NumericProperty)
	{
		return false;
	}

	const void* ValuePtr = NumericProperty->ContainerPtrToValuePtr<void>(Container);
	OutValue = NumericProperty->IsFloatingPoint()
		? FMath::RoundToInt(NumericProperty->GetFloatingPointPropertyValue(ValuePtr))
		: static_cast<int32>(NumericProperty->GetSignedIntPropertyValue(ValuePtr));
	return true;
}

bool ReadInventorySelectTextField(const UStruct* Struct, const void* Container, const TCHAR* FieldName, FText& OutValue)
{
	if (const FTextProperty* TextProperty = CastField<FTextProperty>(FindInventorySelectProperty(Struct, FieldName)))
	{
		const FText* TextValue = TextProperty->ContainerPtrToValuePtr<FText>(Container);
		if (TextValue && !TextValue->IsEmpty())
		{
			OutValue = *TextValue;
			return true;
		}
	}

	if (const FStrProperty* StringProperty = CastField<FStrProperty>(FindInventorySelectProperty(Struct, FieldName)))
	{
		const FString StringValue = StringProperty->GetPropertyValue_InContainer(Container);
		if (!StringValue.IsEmpty())
		{
			OutValue = FText::FromString(StringValue);
			return true;
		}
	}

	FName NameValue = NAME_None;
	if (ReadInventorySelectNameField(Struct, Container, FieldName, NameValue) && !NameValue.IsNone())
	{
		OutValue = FText::FromName(NameValue);
		return true;
	}

	return false;
}

template <typename ObjectType>
ObjectType* ReadInventorySelectObjectField(const UStruct* Struct, const void* Container, const TCHAR* FieldName)
{
	const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(FindInventorySelectProperty(Struct, FieldName));
	if (!ObjectProperty)
	{
		return nullptr;
	}

	return Cast<ObjectType>(ObjectProperty->GetObjectPropertyValue_InContainer(Container));
}

bool SetInventorySelectNameField(UObject* Object, const TCHAR* FieldName, FName Value)
{
	FNameProperty* NameProperty = Object
		? CastField<FNameProperty>(FindInventorySelectProperty(Object->GetClass(), FieldName))
		: nullptr;
	if (!NameProperty)
	{
		return false;
	}

	NameProperty->SetPropertyValue_InContainer(Object, Value);
	return true;
}

bool SetInventorySelectIntField(UObject* Object, const TCHAR* FieldName, int32 Value)
{
	FNumericProperty* NumericProperty = Object
		? CastField<FNumericProperty>(FindInventorySelectProperty(Object->GetClass(), FieldName))
		: nullptr;
	if (!NumericProperty)
	{
		return false;
	}

	void* ValuePtr = NumericProperty->ContainerPtrToValuePtr<void>(Object);
	if (NumericProperty->IsInteger())
	{
		NumericProperty->SetIntPropertyValue(ValuePtr, static_cast<int64>(Value));
	}
	else
	{
		NumericProperty->SetFloatingPointPropertyValue(ValuePtr, static_cast<double>(Value));
	}
	return true;
}

bool SetInventorySelectTextField(UObject* Object, const TCHAR* FieldName, const FText& Value)
{
	FTextProperty* TextProperty = Object
		? CastField<FTextProperty>(FindInventorySelectProperty(Object->GetClass(), FieldName))
		: nullptr;
	if (!TextProperty)
	{
		return false;
	}

	TextProperty->SetPropertyValue_InContainer(Object, Value);
	return true;
}

bool SetInventorySelectObjectField(UObject* Object, const TCHAR* FieldName, UObject* Value)
{
	FObjectPropertyBase* ObjectProperty = Object
		? CastField<FObjectPropertyBase>(FindInventorySelectProperty(Object->GetClass(), FieldName))
		: nullptr;
	if (!ObjectProperty)
	{
		return false;
	}

	if (Value && !Value->IsA(ObjectProperty->PropertyClass))
	{
		return false;
	}

	ObjectProperty->SetObjectPropertyValue_InContainer(Object, Value);
	return true;
}

bool HasInventoryItemsArray(UObject* Object)
{
	const FArrayProperty* ItemsProperty = Object
		? CastField<FArrayProperty>(FindInventorySelectProperty(Object->GetClass(), TEXT("Items")))
		: nullptr;
	return ItemsProperty && CastField<FStructProperty>(ItemsProperty->Inner);
}

UObject* ResolveInventoryComponent(UUserWidget* InventorySelectWidget, UObject* ProvidedInventoryComponent)
{
	if (ProvidedInventoryComponent)
	{
		return ProvidedInventoryComponent;
	}

	if (UObject* WidgetInventoryComponent = ReadInventorySelectObjectField<UObject>(
		InventorySelectWidget ? InventorySelectWidget->GetClass() : nullptr,
		InventorySelectWidget,
		TEXT("InventoryCompRef")))
	{
		return WidgetInventoryComponent;
	}

	APlayerController* OwningPlayer = InventorySelectWidget ? InventorySelectWidget->GetOwningPlayer() : nullptr;
	APawn* Pawn = OwningPlayer ? OwningPlayer->GetPawn() : nullptr;
	if (!Pawn)
	{
		return nullptr;
	}

	const TCHAR* CandidatePropertyNames[] =
	{
		TEXT("InventoryCompRef"),
		TEXT("InventoryComponent"),
		TEXT("InventoryComp"),
		TEXT("InventoryRef")
	};

	for (const TCHAR* CandidatePropertyName : CandidatePropertyNames)
	{
		if (UObject* PawnInventoryComponent = ReadInventorySelectObjectField<UObject>(Pawn->GetClass(), Pawn, CandidatePropertyName))
		{
			if (HasInventoryItemsArray(PawnInventoryComponent))
			{
				return PawnInventoryComponent;
			}
		}
	}

	TArray<UActorComponent*> Components;
	Pawn->GetComponents(Components);
	for (UActorComponent* Component : Components)
	{
		if (HasInventoryItemsArray(Component))
		{
			return Component;
		}
	}

	return HasInventoryItemsArray(Pawn) ? Pawn : nullptr;
}

UDataTable* ResolveItemDataTable(UUserWidget* InventorySelectWidget, UDataTable* ProvidedItemDataTable)
{
	if (ProvidedItemDataTable)
	{
		return ProvidedItemDataTable;
	}

	if (UDataTable* WidgetItemDataTable = ReadInventorySelectObjectField<UDataTable>(
		InventorySelectWidget ? InventorySelectWidget->GetClass() : nullptr,
		InventorySelectWidget,
		TEXT("ItemsDTRef")))
	{
		return WidgetItemDataTable;
	}

	return LoadObject<UDataTable>(nullptr, TEXT("/Game/Resource/DataTable/DT_Items.DT_Items"));
}

TSubclassOf<UUserWidget> ResolveEntryWidgetClass(TSubclassOf<UUserWidget> ProvidedEntryWidgetClass)
{
	if (ProvidedEntryWidgetClass)
	{
		return ProvidedEntryWidgetClass;
	}

	UClass* LoadedClass = StaticLoadClass(UUserWidget::StaticClass(), nullptr, TEXT("/Game/BluePrint/UI/WBP_InvItemEntry.WBP_InvItemEntry_C"));
	return LoadedClass && LoadedClass->IsChildOf(UUserWidget::StaticClass())
		? TSubclassOf<UUserWidget>(LoadedClass)
		: nullptr;
}

const uint8* FindInventorySelectDataTableRow(const UDataTable* DataTable, FName RowName)
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

FText ResolveInventoryEntryDisplayName(UDataTable* ItemDataTable, FName ItemId)
{
	const uint8* RowData = FindInventorySelectDataTableRow(ItemDataTable, ItemId);
	const UScriptStruct* RowStruct = ItemDataTable ? ItemDataTable->GetRowStruct() : nullptr;
	if (!RowData || !RowStruct)
	{
		return FText::FromName(ItemId);
	}

	FText DisplayName;
	if (ReadInventorySelectTextField(RowStruct, RowData, TEXT("DisplayName"), DisplayName)
		|| ReadInventorySelectTextField(RowStruct, RowData, TEXT("Name"), DisplayName)
		|| ReadInventorySelectTextField(RowStruct, RowData, TEXT("ItemName"), DisplayName))
	{
		return DisplayName;
	}

	return FText::FromName(ItemId);
}

UUserWidget* CreateInventoryEntryWidget(UUserWidget* InventorySelectWidget, TSubclassOf<UUserWidget> EntryWidgetClass)
{
	if (!InventorySelectWidget || !EntryWidgetClass)
	{
		return nullptr;
	}

	if (APlayerController* OwningPlayer = InventorySelectWidget->GetOwningPlayer())
	{
		return CreateWidget<UUserWidget>(OwningPlayer, EntryWidgetClass);
	}

	return CreateWidget<UUserWidget>(InventorySelectWidget->GetWorld(), EntryWidgetClass);
}

void TryCallEntrySetLabel(UUserWidget* EntryWidget)
{
	UFunction* SetLabelFunction = EntryWidget ? EntryWidget->FindFunction(TEXT("SetLabel")) : nullptr;
	if (SetLabelFunction && SetLabelFunction->ParmsSize == 0)
	{
		EntryWidget->ProcessEvent(SetLabelFunction, nullptr);
	}
}

void ApplyInventoryEntryLabel(UUserWidget* EntryWidget, const FText& DisplayName)
{
	if (!EntryWidget || !EntryWidget->WidgetTree)
	{
		return;
	}

	const FName LabelNames[] =
	{
		FName(TEXT("Txt_Label")),
		FName(TEXT("Txt_Name")),
		FName(TEXT("Text_Name")),
		FName(TEXT("LabelText"))
	};

	for (const FName& LabelName : LabelNames)
	{
		if (UTextBlock* Label = Cast<UTextBlock>(EntryWidget->WidgetTree->FindWidget(LabelName)))
		{
			Label->SetText(DisplayName);
			return;
		}
	}
}
}

bool UInventorySelectBlueprintLibrary::RefreshInventorySelect(
	UUserWidget* InventorySelectWidget,
	UObject* InventoryComponent,
	UDataTable* ItemDataTable,
	TSubclassOf<UUserWidget> EntryWidgetClass,
	FName ListWidgetName)
{
	if (!InventorySelectWidget || !InventorySelectWidget->WidgetTree)
	{
		return false;
	}

	const FName ResolvedListWidgetName = ListWidgetName.IsNone() ? FName(TEXT("SB_List")) : ListWidgetName;
	UPanelWidget* ListPanel = Cast<UPanelWidget>(InventorySelectWidget->WidgetTree->FindWidget(ResolvedListWidgetName));
	if (!ListPanel)
	{
		UE_LOG(LogInventorySelectUI, Warning, TEXT("RefreshInventorySelect failed: list widget missing. Widget=%s ListWidgetName=%s"),
			*GetNameSafe(InventorySelectWidget),
			*ResolvedListWidgetName.ToString());
		return false;
	}

	UObject* ResolvedInventoryComponent = ResolveInventoryComponent(InventorySelectWidget, InventoryComponent);
	const FArrayProperty* ItemsProperty = ResolvedInventoryComponent
		? CastField<FArrayProperty>(FindInventorySelectProperty(ResolvedInventoryComponent->GetClass(), TEXT("Items")))
		: nullptr;
	const FStructProperty* ItemStructProperty = ItemsProperty ? CastField<FStructProperty>(ItemsProperty->Inner) : nullptr;
	if (!ResolvedInventoryComponent || !ItemsProperty || !ItemStructProperty || !ItemStructProperty->Struct)
	{
		ListPanel->ClearChildren();
		UE_LOG(LogInventorySelectUI, Warning, TEXT("RefreshInventorySelect failed: inventory items array missing. Widget=%s Inventory=%s"),
			*GetNameSafe(InventorySelectWidget),
			*GetNameSafe(ResolvedInventoryComponent));
		return false;
	}

	UDataTable* ResolvedItemDataTable = ResolveItemDataTable(InventorySelectWidget, ItemDataTable);
	TSubclassOf<UUserWidget> ResolvedEntryWidgetClass = ResolveEntryWidgetClass(EntryWidgetClass);
	if (!ResolvedEntryWidgetClass)
	{
		ListPanel->ClearChildren();
		UE_LOG(LogInventorySelectUI, Warning, TEXT("RefreshInventorySelect failed: entry widget class missing."));
		return false;
	}

	ListPanel->ClearChildren();

	const void* ArrayContainer = ItemsProperty->ContainerPtrToValuePtr<void>(ResolvedInventoryComponent);
	FScriptArrayHelper ArrayHelper(ItemsProperty, ArrayContainer);
	int32 AddedCount = 0;

	for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
	{
		const void* ElementContainer = ArrayHelper.GetRawPtr(Index);

		FName ItemId = NAME_None;
		int32 Count = 1;
		ReadInventorySelectNameField(ItemStructProperty->Struct, ElementContainer, TEXT("ItemId"), ItemId);
		ReadInventorySelectIntField(ItemStructProperty->Struct, ElementContainer, TEXT("Count"), Count);

		if (ItemId.IsNone() || Count <= 0)
		{
			continue;
		}

		UUserWidget* EntryWidget = CreateInventoryEntryWidget(InventorySelectWidget, ResolvedEntryWidgetClass);
		if (!EntryWidget)
		{
			continue;
		}

		SetInventorySelectNameField(EntryWidget, TEXT("ItemId"), ItemId);
		SetInventorySelectIntField(EntryWidget, TEXT("Count"), Count);
		SetInventorySelectIntField(EntryWidget, TEXT("StackIndex"), Index);
		SetInventorySelectObjectField(EntryWidget, TEXT("ItemsDTRef"), ResolvedItemDataTable);
		SetInventorySelectObjectField(EntryWidget, TEXT("Parent"), InventorySelectWidget);
		SetInventorySelectTextField(EntryWidget, TEXT("LabelText"), ResolveInventoryEntryDisplayName(ResolvedItemDataTable, ItemId));
		TryCallEntrySetLabel(EntryWidget);
		ApplyInventoryEntryLabel(EntryWidget, ResolveInventoryEntryDisplayName(ResolvedItemDataTable, ItemId));

		ListPanel->AddChild(EntryWidget);
		++AddedCount;
	}

	UE_LOG(LogInventorySelectUI, Log, TEXT("RefreshInventorySelect succeeded. Widget=%s Inventory=%s Added=%d TotalStacks=%d"),
		*GetNameSafe(InventorySelectWidget),
		*GetNameSafe(ResolvedInventoryComponent),
		AddedCount,
		ArrayHelper.Num());
	return true;
}
