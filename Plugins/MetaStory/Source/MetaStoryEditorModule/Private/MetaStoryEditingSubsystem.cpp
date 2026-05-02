// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryEditingSubsystem.h"

#include "SMetaStoryView.h"
#include "MetaStoryCompiler.h"
#include "MetaStoryCompilerLog.h"
#include "MetaStoryCompilerManager.h"
#include "MetaStoryDelegates.h"
#include "MetaStoryEditorData.h"
#include "MetaStoryEditorModule.h"
#include "MetaStoryEditorSchema.h"
#include "MetaStoryObjectHash.h"
#include "MetaStoryTaskBase.h"
#include "UObject/UObjectGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryEditingSubsystem)


UMetaStoryEditingSubsystem::UMetaStoryEditingSubsystem()
{
	PostGarbageCollectHandle = FCoreUObjectDelegates::GetPostGarbageCollect().AddUObject(this, &UMetaStoryEditingSubsystem::HandlePostGarbageCollect);
	PostCompileHandle = UE::MetaStory::Delegates::OnPostCompile.AddUObject(this, &UMetaStoryEditingSubsystem::HandlePostCompile);
}

void UMetaStoryEditingSubsystem::BeginDestroy()
{
	UE::MetaStory::Delegates::OnPostCompile.Remove(PostCompileHandle);
	FCoreUObjectDelegates::GetPostGarbageCollect().Remove(PostGarbageCollectHandle);
	Super::BeginDestroy();
}

bool UMetaStoryEditingSubsystem::CompileStateTree(TNotNull<UMetaStory*> InStateTree, FMetaStoryCompilerLog& InOutLog)
{
	return UE::MetaStory::Compiler::FCompilerManager::CompileSynchronously(InStateTree, InOutLog);
}

TSharedPtr<FMetaStoryViewModel> UMetaStoryEditingSubsystem::FindViewModel(TNotNull<const UMetaStory*> InStateTree) const
{
	const FObjectKey MetaStoryKey = InStateTree;
	TSharedPtr<FMetaStoryViewModel> ViewModelPtr = MetaStoryViewModels.FindRef(MetaStoryKey);
	if (ViewModelPtr)
	{
		// The MetaStory could be re-instantiated. Can occur when the object is destroyed and recreated in a pool or when reloaded in editor.
		//The object might have the same pointer value or the same path but it's a new object and all weakptr are now invalid.
		if (ViewModelPtr->GetStateTree() == InStateTree)
		{
			return ViewModelPtr.ToSharedRef();
		}
	}
	return nullptr;
}

TSharedRef<FMetaStoryViewModel> UMetaStoryEditingSubsystem::FindOrAddViewModel(TNotNull<UMetaStory*> InStateTree)
{
	const FObjectKey MetaStoryKey = InStateTree;
	TSharedPtr<FMetaStoryViewModel> ViewModelPtr = MetaStoryViewModels.FindRef(MetaStoryKey);
	if (ViewModelPtr)
	{
		// The MetaStory could be re-instantiated. Can occur when the object is destroyed and recreated in a pool or when reloaded in editor.
		//The object might have the same pointer value or the same path but it's a new object and all weakptr are now invalid.
		if (ViewModelPtr->GetStateTree() == InStateTree)
		{
			return ViewModelPtr.ToSharedRef();
		}
		else
		{
			MetaStoryViewModels.Remove(MetaStoryKey);
			ViewModelPtr = nullptr;
		}
	}

	ValidateStateTree(InStateTree);

	TSharedRef<FMetaStoryViewModel> SharedModel = MetaStoryViewModels.Add(MetaStoryKey, MakeShared<FMetaStoryViewModel>()).ToSharedRef();
	UMetaStoryEditorData* EditorData = Cast<UMetaStoryEditorData>(InStateTree->EditorData);
	SharedModel->Init(EditorData);

	return SharedModel;
}

TSharedRef<SWidget> UMetaStoryEditingSubsystem::GetStateTreeView(TSharedRef<FMetaStoryViewModel> InViewModel, const TSharedRef<FUICommandList>& TreeViewCommandList)
{
	return SNew(SMetaStoryView, InViewModel, TreeViewCommandList);
}

void UMetaStoryEditingSubsystem::ValidateStateTree(TNotNull<UMetaStory*> InStateTree)
{
	auto FixChangedStateLinkName = [](FMetaStoryStateLink& StateLink, const TMap<FGuid, FName>& IDToName) -> bool
		{
			if (StateLink.ID.IsValid())
			{
				const FName* Name = IDToName.Find(StateLink.ID);
				if (Name == nullptr)
				{
					// Missing link, we'll show these in the UI
					return false;
				}
				if (StateLink.Name != *Name)
				{
					// Name changed, fix!
					StateLink.Name = *Name;
					return true;
				}
			}
			return false;
		};

	auto ValidateLinkedStates = [&FixChangedStateLinkName](TNotNull<UMetaStoryEditorData*> TreeData)
		{
			// Make sure all state links are valid and update the names if needed.

			// Create ID to state name map.
			TMap<FGuid, FName> IDToName;

			TreeData->VisitHierarchy([&IDToName](const UMetaStoryState& State, UMetaStoryState* /*ParentState*/)
			{
				IDToName.Add(State.ID, State.Name);
				return EMetaStoryVisitor::Continue;
			});
		
			// Fix changed names.
			TreeData->VisitHierarchy([&IDToName, FixChangedStateLinkName](UMetaStoryState& State, UMetaStoryState* /*ParentState*/)
			{
				constexpr bool bMarkDirty = false;
				State.Modify(bMarkDirty);
				if (State.Type == EMetaStoryStateType::Linked)
				{
					FixChangedStateLinkName(State.LinkedSubtree, IDToName);
				}
					
				for (FMetaStoryTransition& Transition : State.Transitions)
				{
					FixChangedStateLinkName(Transition.State, IDToName);
				}

				return EMetaStoryVisitor::Continue;
			});
		};

	auto FixEditorData = [](TNotNull<UMetaStory*> MetaStory)
	{
		UMetaStoryEditorData* EditorData = Cast<UMetaStoryEditorData>(MetaStory->EditorData);
		// The schema is defined in the EditorData. If we can't find the editor data (probably because the class doesn't exist anymore), then try the compiled schema in the state tree asset.
		TSubclassOf<const UMetaStorySchema> SchemaClass;
		if (EditorData && EditorData->Schema)
		{
			SchemaClass = EditorData->Schema->GetClass();
		}
		else if (MetaStory->GetSchema())
		{
			SchemaClass = MetaStory->GetSchema()->GetClass();
		}

		if (SchemaClass.Get() == nullptr)
		{
			UE_LOG(LogMetaStoryEditor, Error, TEXT("The state tree '%s' does not have a schema."), *MetaStory->GetPathName());
			return;
		}

		TNonNullSubclassOf<UMetaStoryEditorData> EditorDataClass = FMetaStoryEditorModule::GetModule().GetEditorDataClass(SchemaClass.Get());
		if (EditorData == nullptr)
		{
			EditorData = NewObject<UMetaStoryEditorData>(MetaStory, EditorDataClass.Get(), FName(), RF_Transactional);
			EditorData->AddRootState();
			EditorData->Schema = NewObject<UMetaStorySchema>(EditorData, SchemaClass.Get());
			EditorData->EditorBindings.SetBindingsOwner(EditorData);

			constexpr bool bMarkDirty = false;
			MetaStory->Modify(bMarkDirty);
			MetaStory->EditorData = EditorData;
		}
		else if (!EditorData->IsA(EditorDataClass.Get()))
		{
			// The current EditorData is not of the correct type. The data needs to be patched by the schema desired editor data subclass.
			UMetaStoryEditorData* PreviousEditorData = EditorData;
			EditorData = CastChecked<UMetaStoryEditorData>(StaticDuplicateObject(EditorData, MetaStory, FName(), RF_Transactional, EditorDataClass.Get()));
			if (EditorData->SubTrees.Num() == 0)
			{
				EditorData->AddRootState();
			}
			if (EditorData->Schema == nullptr || !EditorData->Schema->IsA(SchemaClass.Get()))
			{
				EditorData->Schema = NewObject<UMetaStorySchema>(EditorData, SchemaClass.Get());
			}
			EditorData->EditorBindings.SetBindingsOwner(EditorData);

			constexpr bool bMarkDirty = false;
			MetaStory->Modify(bMarkDirty);
			MetaStory->EditorData = EditorData;

			// Trash the previous EditorData
			const FName TrashStateTreeName = MakeUniqueObjectName(GetTransientPackage(), UMetaStory::StaticClass(), *FString::Printf(TEXT("TRASH_%s"), *UMetaStory::StaticClass()->GetName()));
			UMetaStory* TransientOuter = NewObject<UMetaStory>(GetTransientPackage(), TrashStateTreeName, RF_Transient);
			const FName TrashSchemaName = *FString::Printf(TEXT("TRASH_%s"), *PreviousEditorData->GetName());
			constexpr ERenameFlags RenameFlags = REN_DoNotDirty | REN_DontCreateRedirectors;
			PreviousEditorData->Rename(*TrashSchemaName.ToString(), TransientOuter, RenameFlags);
			PreviousEditorData->SetFlags(RF_Transient);
		}
	};

	auto FixEditorSchema = [](TNotNull<UMetaStoryEditorData*> EditorData)
		{
			TSubclassOf<const UMetaStorySchema> SchemaClass = EditorData->Schema ? EditorData->Schema->GetClass() : nullptr;
			if (SchemaClass.Get() == nullptr)
			{
				return;
			}

			TNonNullSubclassOf<UMetaStoryEditorSchema> EditorSchemaClass = FMetaStoryEditorModule::GetModule().GetEditorSchemaClass(SchemaClass.Get());
			if (EditorData->EditorSchema == nullptr)
			{
				EditorData->EditorSchema = NewObject<UMetaStoryEditorSchema>(EditorData, EditorSchemaClass.Get(), FName(), RF_Transactional);
			}
			else if (!EditorData->EditorSchema->IsA(EditorSchemaClass.Get()))
			{
				// The current EditorSchema is not of the correct type. The data needs to be patched by the schema desired editor data subclass.
				UMetaStoryEditorSchema* PreviousEditorSchema = EditorData->EditorSchema;
				EditorData->EditorSchema = CastChecked<UMetaStoryEditorSchema>(StaticDuplicateObject(PreviousEditorSchema, EditorData, FName(), RF_Transactional, EditorSchemaClass.Get()));

				// Trash the previous EditorData
				const FName TrashName = MakeUniqueObjectName(GetTransientPackage(), UMetaStoryEditorSchema::StaticClass(), *FString::Printf(TEXT("TRASH_%s"), *PreviousEditorSchema->GetName()));
				constexpr ERenameFlags RenameFlags = REN_DoNotDirty | REN_DontCreateRedirectors;
				PreviousEditorSchema->Rename(*TrashName.ToString(), PreviousEditorSchema, RenameFlags);
				PreviousEditorSchema->SetFlags(RF_Transient);
			}
		};

	auto RemoveUnusedBindings = [](TNotNull<UMetaStoryEditorData*> EditorData)
		{
			TMap<FGuid, const FPropertyBindingDataView> AllStructValues;
			EditorData->GetAllStructValues(AllStructValues);
			EditorData->GetPropertyEditorBindings()->RemoveInvalidBindings(AllStructValues);
		};

	auto UpdateLinkedStateParameters = [](TNotNull<UMetaStoryEditorData*> TreeData)
		{
			const EMetaStoryVisitor Result = TreeData->VisitHierarchy([](UMetaStoryState& State, UMetaStoryState* /*ParentState*/)
			{
				if (State.Type == EMetaStoryStateType::Linked
					|| State.Type == EMetaStoryStateType::LinkedAsset)
				{
					constexpr bool bMarkDirty = false;
					State.Modify(bMarkDirty);
					State.UpdateParametersFromLinkedSubtree();
				}
				return EMetaStoryVisitor::Continue;
			});
		};

	auto UpdateTransactionalFlags = [](TNotNull<UMetaStoryEditorData*> EditorData)
		{
			for (UMetaStoryState* SubTree : EditorData->SubTrees)
			{
				TArray<UMetaStoryState*> Stack;

				Stack.Add(SubTree);
				while (!Stack.IsEmpty())
				{
					if (UMetaStoryState* State = Stack.Pop())
					{
						State->SetFlags(RF_Transactional);

						for (UMetaStoryState* ChildState : State->Children)
						{
							Stack.Add(ChildState);
						}
					}
				}
			}
		};

	FixEditorData(InStateTree);
	if (InStateTree->EditorData)
	{
		constexpr bool bMarkDirty = false;
		InStateTree->EditorData->Modify(bMarkDirty);

		UMetaStoryEditorData* EditorData = CastChecked<UMetaStoryEditorData>(InStateTree->EditorData);
		FixEditorSchema(EditorData);

		EditorData->ReparentStates();
		if (EditorData->EditorSchema)
		{
			EditorData->EditorSchema->Validate(InStateTree);
		}

		RemoveUnusedBindings(EditorData);
		ValidateLinkedStates(EditorData);
		UpdateLinkedStateParameters(EditorData);
		UpdateTransactionalFlags(EditorData);
	}
}

uint32 UMetaStoryEditingSubsystem::CalculateStateTreeHash(TNotNull<const UMetaStory*> InStateTree)
{
	uint32 EditorDataHash = 0;
	if (InStateTree->EditorData != nullptr)
	{
		FMetaStoryObjectCRC32 Archive;
		EditorDataHash = Archive.Crc32(InStateTree->EditorData, 0);
	}

	return EditorDataHash;
}

void UMetaStoryEditingSubsystem::HandlePostGarbageCollect()
{
	// Remove the stale viewmodels
	for (TMap<FObjectKey, TSharedPtr<FMetaStoryViewModel>>::TIterator It(MetaStoryViewModels); It; ++It)
	{
		if (!It.Key().ResolveObjectPtr())
		{
			It.RemoveCurrent();
		}
		else if (!It.Value() || !It.Value()->GetStateTree())
		{
			It.RemoveCurrent();
		}
	}
}

void UMetaStoryEditingSubsystem::HandlePostCompile(const UMetaStory& InStateTree)
{
	// Notify the UI that something changed. Make sure to not request a new viewmodel. That way, we are not creating new viewmodel when cooking/PIE.
	if (TSharedPtr<FMetaStoryViewModel> ViewModel = FindViewModel(&InStateTree))
	{
		ViewModel->NotifyAssetChangedExternally();
	}
}
