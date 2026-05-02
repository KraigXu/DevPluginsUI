// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryInstanceData.h"
#include "MetaStoryInstanceDataHelpers.h"
#include "MetaStoryExecutionTypes.h"
#include "Debugger/MetaStoryRuntimeValidationInstanceData.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "VisualLogger/VisualLogger.h"
#include "MetaStory.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Serialization/CustomVersion.h"
#include "Serialization/PropertyLocalizationDataGathering.h"
#include "MetaStoryDelegate.h"
#include "AutoRTFM.h"
#include "MetaStoryCustomVersions.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryInstanceData)

// Unique GUID for MetaStory (must not match Engine StateTree's FStateTreeInstanceStorageCustomVersion).
const FGuid FMetaStoryInstanceStorageCustomVersion::GUID(0x7E4C9A2D, 0xF3B81560, 0x9D2E5C81, 0x4A6F0B73);
FCustomVersionRegistration GRegisterMetaStoryInstanceStorageCustomVersion(FMetaStoryInstanceStorageCustomVersion::GUID, FMetaStoryInstanceStorageCustomVersion::LatestVersion, TEXT("MetaStoryInstanceStorage"));

namespace UE::MetaStory::CustomVersions
{
	static const FGuid GLegacyCompatible_InstanceStorageCustomVersionGUID(0x60C4F0DE, 0x8B264C34, 0xAA937201, 0x5DFF09CC);

	int32 GetEffectiveInstanceStorageArchiveVersion(const FArchive& Ar)
	{
		int32 Best = -1;

		const int32 VNew = Ar.CustomVer(FMetaStoryInstanceStorageCustomVersion::GUID);
		if (VNew >= 0)
		{
			Best = FMath::Max(Best, VNew);
		}

		if (Ar.IsLoading())
		{
			const int32 VLegacy = Ar.CustomVer(GLegacyCompatible_InstanceStorageCustomVersionGUID);
			if (VLegacy >= 0)
			{
				Best = FMath::Max(Best, VLegacy);
			}
		}

		return Best;
	}
}

namespace UE::MetaStory::InstanceData::Private
{
	bool AreAllInstancesValid(const FInstancedStructContainer& InstanceStructs)
	{
		for (FConstStructView Instance : InstanceStructs)
		{
			if (!Instance.IsValid())
			{
				return false;
			}
			if (const FMetaStoryInstanceObjectWrapper* Wrapper = Instance.GetPtr<const FMetaStoryInstanceObjectWrapper>())
			{
				if (!Wrapper->InstanceObject)
				{
					return false;
				}
			}
		}
		return true;
	}

	int32 GetAllocatedMemory(const FInstancedStructContainer& InstanceStructs)
	{
		int32 Size = InstanceStructs.GetAllocatedMemory();
		for (FConstStructView Instance : InstanceStructs)
		{
			if (const FMetaStoryInstanceObjectWrapper* Wrapper = Instance.GetPtr<const FMetaStoryInstanceObjectWrapper>())
			{
				if (Wrapper->InstanceObject)
				{
					Size += Wrapper->InstanceObject->GetClass()->GetStructureSize();
				}
			}
		}
		return Size;
	}

	TNotNull<UObject*> CopyNodeInstance(TNotNull<UObject*> Instance, TNotNull<UObject*> InOwner, bool bDuplicate)
	{
		const UClass* InstanceClass = Instance->GetClass();
		if (InstanceClass->HasAnyClassFlags(CLASS_NewerVersionExists))
		{
			const UClass* AuthoritativeClass = InstanceClass->GetAuthoritativeClass();
			UObject* NewInstance = NewObject<UObject>(InOwner, AuthoritativeClass);

			// Try to copy the values over using serialization
			// FObjectAndNameAsStringProxyArchive is used to store and restore names and objects as memory writer does not support UObject references at all.
			TArray<uint8> Data;
			FMemoryWriter Writer(Data);
			FObjectAndNameAsStringProxyArchive WriterProxy(Writer, /*bInLoadIfFindFails*/true);
			Instance->Serialize(WriterProxy);

			FMemoryReader Reader(Data);
			FObjectAndNameAsStringProxyArchive ReaderProxy(Reader, /*bInLoadIfFindFails*/true);
			NewInstance->Serialize(ReaderProxy);

			const UMetaStory* OuterMetaStory = Instance->GetTypedOuter<UMetaStory>();
			UE_LOG(LogMetaStory, Display, TEXT("FMetaStoryInstanceData: Duplicating '%s' with old class '%s' as '%s', potential data loss. Please resave MetaStory asset %s."),
				*GetFullNameSafe(Instance), *GetNameSafe(InstanceClass), *GetNameSafe(AuthoritativeClass), *GetFullNameSafe(OuterMetaStory));

			return NewInstance;
		}

		if (bDuplicate)
		{
			return ::DuplicateObject(&(*Instance), &(*InOwner));
		}

		return Instance;
	}

	void PostAppendToInstanceStructContainer(FInstancedStructContainer& InstanceStructs, TNotNull<UObject*> InOwner, bool bDuplicateWrappedObject, int32 StartIndex)
	{
		for (int32 Index = StartIndex; Index < InstanceStructs.Num(); ++Index)
		{
			if (FMetaStoryInstanceObjectWrapper* Wrapper = InstanceStructs[Index].GetPtr<FMetaStoryInstanceObjectWrapper>())
			{
				if (Wrapper->InstanceObject)
				{
					const bool bDuplicate = bDuplicateWrappedObject || InOwner != Wrapper->InstanceObject->GetOuter();
					Wrapper->InstanceObject = CopyNodeInstance(Wrapper->InstanceObject, InOwner, bDuplicate);
				}
			}
		}
	}
}

namespace UE::MetaStory
{

#if WITH_EDITORONLY_DATA
	void GatherForLocalization(const FString& PathToParent, const UScriptStruct* Struct, const void* StructData, const void* DefaultStructData, FPropertyLocalizationDataGatherer& PropertyLocalizationDataGatherer, const EPropertyLocalizationGathererTextFlags GatherTextFlags)
	{
		const FMetaStoryInstanceData* ThisInstance = static_cast<const FMetaStoryInstanceData*>(StructData);
		const FMetaStoryInstanceData* DefaultInstance = static_cast<const FMetaStoryInstanceData*>(DefaultStructData);

		PropertyLocalizationDataGatherer.GatherLocalizationDataFromStruct(PathToParent, Struct, StructData, DefaultStructData, GatherTextFlags);

		const uint8* DefaultInstanceMemory = nullptr;
		if (DefaultInstance)
		{
			DefaultInstanceMemory = reinterpret_cast<const uint8*>(&DefaultInstance->GetStorage());
		}
		
		const UScriptStruct* StructTypePtr = FMetaStoryInstanceStorage::StaticStruct();
		PropertyLocalizationDataGatherer.GatherLocalizationDataFromStructWithCallbacks(PathToParent + TEXT(".InstanceStorage"), StructTypePtr, &ThisInstance->GetStorage(), DefaultInstanceMemory, GatherTextFlags);
	}

	void RegisterInstanceDataForLocalization()
	{
		{ static const FAutoRegisterLocalizationDataGatheringCallback AutomaticRegistrationOfLocalizationGatherer(TBaseStructure<FMetaStoryInstanceData>::Get(), &GatherForLocalization); }
	}
#endif // WITH_EDITORONLY_DATA

} // UE::MetaStory


//----------------------------------------------------------------//
// FMetaStoryInstanceStorage
//----------------------------------------------------------------//

#if WITH_METASTORY_DEBUG
FMetaStoryInstanceStorage::FMetaStoryInstanceStorage()
	: RuntimeValidationData(MakePimpl<UE::MetaStory::Debug::FRuntimeValidationInstanceData>())
{
}
#else
FMetaStoryInstanceStorage::FMetaStoryInstanceStorage() = default;
#endif

namespace UE::MetaStory::InstanceData::Private
{
	bool IsActiveInstanceHandleSourceValid(const FMetaStoryInstanceStorage& Storage, const FMetaStoryExecutionFrame& CurrentFrame, const FMetaStoryDataHandle& Handle)
	{
		return CurrentFrame.ActiveInstanceIndexBase.IsValid()
			&& CurrentFrame.ActiveStates.Contains(Handle.GetState())
			&& Storage.IsValidIndex(CurrentFrame.ActiveInstanceIndexBase.Get() + Handle.GetIndex());
	}

	bool IsHandleSourceValid(
		const FMetaStoryInstanceStorage& InstanceStorage,
		const FMetaStoryExecutionFrame* ParentFrame,
		const FMetaStoryExecutionFrame& CurrentFrame,
		const FMetaStoryDataHandle& Handle)
	{
		// Checks that the instance data is valid for specific handle types.
		// 
		// The CurrentFrame may not be yet properly initialized, for that reason we need to check
		// that the path to the handle makes sense (it's part of the active states) as well as that
		// we actually have instance data for the handle (index is valid).
		// 
		// The (base) indices can be invalid if the frame/state is not entered yet.
		// For active instance data we need to check that the frame is initialized for a specific state,
		// as well as that the instance data is initialized.

		switch (Handle.GetSource())
		{
		case EMetaStoryDataSourceType::None:
			return true;

		case EMetaStoryDataSourceType::GlobalInstanceData:
		case EMetaStoryDataSourceType::GlobalInstanceDataObject:
			return CurrentFrame.GlobalInstanceIndexBase.IsValid()
				&& InstanceStorage.IsValidIndex(CurrentFrame.GlobalInstanceIndexBase.Get() + Handle.GetIndex());

		case EMetaStoryDataSourceType::ActiveInstanceData:
		case EMetaStoryDataSourceType::ActiveInstanceDataObject:
		case EMetaStoryDataSourceType::StateParameterData:
			return IsActiveInstanceHandleSourceValid(InstanceStorage, CurrentFrame, Handle);
				
		case EMetaStoryDataSourceType::SharedInstanceData:
		case EMetaStoryDataSourceType::SharedInstanceDataObject:
			return true;

		case EMetaStoryDataSourceType::GlobalParameterData:
			return ParentFrame
				? IsHandleSourceValid(InstanceStorage, nullptr, *ParentFrame, CurrentFrame.GlobalParameterDataHandle)
				: CurrentFrame.GlobalParameterDataHandle.IsValid();

		case EMetaStoryDataSourceType::SubtreeParameterData:
			if (ParentFrame)
			{
				// If the current subtree state is not instantiated yet, we cannot assume that the parameter data is instantiated in the parent frame either. 
				if (!CurrentFrame.ActiveInstanceIndexBase.IsValid())
				{
					return false;
				}
				// Linked subtree, params defined in parent scope.
				return IsHandleSourceValid(InstanceStorage, nullptr, *ParentFrame, CurrentFrame.StateParameterDataHandle);
			}
			// Standalone subtree, params define as state params.
			return IsActiveInstanceHandleSourceValid(InstanceStorage, CurrentFrame, Handle);

		default:
			checkf(false, TEXT("Unhandled case or unsupported type for InstanceDataStorage %s"), *UEnum::GetValueAsString(Handle.GetSource()));
		}

		return false;
	}

	FMetaStoryDataView GetTemporaryDataView(
		FMetaStoryInstanceStorage& InstanceStorage,
		const FMetaStoryExecutionFrame* ParentFrame,
		const FMetaStoryExecutionFrame& CurrentFrame,
		const FMetaStoryDataHandle& Handle)
	{
		switch (Handle.GetSource())
		{
		case EMetaStoryDataSourceType::GlobalInstanceData:
		case EMetaStoryDataSourceType::ActiveInstanceData:
			return InstanceStorage.GetMutableTemporaryStruct(CurrentFrame, Handle);

		case EMetaStoryDataSourceType::GlobalInstanceDataObject:
		case EMetaStoryDataSourceType::ActiveInstanceDataObject:
			return InstanceStorage.GetMutableTemporaryObject(CurrentFrame, Handle);

		case EMetaStoryDataSourceType::GlobalParameterData:
			if (ParentFrame)
			{
				if (FMetaStoryCompactParameters* Params = InstanceStorage.GetMutableTemporaryStruct(*ParentFrame, CurrentFrame.GlobalParameterDataHandle).GetPtr<FMetaStoryCompactParameters>())
				{
					return Params->Parameters.GetMutableValue();
				}
			}
			break;

		case EMetaStoryDataSourceType::SubtreeParameterData:
			if (ParentFrame)
			{
				// Linked subtree, params defined in parent scope.
				if (FMetaStoryCompactParameters* Params = InstanceStorage.GetMutableTemporaryStruct(*ParentFrame, CurrentFrame.StateParameterDataHandle).GetPtr<FMetaStoryCompactParameters>())
				{
					return Params->Parameters.GetMutableValue();
				}
			}
			// Standalone subtree, params define as state params.
			if (FMetaStoryCompactParameters* Params = InstanceStorage.GetMutableTemporaryStruct(CurrentFrame, Handle).GetPtr<FMetaStoryCompactParameters>())
			{
				return Params->Parameters.GetMutableValue();
			}
			break;

		case EMetaStoryDataSourceType::StateParameterData:
			if (FMetaStoryCompactParameters* Params = InstanceStorage.GetMutableTemporaryStruct(CurrentFrame, Handle).GetPtr<FMetaStoryCompactParameters>())
			{
				return Params->Parameters.GetMutableValue();
			}
			break;

		default:
			checkf(false, TEXT("Unhandled case or unsupported type for InstanceDataStorage %s"), *UEnum::GetValueAsString(Handle.GetSource()));
		}

		return {};
	}

} // namespace

namespace UE::MetaStory::InstanceData
{
	FMetaStoryDataView GetDataView(
		FMetaStoryInstanceStorage& InstanceStorage,
		FMetaStoryInstanceStorage* SharedInstanceStorage,
		const FMetaStoryExecutionFrame* ParentFrame,
		const FMetaStoryExecutionFrame& CurrentFrame,
		const FMetaStoryDataHandle& Handle)
	{
		switch (Handle.GetSource())
		{
		case EMetaStoryDataSourceType::GlobalInstanceData:
			return InstanceStorage.GetMutableStruct(CurrentFrame.GlobalInstanceIndexBase.Get() + Handle.GetIndex());
		case EMetaStoryDataSourceType::GlobalInstanceDataObject:
			return InstanceStorage.GetMutableObject(CurrentFrame.GlobalInstanceIndexBase.Get() + Handle.GetIndex());

		case EMetaStoryDataSourceType::ActiveInstanceData:
			return InstanceStorage.GetMutableStruct(CurrentFrame.ActiveInstanceIndexBase.Get() + Handle.GetIndex());
		case EMetaStoryDataSourceType::ActiveInstanceDataObject:
			return InstanceStorage.GetMutableObject(CurrentFrame.ActiveInstanceIndexBase.Get() + Handle.GetIndex());

		case EMetaStoryDataSourceType::SharedInstanceData:
			check(SharedInstanceStorage);
			return SharedInstanceStorage->GetMutableStruct(Handle.GetIndex());
		case EMetaStoryDataSourceType::SharedInstanceDataObject:
			check(SharedInstanceStorage);
			return SharedInstanceStorage->GetMutableObject(Handle.GetIndex());

		case EMetaStoryDataSourceType::GlobalParameterData:
			// Defined in parent frame or is root MetaStory parameters
			if (ParentFrame)
			{
				return GetDataView(InstanceStorage, SharedInstanceStorage, nullptr, *ParentFrame, CurrentFrame.GlobalParameterDataHandle);
			}
			return InstanceStorage.GetMutableGlobalParameters();

		case EMetaStoryDataSourceType::SubtreeParameterData:
		{
			// Defined in parent frame.
			if (ParentFrame)
			{
				// Linked subtree, params defined in parent scope.
				return GetDataView(InstanceStorage, SharedInstanceStorage, nullptr, *ParentFrame, CurrentFrame.StateParameterDataHandle);
			}
			// Standalone subtree, params define as state params.
			FMetaStoryCompactParameters& SubtreeParams = InstanceStorage.GetMutableStruct(CurrentFrame.ActiveInstanceIndexBase.Get() + Handle.GetIndex()).Get<FMetaStoryCompactParameters>();
			return SubtreeParams.Parameters.GetMutableValue();
		}

		case EMetaStoryDataSourceType::StateParameterData:
		{
			FMetaStoryCompactParameters& StateParams = InstanceStorage.GetMutableStruct(CurrentFrame.ActiveInstanceIndexBase.Get() + Handle.GetIndex()).Get<FMetaStoryCompactParameters>();
			return StateParams.Parameters.GetMutableValue();
		}

		case EMetaStoryDataSourceType::StateEvent:
		{
			// Return FMetaStoryEvent from shared event.
			FMetaStorySharedEvent& SharedEvent = InstanceStorage.GetMutableStruct(CurrentFrame.ActiveInstanceIndexBase.Get() + Handle.GetIndex()).Get<FMetaStorySharedEvent>();
			if (ensure(SharedEvent.IsValid()))
			{
				// Events are read only, but we cannot express that in FMetaStoryDataView.
				return FMetaStoryDataView(FStructView::Make(*SharedEvent.GetMutable()));
			}
			return {};
		}

		default:
			checkf(false, TEXT("Unhandled case or unsupported type for InstanceDataStorage %s"), *UEnum::GetValueAsString(Handle.GetSource()));
		}

		return {};
	}

	FMetaStoryDataView GetDataViewOrTemporary(
		FMetaStoryInstanceStorage& InstanceStorage,
		FMetaStoryInstanceStorage* SharedInstanceStorage,
		const FMetaStoryExecutionFrame* ParentFrame,
		const FMetaStoryExecutionFrame& CurrentFrame,
		const FMetaStoryDataHandle& Handle)
	{
		if (Private::IsHandleSourceValid(InstanceStorage, ParentFrame, CurrentFrame, Handle))
		{
			return GetDataView(InstanceStorage, SharedInstanceStorage, ParentFrame, CurrentFrame, Handle);
		}

		return Private::GetTemporaryDataView(InstanceStorage, ParentFrame, CurrentFrame, Handle);
	}
} // namespace

FMetaStoryInstanceStorage::FMetaStoryInstanceStorage(const FMetaStoryInstanceStorage& Other)
	: InstanceStructs(Other.InstanceStructs)
	, ExecutionState(Other.ExecutionState)
	, ExecutionRuntimeData(Other.ExecutionRuntimeData)
	, ExecutionRuntimeDataInfos(Other.ExecutionRuntimeDataInfos)
	, TemporaryInstances(Other.TemporaryInstances)
	, EventQueue(MakeShared<FMetaStoryEventQueue>(*Other.EventQueue))
	, TransitionRequests(Other.TransitionRequests)
	, GlobalParameters(Other.GlobalParameters)
#if ENABLE_MT_DETECTOR
	, AccessDetector(Other.AccessDetector)
#endif
	, bIsOwningEventQueue(true)
#if WITH_METASTORY_DEBUG
	, RuntimeValidationData(MakePimpl<UE::MetaStory::Debug::FRuntimeValidationInstanceData>(*Other.RuntimeValidationData))
#endif
{
}

FMetaStoryInstanceStorage::FMetaStoryInstanceStorage(FMetaStoryInstanceStorage&& Other) noexcept
	: InstanceStructs(MoveTemp(Other.InstanceStructs))
	, ExecutionState(MoveTemp(Other.ExecutionState))
	, ExecutionRuntimeData(MoveTemp(Other.ExecutionRuntimeData))
	, ExecutionRuntimeDataInfos(MoveTemp(Other.ExecutionRuntimeDataInfos))
	, TemporaryInstances(MoveTemp(Other.TemporaryInstances))
	, EventQueue(Other.EventQueue)
	, TransitionRequests(MoveTemp(Other.TransitionRequests))
	, GlobalParameters(MoveTemp(Other.GlobalParameters))
#if ENABLE_MT_DETECTOR
	, AccessDetector(MoveTemp(Other.AccessDetector))
#endif
	, bIsOwningEventQueue(Other.bIsOwningEventQueue)
#if WITH_METASTORY_DEBUG
	, RuntimeValidationData(MoveTemp(Other.RuntimeValidationData))
#endif
{
	Other.EventQueue = MakeShared<FMetaStoryEventQueue>();
#if WITH_METASTORY_DEBUG
	Other.RuntimeValidationData = MakePimpl<UE::MetaStory::Debug::FRuntimeValidationInstanceData>();
#endif
}

FMetaStoryInstanceStorage& FMetaStoryInstanceStorage::operator=(const FMetaStoryInstanceStorage& Other)
{
	InstanceStructs = Other.InstanceStructs;
	ExecutionState = Other.ExecutionState;
	ExecutionRuntimeData = Other.ExecutionRuntimeData;
	ExecutionRuntimeDataInfos = Other.ExecutionRuntimeDataInfos;
	TemporaryInstances = Other.TemporaryInstances;
	EventQueue = MakeShared<FMetaStoryEventQueue>(*Other.EventQueue);
	TransitionRequests = Other.TransitionRequests;
	GlobalParameters = Other.GlobalParameters;

#if ENABLE_MT_DETECTOR
	AccessDetector = Other.AccessDetector;
#endif

	bIsOwningEventQueue = true;

#if WITH_METASTORY_DEBUG
	RuntimeValidationData = MakePimpl<UE::MetaStory::Debug::FRuntimeValidationInstanceData>(*Other.RuntimeValidationData);
#endif

	return *this;
}

FMetaStoryInstanceStorage& FMetaStoryInstanceStorage::operator=(FMetaStoryInstanceStorage&& Other) noexcept
{
	InstanceStructs = MoveTemp(Other.InstanceStructs);
	ExecutionState = MoveTemp(Other.ExecutionState);
	ExecutionRuntimeData = MoveTemp(Other.ExecutionRuntimeData);
	ExecutionRuntimeDataInfos = MoveTemp(Other.ExecutionRuntimeDataInfos);
	TemporaryInstances = MoveTemp(Other.TemporaryInstances);
	EventQueue = Other.EventQueue;
	Other.EventQueue = MakeShared<FMetaStoryEventQueue>();
	TransitionRequests = MoveTemp(Other.TransitionRequests);
	GlobalParameters = MoveTemp(Other.GlobalParameters);

#if ENABLE_MT_DETECTOR
	AccessDetector = MoveTemp(Other.AccessDetector);
#endif

	bIsOwningEventQueue = Other.bIsOwningEventQueue;
	Other.bIsOwningEventQueue = true;

#if WITH_METASTORY_DEBUG
	RuntimeValidationData = MoveTemp(Other.RuntimeValidationData);
	Other.RuntimeValidationData = MakePimpl<UE::MetaStory::Debug::FRuntimeValidationInstanceData>();
#endif

	return *this;
}

void FMetaStoryInstanceStorage::SetSharedEventQueue(const TSharedRef<FMetaStoryEventQueue>& InSharedEventQueue)
{
	EventQueue = InSharedEventQueue;
	bIsOwningEventQueue = false;
}

void FMetaStoryInstanceStorage::AddTransitionRequest(const UObject* Owner, const FMetaStoryTransitionRequest& Request)
{
	constexpr int32 MaxPendingTransitionRequests = 32;
	
	if (TransitionRequests.Num() >= MaxPendingTransitionRequests)
	{
		UE_VLOG_UELOG(Owner, LogMetaStory, Error, TEXT("%s: Too many transition requests sent to '%s' (%d pending). Dropping request."), ANSI_TO_TCHAR(__FUNCTION__), *GetNameSafe(Owner), TransitionRequests.Num());
		return;
	}

	TransitionRequests.Add(Request);
}

void FMetaStoryInstanceStorage::MarkDelegateAsBroadcasted(const FMetaStoryDelegateDispatcher& Dispatcher)
{
	// The array is reset once the transitions are processed.
	BroadcastedDelegates.AddUnique(Dispatcher);
}

bool FMetaStoryInstanceStorage::IsDelegateBroadcasted(const FMetaStoryDelegateDispatcher& Dispatcher) const
{
	return BroadcastedDelegates.Contains(Dispatcher);
}

void FMetaStoryInstanceStorage::ResetBroadcastedDelegates()
{
	BroadcastedDelegates.Empty();
}

bool FMetaStoryInstanceStorage::HasBroadcastedDelegates() const
{
	return !BroadcastedDelegates.IsEmpty();
}

void FMetaStoryInstanceStorage::ResetTransitionRequests()
{
	TransitionRequests.Reset();
}

bool FMetaStoryInstanceStorage::AreAllInstancesValid() const
{
	return UE::MetaStory::InstanceData::Private::AreAllInstancesValid(InstanceStructs)
		&& ExecutionRuntimeData.AreAllInstancesValid();
}

int32 FMetaStoryInstanceStorage::AddExecutionRuntimeData(TNotNull<UObject*> Owner, const UE::MetaStory::FMetaStoryExecutionFrameHandle FrameHandle)
{
	using namespace UE::MetaStory;
	using namespace UE::MetaStory::InstanceData;

	check(FrameHandle.IsValid());

	const FObjectKey MetaStoryKey = FrameHandle.GetMetaStory();
	const FExecutionRuntimeInfo* Info = ExecutionRuntimeDataInfos.FindByPredicate([MetaStoryKey](const FExecutionRuntimeInfo& Other)
		{
			return Other.MetaStory == MetaStoryKey;
		});
	if (Info != nullptr)
	{
		return Info->StartIndex;
	}

	const int32 StartIndex = ExecutionRuntimeData.Append(Owner, FrameHandle.GetMetaStory()->GetDefaultExecutionRuntimeData(), FMetaStoryInstanceContainer::FAddArgs());

	FExecutionRuntimeInfo& NewInfo = ExecutionRuntimeDataInfos.AddDefaulted_GetRef();
	NewInfo.MetaStory = MetaStoryKey;
	NewInfo.StartIndex = StartIndex;

	return StartIndex;
}

FStructView FMetaStoryInstanceStorage::AddTemporaryInstance(UObject& InOwner, const FMetaStoryExecutionFrame& Frame, const FMetaStoryIndex16 OwnerNodeIndex, const FMetaStoryDataHandle DataHandle, FConstStructView NewInstanceData)
{
	FMetaStoryTemporaryInstanceData* TempInstance = TemporaryInstances.FindByPredicate([&Frame, &OwnerNodeIndex, &DataHandle](const FMetaStoryTemporaryInstanceData& TempInstance)
	{
		return TempInstance.FrameID == Frame.FrameID
				&& TempInstance.OwnerNodeIndex == OwnerNodeIndex
				&& TempInstance.DataHandle == DataHandle;
	});
	
	if (TempInstance)
	{
		if (TempInstance->Instance.GetScriptStruct() != NewInstanceData.GetScriptStruct())
		{
			TempInstance->Instance = NewInstanceData;
		}
	}
	else
	{
		TempInstance = &TemporaryInstances.AddDefaulted_GetRef();
		check(TempInstance);
		TempInstance->FrameID = Frame.FrameID;
		TempInstance->OwnerNodeIndex = OwnerNodeIndex;
		TempInstance->DataHandle = DataHandle;
		TempInstance->Instance = NewInstanceData;
	}

	if (FMetaStoryInstanceObjectWrapper* Wrapper = TempInstance->Instance.GetMutablePtr<FMetaStoryInstanceObjectWrapper>())
	{
		if (Wrapper->InstanceObject)
		{
			constexpr bool bDuplicate = true;
			Wrapper->InstanceObject = UE::MetaStory::InstanceData::Private::CopyNodeInstance(Wrapper->InstanceObject, &InOwner, bDuplicate);
		}
	}

	return TempInstance->Instance;
}

FStructView FMetaStoryInstanceStorage::GetMutableTemporaryStruct(const FMetaStoryExecutionFrame& Frame, const FMetaStoryDataHandle DataHandle)
{
	FMetaStoryTemporaryInstanceData* ExistingInstance = TemporaryInstances.FindByPredicate([&Frame, &DataHandle](const FMetaStoryTemporaryInstanceData& TempInstance)
	{
		return TempInstance.FrameID == Frame.FrameID
				&& TempInstance.DataHandle == DataHandle;
	});
	return ExistingInstance ? FStructView(ExistingInstance->Instance) : FStructView();
}

UObject* FMetaStoryInstanceStorage::GetMutableTemporaryObject(const FMetaStoryExecutionFrame& Frame, const FMetaStoryDataHandle DataHandle)
{
	FMetaStoryTemporaryInstanceData* ExistingInstance = TemporaryInstances.FindByPredicate([&Frame, &DataHandle](const FMetaStoryTemporaryInstanceData& TempInstance)
	{
		return TempInstance.FrameID == Frame.FrameID
				&& TempInstance.DataHandle == DataHandle;
	});
	if (ExistingInstance)
	{
		const FMetaStoryInstanceObjectWrapper& Wrapper = ExistingInstance->Instance.Get<FMetaStoryInstanceObjectWrapper>();
		return Wrapper.InstanceObject;
	}
	return nullptr;
}

void FMetaStoryInstanceStorage::ResetTemporaryInstances()
{
	TemporaryInstances.Reset();
}


void FMetaStoryInstanceStorage::SetGlobalParameters(const FInstancedPropertyBag& Parameters)
{
	GlobalParameters = Parameters;
}

uint32 FMetaStoryInstanceStorage::GenerateUniqueId()
{
	uint32 NewId = ++UniqueIdGenerator;
	if (NewId == 0)
	{
#if WITH_METASTORY_TRACE && DO_ENSURE
		ensureAlwaysMsgf(false, TEXT("The unique id overflow. Id:%d Serial:%d"), ExecutionState.InstanceDebugId.Id, ExecutionState.InstanceDebugId.SerialNumber);
#elif WITH_METASTORY_TRACE
	UE_LOG(LogMetaStory, Error, TEXT("The unique id overflow. Id:%d Serial:%d"), ExecutionState.InstanceDebugId.Id, ExecutionState.InstanceDebugId.SerialNumber);
#else
	UE_LOG(LogMetaStory, Error, TEXT("The unique id overflow."));
#endif
		NewId = ++UniqueIdGenerator;
	}
	return NewId;
}

void FMetaStoryInstanceStorage::AddStructReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddPropertyReferencesWithStructARO(TBaseStructure<FMetaStoryInstanceStorage>::Get(), this);
	Collector.AddPropertyReferencesWithStructARO(TBaseStructure<FMetaStoryEventQueue>::Get(), &EventQueue.Get());
}

void FMetaStoryInstanceStorage::Reset()
{
	InstanceStructs.Reset();
	ExecutionState.Reset();
	ExecutionRuntimeData.Reset();
	ExecutionRuntimeDataInfos.Reset();
	TemporaryInstances.Reset();
	if (bIsOwningEventQueue)
	{
		EventQueue->Reset();
	}
	TransitionRequests.Reset();
	GlobalParameters.Reset();

#if WITH_METASTORY_DEBUG
	RuntimeValidationData = MakePimpl<UE::MetaStory::Debug::FRuntimeValidationInstanceData>();
#endif
}

// #jira SOL-8070: Ideally, we should use the transactionally-safe access detector instead of relying on OPEN and ONABORT blocks here.
void FMetaStoryInstanceStorage::AcquireReadAccess()
{
	UE_AUTORTFM_OPEN
	{
		UE_MT_ACQUIRE_READ_ACCESS(AccessDetector);
	};
	UE_AUTORTFM_ONABORT(this)
	{
		UE_MT_RELEASE_READ_ACCESS(AccessDetector);
	};
}

void FMetaStoryInstanceStorage::ReleaseReadAccess()
{
	UE_AUTORTFM_OPEN
	{
		UE_MT_RELEASE_READ_ACCESS(AccessDetector);
	};
	UE_AUTORTFM_ONABORT(this)
	{
		UE_MT_ACQUIRE_READ_ACCESS(AccessDetector);
	};
}

void FMetaStoryInstanceStorage::AcquireWriteAccess()
{
	UE_AUTORTFM_OPEN
	{
		UE_MT_ACQUIRE_WRITE_ACCESS(AccessDetector);
	};
	UE_AUTORTFM_ONABORT(this)
	{
		UE_MT_RELEASE_WRITE_ACCESS(AccessDetector);
	};
}

void FMetaStoryInstanceStorage::ReleaseWriteAccess()
{
	UE_AUTORTFM_OPEN
	{
		UE_MT_RELEASE_WRITE_ACCESS(AccessDetector);
	};
	UE_AUTORTFM_ONABORT(this)
	{
		UE_MT_ACQUIRE_WRITE_ACCESS(AccessDetector);
	};
}

UE::MetaStory::Debug::FRuntimeValidation FMetaStoryInstanceStorage::GetRuntimeValidation() const
{
#if WITH_METASTORY_DEBUG
	return UE::MetaStory::Debug::FRuntimeValidation(RuntimeValidationData.Get());
#else
	return UE::MetaStory::Debug::FRuntimeValidation();
#endif
}

//----------------------------------------------------------------//
// FMetaStoryInstanceData
//----------------------------------------------------------------//

FMetaStoryInstanceData::FAddArgs FMetaStoryInstanceData::FAddArgs::Default;

FMetaStoryInstanceData::FMetaStoryInstanceData() = default;

FMetaStoryInstanceData::FMetaStoryInstanceData(const FMetaStoryInstanceData& Other)
{
	InstanceStorage = MakeShared<FMetaStoryInstanceStorage>(*Other.InstanceStorage);
}

FMetaStoryInstanceData::FMetaStoryInstanceData(FMetaStoryInstanceData&& Other) noexcept
{
	InstanceStorage = Other.InstanceStorage;
	Other.InstanceStorage = MakeShared<FMetaStoryInstanceStorage>();
}

FMetaStoryInstanceData& FMetaStoryInstanceData::operator=(const FMetaStoryInstanceData& Other)
{
	InstanceStorage = MakeShared<FMetaStoryInstanceStorage>(*Other.InstanceStorage);
	return *this;
}

FMetaStoryInstanceData& FMetaStoryInstanceData::operator=(FMetaStoryInstanceData&& Other) noexcept
{
	InstanceStorage = Other.InstanceStorage;
	Other.InstanceStorage = MakeShared<FMetaStoryInstanceStorage>();
	return *this;
}

FMetaStoryInstanceData::~FMetaStoryInstanceData()
{
	Reset();
}

const FMetaStoryInstanceStorage& FMetaStoryInstanceData::GetStorage() const
{
	return *InstanceStorage;
}

TWeakPtr<FMetaStoryInstanceStorage> FMetaStoryInstanceData::GetWeakMutableStorage()
{
	return InstanceStorage;
}

TWeakPtr<const FMetaStoryInstanceStorage> FMetaStoryInstanceData::GetWeakStorage() const
{
	return InstanceStorage;
}

FMetaStoryInstanceStorage& FMetaStoryInstanceData::GetMutableStorage()
{
	return *InstanceStorage;
}

FMetaStoryEventQueue& FMetaStoryInstanceData::GetMutableEventQueue()
{
	return GetMutableStorage().GetMutableEventQueue();
}

const TSharedRef<FMetaStoryEventQueue>& FMetaStoryInstanceData::GetSharedMutableEventQueue()
{
	return GetMutableStorage().GetSharedMutableEventQueue();
}

const FMetaStoryEventQueue& FMetaStoryInstanceData::GetEventQueue() const
{
	return GetStorage().GetEventQueue();
}

bool FMetaStoryInstanceData::IsOwningEventQueue() const
{
	return GetStorage().IsOwningEventQueue();
}

void FMetaStoryInstanceData::SetSharedEventQueue(const TSharedRef<FMetaStoryEventQueue>& InSharedEventQueue)
{
	return GetMutableStorage().SetSharedEventQueue(InSharedEventQueue);
}

void FMetaStoryInstanceData::AddTransitionRequest(const UObject* Owner, const FMetaStoryTransitionRequest& Request)
{
	GetMutableStorage().AddTransitionRequest(Owner, Request);
}

TConstArrayView<FMetaStoryTransitionRequest> FMetaStoryInstanceData::GetTransitionRequests() const
{
	return GetStorage().GetTransitionRequests();
}

void FMetaStoryInstanceData::ResetTransitionRequests()
{
	GetMutableStorage().ResetTransitionRequests();
}

bool FMetaStoryInstanceData::AreAllInstancesValid() const
{
	return GetStorage().AreAllInstancesValid();
}

int32 FMetaStoryInstanceData::GetEstimatedMemoryUsage() const
{
	const FMetaStoryInstanceStorage& Storage = GetStorage();
	int32 Size = sizeof(FMetaStoryInstanceData);

	Size += UE::MetaStory::InstanceData::Private::GetAllocatedMemory(Storage.InstanceStructs);
	Size += Storage.ExecutionRuntimeData.GetAllocatedMemory();

	return Size;
}

bool FMetaStoryInstanceData::Identical(const FMetaStoryInstanceData* Other, uint32 PortFlags) const
{
	if (Other == nullptr)
	{
		return false;
	}

	const FMetaStoryInstanceStorage& Storage = GetStorage();
	const FMetaStoryInstanceStorage& OtherStorage = Other->GetStorage();

	// Not identical if global parameters don't match.
	if (!Storage.GlobalParameters.Identical(&OtherStorage.GlobalParameters, PortFlags))
	{
		return false;
	}

	// Not identical if structs are different.
	if (Storage.InstanceStructs.Identical(&OtherStorage.InstanceStructs, PortFlags) == false)
	{
		return false;
	}
	
	// Check that the instance object contents are identical.
	// Copied from object property.
	auto AreObjectsIdentical = [](UObject* A, UObject* B, uint32 PortFlags) -> bool
	{
		if ((PortFlags & PPF_DuplicateForPIE) != 0)
		{
			return false;
		}

		if (A == B)
		{
			return true;
		}

		// Resolve the object handles and run the deep comparison logic 
		if ((PortFlags & (PPF_DeepCompareInstances | PPF_DeepComparison)) != 0)
		{
			return FObjectPropertyBase::StaticIdentical(A, B, PortFlags);
		}

		return true;
	};

	bool bResult = true;

	for (int32 Index = 0; Index < Storage.InstanceStructs.Num(); Index++)
	{
		const FMetaStoryInstanceObjectWrapper* Wrapper = Storage.InstanceStructs[Index].GetPtr<const FMetaStoryInstanceObjectWrapper>();
		const FMetaStoryInstanceObjectWrapper* OtherWrapper = OtherStorage.InstanceStructs[Index].GetPtr<const FMetaStoryInstanceObjectWrapper>();

		if (Wrapper)
		{
			if (!OtherWrapper)
			{
				bResult = false;
				break;
			}
			if (Wrapper->InstanceObject && OtherWrapper->InstanceObject)
			{
				if (!AreObjectsIdentical(Wrapper->InstanceObject, OtherWrapper->InstanceObject, PortFlags))
				{
					bResult = false;
					break;
				}
			}
		}
	}
	
	return bResult;
}

void FMetaStoryInstanceData::AddStructReferencedObjects(FReferenceCollector& Collector)
{
	GetMutableStorage().AddStructReferencedObjects(Collector);
}

bool FMetaStoryInstanceData::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FMetaStoryInstanceStorageCustomVersion::GUID);

	if (Ar.IsLoading())
	{
		if (UE::MetaStory::CustomVersions::GetEffectiveInstanceStorageArchiveVersion(Ar) < FMetaStoryInstanceStorageCustomVersion::AddedCustomSerialization)
		{
#if WITH_EDITORONLY_DATA
PRAGMA_DISABLE_DEPRECATION_WARNINGS
			StaticStruct()->SerializeTaggedProperties(Ar, (uint8*)this, StaticStruct(), nullptr);

			if (InstanceStorage_DEPRECATED.IsValid())
			{
				InstanceStorage = MakeShared<FMetaStoryInstanceStorage>(MoveTemp(InstanceStorage_DEPRECATED.GetMutable()));
				InstanceStorage_DEPRECATED.Reset();
				return true;
			}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITORONLY_DATA

			InstanceStorage = MakeShared<FMetaStoryInstanceStorage>();
			return true;
		}

		InstanceStorage = MakeShared<FMetaStoryInstanceStorage>();
	}

	FMetaStoryInstanceStorage::StaticStruct()->SerializeItem(Ar, &InstanceStorage.Get(), nullptr);

	return true;
}

void FMetaStoryInstanceData::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	UScriptStruct* ScriptStruct = FMetaStoryInstanceStorage::StaticStruct();
	OutDeps.Add(ScriptStruct);

	if (UScriptStruct::ICppStructOps* CppStructOps = ScriptStruct->GetCppStructOps())
	{
		CppStructOps->GetPreloadDependencies(&GetMutableStorage(), OutDeps);
	}

	for (TPropertyValueIterator<FStructProperty> It(ScriptStruct, &GetMutableStorage()); It; ++It)
	{
		const UScriptStruct* StructType = It.Key()->Struct;
		if (UScriptStruct::ICppStructOps* CppStructOps = StructType->GetCppStructOps())
		{
			void* StructDataPtr = const_cast<void*>(It.Value());
			CppStructOps->GetPreloadDependencies(StructDataPtr, OutDeps);
		}
	}
}

void FMetaStoryInstanceData::CopyFrom(UObject& InOwner, const FMetaStoryInstanceData& InOther)
{
	if (&InOther == this)
	{
		return;
	}

	FMetaStoryInstanceStorage& Storage = GetMutableStorage();
	const FMetaStoryInstanceStorage& OtherStorage = InOther.GetStorage();

	// Copy structs
	Storage.InstanceStructs = OtherStorage.InstanceStructs;

	// Copy instance objects.
	for (FStructView Instance : Storage.InstanceStructs)
	{
		if (FMetaStoryInstanceObjectWrapper* Wrapper = Instance.GetPtr<FMetaStoryInstanceObjectWrapper>())
		{
			if (Wrapper->InstanceObject)
			{
				constexpr bool bDuplicate = true;
				Wrapper->InstanceObject = UE::MetaStory::InstanceData::Private::CopyNodeInstance(Wrapper->InstanceObject, &InOwner, bDuplicate);
			}
		}
	}
}

void FMetaStoryInstanceData::Init(UObject& InOwner, TConstArrayView<FInstancedStruct> InStructs, FAddArgs Args)
{
	Reset();
	Append(InOwner, InStructs, Args);
}

void FMetaStoryInstanceData::Init(UObject& InOwner, TConstArrayView<FConstStructView> InStructs, FAddArgs Args)
{
	Reset();
	Append(InOwner, InStructs, Args);
}

void FMetaStoryInstanceData::Append(UObject& InOwner, TConstArrayView<FInstancedStruct> InStructs, FAddArgs Args)
{
	UE::MetaStory::InstanceData::Private::AppendToInstanceStructContainer(GetMutableStorage().InstanceStructs, &InOwner, InStructs, Args.bDuplicateWrappedObject);
}

void FMetaStoryInstanceData::Append(UObject& InOwner, TConstArrayView<FConstStructView> InStructs, FAddArgs Args)
{
	UE::MetaStory::InstanceData::Private::AppendToInstanceStructContainer(GetMutableStorage().InstanceStructs, &InOwner, InStructs, Args.bDuplicateWrappedObject);
}

void FMetaStoryInstanceData::Append(UObject& InOwner, TConstArrayView<FConstStructView> InStructs, TConstArrayView<FInstancedStruct*> InInstancesToMove, FAddArgs Args)
{
	check(InStructs.Num() == InInstancesToMove.Num());
	
	FMetaStoryInstanceStorage& Storage = GetMutableStorage();

	const int32 StartIndex = Storage.InstanceStructs.Num();
	Storage.InstanceStructs.Append(InStructs);

	for (int32 Index = StartIndex; Index < Storage.InstanceStructs.Num(); Index++)
	{
		FStructView Struct = Storage.InstanceStructs[Index];
		FInstancedStruct* Source = InInstancesToMove[Index - StartIndex];

		// The source is used to move temporary instance data into instance data. Not all entries may have it.
		// The instance struct can be empty, in which case the temporary instance is ignored.
		// If the source is specified, move it to the instance data.
		// We assume that if the source is object wrapper, it is already the instance we want.
		if (Struct.IsValid())
		{
			if (Source && Source->IsValid())
			{
				check(Struct.GetScriptStruct() == Source->GetScriptStruct());
				
				FMemory::Memswap(Struct.GetMemory(), Source->GetMutableMemory(), Struct.GetScriptStruct()->GetStructureSize());
				Source->Reset();
			}
			else if (FMetaStoryInstanceObjectWrapper* Wrapper = Struct.GetPtr<FMetaStoryInstanceObjectWrapper>())
			{
				if (Wrapper->InstanceObject)
				{
					const bool bDuplicate = Args.bDuplicateWrappedObject || &InOwner != Wrapper->InstanceObject->GetOuter();
					Wrapper->InstanceObject = UE::MetaStory::InstanceData::Private::CopyNodeInstance(Wrapper->InstanceObject, &InOwner, bDuplicate);
				}
			}
		}
	}
}

void FMetaStoryInstanceData::ShrinkTo(const int32 NumStructs)
{
	FMetaStoryInstanceStorage& Storage = GetMutableStorage();
	check(NumStructs <= Storage.InstanceStructs.Num());  
	Storage.InstanceStructs.SetNum(NumStructs);
}

void FMetaStoryInstanceData::Reset()
{
	GetMutableStorage().Reset();

}
