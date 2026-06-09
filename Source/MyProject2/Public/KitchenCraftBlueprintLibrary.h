#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "KitchenCraftTypes.h"
#include "KitchenCraftBlueprintLibrary.generated.h"

class UKitchenCraftStationComponent;
class AActor;
class APlayerController;
class UDataTable;

UCLASS()
class MYPROJECT2_API UKitchenCraftBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Kitchen|UI")
	static bool GetKitchenIngredientOptions(
		UKitchenCraftStationComponent* KitchenCraftStation,
		UObject* InventoryProvider,
		TArray<FKitchenIngredientOption>& OutOptions,
		bool bOnlyAvailable = true);

	UFUNCTION(BlueprintCallable, Category = "Kitchen|UI")
	static bool GetKitchenFoodOptions(
		UDataTable* ItemDataTable,
		UObject* InventoryProvider,
		TArray<FKitchenIngredientOption>& OutOptions,
		bool bOnlyAvailable = true);

	UFUNCTION(BlueprintCallable, Category = "Kitchen|Inventory")
	static bool GetKitchenFoodStatBonusFromInventory(
		UObject* InventoryProvider,
		FName ItemId,
		FKitchenStatBonus FallbackStatBonus,
		FKitchenStatBonus& OutStatBonus);

	UFUNCTION(BlueprintCallable, Category = "Kitchen|Inventory", meta = (ExpandBoolAsExecs = "ReturnValue"))
	static bool ConsumeKitchenFoodForTable(
		UObject* InventoryProvider,
		FName ItemId,
		int32 Count,
		FKitchenStatBonus FallbackStatBonus,
		FKitchenStatBonus& ConsumedStatBonus);

	UFUNCTION(BlueprintCallable, Category = "Kitchen|Inventory", meta = (ExpandBoolAsExecs = "ReturnValue"))
	static bool ConsumeKitchenFoodStackForTable(
		UObject* InventoryProvider,
		FName ItemId,
		int32 InventoryStackIndex,
		int32 Count,
		FKitchenStatBonus FallbackStatBonus,
		FKitchenStatBonus& ConsumedStatBonus);

	UFUNCTION(BlueprintCallable, Category = "Kitchen|UI", meta = (DefaultToSelf = "KitchenActor", ExpandBoolAsExecs = "ReturnValue", AdvancedDisplay = "RequestingController,InventoryProvider"))
	static bool OpenKitchenPickerOnActor(
		AActor* KitchenActor,
		APlayerController* RequestingController = nullptr,
		UObject* InventoryProvider = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Dining|Meal", meta = (DefaultToSelf = "DiningTableActor", ExpandBoolAsExecs = "ReturnValue", AdvancedDisplay = "RequestingController,InventoryProvider", DisplayName = "Open Meal Picker On Dining Table Actor", Keywords = "Dining Table Meal Food Picker"))
	static bool OpenMealPickerOnDiningTableActor(
		AActor* DiningTableActor,
		APlayerController* RequestingController = nullptr,
		UObject* InventoryProvider = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Kitchen|UI", meta = (DefaultToSelf = "KitchenActor", ExpandBoolAsExecs = "ReturnValue", AdvancedDisplay = "RequestingController,InventoryProvider"))
	static bool OpenKitchenPickerOnActorForMode(
		AActor* KitchenActor,
		EKitchenCraftMode Mode,
		APlayerController* RequestingController = nullptr,
		UObject* InventoryProvider = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Kitchen|Craft", meta = (DefaultToSelf = "KitchenActor", ExpandBoolAsExecs = "ReturnValue", AdvancedDisplay = "RequestingController,bReturnCurrentSelection,bBlendCamera"))
	static bool TryExecuteKitchenCraftOnActorForMode(
		AActor* KitchenActor,
		EKitchenCraftMode Mode,
		FKitchenCraftResult& Result,
		APlayerController* RequestingController = nullptr,
		bool bReturnCurrentSelection = false,
		bool bBlendCamera = false);
};
