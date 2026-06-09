#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "ButcherResultAutoIconSubsystem.generated.h"

class UUserWidget;

UCLASS()
class MYPROJECT2_API UButcherResultAutoIconSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

private:
	UPROPERTY(Transient)
	TSubclassOf<UUserWidget> ButcherResultWidgetClass;

	TSet<TObjectKey<UUserWidget>> ProcessedWidgets;
};
