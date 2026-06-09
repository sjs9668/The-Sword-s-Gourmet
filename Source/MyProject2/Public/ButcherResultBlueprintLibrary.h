#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ButcherResultBlueprintLibrary.generated.h"

class UDataTable;
class UUserWidget;

UCLASS()
class MYPROJECT2_API UButcherResultBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Butcher|UI", meta = (DefaultToSelf = "ResultWidget", AdvancedDisplay = "ItemDataTable", ExpandBoolAsExecs = "ReturnValue"))
	static bool ApplyButcherResultIconLayout(UUserWidget* ResultWidget, UDataTable* ItemDataTable = nullptr);
};
