#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "KitchenCraftTypes.generated.h"

class AKitchenIngredientActor;
class UStaticMesh;
class UTexture2D;

UENUM(BlueprintType)
enum class EKitchenCraftMode : uint8
{
	PanCook UMETA(DisplayName = "Pan Cook"),
	IntermediatePrep UMETA(DisplayName = "Intermediate Prep")
};

USTRUCT(BlueprintType)
struct MYPROJECT2_API FKitchenStatBonus
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Stats")
	int32 Quality = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Stats")
	int32 HPBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Stats")
	int32 AttackTierBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Stats")
	int32 AttackSpeedTierBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Stats")
	int32 DashTierBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Stats")
	int32 DashCountBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Stats")
	int32 MoveSpeedBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Stats")
	int32 DropRateBonus = 0;
};

USTRUCT(BlueprintType)
struct MYPROJECT2_API FKitchenItemStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Item")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Item", meta = (ClampMin = "1"))
	int32 Count = 1;

	bool IsValid() const
	{
		return !ItemId.IsNone() && Count > 0;
	}
};

USTRUCT(BlueprintType)
struct MYPROJECT2_API FKitchenItemRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Item")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Item")
	FName ItemTypeName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Item")
	TObjectPtr<UTexture2D> IconTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|World")
	TObjectPtr<UStaticMesh> IngredientMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|World")
	TSubclassOf<AKitchenIngredientActor> IngredientActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Item")
	bool bCanUseAsIngredient = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Stats")
	FKitchenStatBonus BaseStatBonus;
};

USTRUCT(BlueprintType)
struct MYPROJECT2_API FKitchenRecipeRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Recipe")
	TArray<FKitchenItemStack> Inputs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Recipe")
	FName OutputItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Recipe", meta = (ClampMin = "1"))
	int32 OutputCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Recipe")
	FKitchenStatBonus OutputStatBonus;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Recipe")
	bool bExactMatch = true;
};

USTRUCT(BlueprintType)
struct MYPROJECT2_API FKitchenCraftedItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Result")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Result", meta = (ClampMin = "1"))
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Result")
	FKitchenStatBonus StatBonus;
};

USTRUCT(BlueprintType)
struct MYPROJECT2_API FKitchenCraftResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Result")
	bool bSuccess = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Result")
	EKitchenCraftMode Mode = EKitchenCraftMode::PanCook;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Result")
	FName RecipeRowName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Result")
	FKitchenCraftedItem OutputItem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Result")
	FText FailureReason;
};

USTRUCT(BlueprintType)
struct MYPROJECT2_API FKitchenIngredientOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Ingredient")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Ingredient")
	int32 AvailableCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Ingredient")
	FKitchenItemRow ItemData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Inventory")
	int32 InventoryStackIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Inventory")
	bool bHasInventoryStatBonus = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen|Inventory")
	FKitchenStatBonus InventoryStatBonus;
};
