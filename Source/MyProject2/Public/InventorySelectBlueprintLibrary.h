#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InventorySelectBlueprintLibrary.generated.h"

class UDataTable;
class UUserWidget;

UCLASS()
class MYPROJECT2_API UInventorySelectBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Inventory|UI", meta = (DisplayName = "Refresh Inventory Select", DefaultToSelf = "InventorySelectWidget", HidePin = "InventorySelectWidget", ExpandBoolAsExecs = "ReturnValue", AdvancedDisplay = "InventoryComponent,ItemDataTable,EntryWidgetClass,ListWidgetName"))
	static bool RefreshInventorySelect(
		UUserWidget* InventorySelectWidget,
		UObject* InventoryComponent = nullptr,
		UDataTable* ItemDataTable = nullptr,
		TSubclassOf<UUserWidget> EntryWidgetClass = nullptr,
		FName ListWidgetName = NAME_None);
};
