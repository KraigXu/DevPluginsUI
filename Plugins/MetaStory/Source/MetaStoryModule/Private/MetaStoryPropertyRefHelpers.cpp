// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryPropertyRefHelpers.h"
#include "MetaStoryPropertyRef.h"
#include "UObject/TextProperty.h"
#include "UObject/EnumProperty.h"
#include "UObject/Class.h"
#include "MetaStoryPropertyBindings.h"

#if WITH_EDITOR
#include "EdGraphSchema_K2.h"
#include "IPropertyAccessEditor.h"
#include "EdGraph/EdGraphPin.h"
#include <limits>
#endif

namespace UE::MetaStory::PropertyRefHelpers
{
#if WITH_EDITOR
	static const FLazyName BoolName = "bool";
	static const FLazyName ByteName = "byte";
	static const FLazyName Int32Name = "int32";
	static const FLazyName Int64Name = "int64";
	static const FLazyName FloatName = "float";
	static const FLazyName DoubleName = "double";
	static const FLazyName NameName = "Name";
	static const FLazyName StringName = "String";
	static const FLazyName TextName = "Text";
	const FName IsRefToArrayName = "IsRefToArray";
	const FName CanRefToArrayName = "CanRefToArray";
	const FName RefTypeName = "RefType";
	static const FLazyName IsOptionalName = "Optional";

	bool ArePropertyRefsCompatible(const FProperty& TargetRefProperty, const FProperty& SourceRefProperty, const void* TargetRefAddress, const void* SourceRefAddress)
	{
		check(IsPropertyRef(SourceRefProperty) && IsPropertyRef(TargetRefProperty));
		check(TargetRefAddress);

		FEdGraphPinType SourceRefPin = GetPropertyRefInternalTypeAsPin(SourceRefProperty, SourceRefAddress);
		FEdGraphPinType TargetRefPin = GetPropertyRefInternalTypeAsPin(TargetRefProperty, TargetRefAddress);

		return SourceRefPin.PinCategory == TargetRefPin.PinCategory && SourceRefPin.ContainerType == TargetRefPin.ContainerType 
			&& SourceRefPin.PinSubCategoryObject == TargetRefPin.PinSubCategoryObject;
	}

	bool IsNativePropertyRefCompatibleWithProperty(const FProperty& RefProperty, const FProperty& SourceProperty)
	{
		check(IsPropertyRef(RefProperty));

		const FProperty* TestProperty = &SourceProperty;
		const bool bCanTargetRefArray = RefProperty.HasMetaData(CanRefToArrayName);
		const bool bIsTargetRefArray = RefProperty.HasMetaData(IsRefToArrayName);

		if (bIsTargetRefArray || bCanTargetRefArray)
		{
			if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(TestProperty))
			{
				TestProperty = ArrayProperty->Inner;
			}
			else if(!bCanTargetRefArray)
			{
				return false;
			}
		}

		FString TargetTypeNameFullStr = RefProperty.GetMetaData(RefTypeName);
		if (TargetTypeNameFullStr.IsEmpty())
		{
			return false;
		}

		TargetTypeNameFullStr.RemoveSpacesInline();

		TArray<FString> TargetTypes;
		TargetTypeNameFullStr.ParseIntoArray(TargetTypes, TEXT(","), true);

		const FStructProperty* SourceStructProperty = CastField<FStructProperty>(TestProperty);
		// Check inside loop are only allowed to return true to avoid shortcircuiting the loop.
		for (const FString& TargetTypeNameStr : TargetTypes)
		{
			const FName TargetTypeName = FName(*TargetTypeNameStr);
			// Compare properties metadata directly if SourceProperty is PropertyRef as well
			if (SourceStructProperty && SourceStructProperty->Struct == FMetaStoryPropertyRef::StaticStruct())

			{
				const FName SourceTypeName(SourceStructProperty->GetMetaData(RefTypeName));
				const bool bIsSourceRefArray = SourceStructProperty->GetBoolMetaData(IsRefToArrayName);
				if (SourceTypeName == TargetTypeName && bIsSourceRefArray == bIsTargetRefArray)
				{
					return true;
				}

			}

			if (TargetTypeName == BoolName)
			{
				if (TestProperty->IsA<FBoolProperty>())
				{
					return true;
				}
			}
			else if (TargetTypeName == ByteName)
			{
				if (TestProperty->IsA<FByteProperty>())
				{
					return true;
				}
			}
			else if (TargetTypeName == Int32Name)
			{
				if (TestProperty->IsA<FIntProperty>())
				{
					return true;
				}
			}
			else if (TargetTypeName == Int64Name)
			{
				if (TestProperty->IsA<FInt64Property>())
				{
					return true;
				}
			}
			else if (TargetTypeName == FloatName)
			{
				if (TestProperty->IsA<FFloatProperty>())
				{
					return true;
				}
			}
			else if (TargetTypeName == DoubleName)
			{
				if (TestProperty->IsA<FDoubleProperty>())
				{
					return true;
				}
			}
			else if (TargetTypeName == NameName)
			{
				if (TestProperty->IsA<FNameProperty>())
				{
					return true;
				}
			}

			else if (TargetTypeName == StringName)

			{
				if (TestProperty->IsA<FStrProperty>())
				{
					return true;
				}
			}
			else if (TargetTypeName == TextName)
			{
				if (TestProperty->IsA<FTextProperty>())
				{
					return true;
				}
			}
			else
			{
				UField* TargetRefField = UClass::TryFindTypeSlow<UField>(TargetTypeNameStr);
				if (!TargetRefField)
				{
					TargetRefField = LoadObject<UField>(nullptr, *TargetTypeNameStr);
				}

				if (SourceStructProperty)
				{
					if (SourceStructProperty->Struct->IsChildOf(Cast<UStruct>(TargetRefField)))
					{
						return true;
					}
				}

				if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(TestProperty))
				{
					// Only referencing object of the same exact class should be allowed. Otherwise one could e.g assign UObject to AActor property through reference to UObject.
					if(ObjectProperty->PropertyClass == Cast<UStruct>(TargetRefField))
					{
						return true;

					}
				}
				else if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(TestProperty))
				{
					if (EnumProperty->GetEnum() == TargetRefField)
					{
						return true;
					}
				}
			}
		}

		return false;
	}

	bool IsPropertyRefCompatibleWithProperty(const FProperty& RefProperty, const FProperty& SourceProperty, const void* PropertyRefAddress, const void* SourceAddress)
	{
		check(PropertyRefAddress);
		check(IsPropertyRef(RefProperty));

		if (IsPropertyRef(SourceProperty))
		{
			return ArePropertyRefsCompatible(RefProperty, SourceProperty, PropertyRefAddress, SourceAddress);
		}

		if (const FStructProperty* StructProperty = CastField<FStructProperty>(&RefProperty))
		{
			if (StructProperty->Struct == FMetaStoryPropertyRef::StaticStruct())
			{
				return IsNativePropertyRefCompatibleWithProperty(RefProperty, SourceProperty);
			}
			else if (StructProperty->Struct == FMetaStoryBlueprintPropertyRef::StaticStruct())
			{
				return IsBlueprintPropertyRefCompatibleWithProperty(SourceProperty, PropertyRefAddress);
			}
		}

		checkNoEntry();
		return false;
	}

	bool IsPropertyAccessibleForPropertyRef(const FProperty& SourceProperty, FMetaStoryBindableStructDesc SourceStruct, bool bIsOutput)
	{
		switch (SourceStruct.DataSource)
		{
		case EMetaStoryBindableStructSource::Parameter:
		case EMetaStoryBindableStructSource::StateParameter:
		case EMetaStoryBindableStructSource::TransitionEvent:
		case EMetaStoryBindableStructSource::StateEvent:
			return true;

		case EMetaStoryBindableStructSource::Context:
		case EMetaStoryBindableStructSource::Condition:
		case EMetaStoryBindableStructSource::Consideration:
		case EMetaStoryBindableStructSource::PropertyFunction:
			return false;

		case EMetaStoryBindableStructSource::GlobalTask:
		case EMetaStoryBindableStructSource::Evaluator:
		case EMetaStoryBindableStructSource::Task:
			return bIsOutput || IsPropertyRef(SourceProperty);

		default:
			checkNoEntry();
		}

		return false;
	}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	bool IsPropertyAccessibleForPropertyRef(TConstArrayView<FMetaStoryPropertyPathIndirection> SourcePropertyPathIndirections, FMetaStoryBindableStructDesc SourceStruct)
	{
		return false;
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	bool IsPropertyAccessibleForPropertyRef(TConstArrayView<FPropertyBindingPathIndirection> SourcePropertyPathIndirections, FMetaStoryBindableStructDesc SourceStruct)
	{
		bool bIsOutput = false;
		for (const FPropertyBindingPathIndirection& Indirection : SourcePropertyPathIndirections)
		{
			if (UE::MetaStory::GetUsageFromMetaData(Indirection.GetProperty()) == EMetaStoryPropertyUsage::Output)
			{
				bIsOutput = true;
				break;
			}
		}

		return IsPropertyAccessibleForPropertyRef(*SourcePropertyPathIndirections.Last().GetProperty(), SourceStruct, bIsOutput);
	}

	bool IsPropertyAccessibleForPropertyRef(const FProperty& SourceProperty, TConstArrayView<FBindingChainElement> BindingChain, FMetaStoryBindableStructDesc SourceStruct)
	{
		bool bIsOutput = UE::MetaStory::GetUsageFromMetaData(&SourceProperty) == EMetaStoryPropertyUsage::Output;
		for (const FBindingChainElement& ChainElement : BindingChain)
		{
			if (const FProperty* Property = ChainElement.Field.Get<FProperty>())
			{
				if (UE::MetaStory::GetUsageFromMetaData(Property) == EMetaStoryPropertyUsage::Output)
				{
					bIsOutput = true;
					break;
				}
			}
		}

		return IsPropertyAccessibleForPropertyRef(SourceProperty, SourceStruct, bIsOutput);
	}

	FEdGraphPinType GetBlueprintPropertyRefInternalTypeAsPin(const FMetaStoryBlueprintPropertyRef& PropertyRef)
	{
		FEdGraphPinType PinType;
		PinType.PinSubCategory = NAME_None;

		if (PropertyRef.IsRefToArray())
		{
			PinType.ContainerType = EPinContainerType::Array;
		}

		switch (PropertyRef.GetRefType())
		{
		case EMetaStoryPropertyRefType::None:
			break;

		case EMetaStoryPropertyRefType::Bool:
			PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
			break;

		case EMetaStoryPropertyRefType::Byte:
			PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
			break;

		case EMetaStoryPropertyRefType::Int32:
			PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
			break;

		case EMetaStoryPropertyRefType::Int64:
			PinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
			break;

		case EMetaStoryPropertyRefType::Float:
			PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
			PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
			break;

		case EMetaStoryPropertyRefType::Double:
			PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
			PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
			break;

		case EMetaStoryPropertyRefType::Name:
			PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
			break;

		case EMetaStoryPropertyRefType::String:
			PinType.PinCategory = UEdGraphSchema_K2::PC_String;
			break;

		case EMetaStoryPropertyRefType::Text:
			PinType.PinCategory = UEdGraphSchema_K2::PC_Text;
			break;

		case EMetaStoryPropertyRefType::Enum:
			PinType.PinCategory = UEdGraphSchema_K2::PC_Enum;
			PinType.PinSubCategoryObject = PropertyRef.GetTypeObject();
			if (UEnum* Enum = Cast<UEnum>(PinType.PinSubCategoryObject))
			{
				if (Enum->GetMaxEnumValue() <= (int64)std::numeric_limits<uint8>::max())
				{
					// Use byte for BP. It will use the correct picker and enum k2 node.
					PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
				}
			}
			else
			{
				UE_LOG(LogMetaStory, Warning, TEXT("The property ref of type enum has an invalid enum. %s"), *GetFullNameSafe(PinType.PinSubCategoryObject.Get()));
			}
			break;

		case EMetaStoryPropertyRefType::Struct:
			PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
			PinType.PinSubCategoryObject = PropertyRef.GetTypeObject();
			break;

		case EMetaStoryPropertyRefType::Object:
			PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
			PinType.PinSubCategoryObject = PropertyRef.GetTypeObject();
			break;

		default:
			ensureMsgf(false, TEXT("Unhandled type %s"), *UEnum::GetValueAsString(PropertyRef.GetRefType()));
			break;
		}
		return PinType;
	}

	FEdGraphPinType GetNativePropertyRefInternalTypeAsPin(const FProperty& RefProperty)
	{
		TArray<FEdGraphPinType, TInlineAllocator<1>> PinTypes = GetPropertyRefInternalTypesAsPins(RefProperty);
		if (PinTypes.Num() == 1)
		{
			return PinTypes[0];
		}
		return FEdGraphPinType();
	}

	FEdGraphPinType GetPropertyRefInternalTypeAsPin(const FProperty& RefProperty, const void* PropertyRefAddress)
	{
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(&RefProperty))
		{
			if (StructProperty->Struct == FMetaStoryPropertyRef::StaticStruct())
			{
				return GetNativePropertyRefInternalTypeAsPin(RefProperty);
			}
			else if (StructProperty->Struct == FMetaStoryBlueprintPropertyRef::StaticStruct())
			{
				// The source of the chain can be an uninitialized object.
				if (PropertyRefAddress)
				{
					return GetBlueprintPropertyRefInternalTypeAsPin(*reinterpret_cast<const FMetaStoryBlueprintPropertyRef*>(PropertyRefAddress));
				}
			}
		}

		checkNoEntry();
		return FEdGraphPinType();
	}

	void METASTORYMODULE_API GetBlueprintPropertyRefInternalTypeFromPin(const FEdGraphPinType& PinType, EMetaStoryPropertyRefType& OutRefType, bool& bOutIsArray, UObject*& OutObjectType)
	{
		OutRefType = EMetaStoryPropertyRefType::None;
		bOutIsArray = false;
		OutObjectType = nullptr;

		// Set container type
		switch (PinType.ContainerType)
		{
		case EPinContainerType::Array:
			bOutIsArray = true;
			break;
		case EPinContainerType::Set:
			ensureMsgf(false, TEXT("Unsuported container type [Set] "));
			break;
		case EPinContainerType::Map:
			ensureMsgf(false, TEXT("Unsuported container type [Map] "));
			break;
		default:
			break;
		}
	
		// Value type
		if (PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
		{
			OutRefType = EMetaStoryPropertyRefType::Bool;
		}
		else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Byte)
		{
			if (UEnum* Enum = Cast<UEnum>(PinType.PinSubCategoryObject))
			{
				OutRefType = EMetaStoryPropertyRefType::Enum;
				OutObjectType = PinType.PinSubCategoryObject.Get();
			}
			else
			{
				OutRefType = EMetaStoryPropertyRefType::Byte;
			}
		}
		else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
		{
			OutRefType = EMetaStoryPropertyRefType::Int32;
		}
		else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int64)
		{
			OutRefType = EMetaStoryPropertyRefType::Int64;
		}
		else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Real)
		{
			if (PinType.PinSubCategory == UEdGraphSchema_K2::PC_Float)
			{
				OutRefType = EMetaStoryPropertyRefType::Float;
			}
			else if (PinType.PinSubCategory == UEdGraphSchema_K2::PC_Double)
			{
				OutRefType = EMetaStoryPropertyRefType::Double;
			}		
		}
		else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Name)
		{
			OutRefType = EMetaStoryPropertyRefType::Name;
		}
		else if (PinType.PinCategory == UEdGraphSchema_K2::PC_String)
		{
			OutRefType = EMetaStoryPropertyRefType::String;
		}
		else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Text)
		{
			OutRefType = EMetaStoryPropertyRefType::Text;
		}
		else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Enum)
		{
			OutRefType = EMetaStoryPropertyRefType::Enum;
			OutObjectType = PinType.PinSubCategoryObject.Get();
		}
		else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
		{
			OutRefType = EMetaStoryPropertyRefType::Struct;
			OutObjectType = PinType.PinSubCategoryObject.Get();
		}
		else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Object)
		{
			OutRefType = EMetaStoryPropertyRefType::Object;
			OutObjectType = PinType.PinSubCategoryObject.Get();
		}
		else if (PinType.PinCategory == UEdGraphSchema_K2::PC_SoftObject)
		{
			OutRefType = EMetaStoryPropertyRefType::SoftObject;
			OutObjectType = PinType.PinSubCategoryObject.Get();
		}
		else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Class)
		{
			OutRefType = EMetaStoryPropertyRefType::Class;
			OutObjectType = PinType.PinSubCategoryObject.Get();
		}
		else if (PinType.PinCategory == UEdGraphSchema_K2::PC_SoftClass)
		{
			OutRefType = EMetaStoryPropertyRefType::SoftClass;
			OutObjectType = PinType.PinSubCategoryObject.Get();
		}
		else
		{
			ensureMsgf(false, TEXT("Unhandled pin category %s"), *PinType.PinCategory.ToString());
		}
	}

	bool METASTORYMODULE_API IsPropertyRefMarkedAsOptional(const FProperty& RefProperty, const void* PropertyRefAddress)
	{
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(&RefProperty))
		{
			if (StructProperty->Struct == FMetaStoryPropertyRef::StaticStruct())
			{
				return RefProperty.HasMetaData(IsOptionalName);
			}
			else if (StructProperty->Struct == FMetaStoryBlueprintPropertyRef::StaticStruct())
			{
				check(PropertyRefAddress);
				return reinterpret_cast<const FMetaStoryBlueprintPropertyRef*>(PropertyRefAddress)->IsOptional();
			}
		}

		checkNoEntry();
		return false;
	}

	TArray<FEdGraphPinType, TInlineAllocator<1>> GetPropertyRefInternalTypesAsPins(const FProperty& RefProperty)
	{
		ensure(IsPropertyRef(RefProperty));

		const EPinContainerType ContainerType = RefProperty.HasMetaData(IsRefToArrayName) ? EPinContainerType::Array : EPinContainerType::None;

		TArray<FEdGraphPinType, TInlineAllocator<1>> PinTypes;

		FString TargetTypesString = RefProperty.GetMetaData(RefTypeName);
		if (TargetTypesString.IsEmpty())
		{
			return PinTypes;
		}

		TArray<FString> TargetTypes;
		TargetTypesString.RemoveSpacesInline();
		TargetTypesString.ParseIntoArray(TargetTypes, TEXT(","), true);

		for (const FString& TargetType : TargetTypes)
		{
			const FName TargetTypeName = *TargetType;

			FEdGraphPinType& PinType = PinTypes.AddDefaulted_GetRef();
			PinType.ContainerType = ContainerType;

			if (TargetTypeName == BoolName)
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
			}
			else if (TargetTypeName == ByteName)
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
			}
			else if (TargetTypeName == Int32Name)
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
			}
			else if (TargetTypeName == Int64Name)
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
			}
			else if (TargetTypeName == FloatName)
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
				PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
			}
			else if (TargetTypeName == DoubleName)
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
				PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
			}
			else if (TargetTypeName == NameName)
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
			}
			else if (TargetTypeName == StringName)
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_String;
			}
			else if (TargetTypeName == TextName)
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_Text;
			}
			else
			{
				UField* TargetRefField = UClass::TryFindTypeSlow<UField>(TargetType);
				if (!TargetRefField)
				{
					TargetRefField = LoadObject<UField>(nullptr, *TargetType);
				}

				if (UScriptStruct* Struct = Cast<UScriptStruct>(TargetRefField))
				{
					PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
					PinType.PinSubCategoryObject = Struct;
				}
				else if (UClass* ObjectClass = Cast<UClass>(TargetRefField))
				{
					PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
					PinType.PinSubCategoryObject = ObjectClass;
				}
				else if (UEnum* Enum = Cast<UEnum>(TargetRefField))
				{
					PinType.PinCategory = UEdGraphSchema_K2::PC_Enum;
					PinType.PinSubCategoryObject = Enum;
					if (Enum->GetMaxEnumValue() <= (int64)std::numeric_limits<uint8>::max())
					{
						// Use byte for BP. It will use the correct picker and enum k2 node.
						PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
					}
				}
				else
				{
					checkf(false, TEXT("Typename in meta-data (%s) is invalid"), *TargetType);
				}
			}
		}
		return PinTypes;
	}
#endif

	bool IsBlueprintPropertyRefCompatibleWithProperty(const FProperty& SourceProperty, const void* PropertyRefAddress)
	{
		const FMetaStoryBlueprintPropertyRef& PropertyRef = *reinterpret_cast<const FMetaStoryBlueprintPropertyRef*>(PropertyRefAddress);
		const FProperty* TestProperty = &SourceProperty;
		if (PropertyRef.IsRefToArray())
		{
			if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(TestProperty))
			{
				TestProperty = ArrayProperty->Inner;
			}
			else
			{
				return false;
			}
		}

		switch (PropertyRef.GetRefType())
		{
		case EMetaStoryPropertyRefType::None:
			return false;

		case EMetaStoryPropertyRefType::Bool:
			return Validator<bool>::IsValid(*TestProperty);

		case EMetaStoryPropertyRefType::Byte:
			return Validator<uint8>::IsValid(*TestProperty);

		case EMetaStoryPropertyRefType::Int32:
			return Validator<int32>::IsValid(*TestProperty);

		case EMetaStoryPropertyRefType::Int64:
			return Validator<int64>::IsValid(*TestProperty);

		case EMetaStoryPropertyRefType::Float:
			return Validator<float>::IsValid(*TestProperty);

		case EMetaStoryPropertyRefType::Double:
			return Validator<double>::IsValid(*TestProperty);

		case EMetaStoryPropertyRefType::Name:
			return Validator<FName>::IsValid(*TestProperty);

		case EMetaStoryPropertyRefType::String:
			return Validator<FString>::IsValid(*TestProperty);

		case EMetaStoryPropertyRefType::Text:
			return Validator<FText>::IsValid(*TestProperty);

		case EMetaStoryPropertyRefType::Enum:
			if (const UEnum* Enum = Cast<UEnum>(PropertyRef.GetTypeObject()))
			{
				return IsPropertyCompatibleWithEnum(*TestProperty, *Enum);
			}
			return false;

		case EMetaStoryPropertyRefType::Struct:
			if (const UScriptStruct* Struct = Cast<UScriptStruct>(PropertyRef.GetTypeObject()))
			{
				return IsPropertyCompatibleWithStruct(*TestProperty, *Struct);
			}
			return false;

		case EMetaStoryPropertyRefType::Object:
			if (const UClass* Class = Cast<UClass>(PropertyRef.GetTypeObject()))
			{
				return IsPropertyCompatibleWithClass(*TestProperty, *Class);
			}
			return false;

		default:
			checkNoEntry();
		}

		return false;
	}

	bool IsPropertyRef(const FProperty& Property)
	{
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(&Property))
		{
			return StructProperty->Struct->IsChildOf(FMetaStoryPropertyRef::StaticStruct());
		}

		return false;
	}

	bool IsPropertyCompatibleWithEnum(const FProperty& Property, const UEnum& Enum)
	{
		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(&Property))
		{
			return EnumProperty->GetEnum() == &Enum;
		}
		return false;
	}

	bool IsPropertyCompatibleWithClass(const FProperty& Property, const UClass& Class)
	{
		if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(&Property))
		{
			return ObjectProperty->PropertyClass == &Class;
		}
		return false;
	}

	bool IsPropertyCompatibleWithStruct(const FProperty& Property, const UScriptStruct& Struct)
	{
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(&Property))
		{
			return StructProperty->Struct == &Struct;
		}
		return false;
	}
}