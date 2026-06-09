#include "ButcherResultAutoIconSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "ButcherResultBlueprintLibrary.h"
#include "Engine/World.h"

void UButcherResultAutoIconSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ButcherResultWidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/BluePrint/UI/WBP_ButcherResultUI.WBP_ButcherResultUI_C"));
	ProcessedWidgets.Reset();
}

void UButcherResultAutoIconSubsystem::Deinitialize()
{
	ProcessedWidgets.Reset();
	ButcherResultWidgetClass = nullptr;

	Super::Deinitialize();
}

bool UButcherResultAutoIconSubsystem::IsTickable() const
{
	return !IsTemplate() && GetGameInstance() && ButcherResultWidgetClass;
}

TStatId UButcherResultAutoIconSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UButcherResultAutoIconSubsystem, STATGROUP_Tickables);
}

void UButcherResultAutoIconSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || !ButcherResultWidgetClass)
	{
		return;
	}

	TArray<UUserWidget*> ResultWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, ResultWidgets, ButcherResultWidgetClass, false);

	for (UUserWidget* ResultWidget : ResultWidgets)
	{
		if (!ResultWidget)
		{
			continue;
		}

		const TObjectKey<UUserWidget> WidgetKey(ResultWidget);
		if (ProcessedWidgets.Contains(WidgetKey))
		{
			continue;
		}

		if (UButcherResultBlueprintLibrary::ApplyButcherResultIconLayout(ResultWidget, nullptr))
		{
			ProcessedWidgets.Add(WidgetKey);
		}
	}
}
