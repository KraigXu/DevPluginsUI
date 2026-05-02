// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryCompiler.h"
#include "MetaStory.h"
#include "MetaStoryAnyEnum.h"
#include "MetaStoryCompilerLog.h"
#include "MetaStoryEditorData.h"
#include "MetaStoryEditorDataExtension.h"
#include "MetaStoryEditorSchema.h"
#include "MetaStoryExtension.h"
#include "MetaStoryEditorModule.h"
#include "MetaStoryEvaluatorBase.h"
#include "MetaStoryTaskBase.h"
#include "MetaStoryConditionBase.h"
#include "MetaStoryConsiderationBase.h"
#include "Serialization/ArchiveUObject.h"
#include "GameFramework/Actor.h"
#include "MetaStoryPropertyRef.h"
#include "MetaStoryPropertyRefHelpers.h"
#include "MetaStoryPropertyHelpers.h"
#include "MetaStoryDelegate.h"
#include "Customizations/MetaStoryEditorNodeUtils.h"

namespace UE::MetaStory
{
	struct FCompileNodeContext : ICompileNodeContext
	{
		explicit FCompileNodeContext(const FMetaStoryDataView& InDataView, const FMetaStoryBindableStructDesc& InDesc, const IMetaStoryBindingLookup& InBindingLookup)
			: InstanceDataView(InDataView),
			Desc(InDesc),
			BindingLookup(InBindingLookup)
		{
		}

		virtual void AddValidationError(const FText& Message) override
		{
			ValidationErrors.Add(Message);
		}

		virtual FMetaStoryDataView GetInstanceDataView() const override
		{
			return InstanceDataView;
		}

		virtual bool HasBindingForProperty(const FName PropertyName) const override
		{
			const FPropertyBindingPath& PropertyPath = FPropertyBindingPath(Desc.ID, PropertyName);

			return BindingLookup.GetPropertyBindingSource(PropertyPath) != nullptr;
 		}

		TArray<FText> ValidationErrors;
		FMetaStoryDataView InstanceDataView;
		const FMetaStoryBindableStructDesc& Desc;
		const IMetaStoryBindingLookup& BindingLookup;
	};
}


namespace UE::MetaStory::Compiler
{
	FAutoConsoleVariable CVarLogEnableParameterDelegateDispatcherBinding(
		TEXT("MetaStory.Compiler.EnableParameterDelegateDispatcherBinding"),
		false,
		TEXT("Enable binding from delegate dispatchers that are in the state tree parameters.")
	);

	bool bEnablePropertyFunctionWithEvaluationScopeInstanceData = true;
	FAutoConsoleVariableRef CVarEnablePropertyFunctionWithEvaluationScopeInstanceData(
		TEXT("MetaStory.Compiler.EnablePropertyFunctionWithEvaluationScopeInstanceData"),
		bEnablePropertyFunctionWithEvaluationScopeInstanceData,
		TEXT("Use EvaluationScope data for property functions instead of SharedInstance data.\n")
		TEXT("SharedInstance is the previous behavior that is deprecated.")
	);

	bool bEnableConditionWithEvaluationScopeInstanceData = false;
	FAutoConsoleVariableRef CVarEnableConditionWithEvaluationScopeInstanceData(
		TEXT("MetaStory.Compiler.EnableConditionWithEvaluationScopeInstanceData"),
		bEnableConditionWithEvaluationScopeInstanceData,
		TEXT("Use EvaluationScope data for conditions instead of SharedInstance data.")
	);

	bool bEnableUtilityConsiderationWithEvaluationScopeInstanceData = false;
	FAutoConsoleVariableRef CVarEnableUtilityConsiderationWithEvaluationScopeInstanceData(
		TEXT("MetaStory.Compiler.EnableUtilityConsiderationWithEvaluationScopeInstanceData"),
		bEnableUtilityConsiderationWithEvaluationScopeInstanceData,
		TEXT("Use EvaluationScope data for utility considerations instead of SharedInstance data.")
	);

	FAutoConsoleVariable CVarLogCompiledStateTree(
		TEXT("MetaStory.Compiler.LogResultOnCompilationCompleted"),
		false,
		TEXT("After a MetaStory compiles, log the internal content of the MetaStory.")
	);

	// Helper archive that checks that the all instanced sub-objects have correct outer. 
	class FCheckOutersArchive : public FArchiveUObject
	{
		using Super = FArchiveUObject;
		const UMetaStory& MetaStory;
		const UMetaStoryEditorData& EditorData;
		FMetaStoryCompilerLog& Log;
	public:

		FCheckOutersArchive(const UMetaStory& InStateTree, const UMetaStoryEditorData& InEditorData, FMetaStoryCompilerLog& InLog)
			: MetaStory(InStateTree)
			, EditorData(InEditorData)
			, Log(InLog)
		{
			Super::SetIsSaving(true);
			Super::SetIsPersistent(true);
		}

		virtual bool ShouldSkipProperty(const FProperty* InProperty) const
		{
			// Skip editor data.
			if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(InProperty))
			{
				if (ObjectProperty->PropertyClass == UMetaStoryEditorData::StaticClass())
				{
					return true;
				}
			}
			return false;
		}

		virtual FArchive& operator<<(UObject*& Object) override
		{
			if (Object)
			{
				if (const FProperty* Property = GetSerializedProperty())
				{
					if (Property->HasAnyPropertyFlags(CPF_InstancedReference))
					{
						if (!Object->IsInOuter(&MetaStory))
						{
							Log.Reportf(EMessageSeverity::Error, TEXT("Compiled MetaStory contains instanced object %s (%s), which does not belong to the MetaStory. This is due to error in the State Tree node implementation."),
								*GetFullNameSafe(Object), *GetFullNameSafe(Object->GetClass()));
						}

						if (Object->IsInOuter(&EditorData))
						{
							Log.Reportf(EMessageSeverity::Error, TEXT("Compiled MetaStory contains instanced object %s (%s), which still belongs to the Editor data. This is due to error in the State Tree node implementation."),
								*GetFullNameSafe(Object), *GetFullNameSafe(Object->GetClass()));
						}
					}
				}
			}
			return *this;
		}
	};

	enum class EPropertyVisitorResult : uint8
	{
		Continue,
		Break
	};

	void ScanProperties(FMetaStoryDataView Data, TFunctionRef<EPropertyVisitorResult(const FProperty* InProperty, const void* InAddress)> InFunc)
	{
		TSet<const UObject*> Visited;
		auto ScanPropertiesRecursive = [&Visited, InFunc](auto&& Self, FMetaStoryDataView CurrentData)
			{
				if (!CurrentData.IsValid())
				{
					return;
				}

				for (TPropertyValueIterator<FProperty> It(CurrentData.GetStruct(), CurrentData.GetMemory()); It; ++It)
				{
					const FProperty* Property = It->Key;
					const void* Address = It->Value;

					if (!Address)
					{
						continue;
					}

					if (InFunc(Property, Address) == EPropertyVisitorResult::Break)
					{
						break;
					}

					if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
					{
						if (const UObject* Object = ObjectProperty->GetObjectPropertyValue(Address))
						{
							// Recurse into instanced object
							if (Property->HasAnyPropertyFlags(CPF_InstancedReference))
							{
								if (!Visited.Contains(Object))
								{
									Visited.Add(Object);
									Self(Self, FMetaStoryDataView(const_cast<UObject*>(Object)));
								}
							}
						}
					}
				}
			};

		ScanPropertiesRecursive(ScanPropertiesRecursive, Data);
	}

	/** Scans Data for actors that are tied to some level and returns them. */
	void ScanLevelActorReferences(FMetaStoryDataView Data, TArray<const AActor*>& OutActors)
	{
		ScanProperties(Data, [&OutActors](const FProperty* InProperty, const void* InAddress)
		{
			if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(InProperty))
			{
				if (const UObject* Object = ObjectProperty->GetObjectPropertyValue(InAddress))
				{
					if (const AActor* Actor = Cast<AActor>(Object))
					{
						if (const ULevel* Level = Actor->GetLevel())
						{
							OutActors.Add(Actor);
						}
					}
				}
			}

			return EPropertyVisitorResult::Continue;
			});
	}

	bool ValidateNoLevelActorReferences(FMetaStoryCompilerLog& Log, const FMetaStoryBindableStructDesc& NodeDesc, const FMetaStoryDataView NodeView, const FMetaStoryDataView InstanceView)
	{
		TArray<const AActor*> LevelActors;
		ScanLevelActorReferences(NodeView, LevelActors);
		ScanLevelActorReferences(InstanceView, LevelActors);
		if (!LevelActors.IsEmpty())
		{
			FStringBuilderBase AllActorsString;
			for (const AActor* Actor : LevelActors)
			{
				if (AllActorsString.Len() > 0)
				{
					AllActorsString += TEXT(", ");
				}
				AllActorsString += *GetNameSafe(Actor);
			}
			Log.Reportf(EMessageSeverity::Error, NodeDesc,
				TEXT("Level Actor references were found: %s. Direct Actor references are not allowed."),
					*AllActorsString);
			return false;
		}
		
		return true;
	}

	template<typename... T>
	bool StructHasAnyStructProperties(FConstStructView StructView)
	{
		bool bResult = false;
		ScanProperties(FMetaStoryDataView(StructView.GetScriptStruct(), const_cast<uint8*>(StructView.GetMemory())), 
			[&bResult](const FProperty* InProperty, const void* InAddress)
			{
				if (const FStructProperty* StructProperty = CastField<FStructProperty>(InProperty))
				{
					if ((... || StructProperty->Struct->IsChildOf<T>()))
					{
						bResult = true;
						return EPropertyVisitorResult::Break;
					}
				}

				return EPropertyVisitorResult::Continue;
			});

		return bResult;
	}

	template<typename... T>
	bool IsPropertyChildOfAnyStruct(const FMetaStoryBindableStructDesc& Struct, const FPropertyBindingPath& Path)
	{
		TArray<FPropertyBindingPathIndirection> Indirection;
		const bool bResolved = Path.ResolveIndirections(Struct.Struct, Indirection);

		if (bResolved && Indirection.Num() > 0)
		{
			const FProperty* Property = Indirection.Last().GetProperty();
			check(Property);

			if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				return StructProperty->Struct && (... || StructProperty->Struct->IsChildOf<T>());
			}
		}

		return false;
	}

	bool IsNodeStructEligibleForBinding(FConstStructView NodeView)
	{
		check(NodeView.GetScriptStruct() && NodeView.GetScriptStruct()->IsChildOf<FMetaStoryNodeBase>());

		return StructHasAnyStructProperties<FMetaStoryPropertyRef, FMetaStoryDelegateDispatcher, FMetaStoryDelegateListener>(NodeView);
	}

	void FValidationResult::Log(FMetaStoryCompilerLog& Log, const TCHAR* ContextText, const FMetaStoryBindableStructDesc& ContextStruct) const
	{
		Log.Reportf(EMessageSeverity::Error, ContextStruct, TEXT("The MetaStory is too complex. Compact index %s out of range %d/%d."), ContextText, Value, MaxValue);
	}

	const UScriptStruct* GetBaseStructFromMetaData(const FProperty* Property, FString& OutBaseStructName)
	{
		static const FName NAME_BaseStruct = "BaseStruct";

		const UScriptStruct* Result = nullptr;
		OutBaseStructName = Property->GetMetaData(NAME_BaseStruct);
	
		if (!OutBaseStructName.IsEmpty())
		{
			Result = UClass::TryFindTypeSlow<UScriptStruct>(OutBaseStructName);
			if (!Result)
			{
				Result = LoadObject<UScriptStruct>(nullptr, *OutBaseStructName);
			}
		}

		return Result;
	}

	UObject* DuplicateInstanceObject(FMetaStoryCompilerLog& Log, const FMetaStoryBindableStructDesc& NodeDesc, FGuid NodeID, TNotNull<const UObject*> InstanceObject, TNotNull<UObject*> Owner)
	{
		if (InstanceObject->GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists))
		{
			const UMetaStory* OuterStateTree = Owner->GetTypedOuter<UMetaStory>();
			Log.Reportf(EMessageSeverity::Warning, NodeDesc,
				TEXT("Duplicating '%s' with an old class '%s' Please resave State Tree asset '%s'."),
				*InstanceObject->GetName(), *InstanceObject->GetClass()->GetName(), *GetFullNameSafe(OuterStateTree));
		}

		// We want the object name to match between compilations.
		//Use the class name and increase the counter internally. We do that to not be influenced by another object in a different outer.
		//The objects from a previous compilation are rename in UMetaStory::ResetCompiled.
		FName NewObjectName = InstanceObject->GetClass()->GetFName();
		while (StaticFindObjectFastInternal(nullptr, Owner, NewObjectName, EFindObjectFlags::ExactClass) != nullptr)
		{
			NewObjectName.SetNumber(NewObjectName.GetNumber() + 1);
		}
		return ::DuplicateObject(&(*InstanceObject), &(*Owner), NewObjectName);
	}

	struct FCompletionTasksMaskResult
	{
		FMetaStoryTasksCompletionStatus::FMaskType Mask;
		int32 MaskBufferIndex;	// Index of FMetaStoryTasksCompletionStatus::Buffer.
		int32 MaskFirstTaskBitOffset; // Inside FMetaStoryTasksCompletionStatus::Buffer[MaskBufferIndex], the bit offset of the first task.
		int32 FullMaskEndTaskBitOffset; // the next bit that the next child can take in the full FMetaStoryTasksCompletionStatus::Buffer.
	};

	/** Makes the completion mask for the state or frame. */
	FCompletionTasksMaskResult MakeCompletionTasksMask(int32 FullStartBitIndex, TConstArrayView<FMetaStoryEditorNode> AllTasks, TConstArrayView<int32> ValidTasks)
	{
		FMetaStoryTasksCompletionStatus::FMaskType Mask = 0;
		int32 NumberOfBitsNeeded = 0;
		const int32 NumberOfTasks = ValidTasks.Num();

		// No task, state/frame needs at least one flag to set the state itself completes (ie. for linked state).
		//Each state will take at least 1 bit.
		if (NumberOfTasks == 0)
		{
			Mask = 1;
			NumberOfBitsNeeded = 1;
		}
		else
		{
			for (int32 Index = NumberOfTasks-1; Index >= 0; --Index)
			{
				const int32 TaskIndex = ValidTasks[Index];
				Mask <<= 1;
				if (UE::MetaStoryEditor::EditorNodeUtils::IsTaskEnabled(AllTasks[TaskIndex])
					&& UE::MetaStoryEditor::EditorNodeUtils::IsTaskConsideredForCompletion(AllTasks[TaskIndex]))
				{
					Mask |= 1;
				}
			}
			NumberOfBitsNeeded = NumberOfTasks;
		}

		constexpr int32 NumberOfBitsPerMask = sizeof(FMetaStoryTasksCompletionStatus::FMaskType) * 8;

		// Is the new amount of bits bring up over the next buffer?
		const int32 CurrentEndBitIndex = FullStartBitIndex + NumberOfBitsNeeded;
		const int32 NewMaskBufferIndex = (CurrentEndBitIndex - 1) / NumberOfBitsPerMask;
		if (NewMaskBufferIndex != FullStartBitIndex / NumberOfBitsPerMask)
		{
			// Do not shift the mask. Use the next int32
			const int32 NewMaskFirstTaskBitOffset = 0;
			const int32 NewMaskEndTaskBitOffset = (NewMaskBufferIndex * NumberOfBitsPerMask) + NumberOfBitsNeeded;
			return { .Mask = Mask, .MaskBufferIndex = NewMaskBufferIndex, .MaskFirstTaskBitOffset = NewMaskFirstTaskBitOffset, .FullMaskEndTaskBitOffset = NewMaskEndTaskBitOffset };
		}
		else
		{
			const int32 NewMaskFirstTaskBitOffset = FullStartBitIndex % NumberOfBitsPerMask;
			const int32 NewMaskEndTaskBitOffset = CurrentEndBitIndex;

			Mask <<= NewMaskFirstTaskBitOffset;

			return { .Mask = Mask, .MaskBufferIndex = NewMaskBufferIndex, .MaskFirstTaskBitOffset = NewMaskFirstTaskBitOffset, .FullMaskEndTaskBitOffset = NewMaskEndTaskBitOffset };
		}
	}

}; // UE::MetaStory::Compiler

bool FMetaStoryCompiler::Compile(UMetaStory& InStateTree)
{
	return Compile(&InStateTree);
}

bool FMetaStoryCompiler::Compile(TNotNull<UMetaStory*> InStateTree)
{
	if (bCompiled)
	{
		Log.Reportf(EMessageSeverity::Error, TEXT("Internal error. The compiler has already been executed. Create a new compiler instance."));
		return false;
	}
	bCompiled = true;

	MetaStory = InStateTree;
	EditorData = Cast<UMetaStoryEditorData>(MetaStory->EditorData);

	auto FailCompilation = [MetaStory=MetaStory]()
		{
			MetaStory->ResetCompiled();
			return false;
		};

	if (!EditorData)
	{
		return FailCompilation();
	}
	
	// Cleanup existing state
	MetaStory->ResetCompiled();

	if (!EditorData->Schema)
	{
		Log.Reportf(EMessageSeverity::Error, TEXT("Missing Schema. Please set valid schema in the State Tree Asset settings."));
		return FailCompilation();
	}

	if (!BindingsCompiler.Init(MetaStory->PropertyBindings, Log))
	{
		return FailCompilation();
	}

	EditorData->GetAllStructValues(IDToStructValue);

	// Copy schema the EditorData
	MetaStory->Schema = DuplicateObject(EditorData->Schema, MetaStory);

	if (!CreateParameters())
	{
		return FailCompilation();
	}

	int32 ContextDataIndex = 0;

	// Mark all named external values as binding source
	if (MetaStory->Schema)
	{
		MetaStory->ContextDataDescs = MetaStory->Schema->GetContextDataDescs();
		for (FMetaStoryExternalDataDesc& Desc : MetaStory->ContextDataDescs)
		{
			if (Desc.Struct)
			{
				const FMetaStoryBindableStructDesc ExtDataDesc = {
					UE::MetaStory::Editor::GlobalStateName,
					Desc.Name,
					Desc.Struct,
					FMetaStoryDataHandle(EMetaStoryDataSourceType::ContextData, ContextDataIndex++),
					EMetaStoryBindableStructSource::Context,
					Desc.ID
				};
				BindingsCompiler.AddSourceStruct(ExtDataDesc);
				if (const auto Validation = UE::MetaStory::Compiler::IsValidIndex16(ContextDataIndex); Validation.DidFail())
				{
					Validation.Log(Log, TEXT("ExternalStructIndex"), ExtDataDesc);
					return FailCompilation();
				}
				Desc.Handle.DataHandle = ExtDataDesc.DataHandle;
			}
		}
	}

	if (const UE::MetaStory::Compiler::FValidationResult Validation = UE::MetaStory::Compiler::IsValidIndex16(ContextDataIndex); Validation.DidFail())
	{
		Validation.Log(Log, TEXT("NumContextData"));
		return FailCompilation();
	}
	MetaStory->NumContextData = static_cast<uint16>(ContextDataIndex);

	if (!CreateStates())
	{
		return FailCompilation();
	}

	// Eval and Global task methods use InstanceStructs.Num() as ID generator.
	check(InstanceStructs.Num() == 0);
	
	if (!CreateEvaluators())
	{
		return FailCompilation();
	}

	if (!CreateGlobalTasks())
	{
		return FailCompilation();
	}

	const int32 NumGlobalInstanceData = InstanceStructs.Num();
	if (const auto Validation = UE::MetaStory::Compiler::IsValidIndex16(NumGlobalInstanceData); Validation.DidFail())
	{
		Validation.Log(Log, TEXT("NumGlobalInstanceData"));
		return FailCompilation();
	}
	MetaStory->NumGlobalInstanceData = uint16(NumGlobalInstanceData);

	if (!CreateStateTasksAndParameters())
	{
		return FailCompilation();
	}

	if (!CreateStateTransitions())
	{
		return FailCompilation();
	}

	if (!CreateStateConsiderations())
	{
		return FailCompilation();
	}

	MetaStory->Nodes = Nodes;
	MetaStory->DefaultInstanceData.Init(*MetaStory, InstanceStructs, FMetaStoryInstanceData::FAddArgs{ .bDuplicateWrappedObject = false });
	MetaStory->SharedInstanceData.Init(*MetaStory, SharedInstanceStructs, FMetaStoryInstanceData::FAddArgs{ .bDuplicateWrappedObject = false });
	MetaStory->DefaultEvaluationScopeInstanceData.Init(MetaStory, EvaluationScopeStructs, UE::MetaStory::InstanceData::FMetaStoryInstanceContainer::FAddArgs{ .bDuplicateWrappedObject = false });
	MetaStory->DefaultExecutionRuntimeData.Init(MetaStory, ExecutionRuntimeStructs, UE::MetaStory::InstanceData::FMetaStoryInstanceContainer::FAddArgs{ .bDuplicateWrappedObject = false });

	// Store the new compiled dispatchers.
	EditorData->CompiledDispatchers = BindingsCompiler.GetCompiledDelegateDispatchers();

	BindingsCompiler.Finalize();

	if (!MetaStory->Link())
	{
		Log.Reportf(EMessageSeverity::Error, TEXT("Unexpected failure to link the MetaStory asset. See log for more info."));
		return FailCompilation();
	}

	// Store mapping between node unique ID and their compiled index. Used for debugging purposes.
	for (const TPair<FGuid, int32>& ToNode : IDToNode)
	{
		MetaStory->IDToNodeMappings.Emplace(ToNode.Key, FMetaStoryIndex16(ToNode.Value));
	}

	// Store mapping between state unique ID and state handle. Used for debugging purposes.
	for (const TPair<FGuid, int32>& ToState : IDToState)
	{
		MetaStory->IDToStateMappings.Emplace(ToState.Key, FMetaStoryStateHandle(ToState.Value));
	}

	// Store mapping between state transition identifier and compact transition index. Used for debugging purposes.
	for (const TPair<FGuid, int32>& ToTransition: IDToTransition)
	{
		MetaStory->IDToTransitionMappings.Emplace(ToTransition.Key, FMetaStoryIndex16(ToTransition.Value));
	}

	if (!NotifyInternalPost())
	{
		return FailCompilation();
	}

	UE::MetaStory::Compiler::FCheckOutersArchive CheckOuters(*MetaStory, *EditorData, Log);
	MetaStory->Serialize(CheckOuters);

	if (UE::MetaStory::Compiler::CVarLogCompiledStateTree->GetBool())
	{
		UE_LOG(LogMetaStoryEditor, Log, TEXT("%s"), *MetaStory->DebugInternalLayoutAsString());
	}

	return true;
}

FMetaStoryStateHandle FMetaStoryCompiler::GetStateHandle(const FGuid& StateID) const
{
	const int32* Idx = IDToState.Find(StateID);
	if (Idx == nullptr)
	{
		return FMetaStoryStateHandle::Invalid;
	}

	return FMetaStoryStateHandle(uint16(*Idx));
}

UMetaStoryState* FMetaStoryCompiler::GetState(const FGuid& StateID) const
{
	const int32* Idx = IDToState.Find(StateID);
	if (Idx == nullptr)
	{
		return nullptr;
	}

	return SourceStates[*Idx];
}

bool FMetaStoryCompiler::CreateParameters()
{
	// Copy parameters from EditorData	
	MetaStory->Parameters = EditorData->GetRootParametersPropertyBag();
	MetaStory->ParameterDataType = EditorData->Schema->GetGlobalParameterDataType();

	// Mark parameters as binding source
	const EMetaStoryDataSourceType GlobalParameterDataType = UE::MetaStory::CastToDataSourceType(MetaStory->ParameterDataType);
	const FMetaStoryBindableStructDesc ParametersDesc = {
			UE::MetaStory::Editor::GlobalStateName,
			TEXT("Parameters"),
			MetaStory->Parameters.GetPropertyBagStruct(),
			FMetaStoryDataHandle(GlobalParameterDataType),
			EMetaStoryBindableStructSource::Parameter,
			EditorData->GetRootParametersGuid()
	};
	BindingsCompiler.AddSourceStruct(ParametersDesc);

	const FMetaStoryDataView PropertyBagView(EditorData->GetRootParametersPropertyBag().GetPropertyBagStruct(), (uint8*)EditorData->GetRootParametersPropertyBag().GetValue().GetMemory());
	if (!UE::MetaStory::Compiler::ValidateNoLevelActorReferences(Log, ParametersDesc, FMetaStoryDataView(), PropertyBagView))
	{
		return false;
	}

	// Compile the delegate dispatcher.
	if (UE::MetaStory::Compiler::CVarLogEnableParameterDelegateDispatcherBinding->GetBool())
	{
		FValidatedPathBindings Bindings;
		FMetaStoryDataView SourceValue(MetaStory->Parameters.GetMutableValue());
		if (!GetAndValidateBindings(ParametersDesc, SourceValue, Bindings))
		{
			Log.Reportf(EMessageSeverity::Error, TEXT("Failed to create bindings for global parameters."));
			return false;
		}

		if (Bindings.CopyBindings.Num() != 0 || Bindings.DelegateListeners.Num() != 0 || Bindings.ReferenceBindings.Num() != 0)
		{
			Log.Reportf(EMessageSeverity::Warning, TEXT("The global parameters should not target have binding."));
			return false;
		}

		if (!BindingsCompiler.CompileDelegateDispatchers(ParametersDesc, EditorData->CompiledDispatchers, Bindings.DelegateDispatchers, SourceValue))
		{
			Log.Reportf(EMessageSeverity::Error, TEXT("Failed to create delegate dispatcher bindings."));
			return false;
		}
	}

	return true;
}

bool FMetaStoryCompiler::CreateStates()
{
	check(EditorData);
	
	// Create main tree (omit subtrees)
	for (UMetaStoryState* SubTree : EditorData->SubTrees)
	{
		if (SubTree != nullptr
			&& SubTree->Type != EMetaStoryStateType::Subtree)
		{
			if (!CreateStateRecursive(*SubTree, FMetaStoryStateHandle::Invalid))
			{
				return false;
			}
		}
	}

	// Create Subtrees
	for (UMetaStoryState* SubTree : EditorData->SubTrees)
	{
		TArray<UMetaStoryState*> Stack;
		Stack.Push(SubTree);
		while (!Stack.IsEmpty())
		{
			if (UMetaStoryState* State = Stack.Pop())
			{
				if (State->Type == EMetaStoryStateType::Subtree)
				{
					if (!CreateStateRecursive(*State, FMetaStoryStateHandle::Invalid))
					{
						return false;
					}
				}
				Stack.Append(State->Children);
			}
		}
	}

	return true;
}

bool FMetaStoryCompiler::CreateStateRecursive(UMetaStoryState& State, const FMetaStoryStateHandle Parent)
{
	check(MetaStory);
	check(MetaStory->Schema);

	FMetaStoryCompilerLogStateScope LogStateScope(&State, Log);

	if ((State.Type == EMetaStoryStateType::LinkedAsset
		|| State.Type == EMetaStoryStateType::Linked)
		&& State.Children.Num() > 0)
	{
		Log.Reportf(EMessageSeverity::Warning,
			TEXT("Linked State cannot have child states, because the state selection will enter to the linked state on activation."));
	}

	const int32 StateIdx = MetaStory->States.AddDefaulted();
	FMetaStoryCompactState& CompactState = MetaStory->States[StateIdx];
	CompactState.Name = State.Name;
	CompactState.Tag = State.Tag;
	CompactState.Parent = Parent;
	CompactState.bEnabled = State.bEnabled;
	CompactState.bCheckPrerequisitesWhenActivatingChildDirectly = State.bCheckPrerequisitesWhenActivatingChildDirectly;
	CompactState.Weight = State.Weight;

	CompactState.bHasCustomTickRate = State.bHasCustomTickRate && MetaStory->Schema->IsScheduledTickAllowed();
	CompactState.CustomTickRate = FMath::Max(State.CustomTickRate, 0.0f);
	if (CompactState.bHasCustomTickRate && State.CustomTickRate < 0.0f)
	{
		Log.Reportf(EMessageSeverity::Warning, TEXT("The custom tick rate has to be greater than or equal to 0."));
	}

	CompactState.Type = State.Type;
	CompactState.SelectionBehavior = State.SelectionBehavior;

	if (!MetaStory->Schema->IsStateTypeAllowed(CompactState.Type))
	{
		Log.Reportf(EMessageSeverity::Warning,
			TEXT("The State '%s' has a restricted type for the schema."),
			*CompactState.Name.ToString());
		return false;
	}

	const bool bHasPredefinedSelectionBehavior = (CompactState.Type == EMetaStoryStateType::Linked || CompactState.Type == EMetaStoryStateType::LinkedAsset);
	if (bHasPredefinedSelectionBehavior)
	{
		CompactState.SelectionBehavior = EMetaStoryStateSelectionBehavior::TryEnterState;
	}
	else if (!MetaStory->Schema->IsStateSelectionAllowed(CompactState.SelectionBehavior))
	{
		Log.Reportf(EMessageSeverity::Warning,
			TEXT("The State '%s' has a restricted selection behavior for the schema."),
			*CompactState.Name.ToString());
		return false;
	}

	SourceStates.Add(&State);
	IDToState.Add(State.ID, StateIdx);

	// Child states
	const int32 ChildrenBegin = MetaStory->States.Num();
	if (const auto Validation = UE::MetaStory::Compiler::IsValidCount16(ChildrenBegin); Validation.DidFail())
	{
		Validation.Log(Log, TEXT("ChildrenBegin"));
		return false;
	}
	CompactState.ChildrenBegin = uint16(ChildrenBegin);

	for (UMetaStoryState* Child : State.Children)
	{
		if (Child != nullptr && Child->Type != EMetaStoryStateType::Subtree)
		{
			if (!CreateStateRecursive(*Child, FMetaStoryStateHandle((uint16)StateIdx)))
			{
				return false;
			}
		}
	}
	
	const int32 ChildrenEnd = MetaStory->States.Num();
	if (const auto Validation = UE::MetaStory::Compiler::IsValidCount16(ChildrenEnd); Validation.DidFail())
	{
		Validation.Log(Log, TEXT("ChildrenEnd"));
		return false;
	}
	MetaStory->States[StateIdx].ChildrenEnd = uint16(ChildrenEnd); // Not using CompactState here because the array may have changed.

	// create sub frame info
	if (!Parent.IsValid())
	{
		FMetaStoryCompactFrame& CompactFrame = MetaStory->Frames.AddDefaulted_GetRef();
		CompactFrame.RootState = FMetaStoryStateHandle((uint16)StateIdx);
		CompactFrame.NumberOfTasksStatusMasks = 0;
	}

	return true;
}

bool FMetaStoryCompiler::CreateConditions(UMetaStoryState& State, const FString& StatePath, TConstArrayView<FMetaStoryEditorNode> Conditions)
{
	bool bSucceeded = true;

	for (int32 Index = 0; Index < Conditions.Num(); Index++)
	{
		const bool bIsFirst = Index == 0;
		const FMetaStoryEditorNode& CondNode = Conditions[Index];
		// First operand should be copied as we don't have a previous item to operate on.
		const EMetaStoryExpressionOperand Operand = bIsFirst ? EMetaStoryExpressionOperand::Copy : CondNode.ExpressionOperand;
		// First indent must be 0 to make the parentheses calculation match.
		const int32 CurrIndent = bIsFirst ? 0 : FMath::Clamp((int32)CondNode.ExpressionIndent, 0, UE::MetaStory::MaxExpressionIndent);
		// Next indent, or terminate at zero.
		const int32 NextIndent = Conditions.IsValidIndex(Index + 1) ? FMath::Clamp((int32)Conditions[Index + 1].ExpressionIndent, 0, UE::MetaStory::MaxExpressionIndent) : 0;
		
		const int32 DeltaIndent = NextIndent - CurrIndent;

		if (!CreateCondition(State, StatePath, CondNode, Operand, (int8)DeltaIndent))
		{
			bSucceeded = false;
			continue;
		}
	}

	return bSucceeded;
}

bool FMetaStoryCompiler::CreateEvaluators()
{
	check(EditorData);
	check(MetaStory);

	bool bSucceeded = true;

	const int32 EvaluatorsBegin = Nodes.Num();
	if (const auto Validation = UE::MetaStory::Compiler::IsValidCount16(EvaluatorsBegin); Validation.DidFail())
	{
		Validation.Log(Log, TEXT("EvaluatorsBegin"));
		return false;
	}
	MetaStory->EvaluatorsBegin = uint16(EvaluatorsBegin);

	for (FMetaStoryEditorNode& EvalNode : EditorData->Evaluators)
	{
		const int32 GlobalInstanceIndex = InstanceStructs.Num();
		const FMetaStoryDataHandle EvalDataHandle(EMetaStoryDataSourceType::GlobalInstanceData, GlobalInstanceIndex);
		if (!CreateEvaluator(EvalNode, EvalDataHandle))
		{
			bSucceeded = false;
			continue;
		}
	}
	
	const int32 EvaluatorsNum = Nodes.Num() - EvaluatorsBegin;
	if (const auto Validation = UE::MetaStory::Compiler::IsValidCount16(EvaluatorsNum); Validation.DidFail())
	{
		Validation.Log(Log, TEXT("EvaluatorsNum"));
		return false;
	}
	MetaStory->EvaluatorsNum = uint16(EvaluatorsNum);

	return bSucceeded && CreateBindingsForNodes(EditorData->Evaluators, FMetaStoryIndex16(EvaluatorsBegin), InstanceStructs);
}

bool FMetaStoryCompiler::CreateGlobalTasks()
{
	check(EditorData);
	check(MetaStory);

	bool bSucceeded = true;

	const int32 GlobalTasksBegin = Nodes.Num();
	if (const auto Validation = UE::MetaStory::Compiler::IsValidCount16(GlobalTasksBegin); Validation.DidFail())
	{
		Validation.Log(Log, TEXT("GlobalTasksBegin"));
		return false;
	}
	MetaStory->GlobalTasksBegin = uint16(GlobalTasksBegin);
	MetaStory->CompletionGlobalTasksMask = 0;

	TArray<int32, TInlineAllocator<32>> ValidTaskNodeIndex;
	for (int32 TaskIndex = 0; TaskIndex < EditorData->GlobalTasks.Num(); ++TaskIndex)
	{
		FMetaStoryEditorNode& TaskNode = EditorData->GlobalTasks[TaskIndex];
		// Silently ignore empty nodes.
		if (!TaskNode.Node.IsValid())
		{
			continue;
		}

		const int32 GlobalInstanceIndex = InstanceStructs.Num();
		const FMetaStoryDataHandle TaskDataHandle(EMetaStoryDataSourceType::GlobalInstanceData, GlobalInstanceIndex);
		if (!CreateTask(nullptr, TaskNode, TaskDataHandle))
		{
			bSucceeded = false;
			continue;
		}

		ValidTaskNodeIndex.Add(TaskIndex);
	}

	if (ValidTaskNodeIndex.Num() > FMetaStoryTasksCompletionStatus::MaxNumberOfTasksPerGroup)
	{
		Log.Reportf(EMessageSeverity::Error, FMetaStoryBindableStructDesc(),
			TEXT("Exceeds the maximum number of global tasks (%d)"), FMetaStoryTasksCompletionStatus::MaxNumberOfTasksPerGroup);
		return false;
	}
	
	constexpr int32 CompletionGlobalTaskStartBitIndex = 0;
	const UE::MetaStory::Compiler::FCompletionTasksMaskResult MaskResult = UE::MetaStory::Compiler::MakeCompletionTasksMask(CompletionGlobalTaskStartBitIndex, EditorData->GlobalTasks, ValidTaskNodeIndex);
	MetaStory->CompletionGlobalTasksMask = MaskResult.Mask;
	GlobalTaskEndBit = MaskResult.FullMaskEndTaskBitOffset;
	MetaStory->CompletionGlobalTasksControl = MetaStory->Schema->AllowTasksCompletion() ? EditorData->GlobalTasksCompletion : EMetaStoryTaskCompletionType::Any;

	if (MaskResult.MaskFirstTaskBitOffset != 0)
	{
		ensureMsgf(false, TEXT("Invalid bit offset %d. The Global task should start at 0."), MaskResult.MaskFirstTaskBitOffset);
		Log.Reportf(EMessageSeverity::Error, FMetaStoryBindableStructDesc(), TEXT("Internal Error. Global task bit offset starts at 0."));
		return false;
	}
	
	const int32 GlobalTasksNum = Nodes.Num() - GlobalTasksBegin;
	if (const auto Validation = UE::MetaStory::Compiler::IsValidCount16(GlobalTasksNum); Validation.DidFail())
	{
		Validation.Log(Log, TEXT("GlobalTasksNum"));
		return false;
	}
	MetaStory->GlobalTasksNum = uint16(GlobalTasksNum);

	return bSucceeded && CreateBindingsForNodes(EditorData->GlobalTasks, FMetaStoryIndex16(GlobalTasksBegin), InstanceStructs);
}

bool FMetaStoryCompiler::CreateStateTasksAndParameters()
{
	check(MetaStory);

	bool bSucceeded = true;

	// Index of the first instance data per state. Accumulated depth first.
	struct FTaskAndParametersCompactState
	{
		int32 FirstInstanceDataIndex = 0;
		int32 NextBitIndexForCompletionMask = 0;
		bool bProcessed = false;
	};
	TArray<FTaskAndParametersCompactState> StateInfos;
	StateInfos.SetNum(MetaStory->States.Num());
	
	for (int32 StateIndex = 0; StateIndex < MetaStory->States.Num(); ++StateIndex)
	{
		FMetaStoryCompactState& CompactState = MetaStory->States[StateIndex];
		const FMetaStoryStateHandle CompactStateHandle(StateIndex);
		UMetaStoryState* State = SourceStates[StateIndex];
		check(State != nullptr);

		// Carry over instance data count from parent.
		if (CompactState.Parent.IsValid())
		{
			const FMetaStoryCompactState& ParentCompactState = MetaStory->States[CompactState.Parent.Index];

			check(StateInfos[StateIndex].bProcessed == false);
			check(!bSucceeded || StateInfos[CompactState.Parent.Index].bProcessed == true);

			const int32 InstanceDataBegin = StateInfos[CompactState.Parent.Index].FirstInstanceDataIndex + (int32)ParentCompactState.InstanceDataNum;
			StateInfos[StateIndex].FirstInstanceDataIndex = InstanceDataBegin;

			CompactState.Depth = ParentCompactState.Depth + 1;
		}

		int32 InstanceDataIndex = StateInfos[StateIndex].FirstInstanceDataIndex;

		FMetaStoryCompilerLogStateScope LogStateScope(State, Log);

		// Create parameters
		
		// Each state has their parameters as instance data.
		FInstancedStruct& Instance = InstanceStructs.AddDefaulted_GetRef();
		Instance.InitializeAs<FMetaStoryCompactParameters>(State->Parameters.Parameters);
		FMetaStoryCompactParameters& CompactStateTreeParameters = Instance.GetMutable<FMetaStoryCompactParameters>(); 
			
		const int32 InstanceIndex = InstanceStructs.Num() - 1;
		if (const auto Validation = UE::MetaStory::Compiler::IsValidIndex16(InstanceIndex); Validation.DidFail())
		{
			Validation.Log(Log, TEXT("InstanceIndex"));
			return false;
		}
		CompactState.ParameterTemplateIndex = FMetaStoryIndex16(InstanceIndex);

		if (State->Type == EMetaStoryStateType::Subtree)
		{
			CompactState.ParameterDataHandle = FMetaStoryDataHandle(EMetaStoryDataSourceType::SubtreeParameterData, InstanceDataIndex++, CompactStateHandle);
		}
		else
		{
			CompactState.ParameterDataHandle = FMetaStoryDataHandle(EMetaStoryDataSourceType::StateParameterData, InstanceDataIndex++, CompactStateHandle);
		}

		// @todo: We should be able to skip empty parameter data.

		const FString StatePath = State->GetPath(); 
		
		// Binding target
		FMetaStoryBindableStructDesc LinkedParamsDesc = {
			StatePath,
			FName("Parameters"),
			State->Parameters.Parameters.GetPropertyBagStruct(),
			CompactState.ParameterDataHandle,
			EMetaStoryBindableStructSource::StateParameter,
			State->Parameters.ID
		};

		if (!UE::MetaStory::Compiler::ValidateNoLevelActorReferences(Log, LinkedParamsDesc, FMetaStoryDataView(), FMetaStoryDataView(CompactStateTreeParameters.Parameters.GetMutableValue())))
		{
			bSucceeded = false;
			continue;
		}

		// Add as binding source.
		BindingsCompiler.AddSourceStruct(LinkedParamsDesc);

		if (State->bHasRequiredEventToEnter)
		{
			CompactState.EventDataIndex = FMetaStoryIndex16(InstanceDataIndex++);
			CompactState.RequiredEventToEnter.Tag = State->RequiredEventToEnter.Tag;
			CompactState.RequiredEventToEnter.PayloadStruct = State->RequiredEventToEnter.PayloadStruct;
			CompactState.bConsumeEventOnSelect = State->RequiredEventToEnter.bConsumeEventOnSelect;

			const FString StatePathWithConditions = StatePath + TEXT("/EnterConditions");

			FMetaStoryBindableStructDesc Desc;
			Desc.StatePath = StatePathWithConditions,
			Desc.Struct = FMetaStoryEvent::StaticStruct();
			Desc.Name = FName("Enter Event");
			Desc.ID = State->GetEventID();
			Desc.DataSource = EMetaStoryBindableStructSource::StateEvent;
			Desc.DataHandle = FMetaStoryDataHandle(EMetaStoryDataSourceType::StateEvent, CompactState.EventDataIndex.Get(), CompactStateHandle);

			BindingsCompiler.AddSourceStruct(Desc);

			if (!CompactState.RequiredEventToEnter.IsValid())
			{
				Log.Reportf(EMessageSeverity::Error, Desc,
					TEXT("Event is marked as required, but isn't set up."));
				bSucceeded = false;
				continue;
			}
		}

		if (CompactState.Depth >= FMetaStoryActiveStates::MaxStates)
		{
			Log.Reportf(EMessageSeverity::Error, LinkedParamsDesc,
				TEXT("Exceeds the maximum depth of execution (%u)"), FMetaStoryActiveStates::MaxStates);
			bSucceeded = false;
			continue;
		}

		// Subtrees parameters cannot have bindings
		if (State->Type != EMetaStoryStateType::Subtree)
		{
			FMetaStoryIndex16 PropertyFunctionsBegin(Nodes.Num());
			if (!CreatePropertyFunctionsForStruct(LinkedParamsDesc.ID))
			{
				bSucceeded = false;
				continue;
			}

			FMetaStoryIndex16 PropertyFunctionsEnd(Nodes.Num());
		
			if (PropertyFunctionsBegin == PropertyFunctionsEnd)
			{
				PropertyFunctionsBegin = FMetaStoryIndex16::Invalid;
				PropertyFunctionsEnd = FMetaStoryIndex16::Invalid;
			}

			// Only nodes support output bindings
			constexpr FMetaStoryIndex16* OutputBindingsBatch = nullptr;
			if (!CreateBindingsForStruct(LinkedParamsDesc, FMetaStoryDataView(CompactStateTreeParameters.Parameters.GetMutableValue()), PropertyFunctionsBegin, PropertyFunctionsEnd, CompactState.ParameterBindingsBatch, OutputBindingsBatch))
			{
				bSucceeded = false;
				continue;
			}
		}

		// Create tasks
		const int32 TasksBegin = Nodes.Num();
		if (const auto Validation = UE::MetaStory::Compiler::IsValidCount16(TasksBegin); Validation.DidFail())
		{
			Validation.Log(Log, TEXT("TasksBegin"));
			return false;
		}
		CompactState.TasksBegin = uint16(TasksBegin);
		
		TArrayView<FMetaStoryEditorNode> Tasks;
		if (State->Tasks.Num())
		{
			Tasks = State->Tasks;
		}
		else if (State->SingleTask.Node.IsValid())
		{
			Tasks = TArrayView<FMetaStoryEditorNode>(&State->SingleTask, 1);
		}
		
		bool bCreateTaskSucceeded = true;
		int32 EnabledTasksNum = 0;
		TArray<int32, TInlineAllocator<32>> ValidTaskNodeIndex;
		for (int32 TaskIndex = 0; TaskIndex < Tasks.Num(); ++TaskIndex)
		{
			FMetaStoryEditorNode& TaskNode = Tasks[TaskIndex];
			// Silently ignore empty nodes.
			if (!TaskNode.Node.IsValid())
			{
				continue;
			}

			FMetaStoryTaskBase& Task = TaskNode.Node.GetMutable<FMetaStoryTaskBase>();
			if(Task.bTaskEnabled)
			{
				EnabledTasksNum += 1;
			}

			const FMetaStoryDataHandle TaskDataHandle(EMetaStoryDataSourceType::ActiveInstanceData, InstanceDataIndex++, CompactStateHandle);
			if (!CreateTask(State, TaskNode, TaskDataHandle))
			{
				bSucceeded = false;
				bCreateTaskSucceeded = false;
				continue;
			}

			ValidTaskNodeIndex.Add(TaskIndex);
		}

		if (!bCreateTaskSucceeded)
		{
			continue;
		}
		
		const int32 TasksNum = Nodes.Num() - TasksBegin;
		check(ValidTaskNodeIndex.Num() == TasksNum);
		if (const auto Validation = UE::MetaStory::Compiler::IsValidCount8(TasksNum); Validation.DidFail())
		{
			Validation.Log(Log, TEXT("TasksNum"));
			return false;
		}

		// Create tasks
		if (TasksNum > FMetaStoryTasksCompletionStatus::MaxNumberOfTasksPerGroup)
		{
			Log.Reportf(EMessageSeverity::Error, LinkedParamsDesc,
				TEXT("Exceeds the maximum number of tasks (%d)"), FMetaStoryTasksCompletionStatus::MaxNumberOfTasksPerGroup);
			bSucceeded = false;
			continue;
		}

		const int32 InstanceDataNum = InstanceDataIndex - StateInfos[StateIndex].FirstInstanceDataIndex;
		if (const auto Validation = UE::MetaStory::Compiler::IsValidCount8(InstanceDataNum); Validation.DidFail())
		{
			Validation.Log(Log, TEXT("InstanceDataNum"));
			return false;
		}

		CompactState.TasksNum = uint8(TasksNum);
		CompactState.EnabledTasksNum = uint8(EnabledTasksNum);
		CompactState.InstanceDataNum = uint8(InstanceDataNum);

		// Create completion mask
		{
			int32 StartBitIndex = 0;
			if (CompactState.Parent.IsValid())
			{
				StartBitIndex = StateInfos[CompactState.Parent.Index].NextBitIndexForCompletionMask;
			}
			else
			{
				// Frame need an extra buffer for global tasks.
				//Linked sub-frames do not support global tasks but they can be use to replace root (old behavior).
				StartBitIndex = GlobalTaskEndBit;
			}

			const UE::MetaStory::Compiler::FCompletionTasksMaskResult MaskResult = UE::MetaStory::Compiler::MakeCompletionTasksMask(StartBitIndex, Tasks, ValidTaskNodeIndex);

			const int32 CompletionTasksMaskBufferIndex = MaskResult.MaskBufferIndex;
			if (const auto Validation = UE::MetaStory::Compiler::IsValidCount8(CompletionTasksMaskBufferIndex); Validation.DidFail())
			{
				Validation.Log(Log, TEXT("CompletionTasksMaskBufferIndex"));
				bSucceeded = false;
				continue;
			}
			const int32 CompletionTasksMaskBitsOffset = MaskResult.MaskFirstTaskBitOffset;
			if (const auto Validation = UE::MetaStory::Compiler::IsValidCount8(CompletionTasksMaskBitsOffset); Validation.DidFail())
			{
				Validation.Log(Log, TEXT("CompletionTasksMaskBitsOffset"));
				bSucceeded = false;
				continue;
			}

			CompactState.CompletionTasksMask = MaskResult.Mask;
			CompactState.CompletionTasksControl = MetaStory->Schema->AllowTasksCompletion() ? State->TasksCompletion : EMetaStoryTaskCompletionType::Any;
			CompactState.CompletionTasksMaskBufferIndex = static_cast<uint8>(CompletionTasksMaskBufferIndex);
			CompactState.CompletionTasksMaskBitsOffset = static_cast<uint8>(CompletionTasksMaskBitsOffset);
			StateInfos[StateIndex].NextBitIndexForCompletionMask = MaskResult.FullMaskEndTaskBitOffset;

			// Find Frame and update the number of masks.
			{
				FMetaStoryStateHandle FrameHandle = CompactStateHandle;
				while (true)
				{
					const FMetaStoryCompactState* ParentState = MetaStory->GetStateFromHandle(FrameHandle);
					check(ParentState);
					if (!ParentState->Parent.IsValid())
					{
						break;
					}
					FrameHandle = ParentState->Parent;
				}
				FMetaStoryCompactFrame* FoundFrame = MetaStory->Frames.FindByPredicate([FrameHandle](const FMetaStoryCompactFrame& Frame)
					{
						return Frame.RootState == FrameHandle;
					});
				if (FoundFrame == nullptr)
				{
					Log.Reportf(EMessageSeverity::Error, LinkedParamsDesc, TEXT("The parent frame can't be found"));
					bSucceeded = false;
					continue;
				}

				FoundFrame->NumberOfTasksStatusMasks = FMath::Max(FoundFrame->NumberOfTasksStatusMasks, static_cast<uint8>(CompactState.CompletionTasksMaskBufferIndex+1));
			}
		}

		if (!CreateBindingsForNodes(Tasks, FMetaStoryIndex16(TasksBegin), InstanceStructs))
		{
			bSucceeded = false;
			continue;
		}

		StateInfos[StateIndex].bProcessed = true;
	}
	
	return bSucceeded;
}

bool FMetaStoryCompiler::CreateStateTransitions()
{
	check(MetaStory);

	bool bSucceeded = true;

	for (int32 i = 0; i < MetaStory->States.Num(); i++)
	{
		FMetaStoryCompactState& CompactState = MetaStory->States[i];
		UMetaStoryState* SourceState = SourceStates[i];
		check(SourceState != nullptr);

		FMetaStoryCompilerLogStateScope LogStateScope(SourceState, Log);

		const FString StatePath = SourceState->GetPath();

		// Enter conditions.
		const int32 EnterConditionsBegin = Nodes.Num();
		if (const auto Validation = UE::MetaStory::Compiler::IsValidCount16(EnterConditionsBegin); Validation.DidFail())
		{
			Validation.Log(Log, TEXT("EnterConditionsBegin"));
			return false;
		}
		CompactState.EnterConditionsBegin = uint16(EnterConditionsBegin);

		const FString StatePathWithConditions = StatePath + TEXT("/EnterConditions");
		if (!CreateConditions(*SourceState, StatePathWithConditions, SourceState->EnterConditions))
		{
			Log.Reportf(EMessageSeverity::Error,
				TEXT("Failed to create state enter condition."));
			bSucceeded = false;
			continue;
		}
		
		const int32 EnterConditionsNum = Nodes.Num() - EnterConditionsBegin;
		if (const auto Validation = UE::MetaStory::Compiler::IsValidCount8(EnterConditionsNum); Validation.DidFail())
		{
			Validation.Log(Log, TEXT("EnterConditionsNum"));
			return false;
		}
		CompactState.EnterConditionsNum = uint8(EnterConditionsNum);

		const bool bUseEvaluationScopeInstanceData = UE::MetaStory::Compiler::bEnableConditionWithEvaluationScopeInstanceData;
		TArray<FInstancedStruct>& InstancedStructContainer = bUseEvaluationScopeInstanceData ? EvaluationScopeStructs : SharedInstanceStructs;
		if (!CreateBindingsForNodes(SourceState->EnterConditions, FMetaStoryIndex16(EnterConditionsBegin), InstancedStructContainer))
		{
			bSucceeded = false;
			continue;
		}

		// Check if any of the enter conditions require state completion events, and cache that.
		for (int32 ConditionIndex = (int32)CompactState.EnterConditionsBegin; ConditionIndex < Nodes.Num(); ConditionIndex++)
		{
			if (const FMetaStoryConditionBase* Cond = Nodes[ConditionIndex].GetPtr<const FMetaStoryConditionBase>())
			{
				if (Cond->bHasShouldCallStateChangeEvents)
				{
					CompactState.bHasStateChangeConditions = true;
					break;
				}
			}
		}
		
		// Linked state
		if (SourceState->Type == EMetaStoryStateType::Linked)
		{
			// Make sure the linked state is not self or parent to this state.
			const UMetaStoryState* LinkedParentState = nullptr;
			for (const UMetaStoryState* State = SourceState; State != nullptr; State = State->Parent)
			{
				if (State->ID == SourceState->LinkedSubtree.ID)
				{
					LinkedParentState = State;
					break;
				}
			}
			
			if (LinkedParentState != nullptr)
			{
				Log.Reportf(EMessageSeverity::Error,
					TEXT("State is linked to it's parent subtree '%s', which will create infinite loop."),
					*LinkedParentState->Name.ToString());
				bSucceeded = false;
				continue;
			}

			// The linked state must be a subtree.
			const UMetaStoryState* TargetState = GetState(SourceState->LinkedSubtree.ID);
			if (TargetState == nullptr)
			{
				Log.Reportf(EMessageSeverity::Error,
					TEXT("Failed to resolve linked subtree '%s'."),
					*SourceState->LinkedSubtree.Name.ToString());
				bSucceeded = false;
				continue;
			}
			
			if (TargetState->Type != EMetaStoryStateType::Subtree)
			{
				Log.Reportf(EMessageSeverity::Error,
					TEXT("State '%s' is linked to subtree '%s', which is not a subtree."),
					*SourceState->Name.ToString(), *TargetState->Name.ToString());
				bSucceeded = false;
				continue;
			}
			
			CompactState.LinkedState = GetStateHandle(SourceState->LinkedSubtree.ID);
			
			if (!CompactState.LinkedState.IsValid())
			{
				Log.Reportf(EMessageSeverity::Error,
					TEXT("Failed to resolve linked subtree '%s'."),
					*SourceState->LinkedSubtree.Name.ToString());
				bSucceeded = false;
				continue;
			}
		}
		else if (SourceState->Type == EMetaStoryStateType::LinkedAsset)
		{
			// Do not allow to link to the same asset (might create recursion)
			if (SourceState->LinkedAsset == MetaStory)
			{
				Log.Reportf(EMessageSeverity::Error,
					TEXT("It is not allowed to link to the same tree, as it might create infinite loop."));
				bSucceeded = false;
				continue;
			}

			if (SourceState->LinkedAsset)
			{
				// Linked asset must have same schema.
				const UMetaStorySchema* LinkedAssetSchema = SourceState->LinkedAsset->GetSchema();

				if (!LinkedAssetSchema)
				{
					Log.Reportf(EMessageSeverity::Error,
						TEXT("Linked State Tree asset must have valid schema."));
					bSucceeded = false;
					continue;
				}
			
				check(MetaStory->Schema);
				if (LinkedAssetSchema->GetClass() != MetaStory->Schema->GetClass())
				{
					Log.Reportf(EMessageSeverity::Error,
						TEXT("Linked State Tree asset '%s' must have same schema class as this asset. Linked asset has '%s', expected '%s'."),
						*GetFullNameSafe(SourceState->LinkedAsset),
						*LinkedAssetSchema->GetClass()->GetDisplayNameText().ToString(),
						*MetaStory->Schema->GetClass()->GetDisplayNameText().ToString()
					);
					bSucceeded = false;
					continue;
				}
			}
			
			CompactState.LinkedAsset = SourceState->LinkedAsset;
		}

		// Transitions
		const int32 TransitionsBegin = MetaStory->Transitions.Num();
		if (const auto Validation = UE::MetaStory::Compiler::IsValidCount16(TransitionsBegin); Validation.DidFail())
		{
			Validation.Log(Log, TEXT("TransitionsBegin"));
			return false;
		}
		CompactState.TransitionsBegin = uint16(TransitionsBegin);

		bool bTransitionSucceeded = true;
		for (FMetaStoryTransition& Transition : SourceState->Transitions)
		{
			const int32 TransitionIndex = MetaStory->Transitions.Num();
			IDToTransition.Add(Transition.ID, TransitionIndex);

			FMetaStoryCompactStateTransition& CompactTransition = MetaStory->Transitions.AddDefaulted_GetRef();
			CompactTransition.Trigger = Transition.Trigger;
			CompactTransition.Priority = Transition.Priority;

			if (Transition.Trigger == EMetaStoryTransitionTrigger::OnDelegate)
			{
				FPropertyBindingPath DelegateBindingPath(Transition.ID, GET_MEMBER_NAME_CHECKED(FMetaStoryTransition, DelegateListener));

				const FPropertyBindingBinding* Binding = EditorData->EditorBindings.FindBinding(DelegateBindingPath);

				if (Binding == nullptr)
				{
					bTransitionSucceeded = false;
					Log.Reportf(EMessageSeverity::Error,
						TEXT("On Delegate Transition to '%s' requires to be bound to some delegate dispatcher."),
						*Transition.State.Name.ToString());
					continue;
				}

				CompactTransition.RequiredDelegateDispatcher = BindingsCompiler.GetDispatcherFromPath(Binding->GetSourcePath());
				if (!CompactTransition.RequiredDelegateDispatcher.IsValid())
				{
					bTransitionSucceeded = false;
					Log.Reportf(EMessageSeverity::Error,
						TEXT("On Delegate Transition to '%s' is bound to unknown delegate dispatcher"),
						*Transition.State.Name.ToString());
					continue;
				}
			}

			CompactTransition.bTransitionEnabled = Transition.bTransitionEnabled;

			if (Transition.bDelayTransition)
			{
				CompactTransition.Delay.Set(Transition.DelayDuration, Transition.DelayRandomVariance);
			}
			
			if (CompactState.SelectionBehavior == EMetaStoryStateSelectionBehavior::TryFollowTransitions
				&& Transition.bDelayTransition)
			{
				Log.Reportf(EMessageSeverity::Warning,
					TEXT("Transition to '%s' with delay will be ignored during state selection."),
					*Transition.State.Name.ToString());
			}

			if (EnumHasAnyFlags(Transition.Trigger, EMetaStoryTransitionTrigger::OnStateCompleted))
			{
				// Completion transitions dont have priority.
				CompactTransition.Priority = EMetaStoryTransitionPriority::None;
				
				// Completion transitions cannot have delay.
				CompactTransition.Delay.Reset();

				// Completion transitions must have valid target state.
				if (Transition.State.LinkType == EMetaStoryTransitionType::None)
				{
					Log.Reportf(EMessageSeverity::Error,
						TEXT("State completion transition to '%s' must have transition to valid state, 'None' not accepted."),
						*Transition.State.Name.ToString());
					bTransitionSucceeded = false;
					continue;
				}
			}
			
			CompactTransition.State = FMetaStoryStateHandle::Invalid;
			if (!ResolveTransitionStateAndFallback(SourceState, Transition.State, CompactTransition.State, CompactTransition.Fallback))
			{
				bTransitionSucceeded = false;
				continue;
			}

			if (CompactTransition.State.IsValid()
				&& !CompactTransition.State.IsCompletionState())
			{
				FMetaStoryCompactState& TransitionTargetState = MetaStory->States[CompactTransition.State.Index];
				if (TransitionTargetState.Type == EMetaStoryStateType::Subtree)
				{
					Log.Reportf(EMessageSeverity::Warning,
						TEXT("Transitioning directly to a Subtree State '%s' is not recommended, as it may have unexpected results. Subtree States should be used with Linked States instead."),
						*TransitionTargetState.Name.ToString());
				}
			}

			const FString StatePathWithTransition = StatePath + FString::Printf(TEXT("/Transition[%d]"), TransitionIndex - TransitionsBegin);
			
			if (Transition.Trigger == EMetaStoryTransitionTrigger::OnEvent)
			{
				CompactTransition.RequiredEvent.Tag = Transition.RequiredEvent.Tag;
				CompactTransition.RequiredEvent.PayloadStruct = Transition.RequiredEvent.PayloadStruct;
				CompactTransition.bConsumeEventOnSelect = Transition.RequiredEvent.bConsumeEventOnSelect;

				FMetaStoryBindableStructDesc Desc;
				Desc.StatePath = StatePathWithTransition;
				Desc.Struct = FMetaStoryEvent::StaticStruct();
				Desc.Name = FName(TEXT("Transition Event"));
				Desc.ID = Transition.GetEventID();
				Desc.DataSource = EMetaStoryBindableStructSource::TransitionEvent;
				Desc.DataHandle = FMetaStoryDataHandle(EMetaStoryDataSourceType::TransitionEvent, TransitionIndex);

				if (!Transition.RequiredEvent.IsValid())
				{
					Log.Reportf(EMessageSeverity::Error, Desc,
						TEXT("On Event Transition requires at least tag or payload to be set up."),
						*Transition.State.Name.ToString());
					bTransitionSucceeded = false;
					continue;
				}

				if (CompactTransition.State.IsValid()
					&& !CompactTransition.State.IsCompletionState())
				{
					FMetaStoryCompactState& TransitionTargetState = MetaStory->States[CompactTransition.State.Index];
					if (TransitionTargetState.RequiredEventToEnter.IsValid() && !TransitionTargetState.RequiredEventToEnter.IsSubsetOfAnotherDesc(CompactTransition.RequiredEvent))
					{
						Log.Reportf(EMessageSeverity::Error, Desc,
							TEXT("On Event transition to '%s' will never succeed as transition and state required events are incompatible."),
							*TransitionTargetState.Name.ToString());
						bTransitionSucceeded = false;
						continue;
					}
				}

				BindingsCompiler.AddSourceStruct(Desc);
			}

			if (CompactTransition.bTransitionEnabled)
			{
				CompactState.bHasTickTriggerTransitions |= EnumHasAnyFlags(Transition.Trigger, EMetaStoryTransitionTrigger::OnTick);
				CompactState.bHasEventTriggerTransitions |= EnumHasAnyFlags(Transition.Trigger, EMetaStoryTransitionTrigger::OnEvent);
				CompactState.bHasDelegateTriggerTransitions |= EnumHasAnyFlags(Transition.Trigger, EMetaStoryTransitionTrigger::OnDelegate);
				CompactState.bHasCompletedTriggerTransitions |= EnumHasAnyFlags(Transition.Trigger, EMetaStoryTransitionTrigger::OnStateCompleted);
				CompactState.bHasSucceededTriggerTransitions |= EnumHasAnyFlags(Transition.Trigger, EMetaStoryTransitionTrigger::OnStateSucceeded);
				CompactState.bHasFailedTriggerTransitions |= EnumHasAnyFlags(Transition.Trigger, EMetaStoryTransitionTrigger::OnStateFailed);
			}

			const int32 ConditionsBegin = Nodes.Num();
			if (const auto Validation = UE::MetaStory::Compiler::IsValidCount16(ConditionsBegin); Validation.DidFail())
			{
				Validation.Log(Log, TEXT("ConditionsBegin"));
				return false;
			}
			CompactTransition.ConditionsBegin = uint16(ConditionsBegin);
			
			if (!CreateConditions(*SourceState, StatePathWithTransition, Transition.Conditions))
			{
				Log.Reportf(EMessageSeverity::Error,
					TEXT("Failed to create condition for transition to '%s'."),
					*Transition.State.Name.ToString());
				bTransitionSucceeded = false;
				continue;
			}

			const int32 ConditionsNum = Nodes.Num() - ConditionsBegin;
			if (const auto Validation = UE::MetaStory::Compiler::IsValidCount8(ConditionsNum); Validation.DidFail())
			{
				Validation.Log(Log, TEXT("ConditionsNum"));
				return false;
			}
			CompactTransition.ConditionsNum = uint8(ConditionsNum);

			if (!CreateBindingsForNodes(Transition.Conditions, FMetaStoryIndex16(ConditionsBegin), InstancedStructContainer))
			{
				bTransitionSucceeded = false;
				continue;
			}
		}

		if (!bTransitionSucceeded)
		{
			bSucceeded = false;
			continue;
		}
		
		const int32 TransitionsNum = MetaStory->Transitions.Num() - TransitionsBegin;
		if (const auto Validation = UE::MetaStory::Compiler::IsValidCount8(TransitionsNum); Validation.DidFail())
		{
			Validation.Log(Log, TEXT("TransitionsNum"));
			return false;
		}
		CompactState.TransitionsNum = uint8(TransitionsNum);
	}

	// @todo: Add test to check that all success/failure transition is possible (see editor).
	
	return bSucceeded;
}

bool FMetaStoryCompiler::CreateStateConsiderations()
{
	check(MetaStory);

	bool bSucceeded = true;

	for (int32 i = 0; i < MetaStory->States.Num(); i++)
	{
		FMetaStoryCompactState& CompactState = MetaStory->States[i];
		UMetaStoryState* SourceState = SourceStates[i];
		check(SourceState != nullptr);

		FMetaStoryCompilerLogStateScope LogStateScope(SourceState, Log);

		const FString StatePath = SourceState->GetPath();

		const int32 UtilityConsiderationsBegin = Nodes.Num();
		if (const auto Validation = UE::MetaStory::Compiler::IsValidCount16(UtilityConsiderationsBegin); Validation.DidFail())
		{
			Validation.Log(Log, TEXT("UtilityConsiderationsBegin"));
			bSucceeded = false;
			continue;
		}
		CompactState.UtilityConsiderationsBegin = uint16(UtilityConsiderationsBegin);

		const FString StatePathWithConsiderations = StatePath + TEXT("/Considerations");
		if (!CreateConsiderations(*SourceState, StatePathWithConsiderations, SourceState->Considerations))
		{
			Log.Reportf(EMessageSeverity::Error,
				TEXT("Failed to create state utility considerations."));
			bSucceeded = false;
			continue;
		}

		const int32 UtilityConsiderationsNum = Nodes.Num() - UtilityConsiderationsBegin;
		if (const auto Validation = UE::MetaStory::Compiler::IsValidCount8(UtilityConsiderationsNum); Validation.DidFail())
		{
			Validation.Log(Log, TEXT("UtilityConsiderationsNum"));
			bSucceeded = false;
			continue;
		}
		CompactState.UtilityConsiderationsNum = uint8(UtilityConsiderationsNum);

		const bool bUseEvaluationScopeInstanceData = UE::MetaStory::Compiler::bEnableUtilityConsiderationWithEvaluationScopeInstanceData;
		TArray<FInstancedStruct>& InstancedStructContainer = bUseEvaluationScopeInstanceData ? EvaluationScopeStructs : SharedInstanceStructs;
		if (!CreateBindingsForNodes(SourceState->Considerations, FMetaStoryIndex16(UtilityConsiderationsBegin), InstancedStructContainer))
		{
			bSucceeded = false;
			continue;
		}
	}

	return bSucceeded;
}

bool FMetaStoryCompiler::CreateBindingsForNodes(TConstArrayView<FMetaStoryEditorNode> EditorNodes, FMetaStoryIndex16 NodesBegin, TArray<FInstancedStruct>& Instances)
{
	check(NodesBegin.IsValid());

	bool bSucceeded = true;

	int32 NodeIndex = NodesBegin.Get();
	for (const FMetaStoryEditorNode& EditorNode : EditorNodes)
	{
		// Node might be an empty line in Editor.
		if (!EditorNode.Node.IsValid())
		{
			continue;
		}

		FInstancedStruct& NodeInstancedStruct = Nodes[NodeIndex++];
		FMetaStoryNodeBase& Node = NodeInstancedStruct.GetMutable<FMetaStoryNodeBase>();

		FMetaStoryIndex16 PropertyFunctionsBegin(Nodes.Num());
		if (!CreatePropertyFunctionsForStruct(EditorNode.ID))
		{
			bSucceeded = false;
			continue;
		}
		FMetaStoryIndex16 PropertyFunctionsEnd(Nodes.Num());
		
		if (PropertyFunctionsBegin == PropertyFunctionsEnd)
		{
			PropertyFunctionsBegin = FMetaStoryIndex16::Invalid;
			PropertyFunctionsEnd = FMetaStoryIndex16::Invalid;
		}

		FMetaStoryDataView InstanceView;
		check(Instances.IsValidIndex(Node.InstanceTemplateIndex.Get()));

		FInstancedStruct& Instance = Instances[Node.InstanceTemplateIndex.Get()];
		if (FMetaStoryInstanceObjectWrapper* ObjectWrapper = Instance.GetMutablePtr<FMetaStoryInstanceObjectWrapper>())
		{
			check(EditorNode.InstanceObject->GetClass() == ObjectWrapper->InstanceObject->GetClass());
			InstanceView = FMetaStoryDataView(ObjectWrapper->InstanceObject);
		}
		else
		{
			check(EditorNode.Instance.GetScriptStruct() == Instance.GetScriptStruct());
			InstanceView = FMetaStoryDataView(Instance);
		}

		{
			const FMetaStoryBindableStructDesc* BindableInstanceStruct = BindingsCompiler.GetSourceStructDescByID(EditorNode.ID);
			check(BindableInstanceStruct);
			if (!CreateBindingsForStruct(*BindableInstanceStruct, InstanceView, PropertyFunctionsBegin, PropertyFunctionsEnd, Node.BindingsBatch, &Node.OutputBindingsBatch))
			{
				bSucceeded = false;
				continue;
			}

			if (const FMetaStoryBindableStructDesc* BindableNodeStruct = BindingsCompiler.GetSourceStructDescByID(EditorNode.GetNodeID()))
			{
				if (UE::MetaStory::Compiler::IsNodeStructEligibleForBinding(NodeInstancedStruct))
				{
					if (!CreateBindingsForStruct(*BindableNodeStruct, static_cast<FStructView>(NodeInstancedStruct), PropertyFunctionsBegin, PropertyFunctionsEnd, Node.BindingsBatch))
					{
						bSucceeded = false;
						continue;
					}
				}
			}
		}
	}

	return bSucceeded;
}

bool FMetaStoryCompiler::CreateBindingsForStruct(
	const FMetaStoryBindableStructDesc& TargetStruct, 
	FMetaStoryDataView TargetValue, 
	FMetaStoryIndex16 PropertyFuncsBegin, 
	FMetaStoryIndex16 PropertyFuncsEnd, 
	FMetaStoryIndex16& OutBatchIndex, 
	FMetaStoryIndex16* OutOutputBindingBatchIndex /* nullptr */)
{
	auto CopyBatch = [Self = this, &TargetStruct]
		(TConstArrayView<FMetaStoryPropertyPathBinding> InCopyBindings, FMetaStoryIndex16 InPropertyFuncsBegin, FMetaStoryIndex16 InPropertyFuncsEnd, const TCHAR* InLogContextText, FMetaStoryIndex16& OutBatchIndex)
		{
			int32 BatchIndex = INDEX_NONE;

			// Compile batch copy for this struct, we pass in all the bindings, the compiler will pick up the ones for the target structs.
			if (!Self->BindingsCompiler.CompileBatch(TargetStruct, InCopyBindings, InPropertyFuncsBegin, InPropertyFuncsEnd, BatchIndex))
			{
				return false;
			}

			if (const auto Validation = UE::MetaStory::Compiler::IsValidIndex16(BatchIndex); Validation.DidFail())
			{
				Validation.Log(Self->Log, InLogContextText, TargetStruct);
				return false;
			}

			OutBatchIndex = FMetaStoryIndex16(BatchIndex);
			return true;
		};

	FValidatedPathBindings Bindings;

	// Check that the bindings for this struct are still all valid.
	if (!GetAndValidateBindings(TargetStruct, TargetValue, Bindings))
	{
		return false;
	}

	// Copy Bindings
	{
		if (!CopyBatch(Bindings.CopyBindings, PropertyFuncsBegin, PropertyFuncsEnd, TEXT("CopiesBatchIndex"), OutBatchIndex))
		{
			return false;
		}

		if (OutOutputBindingBatchIndex)
		{
			// For output binding, we don't run any property functions
			if (!CopyBatch(Bindings.OutputCopyBindings, FMetaStoryIndex16::Invalid, FMetaStoryIndex16::Invalid, TEXT("OutputCopiesBatchIndex"), *OutOutputBindingBatchIndex))
			{
				return false;
			}
		}
	}

	// Delegate Dispatcher
	if (!BindingsCompiler.CompileDelegateDispatchers(TargetStruct, EditorData->CompiledDispatchers, Bindings.DelegateDispatchers, TargetValue))
	{
		return false;
	}

	// Delegate Listener
	if (!BindingsCompiler.CompileDelegateListeners(TargetStruct, Bindings.DelegateListeners, TargetValue))
	{
		return false;
	}

	// Reference Bindings
	if (!BindingsCompiler.CompileReferences(TargetStruct, Bindings.ReferenceBindings, TargetValue, IDToStructValue))
	{
		return false;
	}

	return true;
}

bool FMetaStoryCompiler::CreatePropertyFunctionsForStruct(FGuid StructID)
{
	for (const FPropertyBindingBinding& Binding : EditorData->EditorBindings.GetBindings())
	{
		if (Binding.GetTargetPath().GetStructID() != StructID)
		{
			continue;
		}

		const FConstStructView NodeView = Binding.GetPropertyFunctionNode();
		if (!NodeView.IsValid())
		{
			continue;
		}

		const FMetaStoryEditorNode& FuncEditorNode = NodeView.Get<const FMetaStoryEditorNode>();
		if (!CreatePropertyFunction(FuncEditorNode))
		{
			return false;
		}
	}

	return true;
}

bool FMetaStoryCompiler::CreatePropertyFunction(const FMetaStoryEditorNode& FuncEditorNode)
{
	if (!CreatePropertyFunctionsForStruct(FuncEditorNode.ID))
	{
		return false;
	}

	FMetaStoryBindableStructDesc StructDesc;
	StructDesc.StatePath = UE::MetaStory::Editor::PropertyFunctionStateName;
	StructDesc.ID = FuncEditorNode.ID;
	StructDesc.Name = FuncEditorNode.GetName();
	StructDesc.DataSource = EMetaStoryBindableStructSource::PropertyFunction;

	const bool bUseEvaluationScopeInstanceData = UE::MetaStory::Compiler::bEnablePropertyFunctionWithEvaluationScopeInstanceData;
	const FMetaStoryDataHandle DataHandle = bUseEvaluationScopeInstanceData
		? FMetaStoryDataHandle(EMetaStoryDataSourceType::EvaluationScopeInstanceData, EvaluationScopeStructs.Num())
		: FMetaStoryDataHandle(EMetaStoryDataSourceType::SharedInstanceData, SharedInstanceStructs.Num());
	TArray<FInstancedStruct>& InstancedStructContainer = bUseEvaluationScopeInstanceData ? EvaluationScopeStructs : SharedInstanceStructs;
	FInstancedStruct* CreatedNode = CreateNode(nullptr, FuncEditorNode, StructDesc, DataHandle, InstancedStructContainer);
	if (CreatedNode == nullptr)
	{
		return false;
	}

	FMetaStoryNodeBase* Function = CreatedNode->GetMutablePtr<FMetaStoryNodeBase>();
	if (Function == nullptr)
	{
		return false;
	}

	const FMetaStoryBindableStructDesc* BindableStruct = BindingsCompiler.GetSourceStructDescByID(FuncEditorNode.ID);
	check(BindableStruct);

	FMetaStoryDataView InstanceView;
	check(InstancedStructContainer.IsValidIndex(Function->InstanceTemplateIndex.Get()));

	FInstancedStruct& Instance = InstancedStructContainer[Function->InstanceTemplateIndex.Get()];
	if (FMetaStoryInstanceObjectWrapper* ObjectWrapper = Instance.GetMutablePtr<FMetaStoryInstanceObjectWrapper>())
	{
		check(FuncEditorNode.InstanceObject->GetClass() == ObjectWrapper->InstanceObject->GetClass());
		InstanceView = FMetaStoryDataView(ObjectWrapper->InstanceObject);
	}
	else
	{
		check(FuncEditorNode.Instance.GetScriptStruct() == Instance.GetScriptStruct());
		InstanceView = FMetaStoryDataView(Instance);
	}

	return CreateBindingsForStruct(*BindableStruct, InstanceView, FMetaStoryIndex16::Invalid, FMetaStoryIndex16::Invalid, Function->BindingsBatch);
}

FInstancedStruct* FMetaStoryCompiler::CreateNode(UMetaStoryState* State, const FMetaStoryEditorNode& EditorNode, FMetaStoryBindableStructDesc& InstanceDesc, const FMetaStoryDataHandle DataHandle, TArray<FInstancedStruct>& InstancedStructContainer)
{
	if (!EditorNode.Node.IsValid())
	{
		return nullptr;
	}

	// Check that item has valid instance initialized.
	if (!EditorNode.Instance.IsValid() && EditorNode.InstanceObject == nullptr)
	{
		Log.Reportf(EMessageSeverity::Error, InstanceDesc,
			TEXT("Malformed node, missing instance value."));
		return nullptr;
	}

	// Copy the node
	IDToNode.Add(EditorNode.ID, Nodes.Num());
	FInstancedStruct& RawNode = Nodes.Add_GetRef(EditorNode.Node);
	InstantiateStructSubobjects(RawNode);

	FMetaStoryNodeBase& Node = RawNode.GetMutable<FMetaStoryNodeBase>();

	TOptional<FMetaStoryDataView> InstanceDataView = CreateNodeInstanceData(EditorNode, Node, InstanceDesc, DataHandle, InstancedStructContainer);
	if (!InstanceDataView.IsSet())
	{
		return nullptr;
	}

	if (!CompileAndValidateNode(State, InstanceDesc, RawNode, InstanceDataView.GetValue()))
	{
		return nullptr;
	}

	CreateBindingSourceStructsForNode(EditorNode, InstanceDesc);

	return &RawNode;
}

FInstancedStruct* FMetaStoryCompiler::CreateNodeWithSharedInstanceData(UMetaStoryState* State, const FMetaStoryEditorNode& EditorNode, FMetaStoryBindableStructDesc& InstanceDesc)
{
	const FMetaStoryDataHandle DataHandle = FMetaStoryDataHandle(EMetaStoryDataSourceType::SharedInstanceData, SharedInstanceStructs.Num());
	return CreateNode(State, EditorNode, InstanceDesc, DataHandle, SharedInstanceStructs);
}

TOptional<FMetaStoryDataView> FMetaStoryCompiler::CreateNodeInstanceData(const FMetaStoryEditorNode& EditorNode, FMetaStoryNodeBase& Node, FMetaStoryBindableStructDesc& StructDesc, const FMetaStoryDataHandle DataHandle, TArray<FInstancedStruct>& InstancedStructContainer)
{
	FMetaStoryDataView InstanceDataView;

	// Update Node name as description for runtime.
	Node.Name = EditorNode.GetName();

	if (EditorNode.Instance.IsValid())
	{
		if (ensure(EditorNode.Instance.GetScriptStruct() == Node.GetInstanceDataType()))
		{
			// Struct Instance
			const int32 InstanceIndex = InstancedStructContainer.Add(EditorNode.Instance);
			InstantiateStructSubobjects(InstancedStructContainer[InstanceIndex]);

			// Create binding source struct descriptor.
			StructDesc.Struct = EditorNode.Instance.GetScriptStruct();

			if (const auto Validation = UE::MetaStory::Compiler::IsValidIndex16(InstanceIndex); Validation.DidFail())
			{
				Validation.Log(Log, TEXT("InstanceIndex"), StructDesc);
				return {};
			}
			Node.InstanceTemplateIndex = FMetaStoryIndex16(InstanceIndex);
			Node.InstanceDataHandle = DataHandle;
			InstanceDataView = FMetaStoryDataView(InstancedStructContainer[InstanceIndex]);
		}
		else
		{
			Log.Reportf(EMessageSeverity::Error, StructDesc, TEXT("The instance data type does not match."));
			return {};
		}
	}
	else if (EditorNode.InstanceObject != nullptr)
	{
		if (ensure(EditorNode.InstanceObject->GetClass() == Node.GetInstanceDataType()))
		{
			UObject* Instance = UE::MetaStory::Compiler::DuplicateInstanceObject(Log, StructDesc, EditorNode.ID, EditorNode.InstanceObject, MetaStory);

			FInstancedStruct Wrapper;
			Wrapper.InitializeAs<FMetaStoryInstanceObjectWrapper>(Instance);
			const int32 InstanceIndex = InstancedStructContainer.Add(MoveTemp(Wrapper));

			// Create binding source struct descriptor.
			StructDesc.Struct = Instance->GetClass();

			if (const auto Validation = UE::MetaStory::Compiler::IsValidIndex16(InstanceIndex); Validation.DidFail())
			{
				Validation.Log(Log, TEXT("InstanceIndex"), StructDesc);
				return {};
			}
			Node.InstanceTemplateIndex = FMetaStoryIndex16(InstanceIndex);
			Node.InstanceDataHandle = DataHandle.ToObjectSource();
			InstanceDataView = FMetaStoryDataView(Instance);
		}
		else
		{
			Log.Reportf(EMessageSeverity::Error, StructDesc, TEXT("The instance data type does not match."));
			return {};
		}
	}
	else if (Node.GetInstanceDataType() != nullptr)
	{
		Log.Reportf(EMessageSeverity::Error, StructDesc, TEXT("The instance data is missing."));
		return {};
	}

	StructDesc.DataHandle = Node.InstanceDataHandle;

	if (EditorNode.ExecutionRuntimeData.IsValid() || EditorNode.ExecutionRuntimeDataObject != nullptr)
	{
		if (EditorNode.ExecutionRuntimeData.IsValid())
		{
			if (ensure(EditorNode.ExecutionRuntimeData.GetScriptStruct() == Node.GetExecutionRuntimeDataType()))
			{
				// Struct Instance
				const int32 InstanceIndex = ExecutionRuntimeStructs.Add(EditorNode.ExecutionRuntimeData);
				InstantiateStructSubobjects(ExecutionRuntimeStructs[InstanceIndex]);

				if (const auto Validation = UE::MetaStory::Compiler::IsValidIndex16(InstanceIndex); Validation.DidFail())
				{
					Validation.Log(Log, TEXT("ExecutionRuntime Index"), StructDesc);
					return {};
				}
				Node.ExecutionRuntimeTemplateIndex = FMetaStoryIndex16(InstanceIndex);
			}
			else
			{
				Log.Reportf(EMessageSeverity::Error, StructDesc, TEXT("The execution runtime data type does not match."));
				return {};
			}
		}
		else if (EditorNode.ExecutionRuntimeDataObject != nullptr)
		{
			if (ensure(EditorNode.ExecutionRuntimeDataObject->GetClass() == Node.GetExecutionRuntimeDataType()))
			{
				// Object Instance
				UObject* Instance = UE::MetaStory::Compiler::DuplicateInstanceObject(Log, StructDesc, EditorNode.ID, EditorNode.ExecutionRuntimeDataObject, MetaStory);

				FInstancedStruct Wrapper;
				Wrapper.InitializeAs<FMetaStoryInstanceObjectWrapper>(Instance);
				const int32 InstanceIndex = ExecutionRuntimeStructs.Add(MoveTemp(Wrapper));

				if (const auto Validation = UE::MetaStory::Compiler::IsValidIndex16(InstanceIndex); Validation.DidFail())
				{
					Validation.Log(Log, TEXT("ExecutionRuntime Index"), StructDesc);
					return {};
				}
				Node.ExecutionRuntimeTemplateIndex = FMetaStoryIndex16(InstanceIndex);
			}
			else
			{
				Log.Reportf(EMessageSeverity::Error, StructDesc, TEXT("The execution runtime data type does not match."));
				return {};
			}
		}
	}
	else if (Node.GetExecutionRuntimeDataType() != nullptr)
	{
		Log.Reportf(EMessageSeverity::Error, StructDesc, TEXT("The execution runtime data is missing."));
		return {};
	}

	return TOptional<FMetaStoryDataView>(InstanceDataView);
}

bool FMetaStoryCompiler::ResolveTransitionStateAndFallback(const UMetaStoryState* SourceState, const FMetaStoryStateLink& Link, FMetaStoryStateHandle& OutTransitionHandle, EMetaStorySelectionFallback& OutFallback) const 
{
	if (Link.LinkType == EMetaStoryTransitionType::GotoState)
	{
		// Warn if goto state points to another subtree.
		if (const UMetaStoryState* TargetState = GetState(Link.ID))
		{
			if (SourceState && TargetState->GetRootState() != SourceState->GetRootState())
			{
				Log.Reportf(EMessageSeverity::Warning,
					TEXT("Target state '%s' is in different subtree. Verify that this is intentional."),
					*Link.Name.ToString());
			}

			if (TargetState->SelectionBehavior == EMetaStoryStateSelectionBehavior::None)
			{
				Log.Reportf(EMessageSeverity::Error,
					TEXT("The target State '%s' is not selectable, it's selection behavior is set to None."),
					*Link.Name.ToString());
				return false;
			}
		}
		
		OutTransitionHandle = GetStateHandle(Link.ID);
		if (!OutTransitionHandle.IsValid())
		{
			Log.Reportf(EMessageSeverity::Error,
				TEXT("Failed to resolve transition to state '%s'."),
				*Link.Name.ToString());
			return false;
		}
	}
	else if (Link.LinkType == EMetaStoryTransitionType::NextState || Link.LinkType == EMetaStoryTransitionType::NextSelectableState)
	{
		// Find next state.
		const UMetaStoryState* NextState = SourceState ? SourceState->GetNextSelectableSiblingState() : nullptr;
		if (NextState == nullptr)
		{
			Log.Reportf(EMessageSeverity::Error,
				TEXT("Failed to resolve transition, there's no selectable next state."));
			return false;
		}
		OutTransitionHandle = GetStateHandle(NextState->ID);
		if (!OutTransitionHandle.IsValid())
		{
			Log.Reportf(EMessageSeverity::Error,
				TEXT("Failed to resolve transition next state, no handle found for '%s'."),
				*NextState->Name.ToString());
			return false;
		}
	}
	else if(Link.LinkType == EMetaStoryTransitionType::Failed)
	{
		OutTransitionHandle = FMetaStoryStateHandle::Failed;
	}
	else if(Link.LinkType == EMetaStoryTransitionType::Succeeded)
	{
		OutTransitionHandle = FMetaStoryStateHandle::Succeeded;
	}
	else if(Link.LinkType == EMetaStoryTransitionType::None)
	{
		OutTransitionHandle = FMetaStoryStateHandle::Invalid;
	}
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	else if (Link.LinkType == EMetaStoryTransitionType::NotSet)
	{
		OutTransitionHandle = FMetaStoryStateHandle::Invalid;
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	if (Link.LinkType == EMetaStoryTransitionType::NextSelectableState)
	{
		OutFallback = EMetaStorySelectionFallback::NextSelectableSibling;
	}
	else
	{
		OutFallback = EMetaStorySelectionFallback::None;
	}

	return true;
}

bool FMetaStoryCompiler::CreateCondition(UMetaStoryState& State, const FString& StatePath, const FMetaStoryEditorNode& CondNode, const EMetaStoryExpressionOperand Operand, const int8 DeltaIndent)
{
	FMetaStoryBindableStructDesc InstanceDesc;
	InstanceDesc.StatePath = StatePath;
	InstanceDesc.ID = CondNode.ID;
	InstanceDesc.Name = CondNode.GetName();
	InstanceDesc.DataSource = EMetaStoryBindableStructSource::Condition;

	const bool bUseEvaluationScopeInstanceData = UE::MetaStory::Compiler::bEnableConditionWithEvaluationScopeInstanceData;
	const FMetaStoryDataHandle DataHandle = bUseEvaluationScopeInstanceData
		? FMetaStoryDataHandle(EMetaStoryDataSourceType::EvaluationScopeInstanceData, EvaluationScopeStructs.Num())
		: FMetaStoryDataHandle(EMetaStoryDataSourceType::SharedInstanceData, SharedInstanceStructs.Num());
	TArray<FInstancedStruct>& InstancedStructContainer = bUseEvaluationScopeInstanceData ? EvaluationScopeStructs : SharedInstanceStructs;
	FInstancedStruct* CreatedNode = CreateNode(&State, CondNode, InstanceDesc, DataHandle, InstancedStructContainer);
	if (CreatedNode == nullptr)
	{
		return false;
	}

	FMetaStoryConditionBase* Cond = CreatedNode->GetMutablePtr<FMetaStoryConditionBase>();
	if (ensure(Cond))
	{
		if (Cond->EvaluationMode == EMetaStoryConditionEvaluationMode::ForcedFalse
			|| Cond->EvaluationMode == EMetaStoryConditionEvaluationMode::ForcedTrue)
		{
			Log.Reportf(EMessageSeverity::Info, InstanceDesc,
					TEXT("The condition result will always be %s."),
					Cond->EvaluationMode == EMetaStoryConditionEvaluationMode::ForcedTrue ? TEXT("True") : TEXT("False"));
		}

		Cond->Operand = Operand;
		Cond->DeltaIndent = DeltaIndent;
		return true;
	}

	return false;
}

bool FMetaStoryCompiler::CreateConsiderations(UMetaStoryState& State, const FString& StatePath, TConstArrayView<FMetaStoryEditorNode> Considerations)
{
	if (State.Considerations.Num() != 0)
	{
		if (!State.Parent
			|| (State.Parent->SelectionBehavior != EMetaStoryStateSelectionBehavior::TrySelectChildrenWithHighestUtility
				&& State.Parent->SelectionBehavior != EMetaStoryStateSelectionBehavior::TrySelectChildrenAtRandomWeightedByUtility))
		{
			Log.Reportf(EMessageSeverity::Warning, TEXT("State's Utility Considerations data are compiled but they don't have effect."
					"The Utility Considerations are used only when parent State's Selection Behavior is:"
					"\"Try Select Children with Highest Utility\" or \"Try Select Children At Random Weighted By Utility\"."));
		}
	}

	for (int32 Index = 0; Index < Considerations.Num(); Index++)
	{
		const bool bIsFirst = Index == 0;
		const FMetaStoryEditorNode& ConsiderationNode = Considerations[Index];
		// First operand should be copy as we dont have a previous item to operate on.
		const EMetaStoryExpressionOperand Operand = bIsFirst ? EMetaStoryExpressionOperand::Copy : ConsiderationNode.ExpressionOperand;
		// First indent must be 0 to make the parentheses calculation match.
		const int32 CurrIndent = bIsFirst ? 0 : FMath::Clamp((int32)ConsiderationNode.ExpressionIndent, 0, UE::MetaStory::MaxExpressionIndent);
		// Next indent, or terminate at zero.
		const int32 NextIndent = Considerations.IsValidIndex(Index + 1) ? FMath::Clamp((int32)Considerations[Index + 1].ExpressionIndent, 0, UE::MetaStory::MaxExpressionIndent) : 0;

		const int32 DeltaIndent = NextIndent - CurrIndent;

		if (!CreateConsideration(State, StatePath, ConsiderationNode, Operand, (int8)DeltaIndent))
		{
			return false;
		}
	}

	return true;
}

bool FMetaStoryCompiler::CreateConsideration(UMetaStoryState& State, const FString& StatePath, const FMetaStoryEditorNode& ConsiderationNode, const EMetaStoryExpressionOperand Operand, const int8 DeltaIndent)
{
	FMetaStoryBindableStructDesc InstanceDesc;
	InstanceDesc.StatePath = StatePath;
	InstanceDesc.ID = ConsiderationNode.ID;
	InstanceDesc.Name = ConsiderationNode.GetName();
	InstanceDesc.DataSource = EMetaStoryBindableStructSource::Consideration;

	const bool bUseEvaluationScopeInstanceData = UE::MetaStory::Compiler::bEnableUtilityConsiderationWithEvaluationScopeInstanceData;
	const FMetaStoryDataHandle DataHandle = bUseEvaluationScopeInstanceData
		? FMetaStoryDataHandle(EMetaStoryDataSourceType::EvaluationScopeInstanceData, EvaluationScopeStructs.Num())
		: FMetaStoryDataHandle(EMetaStoryDataSourceType::SharedInstanceData, SharedInstanceStructs.Num());
	TArray<FInstancedStruct>& InstancedStructContainer = bUseEvaluationScopeInstanceData ? EvaluationScopeStructs : SharedInstanceStructs;
	FInstancedStruct* CreatedNode = CreateNode(&State, ConsiderationNode, InstanceDesc, DataHandle, InstancedStructContainer);
	if (CreatedNode == nullptr)
	{
		return false;
	}

	FMetaStoryConsiderationBase* Consideration = CreatedNode->GetMutablePtr<FMetaStoryConsiderationBase>();
	if (ensure(Consideration))
	{
		Consideration->Operand = Operand;
		Consideration->DeltaIndent = DeltaIndent;
		return true;
	}

	return false;
}

bool FMetaStoryCompiler::CompileAndValidateNode(const UMetaStoryState* SourceState, const FMetaStoryBindableStructDesc& InstanceDesc, FStructView NodeView, const FMetaStoryDataView InstanceData)
{
	if (!NodeView.IsValid())
	{
		return false;
	}
	
	FMetaStoryNodeBase& Node = NodeView.Get<FMetaStoryNodeBase>();
	check(InstanceData.IsValid());

	auto ValidateStateLinks = [this, SourceState](TPropertyValueIterator<FStructProperty> It)
	{
		for ( ; It; ++It)
		{
			if (It->Key->Struct == TBaseStructure<FMetaStoryStateLink>::Get())
			{
				FMetaStoryStateLink& StateLink = *static_cast<FMetaStoryStateLink*>(const_cast<void*>(It->Value));

				if (!ResolveTransitionStateAndFallback(SourceState, StateLink, StateLink.StateHandle, StateLink.Fallback))
				{
					return false;
				}
			}
		}

		return true;
	};
	
	// Validate any state links.
	if (!ValidateStateLinks(TPropertyValueIterator<FStructProperty>(InstanceData.GetStruct(), InstanceData.GetMutableMemory())))
	{
		return false;
	}
	if (!ValidateStateLinks(TPropertyValueIterator<FStructProperty>(NodeView.GetScriptStruct(), NodeView.GetMemory())))
	{
		return false;
	}

	const FMetaStoryBindingLookup& BindingLookup = FMetaStoryBindingLookup(EditorData);
	UE::MetaStory::FCompileNodeContext CompileContext(InstanceData, InstanceDesc, BindingLookup);
	const EDataValidationResult Result = Node.Compile(CompileContext);

	if (Result == EDataValidationResult::Invalid && CompileContext.ValidationErrors.IsEmpty())
	{
		Log.Report(EMessageSeverity::Error, InstanceDesc, TEXT("Node validation failed."));
	}
	else
	{
		const EMessageSeverity::Type Severity = Result == EDataValidationResult::Invalid ? EMessageSeverity::Error : EMessageSeverity::Warning;
		for (const FText& Error : CompileContext.ValidationErrors)
		{
			Log.Report(Severity, InstanceDesc, Error.ToString());
		}
	}

	// Make sure there's no level actor references in the data.
	if (!UE::MetaStory::Compiler::ValidateNoLevelActorReferences(Log, InstanceDesc, NodeView, InstanceData))
	{
		return false;
	}
	
	if (const UClass* InstanceDataClass = Cast<UClass>(InstanceData.GetStruct()); InstanceDataClass && InstanceDataClass->HasAllClassFlags(CLASS_Deprecated))
	{
		Log.Report(EMessageSeverity::Warning, InstanceDesc, TEXT("The instance data class is deprecated. It won't work in a cooked build."));
	}

	return Result != EDataValidationResult::Invalid;
}

bool FMetaStoryCompiler::CreateTask(UMetaStoryState* State, const FMetaStoryEditorNode& TaskNode, const FMetaStoryDataHandle TaskDataHandle)
{
	if (!TaskNode.Node.IsValid())
	{
		return false;
	}
	
	// Create binding source struct descriptor.
	FMetaStoryBindableStructDesc InstanceDesc;
	InstanceDesc.StatePath = State ? State->GetPath() : UE::MetaStory::Editor::GlobalStateName;
	InstanceDesc.ID = TaskNode.ID;
	InstanceDesc.Name = TaskNode.GetName();
	InstanceDesc.DataSource = EMetaStoryBindableStructSource::Task;

	// Check that node has valid instance initialized.
	if (!TaskNode.Instance.IsValid() && TaskNode.InstanceObject == nullptr)
	{
		Log.Reportf(EMessageSeverity::Error, InstanceDesc,
			TEXT("Malformed task, missing instance value."));
		return false;
	}

	// Copy the task
	IDToNode.Add(TaskNode.ID, Nodes.Num());
	FInstancedStruct& Node = Nodes.Add_GetRef(TaskNode.Node);
	InstantiateStructSubobjects(Node);
	
	FMetaStoryTaskBase& Task = Node.GetMutable<FMetaStoryTaskBase>();
	TOptional<FMetaStoryDataView> InstanceDataView = CreateNodeInstanceData(TaskNode, Task, InstanceDesc, TaskDataHandle, InstanceStructs);
	if (!InstanceDataView.IsSet())
	{
		return false;
	}

	if (!Task.bTaskEnabled)
	{
		Log.Reportf(EMessageSeverity::Info, InstanceDesc, TEXT("Task is disabled and will have no effect."));
	}

	if (!CompileAndValidateNode(State, InstanceDesc, Node,  InstanceDataView.GetValue()))
	{
		return false;
	}

	CreateBindingSourceStructsForNode(TaskNode, InstanceDesc);

	return true;
}

bool FMetaStoryCompiler::CreateEvaluator(const FMetaStoryEditorNode& EvalNode, const FMetaStoryDataHandle EvalDataHandle)
{
	// Silently ignore empty nodes.
	if (!EvalNode.Node.IsValid())
	{
		return true;
	}

	// Create binding source descriptor for instance.
	FMetaStoryBindableStructDesc InstanceDesc;
	InstanceDesc.StatePath = UE::MetaStory::Editor::GlobalStateName;
    InstanceDesc.ID = EvalNode.ID;
	InstanceDesc.Name = EvalNode.GetName();
	InstanceDesc.DataSource = EMetaStoryBindableStructSource::Evaluator;

    // Check that node has valid instance initialized.
    if (!EvalNode.Instance.IsValid() && EvalNode.InstanceObject == nullptr)
    {
        Log.Reportf(EMessageSeverity::Error, InstanceDesc,
        	TEXT("Malformed evaluator, missing instance value."));
        return false;
    }

	// Copy the evaluator
	IDToNode.Add(EvalNode.ID, Nodes.Num());
	FInstancedStruct& Node = Nodes.Add_GetRef(EvalNode.Node);
	InstantiateStructSubobjects(Node);
	
	FMetaStoryEvaluatorBase& Eval = Node.GetMutable<FMetaStoryEvaluatorBase>();
	TOptional<FMetaStoryDataView> InstanceDataView = CreateNodeInstanceData(EvalNode, Eval, InstanceDesc, EvalDataHandle, InstanceStructs);
	if (!InstanceDataView.IsSet())
	{
		return false;
	}

	if (!CompileAndValidateNode(nullptr, InstanceDesc, Node,  InstanceDataView.GetValue()))
	{
		return false;
	}

	CreateBindingSourceStructsForNode(EvalNode, InstanceDesc);

	return true;
}

bool FMetaStoryCompiler::ValidateStructRef(const FMetaStoryBindableStructDesc& SourceStruct, const FPropertyBindingPath& SourcePath,
											const FMetaStoryBindableStructDesc& TargetStruct, const FPropertyBindingPath& TargetPath) const
{
	FString ResolveError;
	TArray<FPropertyBindingPathIndirection> TargetIndirection;
	if (!TargetPath.ResolveIndirections(TargetStruct.Struct, TargetIndirection, &ResolveError))
	{
		// This will later be reported by the bindings compiler.
		Log.Reportf(EMessageSeverity::Error, TargetStruct, TEXT("Failed to resolve binding path in %s: %s"), *TargetStruct.ToString(), *ResolveError);
		return false;
	}
	const FProperty* TargetLeafProperty = TargetIndirection.Num() > 0 ? TargetIndirection.Last().GetProperty() : nullptr;

	// Early out if the target is not FMetaStoryStructRef.
	const FStructProperty* TargetStructProperty = CastField<FStructProperty>(TargetLeafProperty);
	if (TargetStructProperty == nullptr || TargetStructProperty->Struct != TBaseStructure<FMetaStoryStructRef>::Get())
	{
		return true;
	}

	FString TargetBaseStructName;
	const UScriptStruct* TargetBaseStruct = UE::MetaStory::Compiler::GetBaseStructFromMetaData(TargetStructProperty, TargetBaseStructName);
	if (TargetBaseStruct == nullptr)
	{
		Log.Reportf(EMessageSeverity::Error, TargetStruct,
				TEXT("Could not find base struct type '%s' for target %s'."),
				*TargetBaseStructName, *UE::MetaStory::GetDescAndPathAsString(TargetStruct, TargetPath));
		return false;
	}

	TArray<FPropertyBindingPathIndirection> SourceIndirection;
	if (!SourcePath.ResolveIndirections(SourceStruct.Struct, SourceIndirection, &ResolveError))
	{
		// This will later be reported by the bindings compiler.
		Log.Reportf(EMessageSeverity::Error, SourceStruct, TEXT("Failed to resolve binding path in %s: %s"), *SourceStruct.ToString(), *ResolveError);
		return false;
	}
	const FProperty* SourceLeafProperty = SourceIndirection.Num() > 0 ? SourceIndirection.Last().GetProperty() : nullptr;

	// Exit if the source is not a struct property.
	const FStructProperty* SourceStructProperty = CastField<FStructProperty>(SourceLeafProperty);
	if (SourceStructProperty == nullptr)
	{
		return true;
	}
	
	if (SourceStructProperty->Struct == TBaseStructure<FMetaStoryStructRef>::Get())
	{
		// Source is struct ref too, check the types match.
		FString SourceBaseStructName;
		const UScriptStruct* SourceBaseStruct = UE::MetaStory::Compiler::GetBaseStructFromMetaData(SourceStructProperty, SourceBaseStructName);
		if (SourceBaseStruct == nullptr)
		{
			Log.Reportf(EMessageSeverity::Error, TargetStruct,
					TEXT("Could not find base struct '%s' for binding source %s."),
					*SourceBaseStructName, *UE::MetaStory::GetDescAndPathAsString(SourceStruct, SourcePath));
			return false;
		}

		if (SourceBaseStruct->IsChildOf(TargetBaseStruct) == false)
		{
			Log.Reportf(EMessageSeverity::Error, TargetStruct,
						TEXT("Type mismatch between source %s and target %s types, '%s' is not child of '%s'."),
						*UE::MetaStory::GetDescAndPathAsString(SourceStruct, SourcePath),
						*UE::MetaStory::GetDescAndPathAsString(TargetStruct, TargetPath),
						*GetNameSafe(SourceBaseStruct), *GetNameSafe(TargetBaseStruct));
			return false;
		}
	}
	else
	{
		if (!SourceStructProperty->Struct || SourceStructProperty->Struct->IsChildOf(TargetBaseStruct) == false)
		{
			Log.Reportf(EMessageSeverity::Error, TargetStruct,
						TEXT("Type mismatch between source %s and target %s types, '%s' is not child of '%s'."),
						*UE::MetaStory::GetDescAndPathAsString(SourceStruct, SourcePath),
						*UE::MetaStory::GetDescAndPathAsString(TargetStruct, TargetPath),
						*GetNameSafe(SourceStructProperty->Struct), *GetNameSafe(TargetBaseStruct));
			return false;
		}
	}

	return true;
}

bool FMetaStoryCompiler::ValidateBindingOnNode(const FMetaStoryBindableStructDesc& TargetStruct, const FPropertyBindingPath& TargetPath) const
{
	FString ResolveError;
	TArray<FPropertyBindingPathIndirection> TargetIndirection;
	if (!TargetPath.ResolveIndirections(TargetStruct.Struct, TargetIndirection, &ResolveError))
	{
		// This will later be reported by the bindings compiler.
		Log.Reportf(EMessageSeverity::Error, TargetStruct, TEXT("Failed to resolve binding path in %s: %s"), *TargetStruct.ToString(), *ResolveError);
		return false;
	}

	check(TargetStruct.Struct);
	if (TargetStruct.Struct->IsChildOf<FMetaStoryNodeBase>())
	{
		if (!UE::MetaStory::Compiler::IsPropertyChildOfAnyStruct<FMetaStoryPropertyRef, FMetaStoryDelegateListener>(TargetStruct, TargetPath))
		{
			Log.Reportf(EMessageSeverity::Error, TargetStruct,
				TEXT("Node struct(non-instance) only supports DelegateListener and PropertyReference as binding targets. %s is not supported."),
				*UE::MetaStory::GetDescAndPathAsString(TargetStruct, TargetPath));

			return false;
		}
	}

	return true;
}

bool FMetaStoryCompiler::GetAndValidateBindings(const FMetaStoryBindableStructDesc& TargetStruct, FMetaStoryDataView TargetValue, FValidatedPathBindings& OutValidatedBindings) const
{
	using namespace UE::MetaStory::Compiler;

	check(EditorData);
	
	OutValidatedBindings = FValidatedPathBindings();

	// If target struct is not set, nothing to do.
	if (TargetStruct.Struct == nullptr)
	{
		return true;
	}

	bool bSucceeded = true;

	// Handle sources. Need to handle them now while we have the instance.
	for (FMetaStoryPropertyPathBinding& Binding : EditorData->EditorBindings.GetMutableBindings())
	{
		if (Binding.GetSourcePath().GetStructID() == TargetStruct.ID)
		{
			if (IsPropertyChildOfAnyStruct<FMetaStoryDelegateDispatcher>(TargetStruct, Binding.GetSourcePath()))
			{
				FMetaStoryPropertyPathBinding& BindingCopy = OutValidatedBindings.DelegateDispatchers.Add_GetRef(Binding);
				BindingCopy.SetSourceDataHandle(TargetStruct.DataHandle);
			}
		}
	}

	// Handle targets.
	for (FMetaStoryPropertyPathBinding& Binding : EditorData->EditorBindings.GetMutableBindings())
	{
		if (Binding.GetTargetPath().GetStructID() != TargetStruct.ID)
		{
			continue;
		}

		const FGuid SourceStructID = Binding.GetSourcePath().GetStructID();
		const FMetaStoryBindableStructDesc* SourceStruct = BindingsCompiler.GetSourceStructDescByID(SourceStructID);

		// Validate and try to fix up binding paths to be up to date
		// @todo: check if Delegate and PropertyRef is put on Instance Data and throw warning if so
		// @todo: not liking how this mutates the Binding.TargetPath, but currently we dont track well the instanced object changes.
		{
			// Source must be one of the source structs we have discovered in the tree.
			if (!SourceStruct)
			{
				Log.Reportf(EMessageSeverity::Error, TargetStruct,
					TEXT("Failed to find binding source property '%s' for target %s."),
					*Binding.GetSourcePath().ToString(), *UE::MetaStory::GetDescAndPathAsString(TargetStruct, Binding.GetTargetPath()));
				bSucceeded = false;
				continue;
			}

			// Source must be accessible by the target struct via all execution paths.
			TArray<TInstancedStruct<FPropertyBindingBindableStructDescriptor>> AccessibleStructs;
			EditorData->GetBindableStructs(Binding.GetTargetPath().GetStructID(), AccessibleStructs);

			const bool bSourceAccessible = AccessibleStructs.ContainsByPredicate([SourceStructID](TConstStructView<FPropertyBindingBindableStructDescriptor> Struct)
				{
					return (Struct.Get().ID == SourceStructID);
				});

			if (!bSourceAccessible)
			{
				TInstancedStruct<FPropertyBindingBindableStructDescriptor> SourceStructDescriptor;
				const bool bFoundSourceStructDescriptor = EditorData->GetBindableStructByID(SourceStructID, SourceStructDescriptor);
				if (bFoundSourceStructDescriptor
					&& SourceStructDescriptor.Get<FMetaStoryBindableStructDesc>().DataSource == EMetaStoryBindableStructSource::Task
					&& !UE::MetaStory::AcceptTaskInstanceData(TargetStruct.DataSource))
				{
					Log.Reportf(EMessageSeverity::Error, TargetStruct,
						TEXT("Property at %s cannot be bound to %s, because the binding source %s is a task instance data that is possibly not instantiated before %s in the tree."),
						*UE::MetaStory::GetDescAndPathAsString(*SourceStruct, Binding.GetSourcePath()),
						*UE::MetaStory::GetDescAndPathAsString(TargetStruct, Binding.GetTargetPath()),
						*SourceStruct->ToString(), *TargetStruct.ToString());
				}
				else
				{
					Log.Reportf(EMessageSeverity::Error, TargetStruct,
						TEXT("Property at %s cannot be bound to %s, because the binding source %s is not updated before %s in the tree."),
						*UE::MetaStory::GetDescAndPathAsString(*SourceStruct, Binding.GetSourcePath()),
						*UE::MetaStory::GetDescAndPathAsString(TargetStruct, Binding.GetTargetPath()),
						*SourceStruct->ToString(), *TargetStruct.ToString());
				}
				bSucceeded = false;
				continue;
			}

			if (!IDToStructValue.Contains(SourceStructID))
			{
				Log.Reportf(EMessageSeverity::Error, TargetStruct,
					TEXT("Failed to find value for binding source property '%s' for target %s."),
					*Binding.GetSourcePath().ToString(), *UE::MetaStory::GetDescAndPathAsString(TargetStruct, Binding.GetTargetPath()));
				bSucceeded = false;
				continue;
			}

			// Update the source structs only if we have value for it. For some sources (e.g. context structs) we know only type, and in that case there are no instance structs.
			const FMetaStoryDataView SourceValue = IDToStructValue[SourceStructID];
			if (SourceValue.GetStruct() != nullptr)
			{
				if (!Binding.GetMutableSourcePath().UpdateSegmentsFromValue(SourceValue))
				{
					Log.Reportf(EMessageSeverity::Error, TargetStruct,
						TEXT("Malformed target property path for binding source property '%s' for source %s."),
						*Binding.GetSourcePath().ToString(), *UE::MetaStory::GetDescAndPathAsString(TargetStruct, Binding.GetTargetPath()));
					bSucceeded = false;
					continue;
				}
			}

			if (SourceStruct->Struct && !SourceStruct->Struct->IsChildOf<FMetaStoryNodeBase>())
			{
				if (!SourceStruct->DataHandle.IsValid())
				{
					Log.Reportf(EMessageSeverity::Error, TargetStruct,
						TEXT("Malformed source'%s for property binding property '%s'."),
						*UE::MetaStory::GetDescAndPathAsString(*SourceStruct, Binding.GetSourcePath()), *Binding.GetSourcePath().ToString());
					bSucceeded = false;
					continue;
				}
			}

			// Update path instance types from latest data. E.g. binding may have been created for instanced object of type FooB, and changed to FooA.
			if (!Binding.GetMutableTargetPath().UpdateSegmentsFromValue(TargetValue))
			{
				Log.Reportf(EMessageSeverity::Error, TargetStruct,
					TEXT("Malformed target property path for binding source property '%s' for target %s."),
					*Binding.GetSourcePath().ToString(), *UE::MetaStory::GetDescAndPathAsString(TargetStruct, Binding.GetTargetPath()));
				bSucceeded = false;
				continue;
			}

			TArray<FPropertyBindingPathIndirection, TInlineAllocator<16>> Indirections;
			const bool bResolveSucceeded = Binding.GetMutableTargetPath().ResolveIndirections(TargetStruct.Struct, Indirections);
			check(bResolveSucceeded);

			if (Indirections.IsEmpty())
			{
				Log.Reportf(EMessageSeverity::Error, TargetStruct,
					TEXT("Malformed target property path. Target Struct can not be bound directly."));

				bSucceeded = false;
				continue;
			}

			const FProperty* TargetStructRootProperty = Indirections[0].GetProperty();
			check(TargetStructRootProperty);

			// Fix Binding output flag based on target path's root property type
			const bool bIsOutputBinding = Binding.IsOutputBinding();
			if (UE::MetaStory::GetUsageFromMetaData(TargetStructRootProperty) == EMetaStoryPropertyUsage::Output)
			{
				if (!bIsOutputBinding)
				{
					Log.Reportf(EMessageSeverity::Info, TargetStruct,
						TEXT("Automatically fixed Binding to be output one because target property %s is output property."),
						*UE::MetaStory::GetDescAndPathAsString(TargetStruct, Binding.GetTargetPath()));

					constexpr bool bOutputBinding = true;
					Binding.SetIsOutputBinding(bOutputBinding);
				}

				if (IsPropertyChildOfAnyStruct<FMetaStoryDelegateDispatcher, FMetaStoryDelegateListener, FMetaStoryPropertyRef>(*SourceStruct, Binding.GetSourcePath()))
				{
					Log.Reportf(EMessageSeverity::Error, TargetStruct,
						TEXT("Property at %s cannot be bound to %s. Output binding doesn't accept Delegate or PropertyReference as source property."),
						*UE::MetaStory::GetDescAndPathAsString(TargetStruct, Binding.GetTargetPath()), 
						*UE::MetaStory::GetDescAndPathAsString(TargetStruct, Binding.GetSourcePath()));
					
					bSucceeded = false;
					continue;
				}
			}
			else if (bIsOutputBinding)
			{
				Log.Reportf(EMessageSeverity::Info, TargetStruct,
					TEXT("Automatically fixed Binding to be non output one because target property %s is not output property."),
					*UE::MetaStory::GetDescAndPathAsString(TargetStruct, Binding.GetTargetPath()));

				constexpr bool bOutputBinding = false;
				Binding.SetIsOutputBinding(bOutputBinding);
			}
		}

		TArray<FMetaStoryPropertyPathBinding>* OutputBindings = nullptr;
		if (IsPropertyChildOfAnyStruct<FMetaStoryDelegateListener>(TargetStruct, Binding.GetTargetPath()))
		{
			OutputBindings = &OutValidatedBindings.DelegateListeners;
		}
		else if (IsPropertyChildOfAnyStruct<FMetaStoryPropertyRef>(TargetStruct, Binding.GetTargetPath()))
		{
			OutputBindings = &OutValidatedBindings.ReferenceBindings;
		}
		else if (Binding.IsOutputBinding())
		{
			OutputBindings = &OutValidatedBindings.OutputCopyBindings;
		}
		else
		{
			OutputBindings = &OutValidatedBindings.CopyBindings;
		}

		check(OutputBindings);

		FMetaStoryPropertyPathBinding& BindingCopy = OutputBindings->Add_GetRef(Binding);
		BindingCopy.SetSourceDataHandle(SourceStruct->DataHandle);

		// Special case for AnyEnum. MetaStoryBindingExtension allows AnyEnums to bind to other enum types.
		// The actual copy will be done via potential type promotion copy, into the value property inside the AnyEnum.
		// We amend the paths here to point to the 'Value' property.
		const bool bSourceIsAnyEnum = IsPropertyChildOfAnyStruct<FMetaStoryAnyEnum>(*SourceStruct, Binding.GetSourcePath());
		const bool bTargetIsAnyEnum = IsPropertyChildOfAnyStruct<FMetaStoryAnyEnum>(TargetStruct, Binding.GetTargetPath());
		if (bSourceIsAnyEnum || bTargetIsAnyEnum)
		{
			if (bSourceIsAnyEnum)
			{
				BindingCopy.GetMutableSourcePath().AddPathSegment(GET_MEMBER_NAME_STRING_CHECKED(FMetaStoryAnyEnum, Value));
			}
			if (bTargetIsAnyEnum)
			{
				BindingCopy.GetMutableTargetPath().AddPathSegment(GET_MEMBER_NAME_STRING_CHECKED(FMetaStoryAnyEnum, Value));
			}
		}

		// Check if the bindings is for struct ref and validate the types.
		if (!ValidateStructRef(*SourceStruct, Binding.GetSourcePath(), TargetStruct, Binding.GetTargetPath()))
		{
			bSucceeded = false;
			continue;
		}

		if (!ValidateBindingOnNode(TargetStruct, Binding.GetTargetPath()))
		{
			bSucceeded = false;
			continue;
		}
	}

	if (!bSucceeded)
	{
		return false;
	}

	auto IsPropertyBound = [](const FName& PropertyName, TConstArrayView<FMetaStoryPropertyPathBinding> Bindings)
	{
		return Bindings.ContainsByPredicate([&PropertyName](const FMetaStoryPropertyPathBinding& Binding)
			{
				// We're looping over just the first level of properties on the struct, so we assume that the path is just one item
				// (or two in case of AnyEnum, because we expand the path to Property.Value, see code above).
				return Binding.GetTargetPath().GetSegments().Num() >= 1 && Binding.GetTargetPath().GetSegments()[0].GetName() == PropertyName;
			});
	};
	
	// Validate that Input and Context bindings
	for (TFieldIterator<FProperty> It(TargetStruct.Struct); It; ++It)
	{
		const FProperty* Property = *It;
		check(Property);
		const FName PropertyName = Property->GetFName();

		if (UE::MetaStory::PropertyRefHelpers::IsPropertyRef(*Property))
		{
			TArray<FPropertyBindingPathIndirection> TargetIndirections;
			FPropertyBindingPath TargetPath(TargetStruct.ID, PropertyName);
			if (!TargetPath.ResolveIndirectionsWithValue(TargetValue, TargetIndirections))
			{
				Log.Reportf(EMessageSeverity::Error, TargetStruct,
						TEXT("Couldn't resolve path to '%s' for target %s."),
						*PropertyName.ToString(), *TargetStruct.ToString());
				bSucceeded = false;
				continue;
			}
			else
			{
				const void* PropertyRef = TargetIndirections.Last().GetPropertyAddress();
				const bool bIsOptional = UE::MetaStory::PropertyRefHelpers::IsPropertyRefMarkedAsOptional(*Property, PropertyRef);

				if (bIsOptional == false && !IsPropertyBound(PropertyName, OutValidatedBindings.ReferenceBindings))
				{
					Log.Reportf(EMessageSeverity::Error, TargetStruct,
						TEXT("Property reference '%s' on % s is expected to have a binding."),
						*PropertyName.ToString(), *TargetStruct.ToString());
					bSucceeded = false;
					continue;
				}
			}
		}
		else
		{
			const bool bIsOptional = UE::MetaStory::PropertyHelpers::HasOptionalMetadata(*Property);
			const EMetaStoryPropertyUsage Usage = UE::MetaStory::GetUsageFromMetaData(Property);
			if (Usage == EMetaStoryPropertyUsage::Input)
			{
				// Make sure that an Input property is bound unless marked optional.
				if (bIsOptional == false
					&& !IsPropertyBound(PropertyName, OutValidatedBindings.CopyBindings)
					&& !IsPropertyBound(PropertyName, OutValidatedBindings.DelegateListeners))
				{
					Log.Reportf(EMessageSeverity::Error, TargetStruct,
						TEXT("Input property '%s' on %s is expected to have a binding."),
						*PropertyName.ToString(), *TargetStruct.ToString());
					bSucceeded = false;
					continue;
				}
			}
			else if (Usage == EMetaStoryPropertyUsage::Context)
			{
				// Make sure a Context property is manually or automatically bound.
				const UStruct* ContextObjectType = nullptr; 
				if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
				{
					ContextObjectType = StructProperty->Struct;
				}		
				else if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
				{
					ContextObjectType = ObjectProperty->PropertyClass;
				}

				if (ContextObjectType == nullptr)
				{
					Log.Reportf(EMessageSeverity::Error, TargetStruct,
						TEXT("The type of Context property '%s' on %s is expected to be Object Reference or Struct."),
						*PropertyName.ToString(), *TargetStruct.ToString());
					bSucceeded = false;
					continue;
				}

				const bool bIsBound = IsPropertyBound(PropertyName, OutValidatedBindings.CopyBindings);

				if (!bIsBound)
				{
					const FMetaStoryBindableStructDesc Desc = EditorData->FindContextData(ContextObjectType, PropertyName.ToString());

					if (Desc.IsValid())
					{
						// Add automatic binding to Context data.
						constexpr bool bIsOutputBinding = false;
						OutValidatedBindings.CopyBindings.Emplace(FPropertyBindingPath(Desc.ID), FPropertyBindingPath(TargetStruct.ID, PropertyName), bIsOutputBinding);
					}
					else
					{
						Log.Reportf(EMessageSeverity::Error, TargetStruct,
							TEXT("Could not find matching Context object for Context property '%s' on '%s'. Property must have manual binding."),
							*PropertyName.ToString(), *TargetStruct.ToString());
						bSucceeded = false;
						continue;
					}
				}
			}
		}
	}

	return bSucceeded;
}

void FMetaStoryCompiler::InstantiateStructSubobjects(FStructView Struct)
{
	check(MetaStory);
	check(EditorData);
	
	// Empty struct, nothing to do.
	if (!Struct.IsValid())
	{
		return;
	}

	for (TPropertyValueIterator<FProperty> It(Struct.GetScriptStruct(), Struct.GetMemory()); It; ++It)
	{
		if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(It->Key))
		{
			// Duplicate instanced objects.
			if (ObjectProperty->HasAnyPropertyFlags(CPF_InstancedReference))
			{
				if (UObject* Object = ObjectProperty->GetObjectPropertyValue(It->Value))
				{
					UObject* OuterObject = Object->GetOuter();
					// If the instanced object was created as Editor Data as outer,
					// change the outer to State Tree to prevent references to editor only data.
					if (Object->IsInOuter(EditorData))
					{
						OuterObject = MetaStory;
					}
					UObject* DuplicatedObject = DuplicateObject(Object, OuterObject);
					ObjectProperty->SetObjectPropertyValue(const_cast<void*>(It->Value), DuplicatedObject);
				}
			}
		}
	}
}

bool FMetaStoryCompiler::NotifyInternalPost()
{
	struct FPostInternalContextImpl : UE::MetaStory::Compiler::FPostInternalContext
	{
		virtual TNotNull<const UMetaStory*> GetStateTree() const override
		{
			return MetaStory;
		}
		virtual TNotNull<const UMetaStoryEditorData*> GetEditorData() const override
		{
			return EditorData;
		}

		virtual FMetaStoryCompilerLog& GetLog() const override
		{
			return Log;
		}

		virtual UMetaStoryExtension* AddExtension(TNotNull<TSubclassOf<UMetaStoryExtension>> ExtensionType) const override
		{
			UClass* ExtensionClass = static_cast<TSubclassOf<UMetaStoryExtension>>(ExtensionType).Get();
			UMetaStoryExtension* Result = NewObject<UMetaStoryExtension>(MetaStory.Get(), ExtensionClass);
			MetaStory->Extensions.Add(Result);
			return Result;
		}

		FPostInternalContextImpl(TNotNull<UMetaStory*> InStateTree, TNotNull<UMetaStoryEditorData*> InEditorData, FMetaStoryCompilerLog& InLog)
			: MetaStory(InStateTree)
			, EditorData(InEditorData)
			, Log(InLog)
		{ }

		TObjectPtr<UMetaStory> MetaStory = nullptr;
		TObjectPtr<UMetaStoryEditorData> EditorData = nullptr;
		FMetaStoryCompilerLog& Log;
	};

	FPostInternalContextImpl Context = FPostInternalContextImpl(MetaStory, EditorData, Log);

	// Notify the module, schema, and extension. Use that order to go from the less specific to the more specific.
	FMetaStoryEditorModule::GetModule().OnPostInternalCompile().Broadcast(Context);

	if (EditorData->EditorSchema)
	{
		if (!EditorData->EditorSchema->HandlePostInternalCompile(Context))
		{
			Log.Reportf(EMessageSeverity::Error, TEXT("The schema failed compilation. See log for more info."));
			return false;
		}
	}

	for (UMetaStoryEditorDataExtension* Extension : EditorData->Extensions)
	{
		if (Extension)
		{
			if (!Extension->HandlePostInternalCompile(Context))
			{
				Log.Reportf(EMessageSeverity::Error, TEXT("An extension failed compilation. See log for more info."));
				return false;
			}
		}
	}

	return true;
}

void FMetaStoryCompiler::CreateBindingSourceStructsForNode(const FMetaStoryEditorNode& EditorNode, const FMetaStoryBindableStructDesc& InstanceDesc)
{
	using namespace UE::MetaStory::Compiler;

	check(EditorNode.Node.IsValid());

	BindingsCompiler.AddSourceStruct(InstanceDesc);

	if (IsNodeStructEligibleForBinding(EditorNode.GetNode()))
	{
		FMetaStoryBindableStructDesc NodeDesc = InstanceDesc;
		NodeDesc.ID = EditorNode.GetNodeID();
		NodeDesc.Struct = EditorNode.Node.GetScriptStruct();
		NodeDesc.DataHandle = FMetaStoryDataHandle::Invalid;	// DataHandle is only for Instance data

		BindingsCompiler.AddSourceStruct(NodeDesc);
	}
}
