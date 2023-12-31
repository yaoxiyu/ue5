// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimNodeMessages.h"
#include "AlphaBlend.h" // Required for EAlphaBlendOption

#include "AnimNode_Inertialization.generated.h"


// Inertialization: High-Performance Animation Transitions in 'Gears of War'
// David Bollo
// Game Developer Conference 2018
//
// https://www.gdcvault.com/play/1025165/Inertialization
// https://www.gdcvault.com/play/1025331/Inertialization


namespace UE { namespace Anim {

// Event that can be subscribed to request inertialization-based blends
class IInertializationRequester : public IGraphMessage
{
	DECLARE_ANIMGRAPH_MESSAGE_API(IInertializationRequester, ENGINE_API);

public:
	static ENGINE_API const FName Attribute;

	// Request to activate inertialization for a duration.
	// If multiple requests are made on the same inertialization node, the minimum requested time will be used.
	virtual void RequestInertialization(float InRequestedDuration, const UBlendProfile* InBlendProfile = nullptr) = 0;

	// Request to activate inertialization.
	// If multiple requests are made on the same inertialization node, the minimum requested time will be used.
	ENGINE_API virtual void RequestInertialization(const FInertializationRequest& InInertializationRequest);

	// Add a record of this request
	virtual void AddDebugRecord(const FAnimInstanceProxy& InSourceProxy, int32 InSourceNodeId) = 0;
};

}}	// namespace UE::Anim

UENUM()
enum class EInertializationState : uint8
{
	Inactive,		// Inertialization inactive
	Pending,		// Inertialization request pending... prepare to capture the pose difference and then switch to active
	Active			// Inertialization active... apply the previously captured pose difference
};


UENUM()
enum class EInertializationBoneState : uint8
{
	Invalid,		// Invalid bone (ie: bone was present in the skeleton but was not present in the pose when it was captured)
	Valid,			// Valid bone
	Excluded		// Valid bone that is to be excluded from the inertialization request
};


UENUM()
enum class EInertializationSpace : uint8
{
	Default,		// Inertialize in local space (default)
	WorldSpace,		// Inertialize translation and rotation in world space (to conceal discontinuities in actor transform such snapping to a new attach parent)
	WorldRotation	// Inertialize rotation only in world space (to conceal discontinuities in actor orientation)
};

struct FInertializationCurve
{
	FBlendedHeapCurve BlendedCurve;

	UE_DEPRECATED(5.3, "CurveUIDToArrayIndexLUT is no longer used.")
	TArray<uint16> CurveUIDToArrayIndexLUT;

	FInertializationCurve() = default;

	FInertializationCurve(const FInertializationCurve& Other)
	{
		*this = Other;
	}

	FInertializationCurve(FInertializationCurve&& Other)
	{
		*this = MoveTemp(Other);
	}

	FInertializationCurve& operator=(const FInertializationCurve& Other)
	{
		BlendedCurve.CopyFrom(Other.BlendedCurve);
		return *this;
	}

	FInertializationCurve& operator=(FInertializationCurve&& Other)
	{
		BlendedCurve.MoveFrom(Other.BlendedCurve);
		return *this;
	}

	template <typename OtherAllocator>
	void InitFrom(const TBaseBlendedCurve<OtherAllocator>& Other)
	{
		BlendedCurve.CopyFrom(Other);
	}
};

USTRUCT()
struct FInertializationRequest
{
	GENERATED_BODY()

	FInertializationRequest() {}

	FInertializationRequest(float InDuration, const UBlendProfile* InBlendProfile)
		: Duration(InDuration)
		, BlendProfile(InBlendProfile)
	{
	}

	void Clear()
	{
		Duration = -1.0f;
		BlendProfile = nullptr;
		bUseBlendMode = false;
		BlendMode = EAlphaBlendOption::Linear;
		CustomBlendCurve = nullptr;
		Description = FText::GetEmpty();
		NodeId = INDEX_NONE;
		AnimInstance = nullptr;
	}

	friend bool operator==(const FInertializationRequest& A, const FInertializationRequest& B)
	{
		return
			(A.Duration == B.Duration) &&
			(A.BlendProfile == B.BlendProfile) &&
			(A.bUseBlendMode == B.bUseBlendMode) &&
			(A.BlendMode == B.BlendMode) &&
			(A.CustomBlendCurve == B.CustomBlendCurve);
	}

	friend bool operator!=(const FInertializationRequest& A, const FInertializationRequest& B)
	{
		return !(A == B);
	}

	// Blend duration of the inertialization request.
	UPROPERTY(Transient)
	float Duration = -1.0f;

	// Blend profile to control per-joint blend times.
	UPROPERTY(Transient)
	TObjectPtr<const UBlendProfile> BlendProfile = nullptr;

	// If to use the provided blend mode.
	UPROPERTY(Transient)
	bool bUseBlendMode = false;

	// Blend mode to use.
	UPROPERTY(Transient)
	EAlphaBlendOption BlendMode = EAlphaBlendOption::Linear;

	// Custom blend curve to use when use of the blend mode is active.
	UPROPERTY(Transient)
	TObjectPtr<UCurveFloat> CustomBlendCurve = nullptr;

	// Description of the request - used for debugging.
	UPROPERTY(Transient)
	FText Description;

	// Node id from which this request was made.
	UPROPERTY(Transient)
	int32 NodeId = INDEX_NONE;

	// Anim instance from which this request was made.
	UPROPERTY(Transient)
	TObjectPtr<UObject> AnimInstance = nullptr;
};


USTRUCT()
struct FInertializationPose
{
	GENERATED_BODY()

	FTransform ComponentTransform;

	// Bone transforms indexed by skeleton bone index.  Transforms are in local space except for direct descendants of
	// the root which are in component space (ie: they have been multiplied by the root).  Invalid bones (ie: bones
	// that are present in the skeleton but were not present in the pose when it was captured) are all zero
	//
	TArray<FTransform> BoneTransforms;

	// Bone states indexed by skeleton bone index
	//
	TArray<EInertializationBoneState> BoneStates;

	// Snapshot of active curves
	// 
	FInertializationCurve Curves;


	FName AttachParentName;
	float DeltaTime;

	FInertializationPose()
		: ComponentTransform(FTransform::Identity)
		, AttachParentName(NAME_None)
		, DeltaTime(0.0f)
	{
	}
	

	FInertializationPose(const FInertializationPose&) = default;
	FInertializationPose(FInertializationPose&&) = default;
	FInertializationPose& operator=(const FInertializationPose&) = default;
	FInertializationPose& operator=(FInertializationPose&&) = default;

	void InitFrom(const FCompactPose& Pose, const FBlendedCurve& InCurves, const FTransform& InComponentTransform, const FName& InAttachParentName, float InDeltaTime);
};

template <>
struct TUseBitwiseSwap<FInertializationPose>
{
	enum { Value = false };
};


USTRUCT()
struct FInertializationBoneDiff
{
	GENERATED_BODY()

	FVector TranslationDirection;
	FVector RotationAxis;
	FVector ScaleAxis;

	float TranslationMagnitude;
	float TranslationSpeed;

	float RotationAngle;
	float RotationSpeed;

	float ScaleMagnitude;
	float ScaleSpeed;

	FInertializationBoneDiff()
		: TranslationDirection(FVector::ZeroVector)
		, RotationAxis(FVector::ZeroVector)
		, ScaleAxis(FVector::ZeroVector)
		, TranslationMagnitude(0.0f)
		, TranslationSpeed(0.0f)
		, RotationAngle(0.0f)
		, RotationSpeed(0.0f)
		, ScaleMagnitude(0.0f)
		, ScaleSpeed(0.0f)
	{
	}

	void Clear()
	{
		TranslationDirection = FVector::ZeroVector;
		RotationAxis = FVector::ZeroVector;
		ScaleAxis = FVector::ZeroVector;
		TranslationMagnitude = 0.0f;
		TranslationSpeed = 0.0f;
		RotationAngle = 0.0f;
		RotationSpeed = 0.0f;
		ScaleMagnitude = 0.0f;
		ScaleSpeed = 0.0f;
	}
};

struct FInertializationCurveDiffElement : public UE::Anim::FCurveElement
{
	float Delta = 0.0f;
	float Derivative = 0.0f;

	FInertializationCurveDiffElement() = default;

	void Clear()
	{
		Value = 0.0f;
		Delta = 0.0f;
		Derivative = 0.0f;
	}
};


USTRUCT()
struct FInertializationPoseDiff
{
	GENERATED_BODY()

	FInertializationPoseDiff()
		: InertializationSpace(EInertializationSpace::Default)
	{
	}

	void Reset(uint32 NumBonesSlack = 0)
	{
		BoneDiffs.Empty(NumBonesSlack);
		CurveDiffs.Empty();
		InertializationSpace = EInertializationSpace::Default;
	}

	// Initialize the pose difference from the current pose and the two previous snapshots
	//
	// Pose								the current frame's pose
	// ComponentTransform				the current frame's component to world transform
	// AttachParentName					the current frame's attach parent name (for checking if the attachment has changed)
	// Prev1							the previous frame's pose
	// Prev2							the pose from two frames before
	// CurveFilter						filter for curves we don't want to inertialize
	//
	void InitFrom(const FCompactPose& Pose, const FBlendedCurve& Curves, const FTransform& ComponentTransform, const FName& AttachParentName, const FInertializationPose& Prev1, const FInertializationPose& Prev2, const UE::Anim::FCurveFilter& CurveFilter);

	// Apply this difference to a pose, decaying over time as InertializationElapsedTime approaches InertializationDuration
	//
	// Pose								[in/out] the current frame's pose
	// Curves							[in/out] the current frame's animation curves
	// InertializationElapsedTime		time elapsed since the start of the inertialization
	// InertializationDuration			total inertialization duration (used for curves)
	// InertializationDurationPerBone	inertialization duration per bone (indexed by skeleton bone index) (used for pose)
	//
	void ApplyTo(FCompactPose& Pose, FBlendedCurve& Curves, float InertializationElapsedTime, float InertializationDuration, TArrayView<const float> InertializationDurationPerBone) const;

	// Get the inertialization space for this pose diff (for debug display)
	//
	EInertializationSpace GetInertializationSpace() const
	{
		return InertializationSpace;
	}

private:

	static float CalcInertialFloat(float x0, float v0, float t, float t1);

	// Bone differences indexed by skeleton bone index
	TArray<FInertializationBoneDiff> BoneDiffs;

	// Curve differences
	TBaseBlendedCurve<FDefaultAllocator, FInertializationCurveDiffElement> CurveDiffs;

	// Inertialization space (local vs world for situations where we wish to correct a world-space discontinuity such as an abrupt orientation change)
	EInertializationSpace InertializationSpace;
};


USTRUCT(BlueprintInternalUseOnly)
struct FAnimNode_Inertialization : public FAnimNode_Base
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Links)
	FPoseLink Source;

private:

	// Optional default blend profile to use when no blend profile is supplied with the inertialization request
	UPROPERTY(EditAnywhere, Category = BlendProfile, meta = (UseAsBlendProfile = true))
	TObjectPtr<UBlendProfile> DefaultBlendProfile = nullptr;

	// List of curves that should not use inertial blending. These curves will instantly change when inertialization begins.
	UPROPERTY(EditAnywhere, Category = Filter)
	TArray<FName> FilteredCurves;

	/**
	 * Enable this to pre-allocate memory for the node rather than to allocate and deallocate memory when blending
	 * becomes active and inactive. This improves performance, but causes larger memory usage, in particular when you
	 * have multiple Inertialization nodes in an animation graph that are not all used at once.
	 */
	UPROPERTY(EditAnywhere, Category = Memory)
	bool bPreallocateMemory = false;

public: // FAnimNode_Inertialization

	ENGINE_API FAnimNode_Inertialization();
	
	// Request to activate inertialization for a duration.
	// If multiple requests are made on the same inertialization node, the minimum requested time will be used.
	//
	ENGINE_API virtual void RequestInertialization(float Duration, const UBlendProfile* BlendProfile);

	// Request to activate inertialization.
	// If multiple requests are made on the same inertialization node, the minimum requested time will be used.
	//
	ENGINE_API virtual void RequestInertialization(const FInertializationRequest& InertializationRequest);

	// Log an error when a node wants to inertialize but no inertialization ancestor node exists
	//
	static ENGINE_API void LogRequestError(const FAnimationUpdateContext& Context, const int32 NodePropertyIndex);
	static ENGINE_API void LogRequestError(const FAnimationUpdateContext& Context, const FPoseLinkBase& RequesterPoseLink);

public: // FAnimNode_Base

	ENGINE_API virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	ENGINE_API virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	ENGINE_API virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	ENGINE_API virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	ENGINE_API virtual void GatherDebugData(FNodeDebugData& DebugData) override;

	ENGINE_API virtual bool NeedsDynamicReset() const override;
	ENGINE_API virtual void ResetDynamics(ETeleportType InTeleportType) override;

protected:

	// Start Inertialization
	//
	// Computes the inertialization pose difference from the current pose and the two previous poses (to capture velocity).  This function
	// is virtual so that a derived class could optionally regularize the pose snapshots to align better with the current frame's pose
	// before computing the inertial difference (for example to correct for instantaneous changes in the root relative to its children).
	//
	ENGINE_API virtual void StartInertialization(FPoseContext& Context, FInertializationPose& PreviousPose1, FInertializationPose& PreviousPose2, float Duration, TArrayView<const float> DurationPerBone, /*OUT*/ FInertializationPoseDiff& OutPoseDiff);

	// Apply Inertialization
	//
	// Applies the inertialization pose difference to the current pose (feathering down to zero as ElapsedTime approaches Duration).  This
	// function is virtual so that a derived class could optionally adjust the pose based on any regularization done in StartInertialization.
	//
	ENGINE_API virtual void ApplyInertialization(FPoseContext& Context, const FInertializationPoseDiff& PoseDiff, float ElapsedTime, float Duration, TArrayView<const float> DurationPerBone);


private:

	// Snapshots of the actor pose from past frames
	TArray<FInertializationPose> PoseSnapshots;

	// Elapsed delta time between calls to evaluate
	float DeltaTime;

	// Pending inertialization requests
	UPROPERTY(Transient)
	TArray<FInertializationRequest> RequestQueue;

	// Teleport type
	ETeleportType TeleportType;

	// Inertialization state
	EInertializationState InertializationState;
	float InertializationElapsedTime;

	// Inertialization duration for the main inertialization request (used for curve blending and deficit tracking)
	float InertializationDuration;

	// Description for the current inertialization request - used for debugging
	FText InertializationRequestDescription;

	// Node Id for the current inertialization request - used for debugging
	int32 InertializationRequestNodeId = INDEX_NONE;

	// Anim Instance for the current inertialization request - used for debugging
	UPROPERTY(Transient)
	TObjectPtr<UObject> InertializationRequestAnimInstance = nullptr;

	// Inertialization durations indexed by skeleton bone index (used for per-bone blending)
	TCustomBoneIndexArray<float, FSkeletonPoseBoneIndex> InertializationDurationPerBone;

	// Maximum of InertializationDuration and all entries in InertializationDurationPerBone (used for knowing when to shutdown the inertialization)
	float InertializationMaxDuration;

	// Inertialization deficit (for tracking and reducing 'pose melting' when thrashing inertialization requests)
	float InertializationDeficit;

	// Inertialization pose differences
	FInertializationPoseDiff InertializationPoseDiff;

	// Cached curve filter built from FilteredCurves
	UE::Anim::FCurveFilter CurveFilter;
	
	// Reset inertialization timing and state
	void Deactivate();
};
