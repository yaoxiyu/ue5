// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AutomationTestExcludelist.generated.h"

UENUM()
enum class ETEST_RHI_Options
{
	DirectX11,
	DirectX12,
	Vulkan,
	Metal,
	Null
};

inline FString LexToString(ETEST_RHI_Options Option)
{
	switch (Option)
	{
	case ETEST_RHI_Options::DirectX11:    return TEXT("DirectX 11");
	case ETEST_RHI_Options::DirectX12:    return TEXT("DirectX 12");
	case ETEST_RHI_Options::Vulkan:       return TEXT("Vulkan");
	case ETEST_RHI_Options::Metal:        return TEXT("Metal");
	case ETEST_RHI_Options::Null:         return TEXT("Null");
	default:                              return TEXT("Unknown");
	}
}

UENUM()
enum class ETEST_RHI_FeatureLevel_Options
{
	SM5,
	SM6
};

inline FString LexToString(ETEST_RHI_FeatureLevel_Options Option)
{
	switch (Option)
	{
	case ETEST_RHI_FeatureLevel_Options::SM5:   return TEXT("SM5");
	case ETEST_RHI_FeatureLevel_Options::SM6:   return TEXT("SM6");
	default:                                    return TEXT("Unknown");
	}
}

USTRUCT()
struct FAutomationTestExcludeOptions
{
	GENERATED_BODY()

	template<typename EnumType>
	static TSet<FName> GetAllRHIOptionNames()
	{
		static TSet<FName> NameSet;
		if (NameSet.IsEmpty())
		{
			if constexpr (std::is_same_v<EnumType, ETEST_RHI_Options> || std::is_same_v<EnumType, ETEST_RHI_FeatureLevel_Options>)
			{
				UEnum* Enum = StaticEnum<EnumType>();
				int32 Num_Flags = Enum->NumEnums() - 1;
				for (int32 i = 0; i < Num_Flags; i++)
				{
					NameSet.Add(*LexToString((EnumType)Enum->GetValueByIndex(i)));
				}
			}
		}

		return NameSet;
	}

	/* Name of the target test */
	UPROPERTY(VisibleAnywhere, Category = ExcludeTestOptions)
	FName Test;

	/* Reason to why the test is excluded */
	UPROPERTY(EditAnywhere, Category = ExcludeTestOptions)
	FName Reason;

	/* Options to target specific RHI. No option means it should be applied to all RHIs */
	UPROPERTY(EditAnywhere, Category = ExcludeTestOptions)
	TSet<FName> RHIs;

	/* Should the Reason be reported as a warning in the log */
	UPROPERTY(EditAnywhere, Category = ExcludeTestOptions)
	bool Warn = false;
};

USTRUCT()
struct FAutomationTestExcludelistEntry
{
	GENERATED_BODY()

	FAutomationTestExcludelistEntry() { }

	FAutomationTestExcludelistEntry(const FAutomationTestExcludeOptions& Options)
		: Test(Options.Test)
		, Reason(Options.Reason)
		, RHIs(Options.RHIs)
		, Warn(Options.Warn)
	{ }

	TSharedPtr<FAutomationTestExcludeOptions> GetOptions() const
	{
		TSharedPtr<FAutomationTestExcludeOptions> Options = MakeShareable(new FAutomationTestExcludeOptions());
		Options->Test = Test;
		Options->Reason = Reason;
		Options->Warn = Warn;
		Options->RHIs = RHIs;
		
		return Options;
	}

	/* Return the number of RHI types that needs to be matched for exclusion */
	int8 NumRHIType() const
	{
		int8 Num = 0;
		// Test mentions of each RHI option types
		static const TSet<FName> AllRHI_OptionNames = FAutomationTestExcludeOptions::GetAllRHIOptionNames<ETEST_RHI_Options>();
		if (RHIs.Difference(AllRHI_OptionNames).Num() < RHIs.Num())
		{
			Num++;
		}
		static const TSet<FName> AllRHI_FeatureLevel_OptionNames = FAutomationTestExcludeOptions::GetAllRHIOptionNames<ETEST_RHI_FeatureLevel_Options>();
		if (RHIs.Difference(AllRHI_FeatureLevel_OptionNames).Num() < RHIs.Num())
		{
			Num++;
		}

		return Num;
	}

	/* Determine if exclusion entry is propagated based on test name - used for management in test automation window */
	void SetPropagation(const FString& ForTestName)
	{
		bIsPropagated = FullTestName != ForTestName.TrimStartAndEnd().ToLower();
	}

	/* Return true if the entry is not specific */
	bool IsEmpty() const
	{
		return FullTestName.IsEmpty();
	}

	/* Reset entry to be un-specific */
	void Reset()
	{
		FullTestName.Empty();
		bIsPropagated = false;
	}

	/* Has conditional exclusion */
	bool HasConditions() const
	{
		return !RHIs.IsEmpty();
	}

	/* Remove exclusion conditions, return true if a condition was removed */
	bool RemoveConditions(const FAutomationTestExcludelistEntry& Entry)
	{
		if (!Entry.HasConditions())
		{
			return false;
		}

		bool GotRemoved = false;
		// Check RHIs
		int Length = RHIs.Num();
		if (Length > 0)
		{
			RHIs = RHIs.Difference(Entry.RHIs);
			GotRemoved = GotRemoved || Length != RHIs.Num();
		}

		return GotRemoved;
	}

	/* Hold full test name/path */
	FString FullTestName;
	/* Is the entry comes from propagation */
	bool bIsPropagated = false;

	// Use FName instead of FString to read params from config that aren't wrapped with quotes

	/* Hold the name of the target functional test map */
	UPROPERTY(EditDefaultsOnly, Category = AutomationTestExcludelist)
	FName Map;

	/* Hold the name of the target test - full test name is require here unless for functional tests */
	UPROPERTY(EditDefaultsOnly, Category = AutomationTestExcludelist)
	FName Test;

	/* Reason to why the test is excluded */
	UPROPERTY(EditDefaultsOnly, Category = AutomationTestExcludelist)
	FName Reason;

	/* Option to target specific RHI. Empty array means it should be applied to all RHI */
	UPROPERTY(EditDefaultsOnly, Category = AutomationTestExcludelist)
	TSet<FName> RHIs;

	/* Should the Reason be reported as a warning in the log */
	UPROPERTY(EditDefaultsOnly, Category = AutomationTestExcludelist)
	bool Warn = false;
};


UCLASS(config = Engine, defaultconfig, MinimalAPI)
class UAutomationTestExcludelist : public UObject
{
	GENERATED_BODY()

public:
	UAutomationTestExcludelist() {}

	static AUTOMATIONTEST_API UAutomationTestExcludelist* Get();

	AUTOMATIONTEST_API void AddToExcludeTest(const FString& TestName, const FAutomationTestExcludelistEntry& ExcludelistEntry);
	AUTOMATIONTEST_API void RemoveFromExcludeTest(const FString& TestName);
	AUTOMATIONTEST_API bool IsTestExcluded(const FString& TestName, const TSet<FName>& = TSet<FName>(), FName* OutReason = nullptr, bool* OutWarn = nullptr);
	AUTOMATIONTEST_API FAutomationTestExcludelistEntry* GetExcludeTestEntry(const FString& TestName, const TSet<FName>& = TSet<FName>());

	AUTOMATIONTEST_API void SaveConfig();
	FString GetConfigFilename() { return UObject::GetDefaultConfigFilename(); } const
	// It is called automatically when CDO is created, usually you don't need to call LoadConfig manually
	void LoadConfig() { UObject::LoadConfig(GetClass()); }

	AUTOMATIONTEST_API virtual void OverrideConfigSection(FString& SectionName) override;

protected:
	AUTOMATIONTEST_API virtual void PostInitProperties() override;

private:
	AUTOMATIONTEST_API FString GetFullTestName(const FAutomationTestExcludelistEntry& ExcludelistEntry);

	UPROPERTY(EditDefaultsOnly, globalconfig, Category = AutomationTestExcludelist)
	TArray<FAutomationTestExcludelistEntry> ExcludeTest;
};

