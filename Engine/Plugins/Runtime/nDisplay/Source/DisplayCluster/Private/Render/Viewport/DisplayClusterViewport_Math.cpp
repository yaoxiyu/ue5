// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"

#include "Render/Viewport/Configuration/DisplayClusterViewportConfiguration.h"

#include "Render/Projection/IDisplayClusterProjectionPolicy.h"

#include "Render/Viewport/DisplayClusterViewportStereoscopicPass.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrame.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"
#include "Render/Viewport/RenderTarget/DisplayClusterRenderTargetResource.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_PostRenderSettings.h"

#include "EngineUtils.h"
#include "SceneView.h"

#include "DisplayClusterSceneViewExtensions.h"

#include "Misc/DisplayClusterLog.h"

///////////////////////////////////////////////////////////////////////////////////////
//          FDisplayClusterViewport
///////////////////////////////////////////////////////////////////////////////////////

bool FDisplayClusterViewport::CalculateView(const uint32 InContextNum, FVector& InOutViewLocation, FRotator& InOutViewRotation, const FVector& ViewOffset, const float WorldToMeters, const float NCP, const float FCP)
{
	if (Contexts.IsValidIndex(InContextNum))
	{
		if (!EnumHasAnyFlags(Contexts[InContextNum].ContextState, EDisplayClusterViewportContextState::InvalidViewPoint))
		{
			// The function can be called several times per frame.
			// Each time it must return the same values. For optimization purposes, after the first call this function
			// stores the result in the context variables 'ViewLocation' and 'ViewRotation'.
			// Finally, raises this flag for subsequent calls in the current frame.

			if (EnumHasAnyFlags(Contexts[InContextNum].ContextState, EDisplayClusterViewportContextState::HasCalculatedViewPoint))
			{
				// Use calculated values
				// Since this function can be called several times from LocalPlayer.cpp, the cached values are used on repeated calls.
				// This should give a performance boost for 'mesh', 'mpcdi' projections with a large number of vertices in the geometry or large warp texture size.
				InOutViewLocation = Contexts[InContextNum].ViewLocation;
				InOutViewRotation = Contexts[InContextNum].ViewRotation;

				return true;
			}
			else if (ProjectionPolicy.IsValid() && ProjectionPolicy->CalculateView(this, InContextNum, InOutViewLocation, InOutViewRotation, ViewOffset, WorldToMeters, NCP, FCP))
			{
				Contexts[InContextNum].WorldToMeters = WorldToMeters;

				// Save the calculated values and update the state of the context
				Contexts[InContextNum].ViewLocation = InOutViewLocation;
				Contexts[InContextNum].ViewRotation = InOutViewRotation;
				EnumAddFlags(Contexts[InContextNum].ContextState, EDisplayClusterViewportContextState::HasCalculatedViewPoint);

				return true;
			}
			else
			{
				// ProjectionPolicy->CalculateView() returns false, this view is invalid
				EnumAddFlags(Contexts[InContextNum].ContextState, EDisplayClusterViewportContextState::InvalidViewPoint);
			}
		}
	}

	return false;
}

bool FDisplayClusterViewport::GetProjectionMatrix(const uint32 InContextNum, FMatrix& OutPrjMatrix)
{
	if (Contexts.IsValidIndex(InContextNum))
	{
		if (!EnumHasAnyFlags(Contexts[InContextNum].ContextState, EDisplayClusterViewportContextState::InvalidProjectionMatrix))
		{
			// The function can also be called several times per frame.
			// stores the result in the context variables 'ProjectionMatrix' and 'OverscanProjectionMatrix'.
			// Finally, raises this flag for subsequent calls in the current frame.

			if (EnumHasAnyFlags(Contexts[InContextNum].ContextState, EDisplayClusterViewportContextState::HasCalculatedProjectionMatrix))
			{
				// use already calculated values
				if (EnumHasAnyFlags(Contexts[InContextNum].ContextState, EDisplayClusterViewportContextState::HasCalculatedOverscanProjectionMatrix))
				{
					// use overscan proj matrix for rendering
					OutPrjMatrix = Contexts[InContextNum].OverscanProjectionMatrix;
				}
				else
				{
					OutPrjMatrix = Contexts[InContextNum].ProjectionMatrix;
				}

				return true;
			}
			else if (ProjectionPolicy.IsValid() && ProjectionPolicy->GetProjectionMatrix(this, InContextNum, OutPrjMatrix))
			{
				// Save the calculated values and update the state of the context
				Contexts[InContextNum].ProjectionMatrix = OutPrjMatrix;
				EnumAddFlags(Contexts[InContextNum].ContextState, EDisplayClusterViewportContextState::HasCalculatedProjectionMatrix);

				if (OverscanRendering.IsEnabled())
				{
					// use overscan proj matrix for rendering
					OutPrjMatrix = Contexts[InContextNum].OverscanProjectionMatrix;
					EnumAddFlags(Contexts[InContextNum].ContextState, EDisplayClusterViewportContextState::HasCalculatedOverscanProjectionMatrix);
				}

				return true;
			}
			else
			{
				// ProjectionPolicy->GetProjectionMatrix() returns false, this projection matrix is invalid
				EnumAddFlags(Contexts[InContextNum].ContextState, EDisplayClusterViewportContextState::InvalidProjectionMatrix);
			}
		}
	}

	return false;
}

inline void GetNonZeroFrustumRange(float& InOutValue0, float& InOutValue1, float n)
{
	static const float MinHalfFOVRangeRad = FMath::DegreesToRadians(0.5f);
	static const float MinRangeBase = FMath::Tan(MinHalfFOVRangeRad * 2);;

	const float MinRangeValue = n * MinRangeBase;
	if ((InOutValue1 - InOutValue0) < MinRangeValue)
	{
		// Get minimal values from center of range
		const float CenterRad = (FMath::Atan(InOutValue0 / n) + (FMath::Atan(InOutValue1 / n))) * 0.5f;
		InOutValue0 = float(n * FMath::Tan(CenterRad - MinHalfFOVRangeRad));
		InOutValue1 = float(n * FMath::Tan(CenterRad + MinHalfFOVRangeRad));
	}
}

void FDisplayClusterViewport::CalculateProjectionMatrix(const uint32 InContextNum, float Left, float Right, float Top, float Bottom, float ZNear, float ZFar, bool bIsAnglesInput)
{
	// limit max frustum to 89
	static const float MaxFrustumAngle = FMath::Tan(FMath::DegreesToRadians(89));
	const float MaxValue = ZNear * MaxFrustumAngle;

	const float n = ZNear;
	const float f = ZFar;

	float t = bIsAnglesInput ? (ZNear * FMath::Tan(FMath::DegreesToRadians(Top)))    : Top;
	float b = bIsAnglesInput ? (ZNear * FMath::Tan(FMath::DegreesToRadians(Bottom))) : Bottom;
	float l = bIsAnglesInput ? (ZNear * FMath::Tan(FMath::DegreesToRadians(Left)))   : Left;
	float r = bIsAnglesInput ? (ZNear * FMath::Tan(FMath::DegreesToRadians(Right)))  : Right;

	// Protect PrjMatrix from bad input values, and fix\clamp FOV to limits
	{
		// Protect from broken input data, return valid matrix
		if (isnan(l) || isnan(r) || isnan(t) || isnan(b) || isnan(n) || isnan(f) || n <= 0)
		{
			return;
		}

		// Ignore inverted frustum
		if (l > r || b > t)
		{
			return;
		}

		// Clamp frustum values in range -89..89 degree
		l = FMath::Clamp(l, -MaxValue, MaxValue);
		r = FMath::Clamp(r, -MaxValue, MaxValue);
		t = FMath::Clamp(t, -MaxValue, MaxValue);
		b = FMath::Clamp(b, -MaxValue, MaxValue);
	}

	// Support custom frustum rendering
	const float OrigValues[] = {l, r, t, b};
	if (CustomFrustumRendering.UpdateProjectionAngles(l, r, t, b))
	{
		const bool bIsValidLimits =  FMath::IsWithin(l, -MaxValue, MaxValue)
							&& FMath::IsWithin(r, -MaxValue, MaxValue)
							&& FMath::IsWithin(t, -MaxValue, MaxValue)
							&& FMath::IsWithin(b, -MaxValue, MaxValue);

		if (!bIsValidLimits)
		{
			// overscan out of frustum : disable
			CustomFrustumRendering.Disable();

			// restore orig values
			l = OrigValues[0];
			r = OrigValues[1];
			t = OrigValues[2];
			b = OrigValues[3];
		}
	}

	GetNonZeroFrustumRange(l, r, n);
	GetNonZeroFrustumRange(b, t, n);

	Contexts[InContextNum].ProjectionMatrix = IDisplayClusterViewport::MakeProjectionMatrix(l, r, t, b, n, f);

	if (OverscanRendering.UpdateProjectionAngles(l, r, t, b))
	{
		if (FMath::IsWithin(l, -MaxValue, MaxValue) &&
			FMath::IsWithin(r, -MaxValue, MaxValue) &&
			FMath::IsWithin(t, -MaxValue, MaxValue) &&
			FMath::IsWithin(b, -MaxValue, MaxValue)
			)
		{
			// Use overscan projection matrix
			Contexts[InContextNum].OverscanProjectionMatrix = IDisplayClusterViewport::MakeProjectionMatrix(l, r, t, b, n, f);
			return;
		}
	}

	// overscan out of frustum: disable
	OverscanRendering.Disable();
}

///////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterViewport
///////////////////////////////////////////////////////////////////////////////////////
FMatrix IDisplayClusterViewport::MakeProjectionMatrix(float l, float r, float t, float b, float n, float f)
{
	const float mx = 2.f * n / (r - l);
	const float my = 2.f * n / (t - b);
	const float ma = -(r + l) / (r - l);
	const float mb = -(t + b) / (t - b);

	// Support unlimited far plane (f==n)
	const float mc = (f == n) ? (1.0f - Z_PRECISION) : (f / (f - n));
	const float md = (f == n) ? (-n * (1.0f - Z_PRECISION)) : (-(f * n) / (f - n));

	const float me = 1.f;

	// Normal LHS
	const FMatrix ProjectionMatrix = FMatrix(
		FPlane(mx, 0, 0, 0),
		FPlane(0, my, 0, 0),
		FPlane(ma, mb, mc, me),
		FPlane(0, 0, md, 0));

	// Invert Z-axis (UE uses Z-inverted LHS)
	static const FMatrix flipZ = FMatrix(
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, -1, 0),
		FPlane(0, 0, 1, 1));

	const FMatrix ResultMatrix(ProjectionMatrix * flipZ);

	return ResultMatrix;
}
