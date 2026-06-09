#include "KitchenIngredientActor.h"

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "KitchenCraftStationComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicsEngine/BodyInstance.h"

FPrimitiveSceneProxy* UKitchenClickCollisionComponent::CreateSceneProxy()
{
	return nullptr;
}

AKitchenIngredientActor::AKitchenIngredientActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(SceneRoot);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
	MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	MeshComponent->SetGenerateOverlapEvents(false);

	ClickCollisionComponent = CreateDefaultSubobject<UKitchenClickCollisionComponent>(TEXT("ClickCollisionComponent"));
	ClickCollisionComponent->SetupAttachment(MeshComponent);
	ClickCollisionComponent->SetBoxExtent(ClickCollisionExtent);
	ClickCollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ClickCollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	ClickCollisionComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ClickCollisionComponent->SetGenerateOverlapEvents(false);
	ClickCollisionComponent->SetVisibility(false, true);
	ClickCollisionComponent->SetHiddenInGame(true);
}

void AKitchenIngredientActor::BeginPlay()
{
	Super::BeginPlay();

	if (MeshComponent)
	{
		MeshComponent->OnClicked.AddUniqueDynamic(this, &AKitchenIngredientActor::HandleMeshClicked);
	}
	if (ClickCollisionComponent)
	{
		ClickCollisionComponent->OnClicked.AddUniqueDynamic(this, &AKitchenIngredientActor::HandleMeshClicked);
	}
}

void AKitchenIngredientActor::NotifyActorOnClicked(FKey ButtonPressed)
{
	Super::NotifyActorOnClicked(ButtonPressed);
	ReturnToInventory();
}

void AKitchenIngredientActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bIsDropping)
	{
		SetActorTickEnabled(false);
		return;
	}

	DropElapsed += DeltaSeconds;
	const float Alpha = DropDuration <= KINDA_SMALL_NUMBER ? 1.0f : FMath::Clamp(DropElapsed / DropDuration, 0.0f, 1.0f);
	FVector NewLocation = FMath::Lerp(DropStartLocation, DropTargetLocation, Alpha);
	NewLocation.Z += FMath::Sin(Alpha * UE_PI) * DropArcHeight;
	UPrimitiveComponent* DropBody = MeshComponent;
	const FVector PreviousDropLocation = DropBody ? DropBody->GetComponentLocation() : GetActorLocation();
	PendingLandingVelocity = DeltaSeconds > KINDA_SMALL_NUMBER
		? (NewLocation - PreviousDropLocation) / DeltaSeconds
		: FVector::ZeroVector;
	bool bBlockedByCollision = false;
	if (DropBody && DropBody->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
	{
		FHitResult DropHit;
		DropBody->SetWorldLocation(NewLocation, true, &DropHit, ETeleportType::None);
		const AKitchenIngredientActor* HitIngredientActor = DropHit.bBlockingHit ? Cast<AKitchenIngredientActor>(DropHit.GetActor()) : nullptr;
		bBlockedByCollision = HitIngredientActor && HitIngredientActor != this;
		if (bBlockedByCollision)
		{
			SetActorLocation(DropBody->GetComponentLocation(), false, nullptr, ETeleportType::TeleportPhysics);
			DropBody->SetRelativeLocation(FVector::ZeroVector, false, nullptr, ETeleportType::TeleportPhysics);
		}
		else
		{
			SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);
			DropBody->SetRelativeLocation(FVector::ZeroVector, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
	else
	{
		SetActorLocation(NewLocation);
	}

	if (Alpha >= 1.0f || bBlockedByCollision)
	{
		bIsDropping = false;
		if (!bBlockedByCollision)
		{
			SetActorLocation(DropTargetLocation);
			if (DropBody)
			{
				DropBody->SetRelativeLocation(FVector::ZeroVector, false, nullptr, ETeleportType::TeleportPhysics);
			}
		}
		SetActorTickEnabled(false);

		StartLandingPhysics();
		OnDropLanded.Broadcast(this);
	}
}

void AKitchenIngredientActor::InitializeDroppedIngredient(UKitchenCraftStationComponent* InOwnerStation, const FKitchenItemStack& InIngredientStack, UTexture2D* InIconTexture, UStaticMesh* InIngredientMesh)
{
	OwnerStation = InOwnerStation;
	IngredientStack = InIngredientStack;
	CraftedItem = FKitchenCraftedItem();
	bIsCraftResult = false;
	IconTexture = InIconTexture;
	IngredientMesh = InIngredientMesh;

	if (MeshComponent && IngredientMesh)
	{
		MeshComponent->SetStaticMesh(IngredientMesh);
	}
	ConfigureMeshForClickOnly();

	ApplyIconTextureToMeshMaterial();
	OnIngredientVisualDataApplied(IconTexture, IngredientMesh);
}

void AKitchenIngredientActor::InitializeCraftResult(UKitchenCraftStationComponent* InOwnerStation, const FKitchenCraftedItem& InCraftedItem, UTexture2D* InIconTexture, UStaticMesh* InIngredientMesh)
{
	OwnerStation = InOwnerStation;
	IngredientStack = FKitchenItemStack();
	CraftedItem = InCraftedItem;
	bIsCraftResult = true;
	IconTexture = InIconTexture;
	IngredientMesh = InIngredientMesh;
	bIsDropping = false;
	SetActorTickEnabled(false);

	if (MeshComponent)
	{
		MeshComponent->SetSimulatePhysics(false);
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
		MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		if (IngredientMesh)
		{
			MeshComponent->SetStaticMesh(IngredientMesh);
		}
	}
	ConfigureMeshForClickOnly();

	ApplyIconTextureToMeshMaterial();
	OnIngredientVisualDataApplied(IconTexture, IngredientMesh);
}

void AKitchenIngredientActor::StartDropAnimation(FVector InStartLocation, FVector InTargetLocation, float InDuration, float InArcHeight)
{
	DropStartLocation = InStartLocation;
	DropTargetLocation = InTargetLocation;
	DropDuration = FMath::Max(InDuration, 0.01f);
	DropArcHeight = FMath::Max(InArcHeight, 0.0f);
	DropElapsed = 0.0f;
	PendingLandingVelocity = FVector::ZeroVector;
	bIsDropping = true;

	SetActorLocation(DropStartLocation);

	ConfigureMeshAsPhysicsBody(PhysicsLinearDamping, PhysicsAngularDamping);
	if (MeshComponent)
	{
		MeshComponent->SetSimulatePhysics(false);
	}

	if (UPrimitiveComponent* DropBody = MeshComponent)
	{
		DropBody->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	SetActorTickEnabled(true);
}

void AKitchenIngredientActor::StartPhysicsDrop(FVector InStartLocation, float DropLinearDamping, float DropAngularDamping, float HorizontalImpulseScale, float AngularImpulseScale)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhysicsSettleTimerHandle);
	}

	DropStartLocation = InStartLocation;
	DropTargetLocation = InStartLocation;
	DropElapsed = 0.0f;
	PendingLandingVelocity = FVector::ZeroVector;
	bIsDropping = false;
	SetActorTickEnabled(false);
	SetActorLocation(InStartLocation, false, nullptr, ETeleportType::TeleportPhysics);

	UPrimitiveComponent* DropBody = ConfigurePhysicsDropBody(DropLinearDamping, DropAngularDamping);
	if (!DropBody)
	{
		return;
	}

	const FVector HorizontalDirection = FMath::VRand().GetSafeNormal2D();
	const float DropHorizontalImpulse = LandingHorizontalImpulse * FMath::Max(0.0f, HorizontalImpulseScale);
	if (!HorizontalDirection.IsNearlyZero() && DropHorizontalImpulse > 0.0f)
	{
		DropBody->AddImpulse(HorizontalDirection * DropHorizontalImpulse, NAME_None, true);
	}

	const float DropAngularImpulse = LandingAngularImpulse * FMath::Max(0.0f, AngularImpulseScale);
	if (DropAngularImpulse > 0.0f)
	{
		DropBody->AddAngularImpulseInRadians(FMath::VRand() * DropAngularImpulse, NAME_None, true);
	}

	SchedulePhysicsSettle();
	OnDropLanded.Broadcast(this);
}

void AKitchenIngredientActor::StartLandingPhysics()
{
	if (!bEnablePhysicsAfterLanding)
	{
		return;
	}

	UPrimitiveComponent* DropBody = ConfigurePhysicsDropBody(PhysicsLinearDamping, PhysicsAngularDamping);
	if (!DropBody)
	{
		return;
	}

	const FVector RawLandingVelocity = PendingLandingVelocity * FMath::Max(0.0f, LandingDropVelocityScale);
	FVector LandingVelocity = RawLandingVelocity;
	LandingVelocity.Z = FMath::Max(0.0f, LandingVelocity.Z);
	if (!LandingVelocity.IsNearlyZero())
	{
		DropBody->SetPhysicsLinearVelocity(LandingVelocity, false);
	}
	DropBody->WakeAllRigidBodies();

	const FVector HorizontalDirection = FMath::VRand().GetSafeNormal2D();
	const float DownwardSpeed = FMath::Max(0.0f, -RawLandingVelocity.Z);
	const float BounceUpImpulse = LandingUpImpulse + DownwardSpeed * FMath::Max(0.0f, LandingBounceFromFallScale);
	const FVector Impulse = HorizontalDirection * LandingHorizontalImpulse + FVector::UpVector * BounceUpImpulse;
	DropBody->AddImpulse(Impulse, NAME_None, true);

	const FVector AngularImpulse = FMath::VRand() * LandingAngularImpulse;
	DropBody->AddAngularImpulseInRadians(AngularImpulse, NAME_None, true);
	SchedulePhysicsSettle();
}

void AKitchenIngredientActor::SettlePhysicsNow()
{
	SettlePhysicsBody();
}

void AKitchenIngredientActor::ApplyIconTextureToMeshMaterial()
{
	if (!bApplyIconTextureToMaterial || !MeshComponent || !IconTexture || IconTextureParameterName.IsNone())
	{
		return;
	}

	UMaterialInstanceDynamic* DynamicMaterial = MeshComponent->CreateAndSetMaterialInstanceDynamic(0);
	if (DynamicMaterial)
	{
		DynamicMaterial->SetTextureParameterValue(IconTextureParameterName, IconTexture);
	}
}

void AKitchenIngredientActor::ReturnToInventory()
{
	if (bIsDropping)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhysicsSettleTimerHandle);
	}

	if (OwnerStation)
	{
		const bool bHandled = bIsCraftResult
			? OwnerStation->CollectCraftResult(this)
			: OwnerStation->ReturnDroppedIngredient(this);
		if (bHandled)
		{
			OnReturnedToInventory.Broadcast(this);
		}
	}
}

void AKitchenIngredientActor::HandleMeshClicked(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed)
{
	ReturnToInventory();
}

UPrimitiveComponent* AKitchenIngredientActor::ConfigurePhysicsDropBody(float LinearDamping, float AngularDamping)
{
	ConfigureMeshAsPhysicsBody(LinearDamping, AngularDamping);
	return MeshComponent.Get();
}

void AKitchenIngredientActor::ConfigureMeshForClickOnly()
{
	if (!MeshComponent)
	{
		ConfigureClickCollision();
		return;
	}

	MeshComponent->SetRelativeLocation(FVector::ZeroVector, false, nullptr, ETeleportType::TeleportPhysics);
	MeshComponent->SetRelativeRotation(FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
	SetMeshPhysicsLocked(false);
	MeshComponent->SetSimulatePhysics(false);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
	MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ConfigureClickCollision();
}

void AKitchenIngredientActor::ConfigureMeshAsPhysicsBody(float LinearDamping, float AngularDamping)
{
	if (!MeshComponent)
	{
		ConfigureClickCollision();
		return;
	}

	MeshComponent->SetRelativeLocation(FVector::ZeroVector, false, nullptr, ETeleportType::TeleportPhysics);
	MeshComponent->SetRelativeRotation(FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
	SetMeshPhysicsLocked(false);
	MeshComponent->SetSimulatePhysics(false);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
	MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	MeshComponent->SetLinearDamping(FMath::Max(0.0f, LinearDamping));
	MeshComponent->SetAngularDamping(FMath::Max(0.0f, AngularDamping));
	MeshComponent->SetEnableGravity(true);
	MeshComponent->SetUseCCD(true);
	MeshComponent->SetSimulatePhysics(true);
	MeshComponent->WakeAllRigidBodies();
	ConfigureClickCollision();
}

FVector AKitchenIngredientActor::GetSafeClickCollisionExtent() const
{
	return FVector(
		FMath::Max(0.1f, ClickCollisionExtent.X),
		FMath::Max(0.1f, ClickCollisionExtent.Y),
		FMath::Max(0.1f, ClickCollisionExtent.Z));
}

void AKitchenIngredientActor::ConfigureClickCollision()
{
	if (!ClickCollisionComponent)
	{
		return;
	}

	ClickCollisionComponent->SetBoxExtent(GetSafeClickCollisionExtent(), true);
	ClickCollisionComponent->SetRelativeLocation(FVector::ZeroVector, false, nullptr, ETeleportType::TeleportPhysics);
	ClickCollisionComponent->SetRelativeRotation(FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
	ClickCollisionComponent->SetCollisionEnabled(bUseClickCollision ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	ClickCollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	ClickCollisionComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ClickCollisionComponent->SetGenerateOverlapEvents(false);
	ClickCollisionComponent->SetVisibility(false, true);
	ClickCollisionComponent->SetHiddenInGame(true);
}

void AKitchenIngredientActor::SchedulePhysicsSettle()
{
	if (!bAutoSettlePhysicsAfterDrop)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(PhysicsSettleTimerHandle);
	const float Delay = FMath::Max(0.0f, PhysicsSettleDelay);
	if (Delay <= KINDA_SMALL_NUMBER)
	{
		SettlePhysicsBody();
		return;
	}

	World->GetTimerManager().SetTimer(PhysicsSettleTimerHandle, this, &AKitchenIngredientActor::SettlePhysicsBody, Delay, false);
}

void AKitchenIngredientActor::SettlePhysicsBody()
{
	if (!MeshComponent || bIsDropping || bIsCraftResult || !MeshComponent->IsSimulatingPhysics())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhysicsSettleTimerHandle);
	}

	MeshComponent->SetLinearDamping(FMath::Max(0.0f, SettledPhysicsLinearDamping));
	MeshComponent->SetAngularDamping(FMath::Max(0.0f, SettledPhysicsAngularDamping));
	MeshComponent->SetPhysicsLinearVelocity(FVector::ZeroVector, false);
	MeshComponent->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector, false);
	MeshComponent->PutRigidBodyToSleep();

	if (bFreezePhysicsAfterSettle)
	{
		SetMeshPhysicsLocked(true);
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
		MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		MeshComponent->SetEnableGravity(false);
		MeshComponent->SetUseCCD(false);
	}
	ConfigureClickCollision();
}

void AKitchenIngredientActor::SetMeshPhysicsLocked(bool bLocked)
{
	if (!MeshComponent)
	{
		return;
	}

	FBodyInstance* BodyInstance = MeshComponent->GetBodyInstance();
	if (!BodyInstance)
	{
		return;
	}

	BodyInstance->bLockXTranslation = bLocked;
	BodyInstance->bLockYTranslation = bLocked;
	BodyInstance->bLockZTranslation = bLocked;
	BodyInstance->bLockXRotation = bLocked;
	BodyInstance->bLockYRotation = bLocked;
	BodyInstance->bLockZRotation = bLocked;
	MeshComponent->SetConstraintMode(bLocked ? EDOFMode::SixDOF : EDOFMode::Default);
}
