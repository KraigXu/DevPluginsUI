// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaStoryEditorModule.h"
#include "Blueprint/MetaStoryConditionBlueprintBase.h"
#include "Blueprint/MetaStoryConsiderationBlueprintBase.h"
#include "Blueprint/MetaStoryEvaluatorBlueprintBase.h"
#include "Blueprint/MetaStoryTaskBlueprintBase.h"
#include "Customizations/MetaStoryAnyEnumDetails.h"
#include "Customizations/MetaStoryBindingExtension.h"
#include "Customizations/MetaStoryBlueprintPropertyRefDetails.h"
#include "Customizations/MetaStoryEditorColorDetails.h"
#include "Customizations/MetaStoryEditorDataDetails.h"
#include "Customizations/MetaStoryEditorNodeDetails.h"
#include "Customizations/MetaStoryEnumValueScorePairsDetails.h"
#include "Customizations/MetaStoryEventDescDetails.h"
#include "Customizations/MetaStoryReferenceDetails.h"
#include "Customizations/MetaStoryReferenceOverridesDetails.h"
#include "Customizations/MetaStoryStateDetails.h"
#include "Customizations/MetaStoryStateLinkDetails.h"
#include "Customizations/MetaStoryStateParametersDetails.h"
#include "Customizations/MetaStoryTransitionDetails.h"
#include "Debugger/MetaStoryDebuggerCommands.h"
#include "Debugger/MetaStoryRewindDebuggerExtensions.h"
#include "Debugger/MetaStoryRewindDebuggerTrack.h"
#include "IRewindDebuggerExtension.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "MetaStory.h"
#include "MetaStoryCompilerLog.h"
#include "MetaStoryCompilerManager.h"
#include "MetaStoryDelegates.h"
#include "MetaStoryEditingSubsystem.h"
#include "MetaStoryEditor.h"
#include "MetaStoryEditorCommands.h"
#include "MetaStoryEditorData.h"
#include "MetaStoryEditorSchema.h"
#include "MetaStoryEditorStyle.h"
#include "MetaStoryNodeClassCache.h"
#include "MetaStoryPropertyFunctionBase.h"

#define LOCTEXT_NAMESPACE "MetaStoryEditor"

DEFINE_LOG_CATEGORY(LogMetaStoryEditor);

IMPLEMENT_MODULE(FMetaStoryEditorModule, MetaStoryEditorModule)

namespace UE::MetaStory::Editor
{
	// @todo Could we make this a IModularFeature?
	static bool CompileMetaStory(UMetaStory& MetaStory)
	{
		FMetaStoryCompilerLog Log;
		return UMetaStoryEditingSubsystem::CompileStateTree(&MetaStory, Log);
	}

	static TSharedRef<FMetaStoryNodeClassCache> InitNodeClassCache()
	{
		TSharedRef<FMetaStoryNodeClassCache> NodeClassCache = MakeShareable(new FMetaStoryNodeClassCache());
		NodeClassCache->AddRootScriptStruct(FMetaStoryEvaluatorBase::StaticStruct());
		NodeClassCache->AddRootScriptStruct(FMetaStoryTaskBase::StaticStruct());
		NodeClassCache->AddRootScriptStruct(FMetaStoryConditionBase::StaticStruct());
		NodeClassCache->AddRootScriptStruct(FMetaStoryConsiderationBase::StaticStruct());
		NodeClassCache->AddRootScriptStruct(FMetaStoryPropertyFunctionBase::StaticStruct());
		NodeClassCache->AddRootClass(UMetaStoryEvaluatorBlueprintBase::StaticClass());
		NodeClassCache->AddRootClass(UMetaStoryTaskBlueprintBase::StaticClass());
		NodeClassCache->AddRootClass(UMetaStoryConditionBlueprintBase::StaticClass());
		NodeClassCache->AddRootClass(UMetaStoryConsiderationBlueprintBase::StaticClass());
		NodeClassCache->AddRootClass(UMetaStorySchema::StaticClass());
		return NodeClassCache;
	}

}; // UE::MetaStory::Editor

FMetaStoryEditorModule& FMetaStoryEditorModule::GetModule()
{
	return FModuleManager::LoadModuleChecked<FMetaStoryEditorModule>("MetaStoryEditorModule");
}

FMetaStoryEditorModule* FMetaStoryEditorModule::GetModulePtr()
{
	return FModuleManager::GetModulePtr<FMetaStoryEditorModule>("MetaStoryEditorModule");
}

void FMetaStoryEditorModule::StartupModule()
{
	UE::MetaStory::Delegates::OnRequestEditorHash.BindLambda([](const UMetaStory& InMetaStory) -> uint32 { return UMetaStoryEditingSubsystem::CalculateStateTreeHash(&InMetaStory); });
	UE::MetaStory::Compiler::FCompilerManager::Startup();

	OnPostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddLambda([this]()
		{
			NodeClassCache = UE::MetaStory::Editor::InitNodeClassCache();
		});

#if WITH_METASTORY_TRACE_DEBUGGER
	FMetaStoryDebuggerCommands::Register();

	RewindDebuggerPlaybackExtension = MakePimpl<UE::MetaStoryDebugger::FRewindDebuggerPlaybackExtension>();
	IModularFeatures::Get().RegisterModularFeature(IRewindDebuggerExtension::ModularFeatureName, RewindDebuggerPlaybackExtension.Get());

	RewindDebuggerTrackCreator = MakePimpl<UE::MetaStoryDebugger::FRewindDebuggerTrackCreator>();
	IModularFeatures::Get().RegisterModularFeature(RewindDebugger::IRewindDebuggerTrackCreator::ModularFeatureName, RewindDebuggerTrackCreator.Get());
#endif // WITH_METASTORY_TRACE_DEBUGGER

	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	FMetaStoryEditorStyle::Register();
	FMetaStoryEditorCommands::Register();

	// Register the details customizer
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout("MetaStoryTransition", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMetaStoryTransitionDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("MetaStoryEventDesc", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMetaStoryEventDescDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("MetaStoryStateLink", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMetaStoryStateLinkDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("MetaStoryEditorNode", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMetaStoryEditorNodeDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("MetaStoryStateParameters", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMetaStoryStateParametersDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("MetaStoryAnyEnum", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMetaStoryAnyEnumDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("MetaStoryReference", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMetaStoryReferenceDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("MetaStoryReferenceOverrides", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMetaStoryReferenceOverridesDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("MetaStoryEditorColorRef", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMetaStoryEditorColorRefDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("MetaStoryEditorColor", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMetaStoryEditorColorDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("MetaStoryBlueprintPropertyRef", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMetaStoryBlueprintPropertyRefDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("MetaStoryEnumValueScorePairs", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMetaStoryEnumValueScorePairsDetails::MakeInstance));
	PropertyModule.RegisterCustomClassLayout("MetaStoryState", FOnGetDetailCustomizationInstance::CreateStatic(&FMetaStoryStateDetails::MakeInstance));
	PropertyModule.RegisterCustomClassLayout("MetaStoryEditorData", FOnGetDetailCustomizationInstance::CreateStatic(&FMetaStoryEditorDataDetails::MakeInstance));

	PropertyModule.NotifyCustomizationModuleChanged();
}

void FMetaStoryEditorModule::ShutdownModule()
{
	// Unregister the details customization
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout("MetaStoryTransition");
		PropertyModule.UnregisterCustomPropertyTypeLayout("MetaStoryEventDesc");
		PropertyModule.UnregisterCustomPropertyTypeLayout("MetaStoryStateLink");
		PropertyModule.UnregisterCustomPropertyTypeLayout("MetaStoryEditorNode");
		PropertyModule.UnregisterCustomPropertyTypeLayout("MetaStoryStateParameters");
		PropertyModule.UnregisterCustomPropertyTypeLayout("MetaStoryAnyEnum");
		PropertyModule.UnregisterCustomPropertyTypeLayout("MetaStoryReference");
		PropertyModule.UnregisterCustomPropertyTypeLayout("MetaStoryReferenceOverrides");
		PropertyModule.UnregisterCustomPropertyTypeLayout("MetaStoryEditorColorRef");
		PropertyModule.UnregisterCustomPropertyTypeLayout("MetaStoryEditorColor");
		PropertyModule.UnregisterCustomPropertyTypeLayout("MetaStoryBlueprintPropertyRef");
		PropertyModule.UnregisterCustomClassLayout("MetaStoryState");
		PropertyModule.UnregisterCustomClassLayout("MetaStoryEditorData");
		PropertyModule.NotifyCustomizationModuleChanged();
	}

	FMetaStoryEditorStyle::Unregister();
	FMetaStoryEditorCommands::Unregister();

	MenuExtensibilityManager.Reset();
	ToolBarExtensibilityManager.Reset();

#if WITH_METASTORY_TRACE_DEBUGGER
	IModularFeatures::Get().UnregisterModularFeature(RewindDebugger::IRewindDebuggerTrackCreator::ModularFeatureName, RewindDebuggerTrackCreator.Get());
	IModularFeatures::Get().UnregisterModularFeature(IRewindDebuggerExtension::ModularFeatureName, RewindDebuggerPlaybackExtension.Get());
	FMetaStoryDebuggerCommands::Unregister();
#endif // WITH_METASTORY_TRACE_DEBUGGER

	FCoreDelegates::OnPostEngineInit.Remove(OnPostEngineInitHandle);
	NodeClassCache.Reset();

	UE::MetaStory::Compiler::FCompilerManager::Shutdown();
	UE::MetaStory::Delegates::OnRequestEditorHash.Unbind();
}

TSharedRef<IMetaStoryEditor> FMetaStoryEditorModule::CreateMetaStoryEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UMetaStory* MetaStory)
{
	TSharedRef<FMetaStoryEditor> NewEditor(new FMetaStoryEditor());
	NewEditor->InitEditor(Mode, InitToolkitHost, MetaStory);
	return NewEditor;
}

void FMetaStoryEditorModule::SetDetailPropertyHandlers(IDetailsView& DetailsView)
{
	DetailsView.SetExtensionHandler(MakeShared<FMetaStoryBindingExtension>());
	DetailsView.SetChildrenCustomizationHandler(MakeShared<FMetaStoryBindingsChildrenCustomization>());
}

TSharedPtr<FMetaStoryNodeClassCache> FMetaStoryEditorModule::GetNodeClassCache()
{
	check(NodeClassCache.IsValid());
	return NodeClassCache;
}

bool FMetaStoryEditorModule::FEditorTypes::HasData() const
{
	return EditorData.IsValid() || EditorSchema.IsValid();
}

namespace UE::MetaStoryEditor::Private
{
	TOptional<int32> GetDepth(TNotNull<const UStruct*> Struct, TNotNull<const UStruct*> MatchingParent)
	{
		int32 Depth = 0;
		if (Struct == MatchingParent)
		{
			return Depth;
		}

		for (const UStruct* TempStruct : Struct->GetSuperStructIterator())
		{
			++Depth;
			if (TempStruct == MatchingParent)
			{
				return Depth;
			}
		}

		return {};
	}
}

void FMetaStoryEditorModule::RegisterEditorDataClass(TNonNullSubclassOf<const UMetaStorySchema> Schema, TNonNullSubclassOf<const UMetaStoryEditorData> EditorData)
{
	FEditorTypes* EditorType = EditorTypes.FindByPredicate(
		[Schema](const FEditorTypes& Other)
		{
			return Other.Schema == Schema.Get();
		});
	if (EditorType)
	{
		ensureMsgf(EditorType->EditorData.IsExplicitlyNull(), TEXT("The type %s is already registered."), *Schema.Get()->GetName());
		EditorType->EditorData = EditorData.Get();
	}
	else
	{
		FEditorTypes& NewEditorType = EditorTypes.AddDefaulted_GetRef();
		NewEditorType.Schema = Schema.Get();
		NewEditorType.EditorData = EditorData.Get();
	}
}

void FMetaStoryEditorModule::UnregisterEditorDataClass(TNonNullSubclassOf<const UMetaStorySchema> Schema)
{
	const int32 FoundIndex = EditorTypes.IndexOfByPredicate(
		[Schema](const FEditorTypes& Other)
		{
			return Other.Schema == Schema.Get();
		});
	if (FoundIndex != INDEX_NONE)
	{
		EditorTypes[FoundIndex].EditorData.Reset();
		if (!EditorTypes[FoundIndex].HasData())
		{
			EditorTypes.RemoveAtSwap(FoundIndex);
		}
	}
}

TNonNullSubclassOf<UMetaStoryEditorData> FMetaStoryEditorModule::GetEditorDataClass(TNonNullSubclassOf<const UMetaStorySchema> Schema) const
{
	int32 BestDepth = INT_MAX;
	const FEditorTypes* BestEditorType = nullptr;
	for (const FEditorTypes& EditorType : EditorTypes)
	{
		const UClass* OtherSchema = EditorType.Schema.Get();
		TOptional<int32> Depth = UE::MetaStoryEditor::Private::GetDepth(OtherSchema, Schema.Get());
		if (Depth.IsSet() && Depth.GetValue() < BestDepth)
		{
			BestDepth = Depth.GetValue();
			BestEditorType = &EditorType;
		}
	}

	const UClass* Result = BestEditorType && BestEditorType->EditorData.Get() ? BestEditorType->EditorData.Get() : UMetaStoryEditorData::StaticClass();
	return const_cast<UClass*>(Result); // NewObject wants none const UClass
}

void FMetaStoryEditorModule::RegisterEditorSchemaClass(TNonNullSubclassOf<const UMetaStorySchema> Schema, TNonNullSubclassOf<const UMetaStoryEditorSchema> EditorSchema)
{
	FEditorTypes* EditorType = EditorTypes.FindByPredicate(
		[Schema](const FEditorTypes& Other)
		{
			return Other.Schema == Schema.Get();
		});
	if (EditorType)
	{
		ensureMsgf(EditorType->EditorSchema.IsExplicitlyNull(), TEXT("The type %s is already registered."), *Schema.Get()->GetName());
		EditorType->EditorSchema = EditorSchema.Get();
	}
	else
	{
		FEditorTypes& NewEditorType = EditorTypes.AddDefaulted_GetRef();
		NewEditorType.Schema = Schema.Get();
		NewEditorType.EditorSchema = EditorSchema.Get();
	}
}

void FMetaStoryEditorModule::UnregisterEditorSchemaClass(TNonNullSubclassOf<const UMetaStorySchema> Schema)
{
	const int32 FoundIndex = EditorTypes.IndexOfByPredicate(
		[Schema](const FEditorTypes& Other)
		{
			return Other.Schema == Schema.Get();
		});
	if (FoundIndex != INDEX_NONE)
	{
		EditorTypes[FoundIndex].EditorSchema.Reset();
		if (!EditorTypes[FoundIndex].HasData())
		{
			EditorTypes.RemoveAtSwap(FoundIndex);
		}
	}
}

TNonNullSubclassOf<UMetaStoryEditorSchema> FMetaStoryEditorModule::GetEditorSchemaClass(TNonNullSubclassOf<const UMetaStorySchema> Schema) const
{
	int32 BestDepth = INT_MAX;
	const FEditorTypes* BestEditorType = nullptr;
	for (const FEditorTypes& OtherEditorType : EditorTypes)
	{
		const UClass* OtherSchema = OtherEditorType.Schema.Get();
		TOptional<int32> Depth = UE::MetaStoryEditor::Private::GetDepth(OtherSchema, Schema.Get());
		if (Depth.IsSet() && Depth.GetValue() < BestDepth)
		{
			BestDepth = Depth.GetValue();
			BestEditorType = &OtherEditorType;
		}
	}

	const UClass* Result = BestEditorType && BestEditorType->EditorSchema.Get() ? BestEditorType->EditorSchema.Get() : UMetaStoryEditorSchema::StaticClass();
	return const_cast<UClass*>(Result); // NewObject wants none const UClass
}

#undef LOCTEXT_NAMESPACE
