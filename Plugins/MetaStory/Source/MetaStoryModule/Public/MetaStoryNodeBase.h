// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaStoryTypes.h"
#include "MetaStoryNodeBase.generated.h"

struct FMetaStoryBindableStructDesc;
struct FMetaStoryLinker;
struct FPropertyBindingPath;
struct FMetaStoryPropertyPath;
struct IMetaStoryBindingLookup;

#if WITH_METASTORY_TRACE
#define SET_NODE_CUSTOM_TRACE_TEXT(Context, MergePolicy, Format, ...) \
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(MetaStoryDebugChannel)) \
	{ \
		Context.SetNodeCustomDebugTraceData( \
			UE::MetaStoryTrace::FNodeCustomDebugData(FString::Printf(Format, ##__VA_ARGS__) \
													, ::UE::MetaStoryTrace::FNodeCustomDebugData::EMergePolicy::MergePolicy)); \
	}
#else
#define SET_NODE_CUSTOM_TRACE_TEXT(...)
#endif //WITH_METASTORY_TRACE

#if WITH_EDITOR
namespace UE::MetaStory
{
	struct ICompileNodeContext
	{
		virtual ~ICompileNodeContext() {}
		virtual void AddValidationError(const FText& Message) = 0;
		virtual FMetaStoryDataView GetInstanceDataView() const = 0;
		virtual bool HasBindingForProperty(const FName PropertyName) const = 0;
	};
}
#endif

/**
 * Enum describing in what format a text is expected to be returned.
 *
 * - Normal text should be used for values
 * - Bold text should generally be used for actions, like name a of a task "<b>Play Animation</> {AnimName}".
 * - Subdued should be generally used for secondary/structural information, like "{Left} <s>equals</> {Right}".
 */
UENUM(BlueprintType)
enum class EMetaStoryNodeFormatting : uint8
{
	/**
	 * The returned text can contain following right text formatting (no nesting)
	 *	- <b>Bold</> (bolder font is used)
	 *	- <s>Subdued</> (normal font with lighter color) */
	RichText,
	
	/** The text should be unformatted */
	Text,
};

/**
 * Base struct of MetaStory Conditions, Considerations, Evaluators, and Tasks.
 */
USTRUCT()
struct FMetaStoryNodeBase
{
	GENERATED_BODY()

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FMetaStoryNodeBase() = default;
	FMetaStoryNodeBase(const FMetaStoryNodeBase&) = default;
	FMetaStoryNodeBase(FMetaStoryNodeBase&&) = default;
	FMetaStoryNodeBase& operator=(const FMetaStoryNodeBase&) = default;
	FMetaStoryNodeBase& operator=(FMetaStoryNodeBase&&) = default;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	struct FNoInstanceDataType
	{ };

	/**
	 * The instance data type. The implementation node should set its type.
	 * The type should match GetInstanceDataType().
	 */
	using FInstanceDataType = FNoInstanceDataType;

	/**
	 * The execution runtime data type.
	 * Execution runtime is optional. If needed, the implementation node should set its type.
	 * The type should match GetExecutionRuntimeDataType().
	 */
	using FExecutionRuntimeDataType = FNoInstanceDataType;

	virtual ~FMetaStoryNodeBase() {}

	/** @return Struct that represents the runtime data of the node. */
	virtual const UStruct* GetInstanceDataType() const
	{
		return nullptr;
	}

	/**
	 * Execution runtime data is always valid between an FMetaStoryExecutionContext::Start and FMetaStoryExecutionContext::Stop.
	 * If the node is no longer active but the instance (FMetaStoryInstanceData) is active, the data is still valid and will persist.
	 * @note Be careful about UObject reference, the objects are garbage collected like normal references.
	 * @return the struct that represents the persistent runtime data of the node.
	 */
	virtual const UStruct* GetExecutionRuntimeDataType() const
	{
		return nullptr;
	}

	/**
	 * Called when the MetaStory asset is linked. Allows to resolve references to other MetaStory data.
	 * @see TMetaStoryExternalDataHandle
	 * @param Linker Reference to the linker
	 * @return true if linking succeeded. 
	 */
	[[nodiscard]] virtual bool Link(FMetaStoryLinker& Linker)
	{
		return true;
	}

#if WITH_EDITOR
	/**
	 * Called during MetaStory compilation, allows to modify and validate the node and instance data.
	 * The method is called with node and instance that is duplicated during compilation and used at runtime (it's different than the data used in editor).
	 * @param ValidationMessages Any messages to report during validation. Displayed as errors if the validation result is Invalid, else as warnings.
	 * @param CompileContext
	 * @return Validation result based on if the validation succeeded or not. Returning Invalid will fail compilation and messages will be displayed as errors.
	 */
	virtual EDataValidationResult Compile(UE::MetaStory::ICompileNodeContext& CompileContext)
	{
		return EDataValidationResult::NotValidated;
	}

	/**
	 * Called during MetaStory compilation, allows to modify and validate the node and instance data.
	 * The method is called with node and instance that is duplicated during compilation and used at runtime (it's different than the data used in editor).  
	 * @param InstanceDataView Pointer to the instance data.
	 * @param ValidationMessages Any messages to report during validation. Displayed as errors if the validation result is Invalid, else as warnings.
	 * @return Validation result based on if the validation succeeded or not. Returning Invalid will fail compilation and messages will be displayed as errors.
	 */
	UE_DEPRECATED(5.6, "Use the version with Binding infos instead")
	virtual EDataValidationResult Compile(FMetaStoryDataView InstanceDataView, TArray<FText>& ValidationMessages) final { return EDataValidationResult::NotValidated; }

	/**
	 * Returns description for the node, use in the UI.
	 * The UI description is selected as follows: 
	 * - Node Name, if not empty
	 * - Description if not empty
	 * - Display name of the node struct
	 * @param ID ID of the item, can be used make property paths to this item.
	 * @param InstanceDataView View to the instance data, can be struct or class.
	 * @param BindingLookup Reference to binding lookup which can be used to reason about property paths.
	 * @param Formatting Requested formatting (whether rich or plain text should be returned).
	 */
	virtual FText GetDescription(const FGuid& ID, FMetaStoryDataView InstanceDataView, const IMetaStoryBindingLookup& BindingLookup, EMetaStoryNodeFormatting Formatting = EMetaStoryNodeFormatting::Text) const
	{
		return FText::GetEmpty();
	}

	/**
	 * @returns name of the icon in format:
	 *		StyleSetName | StyleName [ | SmallStyleName | StatusOverlayStyleName]
	 *		SmallStyleName and StatusOverlayStyleName are optional.
	 *		Example: "MetaStoryEditorStyle|Node.Animation" 
	 */
	virtual FName GetIconName() const
	{
		return FName();
	}

	/** @return the color to be used with the icon. */
	virtual FColor GetIconColor() const
	{
		return UE::MetaStory::Colors::DarkGrey;
	}

	/**
	 * Called when binding of any of the properties in the node changes.
	 * @param ID ID of the item, can be used make property paths to this item.
	 * @param InstanceDataView view to the instance data, can be struct or class.
	 * @param SourcePath Source path of the new binding.
	 * @param TargetPath Target path of the new binding (the property in the condition).
	 * @param BindingLookup Reference to binding lookup which can be used to reason about property paths.
	 */
	virtual void OnBindingChanged(const FGuid& ID, FMetaStoryDataView InstanceDataView, const FPropertyBindingPath& SourcePath, const FPropertyBindingPath& TargetPath, const IMetaStoryBindingLookup& BindingLookup) {}
	UE_DEPRECATED(5.6, "Use the version taking FPropertyBindingPath instead")
	virtual void OnBindingChanged(const FGuid& ID, FMetaStoryDataView InstanceDataView, const FMetaStoryPropertyPath& SourcePath, const FMetaStoryPropertyPath& TargetPath, const IMetaStoryBindingLookup& BindingLookup) final {}

	/**
	 * Called when a property of the node has been modified externally
	 * @param PropertyChangedEvent The event for the changed property. PropertyChain's active properties are set relative to node.
	 * @param InstanceData view to the instance data, can be struct or class.
	 */
	virtual void PostEditNodeChangeChainProperty(const FPropertyChangedChainEvent& PropertyChangedEvent, FMetaStoryDataView InstanceDataView) {}

	/**
	 * Called when a property of node's instance data has been modified externally
	 * @param PropertyChangedEvent The event for the changed property. PropertyChain's active properties are set relative to instance data.
	 * @param InstanceData view to the instance data, can be struct or class.
	 */
	virtual void PostEditInstanceDataChangeChainProperty(const FPropertyChangedChainEvent& PropertyChangedEvent, FMetaStoryDataView InstanceDataView) {}
#endif

	/**
	 * Called after the MetaStory asset that contains this node is loaded from disk.
	 * @param InstanceDataView view to the instance data, can be struct or class.
	 */
	virtual void PostLoad(FMetaStoryDataView InstanceDataView) {}

	/** Name of the node. */
	UPROPERTY(EditDefaultsOnly, Category = "", meta=(EditCondition = "false", EditConditionHides))
	FName Name;

	/** Property binding copy batch handle. */
	UPROPERTY()
	FMetaStoryIndex16 BindingsBatch = FMetaStoryIndex16::Invalid;

	/** Property output binding copy batch handle. */
	UPROPERTY()
	FMetaStoryIndex16 OutputBindingsBatch = FMetaStoryIndex16::Invalid;

	/** Index of template instance data for the node. Can point to Shared or Default instance data in MetaStory depending on node type. */
	UPROPERTY()
	FMetaStoryIndex16 InstanceTemplateIndex = FMetaStoryIndex16::Invalid;

	/** Index of template execution runtime data for the node. */
	UPROPERTY()
	FMetaStoryIndex16 ExecutionRuntimeTemplateIndex = FMetaStoryIndex16::Invalid;

	/** Data handle to access the instance data. */
	UPROPERTY()
	FMetaStoryDataHandle InstanceDataHandle = FMetaStoryDataHandle::Invalid;
};
