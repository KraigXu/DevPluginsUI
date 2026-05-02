// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryLinker.h"
#include "MetaStory.h"
#include "MetaStorySchema.h"
#include "Templates/Casts.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryLinker)

FMetaStoryLinker::FMetaStoryLinker(TNotNull<const UMetaStory*> InStateTree)
	: MetaStory(InStateTree)
	, Schema(InStateTree->GetSchema())
{
}

void FMetaStoryLinker::LinkExternalData(FMetaStoryExternalDataHandle& Handle, const UStruct* Struct, const EMetaStoryExternalDataRequirement Requirement)
{
	if (Schema != nullptr && !Schema->IsExternalItemAllowed(*Struct))
	{
		UE_LOG(LogMetaStory, Error,
			TEXT("External data of type '%s' used by current node is not allowed by schema '%s' (i.e. rejected by IsExternalItemAllowed)"),
			*Struct->GetName(),
			*Schema->GetClass()->GetName());

		Handle = FMetaStoryExternalDataHandle();
		Status = EMetaStoryLinkerStatus::Failed;
		return;
	}

	const FMetaStoryExternalDataDesc Desc(Struct, Requirement);
	int32 Index = ExternalDataDescs.Find(Desc);

	if (Index == INDEX_NONE)
	{
		Index = ExternalDataDescs.Add(Desc);
		ExternalDataDescs[Index].Handle.DataHandle = FMetaStoryDataHandle(EMetaStoryDataSourceType::ExternalData, Index);
	}

	Handle.DataHandle = ExternalDataDescs[Index].Handle.DataHandle;
}