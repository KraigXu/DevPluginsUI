// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Flow/MetaStoryFlow.h"
#include "MetaStoryState.h"
#include "MetaStoryEditorPropertyBindings.h"
#include "MetaStoryEditorTypes.h"
#include "Debugger/MetaStoryDebuggerTypes.h"
#include "MetaStoryEditorData.generated.h"

#define UE_API METASTORYEDITORMODULE_API

struct FMetaStoryBindableStructDesc;

class UMetaStoryEditorDataExtension;
class UMetaStoryEditorSchema;
class UMetaStorySchema;

namespace UE::MetaStory::Editor
{
	// Name used to describe container of global items (other items use the path to the container State).  
	extern METASTORYEDITORMODULE_API const FString GlobalStateName;

	// Name used to describe container of property functions.
	extern METASTORYEDITORMODULE_API const FString PropertyFunctionStateName;
}

namespace UE::MetaStory::Compiler::Private
{
	class FCompilerManagerImpl;
}

USTRUCT()
struct FMetaStoryEditorBreakpoint
{
	GENERATED_BODY()

	FMetaStoryEditorBreakpoint() = default;
	explicit FMetaStoryEditorBreakpoint(const FGuid& ID, const EMetaStoryBreakpointType BreakpointType)
		: ID(ID)
		, BreakpointType(BreakpointType)
	{
	}

	/** Unique Id of the Node or State associated to the breakpoint. */
	UPROPERTY()
	FGuid ID;

	/** The event type that should trigger the breakpoint (e.g. OnEnter, OnExit, etc.). */
	UPROPERTY()
	EMetaStoryBreakpointType BreakpointType = EMetaStoryBreakpointType::Unset;
};

UENUM()
enum class EMetaStoryVisitor : uint8
{
	Continue,
	Break,
};

/**
 * Edit time data for MetaStory asset. This data gets baked into runtime format before being used by the MetaStoryInstance.
 */
UCLASS(MinimalAPI, BlueprintType, EditInlineNew, CollapseCategories, Within = "MetaStory", meta = (DisallowLevelActorReference = true))
class UMetaStoryEditorData : public UObject, public IMetaStoryEditorPropertyBindingsOwner
{
	GENERATED_BODY()
	
public:
	UE_API UMetaStoryEditorData();

	UE_API virtual void PostInitProperties() override;
	UE_API virtual void Serialize(FArchive& Ar) override;
	
	//~ Begin IMetaStoryEditorPropertyBindingsOwner interface
	UE_API virtual void GetBindableStructs(const FGuid TargetStructID, TArray<TInstancedStruct<FPropertyBindingBindableStructDescriptor>>& OutStructDescs) const override;
	UE_API virtual bool GetBindableStructByID(const FGuid StructID, TInstancedStruct<FPropertyBindingBindableStructDescriptor>& OutStructDesc) const override;
	UE_API virtual bool GetBindingDataViewByID(const FGuid StructID, FPropertyBindingDataView& OutDataView) const override;
	virtual const FMetaStoryEditorPropertyBindings* GetPropertyEditorBindings() const override
	{
		return &EditorBindings;
	}

	virtual FPropertyBindingBindingCollection* GetEditorPropertyBindings() override
	{
		return &EditorBindings;
	}

	virtual const FPropertyBindingBindingCollection* GetEditorPropertyBindings() const override
	{
		return &EditorBindings;
	}

	virtual FMetaStoryEditorPropertyBindings* GetPropertyEditorBindings() override
	{
		return &EditorBindings;
	}

	UE_API virtual FMetaStoryBindableStructDesc FindContextData(const UStruct* ObjectType, const FString ObjectNameHint) const override;

	UE_API virtual bool CanCreateParameter(const FGuid InStructID) const override;
	UE_API virtual void CreateParametersForStruct(const FGuid InStructID, TArrayView<UE::PropertyBinding::FPropertyCreationDescriptor> InOutCreationDescs) override;

	// todo: (jira UE-337309) we have many sites that manipulate state/node which will change bindings, but not calling this function. Currently it is only called when you change binding from the details view
	UE_API virtual void OnPropertyBindingChanged(const FPropertyBindingPath& InSourcePath, const FPropertyBindingPath& InTargetPath) override;

	UE_API virtual void AppendBindablePropertyFunctionStructs(TArray<TInstancedStruct<FPropertyBindingBindableStructDescriptor>>& InOutStructs) const override;
	//~ End IMetaStoryEditorPropertyBindingsOwner interface

	/**
	 * Returns the description for the node for UI.
	 * Handles the name override logic, figures out required data for the GetDescription() call, and handles the fallbacks.
	 * @return description for the node.
	 */
	UE_API FText GetNodeDescription(const FMetaStoryEditorNode& Node, const EMetaStoryNodeFormatting Formatting = EMetaStoryNodeFormatting::Text) const;

#if WITH_EDITOR
	UE_API void OnParametersChanged(const UMetaStory& MetaStory);
	UE_API void OnStateParametersChanged(const UMetaStory& MetaStory, const FGuid StateID);
	UE_API virtual void BeginDestroy() override;
	UE_API virtual void PostLoad() override;
	UE_API virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	UE_API virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;

	/**
	 * 若 MetaStoryFlow 为空，则创建内嵌 UMetaStoryFlow（Outer=this）并带默认 Start 节点。
	 * 该字段不是外部资产引用，Details 中不会出现「选 .uasset」；需内嵌实例或由此函数自动创建。
	 */
	UE_API void EnsureEmbeddedMetaStoryFlow();
#endif

	/** @return the public parameters ID that could be used for bindings within the Tree. */
	FGuid GetRootParametersGuid() const
	{
		return RootParametersGuid;
	}

	/** @return the public parameters that could be used for bindings within the Tree. */
	virtual const FInstancedPropertyBag& GetRootParametersPropertyBag() const
	{
		return RootParameterPropertyBag;
	}

	/** @returns parent state of a struct, or nullptr if not found. */
	UE_API const UMetaStoryState* GetStateByStructID(const FGuid TargetStructID) const;

	/** @returns state based on its ID, or nullptr if not found. */
	UE_API const UMetaStoryState* GetStateByID(const FGuid StateID) const;

	/** @returns mutable state based on its ID, or nullptr if not found. */
	UE_API UMetaStoryState* GetMutableStateByID(const FGuid StateID);

	/** @returns the IDs and instance values of all bindable structs in the MetaStory. */
	UE_API void GetAllStructValues(TMap<FGuid, const FMetaStoryDataView>& OutAllValues) const;

	/** @returns the IDs and instance values of all bindable structs in the MetaStory. */
	UE_API void GetAllStructValues(TMap<FGuid, const FPropertyBindingDataView>& OutAllValues) const;

	/**
	* Iterates over all structs that are related to binding
	* @param InFunc function called at each node, should return Continue if visiting is continued or Break to stop.
	*/
	UE_API EMetaStoryVisitor VisitHierarchy(TFunctionRef<EMetaStoryVisitor(UMetaStoryState& State, UMetaStoryState* ParentState)> InFunc) const;

	/**
	 * Iterates over all structs at the global level (context, tree parameters, evaluators, global tasks) that are related to binding.
	 * @param InFunc function called at each node, should return Continue if visiting is continued or Break to stop.
	 */
	UE_API EMetaStoryVisitor VisitGlobalNodes(TFunctionRef<EMetaStoryVisitor(const UMetaStoryState* State, const FMetaStoryBindableStructDesc& Desc, const FMetaStoryDataView Value)> InFunc) const;

	/**
	 * Iterates over all structs in the state hierarchy that are related to binding.
	 * @param InFunc function called at each node, should return Continue if visiting is continued or Break to stop.
	 */
	UE_API EMetaStoryVisitor VisitHierarchyNodes(TFunctionRef<EMetaStoryVisitor(const UMetaStoryState* State, const FMetaStoryBindableStructDesc& Desc, const FMetaStoryDataView Value)> InFunc) const;

	/**
	 * Iterates over all structs that are related to binding.
	 * @param InFunc function called at each node, should return Continue if visiting is continued or Break to stop.
	 */
	UE_API EMetaStoryVisitor VisitAllNodes(TFunctionRef<EMetaStoryVisitor(const UMetaStoryState* State, const FMetaStoryBindableStructDesc& Desc, const FMetaStoryDataView Value)> InFunc) const;

	/**
	 * Iterates over all nodes in a given state.
	 * @param InFunc function called at each node, should return Continue if visiting is continued or Break to stop.
	 */
	UE_API EMetaStoryVisitor VisitStateNodes(const UMetaStoryState& State, TFunctionRef<EMetaStoryVisitor(const UMetaStoryState* State, const FMetaStoryBindableStructDesc& Desc, const FMetaStoryDataView Value)> InFunc) const;

	/**
	 * Iterates recursively over all property functions of the provided node. Also nested ones.
	 * @param InFunc function called at each node, should return Continue if visiting is continued or Break to stop.
	 */
	UE_API EMetaStoryVisitor VisitStructBoundPropertyFunctions(FGuid StructID, const FString& StatePath, TFunctionRef<EMetaStoryVisitor(const FMetaStoryEditorNode& EditorNode, const FMetaStoryBindableStructDesc& Desc, const FMetaStoryDataView Value)> InFunc) const;

	/**
	 * Returns array of nodes along the execution path, up to the TargetStruct.
	 * @param Path The states to visit during the check
	 * @param TargetStructID The ID of the node where to stop.
	 * @param OutStructDescs Array of nodes accessible on the given path.
	 */
	UE_API void GetAccessibleStructsInExecutionPath(const TConstArrayView<const UMetaStoryState*> Path, const FGuid TargetStructID, TArray<TInstancedStruct<FPropertyBindingBindableStructDescriptor>>& OutStructDescs) const;

	/**
	 * Iterates over all template property function nodes.
	 * Note it is different from calling VisitStructBoundPropertyFunctions which returns the instanced property functions for a node.
	 * @param InFunc function called at each node, should return Continue if visiting is continued or Break to stop.
	 */
	UE_API virtual EMetaStoryVisitor EnumerateBindablePropertyFunctionNodes(TFunctionRef<EMetaStoryVisitor(const UScriptStruct* NodeStruct, const FMetaStoryBindableStructDesc& Desc, const FMetaStoryDataView Value)> InFunc) const override;

	UE_DEPRECATED(5.6, "Use GetAccessibleStructsInExecutionPath instead")
	void GetAccessibleStruct(const TConstArrayView<const UMetaStoryState*> Path, const FGuid TargetStructID, TArray<TInstancedStruct<FPropertyBindingBindableStructDescriptor>>& OutStructDescs) const
	{
		GetAccessibleStructsInExecutionPath(Path, TargetStructID, OutStructDescs);
	}

	/** Find the first extension of the requested type. */
	template<typename ExtensionType>
	ExtensionType* GetExtension()
	{
		return CastChecked<ExtensionType>(K2_GetExtension(ExtensionType::StaticClass()), ECastCheckedType::NullAllowed);
	}

	/** Find the first extension of the requested type. */
	UFUNCTION(BlueprintCallable, Category = "MetaStory|Extension", Meta = (DisplayName="Get Extension", DeterminesOutputType = "ExtensionType"))
	UE_API UMetaStoryEditorDataExtension* K2_GetExtension(TSubclassOf<UMetaStoryEditorDataExtension> ExtensionType);

	UE_API void ReparentStates();
	
	// MetaStory Builder API

	/**
	 * Adds new Subtree with specified name.
	 * @return Pointer to the new Subtree.
	 */
	UMetaStoryState& AddSubTree(const FName Name)
	{
		UMetaStoryState* SubTreeState = NewObject<UMetaStoryState>(this, FName(), RF_Transactional);
		check(SubTreeState);
		SubTreeState->Name = Name;
		SubTrees.Add(SubTreeState);
		return *SubTreeState;
	}

	/**
	 * Adds new Subtree named "Root".
	 * @return Pointer to the new Subtree.
	 */
	UMetaStoryState& AddRootState()
	{
		return AddSubTree(FName(TEXT("Root")));
	}

	/**
	 * Adds Evaluator of specified type.
	 * @return reference to the new Evaluator. 
	 */
	template<typename T, typename... TArgs>
	TMetaStoryTypedEditorNode<T>& AddEvaluator(TArgs&&... InArgs)
	{
		FMetaStoryEditorNode& EditorNode = Evaluators.AddDefaulted_GetRef();
		EditorNode.ID = FGuid::NewGuid();
		EditorNode.Node.InitializeAs<T>(Forward<TArgs>(InArgs)...);
		T& Eval = EditorNode.Node.GetMutable<T>();
		if (const UScriptStruct* InstanceType = Cast<const UScriptStruct>(Eval.GetInstanceDataType()))
		{
			EditorNode.Instance.InitializeAs(InstanceType);
		}
		if (const UScriptStruct* InstanceType = Cast<const UScriptStruct>(Eval.GetExecutionRuntimeDataType()))
		{
			EditorNode.ExecutionRuntimeData.InitializeAs(InstanceType);
		}
		return static_cast<TMetaStoryTypedEditorNode<T>&>(EditorNode);
	}

	/**
	 * Adds Global Task of specified type.
	 * @return reference to the new task. 
	 */
	template<typename T, typename... TArgs>
	TMetaStoryTypedEditorNode<T>& AddGlobalTask(TArgs&&... InArgs)
	{
		FMetaStoryEditorNode& EditorNode = GlobalTasks.AddDefaulted_GetRef();
		EditorNode.ID = FGuid::NewGuid();
		EditorNode.Node.InitializeAs<T>(Forward<TArgs>(InArgs)...);
		const FMetaStoryNodeBase& Task = EditorNode.Node.Get<FMetaStoryNodeBase>();
		if (const UScriptStruct* InstanceType = Cast<const UScriptStruct>(Task.GetInstanceDataType()))
		{
			EditorNode.Instance.InitializeAs(InstanceType);
		}
		if (const UScriptStruct* InstanceType = Cast<const UScriptStruct>(Task.GetExecutionRuntimeDataType()))
		{
			EditorNode.ExecutionRuntimeData.InitializeAs(InstanceType);
		}
		return static_cast<TMetaStoryTypedEditorNode<T>&>(EditorNode);
	}

	/**
	 * Adds property binding between two structs.
	 */
	void AddPropertyBinding(const FPropertyBindingPath& SourcePath, const FPropertyBindingPath& TargetPath)
	{
		EditorBindings.AddBinding(SourcePath, TargetPath);
	}

	/**
	 * Adds property binding to PropertyFunction of provided type.
	 */
	void AddPropertyBinding(const UScriptStruct* PropertyFunctionNodeStruct, TConstArrayView<FPropertyBindingPathSegment> SourcePathSegments, const FPropertyBindingPath& TargetPath)
	{
		EditorBindings.AddFunctionBinding(PropertyFunctionNodeStruct, SourcePathSegments, TargetPath);
	}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.6, "Use the version taking FPropertyBindingPath instead")
	void AddPropertyBinding(const FMetaStoryPropertyPath& SourcePath, const FMetaStoryPropertyPath& TargetPath)
	{
		EditorBindings.AddBinding(SourcePath, TargetPath);
	}

	UE_DEPRECATED(5.6, "Use the version taking FPropertyBindingPath instead")
	void AddPropertyBinding(const UScriptStruct* PropertyFunctionNodeStruct, TConstArrayView<FPropertyBindingPathSegment> SourcePathSegments, const FMetaStoryPropertyPath& TargetPath)
	{
		EditorBindings.AddFunctionBinding(PropertyFunctionNodeStruct, SourcePathSegments, TargetPath);
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/**
	 * Adds property binding between two structs.
	 */
	bool AddPropertyBinding(const FMetaStoryEditorNode& SourceNode, const FString SourcePathStr, const FMetaStoryEditorNode& TargetNode, const FString TargetPathStr)
	{
		FPropertyBindingPath SourcePath;
		FPropertyBindingPath TargetPath;
		SourcePath.SetStructID(SourceNode.ID);
		TargetPath.SetStructID(TargetNode.ID);
		if (SourcePath.FromString(SourcePathStr) && TargetPath.FromString(TargetPathStr))
		{
			EditorBindings.AddBinding(SourcePath, TargetPath);
			return true;
		}
		return false;
	}

#if WITH_METASTORY_TRACE_DEBUGGER
	UE_API bool HasAnyBreakpoint(FGuid ID) const;
	UE_API bool HasBreakpoint(FGuid ID, EMetaStoryBreakpointType BreakpointType) const;
	UE_API const FMetaStoryEditorBreakpoint* GetBreakpoint(FGuid ID, EMetaStoryBreakpointType BreakpointType) const;
	UE_API void AddBreakpoint(FGuid ID, EMetaStoryBreakpointType BreakpointType);
	UE_API bool RemoveBreakpoint(FGuid ID, EMetaStoryBreakpointType BreakpointType);
	UE_API void RemoveAllBreakpoints();
#endif // WITH_METASTORY_TRACE_DEBUGGER

	// ~MetaStory Builder API

	/**
	 * Attempts to find a Color matching the provided Color Key
	 */
	const FMetaStoryEditorColor* FindColor(const FMetaStoryEditorColorRef& ColorRef) const
	{
		return Colors.Find(FMetaStoryEditorColor(ColorRef));
	}

	virtual void CreateRootProperties(TArrayView<UE::PropertyBinding::FPropertyCreationDescriptor> InOutCreationDescs) { UE::PropertyBinding::CreateUniquelyNamedPropertiesInPropertyBag(InOutCreationDescs, RootParameterPropertyBag); }

private:
	UE_API void FixObjectInstances(TSet<UObject*>& SeenObjects, UObject& Outer, FMetaStoryEditorNode& Node);
	UE_API void FixObjectNodes();
	UE_API void FixDuplicateIDs();
	UE_API void DuplicateIDs();
	UE_API void UpdateBindingsInstanceStructs();
	UE_API void CallPostLoadOnNodes();

#if WITH_EDITORONLY_DATA
	FDelegateHandle OnParametersChangedHandle;
	FDelegateHandle OnStateParametersChangedHandle;
#endif

public:
	/** Schema describing which inputs, evaluators, and tasks a MetaStory can contain. */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Common", NoClear)
	TObjectPtr<UMetaStorySchema> Schema = nullptr;
	
	/** 内嵌于本 EditorData 的流程图（Outer=this），非外部资产引用。 */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Common", NoClear)
	TObjectPtr<UMetaStoryFlow> MetaStoryFlow = nullptr;
	
	/** Schema describing how the editor schema is customized. */
	UPROPERTY(Instanced, NoClear)
	TObjectPtr<UMetaStoryEditorSchema> EditorSchema = nullptr;
	
	/** The editor data extensions. A place to add extra information for plugins. */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Extension", NoClear)
	TArray<TObjectPtr<UMetaStoryEditorDataExtension>> Extensions;

	/** Public parameters that could be used for bindings within the Tree. */
	UE_DEPRECATED(5.6, "Public access to RootParameters is deprecated. Use GetRootParametersPropertyBag")
	UPROPERTY(meta = (DeprecatedProperty))
	FMetaStoryStateParameters RootParameters;

private:
	/** Public parameters ID that could be used for bindings within the Tree. */
	UPROPERTY()
	FGuid RootParametersGuid;

	/** Public parameters property bag that could be used for bindings within the Tree. */
	UPROPERTY(EditDefaultsOnly, Category = Parameters)
	FInstancedPropertyBag RootParameterPropertyBag;

public:
	UPROPERTY(EditDefaultsOnly, Category = "Evaluators", meta = (BaseStruct = "/Script/MetaStoryModule.MetaStoryEvaluatorBase", BaseClass = "/Script/MetaStoryModule.MetaStoryEvaluatorBlueprintBase"))
	TArray<FMetaStoryEditorNode> Evaluators;

	UPROPERTY(EditDefaultsOnly, Category = "Global Tasks", meta = (BaseStruct = "/Script/MetaStoryModule.MetaStoryTaskBase", BaseClass = "/Script/MetaStoryModule.MetaStoryTaskBlueprintBase"))
	TArray<FMetaStoryEditorNode> GlobalTasks;

	UPROPERTY(EditDefaultsOnly, Category = "Global Tasks")
	EMetaStoryTaskCompletionType GlobalTasksCompletion = EMetaStoryTaskCompletionType::Any;

	UPROPERTY()
	FMetaStoryEditorPropertyBindings EditorBindings;

	/** Color Options to assign to a State */
	UPROPERTY(EditDefaultsOnly, Category = "Theme")
	TSet<FMetaStoryEditorColor> Colors;

	/** Top level States. */
	UPROPERTY(Instanced)
	TArray<TObjectPtr<UMetaStoryState>> SubTrees;

	/**
	 * Transient list of breakpoints added in the debugging session.
	 * These will be lost if the asset gets reloaded.
	 * If there is eventually a change to make those persist with the asset
	 * we need to prune all dangling breakpoints after states/tasks got removed.
	 */
	UPROPERTY(Transient)
	TArray<FMetaStoryEditorBreakpoint> Breakpoints;

	/**
	 * List of the previous compiled delegate dispatchers.
	 * Saved in the editor data to be duplicated transient.
	 */
	UPROPERTY(DuplicateTransient)
	TArray<FMetaStoryEditorDelegateDispatcherCompiledBinding> CompiledDispatchers;

	friend class FMetaStoryEditorDataDetails;
	friend class UE::MetaStory::Compiler::Private::FCompilerManagerImpl;
};


UCLASS()
class UQAMetaStoryEditorData : public UMetaStoryEditorData
{
	GENERATED_BODY()
};

#undef UE_API
