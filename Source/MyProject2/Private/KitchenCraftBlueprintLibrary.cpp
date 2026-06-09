#include "KitchenCraftBlueprintLibrary.h"

#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "DiningTableMealComponent.h"
#include "KitchenCraftStationComponent.h"
#include "KitchenCraftWidgetHostComponent.h"
#include "KitchenIngredientActor.h"
#include "KitchenInventoryProvider.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogKitchenCraftOptions, Log, All);

namespace
{
FProperty* FindKitchenOptionProperty(const UStruct* Struct, const TCHAR* FieldName)
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

bool ReadKitchenOptionNameField(const UStruct* Struct, const void* Container, const TCHAR* FieldName, FName& OutValue)
{
	const FNameProperty* NameProperty = CastField<FNameProperty>(FindKitchenOptionProperty(Struct, FieldName));
	if (!NameProperty)
	{
		return false;
	}

	OutValue = NameProperty->GetPropertyValue_InContainer(Container);
	return true;
}

bool ReadKitchenOptionIntField(const UStruct* Struct, const void* Container, const TCHAR* FieldName, int32& OutValue)
{
	const FNumericProperty* NumericProperty = CastField<FNumericProperty>(FindKitchenOptionProperty(Struct, FieldName));
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

bool SetKitchenOptionIntField(const UStruct* Struct, void* Container, const TCHAR* FieldName, int32 Value)
{
	FNumericProperty* NumericProperty = CastField<FNumericProperty>(FindKitchenOptionProperty(Struct, FieldName));
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
		NumericProperty->SetFloatingPointPropertyValue(ValuePtr, static_cast<double>(Value));
	}
	return true;
}

bool ReadKitchenOptionTextField(const UStruct* Struct, const void* Container, const TCHAR* FieldName, FText& OutValue)
{
	if (const FTextProperty* TextProperty = CastField<FTextProperty>(FindKitchenOptionProperty(Struct, FieldName)))
	{
		const FText* TextValue = TextProperty->ContainerPtrToValuePtr<FText>(Container);
		if (TextValue)
		{
			OutValue = *TextValue;
			return true;
		}
	}

	if (const FStrProperty* StringProperty = CastField<FStrProperty>(FindKitchenOptionProperty(Struct, FieldName)))
	{
		const FString StringValue = StringProperty->GetPropertyValue_InContainer(Container);
		if (!StringValue.IsEmpty())
		{
			OutValue = FText::FromString(StringValue);
			return true;
		}
	}

	FName NameValue;
	if (ReadKitchenOptionNameField(Struct, Container, FieldName, NameValue) && !NameValue.IsNone())
	{
		OutValue = FText::FromName(NameValue);
		return true;
	}

	return false;
}

bool ReadKitchenOptionBoolField(const UStruct* Struct, const void* Container, const TCHAR* FieldName, bool& OutValue)
{
	const FBoolProperty* BoolProperty = CastField<FBoolProperty>(FindKitchenOptionProperty(Struct, FieldName));
	if (!BoolProperty)
	{
		return false;
	}

	OutValue = BoolProperty->GetPropertyValue_InContainer(Container);
	return true;
}

FName GetKitchenOptionEnumDisplayName(const UEnum* Enum, int64 Value)
{
	if (!Enum)
	{
		return NAME_None;
	}

	const FString DisplayName = Enum->GetDisplayNameTextByValue(Value).ToString();
	if (!DisplayName.IsEmpty() && !DisplayName.StartsWith(TEXT("NewEnumerator")))
	{
		return FName(*DisplayName);
	}

	const FString NameString = Enum->GetNameStringByValue(Value);
	return NameString.IsEmpty() ? NAME_None : FName(*NameString);
}

bool ReadKitchenOptionTypeField(const UStruct* Struct, const void* Container, const TCHAR* FieldName, FName& OutValue)
{
	const FProperty* Property = FindKitchenOptionProperty(Struct, FieldName);
	if (!Property)
	{
		return false;
	}

	if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
	{
		const void* ValuePtr = EnumProperty->ContainerPtrToValuePtr<void>(Container);
		const int64 EnumValue = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
		OutValue = GetKitchenOptionEnumDisplayName(EnumProperty->GetEnum(), EnumValue);
		return !OutValue.IsNone();
	}

	if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
	{
		const uint8 EnumValue = ByteProperty->GetPropertyValue_InContainer(Container);
		OutValue = GetKitchenOptionEnumDisplayName(ByteProperty->Enum, EnumValue);
		return !OutValue.IsNone();
	}

	if (const FNameProperty* NameProperty = CastField<FNameProperty>(Property))
	{
		OutValue = NameProperty->GetPropertyValue_InContainer(Container);
		return !OutValue.IsNone();
	}

	if (const FStrProperty* StringProperty = CastField<FStrProperty>(Property))
	{
		const FString StringValue = StringProperty->GetPropertyValue_InContainer(Container);
		OutValue = StringValue.IsEmpty() ? NAME_None : FName(*StringValue);
		return !OutValue.IsNone();
	}

	if (const FTextProperty* TextProperty = CastField<FTextProperty>(Property))
	{
		const FText* TextValue = TextProperty->ContainerPtrToValuePtr<FText>(Container);
		const FString TextString = TextValue ? TextValue->ToString() : FString();
		OutValue = TextString.IsEmpty() ? NAME_None : FName(*TextString);
		return !OutValue.IsNone();
	}

	return false;
}

template <typename ObjectType>
bool ReadKitchenOptionObjectField(const UStruct* Struct, const void* Container, const TCHAR* FieldName, TObjectPtr<ObjectType>& OutValue)
{
	const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(FindKitchenOptionProperty(Struct, FieldName));
	if (!ObjectProperty)
	{
		return false;
	}

	OutValue = Cast<ObjectType>(ObjectProperty->GetObjectPropertyValue_InContainer(Container));
	return true;
}

bool ReadKitchenOptionClassField(const UStruct* Struct, const void* Container, const TCHAR* FieldName, TSubclassOf<AKitchenIngredientActor>& OutValue)
{
	const FClassProperty* ClassProperty = CastField<FClassProperty>(FindKitchenOptionProperty(Struct, FieldName));
	if (!ClassProperty)
	{
		return false;
	}

	UClass* ClassValue = Cast<UClass>(ClassProperty->GetObjectPropertyValue_InContainer(Container));
	if (!ClassValue || !ClassValue->IsChildOf(AKitchenIngredientActor::StaticClass()))
	{
		return false;
	}

	OutValue = ClassValue;
	return true;
}

bool ReadKitchenOptionStatBonusField(const UStruct* Struct, const void* Container, const TCHAR* FieldName, FKitchenStatBonus& OutValue)
{
	const FStructProperty* StructProperty = CastField<FStructProperty>(FindKitchenOptionProperty(Struct, FieldName));
	if (!StructProperty || !StructProperty->Struct)
	{
		return false;
	}

	const void* StatContainer = StructProperty->ContainerPtrToValuePtr<void>(Container);
	ReadKitchenOptionIntField(StructProperty->Struct, StatContainer, TEXT("Quality"), OutValue.Quality);
	ReadKitchenOptionIntField(StructProperty->Struct, StatContainer, TEXT("HPBonus"), OutValue.HPBonus);
	ReadKitchenOptionIntField(StructProperty->Struct, StatContainer, TEXT("AttackTierBonus"), OutValue.AttackTierBonus);
	ReadKitchenOptionIntField(StructProperty->Struct, StatContainer, TEXT("AttackSpeedTierBonus"), OutValue.AttackSpeedTierBonus);
	ReadKitchenOptionIntField(StructProperty->Struct, StatContainer, TEXT("DashTierBonus"), OutValue.DashTierBonus);
	ReadKitchenOptionIntField(StructProperty->Struct, StatContainer, TEXT("DashCountBonus"), OutValue.DashCountBonus);
	ReadKitchenOptionIntField(StructProperty->Struct, StatContainer, TEXT("MoveSpeedBonus"), OutValue.MoveSpeedBonus);
	ReadKitchenOptionIntField(StructProperty->Struct, StatContainer, TEXT("DropRateBonus"), OutValue.DropRateBonus);
	return true;
}

const uint8* FindKitchenOptionDataTableRow(const UDataTable* DataTable, FName RowName)
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

bool IsKitchenFoodTypeName(FName TypeName)
{
	const FString TypeString = TypeName.ToString();
	return TypeString.Equals(TEXT("Food"), ESearchCase::IgnoreCase)
		|| TypeString.EndsWith(TEXT("::Food"), ESearchCase::IgnoreCase);
}

bool ReadKitchenOptionItemRow(const UDataTable* ItemDataTable, FName ItemId, FKitchenItemRow& OutItemData)
{
	OutItemData = FKitchenItemRow();

	const uint8* RowData = FindKitchenOptionDataTableRow(ItemDataTable, ItemId);
	const UScriptStruct* RowStruct = ItemDataTable ? ItemDataTable->GetRowStruct() : nullptr;
	if (!RowData || !RowStruct)
	{
		return false;
	}

	if (!ReadKitchenOptionTextField(RowStruct, RowData, TEXT("DisplayName"), OutItemData.DisplayName))
	{
		FName NameField;
		OutItemData.DisplayName = ReadKitchenOptionNameField(RowStruct, RowData, TEXT("Name"), NameField)
			? FText::FromName(NameField)
			: FText::FromName(ItemId);
	}

	ReadKitchenOptionObjectField(RowStruct, RowData, TEXT("IconTexture"), OutItemData.IconTexture);
	ReadKitchenOptionObjectField(RowStruct, RowData, TEXT("IngredientMesh"), OutItemData.IngredientMesh);
	ReadKitchenOptionClassField(RowStruct, RowData, TEXT("IngredientActorClass"), OutItemData.IngredientActorClass);

	if (!ReadKitchenOptionTypeField(RowStruct, RowData, TEXT("Type"), OutItemData.ItemTypeName)
		&& !ReadKitchenOptionTypeField(RowStruct, RowData, TEXT("ItemType"), OutItemData.ItemTypeName))
	{
		ReadKitchenOptionTypeField(RowStruct, RowData, TEXT("ItemTypeName"), OutItemData.ItemTypeName);
	}

	bool bCanUseAsIngredient = true;
	if (!ReadKitchenOptionBoolField(RowStruct, RowData, TEXT("bCanUseAsIngredient"), bCanUseAsIngredient))
	{
		ReadKitchenOptionBoolField(RowStruct, RowData, TEXT("CanUseAsIngredient"), bCanUseAsIngredient);
	}
	OutItemData.bCanUseAsIngredient = bCanUseAsIngredient;
	if (IsKitchenFoodTypeName(OutItemData.ItemTypeName))
	{
		OutItemData.bCanUseAsIngredient = false;
	}

	if (!ReadKitchenOptionStatBonusField(RowStruct, RowData, TEXT("BaseStatBonus"), OutItemData.BaseStatBonus)
		&& !ReadKitchenOptionStatBonusField(RowStruct, RowData, TEXT("StatBonus"), OutItemData.BaseStatBonus)
		&& !ReadKitchenOptionStatBonusField(RowStruct, RowData, TEXT("CookingStatBonus"), OutItemData.BaseStatBonus))
	{
		ReadKitchenOptionStatBonusField(RowStruct, RowData, TEXT("FoodStatBonus"), OutItemData.BaseStatBonus);
	}

	return true;
}

struct FKitchenReflectedInventoryEntry
{
	FName ItemId = NAME_None;
	int32 Count = 0;
	int32 StackIndex = INDEX_NONE;
	FKitchenStatBonus StatBonus;
	bool bHasStatBonus = false;

	bool IsValid() const
	{
		return !ItemId.IsNone() && Count > 0 && StackIndex != INDEX_NONE;
	}
};

bool ReadReflectedKitchenInventoryEntries(UObject* InventoryProvider, TArray<FKitchenReflectedInventoryEntry>& OutEntries)
{
	OutEntries.Reset();
	if (!InventoryProvider)
	{
		return false;
	}

	const FArrayProperty* ItemsProperty = CastField<FArrayProperty>(FindKitchenOptionProperty(InventoryProvider->GetClass(), TEXT("Items")));
	const FStructProperty* ItemStructProperty = ItemsProperty ? CastField<FStructProperty>(ItemsProperty->Inner) : nullptr;
	if (!ItemsProperty || !ItemStructProperty || !ItemStructProperty->Struct)
	{
		return false;
	}

	const void* ArrayContainer = ItemsProperty->ContainerPtrToValuePtr<void>(InventoryProvider);
	FScriptArrayHelper ArrayHelper(ItemsProperty, ArrayContainer);
	for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
	{
		const void* ElementContainer = ArrayHelper.GetRawPtr(Index);

		FKitchenReflectedInventoryEntry Entry;
		Entry.StackIndex = Index;
		ReadKitchenOptionNameField(ItemStructProperty->Struct, ElementContainer, TEXT("ItemId"), Entry.ItemId);
		if (!ReadKitchenOptionIntField(ItemStructProperty->Struct, ElementContainer, TEXT("Count"), Entry.Count))
		{
			Entry.Count = 1;
		}

		Entry.bHasStatBonus =
			ReadKitchenOptionStatBonusField(ItemStructProperty->Struct, ElementContainer, TEXT("CookedStatBonus"), Entry.StatBonus)
			|| ReadKitchenOptionStatBonusField(ItemStructProperty->Struct, ElementContainer, TEXT("StatBonus"), Entry.StatBonus)
			|| ReadKitchenOptionStatBonusField(ItemStructProperty->Struct, ElementContainer, TEXT("FoodStatBonus"), Entry.StatBonus)
			|| ReadKitchenOptionStatBonusField(ItemStructProperty->Struct, ElementContainer, TEXT("BaseStatBonus"), Entry.StatBonus);

		if (Entry.IsValid())
		{
			OutEntries.Add(Entry);
		}
	}

	return true;
}

bool ReadReflectedKitchenInventoryItems(UObject* InventoryProvider, TArray<FKitchenItemStack>& OutItems)
{
	OutItems.Reset();
	TArray<FKitchenReflectedInventoryEntry> Entries;
	if (!ReadReflectedKitchenInventoryEntries(InventoryProvider, Entries))
	{
		return false;
	}

	for (const FKitchenReflectedInventoryEntry& Entry : Entries)
	{
		FKitchenItemStack Item;
		Item.ItemId = Entry.ItemId;
		Item.Count = Entry.Count;
		OutItems.Add(Item);
	}

	return true;
}

int32 CountReflectedKitchenInventoryItem(const TArray<FKitchenItemStack>& Items, FName ItemId)
{
	int32 Count = 0;
	for (const FKitchenItemStack& Item : Items)
	{
		if (Item.ItemId == ItemId)
		{
			Count += Item.Count;
		}
	}

	return Count;
}

int32 ResolveKitchenOptionAvailableCount(UObject* InventoryProvider, FName ItemId, bool bHasReflectedInventoryItems, const TArray<FKitchenItemStack>& ReflectedInventoryItems)
{
	const bool bHasInventoryProvider = InventoryProvider && InventoryProvider->GetClass()->ImplementsInterface(UKitchenInventoryProvider::StaticClass());
	const int32 InterfaceAvailableCount = bHasInventoryProvider
		? IKitchenInventoryProvider::Execute_GetKitchenItemCount(InventoryProvider, ItemId)
		: 0;
	const int32 ReflectedAvailableCount = bHasReflectedInventoryItems
		? CountReflectedKitchenInventoryItem(ReflectedInventoryItems, ItemId)
		: 0;
	return InterfaceAvailableCount > 0 ? InterfaceAvailableCount : ReflectedAvailableCount;
}

bool FindKitchenInventoryFoodStatBonus(UObject* InventoryProvider, FName ItemId, int32 RequiredCount, const FKitchenStatBonus& FallbackStatBonus, FKitchenStatBonus& OutStatBonus)
{
	OutStatBonus = FallbackStatBonus;
	if (!InventoryProvider || ItemId.IsNone() || RequiredCount <= 0)
	{
		return false;
	}

	const FArrayProperty* ItemsProperty = CastField<FArrayProperty>(FindKitchenOptionProperty(InventoryProvider->GetClass(), TEXT("Items")));
	const FStructProperty* ItemStructProperty = ItemsProperty ? CastField<FStructProperty>(ItemsProperty->Inner) : nullptr;
	if (!ItemsProperty || !ItemStructProperty || !ItemStructProperty->Struct)
	{
		return false;
	}

	const void* ArrayContainer = ItemsProperty->ContainerPtrToValuePtr<void>(InventoryProvider);
	FScriptArrayHelper ArrayHelper(ItemsProperty, ArrayContainer);
	for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
	{
		const void* ElementContainer = ArrayHelper.GetRawPtr(Index);

		FName CandidateItemId = NAME_None;
		if (!ReadKitchenOptionNameField(ItemStructProperty->Struct, ElementContainer, TEXT("ItemId"), CandidateItemId)
			|| CandidateItemId != ItemId)
		{
			continue;
		}

		int32 CandidateCount = 1;
		ReadKitchenOptionIntField(ItemStructProperty->Struct, ElementContainer, TEXT("Count"), CandidateCount);
		if (CandidateCount < RequiredCount)
		{
			continue;
		}

		FKitchenStatBonus InventoryStatBonus = FallbackStatBonus;
		if (ReadKitchenOptionStatBonusField(ItemStructProperty->Struct, ElementContainer, TEXT("CookedStatBonus"), InventoryStatBonus)
			|| ReadKitchenOptionStatBonusField(ItemStructProperty->Struct, ElementContainer, TEXT("StatBonus"), InventoryStatBonus)
			|| ReadKitchenOptionStatBonusField(ItemStructProperty->Struct, ElementContainer, TEXT("FoodStatBonus"), InventoryStatBonus)
			|| ReadKitchenOptionStatBonusField(ItemStructProperty->Struct, ElementContainer, TEXT("BaseStatBonus"), InventoryStatBonus))
		{
			OutStatBonus = InventoryStatBonus;
			return true;
		}

		return false;
	}

	return false;
}

void BroadcastKitchenInventoryChanged(UObject* InventoryProvider)
{
	if (!InventoryProvider)
	{
		return;
	}

	FMulticastDelegateProperty* DelegateProperty = CastField<FMulticastDelegateProperty>(
		FindKitchenOptionProperty(InventoryProvider->GetClass(), TEXT("OnInventoryChanged")));
	if (!DelegateProperty)
	{
		return;
	}

	FMulticastScriptDelegate* Delegate = DelegateProperty->ContainerPtrToValuePtr<FMulticastScriptDelegate>(InventoryProvider);
	if (Delegate)
	{
		Delegate->ProcessMulticastDelegate<UObject>(nullptr);
	}
}

bool TryConsumeKitchenFoodStackDirect(
	UObject* InventoryProvider,
	FName ItemId,
	int32 InventoryStackIndex,
	int32 Count,
	const FKitchenStatBonus& FallbackStatBonus,
	FKitchenStatBonus& ConsumedStatBonus)
{
	ConsumedStatBonus = FallbackStatBonus;
	if (!InventoryProvider || ItemId.IsNone() || InventoryStackIndex == INDEX_NONE || Count <= 0)
	{
		return false;
	}

	const FArrayProperty* ItemsProperty = CastField<FArrayProperty>(FindKitchenOptionProperty(InventoryProvider->GetClass(), TEXT("Items")));
	const FStructProperty* ItemStructProperty = ItemsProperty ? CastField<FStructProperty>(ItemsProperty->Inner) : nullptr;
	if (!ItemsProperty || !ItemStructProperty || !ItemStructProperty->Struct)
	{
		return false;
	}

	void* ArrayContainer = ItemsProperty->ContainerPtrToValuePtr<void>(InventoryProvider);
	FScriptArrayHelper ArrayHelper(ItemsProperty, ArrayContainer);
	if (!ArrayHelper.IsValidIndex(InventoryStackIndex))
	{
		return false;
	}

	void* ElementContainer = ArrayHelper.GetRawPtr(InventoryStackIndex);
	FName StackItemId = NAME_None;
	if (!ReadKitchenOptionNameField(ItemStructProperty->Struct, ElementContainer, TEXT("ItemId"), StackItemId)
		|| StackItemId != ItemId)
	{
		return false;
	}

	int32 StackCount = 1;
	ReadKitchenOptionIntField(ItemStructProperty->Struct, ElementContainer, TEXT("Count"), StackCount);
	if (StackCount < Count)
	{
		return false;
	}

	FKitchenStatBonus StackStatBonus = FallbackStatBonus;
	if (ReadKitchenOptionStatBonusField(ItemStructProperty->Struct, ElementContainer, TEXT("CookedStatBonus"), StackStatBonus)
		|| ReadKitchenOptionStatBonusField(ItemStructProperty->Struct, ElementContainer, TEXT("StatBonus"), StackStatBonus)
		|| ReadKitchenOptionStatBonusField(ItemStructProperty->Struct, ElementContainer, TEXT("FoodStatBonus"), StackStatBonus)
		|| ReadKitchenOptionStatBonusField(ItemStructProperty->Struct, ElementContainer, TEXT("BaseStatBonus"), StackStatBonus))
	{
		ConsumedStatBonus = StackStatBonus;
	}

	if (StackCount > Count)
	{
		if (!SetKitchenOptionIntField(ItemStructProperty->Struct, ElementContainer, TEXT("Count"), StackCount - Count))
		{
			return false;
		}
	}
	else
	{
		ArrayHelper.RemoveValues(InventoryStackIndex, 1);
	}

	BroadcastKitchenInventoryChanged(InventoryProvider);
	UE_LOG(LogKitchenCraftOptions, Log, TEXT("ConsumeKitchenFoodStackForTable direct stack succeeded. Provider=%s ItemId=%s StackIndex=%d Count=%d Remaining=%d"),
		*GetNameSafe(InventoryProvider),
		*ItemId.ToString(),
		InventoryStackIndex,
		Count,
		FMath::Max(StackCount - Count, 0));
	return true;
}

UKitchenCraftWidgetHostComponent* FindOrCreateKitchenWidgetHost(AActor* KitchenActor)
{
	if (!KitchenActor)
	{
		return nullptr;
	}

	if (UKitchenCraftWidgetHostComponent* ExistingHost = KitchenActor->FindComponentByClass<UKitchenCraftWidgetHostComponent>())
	{
		return ExistingHost;
	}

	const FName ComponentName = MakeUniqueObjectName(KitchenActor, UKitchenCraftWidgetHostComponent::StaticClass(), TEXT("KitchenUIHost"));
	UKitchenCraftWidgetHostComponent* WidgetHost = NewObject<UKitchenCraftWidgetHostComponent>(KitchenActor, UKitchenCraftWidgetHostComponent::StaticClass(), ComponentName);
	if (!WidgetHost)
	{
		return nullptr;
	}

	WidgetHost->CreationMethod = EComponentCreationMethod::Instance;
	WidgetHost->KitchenCraftStation = KitchenActor->FindComponentByClass<UKitchenCraftStationComponent>();
	KitchenActor->AddInstanceComponent(WidgetHost);
	WidgetHost->RegisterComponent();

	UE_LOG(LogKitchenCraftOptions, Log, TEXT("Created runtime Kitchen Craft Widget Host on %s"), *GetNameSafe(KitchenActor));
	return WidgetHost;
}
}

bool UKitchenCraftBlueprintLibrary::GetKitchenIngredientOptions(
	UKitchenCraftStationComponent* KitchenCraftStation,
	UObject* InventoryProvider,
	TArray<FKitchenIngredientOption>& OutOptions,
	bool bOnlyAvailable)
{
	OutOptions.Reset();

	if (!KitchenCraftStation || !KitchenCraftStation->ItemDataTable)
	{
		return false;
	}

	const TArray<FName> RowNames = KitchenCraftStation->ItemDataTable->GetRowNames();
	TArray<FKitchenItemStack> ReflectedInventoryItems;
	const bool bHasReflectedInventoryItems = ReadReflectedKitchenInventoryItems(InventoryProvider, ReflectedInventoryItems);

	for (const FName& RowName : RowNames)
	{
		FKitchenItemRow ItemData;
		if (!KitchenCraftStation->GetItemData(RowName, ItemData))
		{
			continue;
		}

		if (!ItemData.bCanUseAsIngredient)
		{
			continue;
		}

		const int32 AvailableCount = ResolveKitchenOptionAvailableCount(InventoryProvider, RowName, bHasReflectedInventoryItems, ReflectedInventoryItems);
		if (bOnlyAvailable && AvailableCount <= 0)
		{
			continue;
		}

		FKitchenIngredientOption Option;
		Option.ItemId = RowName;
		Option.AvailableCount = AvailableCount;
		Option.ItemData = ItemData;
		OutOptions.Add(Option);
	}

	OutOptions.Sort([](const FKitchenIngredientOption& A, const FKitchenIngredientOption& B)
	{
		return A.ItemData.DisplayName.ToString() < B.ItemData.DisplayName.ToString();
	});

	return true;
}

bool UKitchenCraftBlueprintLibrary::GetKitchenFoodOptions(
	UDataTable* ItemDataTable,
	UObject* InventoryProvider,
	TArray<FKitchenIngredientOption>& OutOptions,
	bool bOnlyAvailable)
{
	OutOptions.Reset();

	if (!ItemDataTable)
	{
		return false;
	}

	TArray<FKitchenReflectedInventoryEntry> ReflectedInventoryEntries;
	const bool bHasReflectedInventoryEntries = ReadReflectedKitchenInventoryEntries(InventoryProvider, ReflectedInventoryEntries);
	if (bHasReflectedInventoryEntries)
	{
		for (const FKitchenReflectedInventoryEntry& Entry : ReflectedInventoryEntries)
		{
			FKitchenItemRow ItemData;
			if (!ReadKitchenOptionItemRow(ItemDataTable, Entry.ItemId, ItemData))
			{
				continue;
			}

			if (!IsKitchenFoodTypeName(ItemData.ItemTypeName))
			{
				continue;
			}

			if (bOnlyAvailable && Entry.Count <= 0)
			{
				continue;
			}

			FKitchenIngredientOption Option;
			Option.ItemId = Entry.ItemId;
			Option.AvailableCount = Entry.Count;
			Option.ItemData = ItemData;
			Option.InventoryStackIndex = Entry.StackIndex;
			Option.bHasInventoryStatBonus = Entry.bHasStatBonus;
			Option.InventoryStatBonus = Entry.StatBonus;
			if (Entry.bHasStatBonus)
			{
				Option.ItemData.BaseStatBonus = Entry.StatBonus;
			}
			OutOptions.Add(Option);
		}
	}
	else
	{
		const TArray<FName> RowNames = ItemDataTable->GetRowNames();

		for (const FName& RowName : RowNames)
		{
			FKitchenItemRow ItemData;
			if (!ReadKitchenOptionItemRow(ItemDataTable, RowName, ItemData))
			{
				continue;
			}

			if (!IsKitchenFoodTypeName(ItemData.ItemTypeName))
			{
				continue;
			}

			const int32 AvailableCount = ResolveKitchenOptionAvailableCount(InventoryProvider, RowName, false, TArray<FKitchenItemStack>());
			if (bOnlyAvailable && AvailableCount <= 0)
			{
				continue;
			}

			FKitchenIngredientOption Option;
			Option.ItemId = RowName;
			Option.AvailableCount = AvailableCount;
			Option.ItemData = ItemData;
			OutOptions.Add(Option);
		}
	}

	OutOptions.Sort([](const FKitchenIngredientOption& A, const FKitchenIngredientOption& B)
	{
		const FString AName = A.ItemData.DisplayName.ToString();
		const FString BName = B.ItemData.DisplayName.ToString();
		if (AName == BName)
		{
			return A.InventoryStackIndex < B.InventoryStackIndex;
		}

		return AName < BName;
	});

	return true;
}

bool UKitchenCraftBlueprintLibrary::GetKitchenFoodStatBonusFromInventory(
	UObject* InventoryProvider,
	FName ItemId,
	FKitchenStatBonus FallbackStatBonus,
	FKitchenStatBonus& OutStatBonus)
{
	return FindKitchenInventoryFoodStatBonus(InventoryProvider, ItemId, 1, FallbackStatBonus, OutStatBonus);
}

bool UKitchenCraftBlueprintLibrary::ConsumeKitchenFoodForTable(
	UObject* InventoryProvider,
	FName ItemId,
	int32 Count,
	FKitchenStatBonus FallbackStatBonus,
	FKitchenStatBonus& ConsumedStatBonus)
{
	ConsumedStatBonus = FallbackStatBonus;
	if (!InventoryProvider || ItemId.IsNone() || Count <= 0)
	{
		UE_LOG(LogKitchenCraftOptions, Warning, TEXT("ConsumeKitchenFoodForTable failed: invalid input. Provider=%s ItemId=%s Count=%d"),
			*GetNameSafe(InventoryProvider),
			*ItemId.ToString(),
			Count);
		return false;
	}

	if (!InventoryProvider->GetClass()->ImplementsInterface(UKitchenInventoryProvider::StaticClass()))
	{
		UE_LOG(LogKitchenCraftOptions, Warning, TEXT("ConsumeKitchenFoodForTable failed: provider does not implement kitchen inventory. Provider=%s ItemId=%s"),
			*GetNameSafe(InventoryProvider),
			*ItemId.ToString());
		return false;
	}

	const bool bResolvedInventoryStatBonus = FindKitchenInventoryFoodStatBonus(InventoryProvider, ItemId, Count, FallbackStatBonus, ConsumedStatBonus);
	if (!IKitchenInventoryProvider::Execute_TryReserveKitchenItem(InventoryProvider, ItemId, Count))
	{
		UE_LOG(LogKitchenCraftOptions, Warning, TEXT("ConsumeKitchenFoodForTable failed: reserve failed. Provider=%s ItemId=%s Count=%d"),
			*GetNameSafe(InventoryProvider),
			*ItemId.ToString(),
			Count);
		return false;
	}

	FKitchenItemStack ConsumedStack;
	ConsumedStack.ItemId = ItemId;
	ConsumedStack.Count = Count;

	TArray<FKitchenItemStack> ConsumedItems;
	ConsumedItems.Add(ConsumedStack);
	if (!IKitchenInventoryProvider::Execute_CommitReservedKitchenItems(InventoryProvider, ConsumedItems))
	{
		IKitchenInventoryProvider::Execute_ReturnReservedKitchenItem(InventoryProvider, ItemId, Count);
		UE_LOG(LogKitchenCraftOptions, Warning, TEXT("ConsumeKitchenFoodForTable failed: commit failed, returned reserved item. Provider=%s ItemId=%s Count=%d"),
			*GetNameSafe(InventoryProvider),
			*ItemId.ToString(),
			Count);
		return false;
	}

	UE_LOG(LogKitchenCraftOptions, Log, TEXT("ConsumeKitchenFoodForTable succeeded. Provider=%s ItemId=%s Count=%d StatSource=%s Quality=%d HP=%d Attack=%d AttackSpeed=%d Dash=%d DashCount=%d MoveSpeed=%d DropRate=%d"),
		*GetNameSafe(InventoryProvider),
		*ItemId.ToString(),
		Count,
		bResolvedInventoryStatBonus ? TEXT("Inventory") : TEXT("Fallback"),
		ConsumedStatBonus.Quality,
		ConsumedStatBonus.HPBonus,
		ConsumedStatBonus.AttackTierBonus,
		ConsumedStatBonus.AttackSpeedTierBonus,
		ConsumedStatBonus.DashTierBonus,
		ConsumedStatBonus.DashCountBonus,
		ConsumedStatBonus.MoveSpeedBonus,
		ConsumedStatBonus.DropRateBonus);
	return true;
}

bool UKitchenCraftBlueprintLibrary::ConsumeKitchenFoodStackForTable(
	UObject* InventoryProvider,
	FName ItemId,
	int32 InventoryStackIndex,
	int32 Count,
	FKitchenStatBonus FallbackStatBonus,
	FKitchenStatBonus& ConsumedStatBonus)
{
	if (TryConsumeKitchenFoodStackDirect(InventoryProvider, ItemId, InventoryStackIndex, Count, FallbackStatBonus, ConsumedStatBonus))
	{
		UE_LOG(LogKitchenCraftOptions, Log, TEXT("ConsumeKitchenFoodStackForTable succeeded. Provider=%s ItemId=%s StackIndex=%d Count=%d StatSource=Inventory Quality=%d HP=%d Attack=%d AttackSpeed=%d Dash=%d DashCount=%d MoveSpeed=%d DropRate=%d"),
			*GetNameSafe(InventoryProvider),
			*ItemId.ToString(),
			InventoryStackIndex,
			Count,
			ConsumedStatBonus.Quality,
			ConsumedStatBonus.HPBonus,
			ConsumedStatBonus.AttackTierBonus,
			ConsumedStatBonus.AttackSpeedTierBonus,
			ConsumedStatBonus.DashTierBonus,
			ConsumedStatBonus.DashCountBonus,
			ConsumedStatBonus.MoveSpeedBonus,
			ConsumedStatBonus.DropRateBonus);
		return true;
	}

	UE_LOG(LogKitchenCraftOptions, Log, TEXT("ConsumeKitchenFoodStackForTable falling back to item consume. Provider=%s ItemId=%s StackIndex=%d Count=%d"),
		*GetNameSafe(InventoryProvider),
		*ItemId.ToString(),
		InventoryStackIndex,
		Count);
	return ConsumeKitchenFoodForTable(InventoryProvider, ItemId, Count, FallbackStatBonus, ConsumedStatBonus);
}

bool UKitchenCraftBlueprintLibrary::OpenKitchenPickerOnActor(
	AActor* KitchenActor,
	APlayerController* RequestingController,
	UObject* InventoryProvider)
{
	UKitchenCraftWidgetHostComponent* WidgetHost = FindOrCreateKitchenWidgetHost(KitchenActor);
	if (!WidgetHost)
	{
		return false;
	}

	return WidgetHost->OpenKitchenPicker(RequestingController, InventoryProvider);
}

bool UKitchenCraftBlueprintLibrary::OpenMealPickerOnDiningTableActor(
	AActor* DiningTableActor,
	APlayerController* RequestingController,
	UObject* InventoryProvider)
{
	if (!DiningTableActor)
	{
		UE_LOG(LogKitchenCraftOptions, Warning, TEXT("OpenMealPickerOnDiningTableActor failed: actor missing."));
		return false;
	}

	UDiningTableMealComponent* MealComponent = DiningTableActor->FindComponentByClass<UDiningTableMealComponent>();
	if (!MealComponent)
	{
		UE_LOG(LogKitchenCraftOptions, Warning, TEXT("OpenMealPickerOnDiningTableActor failed: DiningTableMealComponent not found. Actor=%s"),
			*GetNameSafe(DiningTableActor));
		return false;
	}

	UE_LOG(LogKitchenCraftOptions, Log, TEXT("OpenMealPickerOnDiningTableActor opening picker. Actor=%s Component=%s Controller=%s InventoryProvider=%s"),
		*GetNameSafe(DiningTableActor),
		*GetNameSafe(MealComponent),
		*GetNameSafe(RequestingController),
		*GetNameSafe(InventoryProvider));
	return MealComponent->OpenMealPicker(RequestingController, InventoryProvider);
}

bool UKitchenCraftBlueprintLibrary::OpenKitchenPickerOnActorForMode(
	AActor* KitchenActor,
	EKitchenCraftMode Mode,
	APlayerController* RequestingController,
	UObject* InventoryProvider)
{
	UKitchenCraftWidgetHostComponent* WidgetHost = FindOrCreateKitchenWidgetHost(KitchenActor);
	if (!WidgetHost)
	{
		return false;
	}

	return WidgetHost->OpenKitchenPickerForMode(Mode, RequestingController, InventoryProvider);
}

bool UKitchenCraftBlueprintLibrary::TryExecuteKitchenCraftOnActorForMode(
	AActor* KitchenActor,
	EKitchenCraftMode Mode,
	FKitchenCraftResult& Result,
	APlayerController* RequestingController,
	bool bReturnCurrentSelection,
	bool bBlendCamera)
{
	Result = FKitchenCraftResult();
	if (!KitchenActor)
	{
		return false;
	}

	UKitchenCraftStationComponent* KitchenCraftStation = KitchenActor->FindComponentByClass<UKitchenCraftStationComponent>();
	if (!KitchenCraftStation)
	{
		return false;
	}

	return KitchenCraftStation->TryExecuteCraftForMode(Mode, Result, RequestingController, bReturnCurrentSelection, bBlendCamera);
}
