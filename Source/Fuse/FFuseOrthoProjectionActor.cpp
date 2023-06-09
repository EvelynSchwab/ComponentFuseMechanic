
#include "FFuseOrthoProjectionActor.h"
#include "Components/DecalComponent.h"
#include "Engine/TextureRenderTarget2D.h"

AFFuseOrthoProjectionActor::AFFuseOrthoProjectionActor()
{
 	// Actor ticks at ProjectionUpdateRate per second
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1 / ProjectionUpdateRate;

	USceneComponent* SceneComponent = CreateDefaultSubobject<USceneComponent>("Scene Component");
	SetRootComponent(SceneComponent);
	SceneCaptureComponentForward = InitSceneCaptureComponent(RenderTargetForward, FRotator::ZeroRotator, "Capture Component Forward");
	SceneCaptureComponentRight = InitSceneCaptureComponent(RenderTargetRight, FRotator(0.0f, 90.0f, 0.0f), "Capture Component Right");
	SceneCaptureComponentUp = InitSceneCaptureComponent(RenderTargetUp, FRotator(-90.0f, 0.0f, 0.0f), "Capture Component Up");
	
}

void AFFuseOrthoProjectionActor::BeginPlay()
{
	Super::BeginPlay();

	DecalComponentForward = InitDecalComponent(RenderTargetForward, FRotator(0.0f, 0.0f, 90.0f));
	DecalComponentRight = InitDecalComponent(RenderTargetRight, FRotator(0.0f, 90.0f, 90.0f));
	DecalComponentUp = InitDecalComponent(RenderTargetUp,FRotator(-90.0f, 0.0f, 90.0f));
}

void AFFuseOrthoProjectionActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	SceneCaptureComponentForward->CaptureScene();
	SceneCaptureComponentRight->CaptureScene();
	SceneCaptureComponentUp->CaptureScene();
}

USceneCaptureComponent2D* AFFuseOrthoProjectionActor::InitSceneCaptureComponent(UTextureRenderTarget2D* RenderTarget, FRotator CaptureRotation, FName CompName)
{
	USceneCaptureComponent2D* NewCaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(CompName);
	NewCaptureComponent->SetupAttachment(RootComponent);
	NewCaptureComponent->SetRelativeRotation(CaptureRotation);
	NewCaptureComponent->SetRelativeLocation(CaptureRotation.Vector() * -SceneCaptureComponentDistance);
	
	if (RenderTarget) { NewCaptureComponent->TextureTarget = RenderTargetForward; }
	else { UE_LOG(LogTemp, Error, TEXT("Component %s failed to allocate a render target to a SceneCaptureComponent"), *this->GetName()); }
	
	return NewCaptureComponent;
}

UDecalComponent* AFFuseOrthoProjectionActor::InitDecalComponent(UTextureRenderTarget2D* RenderTarget, FRotator DecalRotation)
{
	if (UDecalComponent* NewDecalComponent = NewObject<UDecalComponent>(this, UDecalComponent::StaticClass()))
	{
		NewDecalComponent->RegisterComponent();
		NewDecalComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
		NewDecalComponent->SetRelativeRotation(DecalRotation);
		if (ProjectionDecalMaterial)
		{
			UE_LOG(LogTemp, Error, TEXT("Component %s failed to set an orthographic projection decal material"), *this->GetName());
			UMaterialInstanceDynamic* DynamicDecalMatInstance = UMaterialInstanceDynamic::Create(ProjectionDecalMaterial, NewDecalComponent);
			if (RenderTarget)
			{
				NewDecalComponent->DecalSize = FVector(DecalLength, RenderTarget->SizeX, RenderTarget->SizeY);
				DynamicDecalMatInstance->SetTextureParameterValue(DecalRenderTargetParameterName, RenderTarget);
				NewDecalComponent->SetDecalMaterial(DynamicDecalMatInstance);
			}
			else { UE_LOG(LogTemp, Error, TEXT("Component %s failed to allocate a render target to a decal dynamic material"), *this->GetName()); }
		}
		return NewDecalComponent;
	}
	UE_LOG(LogTemp, Error, TEXT("Component %s failed to spawn a DecalComponent"), *this->GetName());
	return nullptr;
}


