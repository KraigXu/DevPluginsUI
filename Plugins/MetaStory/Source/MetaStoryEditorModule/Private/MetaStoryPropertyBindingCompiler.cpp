// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetaStoryPropertyBindingCompiler.h"
#include "IPropertyAccessEditor.h"
#include "PropertyPathHelpers.h"
#include "MetaStoryCompilerLog.h"
#include "MetaStoryEditorPropertyBindings.h"
#include "Misc/EnumerateRange.h"
#include "MetaStoryPropertyBindings.h"
#include "MetaStoryPropertyRef.h"
#include "MetaStoryPropertyRefHelpers.h"
#include "MetaStoryPropertyHelpers.h"
#include "MetaStoryDelegate.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryPropertyBindingCompiler)

bool FMetaStoryPropertyBindingCompiler::Init(FMetaStoryPropertyBindings& InPropertyBindings, FMetaStoryCompilerLog& InLog)
{
	Log = &InLog;
	PropertyBindings = &InPropertyBindings;
	PropertyBindings->Reset();

	SourceStructs.Reset();
	
	return true;
}

bool FMetaStoryPropertyBindingCompiler::CompileBatch(const FMetaStoryBindableStructDesc& TargetStruct, TConstArrayView<FMetaStoryPropertyPathBinding> BatchPropertyBindings, FMetaStoryIndex16 PropertyFuncsBegin, FMetaStoryIndex16 PropertyFuncsEnd, int32& OutBatchIndex)
{
	check(Log);
	check(PropertyBindings);
	OutBatchIndex = INDEX_NONE;

	StoreSourceStructs();

	struct FSortedBinding
	{
		FMetaStoryPropertyPathBinding Binding;
		TArray<FPropertyBindingPathIndirection> TargetIndirections;
	};
	TArray<FSortedBinding> NewBindings;

	for (const FMetaStoryPropertyPathBinding& Binding : BatchPropertyBindings)
	{
		if (Binding.GetTargetPath().GetStructID() != TargetStruct.ID)
		{
			continue;
		}
		// Source must be in the source array
		const FMetaStoryBindableStructDesc* SourceStruct = GetSourceStructDescByID(Binding.GetSourcePath().GetStructID());
		if (!SourceStruct)
		{
			Log->Reportf(EMessageSeverity::Error, TargetStruct,
				TEXT("Could not find a binding source."));
			return false;
		}

		FString Error;
		TArray<FPropertyBindingPathIndirection> SourceIndirections;
		TArray<FPropertyBindingPathIndirection> TargetIndirections;
		
		if (!Binding.GetSourcePath().ResolveIndirections(SourceStruct->Struct, SourceIndirections, &Error))
		{
			Log->Reportf(EMessageSeverity::Error, TargetStruct, TEXT("Resolving path in %s: %s"), *SourceStruct->ToString(), *Error);
			return false;
		}

		if (!Binding.GetTargetPath().ResolveIndirections(TargetStruct.Struct, TargetIndirections, &Error))
		{
			Log->Reportf(EMessageSeverity::Error, TargetStruct, TEXT("Resolving path in %s: %s"), *TargetStruct.ToString(), *Error);
			return false;
		}

		FPropertyBindingCopyInfo DummyCopy;
		FPropertyBindingPathIndirection LastSourceIndirection = !SourceIndirections.IsEmpty() ? SourceIndirections.Last() : FPropertyBindingPathIndirection(SourceStruct->Struct);
		FPropertyBindingPathIndirection LastTargetIndirection = !TargetIndirections.IsEmpty() ? TargetIndirections.Last() : FPropertyBindingPathIndirection(TargetStruct.Struct);
		if (!PropertyBindings->ResolveBindingCopyInfo(Binding, LastSourceIndirection, LastTargetIndirection, DummyCopy))
		{
			Log->Reportf(EMessageSeverity::Error, TargetStruct,
			TEXT("Cannot copy properties between %s and %s, properties are incompatible."),
				*UE::MetaStory::GetDescAndPathAsString(*SourceStruct, Binding.GetSourcePath()),
				*UE::MetaStory::GetDescAndPathAsString(TargetStruct, Binding.GetTargetPath()));
			return false;
		}

		FSortedBinding& NewBinding = NewBindings.AddDefaulted_GetRef();
		NewBinding.Binding = FMetaStoryPropertyPathBinding(SourceStruct->DataHandle, Binding.GetSourcePath(), Binding.GetTargetPath(), Binding.IsOutputBinding());
		NewBinding.TargetIndirections = MoveTemp(TargetIndirections);
	}

	if (!NewBindings.IsEmpty())
	{
		// Sort bindings base on copy target memory layout.
		NewBindings.StableSort([](const FSortedBinding& A, const FSortedBinding& B)
		{
			const int32 MaxSegments = FMath::Min(A.TargetIndirections.Num(), B.TargetIndirections.Num());
			for (int32 Index = 0; Index < MaxSegments; Index++)
			{
				// If property A is in struct before B, copy A first. 
				if (A.TargetIndirections[Index].GetPropertyOffset() < B.TargetIndirections[Index].GetPropertyOffset())
				{
					return true;
				}
				if (A.TargetIndirections[Index].GetPropertyOffset() > B.TargetIndirections[Index].GetPropertyOffset())
				{
					return false;
				}

				// If A and B points to the same property, choose the one that points to an earlier array item.
				// Note: this assumes that INDEX_NONE = -1, which means that binding directly to an array comes before an array access,
				// and non-array access will compare equal (both INDEX_NONE).
				if (A.TargetIndirections[Index].GetArrayIndex() < B.TargetIndirections[Index].GetArrayIndex())
				{
					return true;
				}
				if (A.TargetIndirections[Index].GetArrayIndex() > B.TargetIndirections[Index].GetArrayIndex())
				{
					return false;
				}
			}

			// We get here if the common path is the same, shorter path wins.
			return A.TargetIndirections.Num() <= B.TargetIndirections.Num(); 
		});

		// Store bindings batch.
		const int32 BindingsBegin = PropertyBindings->PropertyPathBindings.Num();
		for (const FSortedBinding& NewBinding : NewBindings)
		{
			PropertyBindings->PropertyPathBindings.Add(NewBinding.Binding);
		}
		const int32 BindingsEnd = PropertyBindings->PropertyPathBindings.Num();

		FPropertyBindingCopyInfoBatch& Batch = PropertyBindings->AddCopyBatch();
		Batch.TargetStruct = TInstancedStruct<FMetaStoryBindableStructDesc>::Make(TargetStruct);
		Batch.BindingsBegin = FPropertyBindingIndex16(BindingsBegin);
		Batch.BindingsEnd = FPropertyBindingIndex16(BindingsEnd);
		Batch.PropertyFunctionsBegin = PropertyFuncsBegin;
		Batch.PropertyFunctionsEnd = PropertyFuncsEnd;
		OutBatchIndex = PropertyBindings->GetNumCopyBatches() - 1;
	}

	return true;
}

bool FMetaStoryPropertyBindingCompiler::CompileDelegateDispatchers(
	const FMetaStoryBindableStructDesc& SourceStruct,
	TConstArrayView<FMetaStoryEditorDelegateDispatcherCompiledBinding> PreviousCompiledDistpatchers,
	TConstArrayView<FMetaStoryPropertyPathBinding> DelegateDispatcherBindings,
	FMetaStoryDataView InstanceDataView)
{
	check(Log);
	check(PropertyBindings);

	StoreSourceStructs();

	bool bSuccess = true;
	for (const FMetaStoryPropertyPathBinding& Binding : DelegateDispatcherBindings)
	{
		if (Binding.GetSourcePath().GetStructID() != SourceStruct.ID)
		{
			continue;
		}

		// Source must be in the source array
		const FMetaStoryBindableStructDesc* DispatcherStruct = GetSourceStructDescByID(Binding.GetSourcePath().GetStructID());
		if (!DispatcherStruct)
		{
			Log->Reportf(EMessageSeverity::Error, SourceStruct, TEXT("Could not find a binding source."));
			bSuccess = false;
			continue;
		}

		auto FindBySourcePathPredicate = [&Binding](const FMetaStoryEditorDelegateDispatcherCompiledBinding& Other)
			{
				return Other.DispatcherPath == Binding.GetSourcePath();
			};

		if (!CompiledDelegateDispatchers.ContainsByPredicate(FindBySourcePathPredicate))
		{
			FString Error;
			TArray<FPropertyBindingPathIndirection> DispatcherIndirections;
			if (!Binding.GetSourcePath().ResolveIndirectionsWithValue(InstanceDataView, DispatcherIndirections, &Error))
			{
				Log->Reportf(EMessageSeverity::Error, SourceStruct, TEXT("Resolving path in %s: %s"), *DispatcherStruct->ToString(), *Error);
				bSuccess = false;
				continue;
			}

			const FPropertyBindingPathIndirection& DispatcherLeafIndirection = DispatcherIndirections.Last();
			const FStructProperty* LeafAsStructProperty = CastField<FStructProperty>(DispatcherLeafIndirection.GetProperty());
			if (LeafAsStructProperty == nullptr || LeafAsStructProperty->Struct != FMetaStoryDelegateDispatcher::StaticStruct())
			{
				Log->Reportf(EMessageSeverity::Error, SourceStruct, TEXT("The source is not a valid delegate dispatcher."));
				bSuccess = false;
				continue;
			}

			if (DispatcherLeafIndirection.GetContainerAddress() == nullptr)
			{
				Log->Reportf(EMessageSeverity::Error, SourceStruct, TEXT("The dispatcher can't be initialized."));
				bSuccess = false;
				continue;
			}

			FMetaStoryDelegateDispatcher* Dispatcher = reinterpret_cast<FMetaStoryDelegateDispatcher*>(DispatcherLeafIndirection.GetMutablePropertyAddress());
			if (Dispatcher == nullptr)
			{
				Log->Reportf(EMessageSeverity::Error, SourceStruct, TEXT("The dispatcher can't be initialized."));
				bSuccess = false;
				continue;
			}

			if (const FMetaStoryEditorDelegateDispatcherCompiledBinding* PreviousCompiled = PreviousCompiledDistpatchers.FindByPredicate(FindBySourcePathPredicate))
			{
				// Reuse previous ID
				*Dispatcher = PreviousCompiled->ID;
			}
			else
			{
				Dispatcher->ID = FGuid::NewGuid();
			}

			const bool bFoundID = CompiledDelegateDispatchers.ContainsByPredicate([ID = Dispatcher->ID](const FMetaStoryEditorDelegateDispatcherCompiledBinding& Other)
				{
					return Other.ID.ID == ID;
				});
			ensureMsgf(!bFoundID, TEXT("The ID is already used by another delegate dispatcher."));
			CompiledDelegateDispatchers.Add({ Binding.GetSourcePath(), *Dispatcher });
		}
	}

	return bSuccess;
}
	
bool FMetaStoryPropertyBindingCompiler::CompileDelegateListeners(
	const FMetaStoryBindableStructDesc& TargetStruct,
	TConstArrayView<FMetaStoryPropertyPathBinding> DelegateSourceBindings,
	FMetaStoryDataView InstanceDataView)
{
	check(Log);
	check(PropertyBindings);

	StoreSourceStructs();

	bool bSuccess = true;

	for (const FMetaStoryPropertyPathBinding& Binding : DelegateSourceBindings)
	{
		if (Binding.GetTargetPath().GetStructID() != TargetStruct.ID)
		{
			continue;
		}

		// Source must be in the source array
		const FMetaStoryBindableStructDesc* DispatcherStruct = GetSourceStructDescByID(Binding.GetSourcePath().GetStructID());
		if (!DispatcherStruct)
		{
			Log->Reportf(EMessageSeverity::Error, TargetStruct, TEXT("Could not find a binding source."));
			bSuccess = false;
			continue;
		}

		FString Error;
		TArray<FPropertyBindingPathIndirection> ListenerIndirections;
		if (!Binding.GetTargetPath().ResolveIndirectionsWithValue(InstanceDataView, ListenerIndirections, &Error))
		{
			Log->Reportf(EMessageSeverity::Error, TargetStruct, TEXT("Resolving path in %s: %s"), *TargetStruct.ToString(), *Error);
			bSuccess = false;
			continue;
		}

		const FMetaStoryDelegateDispatcher Dispatcher = GetDispatcherFromPath(Binding.GetSourcePath());
		if (!Dispatcher.IsValid())
		{
			Log->Reportf(EMessageSeverity::Error, TargetStruct, TEXT("Delegate Listener %s is bound to unknown dispatcher %s"), *TargetStruct.ToString(), *DispatcherStruct->ToString());
			bSuccess = false;
			continue;
		}

		FPropertyBindingPathIndirection& ListenerLeafIndirection = ListenerIndirections.Last();
		const FStructProperty* LeafAsStructProperty = CastField<FStructProperty>(ListenerLeafIndirection.GetProperty());
		check(LeafAsStructProperty && LeafAsStructProperty->Struct == FMetaStoryDelegateListener::StaticStruct());
		if (LeafAsStructProperty == nullptr || LeafAsStructProperty->Struct != FMetaStoryDelegateListener::StaticStruct())
		{
			Log->Reportf(EMessageSeverity::Error, TargetStruct, TEXT("The target is not a valid delegate listener."));
			bSuccess = false;
			continue;
		}

		FMetaStoryDelegateListener* Listener = reinterpret_cast<FMetaStoryDelegateListener*>(ListenerLeafIndirection.GetMutablePropertyAddress());
		Listener->Dispatcher = Dispatcher;
		Listener->ID = ++ListenersNum;
	}

	return bSuccess;
}

bool FMetaStoryPropertyBindingCompiler::CompileReferences(const FMetaStoryBindableStructDesc& TargetStruct, TConstArrayView<FMetaStoryPropertyPathBinding> PropertyReferenceBindings, FMetaStoryDataView InstanceDataView, const TMap<FGuid, const FMetaStoryDataView>& IDToStructValue)
{
	for (const FMetaStoryPropertyPathBinding& Binding : PropertyReferenceBindings)
	{
		if (Binding.GetTargetPath().GetStructID() != TargetStruct.ID)
		{
			continue;
		}

		// Source must be in the source array
		const FMetaStoryBindableStructDesc* SourceStruct = GetSourceStructDescByID(Binding.GetSourcePath().GetStructID());
		if (!SourceStruct)
		{
			Log->Reportf(EMessageSeverity::Error, TargetStruct,
				TEXT("Could not find a binding source."));
			return false;
		}

		const FMetaStoryDataView* SourceDataView = IDToStructValue.Find(Binding.GetSourcePath().GetStructID());
		if (!SourceDataView)
		{
			Log->Reportf(EMessageSeverity::Error, TargetStruct,
				TEXT("Could not find a binding source data view."));
			return false;
		}

		FString Error;
		TArray<FPropertyBindingPathIndirection> SourceIndirections;
		if (!Binding.GetSourcePath().ResolveIndirectionsWithValue(*SourceDataView, SourceIndirections, &Error))
		{
			Log->Reportf(EMessageSeverity::Error, TargetStruct, TEXT("Resolving path in %s: %s"), *SourceStruct->ToString(), *Error);
			return false;
		}

		if (!UE::MetaStory::PropertyRefHelpers::IsPropertyAccessibleForPropertyRef(SourceIndirections, *SourceStruct))
		{
			Log->Reportf(EMessageSeverity::Error, TargetStruct,
					TEXT("%s cannot reference non-output or property function output %s "),
					*UE::MetaStory::GetDescAndPathAsString(TargetStruct, Binding.GetTargetPath()),
					*UE::MetaStory::GetDescAndPathAsString(*SourceStruct, Binding.GetSourcePath()));
			return false;
		}

		TArray<FPropertyBindingPathIndirection> TargetIndirections;
		if (!Binding.GetTargetPath().ResolveIndirectionsWithValue(InstanceDataView, TargetIndirections, &Error))
		{
			Log->Reportf(EMessageSeverity::Error, TargetStruct, TEXT("Resolving path in %s: %s"), *TargetStruct.ToString(), *Error);
			return false;
		}

		FPropertyBindingPathIndirection& TargetLeafIndirection = TargetIndirections.Last();
		FMetaStoryPropertyRef* PropertyRef = static_cast<FMetaStoryPropertyRef*>(const_cast<void*>(TargetLeafIndirection.GetPropertyAddress()));
		check(PropertyRef);

		FPropertyBindingPathIndirection& SourceLeafIndirection = SourceIndirections.Last();
		if (!UE::MetaStory::PropertyRefHelpers::IsPropertyRefCompatibleWithProperty(*TargetLeafIndirection.GetProperty(), *SourceLeafIndirection.GetProperty(), PropertyRef, SourceLeafIndirection.GetPropertyAddress()))
		{
			Log->Reportf(EMessageSeverity::Error, TargetStruct,
				TEXT("%s cannot reference %s, types are incompatible."),		
				*UE::MetaStory::GetDescAndPathAsString(TargetStruct, Binding.GetTargetPath()),
				*UE::MetaStory::GetDescAndPathAsString(*SourceStruct, Binding.GetSourcePath()));
			return false;
		}

		FMetaStoryIndex16 ReferenceIndex;

		// Reuse the index if another PropertyRef already references the same property.
		{
			int32 IndexOfAlreadyExisting = PropertyBindings->PropertyReferencePaths.IndexOfByPredicate([&Binding](const FMetaStoryPropertyRefPath& RefPath)
			{
				return RefPath.GetSourcePath() == Binding.GetSourcePath();
			});

			if (IndexOfAlreadyExisting != INDEX_NONE)
			{
				ReferenceIndex = FMetaStoryIndex16(IndexOfAlreadyExisting);
			}
		}

		if (!ReferenceIndex.IsValid())
		{
			// If referencing another non global or subtree parameter PropertyRef, reuse it's index.
			if (UE::MetaStory::PropertyRefHelpers::IsPropertyRef(*SourceIndirections.Last().GetProperty())
				&& SourceStruct->DataHandle.GetSource() != EMetaStoryDataSourceType::GlobalParameterData
				&& SourceStruct->DataHandle.GetSource() != EMetaStoryDataSourceType::ExternalGlobalParameterData
				&& SourceStruct->DataHandle.GetSource() != EMetaStoryDataSourceType::SubtreeParameterData)
			{
				const FCompiledReference* ReferencedReference = CompiledReferences.FindByPredicate([&Binding](const FCompiledReference& CompiledReference)
				{
					return CompiledReference.Path == Binding.GetSourcePath();
				});

				if (ReferencedReference)
				{
					ReferenceIndex = ReferencedReference->Index;
				}
				else
				{
					if(!UE::MetaStory::PropertyHelpers::HasOptionalMetadata(*TargetLeafIndirection.GetProperty()))
					{
						Log->Reportf(EMessageSeverity::Error, TargetStruct, TEXT("Referenced %s is not bound"), *UE::MetaStory::GetDescAndPathAsString(*SourceStruct, Binding.GetSourcePath()));
						return false;
					}
							
					return true;
				}
			}
		}

		if (!ReferenceIndex.IsValid())
		{
			ReferenceIndex = FMetaStoryIndex16(PropertyBindings->PropertyReferencePaths.Num());
			PropertyBindings->PropertyReferencePaths.Emplace(SourceStruct->DataHandle, Binding.GetSourcePath());
		}

		// Store index in instance data.
		PropertyRef->RefAccessIndex = ReferenceIndex;

		FCompiledReference& CompiledReference = CompiledReferences.AddDefaulted_GetRef();
		CompiledReference.Path = Binding.GetTargetPath();
		CompiledReference.Index = ReferenceIndex;
	}

	return true;
}

void FMetaStoryPropertyBindingCompiler::Finalize()
{
	StoreSourceStructs();

	CompiledDelegateDispatchers.Reset();
	CompiledReferences.Reset();
}

int32 FMetaStoryPropertyBindingCompiler::AddSourceStruct(const FMetaStoryBindableStructDesc& SourceStruct)
{
	const FMetaStoryBindableStructDesc* ExistingStruct = SourceStructs.FindByPredicate([&SourceStruct](const FMetaStoryBindableStructDesc& Struct) { return (Struct.ID == SourceStruct.ID); });
	if (ExistingStruct)
	{
		UE_LOG(LogMetaStory, Error, TEXT("%s already exists as %s using ID '%s'"),
			*SourceStruct.ToString(), *ExistingStruct->ToString(), *ExistingStruct->ID.ToString());
	}

	if (SourceStruct.Struct && !SourceStruct.Struct->IsChildOf<FMetaStoryNodeBase>())
	{
		UE_CLOG(!SourceStruct.DataHandle.IsValid(), LogMetaStory, Error, TEXT("%s does not have a valid data handle."), *SourceStruct.ToString())
	}
	
	SourceStructs.Add(SourceStruct);
	return SourceStructs.Num() - 1;
}

FMetaStoryDelegateDispatcher FMetaStoryPropertyBindingCompiler::GetDispatcherFromPath(const FPropertyBindingPath& PathToDispatcher) const
{
	const FMetaStoryEditorDelegateDispatcherCompiledBinding* FoundDispatcher = CompiledDelegateDispatchers.FindByPredicate(
		[&PathToDispatcher](const FMetaStoryEditorDelegateDispatcherCompiledBinding& Dispatcher)
		{
			return PathToDispatcher == Dispatcher.DispatcherPath;
		});
	if (FoundDispatcher)
	{
		return FoundDispatcher->ID;
	}

	return FMetaStoryDelegateDispatcher();
}

TArray<FMetaStoryEditorDelegateDispatcherCompiledBinding> FMetaStoryPropertyBindingCompiler::GetCompiledDelegateDispatchers() const
{
	return CompiledDelegateDispatchers;
}

void FMetaStoryPropertyBindingCompiler::StoreSourceStructs()
{
	// Check that existing structs are compatible
	check(PropertyBindings->SourceStructs.Num() <= SourceStructs.Num());
	for (int32 i = 0; i < PropertyBindings->SourceStructs.Num(); i++)
	{
		check(PropertyBindings->SourceStructs[i] == SourceStructs[i]);
	}

	// Add new
	if (SourceStructs.Num() > PropertyBindings->SourceStructs.Num())
	{
		for (int32 i = PropertyBindings->SourceStructs.Num(); i < SourceStructs.Num(); i++)
		{
			PropertyBindings->SourceStructs.Add(SourceStructs[i]);
		}
	}
}
