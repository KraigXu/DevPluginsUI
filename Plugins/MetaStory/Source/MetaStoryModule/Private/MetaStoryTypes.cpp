// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryTypes.h"
#include "MetaStoryEvents.h"
#include "MetaStory.h" // FMetaStoryCustomVersion
#include "MetaStoryCustomVersions.h"
#include "Math/ColorList.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryTypes)

DEFINE_LOG_CATEGORY(LogMetaStory);

namespace UE::MetaStory::Colors
{
	FColor Darken(const FColor Col, const float Level)
	{
		const int32 Mul = (int32)FMath::Clamp(Level * 255.f, 0.f, 255.f);
		const int32 R = (int32)Col.R * Mul / 255;
		const int32 G = (int32)Col.G * Mul / 255;
		const int32 B = (int32)Col.B * Mul / 255;
		return FColor((uint8)R, (uint8)G, (uint8)B, Col.A);
	}

	const FColor Grey = FColor::FromHex(TEXT("#949494"));
	const FColor Red = FColor::FromHex(TEXT("#DE6659"));
	const FColor Orange = FColor::FromHex(TEXT("#E3983F"));
	const FColor Yellow = FColor::FromHex(TEXT("#EFD964"));
	const FColor Green = FColor::FromHex(TEXT("#8AB75E"));
	const FColor Cyan = FColor::FromHex(TEXT("#56C3BD"));
	const FColor Blue = FColor::FromHex(TEXT("#649ED3"));
	const FColor Purple = FColor::FromHex(TEXT("#B397D6"));
	const FColor Magenta = FColor::FromHex(TEXT("#CE85C7"));
	const FColor Bronze = FColorList::Bronze;

	constexpr float DarkenLevel = 0.6f;
	const FColor DarkGrey = Darken(Grey, DarkenLevel);
	const FColor DarkRed = Darken(Red, DarkenLevel);
	const FColor DarkOrange = Darken(Orange, DarkenLevel);
	const FColor DarkYellow = Darken(Yellow, DarkenLevel);
	const FColor DarkGreen = Darken(Green, DarkenLevel);
	const FColor DarkCyan = Darken(Cyan, DarkenLevel);
	const FColor DarkBlue = Darken(Blue, DarkenLevel);
	const FColor DarkPurple = Darken(Purple, DarkenLevel);
	const FColor DarkMagenta = Darken(Magenta, DarkenLevel);
	const FColor DarkBronze = Darken(Bronze, DarkenLevel);
} // UE::MetaStory::Colors


const FMetaStoryStateHandle FMetaStoryStateHandle::Invalid = FMetaStoryStateHandle();
const FMetaStoryStateHandle FMetaStoryStateHandle::Succeeded = FMetaStoryStateHandle(SucceededIndex);
const FMetaStoryStateHandle FMetaStoryStateHandle::Failed = FMetaStoryStateHandle(FailedIndex);
const FMetaStoryStateHandle FMetaStoryStateHandle::Stopped = FMetaStoryStateHandle(StoppedIndex);
const FMetaStoryStateHandle FMetaStoryStateHandle::Root = FMetaStoryStateHandle(0);

const FMetaStoryDataHandle FMetaStoryDataHandle::Invalid = FMetaStoryDataHandle();

const FMetaStoryIndex16 FMetaStoryIndex16::Invalid = FMetaStoryIndex16();
const FMetaStoryIndex8 FMetaStoryIndex8::Invalid = FMetaStoryIndex8();


//////////////////////////////////////////////////////////////////////////
// FMetaStoryStateHandle

EMetaStoryRunStatus FMetaStoryStateHandle::ToCompletionStatus() const
{
	if (Index == SucceededIndex)
	{
		return EMetaStoryRunStatus::Succeeded;
	}

	if (Index == FailedIndex)
	{
		return EMetaStoryRunStatus::Failed;
	}

	if (Index == StoppedIndex)
	{
		return EMetaStoryRunStatus::Stopped;
	}
	return EMetaStoryRunStatus::Unset;
}

FMetaStoryStateHandle FMetaStoryStateHandle::FromCompletionStatus(const EMetaStoryRunStatus Status)
{
	if (Status == EMetaStoryRunStatus::Succeeded)
	{
		return Succeeded;
	}

	if (Status == EMetaStoryRunStatus::Failed)
	{
		return Failed;
	}

	if (Status == EMetaStoryRunStatus::Stopped)
	{
		return Stopped;
	}
	return {};
}

FString FMetaStoryStateHandle::Describe() const
{
	switch (Index)
	{
	case InvalidIndex:
		return TEXT("Invalid");
	case SucceededIndex:
		return TEXT("Succeeded");
	case FailedIndex:
		return TEXT("Failed");
	case StoppedIndex:
		return TEXT("Stopped");
	default:
		return FString::Printf(TEXT("%d"), Index);
	}
}

//////////////////////////////////////////////////////////////////////////
// FMetaStoryStateLink

bool FMetaStoryStateLink::Serialize(FStructuredArchive::FSlot Slot)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const FGuid MetaStoryCustomVersionID = FMetaStoryCustomVersion::GUID;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	Slot.GetUnderlyingArchive().UsingCustomVersion(MetaStoryCustomVersionID);
	return false; // Let the default serializer handle serializing.
}

void FMetaStoryStateLink::PostSerialize(const FArchive& Ar)
{
#if WITH_EDITORONLY_DATA
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const int32 CurrentMetaStoryCustomVersion = UE::MetaStory::CustomVersions::GetEffectiveAssetArchiveVersion(Ar);
	constexpr int32 AddedExternalTransitionsVersion = FMetaStoryCustomVersion::AddedExternalTransitions;

	if (CurrentMetaStoryCustomVersion < AddedExternalTransitionsVersion)
	{
		LinkType = Type_DEPRECATED;
		if (LinkType == EMetaStoryTransitionType::NotSet)
		{
			LinkType = EMetaStoryTransitionType::None;
		}

	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITORONLY_DATA
}

//////////////////////////////////////////////////////////////////////////
// FMetaStoryDataHandle

bool FMetaStoryDataHandle::IsObjectSource() const
{
	return Source == EMetaStoryDataSourceType::GlobalInstanceDataObject
		|| Source == EMetaStoryDataSourceType::ActiveInstanceDataObject
		|| Source == EMetaStoryDataSourceType::SharedInstanceDataObject
		|| Source == EMetaStoryDataSourceType::EvaluationScopeInstanceDataObject
		|| Source == EMetaStoryDataSourceType::ExecutionRuntimeDataObject;
}

FMetaStoryDataHandle FMetaStoryDataHandle::ToObjectSource() const
{
	switch (Source)
	{
	case EMetaStoryDataSourceType::GlobalInstanceData:
		return FMetaStoryDataHandle(EMetaStoryDataSourceType::GlobalInstanceDataObject, Index, StateHandle);
	case EMetaStoryDataSourceType::ActiveInstanceData:
		return FMetaStoryDataHandle(EMetaStoryDataSourceType::ActiveInstanceDataObject, Index, StateHandle);
	case EMetaStoryDataSourceType::SharedInstanceData:
		return FMetaStoryDataHandle(EMetaStoryDataSourceType::SharedInstanceDataObject, Index, StateHandle);
	case EMetaStoryDataSourceType::EvaluationScopeInstanceData:
		return FMetaStoryDataHandle(EMetaStoryDataSourceType::EvaluationScopeInstanceDataObject, Index, StateHandle);
	case EMetaStoryDataSourceType::ExecutionRuntimeData:
		return FMetaStoryDataHandle(EMetaStoryDataSourceType::ExecutionRuntimeDataObject, Index, StateHandle);
	default:
		return *this;
	}
}

FString FMetaStoryDataHandle::Describe() const
{
	switch (Source)
	{
	case EMetaStoryDataSourceType::None:
		return TEXT("None");
	case EMetaStoryDataSourceType::GlobalInstanceData:
		return FString::Printf(TEXT("Global[%d]"), Index);
	case EMetaStoryDataSourceType::GlobalInstanceDataObject:
		return FString::Printf(TEXT("GlobalObj[%d]"), Index);
	case EMetaStoryDataSourceType::ActiveInstanceData:
		return FString::Printf(TEXT("Active[%d]"), Index);
	case EMetaStoryDataSourceType::ActiveInstanceDataObject:
		return FString::Printf(TEXT("ActiveObj[%d]"), Index);
	case EMetaStoryDataSourceType::SharedInstanceData:
		return FString::Printf(TEXT("Shared[%d]"), Index);
	case EMetaStoryDataSourceType::SharedInstanceDataObject:
		return FString::Printf(TEXT("SharedObj[%d]"), Index);
	case EMetaStoryDataSourceType::EvaluationScopeInstanceData:
		return FString::Printf(TEXT("EvalScope[%d]"), Index);
	case EMetaStoryDataSourceType::EvaluationScopeInstanceDataObject:
		return FString::Printf(TEXT("EvalScopeObj[%d]"), Index);
	case EMetaStoryDataSourceType::ExecutionRuntimeData:
		return FString::Printf(TEXT("ExecRun[%d]"), Index);
	case EMetaStoryDataSourceType::ExecutionRuntimeDataObject:
		return FString::Printf(TEXT("ExecRunObj[%d]"), Index);
	case EMetaStoryDataSourceType::ContextData:
		return FString::Printf(TEXT("Context[%d]"), Index);
	case EMetaStoryDataSourceType::GlobalParameterData:
		return TEXT("GlobalParam");
	case EMetaStoryDataSourceType::ExternalGlobalParameterData:
		return TEXT("ExternalGlobalParam");
	case EMetaStoryDataSourceType::SubtreeParameterData:
		return FString::Printf(TEXT("SubtreeParam[%d]"), Index);
	case EMetaStoryDataSourceType::StateParameterData:
		return FString::Printf(TEXT("LinkedParam[%d]"), Index);
	default:
		return TEXT("---");
	}
}

//////////////////////////////////////////////////////////////////////////
// FMetaStoryCompactEventDesc

bool FMetaStoryCompactEventDesc::DoesEventMatchDesc(const FMetaStoryEvent& Event) const
{
	if (Tag.IsValid() && (!Event.Tag.IsValid() || !Event.Tag.MatchesTag(Tag)))
	{
		return false;
	}

	const UScriptStruct* EventPayloadStruct = Event.Payload.GetScriptStruct();
	if (PayloadStruct && (EventPayloadStruct == nullptr || !EventPayloadStruct->IsChildOf(PayloadStruct)))
	{
		return false;
	}

	return true;
}