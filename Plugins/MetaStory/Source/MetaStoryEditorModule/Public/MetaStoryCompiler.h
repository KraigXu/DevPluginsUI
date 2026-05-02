// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryPropertyBindingCompiler.h"

#define UE_API METASTORYEDITORMODULE_API

struct FStructView;

enum class EMetaStoryExpressionOperand : uint8;
enum class EMetaStoryPropertyUsage : uint8;
struct FMetaStoryDataView;
struct FMetaStoryStateHandle;

class UMetaStory;
class UMetaStoryState;
class UMetaStoryEditorData;
class UMetaStoryExtension;
class UMetaStorySchema;
struct FMetaStoryEditorNode;
struct FMetaStoryStateLink;
struct FMetaStoryNodeBase;

namespace UE::MetaStory::Compiler
{
	// Compiler steps: PrePublic, PostPublic, PreInternal, PostInternal

	/**
	 * Compiler context for when the MetaStory fully compiled and succeeded.
	 * This is the last compilation step.
	 */
	struct FPostInternalContext
	{
		virtual ~FPostInternalContext() = default;

		virtual TNotNull<const UMetaStory*> GetMetaStory() const = 0;
		virtual TNotNull<const UMetaStoryEditorData*> GetEditorData() const = 0;
		virtual FMetaStoryCompilerLog& GetLog() const = 0;

		virtual UMetaStoryExtension* AddExtension(TNotNull<TSubclassOf<UMetaStoryExtension>> ExtensionType) const = 0;
	};
}

/**
 * Helper class to convert MetaStory editor representation into a compact data.
 * Holds data needed during compiling.
 */
struct FMetaStoryCompiler
{
public:

	explicit FMetaStoryCompiler(FMetaStoryCompilerLog& InLog)
		: Log(InLog)
	{
	}

	UE_API bool Compile(TNotNull<UMetaStory*> MetaStory);
	UE_API bool Compile(UMetaStory& InMetaStory);

private:
	/** Resolves the state a transition points to, and the optional fallback for failing to enter the state. SourceState is nullptr for global tasks. */
	bool ResolveTransitionStateAndFallback(const UMetaStoryState* SourceState, const FMetaStoryStateLink& Link, FMetaStoryStateHandle& OutTransitionHandle, EMetaStorySelectionFallback& OutFallback) const;
	FMetaStoryStateHandle GetStateHandle(const FGuid& StateID) const;
	UMetaStoryState* GetState(const FGuid& StateID) const;

	bool CreateParameters();
	bool CreateStates();
	bool CreateStateRecursive(UMetaStoryState& State, const FMetaStoryStateHandle Parent);
	bool CreateEvaluators();
	bool CreateGlobalTasks();
	bool CreateStateTasksAndParameters();
	bool CreateStateTransitions();
	bool CreateStateConsiderations();
	
	bool CreateBindingsForNodes(TConstArrayView<FMetaStoryEditorNode> EditorNodes, FMetaStoryIndex16 NodesBegin, TArray<FInstancedStruct>& Instances);
	bool CreateBindingsForStruct(const FMetaStoryBindableStructDesc& TargetStruct, FMetaStoryDataView TargetValue, FMetaStoryIndex16 PropertyFuncsBegin, FMetaStoryIndex16 PropertyFuncsEnd, FMetaStoryIndex16& OutBatchIndex, FMetaStoryIndex16* OutOutputBindingBatchIndex = nullptr);

	bool CreatePropertyFunctionsForStruct(FGuid StructID);
	bool CreatePropertyFunction(const FMetaStoryEditorNode& FuncEditorNode);
	
	bool CreateConditions(UMetaStoryState& State, const FString& StatePath, TConstArrayView<FMetaStoryEditorNode> Conditions);
	bool CreateCondition(UMetaStoryState& State, const FString& StatePath, const FMetaStoryEditorNode& CondNode, const EMetaStoryExpressionOperand Operand, const int8 DeltaIndent);
	bool CreateConsiderations(UMetaStoryState& State, const FString& StatePath, TConstArrayView<FMetaStoryEditorNode> Considerations);
	bool CreateConsideration(UMetaStoryState& State, const FString& StatePath, const FMetaStoryEditorNode& ConsiderationNode, const EMetaStoryExpressionOperand Operand, const int8 DeltaIndent);
	bool CreateTask(UMetaStoryState* State, const FMetaStoryEditorNode& TaskNode, const FMetaStoryDataHandle TaskDataHandle);
	bool CreateEvaluator(const FMetaStoryEditorNode& EvalNode, const FMetaStoryDataHandle EvalDataHandle);

	FInstancedStruct* CreateNode(UMetaStoryState* State, const FMetaStoryEditorNode& EditorNode, FMetaStoryBindableStructDesc& InstanceDesc, const FMetaStoryDataHandle DataHandle, TArray<FInstancedStruct>& InstancedStructContainer);
	FInstancedStruct* CreateNodeWithSharedInstanceData(UMetaStoryState* State, const FMetaStoryEditorNode& EditorNode, FMetaStoryBindableStructDesc& InstanceDesc);
	TOptional<FMetaStoryDataView> CreateNodeInstanceData(const FMetaStoryEditorNode& EditorNode, FMetaStoryNodeBase& Node, FMetaStoryBindableStructDesc& InstanceDesc, const FMetaStoryDataHandle DataHandle, TArray<FInstancedStruct>& InstancedStructContainer);

	struct FValidatedPathBindings
	{
		TArray<FMetaStoryPropertyPathBinding> CopyBindings;
		TArray<FMetaStoryPropertyPathBinding> OutputCopyBindings;
		TArray<FMetaStoryPropertyPathBinding> DelegateDispatchers;
		TArray<FMetaStoryPropertyPathBinding> DelegateListeners;
		TArray<FMetaStoryPropertyPathBinding> ReferenceBindings;
	};

	bool GetAndValidateBindings(const FMetaStoryBindableStructDesc& TargetStruct, FMetaStoryDataView TargetValue, FValidatedPathBindings& OutValidatedBindings) const;
	bool ValidateStructRef(const FMetaStoryBindableStructDesc& SourceStruct, const FPropertyBindingPath& SourcePath,
	const FMetaStoryBindableStructDesc& TargetStruct, const FPropertyBindingPath& TargetPath) const;
	bool ValidateBindingOnNode(const FMetaStoryBindableStructDesc& TargetStruct, const FPropertyBindingPath& TargetPath) const;
	bool CompileAndValidateNode(const UMetaStoryState* SourceState, const FMetaStoryBindableStructDesc& InstanceDesc, FStructView NodeView, const FMetaStoryDataView InstanceData);

	void InstantiateStructSubobjects(FStructView Struct);
        
	bool NotifyInternalPost();
	
	void CreateBindingSourceStructsForNode(const FMetaStoryEditorNode& EditorNode, const FMetaStoryBindableStructDesc& InstanceDesc);
      
private:
	FMetaStoryCompilerLog& Log;
	TObjectPtr<UMetaStory> MetaStory = nullptr;
	TObjectPtr<UMetaStoryEditorData> EditorData = nullptr;
	TMap<FGuid, int32> IDToNode;
	TMap<FGuid, int32> IDToState;
	TMap<FGuid, int32> IDToTransition;
	TMap<FGuid, const FMetaStoryDataView > IDToStructValue;
	TArray<TObjectPtr<UMetaStoryState>> SourceStates;

	TArray<FInstancedStruct> Nodes;
	TArray<FInstancedStruct> InstanceStructs;
	TArray<FInstancedStruct> SharedInstanceStructs;
	TArray<FInstancedStruct> EvaluationScopeStructs;
	TArray<FInstancedStruct> ExecutionRuntimeStructs;
	
	/** Cached result of MakeCompletionTasksMask for global tasks. Indicates where state tasks should start. */
	int32 GlobalTaskEndBit = 0;

	FMetaStoryPropertyBindingCompiler BindingsCompiler;

	/** The Compile function executed. */
	bool bCompiled = false;
};


namespace UE::MetaStory::Compiler
{
	struct FValidationResult
	{
		FValidationResult() = default;
		FValidationResult(const bool bInResult, const int32 InValue, const int32 InMaxValue) : bResult(bInResult), Value(InValue), MaxValue(InMaxValue) {}

		/** Validation succeeded */
		bool DidSucceed() const { return bResult == true; }

		/** Validation failed */
		bool DidFail() const { return bResult == false; }

		/**
		 * Logs common validation for IsValidIndex16(), IsValidIndex8(), IsValidCount16(), IsValidCount8().
		 * @param Log reference to the compiler log.
		 * @param ContextText Text identifier for the context where the test is done.
		 * @param ContextStruct Struct identifier for the context where the test is done.
		 */
		void Log(FMetaStoryCompilerLog& Log, const TCHAR* ContextText, const FMetaStoryBindableStructDesc& ContextStruct = FMetaStoryBindableStructDesc()) const;
		
		bool bResult = true;
		int32 Value = 0;
		int32 MaxValue = 0;
	};

	/**
	 * Checks if given index can be represented as uint16, including MAX_uint16 as INDEX_NONE.
	 * @param Index Index to test
	 * @return validation result.
	 */
	inline FValidationResult IsValidIndex16(const int32 Index)
	{
		return FValidationResult(Index == INDEX_NONE || (Index >= 0 && Index < MAX_uint16), Index, MAX_uint16 - 1);
	}

	/**
	 * Checks if given index can be represented as uint8, including MAX_uint8 as INDEX_NONE. 
	 * @param Index Index to test
	 * @return true if the index is valid.
	 */
	inline FValidationResult IsValidIndex8(const int32 Index)
	{
		return FValidationResult(Index == INDEX_NONE || (Index >= 0 && Index < MAX_uint8), Index, MAX_uint8 - 1);
	}

	/**
	 * Checks if given count can be represented as uint16. 
	 * @param Count Count to test
	 * @return true if the count is valid.
	 */
	inline FValidationResult IsValidCount16(const int32 Count)
	{
		return FValidationResult(Count >= 0 && Count <= MAX_uint16, Count, MAX_uint16);
	}

	/**
	 * Checks if given count can be represented as uint8. 
	 * @param Count Count to test
	 * @return true if the count is valid.
	 */
	inline FValidationResult IsValidCount8(const int32 Count)
	{
		return FValidationResult(Count >= 0 && Count <= MAX_uint8, Count, MAX_uint8);
	}

	/**
	 * Returns UScriptStruct defined in "BaseStruct" metadata of given property.
	 * @param Property Handle to property where value is got from.
	 * @param OutBaseStructName Handle to property where value is got from.
	 * @return Script struct defined by the BaseStruct or nullptr if not found.
	 */
	const UScriptStruct* GetBaseStructFromMetaData(const FProperty* Property, FString& OutBaseStructName);

}; // UE::MetaStory::Compiler

#undef UE_API
