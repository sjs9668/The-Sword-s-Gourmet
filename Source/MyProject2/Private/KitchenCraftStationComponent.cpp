#include "KitchenCraftStationComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "KitchenIngredientActor.h"
#include "KitchenInventoryProvider.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogKitchenCraftStation, Log, All);

namespace
{
FProperty* FindKitchenStructProperty(const UStruct* Struct, const TCHAR* FieldName)
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

TSubclassOf<AKitchenIngredientActor> GetFallbackKitchenIngredientActorClass()
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

UStaticMesh* GetFallbackKitchenIconMesh()
{
	static TWeakObjectPtr<UStaticMesh> CachedMesh;
	if (!CachedMesh.IsValid())
	{
		CachedMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	}

	return CachedMesh.Get();
}

UMaterialInterface* GetFallbackKitchenIconMaterial()
{
	static TWeakObjectPtr<UMaterialInterface> CachedMaterial;
	if (!CachedMaterial.IsValid())
	{
		CachedMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Resource/Material/M_KitchenIngredientIcon.M_KitchenIngredientIcon"));
	}

	return CachedMaterial.Get();
}

bool ReadKitchenNameField(const UStruct* Struct, const void* Container, const TCHAR* FieldName, FName& OutValue)
{
	const FNameProperty* NameProperty = CastField<FNameProperty>(FindKitchenStructProperty(Struct, FieldName));
	if (!NameProperty)
	{
		return false;
	}

	OutValue = NameProperty->GetPropertyValue_InContainer(Container);
	return true;
}

bool ReadKitchenTextField(const UStruct* Struct, const void* Container, const TCHAR* FieldName, FText& OutValue)
{
	const FTextProperty* TextProperty = CastField<FTextProperty>(FindKitchenStructProperty(Struct, FieldName));
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

bool ReadKitchenBoolField(const UStruct* Struct, const void* Container, const TCHAR* FieldName, bool& OutValue)
{
	const FBoolProperty* BoolProperty = CastField<FBoolProperty>(FindKitchenStructProperty(Struct, FieldName));
	if (!BoolProperty)
	{
		return false;
	}

	OutValue = BoolProperty->GetPropertyValue_InContainer(Container);
	return true;
}

FName GetKitchenEnumDisplayName(const UEnum* Enum, int64 Value)
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

bool ReadKitchenEnumFieldAsName(const UStruct* Struct, const void* Container, const TCHAR* FieldName, FName& OutValue)
{
	const FProperty* Property = FindKitchenStructProperty(Struct, FieldName);
	if (!Property)
	{
		return false;
	}

	if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
	{
		const void* ValuePtr = EnumProperty->ContainerPtrToValuePtr<void>(Container);
		const int64 EnumValue = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
		OutValue = GetKitchenEnumDisplayName(EnumProperty->GetEnum(), EnumValue);
		return !OutValue.IsNone();
	}

	if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
	{
		const uint8 EnumValue = ByteProperty->GetPropertyValue_InContainer(Container);
		OutValue = GetKitchenEnumDisplayName(ByteProperty->Enum, EnumValue);
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

	return false;
}

bool IsKitchenFoodTypeName(FName TypeName)
{
	const FString TypeString = TypeName.ToString();
	return TypeString.Equals(TEXT("Food"), ESearchCase::IgnoreCase)
		|| TypeString.EndsWith(TEXT("::Food"), ESearchCase::IgnoreCase);
}

bool IsKitchenMaterialOrIntermediateTypeName(FName TypeName)
{
	const FString TypeString = TypeName.ToString();
	return TypeString.Equals(TEXT("Material"), ESearchCase::IgnoreCase)
		|| TypeString.EndsWith(TEXT("::Material"), ESearchCase::IgnoreCase)
		|| TypeString.Equals(TEXT("Intermediate"), ESearchCase::IgnoreCase)
		|| TypeString.EndsWith(TEXT("::Intermediate"), ESearchCase::IgnoreCase);
}

bool IsKitchenIntermediateTypeName(FName TypeName)
{
	const FString TypeString = TypeName.ToString();
	return TypeString.Equals(TEXT("Intermediate"), ESearchCase::IgnoreCase)
		|| TypeString.EndsWith(TEXT("::Intermediate"), ESearchCase::IgnoreCase);
}

bool ReadKitchenIntField(const UStruct* Struct, const void* Container, const TCHAR* FieldName, int32& OutValue)
{
	const FNumericProperty* NumericProperty = CastField<FNumericProperty>(FindKitchenStructProperty(Struct, FieldName));
	if (!NumericProperty || !NumericProperty->IsInteger())
	{
		return false;
	}

	const void* ValuePtr = NumericProperty->ContainerPtrToValuePtr<void>(Container);
	OutValue = static_cast<int32>(NumericProperty->GetSignedIntPropertyValue(ValuePtr));
	return true;
}

bool ReadKitchenFloatField(const UStruct* Struct, const void* Container, const TCHAR* FieldName, float& OutValue)
{
	const FNumericProperty* NumericProperty = CastField<FNumericProperty>(FindKitchenStructProperty(Struct, FieldName));
	if (!NumericProperty)
	{
		return false;
	}

	const void* ValuePtr = NumericProperty->ContainerPtrToValuePtr<void>(Container);
	OutValue = NumericProperty->IsFloatingPoint()
		? static_cast<float>(NumericProperty->GetFloatingPointPropertyValue(ValuePtr))
		: static_cast<float>(NumericProperty->GetSignedIntPropertyValue(ValuePtr));
	return true;
}

template <typename ObjectType>
bool ReadKitchenObjectField(const UStruct* Struct, const void* Container, const TCHAR* FieldName, TObjectPtr<ObjectType>& OutValue)
{
	const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(FindKitchenStructProperty(Struct, FieldName));
	if (!ObjectProperty)
	{
		return false;
	}

	OutValue = Cast<ObjectType>(ObjectProperty->GetObjectPropertyValue_InContainer(Container));
	return true;
}

bool ReadKitchenStatBonusField(const UStruct* Struct, const void* Container, const TCHAR* FieldName, FKitchenStatBonus& OutValue)
{
	const FStructProperty* StructProperty = CastField<FStructProperty>(FindKitchenStructProperty(Struct, FieldName));
	if (!StructProperty || !StructProperty->Struct)
	{
		return false;
	}

	const void* StatContainer = StructProperty->ContainerPtrToValuePtr<void>(Container);
	ReadKitchenIntField(StructProperty->Struct, StatContainer, TEXT("Quality"), OutValue.Quality);
	ReadKitchenIntField(StructProperty->Struct, StatContainer, TEXT("HPBonus"), OutValue.HPBonus);
	ReadKitchenIntField(StructProperty->Struct, StatContainer, TEXT("AttackTierBonus"), OutValue.AttackTierBonus);
	ReadKitchenIntField(StructProperty->Struct, StatContainer, TEXT("AttackSpeedTierBonus"), OutValue.AttackSpeedTierBonus);
	ReadKitchenIntField(StructProperty->Struct, StatContainer, TEXT("DashTierBonus"), OutValue.DashTierBonus);
	ReadKitchenIntField(StructProperty->Struct, StatContainer, TEXT("DashCountBonus"), OutValue.DashCountBonus);
	ReadKitchenIntField(StructProperty->Struct, StatContainer, TEXT("MoveSpeedBonus"), OutValue.MoveSpeedBonus);
	ReadKitchenIntField(StructProperty->Struct, StatContainer, TEXT("DropRateBonus"), OutValue.DropRateBonus);
	return true;
}

bool ReadKitchenInputArray(const UStruct* Struct, const void* Container, const TCHAR* FieldName, TArray<FKitchenItemStack>& OutInputs)
{
	const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(FindKitchenStructProperty(Struct, FieldName));
	const FStructProperty* InnerStructProperty = ArrayProperty ? CastField<FStructProperty>(ArrayProperty->Inner) : nullptr;
	if (!ArrayProperty || !InnerStructProperty || !InnerStructProperty->Struct)
	{
		return false;
	}

	const void* ArrayContainer = ArrayProperty->ContainerPtrToValuePtr<void>(Container);
	FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayContainer);
	OutInputs.Reset();

	for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
	{
		const void* ElementContainer = ArrayHelper.GetRawPtr(Index);

		FKitchenItemStack Stack;
		ReadKitchenNameField(InnerStructProperty->Struct, ElementContainer, TEXT("ItemId"), Stack.ItemId);
		if (!ReadKitchenIntField(InnerStructProperty->Struct, ElementContainer, TEXT("Count"), Stack.Count))
		{
			Stack.Count = 1;
		}

		if (Stack.IsValid())
		{
			OutInputs.Add(Stack);
		}
	}

	return true;
}

const uint8* FindKitchenDataTableRow(const UDataTable* DataTable, FName RowName)
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

bool ReadKitchenItemRow(const UDataTable* DataTable, FName ItemId, FKitchenItemRow& OutItemData)
{
	OutItemData = FKitchenItemRow();

	const uint8* RowData = FindKitchenDataTableRow(DataTable, ItemId);
	const UScriptStruct* RowStruct = DataTable ? DataTable->GetRowStruct() : nullptr;
	if (!RowData || !RowStruct)
	{
		return false;
	}

	if (!ReadKitchenTextField(RowStruct, RowData, TEXT("DisplayName"), OutItemData.DisplayName))
	{
		FName NameField;
		OutItemData.DisplayName = ReadKitchenNameField(RowStruct, RowData, TEXT("Name"), NameField)
			? FText::FromName(NameField)
			: FText::FromName(ItemId);
	}

	ReadKitchenObjectField(RowStruct, RowData, TEXT("IconTexture"), OutItemData.IconTexture);
	ReadKitchenObjectField(RowStruct, RowData, TEXT("IngredientMesh"), OutItemData.IngredientMesh);
	ReadKitchenEnumFieldAsName(RowStruct, RowData, TEXT("Type"), OutItemData.ItemTypeName);

	bool bCanUseAsIngredient = true;
	if (!ReadKitchenBoolField(RowStruct, RowData, TEXT("bCanUseAsIngredient"), bCanUseAsIngredient))
	{
		ReadKitchenBoolField(RowStruct, RowData, TEXT("CanUseAsIngredient"), bCanUseAsIngredient);
	}
	OutItemData.bCanUseAsIngredient = bCanUseAsIngredient;
	if (IsKitchenFoodTypeName(OutItemData.ItemTypeName))
	{
		OutItemData.bCanUseAsIngredient = false;
	}
	else if (IsKitchenMaterialOrIntermediateTypeName(OutItemData.ItemTypeName))
	{
		OutItemData.bCanUseAsIngredient = true;
	}

	ReadKitchenStatBonusField(RowStruct, RowData, TEXT("BaseStatBonus"), OutItemData.BaseStatBonus);
	return true;
}

bool ReadKitchenRecipeRow(const UDataTable* RecipeTable, const UDataTable* ItemDataTable, FName RowName, FKitchenRecipeRow& OutRecipe);

bool IsKitchenStatBonusZero(const FKitchenStatBonus& Bonus)
{
	return Bonus.Quality == 0
		&& Bonus.HPBonus == 0
		&& Bonus.AttackTierBonus == 0
		&& Bonus.AttackSpeedTierBonus == 0
		&& Bonus.DashTierBonus == 0
		&& Bonus.DashCountBonus == 0
		&& Bonus.MoveSpeedBonus == 0
		&& Bonus.DropRateBonus == 0;
}

void AddKitchenStatBonus(FKitchenStatBonus& InOutBonus, const FKitchenStatBonus& BonusToAdd, int32 Multiplier = 1)
{
	const int32 ClampedMultiplier = FMath::Max(Multiplier, 1);
	InOutBonus.Quality += BonusToAdd.Quality * ClampedMultiplier;
	InOutBonus.HPBonus += BonusToAdd.HPBonus * ClampedMultiplier;
	InOutBonus.AttackTierBonus += BonusToAdd.AttackTierBonus * ClampedMultiplier;
	InOutBonus.AttackSpeedTierBonus += BonusToAdd.AttackSpeedTierBonus * ClampedMultiplier;
	InOutBonus.DashTierBonus += BonusToAdd.DashTierBonus * ClampedMultiplier;
	InOutBonus.DashCountBonus += BonusToAdd.DashCountBonus * ClampedMultiplier;
	InOutBonus.MoveSpeedBonus += BonusToAdd.MoveSpeedBonus * ClampedMultiplier;
	InOutBonus.DropRateBonus += BonusToAdd.DropRateBonus * ClampedMultiplier;
}

bool IsKitchenIntermediateItem(const UDataTable* ItemDataTable, FName ItemId, FKitchenItemRow* OutItemData = nullptr)
{
	FKitchenItemRow ItemData;
	if (!ReadKitchenItemRow(ItemDataTable, ItemId, ItemData))
	{
		return false;
	}

	if (OutItemData)
	{
		*OutItemData = ItemData;
	}

	return IsKitchenIntermediateTypeName(ItemData.ItemTypeName);
}

TArray<FKitchenItemStack> GetKitchenRecipeMatchIngredients(EKitchenCraftMode Mode, const TArray<FKitchenItemStack>& Ingredients, const UDataTable* ItemDataTable)
{
	if (Mode != EKitchenCraftMode::PanCook)
	{
		return Ingredients;
	}

	TArray<FKitchenItemStack> MatchIngredients;
	for (const FKitchenItemStack& Ingredient : Ingredients)
	{
		if (!Ingredient.IsValid())
		{
			continue;
		}

		if (IsKitchenIntermediateItem(ItemDataTable, Ingredient.ItemId))
		{
			continue;
		}

		MatchIngredients.Add(Ingredient);
	}

	return MatchIngredients;
}

bool FindKitchenIntermediateOutputStatBonus(const UDataTable* IntermediateRecipeTable, const UDataTable* ItemDataTable, FName OutputItemId, FKitchenStatBonus& OutBonus)
{
	OutBonus = FKitchenStatBonus();
	if (!IntermediateRecipeTable || OutputItemId.IsNone())
	{
		return false;
	}

	for (const FName& RowName : IntermediateRecipeTable->GetRowNames())
	{
		FKitchenRecipeRow Row;
		if (!ReadKitchenRecipeRow(IntermediateRecipeTable, ItemDataTable, RowName, Row))
		{
			continue;
		}

		if (Row.OutputItemId == OutputItemId)
		{
			OutBonus = Row.OutputStatBonus;
			return true;
		}
	}

	return false;
}

FKitchenStatBonus GetKitchenPanModifierBonus(const TArray<FKitchenItemStack>& Ingredients, const UDataTable* ItemDataTable, const UDataTable* IntermediateRecipeTable)
{
	FKitchenStatBonus ModifierBonus;
	for (const FKitchenItemStack& Ingredient : Ingredients)
	{
		if (!Ingredient.IsValid())
		{
			continue;
		}

		FKitchenItemRow ItemData;
		if (!IsKitchenIntermediateItem(ItemDataTable, Ingredient.ItemId, &ItemData))
		{
			continue;
		}

		FKitchenStatBonus IngredientBonus = ItemData.BaseStatBonus;
		FKitchenStatBonus IntermediateOutputBonus;
		if (FindKitchenIntermediateOutputStatBonus(IntermediateRecipeTable, ItemDataTable, Ingredient.ItemId, IntermediateOutputBonus)
			&& !IsKitchenStatBonusZero(IntermediateOutputBonus))
		{
			IngredientBonus = IntermediateOutputBonus;
		}

		AddKitchenStatBonus(ModifierBonus, IngredientBonus, Ingredient.Count);
	}

	return ModifierBonus;
}

int32 ReadKitchenItemBaseQuality(const UDataTable* ItemDataTable, FName ItemId)
{
	const uint8* RowData = FindKitchenDataTableRow(ItemDataTable, ItemId);
	const UScriptStruct* RowStruct = ItemDataTable ? ItemDataTable->GetRowStruct() : nullptr;
	if (!RowData || !RowStruct)
	{
		return 0;
	}

	int32 BaseQuality = 0;
	ReadKitchenIntField(RowStruct, RowData, TEXT("BaseQuality"), BaseQuality);
	return BaseQuality;
}

int32 CalculateKitchenRecipeQuality(const TArray<FKitchenItemStack>& Inputs, const UDataTable* ItemDataTable, float QualityMultiplier)
{
	int32 TotalQuality = 0;
	int32 TotalCount = 0;

	for (const FKitchenItemStack& Input : Inputs)
	{
		if (!Input.IsValid())
		{
			continue;
		}

		TotalQuality += ReadKitchenItemBaseQuality(ItemDataTable, Input.ItemId) * Input.Count;
		TotalCount += Input.Count;
	}

	if (TotalCount <= 0)
	{
		return 0;
	}

	return FMath::Max(0, FMath::RoundToInt((static_cast<float>(TotalQuality) / static_cast<float>(TotalCount)) * QualityMultiplier));
}

bool ReadKitchenRecipeRow(const UDataTable* RecipeTable, const UDataTable* ItemDataTable, FName RowName, FKitchenRecipeRow& OutRecipe)
{
	OutRecipe = FKitchenRecipeRow();

	const uint8* RowData = FindKitchenDataTableRow(RecipeTable, RowName);
	const UScriptStruct* RowStruct = RecipeTable ? RecipeTable->GetRowStruct() : nullptr;
	if (!RowData || !RowStruct)
	{
		return false;
	}

	ReadKitchenInputArray(RowStruct, RowData, TEXT("Inputs"), OutRecipe.Inputs);
	ReadKitchenNameField(RowStruct, RowData, TEXT("OutputItemId"), OutRecipe.OutputItemId);
	if (!ReadKitchenIntField(RowStruct, RowData, TEXT("OutputCount"), OutRecipe.OutputCount))
	{
		OutRecipe.OutputCount = 1;
	}

	ReadKitchenStatBonusField(RowStruct, RowData, TEXT("OutputStatBonus"), OutRecipe.OutputStatBonus);
	OutRecipe.OutputStatBonus.Quality = ReadKitchenItemBaseQuality(ItemDataTable, OutRecipe.OutputItemId);

	bool bUseQuality = false;
	float QualityMultiplier = 1.0f;
	ReadKitchenBoolField(RowStruct, RowData, TEXT("UseQuality"), bUseQuality);
	ReadKitchenFloatField(RowStruct, RowData, TEXT("QualityMultiplier"), QualityMultiplier);

	if (bUseQuality)
	{
		OutRecipe.OutputStatBonus.Quality = CalculateKitchenRecipeQuality(OutRecipe.Inputs, ItemDataTable, QualityMultiplier);
	}

	return !OutRecipe.OutputItemId.IsNone() && OutRecipe.Inputs.Num() > 0;
}
}

UKitchenCraftStationComponent::UKitchenCraftStationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UKitchenCraftStationComponent::BeginKitchenSession(UObject* InInventoryProvider, EKitchenCraftMode InMode, APlayerController* RequestingController, bool bBlendCamera)
{
	if (bPreventCraftWhileResultWaiting && CurrentMode != InMode && IsValid(PendingCraftResultActor))
	{
		UE_LOG(LogKitchenCraftStation, Warning, TEXT("BeginKitchenSession blocked: pending craft result is waiting. CurrentMode=%s RequestedMode=%s Actor=%s"),
			*UEnum::GetValueAsString(CurrentMode),
			*UEnum::GetValueAsString(InMode),
			*GetNameSafe(PendingCraftResultActor));
		return false;
	}

	ActiveInventoryProvider = InInventoryProvider;
	CurrentMode = InMode;

	if (ActiveInventoryProvider && !ActiveInventoryProvider->GetClass()->ImplementsInterface(UKitchenInventoryProvider::StaticClass()))
	{
		ActiveInventoryProvider = nullptr;
		if (!bAllowSelectionWithoutInventoryProvider)
		{
			return false;
		}
	}

	return !bBlendCamera || BlendToCurrentModeView(RequestingController);
}

void UKitchenCraftStationComponent::EndKitchenSession(bool bReturnReservedIngredients, APlayerController* RequestingController, bool bBlendBack)
{
	if (bReturnReservedIngredients)
	{
		ReturnAllIngredients();
	}
	else
	{
		DestroySpawnedIngredientActors();
		SelectedIngredients.Reset();
		OnSelectionChanged.Broadcast();
	}

	ActiveInventoryProvider = nullptr;

	if (bBlendBack)
	{
		BlendBackToPreviousView(RequestingController);
	}
}

void UKitchenCraftStationComponent::SetCraftMode(EKitchenCraftMode NewMode, bool bReturnCurrentSelection, APlayerController* RequestingController, bool bBlendCamera)
{
	if (CurrentMode == NewMode)
	{
		if (bBlendCamera)
		{
			BlendToCurrentModeView(RequestingController);
		}
		return;
	}

	if (bPreventCraftWhileResultWaiting && IsValid(PendingCraftResultActor))
	{
		UE_LOG(LogKitchenCraftStation, Warning, TEXT("SetCraftMode blocked: pending craft result is waiting. CurrentMode=%s RequestedMode=%s Actor=%s"),
			*UEnum::GetValueAsString(CurrentMode),
			*UEnum::GetValueAsString(NewMode),
			*GetNameSafe(PendingCraftResultActor));
		return;
	}

	if (bReturnCurrentSelection)
	{
		ReturnAllIngredients();
	}

	CurrentMode = NewMode;

	if (bBlendCamera)
	{
		BlendToCurrentModeView(RequestingController);
	}
}

bool UKitchenCraftStationComponent::TrySelectIngredient(FName ItemId, int32 Count, AKitchenIngredientActor*& SpawnedActor)
{
	SpawnedActor = nullptr;

	if (ItemId.IsNone() || Count <= 0)
	{
		return false;
	}

	if (bPreventCraftWhileResultWaiting && IsValid(PendingCraftResultActor))
	{
		return false;
	}

	FKitchenItemRow ItemData;
	if (!GetItemData(ItemId, ItemData))
	{
		return false;
	}

	if (!ItemData.bCanUseAsIngredient)
	{
		return false;
	}

	if (!ReserveIngredient(ItemId, Count))
	{
		return false;
	}

	FKitchenItemStack Ingredient;
	Ingredient.ItemId = ItemId;
	Ingredient.Count = Count;
	AddIngredientStack(Ingredient);
	SpawnedActor = SpawnIngredientActor(Ingredient, ItemData);
	if (!SpawnedActor)
	{
		RemoveIngredientStack(Ingredient);
		ReturnIngredientToProvider(Ingredient);
		OnSelectionChanged.Broadcast();
		return false;
	}

	OnSelectionChanged.Broadcast();
	OnIngredientDropped.Broadcast(SpawnedActor);

	return true;
}

bool UKitchenCraftStationComponent::ReturnDroppedIngredient(AKitchenIngredientActor* IngredientActor)
{
	if (!IngredientActor)
	{
		return false;
	}

	const int32 ActorIndex = SpawnedIngredientActors.IndexOfByKey(IngredientActor);
	if (ActorIndex == INDEX_NONE)
	{
		return false;
	}

	const FKitchenItemStack Ingredient = IngredientActor->GetIngredientStack();
	if (!RemoveIngredientStack(Ingredient))
	{
		return false;
	}

	ReturnIngredientToProvider(Ingredient);

	SpawnedIngredientActors.RemoveAt(ActorIndex);
	OnIngredientReturned.Broadcast(IngredientActor);
	IngredientActor->Destroy();

	OnSelectionChanged.Broadcast();
	return true;
}

bool UKitchenCraftStationComponent::ReturnIngredientAtIndex(int32 IngredientIndex)
{
	if (!SpawnedIngredientActors.IsValidIndex(IngredientIndex))
	{
		return false;
	}

	return ReturnDroppedIngredient(SpawnedIngredientActors[IngredientIndex]);
}

void UKitchenCraftStationComponent::ReturnAllIngredients()
{
	for (const FKitchenItemStack& Ingredient : SelectedIngredients)
	{
		ReturnIngredientToProvider(Ingredient);
	}

	DestroySpawnedIngredientActors();
	SelectedIngredients.Reset();
	OnSelectionChanged.Broadcast();
}

bool UKitchenCraftStationComponent::TryExecuteCurrentCraft(FKitchenCraftResult& Result)
{
	Result = FKitchenCraftResult();
	Result.Mode = CurrentMode;

	UE_LOG(LogKitchenCraftStation, Log, TEXT("TryExecuteCurrentCraft called. Mode=%s SelectedCount=%d PendingResult=%s Provider=%s"),
		*UEnum::GetValueAsString(CurrentMode),
		SelectedIngredients.Num(),
		*GetNameSafe(PendingCraftResultActor),
		*GetNameSafe(ActiveInventoryProvider.Get()));

	if (bPreventCraftWhileResultWaiting && IsValid(PendingCraftResultActor))
	{
		UE_LOG(LogKitchenCraftStation, Warning, TEXT("TryExecuteCurrentCraft blocked: pending result is waiting. Actor=%s"),
			*GetNameSafe(PendingCraftResultActor));
		Result = MakeFailureResult(NSLOCTEXT("KitchenCraft", "PendingResultWaiting", "Collect the cooked dish first."));
		OnCraftCompleted.Broadcast(Result);
		return false;
	}

	if (SelectedIngredients.Num() <= 0)
	{
		UE_LOG(LogKitchenCraftStation, Warning, TEXT("TryExecuteCurrentCraft failed: no selected ingredients."));
		Result = MakeFailureResult(NSLOCTEXT("KitchenCraft", "NoSelectedIngredients", "No ingredients selected."));
		OnCraftCompleted.Broadcast(Result);
		return false;
	}

	auto FinishCraftWithItem = [this, &Result](const FKitchenCraftedItem& CraftedItem, FName RecipeRowName, const FText& NoticeReason) -> bool
	{
		if (CraftedItem.ItemId.IsNone())
		{
			UE_LOG(LogKitchenCraftStation, Warning, TEXT("TryExecuteCurrentCraft failed: recipe output item is empty."));
			Result = MakeFailureResult(NSLOCTEXT("KitchenCraft", "RecipeOutputMissing", "Recipe output item is empty."));
			OnCraftCompleted.Broadcast(Result);
			return false;
		}

		if (!CommitReservedIngredients())
		{
			UE_LOG(LogKitchenCraftStation, Warning, TEXT("TryExecuteCurrentCraft failed: could not commit reserved ingredients."));
			Result = MakeFailureResult(NSLOCTEXT("KitchenCraft", "CommitFailed", "Could not commit reserved ingredients."));
			OnCraftCompleted.Broadcast(Result);
			return false;
		}

		const bool bShouldSpawnCraftResultForPickup = bSpawnCraftResultOnPan || CurrentMode == EKitchenCraftMode::PanCook;
		if (bShouldSpawnCraftResultForPickup)
		{
			AKitchenIngredientActor* ResultActor = SpawnCraftResultActor(CraftedItem);
			if (!ResultActor)
			{
				if (bReturnIngredientsIfOutputAddFails)
				{
					ReturnAllIngredients();
				}

				Result = MakeFailureResult(NSLOCTEXT("KitchenCraft", "ResultSpawnFailed", "Could not spawn cooked dish."));
				OnCraftCompleted.Broadcast(Result);
				return false;
			}
		}
		else if (!AddCraftedItemToProvider(CraftedItem))
		{
			if (bReturnIngredientsIfOutputAddFails)
			{
				ReturnAllIngredients();
			}

			Result = MakeFailureResult(NSLOCTEXT("KitchenCraft", "OutputAddFailed", "Could not add crafted item to inventory."));
			OnCraftCompleted.Broadcast(Result);
			return false;
		}

		DestroySpawnedIngredientActors();
		SelectedIngredients.Reset();
		OnSelectionChanged.Broadcast();

		Result.bSuccess = true;
		Result.Mode = CurrentMode;
		Result.RecipeRowName = RecipeRowName;
		Result.OutputItem = CraftedItem;
		Result.FailureReason = NoticeReason;
		UE_LOG(LogKitchenCraftStation, Log, TEXT("TryExecuteCurrentCraft succeeded. OutputItem=%s Count=%d RecipeRow=%s ResultActor=%s"),
			*CraftedItem.ItemId.ToString(),
			CraftedItem.Count,
			*RecipeRowName.ToString(),
			*GetNameSafe(PendingCraftResultActor));
		OnCraftCompleted.Broadcast(Result);
		return true;
	};

	FKitchenRecipeRow Recipe;
	FName RecipeRowName;
	if (!FindMatchingRecipe(CurrentMode, Recipe, RecipeRowName))
	{
		UE_LOG(LogKitchenCraftStation, Log, TEXT("TryExecuteCurrentCraft: no matching recipe. Mode=%s FallbackEnabled=%s FailedPanDishItemId=%s"),
			*UEnum::GetValueAsString(CurrentMode),
			bCreateFailedPanDishOnRecipeMismatch ? TEXT("true") : TEXT("false"),
			*FailedPanDishItemId.ToString());
		if (CurrentMode == EKitchenCraftMode::PanCook && bCreateFailedPanDishOnRecipeMismatch && !FailedPanDishItemId.IsNone())
		{
			FKitchenCraftedItem FailedDish;
			FailedDish.ItemId = FailedPanDishItemId;
			FailedDish.Count = FMath::Max(FailedPanDishCount, 1);
			FailedDish.StatBonus = FailedPanDishStatBonus;

			return FinishCraftWithItem(FailedDish, NAME_None, NSLOCTEXT("KitchenCraft", "RecipeNotFoundFallback", "No matching recipe."));
		}

		Result = MakeFailureResult(NSLOCTEXT("KitchenCraft", "RecipeNotFound", "No matching recipe."));
		OnCraftCompleted.Broadcast(Result);
		return false;
	}

	FKitchenCraftedItem CraftedItem;
	CraftedItem.ItemId = Recipe.OutputItemId;
	CraftedItem.Count = FMath::Max(Recipe.OutputCount, 1);
	CraftedItem.StatBonus = Recipe.OutputStatBonus;
	if (CurrentMode == EKitchenCraftMode::PanCook)
	{
		AddKitchenStatBonus(CraftedItem.StatBonus, GetKitchenPanModifierBonus(SelectedIngredients, ItemDataTable, IntermediateRecipeDataTable));
	}

	return FinishCraftWithItem(CraftedItem, RecipeRowName, FText::GetEmpty());
}

bool UKitchenCraftStationComponent::TryExecuteCraftForMode(EKitchenCraftMode Mode, FKitchenCraftResult& Result, APlayerController* RequestingController, bool bReturnCurrentSelection, bool bBlendCamera)
{
	SetCraftMode(Mode, bReturnCurrentSelection, RequestingController, bBlendCamera);
	return TryExecuteCurrentCraft(Result);
}

bool UKitchenCraftStationComponent::CollectCraftResult(AKitchenIngredientActor* ResultActor)
{
	if (!ResultActor || ResultActor != PendingCraftResultActor || !ResultActor->IsCraftResultActor())
	{
		return false;
	}

	const FKitchenCraftedItem CraftedItem = ResultActor->GetCraftedItem();
	if (CraftedItem.ItemId.IsNone())
	{
		return false;
	}

	UObject* InventoryProvider = PendingCraftResultInventoryProvider ? PendingCraftResultInventoryProvider.Get() : ActiveInventoryProvider.Get();
	UE_LOG(LogKitchenCraftStation, Log, TEXT("Collecting craft result into inventory. ItemId=%s Count=%d Actor=%s Provider=%s"),
		*CraftedItem.ItemId.ToString(),
		CraftedItem.Count,
		*GetNameSafe(ResultActor),
		*GetNameSafe(InventoryProvider));
	if (!AddCraftedItemToProvider(CraftedItem, InventoryProvider))
	{
		return false;
	}

	PendingCraftResultActor = nullptr;
	PendingCraftResultItem = FKitchenCraftedItem();
	PendingCraftResultInventoryProvider = nullptr;
	PendingCraftResultBaseMeshScale = FVector::OneVector;
	ResultActor->Destroy();
	OnSelectionChanged.Broadcast();
	return true;
}

void UKitchenCraftStationComponent::ApplyCraftResultVisualScale()
{
	if (!IsValid(PendingCraftResultActor) || !PendingCraftResultActor->MeshComponent)
	{
		return;
	}

	const float SafeScale = FMath::Max(CraftResultVisualScale, 0.01f);
	PendingCraftResultActor->MeshComponent->SetRelativeScale3D(PendingCraftResultBaseMeshScale * SafeScale);
}

bool UKitchenCraftStationComponent::FindMatchingRecipe(EKitchenCraftMode Mode, FKitchenRecipeRow& OutRecipe, FName& OutRecipeRowName) const
{
	const TArray<FKitchenItemStack> MatchIngredients = GetKitchenRecipeMatchIngredients(Mode, SelectedIngredients, ItemDataTable);
	return ResolveKitchenRecipe(Mode, MatchIngredients, OutRecipe, OutRecipeRowName);
}

bool UKitchenCraftStationComponent::GetItemData(FName ItemId, FKitchenItemRow& OutItemData) const
{
	return ResolveKitchenItemData(ItemId, OutItemData);
}

bool UKitchenCraftStationComponent::ResolveKitchenItemData_Implementation(FName ItemId, FKitchenItemRow& OutItemData) const
{
	return ReadKitchenItemRow(ItemDataTable, ItemId, OutItemData);
}

bool UKitchenCraftStationComponent::ResolveKitchenRecipe_Implementation(EKitchenCraftMode Mode, const TArray<FKitchenItemStack>& Ingredients, FKitchenRecipeRow& OutRecipe, FName& OutRecipeRowName) const
{
	OutRecipe = FKitchenRecipeRow();
	OutRecipeRowName = NAME_None;

	UDataTable* RecipeTable = GetRecipeTableForMode(Mode);
	if (!RecipeTable)
	{
		return false;
	}

	const TArray<FKitchenItemStack> IngredientsForMatch = GetKitchenRecipeMatchIngredients(Mode, Ingredients, ItemDataTable);
	TArray<FName> RowNames = RecipeTable->GetRowNames();
	for (const FName& RowName : RowNames)
	{
		FKitchenRecipeRow Row;
		if (!ReadKitchenRecipeRow(RecipeTable, ItemDataTable, RowName, Row))
		{
			continue;
		}

		const bool bExactMatch = bRequireExactRecipeMatch && Row.bExactMatch;
		if (DoStacksMatch(IngredientsForMatch, Row.Inputs, bExactMatch))
		{
			OutRecipe = Row;
			OutRecipeRowName = RowName;
			return true;
		}
	}

	return false;
}

bool UKitchenCraftStationComponent::BlendToCurrentModeView(APlayerController* RequestingController)
{
	APlayerController* Controller = ResolveController(RequestingController);
	AActor* ViewActor = GetViewActorForMode(CurrentMode);

	if (!Controller || !ViewActor)
	{
		return false;
	}

	AActor* CurrentViewTarget = Controller->GetViewTarget();
	if (CurrentViewTarget && CurrentViewTarget != ViewActor)
	{
		CachedPreviousViewTarget = CurrentViewTarget;
	}

	CachedController = Controller;
	Controller->SetViewTargetWithBlend(ViewActor, CameraBlendTime, CameraBlendFunction, CameraBlendExp, bCameraLockOutgoing);
	return true;
}

bool UKitchenCraftStationComponent::BlendBackToPreviousView(APlayerController* RequestingController)
{
	APlayerController* Controller = ResolveController(RequestingController);
	if (!Controller)
	{
		Controller = CachedController.Get();
	}

	if (!Controller)
	{
		return false;
	}

	AActor* ReturnTarget = CachedPreviousViewTarget.Get();
	if (!ReturnTarget)
	{
		ReturnTarget = Controller->GetPawn();
	}

	if (!ReturnTarget)
	{
		return false;
	}

	Controller->SetViewTargetWithBlend(ReturnTarget, CameraBlendTime, CameraBlendFunction, CameraBlendExp, bCameraLockOutgoing);
	return true;
}

void UKitchenCraftStationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReturnAllIngredients();
	DestroyPendingCraftResultActor();
	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void UKitchenCraftStationComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UKitchenCraftStationComponent, CraftResultVisualScale))
	{
		ApplyCraftResultVisualScale();
	}
}
#endif

UDataTable* UKitchenCraftStationComponent::GetRecipeTableForMode(EKitchenCraftMode Mode) const
{
	return Mode == EKitchenCraftMode::PanCook ? PanRecipeDataTable : IntermediateRecipeDataTable;
}

AActor* UKitchenCraftStationComponent::GetDropTargetForMode(EKitchenCraftMode Mode) const
{
	return Mode == EKitchenCraftMode::PanCook ? PanDropTarget : IntermediateDropTarget;
}

AActor* UKitchenCraftStationComponent::GetSpawnSourceForMode(EKitchenCraftMode Mode) const
{
	return Mode == EKitchenCraftMode::PanCook ? PanSpawnSource : IntermediateSpawnSource;
}

AActor* UKitchenCraftStationComponent::GetViewActorForMode(EKitchenCraftMode Mode) const
{
	return Mode == EKitchenCraftMode::PanCook ? PanViewActor : IntermediateViewActor;
}

FVector UKitchenCraftStationComponent::GetDropTargetOffsetForMode(EKitchenCraftMode Mode) const
{
	return Mode == EKitchenCraftMode::PanCook ? PanDropTargetOffset : IntermediateDropTargetOffset;
}

FVector UKitchenCraftStationComponent::GetRandomizedDropTargetLocation(EKitchenCraftMode Mode, const FVector& BaseLocation) const
{
	const float ScatterRadius = Mode == EKitchenCraftMode::PanCook ? PanDropScatterRadius : IntermediateDropScatterRadius;
	if (ScatterRadius <= KINDA_SMALL_NUMBER)
	{
		return BaseLocation;
	}

	const float Angle = FMath::FRandRange(0.0f, 2.0f * UE_PI);
	const float Distance = FMath::Sqrt(FMath::FRand()) * ScatterRadius;
	return BaseLocation + FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.0f);
}

APlayerController* UKitchenCraftStationComponent::ResolveController(APlayerController* RequestingController) const
{
	if (RequestingController)
	{
		return RequestingController;
	}

	UWorld* World = GetWorld();
	return World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
}

bool UKitchenCraftStationComponent::ReserveIngredient(FName ItemId, int32 Count) const
{
	if (!ActiveInventoryProvider)
	{
		return bAllowSelectionWithoutInventoryProvider;
	}

	if (!ActiveInventoryProvider->GetClass()->ImplementsInterface(UKitchenInventoryProvider::StaticClass()))
	{
		return bAllowSelectionWithoutInventoryProvider;
	}

	return IKitchenInventoryProvider::Execute_TryReserveKitchenItem(ActiveInventoryProvider, ItemId, Count);
}

void UKitchenCraftStationComponent::ReturnIngredientToProvider(const FKitchenItemStack& Ingredient) const
{
	if (!Ingredient.IsValid() || !ActiveInventoryProvider)
	{
		return;
	}

	if (ActiveInventoryProvider->GetClass()->ImplementsInterface(UKitchenInventoryProvider::StaticClass()))
	{
		IKitchenInventoryProvider::Execute_ReturnReservedKitchenItem(ActiveInventoryProvider, Ingredient.ItemId, Ingredient.Count);
	}
}

bool UKitchenCraftStationComponent::CommitReservedIngredients() const
{
	if (!ActiveInventoryProvider)
	{
		return bAllowSelectionWithoutInventoryProvider;
	}

	if (!ActiveInventoryProvider->GetClass()->ImplementsInterface(UKitchenInventoryProvider::StaticClass()))
	{
		return bAllowSelectionWithoutInventoryProvider;
	}

	return IKitchenInventoryProvider::Execute_CommitReservedKitchenItems(ActiveInventoryProvider, SelectedIngredients);
}

bool UKitchenCraftStationComponent::AddCraftedItemToProvider(const FKitchenCraftedItem& Item, UObject* InventoryProviderOverride) const
{
	UObject* InventoryProvider = InventoryProviderOverride ? InventoryProviderOverride : ActiveInventoryProvider.Get();
	if (!InventoryProvider)
	{
		return bAllowSelectionWithoutInventoryProvider;
	}

	if (!InventoryProvider->GetClass()->ImplementsInterface(UKitchenInventoryProvider::StaticClass()))
	{
		return bAllowSelectionWithoutInventoryProvider;
	}

	UE_LOG(LogKitchenCraftStation, Log, TEXT("Adding crafted item to inventory provider. ItemId=%s Count=%d Provider=%s"),
		*Item.ItemId.ToString(),
		Item.Count,
		*GetNameSafe(InventoryProvider));
	return IKitchenInventoryProvider::Execute_AddKitchenCraftedItem(InventoryProvider, Item);
}

void UKitchenCraftStationComponent::AddIngredientStack(const FKitchenItemStack& Ingredient)
{
	if (!Ingredient.IsValid())
	{
		return;
	}

	for (FKitchenItemStack& Existing : SelectedIngredients)
	{
		if (Existing.ItemId == Ingredient.ItemId)
		{
			Existing.Count += Ingredient.Count;
			return;
		}
	}

	SelectedIngredients.Add(Ingredient);
}

bool UKitchenCraftStationComponent::RemoveIngredientStack(const FKitchenItemStack& Ingredient)
{
	if (!Ingredient.IsValid())
	{
		return false;
	}

	for (int32 Index = 0; Index < SelectedIngredients.Num(); ++Index)
	{
		FKitchenItemStack& Existing = SelectedIngredients[Index];
		if (Existing.ItemId != Ingredient.ItemId)
		{
			continue;
		}

		if (Existing.Count < Ingredient.Count)
		{
			return false;
		}

		Existing.Count -= Ingredient.Count;
		if (Existing.Count <= 0)
		{
			SelectedIngredients.RemoveAt(Index);
		}
		return true;
	}

	return false;
}

TArray<FKitchenItemStack> UKitchenCraftStationComponent::NormalizeStacks(const TArray<FKitchenItemStack>& Stacks)
{
	TArray<FKitchenItemStack> Normalized;
	for (const FKitchenItemStack& Stack : Stacks)
	{
		if (!Stack.IsValid())
		{
			continue;
		}

		bool bFound = false;
		for (FKitchenItemStack& Existing : Normalized)
		{
			if (Existing.ItemId == Stack.ItemId)
			{
				Existing.Count += Stack.Count;
				bFound = true;
				break;
			}
		}

		if (!bFound)
		{
			Normalized.Add(Stack);
		}
	}

	Normalized.Sort([](const FKitchenItemStack& A, const FKitchenItemStack& B)
	{
		return A.ItemId.LexicalLess(B.ItemId);
	});

	return Normalized;
}

bool UKitchenCraftStationComponent::DoStacksMatch(const TArray<FKitchenItemStack>& Selected, const TArray<FKitchenItemStack>& Required, bool bExactMatch)
{
	const TArray<FKitchenItemStack> NormalizedSelected = NormalizeStacks(Selected);
	const TArray<FKitchenItemStack> NormalizedRequired = NormalizeStacks(Required);

	if (bExactMatch && NormalizedSelected.Num() != NormalizedRequired.Num())
	{
		return false;
	}

	for (const FKitchenItemStack& RequiredStack : NormalizedRequired)
	{
		const FKitchenItemStack* MatchingSelected = NormalizedSelected.FindByPredicate([&RequiredStack](const FKitchenItemStack& SelectedStack)
		{
			return SelectedStack.ItemId == RequiredStack.ItemId;
		});

		if (!MatchingSelected)
		{
			return false;
		}

		if (MatchingSelected->Count < RequiredStack.Count)
		{
			return false;
		}

		if (bExactMatch && MatchingSelected->Count != RequiredStack.Count)
		{
			return false;
		}
	}

	return true;
}

AKitchenIngredientActor* UKitchenCraftStationComponent::SpawnIngredientActor(const FKitchenItemStack& Ingredient, const FKitchenItemRow& ItemData)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	TSubclassOf<AKitchenIngredientActor> ActorClass = ItemData.IngredientActorClass ? ItemData.IngredientActorClass : DefaultIngredientActorClass;
	if (!ActorClass)
	{
		return nullptr;
	}

	AActor* DropTarget = GetDropTargetForMode(CurrentMode);
	const FVector DropTargetOffset = GetDropTargetOffsetForMode(CurrentMode);
	const FVector ResolvedDropOffset = DropTarget && bUseDropTargetLocalOffset
		? DropTarget->GetActorTransform().TransformVectorNoScale(DropTargetOffset)
		: DropTargetOffset;
	const FVector BaseTargetLocation = (DropTarget ? DropTarget->GetActorLocation() : GetOwner()->GetActorLocation()) + ResolvedDropOffset;
	const FVector TargetLocation = GetRandomizedDropTargetLocation(CurrentMode, BaseTargetLocation);

	AActor* SpawnSource = GetSpawnSourceForMode(CurrentMode);
	FVector StartLocation = SpawnSource ? SpawnSource->GetActorLocation() : TargetLocation + FVector(0.0f, 0.0f, FallbackSpawnHeight);
	if (bUsePhysicsIngredientDrop)
	{
		StartLocation = FVector(
			TargetLocation.X,
			TargetLocation.Y,
			BaseTargetLocation.Z + FMath::Max(0.0f, PhysicsIngredientDropHeight));
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AKitchenIngredientActor* IngredientActor = World->SpawnActor<AKitchenIngredientActor>(ActorClass, StartLocation, FRotator::ZeroRotator, SpawnParams);
	if (!IngredientActor)
	{
		return nullptr;
	}

	IngredientActor->InitializeDroppedIngredient(this, Ingredient, ItemData.IconTexture, ItemData.IngredientMesh);
	if (bUsePhysicsIngredientDrop)
	{
		IngredientActor->StartPhysicsDrop(
			StartLocation,
			PhysicsIngredientDropLinearDamping,
			PhysicsIngredientDropAngularDamping,
			PhysicsIngredientDropHorizontalImpulseScale,
			PhysicsIngredientDropAngularImpulseScale);
	}
	else
	{
		IngredientActor->StartDropAnimation(StartLocation, TargetLocation, DropDuration, DropArcHeight);
	}
	SpawnedIngredientActors.Add(IngredientActor);
	return IngredientActor;
}

AKitchenIngredientActor* UKitchenCraftStationComponent::SpawnCraftResultActor(const FKitchenCraftedItem& CraftedItem)
{
	UWorld* World = GetWorld();
	if (!World || CraftedItem.ItemId.IsNone())
	{
		UE_LOG(LogKitchenCraftStation, Warning, TEXT("Could not spawn craft result: invalid world or item id. World=%s ItemId=%s"),
			*GetNameSafe(World),
			*CraftedItem.ItemId.ToString());
		return nullptr;
	}

	FKitchenItemRow ItemData;
	if (!GetItemData(CraftedItem.ItemId, ItemData))
	{
		UE_LOG(LogKitchenCraftStation, Warning, TEXT("Could not spawn craft result: item row not found. ItemId=%s ItemDataTable=%s"),
			*CraftedItem.ItemId.ToString(),
			*GetNameSafe(ItemDataTable));
		return nullptr;
	}

	TSubclassOf<AKitchenIngredientActor> ActorClass = ItemData.IngredientActorClass ? ItemData.IngredientActorClass : DefaultIngredientActorClass;
	if (!ActorClass)
	{
		ActorClass = GetFallbackKitchenIngredientActorClass();
	}

	if (!ActorClass)
	{
		UE_LOG(LogKitchenCraftStation, Warning, TEXT("Could not spawn craft result: no actor class. ItemId=%s ItemActorClass=%s DefaultActorClass=%s"),
			*CraftedItem.ItemId.ToString(),
			*GetNameSafe(ItemData.IngredientActorClass.Get()),
			*GetNameSafe(DefaultIngredientActorClass.Get()));
		return nullptr;
	}

	DestroyPendingCraftResultActor();

	AActor* DropTarget = GetDropTargetForMode(CurrentMode);
	const FVector CombinedOffset = GetDropTargetOffsetForMode(CurrentMode) + CraftResultTargetOffset;
	const FVector ResolvedDropOffset = DropTarget && bUseDropTargetLocalOffset
		? DropTarget->GetActorTransform().TransformVectorNoScale(CombinedOffset)
		: CombinedOffset;
	FVector TargetLocation = (DropTarget ? DropTarget->GetActorLocation() : GetOwner()->GetActorLocation()) + ResolvedDropOffset;
	TargetLocation.Z += 8.0f;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AKitchenIngredientActor* ResultActor = World->SpawnActor<AKitchenIngredientActor>(ActorClass, TargetLocation, FRotator::ZeroRotator, SpawnParams);
	if (!ResultActor)
	{
		UE_LOG(LogKitchenCraftStation, Warning, TEXT("Could not spawn craft result actor. ItemId=%s Class=%s Location=%s"),
			*CraftedItem.ItemId.ToString(),
			*GetNameSafe(ActorClass.Get()),
			*TargetLocation.ToString());
		return nullptr;
	}

	ResultActor->InitializeCraftResult(this, CraftedItem, ItemData.IconTexture, ItemData.IngredientMesh);
	if (ResultActor->MeshComponent)
	{
		if (!ResultActor->MeshComponent->GetStaticMesh())
		{
			if (UStaticMesh* FallbackMesh = GetFallbackKitchenIconMesh())
			{
				ResultActor->MeshComponent->SetStaticMesh(FallbackMesh);
			}
		}

		if (!ResultActor->MeshComponent->GetMaterial(0))
		{
			if (UMaterialInterface* FallbackMaterial = GetFallbackKitchenIconMaterial())
			{
				ResultActor->MeshComponent->SetMaterial(0, FallbackMaterial);
				ResultActor->ApplyIconTextureToMeshMaterial();
			}
		}

		ResultActor->MeshComponent->SetVisibility(true, true);
		ResultActor->MeshComponent->SetHiddenInGame(false, true);
	}
	ResultActor->SetActorHiddenInGame(false);
	ResultActor->SetActorEnableCollision(true);
	PendingCraftResultActor = ResultActor;
	PendingCraftResultItem = CraftedItem;
	PendingCraftResultInventoryProvider = ActiveInventoryProvider;
	PendingCraftResultBaseMeshScale = ResultActor->MeshComponent ? ResultActor->MeshComponent->GetRelativeScale3D() : FVector::OneVector;
	ApplyCraftResultVisualScale();
	UE_LOG(LogKitchenCraftStation, Log, TEXT("Spawned craft result. ItemId=%s Actor=%s Class=%s Location=%s Icon=%s Mesh=%s VisualScale=%.2f"),
		*CraftedItem.ItemId.ToString(),
		*GetNameSafe(ResultActor),
		*GetNameSafe(ActorClass.Get()),
		*TargetLocation.ToString(),
		*GetNameSafe(ItemData.IconTexture),
		*GetNameSafe(ItemData.IngredientMesh),
		CraftResultVisualScale);
	return ResultActor;
}

void UKitchenCraftStationComponent::DestroySpawnedIngredientActors()
{
	for (AKitchenIngredientActor* IngredientActor : SpawnedIngredientActors)
	{
		if (IsValid(IngredientActor))
		{
			IngredientActor->Destroy();
		}
	}

	SpawnedIngredientActors.Reset();
}

void UKitchenCraftStationComponent::DestroyPendingCraftResultActor()
{
	if (IsValid(PendingCraftResultActor))
	{
		PendingCraftResultActor->Destroy();
	}

	PendingCraftResultActor = nullptr;
	PendingCraftResultItem = FKitchenCraftedItem();
	PendingCraftResultInventoryProvider = nullptr;
	PendingCraftResultBaseMeshScale = FVector::OneVector;
}

FKitchenCraftResult UKitchenCraftStationComponent::MakeFailureResult(const FText& Reason) const
{
	FKitchenCraftResult Result;
	Result.bSuccess = false;
	Result.Mode = CurrentMode;
	Result.FailureReason = Reason;
	return Result;
}
