#pragma once

#include "CoreMinimal.h"
#include "KitchenCraftTypes.h"
#include "UObject/Interface.h"
#include "KitchenInventoryProvider.generated.h"

UINTERFACE(BlueprintType)
class MYPROJECT2_API UKitchenInventoryProvider : public UInterface
{
	GENERATED_BODY()
};

class MYPROJECT2_API IKitchenInventoryProvider
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Kitchen|Inventory")
	bool TryReserveKitchenItem(FName ItemId, int32 Count);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Kitchen|Inventory")
	void ReturnReservedKitchenItem(FName ItemId, int32 Count);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Kitchen|Inventory")
	bool CommitReservedKitchenItems(const TArray<FKitchenItemStack>& Items);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Kitchen|Inventory")
	bool AddKitchenCraftedItem(const FKitchenCraftedItem& Item);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Kitchen|Inventory")
	int32 GetKitchenItemCount(FName ItemId);
};
