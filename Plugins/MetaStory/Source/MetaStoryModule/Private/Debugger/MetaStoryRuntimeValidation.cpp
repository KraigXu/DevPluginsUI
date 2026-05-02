// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debugger/MetaStoryRuntimeValidation.h"
#include "Debugger/MetaStoryRuntimeValidationInstanceData.h"

#if WITH_METASTORY_DEBUG
#include "HAL/IConsoleManager.h"
#include "MetaStory.h"
#endif

namespace UE::MetaStory::Debug
{
#if WITH_METASTORY_DEBUG

namespace Private
{
bool bRuntimeValidationContext = true;
static FAutoConsoleVariableRef CVarRuntimeValidationContext(
	TEXT("MetaStory.RuntimeValidation.Context"),
	bRuntimeValidationContext,
	TEXT("Test if the context creation parameters are the same between each creation of MetaStoryExecutionContext.")
);

bool bRuntimeValidationDoesNewerVersionExists = true;
static FAutoConsoleVariableRef CVarRuntimeValidationDoesNewerVersionExists(
	TEXT("MetaStory.RuntimeValidation.DoesNewerVersionExists"),
	bRuntimeValidationDoesNewerVersionExists,
	TEXT("Test if a MetaStoryExecutionContext started with an old version of a blueprint type.")
);

bool bRuntimeValidationEnterExitState = false;
static FAutoConsoleVariableRef CVarRuntimeValidationEnterExitState(
	TEXT("MetaStory.RuntimeValidation.EnterExitState"),
	bRuntimeValidationEnterExitState,
	TEXT("Test that if a node get a EnterState, it will receive an ExitState.\n"
		"Test that if a node get a ExitState, it did receive an EnterState before.")
);

bool bRuntimeValidationInstanceData = false;
static FAutoConsoleVariableRef CVarRuntimeValidationInstanceData(
	TEXT("MetaStory.RuntimeValidation.InstanceData"),
	bRuntimeValidationInstanceData,
	TEXT("Test if the MetaStory instance data and shared instance data are valid.")
);

FString NodeToString(const UObject* Obj, FGuid Id)
{
	TStringBuilderWithBuffer<TCHAR, 128> Buffer;
	if (Obj)
	{
		Obj->GetPathName(nullptr, Buffer);
	}
	Buffer << TEXT(':');
	Buffer << Id;
	return Buffer.ToString();
}
} // namespace Private

FRuntimeValidationInstanceData::~FRuntimeValidationInstanceData()
{
	if (Private::bRuntimeValidationEnterExitState && UObjectInitialized() && !IsEngineExitRequested())
	{
		for (const FNodeStatePair& Pair : NodeStates)
		{
			if (EnumHasAllFlags(Pair.State, EState::BetweenEnterExitState))
			{
				ensureAlwaysMsgf(false, TEXT("Tree exited. Missing ExitState on %s."), *Private::NodeToString(MetaStory.Get(), Pair.NodeID));
				Private::bRuntimeValidationEnterExitState = false;
			}
		}
	}
}

void FRuntimeValidationInstanceData::SetContext(const UObject* InNewOwner, const UMetaStory* InNewMetaStory, bool bInInstanceDataWriteAccessAcquired)
{
	TWeakObjectPtr<const UMetaStory> NewMetaStory = InNewMetaStory;
	TWeakObjectPtr<const UObject> NewOwner = InNewOwner;
	if (Private::bRuntimeValidationContext)
	{
		if (MetaStory.IsValid() && MetaStory != NewMetaStory && bInstanceDataWriteAccessAcquired)
		{
			ensureAlwaysMsgf(false, TEXT("MetaStory runtime check failed: The MetaStory '%s' is different from the previously set '%s'.\n"
				"Make sure you initialize FMetaStoryExecutionContext with the same value every time.\n"
				"Auto deactivate Runtime check MetaStory.RuntimeValidation.Context to prevent reporting the same error multiple times.")
				, InNewMetaStory ? *InNewMetaStory->GetFullName() : TEXT("MetaStory"), *MetaStory.Get()->GetFullName());
			Private::bRuntimeValidationContext = false;
		}
		if (Owner.IsValid() && Owner != NewOwner)
		{
			ensureAlwaysMsgf(false, TEXT("MetaStory runtime check failed: The owner '%s' is different from the previously set '%s'.\n"
				"Make sure you initialize FMetaStoryExecutionContext with the same values every time.\n"
				"Auto deactivate Runtime check MetaStory.RuntimeValidation.Context to prevent reporting the same error multiple times.")
				, InNewOwner ? *InNewOwner->GetFullName() : TEXT("owner"), *Owner.Get()->GetFullName());
			Private::bRuntimeValidationContext = false;
		}
	}

	ValidateTreeNodes(InNewMetaStory);
	ValidateInstanceData(InNewMetaStory);

	MetaStory = NewMetaStory;
	Owner = NewOwner;
	bInstanceDataWriteAccessAcquired |= bInInstanceDataWriteAccessAcquired;
}

void FRuntimeValidationInstanceData::NodeEnterState(FGuid NodeID, FActiveFrameID FrameID)
{
	FNodeStatePair* Found = NodeStates.FindByPredicate([&NodeID, &FrameID](const FNodeStatePair& Other)
	{
		return Other.NodeID == NodeID && Other.FrameID == FrameID;
	});
	if (Found)
	{
		if (Private::bRuntimeValidationEnterExitState && EnumHasAllFlags(Found->State, EState::BetweenEnterExitState))
		{
			ensureAlwaysMsgf(false, TEXT("MetaStory runtime check failed: EnterState executed on node %s without an ExitState.\n"
				"Auto deactivate Runtime check MetaStory.RuntimeValidation.EnterExitState to prevent reporting the same error multiple times.")
				, *Private::NodeToString(Owner.Get(), NodeID));
			Private::bRuntimeValidationEnterExitState = false;
		}
		EnumAddFlags(Found->State, EState::BetweenEnterExitState);
	}
	else
	{
		NodeStates.Add(FNodeStatePair{.NodeID = NodeID, .FrameID = FrameID, .State = EState::BetweenEnterExitState });
	}
}

void FRuntimeValidationInstanceData::NodeExitState(FGuid NodeID, FActiveFrameID FrameID)
{
	FNodeStatePair* Found = NodeStates.FindByPredicate([&NodeID, &FrameID](const FNodeStatePair& Other)
	{
		return Other.NodeID == NodeID && Other.FrameID == FrameID;
	});
	if (Found)
	{
		if (Private::bRuntimeValidationEnterExitState && !EnumHasAllFlags(Found->State, EState::BetweenEnterExitState))
		{
			ensureAlwaysMsgf(false, TEXT("MetaStory runtime check failed: ExitState executed on node %s without an EnterState.\n"
				"Auto deactivate Runtime check MetaStory.RuntimeValidation.EnterExitState to prevent reporting the same error multiple times.")
				, *Private::NodeToString(Owner.Get(), NodeID));
			Private::bRuntimeValidationEnterExitState = false;
		}
		EnumRemoveFlags(Found->State, EState::BetweenEnterExitState);
	}
	else if (Private::bRuntimeValidationEnterExitState)
	{
		ensureAlwaysMsgf(false, TEXT("MetaStory runtime check failed: ExitState executed on node %s without an EnterState.\n"
			"Auto deactivate Runtime check MetaStory.RuntimeValidation.EnterExitState to prevent reporting the same error multiple times.")
			, *Private::NodeToString(Owner.Get(), NodeID));
		Private::bRuntimeValidationEnterExitState = false;
	}
}

void FRuntimeValidationInstanceData::ValidateTreeNodes(const UMetaStory* InNewMetaStory) const
{
	if (Private::bRuntimeValidationDoesNewerVersionExists)
	{
		if (InNewMetaStory && InNewMetaStory->IsReadyToRun())
		{
			auto DoesNewerVersionExists = [](const UObject* InstanceDataType)
			{
				// Is the class/scriptstruct a blueprint that got replaced by another class.
				bool bHasNewerVersionExistsFlag = InstanceDataType->HasAnyFlags(RF_NewerVersionExists);
				if (!bHasNewerVersionExistsFlag)
				{
					if (const UClass* InstanceDataClass = Cast<UClass>(InstanceDataType))
					{
						bHasNewerVersionExistsFlag = InstanceDataClass->HasAnyClassFlags(CLASS_NewerVersionExists);
					}
					else if (const UScriptStruct* InstanceDataStruct = Cast<UScriptStruct>(InstanceDataType))
					{
						bHasNewerVersionExistsFlag = (InstanceDataStruct->StructFlags & STRUCT_NewerVersionExists) != 0;
					}
				}
				return bHasNewerVersionExistsFlag;
			};
			{
				const FMetaStoryInstanceData& InstanceData = InNewMetaStory->GetDefaultInstanceData();
				const int32 InstanceDataNum = InstanceData.Num();
				for (int32 Index = 0; Index < InstanceDataNum; ++Index)
				{
					bool bFailed = false;
					const UObject* InstanceObject = nullptr;
					if (InstanceData.IsObject(Index))
					{
						InstanceObject = InstanceData.GetObject(Index);
						bFailed = DoesNewerVersionExists(InstanceObject)
							|| (InstanceObject && DoesNewerVersionExists(InstanceObject->GetClass()));
					}
					else
					{
						InstanceObject = InstanceData.GetStruct(Index).GetScriptStruct();
						bFailed = DoesNewerVersionExists(InstanceObject);
					}

					if (bFailed)
					{
						ensureAlwaysMsgf(false, TEXT("MetaStory runtime check failed: The data '%s' has a newest version.\n"
							"It should be detected in MetaStory::Link.\n"
							"Auto deactivate Runtime check MetaStory.RuntimeValidation.DoesNewerVersionExists to prevent reporting the same error multiple times.")
							, *InstanceObject->GetFullName());
						Private::bRuntimeValidationDoesNewerVersionExists = false;
					}
				}
			}

			for (FConstStructView NodeView : InNewMetaStory->GetNodes())
			{
				const FMetaStoryNodeBase* Node = NodeView.GetPtr<const FMetaStoryNodeBase>();
				if (Node)
				{
					const UStruct* DesiredInstanceDataType = Node->GetInstanceDataType();
					if (DoesNewerVersionExists(DesiredInstanceDataType))
					{
						ensureAlwaysMsgf(false, TEXT("MetaStory runtime check failed: The node '%s' has a newest version.\n"
							"It should be detected in MetaStory::Link.\n"
							"Auto deactivate Runtime check MetaStory.RuntimeValidation.DoesNewerVersionExists to prevent reporting the same error multiple times.")
							, *DesiredInstanceDataType->GetFullName());
						Private::bRuntimeValidationDoesNewerVersionExists = false;
					}
				}
			}
		}
	}
}

void FRuntimeValidationInstanceData::ValidateInstanceData(const UMetaStory* NewMetaStory)
{
	if (Private::bRuntimeValidationInstanceData)
	{
		if (NewMetaStory)
		{
			if (!NewMetaStory->GetDefaultInstanceData().GetStorage().AreAllInstancesValid())
			{
				ensureAlwaysMsgf(false, TEXT("The instance data has invalid data."));
				Private::bRuntimeValidationInstanceData = false;
			}

			TSharedPtr<FMetaStoryInstanceData> SharedPtr = NewMetaStory->GetSharedInstanceData();
			if (ensureMsgf(SharedPtr, TEXT("The shared instance data is invalid")))
			{
				if (!SharedPtr->GetStorage().AreAllInstancesValid())
				{
					ensureAlwaysMsgf(false, TEXT("The shared instance data has invalid data."));
					Private::bRuntimeValidationInstanceData = false;
				}
			}
		}
	}
}
#endif //WITH_METASTORY_DEBUG

/**
 * 
 */


#if WITH_METASTORY_DEBUG
FRuntimeValidation::FRuntimeValidation(TNotNull<FRuntimeValidationInstanceData*> Instance)
	: RuntimeValidationData(Instance)
{
}

FRuntimeValidationInstanceData* FRuntimeValidation::GetInstanceData() const
{
	return RuntimeValidationData;
}

void FRuntimeValidation::SetContext(const UObject* Owner, const UMetaStory* MetaStory, bool bInstanceDataWriteAccessAcquired) const
{
	if (RuntimeValidationData)
	{
		RuntimeValidationData->SetContext(Owner, MetaStory, bInstanceDataWriteAccessAcquired);
	}
}

#else

void FRuntimeValidation::SetContext(const UObject*, const UMetaStory*, bool) const
{
}

#endif //WITH_METASTORY_DEBUG

} // UE::MetaStory::Debug


