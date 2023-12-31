// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"

#include "MetalFence.h"
#include "MetalCommandBuffer.h"
#include "MetalCommandQueue.h"
#include "MetalContext.h"
#include "MetalProfiler.h"

@implementation FMetalDebugFence
@synthesize Inner;

APPLE_PLATFORM_OBJECT_ALLOC_OVERRIDES(FMetalDebugFence)

-(instancetype)init
{
	id Self = [super init];
	if (Self)
	{
		Label = nil;
	}
	return Self;
}

-(void)dealloc
{
	[self validate];
	FMetalDebugCommandEncoder* Encoder = nil;
	while ((Encoder = UpdatingEncoders.Pop()))
	{
		[Encoder release];
	}
	Encoder = nil;
	while ((Encoder = WaitingEncoders.Pop()))
	{
		[Encoder release];
	}
	[Label release];
	[super dealloc];
}

-(id <MTLDevice>) device
{
	if (Inner)
	{
		return Inner.device;
	}
	else
	{
		return nil;
	}
}

-(NSString *_Nullable)label
{
	return Label;
}

-(void)setLabel:(NSString *_Nullable)Text
{
	[Text retain];
	[Label release];
	Label = Text;
	if(Inner)
	{
		Inner.label = Text;
	}
}

-(void)validate
{
	UE_CLOG(UpdatingEncoders.IsEmpty() != WaitingEncoders.IsEmpty(), LogMetal, Fatal, TEXT("Fence with unmatched updates/waits destructed - there's a gap in fence (%p) %s"), self, Label ? *FString(Label) : TEXT("Null"));
}

-(void)updatingEncoder:(FMetalDebugCommandEncoder*)Encoder
{
	check(Encoder);
	UpdatingEncoders.Push([Encoder retain]);
}

-(void)waitingEncoder:(FMetalDebugCommandEncoder*)Encoder
{
	check(Encoder);
	WaitingEncoders.Push([Encoder retain]);
}

-(TLockFreePointerListLIFO<FMetalDebugCommandEncoder>*)updatingEncoders
{
	return &UpdatingEncoders;
}

-(TLockFreePointerListLIFO<FMetalDebugCommandEncoder>*)waitingEncoders
{
	return &WaitingEncoders;
}
@end

#if METAL_DEBUG_OPTIONS
extern int32 GMetalRuntimeDebugLevel;
#endif

uint32 FMetalFence::Release() const
{
	uint32 Refs = uint32(FPlatformAtomics::InterlockedDecrement(&NumRefs));
	if(Refs == 0)
	{
#if METAL_DEBUG_OPTIONS // When using validation we need to use fences only once per-frame in order to make it tractable
		if (GMetalRuntimeDebugLevel >= EMetalDebugLevelValidation)
		{
			SafeReleaseMetalFence(const_cast<FMetalFence*>(this));
		}
		else // However in a final game, we need to reuse fences aggressively so that we don't run out when loading into projects
#endif
		{
			FMetalFencePool::Get().ReleaseFence(const_cast<FMetalFence*>(this));
		}
	}
	return Refs;
}

void FMetalFencePool::Initialise(mtlpp::Device const& InDevice)
{
	Device = InDevice;
	for (int32 i = 0; i < FMetalFencePool::NumFences; i++)
	{
#if METAL_DEBUG_OPTIONS
		if (GMetalRuntimeDebugLevel >= EMetalDebugLevelValidation)
		{
			FMetalDebugFence* DebugFence = [[FMetalDebugFence new] autorelease];
			DebugFence.Inner = Device.NewFence();
			FMetalFence* F = new FMetalFence;
			F->Set(DebugFence);
			Fences.Add(F);
			Lifo.Push(F);
		}
		else
#endif
		{
			FMetalFence* F = new FMetalFence;
			F->Set(Device.NewFence());
#if METAL_DEBUG_OPTIONS
			if (GMetalRuntimeDebugLevel >= EMetalDebugLevelValidation)
			{
				Fences.Add(F);
			}
#endif
			Lifo.Push(F);
		}
	}
	Count = FMetalFencePool::NumFences;
    Allocated = 0;
}

FMetalFence* FMetalFencePool::AllocateFence()
{
	FMetalFence* Fence = Lifo.Pop();
	if (Fence)
	{
        INC_DWORD_STAT(STAT_MetalFenceCount);
        FPlatformAtomics::InterlockedDecrement(&Count);
        FPlatformAtomics::InterlockedIncrement(&Allocated);
#if METAL_DEBUG_OPTIONS
		if (GMetalRuntimeDebugLevel >= EMetalDebugLevelValidation)
		{
			FScopeLock Lock(&Mutex);
			check(Fences.Contains(Fence));
			Fences.Remove(Fence);
		}
#endif
	}
	check(Fence);
	Fence->Reset();
	return Fence;
}

void FMetalFencePool::ReleaseFence(FMetalFence* const InFence)
{
	if (InFence)
	{
        DEC_DWORD_STAT(STAT_MetalFenceCount);
        FPlatformAtomics::InterlockedDecrement(&Allocated);
#if METAL_DEBUG_OPTIONS
		if (GMetalRuntimeDebugLevel >= EMetalDebugLevelValidation)
		{
			FScopeLock Lock(&Mutex);
			check(!Fences.Contains(InFence));
			Fences.Add(InFence);
		}
#endif
		FPlatformAtomics::InterlockedIncrement(&Count);
		check(Count <= FMetalFencePool::NumFences);
		Lifo.Push(InFence);
	}
}
