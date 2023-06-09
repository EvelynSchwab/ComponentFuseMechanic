
#pragma once

#include "CoreMinimal.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "PhysicsEngine/PhysicsConstraintActor.h"
#include "FFuseComponent.generated.h"

// Enum for tracking the current state of the fuser (owning character)
UENUM(BlueprintType)
enum EFuserState
{
	FSTATE_NONE			UMETA(DisplayName = "None"),
	FSTATE_SEARCHING	UMETA(DisplayName = "Searching"),
	FSTATE_FUSING		UMETA(DisplayName = "Fusing"),
	FSTATE_ACTIVEFUSING	UMETA(DisplayName = "Active Fusing")
};

// Struct for containing data on supplemental fuse operations
USTRUCT(BlueprintType)
struct FSupplementalFuseSocketPairs
{
	// Supplemental socket on the target component
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Fuse")

	// Supplemental socket on the source component
	FName TargetSocket;
	UPROPERTY(BlueprintReadOnly, Category = "Fuse")
	FName SourceSocket;
};

// Struct for containing data on potential fuse operations
USTRUCT(BlueprintType)
struct FFuseOperationData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Fuse")
	bool bHasValidFuse = false;
	
	// Distance between the two sockets
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuse")
	float DistanceBetweenSockets;

	// Pointer to the source object
	UPROPERTY(BlueprintReadOnly)
	UPrimitiveComponent* IdealSourceComponent;
	
	// Socket on the held object
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuse")
	FName IdealSoureObjectSocket;

	// Pointer to the target object
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuse")
	UPrimitiveComponent* IdealTargetComponent;
	
	// Socket on the target object
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuse")
	FName IdealTargetObjectSocket;

	// Socket on the target object
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fuse")
	TArray<FSupplementalFuseSocketPairs> SupplementalSocketPairs;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFuserStateChanged, EFuserState, NewState, EFuserState, PreviousState);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class FUSE_API UFFuseComponent : public UPhysicsHandleComponent
{
	GENERATED_BODY()

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
public:

	// Overwritten to allow resetting custom depth and tags
	virtual void ReleaseComponent() override;

#pragma region ExposedFunctions
	
	// Manually update the fuser state
	// Generally should be updated through functions like TryStartSearching
	// or TryGrabTargetedFusable
    UFUNCTION(BlueprintCallable, Category = "Fuse")
    bool UpdateFuserState(EFuserState NewState);

	// Try to enter the searching state
	// will fail if the state is fusing or is already searching
	UFUNCTION(BlueprintCallable, Category = "Fuse")
	bool TryStartSearching();

	// Try to exit the searching state
	// Will also exit fusing state and drop held fusables
	// will fail if the state is none
	UFUNCTION(BlueprintCallable, Category = "Fuse")
	bool TryStopSearching();
	
	// Try and grab the targeted fusable component
	// Requires fuser state to be Searching
	// If successful, will change fuser state to Fusing
	UFUNCTION(BlueprintCallable, Category = "Fuse")
	bool TryGrabTargetedFusable();

	// Adjust the target distance of the grabbed component by a delta
	// Value will be reset when a component is grabbed
	UFUNCTION(BlueprintCallable, Category = "Fuse")
	void AdjustGrabbedComponentTargetDistance(float Delta);

	// Add to the held component rotation
	UFUNCTION(BlueprintCallable, Category = "Fuse")
	void AdjustGrabbedComponentTargetRotation(float YawInput, float PitchInput);

	// Adjust the held component height
	UFUNCTION(BlueprintCallable, Category = "Fuse")
	void AdjustGrabbedComponentTargetHeight(float Delta);

	// Try to find and fuse a held object with an adjacent object
	UFUNCTION(BlueprintCallable, Category = "Fuse")
	bool TryFuseObjects();

	// Get the state of cvar f.drawdebugfuser for drawing additional fuse component debug info
	UFUNCTION(BlueprintPure, Category = "Fuse")
	static bool GetFuseComponentDebugState();

	UFUNCTION(BlueprintCallable, Category = "Fuse")
	bool TryDetachGrabbedComponent();
	
#pragma endregion

#pragma region ExposedVars
	
	// Subname included in the socket names of attachable sockets
	// eg. "Attach" for "Attach01", "Attach02" etc.
	UPROPERTY(EditDefaultsOnly, Category = "Fusable")
	FString FusableSocketSubName = "Attach";
	
	// Minimum distance for the grabbed component to be from the component owner
	// Additive to the bounds radius of the grabbed object
	UPROPERTY(EditDefaultsOnly, Category = "Fuse", meta = (ClampMin = 15.0f, ClampMax = 150.0f))
	float MinGrabbedComponentTargetDistance = 50.0f;

	// Maximum distance for the grabbed component to be from the component owner
	// Must be less than the min distance
	UPROPERTY(EditDefaultsOnly, Category = "Fuse", meta = (ClampMin = 200.0f, ClampMax = 2000.0f))
	float MaxGrabbedComponentTargetDistance = 1000.0f;
	
	// Max positive and negative Z offset height for grabbed components, relative to the owning character
	UPROPERTY(EditDefaultsOnly, Category = "Fuse")
	float MaxGrabbedComponentTargetHeight = 500.0f;
	
	// Times the component updates per second
	UPROPERTY(EditDefaultsOnly, Category = "Fuse", meta = (ClampMin = 10, ClampMax = 30))
	int ComponentUpdateRate = 30;

	// Distance to trace when searching for fusable objects
	UPROPERTY(EditDefaultsOnly, Category = "Fuse", meta = (ClampMin = 256.0f, ClampMax = 2000.0f))
	float SearchTraceDistance = 1000.0f;

	// Radius to trace when searching for fusable objects
	UPROPERTY(EditDefaultsOnly, Category = "Fuse", meta = (ClampMin = 1.0f, ClampMax = 50.0f))
	float SearchTraceRadius = 16.0f;

	// Multiplier for rotation values input in AdjustGrabbedComponentTargetRotation
	UPROPERTY(EditDefaultsOnly, Category = "Fuse")
	float ComponentRotationMultiplier = 45.0f;

	// The specific physics constraint actor to spawn when constraining fusables
	UPROPERTY(EditDefaultsOnly, Category = "Fuse")
	TSubclassOf<APhysicsConstraintActor> PhysicsConstraintActor = APhysicsConstraintActor::StaticClass();
	
	// Maximum distance for object fusing
	UPROPERTY(EditDefaultsOnly, Category = "Fuse", meta = (ClampMin = 5.0f, ClampMax = 200.0f))
	float MaxFuseDistance = 75.0f;

	// Speed at which to interpolate fused components together when fusing
	UPROPERTY(EditDefaultsOnly, Category = "Fuse")
	float FuseInterpSpeed = 10.0f;

	// Max time for a physics based interp to take before the fuse object ignores collisions and interps directly
	UPROPERTY(EditDefaultsOnly, Category = "Fuse")
	float FuseInterpOperationMaxTime = 1.5f;

	// Max time for a fuse interp operation before it just sets the location directly
	// In theory the non-physics fuse should prevent this from triggering, but it's here as a failsafe
	UPROPERTY(EditDefaultsOnly, Category = "Fuse")
	float FuseMaxTimeBeforeSnap = 10.0f;

	// Actor responsible for orthographic decal projection
	UPROPERTY(EditDefaultsOnly, Category = "Fuse")
	TSubclassOf<AActor> OrthographicProjectionActor;
	
	// A trace channel that should be ignored by fusables
	// This is to prevent held objects from seeing each other as an
	// obstacle for the target distance
	// Not used for the initial trace to find fusables
	UPROPERTY(EditDefaultsOnly, Category = "Fuse")
	TEnumAsByte<ECollisionChannel> FusableIgnoredTraceChannel = ECC_Camera;

	// Blueprint event for fuser state changed
	UPROPERTY(BlueprintAssignable, Category = "Fuse")
	FOnFuserStateChanged OnFuserStateChanged;

#pragma endregion
	
	/* * Getters */

	// Get the current fuse state
	UFUNCTION(BlueprintPure, Category = "Fuse")
	EFuserState GetCurrentFuseState() const {return CurrentFuserState;}

	// Is the owning pawn of this component currently in a fusing state
	UFUNCTION(BlueprintPure, Category = "Fuse")
	bool IsFusing() const {return !CurrentFuserState == FSTATE_NONE;}

	// Getter for 1 / Increment time, only used for debug drawing since it's not a concrete value
	UFUNCTION(BlueprintPure, Category = "Fuse")
	float GetDeltaFuseTickTime() const {return 1.0f / ComponentUpdateRate;}

	// Getter for the current grabbed component target distance
	UFUNCTION(BlueprintGetter, Category = "Fuse")
	float GetGrabbedComponentTargetDistance() const {return GrabbedComponentTargetDistance;}
	
private:
	UPROPERTY()
	APlayerController* OwningCharacterController;
	EFuserState CurrentFuserState;
	FHitResult LastSearchHitResult;

	// The most recent fuse data
	// May not be valid
	FFuseOperationData LastFuseOperationData;
	void ClearFuseOperationData();
	
	UPROPERTY()
	AActor* LastSpawnedOrthoProjectionActor;
	
	UPROPERTY()
	APhysicsConstraintActor* LastSpawnedConstraintActor;
	
	UPROPERTY(BlueprintGetter = GetGrabbedComponentTargetDistance)
	float GrabbedComponentTargetDistance;

	float GrabbedComponentTargetHeight;
	
	// Rotator that is transformed to the owner's control rotation space for the final target
	FRotator GrabbedComponentLocalTargetRotator;
	
	// Tick function running on a looped timer
	// Not using PrimaryObjectTick.TickInterval as to not mess with the parent
	void FuseTick();
	FTimerHandle FuseTickTimerHandle;
	
	// Trace for a potential fusable object from the owning character's camera viewpoint
	void SearchForFusable();
	bool IsComponentFusable(FHitResult InHitResult);
	// Update location and rotation of held fusable
	void UpdateHeldFusable();

	// Search for nearby fusable objects and loop through sockets to try and find the best fusable sockets
	bool TryFindIdealFuseSockets(FFuseOperationData& FuseOperationData);

	FTransform FindSourceFusableTargetTransform(UPrimitiveComponent* SourceComponent, FName SourceSocketName,
	                                            UPrimitiveComponent* TargetComponent, FName TargetSocketName);	
	
	void FuseObjects(float DeltaTime);
	float FuseOperationTime;

	void EndFuseObjects();
	
	/* Utility */
	
	// Get the owner control rotation, without pitch or roll
	FRotator GetOwnerControlRotation() const;
	FRotator GetOwnerControlRotationYaw() const;
	FRotator OwnerCameraRotCached;

	// Rotate or inverse rotate rotators
	static FRotator RotateRotator(FRotator RotA, FRotator RotB);
	static FRotator InverseRotateRotator(FRotator RotA, FRotator RotB);
	
	float RoundFloatToNearestMultiple(float Value, float Multiple) const;
	FRotator RoundRotatorToNearestMultiple(FRotator InRotator, float Multiple) const;


};

