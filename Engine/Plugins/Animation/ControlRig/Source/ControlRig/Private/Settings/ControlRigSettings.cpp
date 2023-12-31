// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/ControlRigSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigSettings)

UControlRigSettings::UControlRigSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	DefaultShapeLibrary = LoadObject<UControlRigShapeLibrary>(nullptr, TEXT("/ControlRig/Controls/DefaultGizmoLibraryNormalized.DefaultGizmoLibraryNormalized"));
#endif
}

UControlRigEditorSettings::UControlRigEditorSettings(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
#if WITH_EDITORONLY_DATA
    , bResetControlTransformsOnCompile(true)
#endif
{
#if WITH_EDITORONLY_DATA
	bResetControlsOnCompile = true;
	bResetControlsOnPinValueInteraction = false;
	bEnableUndoForPoseInteraction = true;

	ConstructionEventBorderColor = FLinearColor::Red;
	BackwardsSolveBorderColor = FLinearColor::Yellow;
	BackwardsAndForwardsBorderColor = FLinearColor::Blue;
	bShowStackedHierarchy = false;
	MaxStackSize = 16;
#endif
}

