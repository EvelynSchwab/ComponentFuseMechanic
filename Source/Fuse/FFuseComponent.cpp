
#include "FFuseComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"


static TAutoConsoleVariable<bool> CVarDrawDebugFuser(
	TEXT("f.drawdebugfuser"), true, TEXT("Display debug information from a fuse component"), ECVF_Cheat);

void UFFuseComponent::BeginPlay()
{
	Super::BeginPlay();

	// Set owning character controller reference, for getting the camera in SearchForFusable()
	// If the owning character is not of type actor, do not start fuse tick
	if (const APawn* OwningPawn = Cast<APawn>(GetOwner()))
	{
		OwningCharacterController = Cast<APlayerController>(OwningPawn->GetController());
	}
	
	if (OwningCharacterController == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Owner %s of fuse component %s does not have a valid character controller"),
		       *GetOwner()->GetName(), *this->GetName());
		return;
	}
	
	// Start looping timer for updating fuse functions
	GetWorld()->GetTimerManager().SetTimer(FuseTickTimerHandle, this, &UFFuseComponent::FuseTick,
	                                       1.0f / ComponentUpdateRate, true);
}

void UFFuseComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (CurrentFuserState == FSTATE_ACTIVEFUSING)
	{
		FuseObjects(DeltaTime);
	}
}

void UFFuseComponent::ReleaseComponent()
{
	GetGrabbedComponent()->SetRenderCustomDepth(false);
	GetGrabbedComponent()->SetReceivesDecals(true);
	GetGrabbedComponent()->SetCustomPrimitiveDataFloat(0, 0.0f);
	
	if (LastSpawnedOrthoProjectionActor) { LastSpawnedOrthoProjectionActor->Destroy(); }
	
	Super::ReleaseComponent();
}

bool UFFuseComponent::UpdateFuserState(EFuserState NewState)
{
	if (CurrentFuserState != NewState)
	{
		const EFuserState PreviousState = GetCurrentFuseState();
		CurrentFuserState = NewState;
		OnFuserStateChanged.Broadcast(GetCurrentFuseState(), PreviousState);
		return true;
	}
	return false;
}

void UFFuseComponent::FuseTick()
{
	switch (GetCurrentFuseState())
	{
	case FSTATE_SEARCHING:
		SearchForFusable();
		break;
	case FSTATE_FUSING:
		UpdateHeldFusable();
		break;
	default:
		break;
	}
}

bool UFFuseComponent::TryStartSearching()
{
	if (CurrentFuserState == FSTATE_NONE)
	{
		UpdateFuserState(FSTATE_SEARCHING);
		return true;
	}
	return false;
}

bool UFFuseComponent::TryStopSearching()
{
	if (CurrentFuserState == FSTATE_NONE) { return false; }
	if (GetGrabbedComponent()) { ReleaseComponent(); }
	UpdateFuserState(FSTATE_NONE);
	return true;
}

void UFFuseComponent::SearchForFusable()
{
	FVector CameraLoc;
	FRotator CameraRot;
	OwningCharacterController->GetPlayerViewPoint(CameraLoc, CameraRot);
	
    FCollisionObjectQueryParams ObjectQueryParams;
    FCollisionQueryParams CollisionParams;
    ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionShape CollisionShape;
	CollisionShape.SetSphere(SearchTraceRadius);
	// This won't ignore attached actors, but it's only looking for physics bodies so that won't usually be an issue
	CollisionParams.AddIgnoredActor(GetOwner());

	bool bTraceResult = GetWorld()->SweepSingleByObjectType(
			LastSearchHitResult,
			CameraLoc,
			CameraLoc + CameraRot.Vector() * SearchTraceDistance,
			FQuat::Identity,
			ObjectQueryParams,
			CollisionShape,
			CollisionParams);
	
	// Draw debug info if debug is enabled
	if (CVarDrawDebugFuser.GetValueOnGameThread())
	{
		DrawDebugLine(GetWorld(), CameraLoc, LastSearchHitResult.TraceEnd, FColor::Silver, false, GetDeltaFuseTickTime());
		if (bTraceResult && IsComponentFusable(LastSearchHitResult))
		{
			DrawDebugSphere(GetWorld(), LastSearchHitResult.Location, 10.0f, 8, FColor::Green, false, GetDeltaFuseTickTime());
		}
	}
}

bool UFFuseComponent::IsComponentFusable(FHitResult InHitResult)
{
	if (InHitResult.Component.IsValid())
	{
		for (FName SocketName : InHitResult.Component->GetAllSocketNames())
        {
        	if (SocketName.ToString().Contains("Attach")) { return true; }
        }
	}
	return false;
}

bool UFFuseComponent::TryGrabTargetedFusable()
{
	// Early return if there is no hit component, or we already have a component grabbed
	if (GetGrabbedComponent() || !IsComponentFusable(LastSearchHitResult)) { return false; }

	// Find the target location distance (modified by the inverse of the params that drive the target distance in UpdateHeldFusable())
	FVector CameraLocation;
	FRotator CameraRotation;
	OwningCharacterController->GetPlayerViewPoint(CameraLocation, CameraRotation);
	FVector OwnerXYLocation = GetOwner()->GetActorLocation();
	const float OwnerZHeight = OwnerXYLocation.Z;
	OwnerXYLocation.Z = 0.0f;
	FVector CameraXYLocation = CameraLocation;
	CameraXYLocation.Z = 0.0f;
	FVector ComponentXYLocation = LastSearchHitResult.Component->GetComponentLocation();
	const float ComponentZHeight = ComponentXYLocation.Z;
	ComponentXYLocation.Z = 0.0f;
	GrabbedComponentTargetDistance = FVector::Distance(CameraXYLocation, ComponentXYLocation) - FVector::Distance(OwnerXYLocation, CameraXYLocation);
	GrabbedComponentTargetHeight = ComponentZHeight - OwnerZHeight;
	
	// Derive target local quaternion from world rotation, rounded to nearest ComponentRotationMultiplier
	GrabbedComponentLocalTargetRotator = RoundRotatorToNearestMultiple(InverseRotateRotator(GetOwnerControlRotationYaw(), LastSearchHitResult.Component->GetComponentRotation()), ComponentRotationMultiplier);
	
	GrabComponentAtLocationWithRotation(LastSearchHitResult.GetComponent(), "None",
	                                    LastSearchHitResult.GetComponent()->GetComponentLocation(),
	                                    LastSearchHitResult.GetComponent()->GetComponentRotation());
	UpdateFuserState(FSTATE_FUSING);

	GetGrabbedComponent()->SetRenderCustomDepth(true);
	GetGrabbedComponent()->SetCustomDepthStencilValue(1);
	GetGrabbedComponent()->SetCustomPrimitiveDataFloat(0, 1.0f);
	if (OrthographicProjectionActor)
	{
		LastSpawnedOrthoProjectionActor = GetWorld()->SpawnActor<AActor>(OrthographicProjectionActor, GetGrabbedComponent()->GetComponentLocation(), GetOwnerControlRotationYaw());
		GetGrabbedComponent()->SetReceivesDecals(false);
	}
	
	return true;
}

void UFFuseComponent::AdjustGrabbedComponentTargetDistance(float Delta)
{
	const float NewTargetDistance = GrabbedComponentTargetDistance + Delta;
	GrabbedComponentTargetDistance = FMath::Clamp(NewTargetDistance, MinGrabbedComponentTargetDistance, MaxGrabbedComponentTargetDistance);
}

void UFFuseComponent::AdjustGrabbedComponentTargetRotation(float YawInput, float PitchInput)
{
	YawInput *= ComponentRotationMultiplier;
	PitchInput *= ComponentRotationMultiplier;
	
	FQuat NewRelativeQuat = FQuat(FRotator(PitchInput, YawInput, 0.0f));
	
	GrabbedComponentLocalTargetRotator = FRotator(NewRelativeQuat * FQuat(GrabbedComponentLocalTargetRotator));
}

void UFFuseComponent::AdjustGrabbedComponentTargetHeight(float Delta)
{
	// This doesn't need to be clamped as it's being naturally clamped in UpdateHeldFusable()
	GrabbedComponentTargetHeight += Delta;
}

void UFFuseComponent::UpdateHeldFusable()
{
	if (!GetGrabbedComponent()) { return; }

	/* Target location */
	
	// Sphere trace with same params as search to find the target location for the held object
	// Trace ignores the currently grabbed component
	FVector TargetLocation;
	FHitResult TargetUpLocationHit;
	FHitResult TargetDownLocationHit;
	FHitResult TargetLocationHit;
	
	FVector CameraLoc;
	FRotator CameraRot;
	OwningCharacterController->GetPlayerViewPoint(CameraLoc, CameraRot);
	FVector TraceXYStartLocation = CameraLoc;
	FVector OwnerXYLocation = GetOwner()->GetActorLocation();
	const float OwnerLocationZ = OwnerXYLocation.Z;
	OwnerXYLocation.Z = 0.0f;
	FVector CameraXYLocation = CameraLoc;
	CameraXYLocation.Z = 0.0f;
	float GrabbedComponentRadius = GetGrabbedComponent()->GetLocalBounds().SphereRadius;
	FVector TraceEndLocation = TraceXYStartLocation + (GetOwnerControlRotationYaw().Vector() * (FMath::Max(GrabbedComponentRadius + 100.0f, GrabbedComponentTargetDistance) + FVector::Distance(OwnerXYLocation, CameraXYLocation)));
	TraceEndLocation.Z = GetOwner()->GetActorLocation().Z;

	// Get trace locations for Z target traces from pre-offset value

	
	FCollisionQueryParams CollisionParams;
	// Ignore the current held component
    if (GetGrabbedComponent()) { CollisionParams.AddIgnoredComponent(GetGrabbedComponent()); }
	// This won't ignore attached actors, but it's only looking for physics bodies so that won't usually be an issue
	CollisionParams.AddIgnoredActor(GetOwner());
	
	FCollisionShape CollisionShape;
	CollisionShape.SetSphere(SearchTraceRadius);

	// Trace forward to find the forward max location
	bool bTraceResult = GetWorld()->SweepSingleByChannel(
		TargetLocationHit,
		CameraLoc,
		TraceEndLocation,
		FQuat::Identity, FusableIgnoredTraceChannel, CollisionShape, CollisionParams);

	// Set XY target location
	TargetLocation = bTraceResult && TargetLocationHit.Distance > 0.0f ? TargetLocationHit.Location : TargetLocationHit.TraceEnd;
	
	const FVector TraceZStartLocation = TargetLocation;
    FVector TraceZUpEndLocation = TraceZStartLocation;
    TraceZUpEndLocation.Z += MaxGrabbedComponentTargetHeight;
    FVector TraceZDownEndLocation = TraceZStartLocation;
    TraceZDownEndLocation.Z -= MaxGrabbedComponentTargetHeight;
		
	// Trace up and down from the non-Z-offset target location to find the real min and max values
	float MaxGrabbedComponentHeight = TargetLocation.Z + MaxGrabbedComponentTargetHeight;
    float MinGrabbedComponentHeight = TargetLocation.Z - MaxGrabbedComponentTargetHeight;
	// Trace up
	if (GetWorld()->SweepSingleByChannel(
		TargetUpLocationHit,
		TraceZStartLocation,
		TraceZUpEndLocation,
		FQuat::Identity, FusableIgnoredTraceChannel, CollisionShape, CollisionParams))
	{
		MaxGrabbedComponentHeight = TargetUpLocationHit.Location.Z;
	}
	
	// Trace down
	if (GetWorld()->SweepSingleByChannel(
	TargetDownLocationHit,
	TraceZStartLocation,
	TraceZDownEndLocation,
	FQuat::Identity, FusableIgnoredTraceChannel, CollisionShape, CollisionParams))
	{
		MinGrabbedComponentHeight = TargetDownLocationHit.Location.Z;
	}

	// Draw debug for up and down traces
	if (CVarDrawDebugFuser.GetValueOnGameThread())
	{
		// Up trace
		DrawDebugLine(GetWorld(), TraceZStartLocation, TargetUpLocationHit.TraceEnd, FColor::Black, false, GetDeltaFuseTickTime());
		DrawDebugSphere(GetWorld(), TargetUpLocationHit.Location, 10.0f, 16, FColor::Black, false, GetDeltaFuseTickTime());
		// Down trace
		DrawDebugLine(GetWorld(), TraceZStartLocation, TargetDownLocationHit.TraceEnd, FColor::Black, false, GetDeltaFuseTickTime());
		DrawDebugSphere(GetWorld(), TargetDownLocationHit.Location, 10.0f, 16, FColor::Black, false, GetDeltaFuseTickTime());
	}
	
	// Set Z target location
	GrabbedComponentTargetHeight = FMath::Clamp(GrabbedComponentTargetHeight, MinGrabbedComponentHeight - OwnerLocationZ,  MaxGrabbedComponentHeight - OwnerLocationZ);
	TargetLocation.Z = OwnerLocationZ + GrabbedComponentTargetHeight;
	
	
	// Draw debug info if debug is enabled
	if (CVarDrawDebugFuser.GetValueOnGameThread())
	{
		DrawDebugLine(GetWorld(), CameraLoc, TargetLocationHit.TraceEnd, FColor::Cyan, false, GetDeltaFuseTickTime());
		if (bTraceResult)
		{
			DrawDebugSphere(GetWorld(), TargetLocationHit.Location, 10.0f, 16, FColor::Green, false, GetDeltaFuseTickTime());
		}
	}
	
	/* Target rotation */
	
	// Transform local target quaternion to global rotator, rounded to nearest ComponentRotationMultiplier
	FRotator TargetRotation = FRotator(RotateRotator(GetOwnerControlRotationYaw(), FRotator(RoundRotatorToNearestMultiple(GrabbedComponentLocalTargetRotator, ComponentRotationMultiplier))));

	// Apply location and rotation to held fusable target
	SetTargetLocationAndRotation(TargetLocation, TargetRotation);
	
	// Update location and rotation of orthographic projection actor
	if (LastSpawnedOrthoProjectionActor)
	{
		LastSpawnedOrthoProjectionActor->SetActorLocationAndRotation(GetGrabbedComponent()->GetComponentLocation(), GetOwnerControlRotationYaw());
	}

	// Try and find a pair of fuse sockets for the current held fusable
	// Only run this if the control rotation is significantly different to the last tick rotation
	if (!CameraRot.Equals(OwnerCameraRotCached, 2.0f))
    {
		ClearFuseOperationData();
		TryFindIdealFuseSockets(LastFuseOperationData);
    }
    OwnerCameraRotCached = GetOwnerControlRotation();
}

bool UFFuseComponent::TryFindIdealFuseSockets(FFuseOperationData& FuseOperationData)
{
	/*
	 * Finds nearby fusable actors, and loops through all the sockets to find the nearest pair of sockets on the held
	 * fusable and nearby fusables
	 */
	
	if (GetGrabbedComponent() == nullptr) { return false; }

	// Init the socket distance to MaxFuseDistance before starting distance checks
	FuseOperationData.DistanceBetweenSockets = MaxFuseDistance;
	
	// Trace for potential fusables within held fusable bounds + Max fusable distance radius
	TArray<FHitResult> HitResults;
	const FVector TraceLocation =  GetGrabbedComponent()->GetComponentLocation();
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams CollisionParams;
	CollisionParams.AddIgnoredComponent(GetGrabbedComponent());
	FCollisionShape CollisionShape;
	const float TraceRadius = GetGrabbedComponent()->GetLocalBounds().SphereRadius + MaxFuseDistance;
	CollisionShape.SetSphere(TraceRadius);

	bool bTraceHit = GetWorld()->SweepMultiByObjectType(
                        HitResults,
                        TraceLocation,
                        TraceLocation,
                        FQuat::Identity,
                        ObjectQueryParams,
                        CollisionShape,
                        CollisionParams);

	if (CVarDrawDebugFuser.GetValueOnGameThread())
    {
    	DrawDebugSphere(GetWorld(), TraceLocation, TraceRadius, 16, FColor::Green, false, GetDeltaFuseTickTime());
    }
	
	if (bTraceHit)
	{
		for (FHitResult HitResult : HitResults)
		{
			// Check that the active hit result hit a fusable component
			if (!IsComponentFusable(HitResult)) { continue; }
			
            // Find the nearest two sockets of the two fusables within a max distance
            // Might be a better way to do this than a nested for each loop
            for (FName SourceSocketName : GetGrabbedComponent()->GetAllSocketNames())
            {
                if (SourceSocketName.ToString().Contains(FusableSocketSubName))
                {
                    for (FName TargetSocketName : HitResult.GetComponent()->GetAllSocketNames())
                    {
                        if (TargetSocketName.ToString().Contains(FusableSocketSubName))
                        {
                        	// Check that the latest socket distance is shorter than any previous checks this tick
                        	if (const float SocketDistance = FVector::Distance(
		                            GetGrabbedComponent()->GetSocketLocation(SourceSocketName),
		                            HitResult.GetComponent()->GetSocketLocation(TargetSocketName)) < FuseOperationData.
	                            DistanceBetweenSockets)
                        	{
                        		// Check if the source would collide with the target if transformed to the relevant socket
                        		TArray<FOverlapResult> ComponentOverlapResults;
                        		
                        		FTransform SourceTargetTransform = FindSourceFusableTargetTransform(GetGrabbedComponent(), SourceSocketName, HitResult.GetComponent(), TargetSocketName);
                        		FVector SourceTargetLocation = SourceTargetTransform.GetLocation();
                        		FRotator SourceTargetRotation = FRotator(SourceTargetTransform.GetRotation());
                        		
                        		FComponentQueryParams ComponentQueryParams;
                        		ComponentQueryParams.AddIgnoredComponent(GetGrabbedComponent());
                        		FCollisionObjectQueryParams ComponentObjectQueryParams;
                        		ComponentObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
                        		
                        		bool bCollidesOtherFusable = GetWorld()->ComponentOverlapMulti(ComponentOverlapResults, GetGrabbedComponent(),
                                                                  SourceTargetLocation, SourceTargetRotation,
                                                                  ComponentQueryParams, ComponentObjectQueryParams);
								
                        		/*// Loop through source hits to check if any are fusable objects
                        		// If any are, then this will be considered an invalid operation due to a likely collision
                        		bool bCollidesOtherFusable;
                        		for (FOverlapResult OverlapResult : ComponentOverlapResults)
                        		{
                        			TArray<FName> OverlappedCompSocketNames = OverlapResult.Component->GetAllSocketNames();
                        			for (FName SocketName : OverlappedCompSocketNames)
                        			{
	                                    if (SocketName.ToString().Contains("Attach"))
	                                    {
		                                    bCollidesOtherFusable = true;
	                                    	break;
	                                    }
                        			}
                                    if (bCollidesOtherFusable) { break; }
                        		}*/

                        		// Draw coloured debug capsules to represent possible locations and their collision validity
                        		if (CVarDrawDebugFuser.GetValueOnGameThread())
                        		{
                        			DrawDebugCapsule(GetWorld(), SourceTargetLocation, 30.0f, 10.0f,
	                                                 FQuat(SourceTargetRotation),
	                                                 bCollidesOtherFusable ? FColor::Red : FColor::Blue, false, GetDeltaFuseTickTime(), 1, 5);
                        		}
                        		
                        		// If all checks have passed, promote this loop's sockets as the best operation data
                                if (!bCollidesOtherFusable)
                                {
                                	FuseOperationData.bHasValidFuse = true;
                                	FuseOperationData.DistanceBetweenSockets = SocketDistance;
                                	FuseOperationData.IdealSoureObjectSocket = SourceSocketName;
                                	FuseOperationData.IdealTargetComponent = HitResult.GetComponent();
                                	FuseOperationData.IdealTargetObjectSocket = TargetSocketName;
                                }
                        	}
                        }
                    }
                }
            }
		}
	}
	
	// Get all the socket pairs that are very close together where the fused objects would be, to spawn constraints at those too
	if (FuseOperationData.bHasValidFuse)
	{
		FTransform SourceTargetTransform = FindSourceFusableTargetTransform(GetGrabbedComponent(), FuseOperationData.IdealSoureObjectSocket, FuseOperationData.IdealTargetComponent, FuseOperationData.IdealTargetObjectSocket);
		for (FName SourceSocket : GetGrabbedComponent()->GetAllSocketNames())
		{
			if (SourceSocket != FuseOperationData.IdealSoureObjectSocket)
			{
				for (FName TargetSocket : FuseOperationData.IdealTargetComponent->GetAllSocketNames())
				{
					if (TargetSocket != FuseOperationData.IdealTargetObjectSocket)
					{
						// Get the location of the socket at the target location of the source component
						const FVector SourceSocketLocation = GetGrabbedComponent()->GetSocketLocation(SourceSocket);
						const FVector SourceSocketRelativeLocation = GetGrabbedComponent()->GetComponentTransform().InverseTransformPosition(SourceSocketLocation);
						const FVector SourceSocketTargetLocation = SourceTargetTransform.TransformPosition(SourceSocketRelativeLocation);

						// Check if the distance between the sockets is within a threshold
                        // if the sockets are in that threshold, add them as supplimentary sockets
						if (FuseOperationData.IdealTargetComponent->GetSocketLocation(TargetSocket).Equals(SourceSocketTargetLocation, 5.0f))
						{
							FSupplementalFuseSocketPairs SocketPair;
							SocketPair.SourceSocket = SourceSocket;
							SocketPair.TargetSocket = TargetSocket;
							FuseOperationData.SupplementalSocketPairs.Add(SocketPair);
						}
					}
				}
			}
		}
		
		// draw debug line if debug is enabled
		if (CVarDrawDebugFuser.GetValueOnGameThread())
		{
			const FVector DebugLineStartLoc = GetGrabbedComponent()->GetSocketLocation(FuseOperationData.IdealSoureObjectSocket);
            const FVector DebugLineEndLoc = FuseOperationData.IdealTargetComponent->GetSocketLocation(FuseOperationData.IdealTargetObjectSocket);
            DrawDebugDirectionalArrow(GetWorld(), DebugLineStartLoc, DebugLineEndLoc, 10.0f, FColor::Orange, false, 0.05f, 1, 5.0f);
			for (FSupplementalFuseSocketPairs SocketPair : FuseOperationData.SupplementalSocketPairs)
			{
				const FVector SuppLineStart = GetGrabbedComponent()->GetSocketLocation(SocketPair.SourceSocket);
				const FVector SuppLineEnd = FuseOperationData.IdealTargetComponent->GetSocketLocation(SocketPair.TargetSocket);
				DrawDebugLine(GetWorld(), SuppLineStart, SuppLineEnd, FColor::Emerald, false, 0.05f, 1, 4.0f);
			}
		}
		return true;
	}
	return false;
}

FTransform UFFuseComponent::FindSourceFusableTargetTransform(UPrimitiveComponent* SourceComponent,
	FName SourceSocketName, UPrimitiveComponent* TargetComponent, FName TargetSocketName)
{
	const FRotator SourceSocketRotation = SourceComponent->GetSocketRotation(SourceSocketName);
                        		
	const FRotator TargetComponentRotation = TargetComponent->GetComponentRotation();
	const FRotator SourceTargetRotation = RotateRotator(
		TargetComponentRotation,
		RoundRotatorToNearestMultiple(
			InverseRotateRotator(TargetComponentRotation, SourceSocketRotation),
			ComponentRotationMultiplier));
                        		
	// Get current location of socket
	const FVector TargetSocketLocation = TargetComponent->GetSocketLocation(TargetSocketName);
	const FVector SourceCurrentSocketLocation = SourceComponent->GetSocketLocation(SourceSocketName);
                        		
	// Convert current location to relative location
	const FVector SourceCurrentRelativeSocketLocation = SourceComponent->GetComponentLocation() - SourceCurrentSocketLocation;
	const FVector SourceTargetZeroedSocketLocation = SourceComponent->GetComponentRotation().UnrotateVector(SourceCurrentRelativeSocketLocation);
                        		
	// Rotate socket by current component rotation
	const FVector SourceTargetRelativeSocketLocation = SourceTargetRotation.RotateVector(SourceTargetZeroedSocketLocation);
                        		
	// Rotate by target component rotation
	const FVector SourceTargetLocation = SourceTargetRelativeSocketLocation + TargetSocketLocation;

	FTransform SourceTargetTransform;
	SourceTargetTransform.SetLocation(SourceTargetLocation);
	SourceTargetTransform.SetRotation(FQuat(SourceTargetRotation));
	return SourceTargetTransform;
}

bool UFFuseComponent::TryFuseObjects()
{
	if (LastFuseOperationData.bHasValidFuse && LastFuseOperationData.IdealTargetComponent)
	{
		// Spawn constraint actor
		LastSpawnedConstraintActor = GetWorld()->SpawnActor<APhysicsConstraintActor>(PhysicsConstraintActor, LastFuseOperationData.IdealTargetComponent->GetComponentLocation(), LastFuseOperationData.IdealTargetComponent->GetComponentRotation());
		
		if (LastSpawnedConstraintActor)
		{
			LastFuseOperationData.IdealSourceComponent = GetGrabbedComponent();
			
			LastSpawnedConstraintActor->GetConstraintComp()->AttachToComponent(LastFuseOperationData.IdealTargetComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, LastFuseOperationData.IdealTargetObjectSocket);
			LastSpawnedConstraintActor->GetConstraintComp()->SetAngularTwistLimit(ACM_Locked, 1.0f);
			LastSpawnedConstraintActor->GetConstraintComp()->SetAngularSwing1Limit(ACM_Locked, 1.0f);
			LastSpawnedConstraintActor->GetConstraintComp()->SetAngularSwing2Limit(ACM_Locked, 1.0f);
			
        	ReleaseComponent();
        	
        	UpdateFuserState(FSTATE_ACTIVEFUSING);
            return true;	
		}
	}
	ReleaseComponent();
	UpdateFuserState(FSTATE_NONE);
	return false;
}

void UFFuseComponent::FuseObjects(const float DeltaTime)
{
	/*
	 * This is a fairly unpleasant solution, but it only runs while something is being actively fused so it's not a big hit performance wise
	 * Other solutions caused various issues, mainly clipping or incorrect final locations for constrained objects
	 *
	 * This could be replaced with cleaner logic in a custom UPhysicsConstraintComponent extension, eventually
	 */
	if (LastSpawnedConstraintActor && LastFuseOperationData.IdealSourceComponent && LastFuseOperationData.IdealTargetComponent && LastFuseOperationData.DistanceBetweenSockets < MaxFuseDistance)
	{
		const FTransform SourceTargetTransform = FindSourceFusableTargetTransform(
			LastFuseOperationData.IdealSourceComponent, LastFuseOperationData.IdealSoureObjectSocket,
			LastFuseOperationData.IdealTargetComponent, LastFuseOperationData.IdealTargetObjectSocket);

		// If the total fuse time has exceeded the max before snap, set the location directly
		if (FuseOperationTime > FuseMaxTimeBeforeSnap)
		{
			LastFuseOperationData.IdealSourceComponent->SetWorldLocationAndRotation(SourceTargetTransform.GetLocation(), SourceTargetTransform.GetRotation(), false, nullptr, ETeleportType::ResetPhysics);
		}
		
		// Check if the interp is done
        if (SourceTargetTransform.GetLocation().Equals(LastFuseOperationData.IdealSourceComponent->GetComponentLocation(),0.5f) &&
	        SourceTargetTransform.GetRotation().Equals(FQuat(LastFuseOperationData.IdealSourceComponent->GetComponentRotation()), 0.2f))
        {
        	// Ensure that the constraint and physics are set up if we had to interp it without physics
            if (FuseOperationTime > FuseInterpOperationMaxTime)
            {
	            LastSpawnedConstraintActor->GetConstraintComp()->SetConstrainedComponents(LastFuseOperationData.IdealTargetComponent, "None", LastFuseOperationData.IdealSourceComponent, "None");
                LastFuseOperationData.IdealSourceComponent->SetSimulatePhysics(true);
            	LastFuseOperationData.IdealTargetComponent->SetSimulatePhysics(true);
            }
        	LastSpawnedConstraintActor->GetConstraintComp()->SetAngularTwistLimit(ACM_Limited, 1.0f);
        	LastSpawnedConstraintActor->GetConstraintComp()->SetAngularSwing1Limit(ACM_Limited, 1.0f);
        	LastSpawnedConstraintActor->GetConstraintComp()->SetAngularSwing2Limit(ACM_Limited, 1.0f);
			FuseOperationTime = 0.0f;
        	EndFuseObjects();
        	return;
        }
		
		// Increment the fuse operation time
		FuseOperationTime += DeltaTime;
		// Break constraint
		LastSpawnedConstraintActor->GetConstraintComp()->BreakConstraint();
		LastFuseOperationData.IdealSourceComponent->SetSimulatePhysics(false);
		// Lerp the target transform
        const FVector InterpSourceTargetLocation = FMath::VInterpTo(LastFuseOperationData.IdealSourceComponent->GetComponentLocation(), SourceTargetTransform.GetLocation(), DeltaTime, FuseInterpSpeed);
        const FRotator InterpSourceTargetRotation = FMath::RInterpTo(LastFuseOperationData.IdealSourceComponent->GetComponentRotation(), SourceTargetTransform.Rotator(), DeltaTime, FuseInterpSpeed);
        // Set the new transform
        LastFuseOperationData.IdealSourceComponent->SetWorldLocationAndRotation(InterpSourceTargetLocation, InterpSourceTargetRotation, false, nullptr, ETeleportType::ResetPhysics);
		
		// If the total time exceeds the max time, don't reenable the constraint or physics until the interp is done
		if (FuseOperationTime <= FuseInterpOperationMaxTime)
		{
			// Re-constrain the objects
        	LastSpawnedConstraintActor->GetConstraintComp()->SetConstrainedComponents(LastFuseOperationData.IdealTargetComponent, "None", LastFuseOperationData.IdealSourceComponent, "None");
        	LastFuseOperationData.IdealSourceComponent->SetSimulatePhysics(true);
			return;
		}
		// If the constraint exceeds the max time, disable physics on the target object too to avoid particularly bad physics clipping issues
		LastFuseOperationData.IdealTargetComponent->SetSimulatePhysics(false); 
	}
}

void UFFuseComponent::EndFuseObjects()
{
	// Spawn additional physics constraints on supplementary sockets
	// This only applies to the target component, but could be applied to other objects in the same construction
	for (const FSupplementalFuseSocketPairs SocketPair : LastFuseOperationData.SupplementalSocketPairs)
	{
		if (const APhysicsConstraintActor* SpawnedConstraintActor = GetWorld()->SpawnActor<APhysicsConstraintActor>(PhysicsConstraintActor, LastFuseOperationData.IdealTargetComponent->GetComponentLocation(), LastFuseOperationData.IdealTargetComponent->GetComponentRotation()))
		{
			SpawnedConstraintActor->GetConstraintComp()->AttachToComponent(LastFuseOperationData.IdealTargetComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketPair.TargetSocket);
			SpawnedConstraintActor->GetConstraintComp()->SetConstrainedComponents(LastFuseOperationData.IdealTargetComponent, "None", LastFuseOperationData.IdealSourceComponent, "None");
		}
	}
	ClearFuseOperationData();
    // Reset fuse state
    UpdateFuserState(FSTATE_NONE);
}

bool UFFuseComponent::TryDetachGrabbedComponent()
{
	// Finds the nearby fusable components, gets the attached constraints and searches through them to try find this component in its constraints
	// Not an ideal solution, would be redesigned so that constraints components are part of a large actor so that the components and their relationships could be easily searched through
	if (GetCurrentFuseState() == FSTATE_FUSING && GetGrabbedComponent())
	{
		TArray<AActor*> AttachedActors;
		// Get attached actors for the grabbed comp
		GetGrabbedComponent()->GetOwner()->GetAttachedActors(AttachedActors);
		// Get attached actors for nearby comps
		TArray<FHitResult> HitResults;
		const FVector TraceLocation =  GetGrabbedComponent()->GetComponentLocation();
		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
		FCollisionQueryParams CollisionParams;
		CollisionParams.AddIgnoredComponent(GetGrabbedComponent());
		FCollisionShape CollisionShape;
		const float TraceRadius = GetGrabbedComponent()->GetLocalBounds().SphereRadius + 10.0f;
		CollisionShape.SetSphere(TraceRadius);

		if (GetWorld()->SweepMultiByObjectType(
							HitResults,
							TraceLocation,
							TraceLocation,
							FQuat::Identity,
							ObjectQueryParams,
							CollisionShape,
							CollisionParams))
		{
			for (FHitResult HitResult : HitResults)
			{
				
				HitResult.GetActor()->GetAttachedActors(AttachedActors, false, false);

			}
		}
		
		// Find physics constraint actors within the attached actors, break their constraints and destroy them
		for (AActor* AttachedActor : AttachedActors)
		{
			if (APhysicsConstraintActor* ConstraintActor = Cast<APhysicsConstraintActor>(AttachedActor))
			{
				// Get both the constrained components and break the constraint if either match the grabbed component
				UPrimitiveComponent* ConstrainedCompA;
				FName ConstrainedCompASocket;
				UPrimitiveComponent* ConstrainedCompB;
				FName ConstrainedCompBSocket;
				ConstraintActor->GetConstraintComp()->GetConstrainedComponents(ConstrainedCompA, ConstrainedCompASocket, ConstrainedCompB, ConstrainedCompBSocket);
				if (GetGrabbedComponent() == ConstrainedCompA || GetGrabbedComponent() == ConstrainedCompB)
				{
					ConstraintActor->GetConstraintComp()->BreakConstraint();
                	ConstraintActor->Destroy();	
				}

			}
		}
	}
	return false;
}

void UFFuseComponent::ClearFuseOperationData()
{
	FFuseOperationData ClearedOperationData;
	LastFuseOperationData = ClearedOperationData;
}

bool UFFuseComponent::GetFuseComponentDebugState()
{
	return CVarDrawDebugFuser.GetValueOnGameThread();
}



FRotator UFFuseComponent::GetOwnerControlRotation() const
{
	return OwningCharacterController->GetControlRotation();
}

FRotator UFFuseComponent::GetOwnerControlRotationYaw() const
{
	FRotator PlayerXYControlRot = GetOwnerControlRotation();
 	PlayerXYControlRot.Pitch = 0.0f;
 	PlayerXYControlRot.Roll = 0.0f;
 	return PlayerXYControlRot;
}

FRotator UFFuseComponent::RotateRotator(FRotator RotA, FRotator RotB)
{
	return FRotator(FQuat(RotA) * FQuat(RotB));
}

FRotator UFFuseComponent::InverseRotateRotator(FRotator RotA, FRotator RotB)
{
	return FRotator(FQuat(RotA.GetInverse()) * FQuat(RotB));
}

float UFFuseComponent::RoundFloatToNearestMultiple(float Value, float Multiple) const
{
	return FMath::RoundToFloat(Value/Multiple) * Multiple;
}

FRotator UFFuseComponent::RoundRotatorToNearestMultiple(FRotator InRotator, float Multiple) const
{
	const FRotator OldRot = InRotator;
	FRotator NewRot;
	NewRot.Pitch = RoundFloatToNearestMultiple(OldRot.Pitch, Multiple);
	NewRot.Yaw = RoundFloatToNearestMultiple(OldRot.Yaw, Multiple);
	NewRot.Roll = RoundFloatToNearestMultiple(OldRot.Roll, Multiple);
	return NewRot;
}

