// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryTest.h"
#include "MetaStoryTestBase.h"
#include "MetaStoryTestTypes.h"

#include "MetaStoryEditorData.h"
#include "Conditions/MetaStoryCommonConditions.h"

#define LOCTEXT_NAMESPACE "AITestSuite_MetaStoryTest"

UE_DISABLE_OPTIMIZATION_SHIP

namespace UE::MetaStory::Tests
{

struct FMetaStoryTest_PropertyPathOffset : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		FPropertyBindingPath Path;
		const bool bParseResult = Path.FromString(TEXT("StructB.B"));

		AITEST_TRUE(TEXT("Parsing path should succeeed"), bParseResult);
		AITEST_EQUAL(TEXT("Should have 2 path segments"), Path.NumSegments(), 2);

		FString ResolveErrors;
		TArray<FPropertyBindingPathIndirection> Indirections;
		const bool bResolveResult = Path.ResolveIndirections(FMetaStoryTest_PropertyStruct::StaticStruct(), Indirections, &ResolveErrors);

		AITEST_TRUE(TEXT("Resolve path should succeeed"), bResolveResult);
		AITEST_EQUAL(TEXT("Should have no resolve errors"), ResolveErrors.Len(), 0);
		
		AITEST_EQUAL(TEXT("Should have 2 indirections"), Indirections.Num(), 2);
		AITEST_EQUAL(TEXT("Indirection 0 should be Offset type"), Indirections[0].GetAccessType(), EPropertyBindingPropertyAccessType::Offset);
		AITEST_EQUAL(TEXT("Indirection 1 should be Offset type"), Indirections[1].GetAccessType(), EPropertyBindingPropertyAccessType::Offset);

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_PropertyPathOffset, "System.MetaStory.PropertyPath.Offset");

struct FMetaStoryTest_PropertyPathParseFail : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		{
			FPropertyBindingPath Path;
			const bool bParseResult = Path.FromString(TEXT("")); // empty is valid.
			AITEST_TRUE(TEXT("Parsing path should succeed"), bParseResult);
		}

		{
			FPropertyBindingPath Path;
			const bool bParseResult = Path.FromString(TEXT("StructB.[0]B"));
			AITEST_FALSE(TEXT("Parsing path should fail"), bParseResult);
		}

		{
			FPropertyBindingPath Path;
			const bool bParseResult = Path.FromString(TEXT("StructB..NoThere"));
			AITEST_FALSE(TEXT("Parsing path should fail"), bParseResult);
		}

		{
			FPropertyBindingPath Path;
			const bool bParseResult = Path.FromString(TEXT("."));
			AITEST_FALSE(TEXT("Parsing path should fail"), bParseResult);
		}

		{
			FPropertyBindingPath Path;
			const bool bParseResult = Path.FromString(TEXT("StructB..B"));
			AITEST_FALSE(TEXT("Parsing path should fail"), bParseResult);
		}

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_PropertyPathParseFail, "System.MetaStory.PropertyPath.ParseFail");

struct FMetaStoryTest_PropertyPathOffsetFail : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		FPropertyBindingPath Path;
		const bool bParseResult = Path.FromString(TEXT("StructB.Q"));

		AITEST_TRUE(TEXT("Parsing path should succeeed"), bParseResult);
		AITEST_EQUAL(TEXT("Should have 2 path segments"), Path.NumSegments(), 2);

		FString ResolveErrors;
		TArray<FPropertyBindingPathIndirection> Indirections;
		const bool bResolveResult = Path.ResolveIndirections(FMetaStoryTest_PropertyStruct::StaticStruct(), Indirections, &ResolveErrors);

		AITEST_FALSE(TEXT("Resolve path should not succeeed"), bResolveResult);
		AITEST_NOT_EQUAL(TEXT("Should have errors"), ResolveErrors.Len(), 0);
		
		AITEST_EQUAL(TEXT("Should have 0 indirections"), Indirections.Num(), 0);

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_PropertyPathOffsetFail, "System.MetaStory.PropertyPath.OffsetFail");

struct FMetaStoryTest_PropertyPathObject : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		FPropertyBindingPath Path;
		const bool bParseResult = Path.FromString(TEXT("InstancedObject.A"));

		AITEST_TRUE(TEXT("Parsing path should succeeed"), bParseResult);
		AITEST_EQUAL(TEXT("Should have 2 path segments"), Path.NumSegments(), 2);

		UMetaStoryTest_PropertyObject* Object = NewObject<UMetaStoryTest_PropertyObject>();
		Object->InstancedObject = NewObject<UMetaStoryTest_PropertyObjectInstanced>();
		
		const bool bUpdateResult = Path.UpdateSegmentsFromValue(FMetaStoryDataView(Object));

		AITEST_TRUE(TEXT("Update instance types should succeeed"), bUpdateResult);
		AITEST_TRUE(TEXT("Path segment 0 instance type should be UMetaStoryTest_PropertyObjectInstanced"), Path.GetSegment(0).GetInstanceStruct() == UMetaStoryTest_PropertyObjectInstanced::StaticClass());
		AITEST_TRUE(TEXT("Path segment 1 instance type should be nullptr"), Path.GetSegment(1).GetInstanceStruct() == nullptr);

		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_PropertyPathObject, "System.MetaStory.PropertyPath.Object");

struct FMetaStoryTest_PropertyPathWrongObject : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		FPropertyBindingPath Path;
		const bool bParseResult = Path.FromString(TEXT("InstancedObject.B"));

		AITEST_TRUE(TEXT("Parsing path should succeeed"), bParseResult);
		AITEST_EQUAL(TEXT("Should have 2 path segments"), Path.NumSegments(), 2);

		UMetaStoryTest_PropertyObject* Object = NewObject<UMetaStoryTest_PropertyObject>();

		Object->InstancedObject = NewObject<UMetaStoryTest_PropertyObjectInstancedWithB>();
		{
			FString ResolveErrors;
			TArray<FPropertyBindingPathIndirection> Indirections;
			const bool bResolveResult = Path.ResolveIndirectionsWithValue(FMetaStoryDataView(Object), Indirections, &ResolveErrors);

			AITEST_TRUE(TEXT("Resolve path should succeeed"), bResolveResult);
			AITEST_EQUAL(TEXT("Should have 2 indirections"), Indirections.Num(), 2);
			AITEST_TRUE(TEXT("Object "), Indirections[0].GetAccessType() == EPropertyBindingPropertyAccessType::ObjectInstance);
			AITEST_TRUE(TEXT("Object "), Indirections[0].GetContainerStruct() == Object->GetClass());
			AITEST_TRUE(TEXT("Object "), Indirections[0].GetInstanceStruct() == UMetaStoryTest_PropertyObjectInstancedWithB::StaticClass());
			AITEST_EQUAL(TEXT("Should not have error"), ResolveErrors.Len(), 0);
		}

		Object->InstancedObject = NewObject<UMetaStoryTest_PropertyObjectInstanced>();
		{
			FString ResolveErrors;
			TArray<FPropertyBindingPathIndirection> Indirections;
			const bool bResolveResult = Path.ResolveIndirectionsWithValue(FMetaStoryDataView(Object), Indirections, &ResolveErrors);

			AITEST_FALSE(TEXT("Resolve path should fail"), bResolveResult);
			AITEST_EQUAL(TEXT("Should have 0 indirections"), Indirections.Num(), 0);
			AITEST_NOT_EQUAL(TEXT("Should have error"), ResolveErrors.Len(), 0);
		}
		
		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_PropertyPathWrongObject, "System.MetaStory.PropertyPath.WrongObject");

struct FMetaStoryTest_PropertyPathArray : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		FPropertyBindingPath Path;
		const bool bParseResult = Path.FromString(TEXT("ArrayOfInts[1]"));

		AITEST_TRUE(TEXT("Parsing path should succeeed"), bParseResult);
		AITEST_EQUAL(TEXT("Should have 1 path segments"), Path.NumSegments(), 1);

		UMetaStoryTest_PropertyObject* Object = NewObject<UMetaStoryTest_PropertyObject>();
		Object->ArrayOfInts.Add(42);
		Object->ArrayOfInts.Add(123);

		FString ResolveErrors;
		TArray<FPropertyBindingPathIndirection> Indirections;
		const bool bResolveResult = Path.ResolveIndirectionsWithValue(FMetaStoryDataView(Object), Indirections, &ResolveErrors);

		AITEST_TRUE(TEXT("Resolve path should succeeed"), bResolveResult);
		AITEST_EQUAL(TEXT("Should have no resolve errors"), ResolveErrors.Len(), 0);
		AITEST_EQUAL(TEXT("Should have 2 indirections"), Indirections.Num(), 2);
		AITEST_EQUAL(TEXT("Indirection 0 should be IndexArray type"), Indirections[0].GetAccessType(), EPropertyBindingPropertyAccessType::IndexArray);
		AITEST_EQUAL(TEXT("Indirection 1 should be Offset type"), Indirections[1].GetAccessType(), EPropertyBindingPropertyAccessType::Offset);

		const int32 Value = *reinterpret_cast<const int32*>(Indirections[1].GetPropertyAddress());
		AITEST_EQUAL(TEXT("Value should be 123"), Value, 123);
		
		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_PropertyPathArray, "System.MetaStory.PropertyPath.Array");

struct FMetaStoryTest_PropertyPathArrayInvalidIndex : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		FPropertyBindingPath Path;
		const bool bParseResult = Path.FromString(TEXT("ArrayOfInts[123]"));

		AITEST_TRUE(TEXT("Parsing path should succeeed"), bParseResult);
		AITEST_EQUAL(TEXT("Should have 1 path segments"), Path.NumSegments(), 1);

		UMetaStoryTest_PropertyObject* Object = NewObject<UMetaStoryTest_PropertyObject>();
		Object->ArrayOfInts.Add(42);
		Object->ArrayOfInts.Add(123);

		FString ResolveErrors;
		TArray<FPropertyBindingPathIndirection> Indirections;
		const bool bResolveResult = Path.ResolveIndirectionsWithValue(FMetaStoryDataView(Object), Indirections, &ResolveErrors);

		AITEST_FALSE(TEXT("Resolve path should fail"), bResolveResult);
		
		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_PropertyPathArrayInvalidIndex, "System.MetaStory.PropertyPath.ArrayInvalidIndex");

struct FMetaStoryTest_PropertyPathArrayOfStructs : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		FPropertyBindingPath Path1;
		Path1.FromString(TEXT("ArrayOfStruct[0].B"));

		FPropertyBindingPath Path2;
		Path2.FromString(TEXT("ArrayOfStruct[2].StructB.B"));

		UMetaStoryTest_PropertyObject* Object = NewObject<UMetaStoryTest_PropertyObject>();
		Object->ArrayOfStruct.AddDefaulted_GetRef().B = 3;
		Object->ArrayOfStruct.AddDefaulted();
		Object->ArrayOfStruct.AddDefaulted_GetRef().StructB.B = 42;

		{
			FString ResolveErrors;
			TArray<FPropertyBindingPathIndirection> Indirections;
			const bool bResolveResult = Path1.ResolveIndirectionsWithValue(FMetaStoryDataView(Object), Indirections, &ResolveErrors);

			AITEST_TRUE(TEXT("Resolve path1 should succeeed"), bResolveResult);
			AITEST_EQUAL(TEXT("Should have no resolve errors"), ResolveErrors.Len(), 0);
			AITEST_EQUAL(TEXT("Should have 3 indirections"), Indirections.Num(), 3);
			AITEST_EQUAL(TEXT("Indirection 0 should be ArrayIndex type"), Indirections[0].GetAccessType(), EPropertyBindingPropertyAccessType::IndexArray);
			AITEST_EQUAL(TEXT("Indirection 1 should be Offset type"), Indirections[1].GetAccessType(), EPropertyBindingPropertyAccessType::Offset);
			AITEST_EQUAL(TEXT("Indirection 2 should be Offset type"), Indirections[2].GetAccessType(), EPropertyBindingPropertyAccessType::Offset);

			const int32 Value = *reinterpret_cast<const int32*>(Indirections[2].GetPropertyAddress());
			AITEST_EQUAL(TEXT("Value should be 3"), Value, 3);
		}

		{
			FString ResolveErrors;
			TArray<FPropertyBindingPathIndirection> Indirections;
			const bool bResolveResult = Path2.ResolveIndirectionsWithValue(FMetaStoryDataView(Object), Indirections, &ResolveErrors);

			AITEST_TRUE(TEXT("Resolve path2 should succeeed"), bResolveResult);
			AITEST_EQUAL(TEXT("Should have no resolve errors"), ResolveErrors.Len(), 0);
			AITEST_EQUAL(TEXT("Should have 4 indirections"), Indirections.Num(), 4);
			AITEST_EQUAL(TEXT("Indirection 0 should be ArrayIndex type"), Indirections[0].GetAccessType(), EPropertyBindingPropertyAccessType::IndexArray);
			AITEST_EQUAL(TEXT("Indirection 1 should be Offset type"), Indirections[1].GetAccessType(), EPropertyBindingPropertyAccessType::Offset);
			AITEST_EQUAL(TEXT("Indirection 2 should be Offset type"), Indirections[2].GetAccessType(), EPropertyBindingPropertyAccessType::Offset);
			AITEST_EQUAL(TEXT("Indirection 3 should be Offset type"), Indirections[3].GetAccessType(), EPropertyBindingPropertyAccessType::Offset);

			const int32 Value = *reinterpret_cast<const int32*>(Indirections[3].GetPropertyAddress());
			AITEST_EQUAL(TEXT("Value should be 42"), Value, 42);
		}
		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_PropertyPathArrayOfStructs, "System.MetaStory.PropertyPath.ArrayOfStructs");

struct FMetaStoryTest_PropertyPathArrayOfInstancedObjects : FMetaStoryTestBase
{
	virtual bool InstantTest() override
	{
		FPropertyBindingPath Path;
		Path.FromString(TEXT("ArrayOfInstancedStructs[0].B"));

		FMetaStoryTest_PropertyStruct Struct;
		Struct.B = 123;
		
		UMetaStoryTest_PropertyObject* Object = NewObject<UMetaStoryTest_PropertyObject>();
		Object->ArrayOfInstancedStructs.Emplace(FConstStructView::Make(Struct));

		const bool bUpdateResult = Path.UpdateSegmentsFromValue(FMetaStoryDataView(Object));
		AITEST_TRUE(TEXT("Update instance types should succeeed"), bUpdateResult);
		AITEST_EQUAL(TEXT("Should have 2 path segments"), Path.NumSegments(), 2);
		AITEST_TRUE(TEXT("Path segment 0 instance type should be FMetaStoryTest_PropertyStruct"), Path.GetSegment(0).GetInstanceStruct() == FMetaStoryTest_PropertyStruct::StaticStruct());
		AITEST_TRUE(TEXT("Path segment 1 instance type should be nullptr"), Path.GetSegment(1).GetInstanceStruct() == nullptr);

		{
			FString ResolveErrors;
			TArray<FPropertyBindingPathIndirection> Indirections;
			const bool bResolveResult = Path.ResolveIndirections(UMetaStoryTest_PropertyObject::StaticClass(), Indirections, &ResolveErrors);

			AITEST_TRUE(TEXT("Resolve path should succeeed"), bResolveResult);
			AITEST_EQUAL(TEXT("Should have no resolve errors"), ResolveErrors.Len(), 0);
			AITEST_EQUAL(TEXT("Should have 3 indirections"), Indirections.Num(), 3);
			AITEST_EQUAL(TEXT("Indirection 0 should be ArrayIndex type"), Indirections[0].GetAccessType(), EPropertyBindingPropertyAccessType::IndexArray);
			AITEST_EQUAL(TEXT("Indirection 1 should be StructInstance type"), Indirections[1].GetAccessType(), EPropertyBindingPropertyAccessType::StructInstance);
			AITEST_EQUAL(TEXT("Indirection 2 should be Offset type"), Indirections[2].GetAccessType(), EPropertyBindingPropertyAccessType::Offset);
		}

		{
			FString ResolveErrors;
			TArray<FPropertyBindingPathIndirection> Indirections;
			const bool bResolveResult = Path.ResolveIndirectionsWithValue(FMetaStoryDataView(Object), Indirections, &ResolveErrors);

			AITEST_TRUE(TEXT("Resolve path should succeeed"), bResolveResult);
			AITEST_EQUAL(TEXT("Should have no resolve errors"), ResolveErrors.Len(), 0);
			AITEST_EQUAL(TEXT("Should have 3 indirections"), Indirections.Num(), 3);
			AITEST_EQUAL(TEXT("Indirection 0 should be ArrayIndex type"), Indirections[0].GetAccessType(), EPropertyBindingPropertyAccessType::IndexArray);
			AITEST_EQUAL(TEXT("Indirection 1 should be StructInstance type"), Indirections[1].GetAccessType(), EPropertyBindingPropertyAccessType::StructInstance);
			AITEST_EQUAL(TEXT("Indirection 2 should be Offset type"), Indirections[2].GetAccessType(), EPropertyBindingPropertyAccessType::Offset);

			const int32 Value = *reinterpret_cast<const int32*>(Indirections[2].GetPropertyAddress());
			AITEST_EQUAL(TEXT("Value should be 123"), Value, 123);
		}
		
		return true;
	}
};
IMPLEMENT_METASTORY_INSTANT_TEST(FMetaStoryTest_PropertyPathArrayOfInstancedObjects, "System.MetaStory.PropertyPath.ArrayOfInstancedObjects");

} // namespace UE::MetaStory::Tests

UE_ENABLE_OPTIMIZATION_SHIP

#undef LOCTEXT_NAMESPACE
