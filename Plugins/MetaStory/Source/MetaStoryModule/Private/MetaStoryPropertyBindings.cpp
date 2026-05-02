// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetaStoryPropertyBindings.h"
#include "UObject/EnumProperty.h"
#include "Misc/EnumerateRange.h"
#include "PropertyPathHelpers.h"
#include "StructUtils/PropertyBag.h"
#include "MetaStoryPropertyRef.h"

#if WITH_EDITOR
#include "UObject/CoreRedirects.h"
#include "UObject/Package.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "StructUtils/UserDefinedStruct.h"
#include "UObject/Field.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaStoryPropertyBindings)

namespace UE::MetaStory
{
	bool AcceptTaskInstanceData(EMetaStoryBindableStructSource Target)
	{
		// Condition and Utility are constructed before the task instance data is constructed.
		return Target != EMetaStoryBindableStructSource::StateParameter
			&& Target != EMetaStoryBindableStructSource::Condition
			&& Target != EMetaStoryBindableStructSource::Consideration;
	}

	FString GetDescAndPathAsString(const FMetaStoryBindableStructDesc& Desc, const FPropertyBindingPath& Path)
	{
		return PropertyBinding::GetDescriptorAndPathAsString(Desc, Path);
	}

#if WITH_EDITOR
	EMetaStoryPropertyUsage GetUsageFromMetaData(const FProperty* Property)
	{
		static const FName CategoryName(TEXT("Category"));

		if (Property == nullptr)
		{
			return EMetaStoryPropertyUsage::Invalid;
		}
		
		const FString Category = Property->GetMetaData(CategoryName);

		if (Category == TEXT("Input"))
		{
			return EMetaStoryPropertyUsage::Input;
		}
		if (Category == TEXT("Inputs"))
		{
			return EMetaStoryPropertyUsage::Input;
		}
		if (Category == TEXT("Output"))
		{
			return EMetaStoryPropertyUsage::Output;
		}
		if (Category == TEXT("Outputs"))
		{
			return EMetaStoryPropertyUsage::Output;
		}
		if (Category == TEXT("Context"))
		{
			return EMetaStoryPropertyUsage::Context;
		}

		return EMetaStoryPropertyUsage::Parameter;
	}

	const FProperty* GetStructSingleOutputProperty(const UStruct& InStruct)
	{
		const FProperty* FuncOutputProperty = nullptr;
		for (TFieldIterator<FProperty> PropIt(&InStruct, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
		{
			if (GetUsageFromMetaData(*PropIt) == EMetaStoryPropertyUsage::Output)
			{
				if (FuncOutputProperty)
				{
					return nullptr;
				}

				FuncOutputProperty = *PropIt;
			}
		}

		return FuncOutputProperty;
	}
#endif

} // UE::MetaStory

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#if WITH_EDITORONLY_DATA
namespace UE::MetaStory::Deprecation
{
	FPropertyBindingPath ConvertEditorPath(const FMetaStoryEditorPropertyPath& InEditorPath)
	{
		FPropertyBindingPath Path;
		Path.SetStructID(InEditorPath.StructID);

		for (const FString& Segment : InEditorPath.Path)
		{
			const TCHAR* PropertyNamePtr = nullptr;
			int32 PropertyNameLength = 0;
			int32 ArrayIndex = INDEX_NONE;
			PropertyPathHelpers::FindFieldNameAndArrayIndex(Segment.Len(), *Segment, PropertyNameLength, &PropertyNamePtr, ArrayIndex);
			FString PropertyNameString(PropertyNameLength, PropertyNamePtr);
			const FName PropertyName(*PropertyNameString, FNAME_Find);
			Path.AddPathSegment(PropertyName, ArrayIndex);
		}
		return Path;
	}
} // UE::MetaStory::Deprecation

//----------------------------------------------------------------//
//  FMetaStoryPropertyPathBinding
//----------------------------------------------------------------//
void FMetaStoryPropertyPathBinding::PostSerialize(const FArchive& Ar)
{
	if (SourcePath_DEPRECATED.IsValid())
	{
		SourcePropertyPath = UE::MetaStory::Deprecation::ConvertEditorPath(SourcePath_DEPRECATED);
		SourcePath_DEPRECATED.StructID = FGuid();
		SourcePath_DEPRECATED.Path.Reset();
	}

	if (TargetPath_DEPRECATED.IsValid())
	{
		TargetPropertyPath = UE::MetaStory::Deprecation::ConvertEditorPath(TargetPath_DEPRECATED);
		TargetPath_DEPRECATED.StructID = FGuid();
		TargetPath_DEPRECATED.Path.Reset();
	}
}
#endif // WITH_EDITORONLY_DATA
PRAGMA_ENABLE_DEPRECATION_WARNINGS

//----------------------------------------------------------------//
//  FMetaStoryBindableStructDesc
//----------------------------------------------------------------//

FString FMetaStoryBindableStructDesc::ToString() const
{
	FStringBuilderBase Result;

	Result += *UEnum::GetDisplayValueAsText(DataSource).ToString();
	Result += TEXT(" '");
#if WITH_EDITORONLY_DATA
	Result += StatePath;
	Result += TEXT("/");
#endif
	Result += Name.ToString();
	Result += TEXT("'");

	return Result.ToString();
}

//----------------------------------------------------------------//
//  FMetaStoryPropertyBindings
//----------------------------------------------------------------//

FMetaStoryPropertyBindings::FMetaStoryPropertyBindings()
{
	// MetaStory supports Property references
	PropertyReferenceStructType = FMetaStoryStructRef::StaticStruct();

	// Set copy function
	PropertyReferenceCopyFunc = [](const FStructProperty& SourceStructProperty, uint8* SourceAddress, uint8* TargetAddress)
	{
		FMetaStoryStructRef* Target = reinterpret_cast<FMetaStoryStructRef*>(TargetAddress);
		Target->Set(FStructView(SourceStructProperty.Struct, SourceAddress));
	};

	// Set reset function
	PropertyReferenceResetFunc = [](uint8* TargetAddress)
	{
		reinterpret_cast<FMetaStoryStructRef*>(TargetAddress)->Set(FStructView());
	};
}

void FMetaStoryPropertyBindings::OnReset()
{
	SourceStructs.Reset();
	PropertyPathBindings.Reset();
	PropertyAccesses.Reset();
	PropertyReferencePaths.Reset();
}

int32 FMetaStoryPropertyBindings::GetNumBindableStructDescriptors() const
{
	return SourceStructs.Num();
}

const FPropertyBindingBindableStructDescriptor* FMetaStoryPropertyBindings::GetBindableStructDescriptorFromHandle(const FConstStructView InSourceHandleView) const
{
	check(InSourceHandleView.GetScriptStruct() == FMetaStoryDataHandle::StaticStruct());
	return GetBindableStructDescriptorFromHandle(InSourceHandleView.Get<const FMetaStoryDataHandle>());
}

const FPropertyBindingBindableStructDescriptor* FMetaStoryPropertyBindings::GetBindableStructDescriptorFromHandle(FMetaStoryDataHandle InSourceHandle) const
{
	return SourceStructs.FindByPredicate([SourceDataHandle = InSourceHandle](const FMetaStoryBindableStructDesc& Desc)
	{
		return Desc.DataHandle == SourceDataHandle;
	});
}

void FMetaStoryPropertyBindings::VisitSourceStructDescriptorInternal(
	TFunctionRef<EVisitResult(const FPropertyBindingBindableStructDescriptor& Descriptor)> InFunction) const
{
	for (const FMetaStoryBindableStructDesc& SourceStruct : SourceStructs)
	{
		if (InFunction(SourceStruct) == EVisitResult::Break)
		{
			break;
		}
	}
}

bool FMetaStoryPropertyBindings::ResolveBindingCopyInfo(const FPropertyBindingBinding& InResolvedBinding,
	const FPropertyBindingPathIndirection& InBindingSourceLeafIndirection, const FPropertyBindingPathIndirection& InBindingTargetLeafIndirection,
	FPropertyBindingCopyInfo& OutCopyInfo)
{
	OutCopyInfo.bCopyFromTargetToSource = static_cast<const FMetaStoryPropertyPathBinding&>(InResolvedBinding).IsOutputBinding();

	return Super::ResolveBindingCopyInfo(InResolvedBinding, InBindingSourceLeafIndirection, InBindingTargetLeafIndirection, OutCopyInfo);
}

bool FMetaStoryPropertyBindings::OnResolvingPaths()
{
	// Base class handled common bindings, here we only need to handle Property references
	bool bResult = true;

	PropertyAccesses.Reset();
	PropertyAccesses.Reserve(PropertyReferencePaths.Num());

	for (const FMetaStoryPropertyRefPath& ReferencePath : PropertyReferencePaths)
	{
		FMetaStoryPropertyAccess& PropertyAccess = PropertyAccesses.AddDefaulted_GetRef();
		
		PropertyAccess.SourceDataHandle = ReferencePath.GetSourceDataHandle();
		const FPropertyBindingBindableStructDescriptor* SourceDesc = GetBindableStructDescriptorFromHandle(PropertyAccess.SourceDataHandle);
		PropertyAccess.SourceStructType = SourceDesc->Struct;

		FPropertyBindingPathIndirection SourceLeafIndirection;
		if (!Super::ResolvePath(SourceDesc->Struct, ReferencePath.GetSourcePath(), PropertyAccess.SourceIndirection, SourceLeafIndirection))
		{
			bResult = false;
		}

		PropertyAccess.SourceLeafProperty = SourceLeafIndirection.GetProperty();
	}

	return bResult;
}

int32 FMetaStoryPropertyBindings::GetNumBindings() const
{
	return PropertyPathBindings.Num();
}

void FMetaStoryPropertyBindings::ForEachBinding(TFunctionRef<void(const FPropertyBindingBinding& Binding)> InFunction) const
{
	for (const FMetaStoryPropertyPathBinding& Binding : PropertyPathBindings)
	{
		InFunction(Binding);
	}
}

void FMetaStoryPropertyBindings::ForEachBinding(const FPropertyBindingIndex16 InBegin, const FPropertyBindingIndex16 InEnd
	, const TFunctionRef<void(const FPropertyBindingBinding& Binding, const int32 BindingIndex)> InFunction) const
{
	ensureMsgf(InBegin.IsValid() && InEnd.IsValid(), TEXT("%hs expects valid indices."), __FUNCTION__);

	for (int32 BindingIndex = InBegin.Get(); BindingIndex < InEnd.Get(); ++BindingIndex)
	{
		InFunction(PropertyPathBindings[BindingIndex], BindingIndex);
	}
}

void FMetaStoryPropertyBindings::ForEachMutableBinding(TFunctionRef<void(FPropertyBindingBinding& Binding)> InFunction)
{
	for (FMetaStoryPropertyPathBinding& Binding : PropertyPathBindings)
	{
		InFunction(Binding);
	}
}

void FMetaStoryPropertyBindings::VisitBindings(TFunctionRef<EVisitResult(const FPropertyBindingBinding& Binding)> InFunction) const
{
	for (const FMetaStoryPropertyPathBinding& Binding : PropertyPathBindings)
	{
		if (InFunction(Binding) == EVisitResult::Break)
		{
			break;
		}
	}
}

void FMetaStoryPropertyBindings::VisitMutableBindings(TFunctionRef<EVisitResult(FPropertyBindingBinding& Binding)> InFunction)
{
	for (FMetaStoryPropertyPathBinding& Binding : PropertyPathBindings)
	{
		if (InFunction(Binding) == EVisitResult::Break)
		{
			break;
		}
	}
}

#if WITH_EDITOR
FPropertyBindingBinding* FMetaStoryPropertyBindings::AddBindingInternal(const FPropertyBindingPath& InSourcePath
	, const FPropertyBindingPath& InTargetPath)
{
	checkf(false, TEXT("Not expected to get called for MetaStory runtime bindings."
					" Editor operations for bindings are handled by FMetaStoryEditorPropertyBindings"));
	return nullptr;
}

void FMetaStoryPropertyBindings::RemoveBindingsInternal(TFunctionRef<bool(FPropertyBindingBinding&)> InPredicate)
{
	checkf(false, TEXT("Not expected to get called for MetaStory runtime bindings."
					" Editor operations for bindings are handled by FMetaStoryEditorPropertyBindings"));
}

bool FMetaStoryPropertyBindings::HasBindingInternal(TFunctionRef<bool(const FPropertyBindingBinding&)> InPredicate) const
{
	checkf(false, TEXT("Not expected to get called for MetaStory runtime bindings."
					" Editor operations for bindings are handled by FMetaStoryEditorPropertyBindings"));
	return false;
}

const FPropertyBindingBinding* FMetaStoryPropertyBindings::FindBindingInternal(TFunctionRef<bool(const FPropertyBindingBinding&)> InPredicate) const
{
	checkf(false, TEXT("Not expected to get called for MetaStory runtime bindings."
					" Editor operations for bindings are handled by FMetaStoryEditorPropertyBindings"));
	return nullptr;
}
#endif // WITH_EDITOR

bool FMetaStoryPropertyBindings::ResetObjects(const FMetaStoryIndex16 TargetBatchIndex, FMetaStoryDataView TargetStructView) const
{
	return Super::ResetObjects(TargetBatchIndex, TargetStructView);
}

const FMetaStoryPropertyAccess* FMetaStoryPropertyBindings::GetPropertyAccess(const FMetaStoryPropertyRef& InPropertyReference) const
{
	if (!InPropertyReference.GetRefAccessIndex().IsValid())
	{
		return nullptr;
	}

	if (!ensure(PropertyAccesses.IsValidIndex(InPropertyReference.GetRefAccessIndex().Get())))
	{
		return nullptr;
	}

	return &PropertyAccesses[InPropertyReference.GetRefAccessIndex().Get()];
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
int32 FMetaStoryPropertyBindings::GetSourceStructNum() const
{
	return SourceStructs.Num();
}

const FMetaStoryPropertyCopyBatch& FMetaStoryPropertyBindings::GetBatch(const FMetaStoryIndex16 TargetBatchIndex) const
{
	check(TargetBatchIndex.IsValid());
	static FMetaStoryPropertyCopyBatch Batch;
	return Batch;
}

TConstArrayView<FMetaStoryPropertyCopy> FMetaStoryPropertyBindings::GetBatchCopies(const FMetaStoryIndex16 TargetBatchIndex) const
{
	return GetBatchCopies(GetBatch(TargetBatchIndex));
}

TConstArrayView<FMetaStoryPropertyCopy> FMetaStoryPropertyBindings::GetBatchCopies(const FMetaStoryPropertyCopyBatch& Batch) const
{
	return {};
}

bool FMetaStoryPropertyBindings::ResolveCopyType(const FMetaStoryPropertyPathIndirection& SourceIndirection, const FMetaStoryPropertyPathIndirection& TargetIndirection, FMetaStoryPropertyCopy& OutCopy)
{
	return false;
}

bool FMetaStoryPropertyBindings::ResolveCopyType(const FPropertyBindingPathIndirection& SourceIndirection, const FPropertyBindingPathIndirection& TargetIndirection, FMetaStoryPropertyCopy& OutCopy)
{
	return false;
}

EMetaStoryPropertyAccessCompatibility FMetaStoryPropertyBindings::GetPropertyCompatibility(const FProperty* FromProperty, const FProperty* ToProperty)
{
	// Temporary using cast as this method is no longer used and will be removed so we don't care about matching values in both enums
	//const UE::PropertyBinding::EPropertyCompatibility Value = UE::PropertyBinding::GetPropertyCompatibility(FromProperty, ToProperty);
	//check(UEnum::GetValueAsName<EMetaStoryPropertyAccessCompatibility>((EMetaStoryPropertyAccessCompatibility)Value) ==
	//	  UEnum::GetValueAsName<UE::PropertyBinding::EPropertyCompatibility>(Value));
	return static_cast<EMetaStoryPropertyAccessCompatibility>(UE::PropertyBinding::GetPropertyCompatibility(FromProperty, ToProperty));
}

bool FMetaStoryPropertyBindings::ResolvePath(const UStruct* Struct, const FPropertyBindingPath& Path
	, FMetaStoryPropertyIndirection& OutFirstIndirection, FPropertyBindingPathIndirection& OutLeafIndirection)
{
	return false;
}

const FMetaStoryBindableStructDesc* FMetaStoryPropertyBindings::GetSourceDescByHandle(const FMetaStoryDataHandle SourceDataHandle)
{
	return nullptr;
}

void FMetaStoryPropertyBindings::PerformCopy(const FMetaStoryPropertyCopy& Copy, uint8* SourceAddress, uint8* TargetAddress) const
{
}

void FMetaStoryPropertyBindings::PerformResetObjects(const FMetaStoryPropertyCopy& Copy, uint8* TargetAddress) const
{
}

uint8* FMetaStoryPropertyBindings::GetAddress(FMetaStoryDataView InStructView, const FMetaStoryPropertyIndirection& FirstIndirection
	, const FProperty* LeafProperty) const
{
	return nullptr;
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS
