// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGContext.h"
#include "PCGSettings.h"



#include "PCGExecuteBlueprint.generated.h"

class UPCGBlueprintElement;
class UPCGMetadata;
class UPCGPointData;
class UPCGSpatialData;
struct FPCGPoint;

class UWorld;

#if WITH_EDITOR
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPCGBlueprintChanged, UPCGBlueprintElement*);

namespace PCGBlueprintHelper
{
	TSet<TObjectPtr<UObject>> GetDataDependencies(UPCGBlueprintElement* InElement);
}
#endif // WITH_EDITOR

UCLASS(Abstract, BlueprintType, Blueprintable, hidecategories = (Object))
class PCG_API UPCGBlueprintElement : public UObject
{
	GENERATED_BODY()

public:
	// ~Begin UObject interface
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
	// ~End UObject interface

	UFUNCTION(BlueprintNativeEvent, Category = "PCG|Execution")
	void ExecuteWithContext(UPARAM(ref)FPCGContext& InContext, const FPCGDataCollection& Input, FPCGDataCollection& Output);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "PCG|Execution")
	void Execute(const FPCGDataCollection& Input, FPCGDataCollection& Output);

	UFUNCTION(BlueprintImplementableEvent, Category = "PCG|Flow Control")
	bool PointLoopBody(const FPCGContext& InContext, const UPCGPointData* InData, const FPCGPoint& InPoint, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const;

	UFUNCTION(BlueprintImplementableEvent, Category = "PCG|Flow Control")
	TArray<FPCGPoint> VariableLoopBody(const FPCGContext& InContext, const UPCGPointData* InData, const FPCGPoint& InPoint, UPCGMetadata* OutMetadata) const;

	UFUNCTION(BlueprintImplementableEvent, Category = "PCG|Flow Control")
	bool NestedLoopBody(const FPCGContext& InContext, const UPCGPointData* InOuterData, const UPCGPointData* InInnerData, const FPCGPoint& InOuterPoint, const FPCGPoint& InInnerPoint, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const;

	UFUNCTION(BlueprintImplementableEvent, Category = "PCG|Flow Control")
	bool IterationLoopBody(const FPCGContext& InContext, int64 Iteration, const UPCGSpatialData* InA, const UPCGSpatialData* InB, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const;

	/** Calls the PointLoopBody function on all points */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "PCG|Flow Control", meta = (HideSelfPin = "true"))
	void PointLoop(UPARAM(ref) FPCGContext& InContext, const UPCGPointData* InData, UPCGPointData*& OutData, UPCGPointData* OptionalOutData = nullptr) const;

	/** Calls the VariableLoopBody function on all points, each call can return a variable number of points */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "PCG|Flow Control", meta = (HideSelfPin = "true"))
	void VariableLoop(UPARAM(ref) FPCGContext& InContext, const UPCGPointData* InData, UPCGPointData*& OutData, UPCGPointData* OptionalOutData = nullptr) const;

	/** Calls the NestedLoopBody function on all nested loop pairs (e.g. (o, i) for all o in Outer, i in Inner) */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "PCG|FLow Control", meta = (HideSelfPin = "true"))
	void NestedLoop(UPARAM(ref) FPCGContext& InContext, const UPCGPointData* InOuterData, const UPCGPointData* InInnerData, UPCGPointData*& OutData, UPCGPointData* OptionalOutData = nullptr) const;
	
	/** Calls the IterationLoopBody a fixed number of times, optional parameters are used to potentially initialized the Out Data, but otherwise are used to remove the need to have variables */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "PCG|Flow Control", meta = (HideSelfPin = "true"))
	void IterationLoop(UPARAM(ref) FPCGContext& InContext, int64 NumIterations, UPCGPointData*& OutData, const UPCGSpatialData* OptionalA = nullptr, const UPCGSpatialData* OptionalB = nullptr, UPCGPointData* OptionalOutData = nullptr) const;

	/** Override for the default node name */
	UFUNCTION(BlueprintNativeEvent, Category = "PCG|Node Customization")
	FName NodeTitleOverride() const;

	UFUNCTION(BlueprintNativeEvent, Category = "PCG|Node Customization")
	FLinearColor NodeColorOverride() const;

	UFUNCTION(BlueprintNativeEvent, Category = "PCG|Node Customization")
	EPCGSettingsType NodeTypeOverride() const;

	/** Override for the IsCacheable node property when it depends on the settings in your node */
	UFUNCTION(BlueprintNativeEvent, Category = "PCG|Execution")
	bool IsCacheableOverride() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "PCG|Preconfigure Settings", meta = (ForceAsFunction))
	void ApplyPreconfiguredSettings(UPARAM(ref) const FPCGPreConfiguredSettingsInfo& InPreconfigureInfo);

	// Returns the labels of custom input pins only
	UFUNCTION(BlueprintCallable, Category = "PCG|Input & Output")
	TSet<FName> CustomInputLabels() const;

	// Returns the labels of custom output pins only
	UFUNCTION(BlueprintCallable, Category = "PCG|Input & Output")
	TSet<FName> CustomOutputLabels() const;

	UFUNCTION(BlueprintCallable, Category = "PCG|Input & Output", meta = (HideSelfPin = "true"))
	TArray<FPCGPinProperties> GetInputPins() const;

	UFUNCTION(BlueprintCallable, Category = "PCG|Input & Output", meta = (HideSelfPin = "true"))
	TArray<FPCGPinProperties> GetOutputPins() const;

	/** Returns true if there is an input pin with the matching label. If found, will copy the pin properties in OutFoundPin */
	UFUNCTION(BlueprintCallable, Category = "PCG|Input & Output", meta = (HideSelfPin = "true"))
	bool GetInputPinByLabel(FName InPinLabel, FPCGPinProperties& OutFoundPin) const;

	/** Returns true if there is an output pin with the matching label. If found, will copy the pin properties in OutFoundPin */
	UFUNCTION(BlueprintCallable, Category = "PCG|Input & Output", meta = (HideSelfPin = "true"))
	bool GetOutputPinByLabel(FName InPinLabel, FPCGPinProperties& OutFoundPin) const;

	/** Gets the seed from the associated settings & source component */
	UFUNCTION(BlueprintCallable, Category = "PCG|Random")
	int GetSeed(UPARAM(ref) FPCGContext& InContext) const;

	/** Creates a random stream from the settings & source component */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "PCG|Random")
	FRandomStream GetRandomStream(UPARAM(ref) FPCGContext& InContext) const;

	/** Called after object creation to setup the object callbacks */
	void Initialize();

	/** Retrieves the execution context - note that this will not be valid outside of the Execute functions */
	UFUNCTION(BlueprintCallable, Category = "PCG|Advanced", meta = (HideSelfPin = "true"))
	FPCGContext& GetContext() const;

	/** Called after the element duplication during execution to be able to get the context easily - internal call only */
	void SetCurrentContext(FPCGContext* InCurrentContext);

#if WITH_EDITOR
	// ~Begin UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	// ~End UObject interface

	/** Used for filtering */
	static FString GetParentClassName();
#endif

	/** Needed to be able to call certain blueprint functions */
	virtual UWorld* GetWorld() const override;

#if !WITH_EDITOR
	void SetInstanceWorld(UWorld* World) { InstanceWorld = World; }
#endif

#if WITH_EDITOR
	FOnPCGBlueprintChanged OnBlueprintChangedDelegate;
#endif

	/** Controls whether results can be cached so we can bypass execution if the inputs & settings are the same in a subsequent execution.
	* If you have implemented the IsCacheableOverride function, then this value is ignored.
	* Note that if your node relies on data that is not directly tracked by PCG or creates any kind of artifact (adds components, creates actors, etc.) then it should not be cacheable.
	*/
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, AdvancedDisplay, Category = Settings)
	bool bIsCacheable = false;

	/** In cases where your node is non-cacheable but is likely to yield the same results on subsequent executions, this controls whether we will do a deep & computationally intensive CRC computation (true), 
	* which will allow cache usage in downstream nodes in your graph, or, by default (false), a shallow but quick crc computation which will not be cache-friendly. */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, AdvancedDisplay, Category = Settings)
	bool bComputeFullDataCrc = false;

	/** Controls whether this node execution can be run from a non-game thread. This is not related to the Loop functions provided/implemented in this class, which should always run on any thread. */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = Settings)
	bool bCanBeMultithreaded = false;

#if WITH_EDITORONLY_DATA
	UPROPERTY(BlueprintGetter=CustomInputLabels, Category = "Settings|Input & Output", meta = (DeprecatedProperty, DeprecatedMessage = "Input Pin Labels are deprecated - use Input Labels instead."))
	TSet<FName> InputPinLabels_DEPRECATED;

	UPROPERTY(BlueprintGetter=CustomOutputLabels, Category = "Settings|Input & Output")
	TSet<FName> OutputPinLabels_DEPRECATED;
#endif

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Settings|Input & Output")
	TArray<FPCGPinProperties> CustomInputPins;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Settings|Input & Output")
	TArray<FPCGPinProperties> CustomOutputPins;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Settings|Input & Output")
	bool bHasDefaultInPin = true;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Settings|Input & Output")
	bool bHasDefaultOutPin = true;

#if WITH_EDITORONLY_DATA
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = AssetInfo, AssetRegistrySearchable)
	bool bExposeToLibrary = false;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "AssetInfo|Preconfigured Settings", AssetRegistrySearchable)
	bool bEnablePreconfiguredSettings = false;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "AssetInfo|Preconfigured Settings", AssetRegistrySearchable, meta = (EditCondition = bEnablePreconfiguredSettings, EditConditionHides))
	bool bOnlyExposePreconfiguredSettings = false;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "AssetInfo|Preconfigured Settings", meta = (EditCondition = bEnablePreconfiguredSettings, EditConditionHides))
	TArray<FPCGPreConfiguredSettingsInfo> PreconfiguredInfo;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = AssetInfo, AssetRegistrySearchable)
	FText Category;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = AssetInfo, AssetRegistrySearchable)
	FText Description;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, AdvancedDisplay, Category = "Settings")
	int32 DependencyParsingDepth = 1;
#endif

protected:
#if WITH_EDITOR
	void OnDependencyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent);
	TSet<TObjectPtr<UObject>> DataDependencies;
#endif

#if !WITH_EDITORONLY_DATA
	UWorld* InstanceWorld = nullptr;
#endif

	// Since we duplicate the blueprint elements prior to execution, they will be unique
	// and have a 1:1 match with their context, which allows us to store it here
	FPCGContext* CurrentContext = nullptr;
};

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGBlueprintSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGBlueprintSettings();

	friend class FPCGExecuteBlueprintElement;

	// ~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("ExecuteBlueprint")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGBlueprintSettings", "NodeTitle", "Execute Blueprint"); }
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual EPCGSettingsType GetType() const override;
	virtual void GetTrackedActorKeys(FPCGActorSelectionKeyToSettingsMap& OutKeysToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const override;
	virtual UObject* GetJumpTargetForDoubleClick() const override;
	virtual void ApplyDeprecationBeforeUpdatePins(UPCGNode* InOutNode, TArray<TObjectPtr<UPCGPin>>& InputPins, TArray<TObjectPtr<UPCGPin>>& OutputPins) override;
	virtual TArray<FPCGPreConfiguredSettingsInfo> GetPreconfiguredInfo() const override;
	virtual bool OnlyExposePreconfiguredSettings() const override;
#endif

	virtual void ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& InPreconfiguredsInfo) override;
	virtual FName AdditionalTaskName() const override;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

	/** To be removed when we support automatic override of BP params. For now always return true to force params pin. */
	virtual bool HasOverridableParams() const override { return true; }
protected:
	virtual FPCGElementPtr CreateElement() const override;
#if WITH_EDITOR
	virtual TArray<FPCGSettingsOverridableParam> GatherOverridableParams() const override;
#endif // WITH_EDITOR
	virtual void FixingOverridableParamPropertyClass(FPCGSettingsOverridableParam& Param) const override;
	// ~End UPCGSettings interface

public:
	// ~Begin UObject interface
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	// ~End UObject interface
#endif

	UFUNCTION(BlueprintCallable, Category = "Settings|Template", meta=(DeterminesOutputType="InElementType", DynamicOutputParam = "ElementInstance"))
	void SetElementType(TSubclassOf<UPCGBlueprintElement> InElementType, UPCGBlueprintElement*& ElementInstance);

	UFUNCTION(BlueprintCallable, Category = "Settings|Template")
	TSubclassOf<UPCGBlueprintElement> GetElementType() const { return BlueprintElementType; }

#if WITH_EDITOR
	TObjectPtr<UPCGBlueprintElement> GetElementInstance() const { return BlueprintElementInstance; }
#endif

protected:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TSubclassOf<UPCGBlueprintElement> BlueprintElement_DEPRECATED;
#endif

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Template)
	TSubclassOf<UPCGBlueprintElement> BlueprintElementType;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Instanced, Category = "Instance", meta = (ShowOnlyInnerProperties))
	TObjectPtr<UPCGBlueprintElement> BlueprintElementInstance;

#if WITH_EDITORONLY_DATA
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FName> TrackedActorTags;

	/** If this is checked, found actors that are outside component bounds will not trigger a refresh. Only works for tags for now in editor. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings)
	bool bTrackActorsOnlyWithinBounds = false;

	UPROPERTY()
	bool bCanBeMultithreaded_DEPRECATED = false;
#endif

protected:
#if WITH_EDITOR
	void OnBlueprintChanged(UBlueprint* InBlueprint);
	void OnBlueprintElementChanged(UPCGBlueprintElement* InElement);
#endif

	void RefreshBlueprintElement();
	void SetupBlueprintEvent();
	void TeardownBlueprintEvent();
	void SetupBlueprintElementEvent();
	void TeardownBlueprintElementEvent();
};

struct FPCGBlueprintExecutionContext : public FPCGContext
{
	virtual ~FPCGBlueprintExecutionContext();

	UPCGBlueprintElement* BlueprintElementInstance = nullptr;

protected:
	virtual UObject* GetExternalContainerForOverridableParam(const FPCGSettingsOverridableParam& InParam) override { return BlueprintElementInstance; }
};

class FPCGExecuteBlueprintElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override;
	virtual bool ShouldComputeFullOutputDataCrc(FPCGContext* Context) const override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;	
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Data/PCGPointData.h"
#include "Math/RandomStream.h"
#include "PCGPoint.h"
#endif
