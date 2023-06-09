
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneCaptureComponent2D.h"
#include "GameFramework/Actor.h"
#include "FFuseOrthoProjectionActor.generated.h"

/*
 *
 * Actor spawned while fusing, used to capture and project orthographic views of the currently fused component.
 * This is using 3 scene capture 2d components, the components should only draw the required elements but it
 * is very costly and could do with a better implementation.
 * 
 */

UCLASS()
class FUSE_API AFFuseOrthoProjectionActor : public AActor
{
	GENERATED_BODY()
	
public:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scene Capture", meta = (AllowPrivateAccess = "true"))
	USceneCaptureComponent2D* SceneCaptureComponentForward;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scene Capture", meta = (AllowPrivateAccess = "true"))
	USceneCaptureComponent2D* SceneCaptureComponentRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scene Capture", meta = (AllowPrivateAccess = "true"))
	USceneCaptureComponent2D* SceneCaptureComponentUp;
	
	// Per second update rate for scene capture components
	UPROPERTY(EditDefaultsOnly, Category = "Orthographic Projection")
	int32 ProjectionUpdateRate = 30;

	// Material used for decal projection
	UPROPERTY(EditDefaultsOnly, Category = "Orthographic Projection|Decal")
	UMaterialInterface* ProjectionDecalMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Orthographic Projection|Decal")
	FName DecalRenderTargetParameterName = "RenderTarget";
	
	// Total length of the decal projection
	UPROPERTY(EditDefaultsOnly, Category = "Orthographic Projection|Decal")
	float DecalLength = 1000.0f;
	
	// Render target for the scene capture forward
	UPROPERTY(EditDefaultsOnly, Category = "Orthographic Projection|Render Targets")
	UTextureRenderTarget2D* RenderTargetForward;

	// Render target for the scene capture forward
	UPROPERTY(EditDefaultsOnly, Category = "Orthographic Projection|Render Targets")
	UTextureRenderTarget2D* RenderTargetRight;

	// Render target for the scene capture forward
	UPROPERTY(EditDefaultsOnly, Category = "Orthographic Projection|Render Targets")
	UTextureRenderTarget2D* RenderTargetUp;
	
	// Render target for the scene capture forward
	UPROPERTY(EditDefaultsOnly, Category = "Orthographic Projection")
	TSubclassOf<USceneCaptureComponent2D> SceneCaptureComponentClass = USceneCaptureComponent2D::StaticClass();

	// Distance to place the orthographic captures from the grabbed object
	UPROPERTY(EditDefaultsOnly, Category = "Orthographic Projection")
	float SceneCaptureComponentDistance = 1000.0f;
	
	AFFuseOrthoProjectionActor();
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

private:

	// Component pointers

	UPROPERTY() UDecalComponent* DecalComponentForward;
	UPROPERTY() UDecalComponent* DecalComponentRight;
	UPROPERTY() UDecalComponent* DecalComponentUp;

	// Spawn and set required values for a scene capture component 2D
	USceneCaptureComponent2D* InitSceneCaptureComponent(UTextureRenderTarget2D* RenderTarget, FRotator CaptureRotation, FName CompName);

	// Spawn and set required values for a decal component;
	UDecalComponent* InitDecalComponent(UTextureRenderTarget2D* RenderTarget, FRotator DecalRotation);
};
