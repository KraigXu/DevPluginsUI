// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryEditorSchema.h"
#include "MetaStory.h"
#include "MetaStoryEditorData.h"
#include "MetaStoryEditorModule.h"
#include "MetaStorySchema.h"
#include "MetaStoryState.h"
#include "MetaStoryTaskBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryEditorSchema)

void UMetaStoryEditorSchema::Validate(TNotNull<UMetaStory*> MetaStory)
{
	UMetaStoryEditorData* TreeData = Cast<UMetaStoryEditorData>(MetaStory->EditorData);
	if (!TreeData)
	{
		return;
	}

	const UMetaStorySchema* Schema = TreeData->Schema;
	if (!Schema)
	{
		return;
	}

	// Clear evaluators if not allowed.
	if (Schema->AllowEvaluators() == false && TreeData->Evaluators.Num() > 0)
	{
		UE_LOG(LogMetaStoryEditor, Warning, TEXT("%s: Resetting Evaluators due to current schema restrictions."), *MetaStory->GetName());
		TreeData->Evaluators.Reset();
	}

	TreeData->VisitHierarchy(
		[&MetaStory, Schema](UMetaStoryState& State, UMetaStoryState* /*ParentState*/)
		{
			constexpr bool bMarkDirty = false;
			State.Modify(bMarkDirty);

			// Clear enter conditions if not allowed.
			if (Schema->AllowEnterConditions() == false && State.EnterConditions.Num() > 0)
			{
				UE_LOG(LogMetaStoryEditor, Warning, TEXT("%s: Resetting Enter Conditions in state %s due to current schema restrictions."), *MetaStory->GetName(), *State.GetName());
				State.EnterConditions.Reset();
			}

			// Clear Utility if not allowed
			if (Schema->AllowUtilityConsiderations() == false && State.Considerations.Num() > 0)
			{
				UE_LOG(LogMetaStoryEditor, Warning, TEXT("%s: Resetting Utility Considerations in state %s due to current schema restrictions."), *MetaStory->GetName(), *State.GetName());
				State.Considerations.Reset();
			}

			// Keep single and many tasks based on what is allowed.
			if (Schema->AllowMultipleTasks() == false)
			{
				if (State.Tasks.Num() > 0)
				{
					State.Tasks.Reset();
					UE_LOG(LogMetaStoryEditor, Warning, TEXT("%s: Resetting Tasks in state %s due to current schema restrictions."), *MetaStory->GetName(), *State.GetName());
				}

				// Task name is the same as state name.
				if (FMetaStoryTaskBase* Task = State.SingleTask.Node.GetMutablePtr<FMetaStoryTaskBase>())
				{
					Task->Name = State.Name;
				}
			}
			else
			{
				if (State.SingleTask.Node.IsValid())
				{
					State.SingleTask.Reset();
					UE_LOG(LogMetaStoryEditor, Warning, TEXT("%s: Resetting Single Task in state %s due to current schema restrictions."), *MetaStory->GetName(), *State.GetName());
				}
			}

			return EMetaStoryVisitor::Continue;
		});
}
