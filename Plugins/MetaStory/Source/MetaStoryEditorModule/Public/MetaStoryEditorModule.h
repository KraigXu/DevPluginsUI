// Copyright Epic Games, Inc. All Rights Reserved.

#pragma  once

#include "Modules/ModuleInterface.h"
#include "Logging/LogMacros.h"
#include "Templates/NonNullSubclassOf.h"
#include "Templates/PimplPtr.h"
#include "Toolkits/AssetEditorToolkit.h"

#define UE_API METASTORYEDITORMODULE_API

class UMetaStory;
class UMetaStoryEditorData;
class UMetaStoryEditorSchema;
class UMetaStorySchema;
class UUserDefinedStruct;
class IMetaStoryEditor;
struct FMetaStoryNodeClassCache;

namespace UE::MetaStory::Compiler
{
	struct FPostInternalContext;
}

namespace UE::MetaStoryDebugger
{
	class FRewindDebuggerPlaybackExtension;
	class FRewindDebuggerRecordingExtension;
	struct FRewindDebuggerTrackCreator;
}

METASTORYEDITORMODULE_API DECLARE_LOG_CATEGORY_EXTERN(LogMetaStoryEditor, Log, All);

/**
* The public interface to this module
*/
class FMetaStoryEditorModule : public IModuleInterface, public IHasMenuExtensibility, public IHasToolBarExtensibility
{
public:
	//~Begin IModuleInterface
	UE_API virtual void StartupModule() override;
	UE_API virtual void ShutdownModule() override;
	//~End IModuleInterface

	/** Gets this module, will attempt to load and should always exist. */
	static UE_API FMetaStoryEditorModule& GetModule();

	/** Gets this module, will not attempt to load and may not exist. */
	static UE_API FMetaStoryEditorModule* GetModulePtr();

	/** Creates an instance of MetaStory editor. Only virtual so that it can be called across the DLL boundary. */
	UE_API virtual TSharedRef<IMetaStoryEditor> CreateMetaStoryEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UMetaStory* MetaStory);

	/** Sets the Details View with required MetaStory detail property handlers. */
	static UE_API void SetDetailPropertyHandlers(IDetailsView& DetailsView);

	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() override
	{
		return MenuExtensibilityManager;
	}
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() override
	{
		return ToolBarExtensibilityManager;
	}

	UE_API TSharedPtr<FMetaStoryNodeClassCache> GetNodeClassCache();
	
	DECLARE_EVENT_OneParam(FMetaStoryEditorModule, FOnRegisterLayoutExtensions, FLayoutExtender&);
	FOnRegisterLayoutExtensions& OnRegisterLayoutExtensions()
	{
		return RegisterLayoutExtensions;
	}

	DECLARE_EVENT_OneParam(FMetaStoryEditorModule, FPostInternalCompile, const UE::MetaStory::Compiler::FPostInternalContext&);
	/**
	 * Handle post internal compilation for all MetaStory assets.
	 * The MetaStory asset compiled successfully.
	 * @note Use the UMetaStoryEditorExtension::HandlePostInternalCompile for controlling a single asset.
	 * @note Use the UMetaStoryEditorSchema::HandlePostInternalCompile for controlling a type of MetaStory asset.
	 */
	FPostInternalCompile& OnPostInternalCompile()
	{
		return PostInternalCompile;
	}

	/** Register the editor data type for a specific schema. */
	UE_API void RegisterEditorDataClass(TNonNullSubclassOf<const UMetaStorySchema> Schema, TNonNullSubclassOf<const UMetaStoryEditorData> EditorData);
	/** Unregister the editor data type for a specific schema. */
	UE_API void UnregisterEditorDataClass(TNonNullSubclassOf<const UMetaStorySchema> Schema);
	/** Get the editor data type for a specific schema. */
	UE_API TNonNullSubclassOf<UMetaStoryEditorData> GetEditorDataClass(TNonNullSubclassOf<const UMetaStorySchema> Schema) const;

	/** Register the editor schema type for a specific schema. */
	void RegisterEditorSchemaClass(TNonNullSubclassOf<const UMetaStorySchema> Schema, TNonNullSubclassOf<const UMetaStoryEditorSchema> EditorSchema);
	/** Unregister the editor schema type for a specific schema. */
	void UnregisterEditorSchemaClass(TNonNullSubclassOf<const UMetaStorySchema> Schema);
	/** Get the editor data type for a specific schema. */
	TNonNullSubclassOf<UMetaStoryEditorSchema> GetEditorSchemaClass(TNonNullSubclassOf<const UMetaStorySchema> Schema) const;

protected:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;
	TSharedPtr<FMetaStoryNodeClassCache> NodeClassCache;

	struct FEditorTypes
	{
		TWeakObjectPtr<const UClass> Schema;
		TWeakObjectPtr<const UClass> EditorData;
		TWeakObjectPtr<const UClass> EditorSchema;
		bool HasData() const;
	};
	TArray<FEditorTypes> EditorTypes;

#if WITH_METASTORY_TRACE_DEBUGGER
	TPimplPtr<UE::MetaStoryDebugger::FRewindDebuggerPlaybackExtension> RewindDebuggerPlaybackExtension;
	TPimplPtr<UE::MetaStoryDebugger::FRewindDebuggerTrackCreator> RewindDebuggerTrackCreator;
#endif  // WITH_METASTORY_TRACE_DEBUGGER

	FDelegateHandle OnPostEngineInitHandle;
	UE_DEPRECATED(5.7, "OnUserDefinedStructReinstancedHandle is not used.")
	FDelegateHandle OnUserDefinedStructReinstancedHandle;
	FOnRegisterLayoutExtensions RegisterLayoutExtensions;
	FPostInternalCompile PostInternalCompile;
};

#undef UE_API
