// Copyright Epic Games, Inc. All Rights Reserved.

#include "AsyncMetaStoryDiff.h"
#include "DiffUtils.h"
#include "SMetaStoryView.h"
#include "MetaStoryDiffHelper.h"
#include "MetaStoryEditorData.h"
#include "MetaStoryState.h"
#include "MetaStoryViewModel.h"

namespace UE::MetaStory::Diff
{

static bool AreObjectsEqual(const UObject* ObjectA, const UObject* ObjectB)
{
	if (!ObjectA || !ObjectB)
	{
		return ObjectA == ObjectB;
	}

	if (ObjectA->GetClass() != ObjectB->GetClass())
	{
		return false;
	}

	if (ObjectA != ObjectB)
	{
		const FProperty* ClassProperty = ObjectA->GetClass()->PropertyLink;
		while (ClassProperty)
		{
			if (!ClassProperty->Identical_InContainer(ObjectA, ObjectB, /*ArrayIndex*/ 0, PPF_DeepComparison | PPF_ForDiff))
			{
				return false;
			}
			ClassProperty = ClassProperty->PropertyLinkNext;
		}
	}

	return true;
}

static bool AreNodesEqual(const FMetaStoryEditorNode& NodeA, const FMetaStoryEditorNode& NodeB)
{
	return AreObjectsEqual(NodeA.InstanceObject.Get(), NodeB.InstanceObject.Get())
		&& AreObjectsEqual(NodeA.ExecutionRuntimeDataObject.Get(), NodeB.ExecutionRuntimeDataObject.Get())
		&& NodeA.Node.Identical(&NodeB.Node, PPF_DeepComparison | PPF_ForDiff)
		&& NodeA.Instance.Identical(&NodeB.Instance, PPF_DeepComparison | PPF_ForDiff)
		&& NodeA.ExecutionRuntimeData.Identical(&NodeB.ExecutionRuntimeData, PPF_DeepComparison | PPF_ForDiff)
		&& NodeA.ExpressionIndent == NodeB.ExpressionIndent
		&& NodeA.ExpressionOperand == NodeB.ExpressionOperand;
}

static bool AreNodeArraysEqual(const TArray<FMetaStoryEditorNode>& ArrayA, const TArray<FMetaStoryEditorNode>& ArrayB)
{
	const int32 Count = ArrayA.Num();
	if (Count != ArrayB.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < Count; Index++)
	{
		if (!AreNodesEqual(ArrayA[Index], ArrayB[Index]))
		{
			return false;
		}
	}
	return true;
}
static bool AreMetaStoryStatePropertyBagsEqual(const FInstancedPropertyBag& ParametersA, const FInstancedPropertyBag& ParametersB)
{
	if (ParametersA.GetNumPropertiesInBag() != ParametersB.GetNumPropertiesInBag())
	{
		return false;
	}
	
	const UPropertyBag* BagA = ParametersA.GetPropertyBagStruct();
	const UPropertyBag* BagB = ParametersB.GetPropertyBagStruct();
	if (!BagA || !BagB)
	{
		return BagA == BagB;
	}

	const TConstArrayView<FPropertyBagPropertyDesc> DescsA = BagA->GetPropertyDescs();
	const TConstArrayView<FPropertyBagPropertyDesc> DescsB = BagB->GetPropertyDescs();
	const int32 Count = DescsA.Num();
	for (int32 Index = 0; Index < Count; Index++)
	{
		if (DescsA[Index].Name != DescsB[Index].Name
			|| !DescsA[Index].CompatibleType(DescsB[Index]))
		{
			return false;
		}

		const FName Name = DescsA[Index].Name;
		TValueOrError<FString, EPropertyBagResult> SerializedA = ParametersA.GetValueSerializedString(Name);
		TValueOrError<FString, EPropertyBagResult> SerializedB = ParametersB.GetValueSerializedString(Name);
		if (SerializedA.HasError() || SerializedB.HasError())
		{
			return false;
		}

		if (SerializedA.HasValue() != SerializedB.HasValue())
		{
			return false;
		}

		if (SerializedA.GetValue() != SerializedB.GetValue())
		{
			return false;
		}
	}
	return true;
}

static bool AreMetaStoryStateParametersEqual(const FMetaStoryStateParameters& ParametersA, const FMetaStoryStateParameters& ParametersB)
{
	if (!AreMetaStoryStatePropertyBagsEqual(ParametersA.Parameters, ParametersB.Parameters))
	{
		return false;
	}
	if (ParametersA.PropertyOverrides != ParametersB.PropertyOverrides)
	{
		return false;
	}

	return true;
}

static bool ArePropertiesEqual(const UMetaStoryState* StateA, const UMetaStoryState* StateB)
{
	if (!StateA || !StateB)
	{
		return StateA == StateB;
	}

	return StateA->Name == StateB->Name
		&& StateA->Tag == StateB->Tag
		&& StateA->ColorRef == StateB->ColorRef
		&& StateA->Type == StateB->Type
		&& StateA->SelectionBehavior == StateB->SelectionBehavior;
}

static bool AreParametersEqual(const UMetaStoryState* StateA, const UMetaStoryState* StateB)
{
	return AreMetaStoryStateParametersEqual(StateA->Parameters, StateB->Parameters);
}

static bool AreConditionsEqual(const UMetaStoryState* StateA, const UMetaStoryState* StateB)
{
	return AreNodeArraysEqual(StateA->EnterConditions, StateB->EnterConditions);
}

static bool AreConsiderationsEqual(const UMetaStoryState* StateA, const UMetaStoryState* StateB)
{
	return AreNodeArraysEqual(StateA->Considerations, StateB->Considerations);
}

static bool AreTasksEqual(const UMetaStoryState* StateA, const UMetaStoryState* StateB)
{
	return AreNodeArraysEqual(StateA->Tasks, StateB->Tasks);
}

static bool AreTransitionsEqual(const UMetaStoryState* StateA, const UMetaStoryState* StateB)
{
	if (StateA->Transitions.Num() != StateB->Transitions.Num())
	{
		return false;
	}

	for (int32 Index = 0; Index < StateA->Transitions.Num(); Index++)
	{
		const FMetaStoryTransition* TransitionA = &StateA->Transitions[Index];
		const FMetaStoryTransition* TransitionB = &StateB->Transitions[Index];
		if (!TransitionA || !TransitionB)
		{
			return TransitionA == TransitionB;
		}
		// Not checking transitions on IDs
		const bool bEqual = FMetaStoryTransition::StaticStruct()->CompareScriptStruct(TransitionA, TransitionB, 0);
		if (!bEqual)
		{
			return false;
		}
	}
	return true;
}

static bool AreMetaStoryPropertiesEqual(const UMetaStoryEditorData* MetaStoryDataA, const UMetaStoryEditorData* MetaStoryDataB)
{
	// Check the differences in Bindings
	bool bBindingsEqual = MetaStoryDataA->EditorBindings.GetBindings().Num() == MetaStoryDataB->EditorBindings.GetBindings().Num();
	if (bBindingsEqual)
	{
		for (const FPropertyBindingBinding& PropertyPathBinding : MetaStoryDataA->EditorBindings.GetBindings())
		{
			const FPropertyBindingPath& PropertyPathTarget = PropertyPathBinding.GetTargetPath();
			if (MetaStoryDataB->EditorBindings.HasBinding(PropertyPathBinding.GetTargetPath()))
			{
				if (*MetaStoryDataA->EditorBindings.GetBindingSource(PropertyPathTarget) != *MetaStoryDataB->EditorBindings.GetBindingSource(PropertyPathTarget))
				{
					bBindingsEqual = false;
					break;
				}
			}
			else
			{
				bBindingsEqual = false;
				break;
			}
		}
	}
	if (!bBindingsEqual)
	{
		return false;
	}

	// Check the differences in Evaluators and Tasks
	if (!AreNodeArraysEqual(MetaStoryDataA->Evaluators, MetaStoryDataB->Evaluators))
	{
		return false;
	}
	if (!AreNodeArraysEqual(MetaStoryDataA->GlobalTasks, MetaStoryDataB->GlobalTasks))
	{
		return false;
	}

	// Check the differences in MetaStory root level parameters
	if (!AreMetaStoryStatePropertyBagsEqual(MetaStoryDataA->GetRootParametersPropertyBag(), MetaStoryDataB->GetRootParametersPropertyBag()))
	{
		return false;
	}
	return true;
}

static FPropertySoftPath GetPropertyPath(const FPropertyBindingPath& MetaStoryPropertyPath, const UMetaStoryState* MetaStoryState)
{
	TArray<FName> Path;
	auto CheckNodes = [&MetaStoryPropertyPath, &Path](const TArray<FMetaStoryEditorNode>& List, FName PathSegmentName)
	{
		for (int i = 0; i < List.Num(); i++)
		{
			if (List[i].ID == MetaStoryPropertyPath.GetStructID())
			{
				Path.Add(PathSegmentName);
				Path.Add(FName(FString::FromInt(i)));
				if (List[i].InstanceObject)
				{
					Path.Add(FName("InstanceObject"));
				}
				else
				{
					Path.Add(FName("Instance"));
				}
				return true;
			}
		}
		return false;
	};
	auto CheckTransitions = [MetaStoryPropertyPath, &Path](const TArray<FMetaStoryTransition>& List, FName PathSegmentName)
	{
		for (int i = 0; i < List.Num(); i++)
		{
			if (List[i].ID == MetaStoryPropertyPath.GetStructID())
			{
				Path.Add(PathSegmentName);
				Path.Add(FName(FString::FromInt(i)));
				return true;
			}
		}
		return false;
	};
	if (CheckNodes(MetaStoryState->EnterConditions, "EnterConditions")
		|| CheckNodes(MetaStoryState->Tasks, "Tasks")
		|| CheckTransitions(MetaStoryState->Transitions, "Transitions"))
	{
		for (const FPropertyBindingPathSegment& PropertySegment : MetaStoryPropertyPath.GetSegments())
		{
			Path.Add(PropertySegment.GetName());
		}
	}

	return FPropertySoftPath(Path);
}

static void GetBindingsDifferences(UMetaStoryEditorData* MetaStoryDataA, UMetaStoryEditorData* MetaStoryDataB, TArray<FSingleDiffEntry>& OutDiffEntries)
{
	struct FBindingDiff
	{
		FPropertyBindingPath TargetPath;
		FPropertyBindingPath SourcePathA;
		FPropertyBindingPath SourcePathB;
	};

	TArray<FBindingDiff> BindingDiffs;
	// Check the differences in Bindings
	for (const FPropertyBindingBinding& PropertyPathBinding : MetaStoryDataA->EditorBindings.GetBindings())
	{
		FPropertyBindingPath PropertyPathTarget = PropertyPathBinding.GetTargetPath();
		FPropertyBindingPath PropertyPathSource = PropertyPathBinding.GetSourcePath();

		FBindingDiff Entry;
		Entry.TargetPath = PropertyPathTarget;
		Entry.SourcePathA = PropertyPathSource;
		BindingDiffs.Add(Entry);
	}
	for (const FPropertyBindingBinding& PropertyPathBinding : MetaStoryDataB->EditorBindings.GetBindings())
	{
		FPropertyBindingPath PropertyPathTarget = PropertyPathBinding.GetTargetPath();
		FPropertyBindingPath PropertyPathSource = PropertyPathBinding.GetSourcePath();
		bool bFound = false;
		for (FBindingDiff& Diff : BindingDiffs)
		{
			if (Diff.TargetPath == PropertyPathTarget)
			{
				Diff.SourcePathB = PropertyPathSource;
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			FBindingDiff Entry;
			Entry.TargetPath = PropertyPathTarget;
			Entry.SourcePathB = PropertyPathSource;
			BindingDiffs.Add(Entry);
		}
	}

	for (const FBindingDiff& DiffEntry : BindingDiffs)
	{
		if (DiffEntry.SourcePathA != DiffEntry.SourcePathB)
		{
			const UMetaStoryState* TargetStateA = MetaStoryDataA->GetStateByStructID(DiffEntry.TargetPath.GetStructID());
			const UMetaStoryState* TargetStateB = MetaStoryDataB->GetStateByStructID(DiffEntry.TargetPath.GetStructID());

			if (TargetStateA && TargetStateB)
			{
				FStateSoftPath StatePathA(TargetStateA);
				FStateSoftPath StatePathB(TargetStateB);
				FPropertySoftPath PropertyPath = GetPropertyPath(DiffEntry.TargetPath, TargetStateA);

				EStateDiffType MetaStoryDiffType = EStateDiffType::BindingChanged;
				if (DiffEntry.SourcePathA.IsPathEmpty())
				{
					MetaStoryDiffType = EStateDiffType::BindingAddedToB;
				}
				else if (DiffEntry.SourcePathB.IsPathEmpty())
				{
					MetaStoryDiffType = EStateDiffType::BindingAddedToA;
				}
				OutDiffEntries.Add(FSingleDiffEntry(StatePathA, StatePathB, MetaStoryDiffType, PropertyPath));
			}
		}
	}
}

FAsyncDiff::FAsyncDiff(const TSharedRef<SMetaStoryView>& LeftTree, const TSharedRef<SMetaStoryView>& RightTree)
	: TAsyncTreeDifferences(RootNodesAttribute(LeftTree), RootNodesAttribute(RightTree))
	, LeftView(LeftTree)
	, RightView(RightTree)
{}

TAttribute<TArray<TWeakObjectPtr<UMetaStoryState>>> FAsyncDiff::RootNodesAttribute(TWeakPtr<SMetaStoryView> MetaStoryView)
{
	return TAttribute<TArray<TWeakObjectPtr<UMetaStoryState>>>::CreateLambda([MetaStoryView]()
	{
		if (const TSharedPtr<SMetaStoryView> TreeView = StaticCastSharedPtr<SMetaStoryView>(MetaStoryView.Pin()))
		{
			TArray<TWeakObjectPtr<UMetaStoryState>> SubTrees;
			TreeView->GetViewModel()->GetSubTrees(SubTrees);
			return SubTrees;
		}
		return TArray<TWeakObjectPtr<UMetaStoryState>>();
	});
}

void FAsyncDiff::GetStatesDifferences(TArray<FSingleDiffEntry>& OutDiffEntries) const
{
	TArray<FString> RemovedStates;
	TArray<FString> AddedStates;
	ForEach(ETreeTraverseOrder::PreOrder, [&](const TUniquePtr<DiffNodeType>& Node) -> ETreeTraverseControl
	{
		FStateSoftPath StatePath;
		UMetaStoryState* LeftState = Node->ValueA.Get();
		UMetaStoryState* RightState = Node->ValueB.Get();
		if (LeftState)
		{
			StatePath = FStateSoftPath(LeftState);
		}
		else if (RightState)
		{
			StatePath = FStateSoftPath(RightState);
		}

		EStateDiffType MetaStoryDiffType = EStateDiffType::Invalid;
		bool bSkipChildren = false;
		switch (Node->DiffResult)
		{
		case ETreeDiffResult::MissingFromTree1:
			MetaStoryDiffType = EStateDiffType::StateAddedToB;
			AddedStates.Add(StatePath.ToDisplayName(true));
			if (RemovedStates.Contains(StatePath.ToDisplayName(true)))
			{
				MetaStoryDiffType = EStateDiffType::StateMoved;
			}
			bSkipChildren = true;
			break;
		case ETreeDiffResult::MissingFromTree2:
			MetaStoryDiffType = EStateDiffType::StateAddedToA;
			RemovedStates.Add(StatePath.ToDisplayName(true));
			if (AddedStates.Contains(StatePath.ToDisplayName(true)))
			{
				MetaStoryDiffType = EStateDiffType::StateMoved;
			}
			bSkipChildren = true;
			break;
		case ETreeDiffResult::DifferentValues:
			MetaStoryDiffType = EStateDiffType::StateChanged;
			break;
		case ETreeDiffResult::Identical:
			MetaStoryDiffType = EStateDiffType::Identical;
			if (LeftState && RightState)
			{
				if (LeftState->bEnabled != RightState->bEnabled)
				{
					MetaStoryDiffType = RightState->bEnabled ? EStateDiffType::StateEnabled : EStateDiffType::StateDisabled;
				}
			}
			break;
		default:
			return ETreeTraverseControl::Continue;
		}

		if (MetaStoryDiffType == EStateDiffType::Identical)
		{
			return ETreeTraverseControl::Continue;
		}

		if (MetaStoryDiffType == EStateDiffType::StateMoved)
		{
			for (FSingleDiffEntry& DiffEntry : OutDiffEntries)
			{
				if (DiffEntry.Identifier.ToDisplayName(true) == StatePath.ToDisplayName(true))
				{
					if (DiffEntry.DiffType == EStateDiffType::StateAddedToA)
					{
						DiffEntry.SecondaryIdentifier = StatePath;
					}
					else
					{
						DiffEntry.SecondaryIdentifier = DiffEntry.Identifier;
						DiffEntry.Identifier = StatePath;
					}
					DiffEntry.DiffType = EStateDiffType::StateMoved;

					// For now, we are skipping children, we may need to revisit that
					return ETreeTraverseControl::SkipChildren;
				}
			}
		}

		OutDiffEntries.Add(FSingleDiffEntry(StatePath, MetaStoryDiffType));
		
		return bSkipChildren ? ETreeTraverseControl::SkipChildren : ETreeTraverseControl::Continue;
	});
}

void FAsyncDiff::GetMetaStoryDifferences(TArray<FSingleDiffEntry>& OutDiffEntries) const
{
	if (LeftView && RightView)
	{
		const FMetaStoryViewModel* LeftViewModel = LeftView->GetViewModel().Get();
		const FMetaStoryViewModel* RightViewModel = RightView->GetViewModel().Get();
		if (LeftViewModel && RightViewModel)
		{
			UMetaStoryEditorData* LeftEditorData = Cast<UMetaStoryEditorData>(LeftViewModel->GetMetaStory()->EditorData);
			UMetaStoryEditorData* RightEditorData = Cast<UMetaStoryEditorData>(RightViewModel->GetMetaStory()->EditorData);
			if (!AreMetaStoryPropertiesEqual(LeftEditorData, RightEditorData))
			{
				OutDiffEntries.Add(FSingleDiffEntry(
					/*Identifier*/FStateSoftPath(),
					EStateDiffType::MetaStoryPropertiesChanged));
			}

			GetStatesDifferences(OutDiffEntries);

			GetBindingsDifferences(LeftEditorData, RightEditorData, OutDiffEntries);
		}
	}
}

} // UE::AsyncMetaStoryDiff


bool TTreeDiffSpecification<TWeakObjectPtr<UMetaStoryState>>::AreValuesEqual(const TWeakObjectPtr<UMetaStoryState>& MetaStoryNodeA, const TWeakObjectPtr<UMetaStoryState>& MetaStoryNodeB, TArray<FPropertySoftPath>*) const
{
	const TStrongObjectPtr<UMetaStoryState> StrongStateA = MetaStoryNodeA.Pin();
	const TStrongObjectPtr<UMetaStoryState> StrongStateB = MetaStoryNodeB.Pin();
	const UMetaStoryState* StateA = StrongStateA.Get();
	const UMetaStoryState* StateB = StrongStateB.Get();

	if (!StateA || !StateB)
	{
		return StateA == StateB;
	}

	using namespace UE::MetaStory::Diff;
	return ArePropertiesEqual(StateA, StateB)
		&& AreParametersEqual(StateA, StateB)
		&& AreConditionsEqual(StateA, StateB)
		&& AreTasksEqual(StateA, StateB)
		&& AreTransitionsEqual(StateA, StateB)
		&& AreConsiderationsEqual(StateA, StateB);
}

bool TTreeDiffSpecification<TWeakObjectPtr<UMetaStoryState>>::AreMatching(const TWeakObjectPtr<UMetaStoryState>& MetaStoryNodeA, const TWeakObjectPtr<UMetaStoryState>& MetaStoryNodeB, TArray<FPropertySoftPath>*) const
{
	const TStrongObjectPtr<UMetaStoryState> StrongStateA = MetaStoryNodeA.Pin();
	const TStrongObjectPtr<UMetaStoryState> StrongStateB = MetaStoryNodeB.Pin();
	const UMetaStoryState* StateA = StrongStateA.Get();
	const UMetaStoryState* StateB = StrongStateB.Get();
	if (!StateA || !StateB)
	{
		return StateA == StateB;
	}

	return StateA->ID == StateB->ID;
}

void TTreeDiffSpecification<TWeakObjectPtr<UMetaStoryState>>::GetChildren(const TWeakObjectPtr<UMetaStoryState>& InParent, TArray<TWeakObjectPtr<UMetaStoryState>>& OutChildren) const
{
	const TStrongObjectPtr<UMetaStoryState> StrongParent = InParent.Pin();
	if (UMetaStoryState* InParentPtr = StrongParent.Get())
	{
		OutChildren.Reserve(InParentPtr->Children.Num());
		for (const TObjectPtr<UMetaStoryState>& Child : InParentPtr->Children)
		{
			OutChildren.Add(Child);
		}
	}
}