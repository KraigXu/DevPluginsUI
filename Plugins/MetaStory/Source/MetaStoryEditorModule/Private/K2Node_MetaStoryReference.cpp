// Copyright Epic Games, Inc. All Rights Reserved.

#include "K2Node_MetaStoryReference.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Variable.h"
#include "KismetCompiler.h"
#include "KismetCompilerMisc.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MetaStory.h"
#include "MetaStoryDelegates.h"
#include "MetaStoryFunctionLibrary.h"
#include "MetaStoryReference.h"
#include "StructUtils/PropertyBag.h"

#include "KismetNodes/SGraphNodeK2Default.h"
#include "SGraphPin.h"
#include "Styling/AppStyle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(K2Node_MetaStoryReference)

#define LOCTEXT_NAMESPACE "K2Node_MetaStoryReference"

namespace UE::MetaStoryEditor::Private
{
	static FLazyName MetaStoryPinName = "BA2CE32D97D46A3A524AC510A794C3C";

	bool IsPropertyPin(const UEdGraphPin* Pin)
	{
		if (Pin->ParentPin)
		{
			return IsPropertyPin(Pin->ParentPin);
		}

		return Pin->Direction == EEdGraphPinDirection::EGPD_Input
			&& Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec
			&& Pin->PinName != MetaStoryPinName;
	}

	bool CanUseProperty(const UEdGraphPin* Pin)
	{
		// A property needs to be linked to be considered.
		//The default value won't matches the value in the MetaStory asset.
		return !Pin->bOrphanedPin
			&& Pin->ParentPin == nullptr
			&& Pin->LinkedTo.Num() > 0;
	}

	bool DoRenamedPinsMatch(UMetaStory* MetaStory, const UEdGraphPin* NewPin, const UEdGraphPin* OldPin)
	{
		if (MetaStory == nullptr
			|| NewPin == nullptr
			|| OldPin == nullptr
			|| OldPin->Direction != NewPin->Direction)
		{
			return false;
		}

		const FInstancedPropertyBag& Parameters = MetaStory->GetDefaultParameters();
		if (!Parameters.IsValid())
		{
			return false;
		}

		const bool bCompatible = GetDefault<UEdGraphSchema_K2>()->ArePinTypesCompatible(NewPin->PinType, OldPin->PinType);
		if (!bCompatible)
		{
			return false;
		}

		UScriptStruct* Struct = const_cast<UScriptStruct*>(Parameters.GetValue().GetScriptStruct());
		return UK2Node_Variable::DoesRenamedVariableMatch(OldPin->PinName, NewPin->PinName, Struct);
	}
}

UK2Node_MakeMetaStoryReference::UK2Node_MakeMetaStoryReference()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		ParametersChangedHandle = UE::MetaStory::Delegates::OnPostCompile.AddUObject(this, &UK2Node_MakeMetaStoryReference::HandleMetaStoryCompiled);
	}
}

void UK2Node_MakeMetaStoryReference::BeginDestroy()
{
	UE::MetaStory::Delegates::OnPostCompile.Remove(ParametersChangedHandle);
	Super::BeginDestroy();
}

FText UK2Node_MakeMetaStoryReference::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	auto MakeNodeTitle = []()
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("StructName"), FMetaStoryReference::StaticStruct()->GetDisplayNameText());
			return FText::Format(LOCTEXT("MakeNodeTitle", "Make {StructName}"), Args);
		};
	static FText LocalCachedNodeTitle = MakeNodeTitle();
	return LocalCachedNodeTitle;
}

FText UK2Node_MakeMetaStoryReference::GetTooltipText() const
{
	auto MakeNodeTooltip = []()
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("StructName"), FMetaStoryReference::StaticStruct()->GetDisplayNameText());
			return FText::Format(LOCTEXT("MakeNodeTooltip", "Adds a node that create a  {StructName} from its members"), Args);
		};
	static FText LocalCachedTooltip = MakeNodeTooltip();
	return LocalCachedTooltip;
}

FText UK2Node_MakeMetaStoryReference::GetMenuCategory() const
{
	static FText LocalCachedCategory = FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::Struct);
	return LocalCachedCategory;
}

FSlateIcon UK2Node_MakeMetaStoryReference::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon LocalCachedIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.MakeStruct_16x");
	return LocalCachedIcon;
}

FLinearColor UK2Node_MakeMetaStoryReference::GetNodeTitleColor() const
{
	auto MakeTitleColor = []() -> FLinearColor
		{
			const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
			FEdGraphPinType PinType;
			PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
			PinType.PinSubCategoryObject = FMetaStoryReference::StaticStruct();
			return K2Schema->GetPinTypeColor(PinType);
		};

	static FLinearColor LocalCachedTitleColor = MakeTitleColor();
	return LocalCachedTitleColor;
}

void UK2Node_MakeMetaStoryReference::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
	UEdGraphPin* ReturnValuePin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Struct, FMetaStoryReference::StaticStruct(), UEdGraphSchema_K2::PN_ReturnValue);
	ReturnValuePin->PinFriendlyName = FMetaStoryReference::StaticStruct()->GetDisplayNameText();

	UEdGraphPin* MetaStoryPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UMetaStory::StaticClass(), UE::MetaStoryEditor::Private::MetaStoryPinName);
	MetaStoryPin->bNotConnectable = true;
	MetaStoryPin->PinFriendlyName = LOCTEXT("MetaStoryPinName", "MetaStory");

	CreatePropertyPins();
}

void UK2Node_MakeMetaStoryReference::CreatePropertyPins()
{
	if (MetaStory)
	{
		const FInstancedPropertyBag& Parameters = MetaStory->GetDefaultParameters();
		if (Parameters.IsValid())
		{
			UScriptStruct* Struct = const_cast<UScriptStruct*>(Parameters.GetValue().GetScriptStruct());
			FOptionalPinManager OptionalPinManager;
			OptionalPinManager.RebuildPropertyList(ShowPinForProperties, Struct);
			OptionalPinManager.CreateVisiblePins(ShowPinForProperties, Struct, EGPD_Input, this);

			for (UEdGraphPin* Pin : Pins)
			{
				if (UE::MetaStoryEditor::Private::IsPropertyPin(Pin))
				{
					// Force the property to be linked until we have the enabled/disabled on the default value.
					Pin->bDefaultValueIsIgnored = true;
				}
			}
		}
	}
}

UK2Node::ERedirectType UK2Node_MakeMetaStoryReference::DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex) const
{
	ERedirectType Result = Super::DoPinsMatchForReconstruction(NewPin, NewPinIndex, OldPin, OldPinIndex);

	if (Result == ERedirectType_None
		&& UE::MetaStoryEditor::Private::DoRenamedPinsMatch(MetaStory, NewPin, OldPin))
	{
		Result = ERedirectType_Name;
	}
	return Result;
}

void UK2Node_MakeMetaStoryReference::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	Super::PinDefaultValueChanged(Pin);

	if (Pin == FindPinChecked(UE::MetaStoryEditor::Private::MetaStoryPinName))
	{
		SetMetaStory(GetMetaStoryDefaultValue());
	}
}

namespace UE::MetaStoryEditor::Private
{
	class SMakeMetaStoryReferenceNode : public SGraphNodeK2Default
	{
		TSharedPtr<SGraphPin> CreatePinWidget(UEdGraphPin* Pin) const
		{
			TSharedPtr<SGraphPin> Result = SGraphNodeK2Default::CreatePinWidget(Pin);
			if (Pin->PinName == UE::MetaStoryEditor::Private::MetaStoryPinName)
			{
				Result->GetPinImageWidget()->SetVisibility(EVisibility::Hidden);
			}
			return Result;
		}
	};
}

TSharedPtr<SGraphNode> UK2Node_MakeMetaStoryReference::CreateVisualWidget()
{
	return SNew(UE::MetaStoryEditor::Private::SMakeMetaStoryReferenceNode, this);
}

void UK2Node_MakeMetaStoryReference::PreloadRequiredAssets()
{
	PreloadObject(FMetaStoryReference::StaticStruct());
	if (MetaStory)
	{
		PreloadObject(MetaStory);
	}

	Super::PreloadRequiredAssets();
}

void UK2Node_MakeMetaStoryReference::ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	bool bTestExistingProperties = true;
	if (MetaStory)
	{
		// Tests if the property pins are valid
		const FInstancedPropertyBag& Parameters = MetaStory->GetDefaultParameters();
		if (Parameters.IsValid())
		{
			bTestExistingProperties = false;
			const UScriptStruct* Struct = Parameters.GetValue().GetScriptStruct();
			for (UEdGraphPin* Pin : Pins)
			{
				if (UE::MetaStoryEditor::Private::IsPropertyPin(Pin)
					&& UE::MetaStoryEditor::Private::CanUseProperty(Pin))
				{
					const FProperty* Property = Struct->FindPropertyByName(Pin->PinName);
					const FPropertyBagPropertyDesc* PropertyDesc = Parameters.FindPropertyDescByName(Pin->PinName);
					if (Property == nullptr || PropertyDesc == nullptr)
					{
						FFormatNamedArguments Args;
						Args.Add(TEXT("PropertyName"), FText::FromName(Pin->PinName));
						const FText Error = FText::Format(LOCTEXT("CanNotFindProeprty_Error", "Can't find the property {PropertyName} in @@"), Args);
						MessageLog.Error(*Error.ToString(), this);
					}
				}
			}
		}

		// Tests if the cached value matches the value of the pin. It should match unless it was set manually by code.
		{
			UEdGraphPin* ThisMetaStoryPin = FindPinChecked(UE::MetaStoryEditor::Private::MetaStoryPinName);
			if (MetaStory != ThisMetaStoryPin->DefaultObject)
			{
				MessageLog.Error(*LOCTEXT("MetaStoryMatchingError", "The MetaStory asset does not match the pin @@. Clear and set the MetaStory pin.").ToString(), this);
			}
		}
	}
	
	// Tests if we expect a MetaStory (it is valid to construct an empty struct)
	if (bTestExistingProperties)
	{
		const bool bHasProperty = Pins.ContainsByPredicate([](const UEdGraphPin* Pin)
			{
				return UE::MetaStoryEditor::Private::IsPropertyPin(Pin)
					&& UE::MetaStoryEditor::Private::CanUseProperty(Pin);
			});
		if (bHasProperty)
		{
			MessageLog.Error(*LOCTEXT("NoMetaStory_Error", "No MetaStory in @@").ToString(), this);
		}
	}
}

void UK2Node_MakeMetaStoryReference::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* Action = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(Action))
	{
		UBlueprintNodeSpawner* MakeNodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(MakeNodeSpawner != nullptr);
		ActionRegistrar.AddBlueprintAction(Action, MakeNodeSpawner);
	}
}

void UK2Node_MakeMetaStoryReference::SetMetaStory(UMetaStory* InMetaStory)
{
	MetaStory = InMetaStory;
	FBlueprintEditorUtils::MarkBlueprintAsModified(GetBlueprint());
	ReconstructNode();
}

UMetaStory* UK2Node_MakeMetaStoryReference::GetMetaStoryDefaultValue() const
{
	return Cast<UMetaStory>(FindPinChecked(UE::MetaStoryEditor::Private::MetaStoryPinName)->DefaultObject);
}

void UK2Node_MakeMetaStoryReference::HandleMetaStoryCompiled(const UMetaStory& InMetaStory)
{
	if (&InMetaStory == MetaStory)
	{
		SetMetaStory(MetaStory);
	}
}

void UK2Node_MakeMetaStoryReference::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	if (CompilerContext.bIsFullCompile)
	{
		const UEdGraphSchema_K2* K2Schema = CompilerContext.GetSchema();

		// Convert to
		//local = MakeMetaStoryReference(MetaStory)
		//for each properties
		//  K2_SetParametersProperty(local, id, value)
		UEdGraphPin* LastThen = nullptr;
		UEdGraphPin* MakeMetaStoryReferenceNodeResultPin = nullptr;
		{
			UK2Node_CallFunction* MakeMetaStoryReferenceNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
			UFunction* Function = UMetaStoryFunctionLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UMetaStoryFunctionLibrary, MakeMetaStoryReference));
			MakeMetaStoryReferenceNode->SetFromFunction(Function);
			MakeMetaStoryReferenceNode->AllocateDefaultPins();
			CompilerContext.MessageLog.NotifyIntermediateObjectCreation(MakeMetaStoryReferenceNode, SourceGraph);
			{
				UEdGraphPin* ThisMetaStoryPin = FindPinChecked(UE::MetaStoryEditor::Private::MetaStoryPinName);
				UEdGraphPin* NewMetaStoryPin = MakeMetaStoryReferenceNode->FindPinChecked(FName("MetaStory"));
				CompilerContext.MovePinLinksToIntermediate(*ThisMetaStoryPin, *NewMetaStoryPin);
			}
			{
				UEdGraphPin* ThisResultPin = FindPinChecked(UEdGraphSchema_K2::PN_ReturnValue);
				MakeMetaStoryReferenceNodeResultPin = MakeMetaStoryReferenceNode->FindPinChecked(UEdGraphSchema_K2::PN_ReturnValue);
				CompilerContext.MovePinLinksToIntermediate(*ThisResultPin, *MakeMetaStoryReferenceNodeResultPin);
			}
			{
				UEdGraphPin* ThisExecPin = GetExecPin();
				UEdGraphPin* NewExecPin = MakeMetaStoryReferenceNode->GetExecPin();
				CompilerContext.MovePinLinksToIntermediate(*ThisExecPin, *NewExecPin);
			}
			{
				UEdGraphPin* ThisThenPin = GetThenPin();
				UEdGraphPin* NewThenPin = MakeMetaStoryReferenceNode->GetThenPin();
				CompilerContext.MovePinLinksToIntermediate(*ThisThenPin, *NewThenPin);
				LastThen = NewThenPin;
			}
		}

		if (MetaStory != nullptr)
		{
			//for each pin call K2_SetParametersProperty
			for (UEdGraphPin* Pin : Pins)
			{
				if (UE::MetaStoryEditor::Private::IsPropertyPin(Pin)
					&& UE::MetaStoryEditor::Private::CanUseProperty(Pin))
				{
						UK2Node_CallFunction* SetParametersPropertyNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
						UFunction* Function = UMetaStoryFunctionLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UMetaStoryFunctionLibrary, K2_SetParametersProperty));
						SetParametersPropertyNode->SetFromFunction(Function);
						SetParametersPropertyNode->AllocateDefaultPins();
						CompilerContext.MessageLog.NotifyIntermediateObjectCreation(SetParametersPropertyNode, SourceGraph);
						
						{
							UEdGraphPin* NewValuePin = SetParametersPropertyNode->FindPinChecked(FName("Reference"), EGPD_Input);
							ensure(K2Schema->TryCreateConnection(MakeMetaStoryReferenceNodeResultPin, NewValuePin));
						}
						{
							UEdGraphPin* NewValuePin = SetParametersPropertyNode->FindPinChecked(FName("PropertyID"), EGPD_Input);
							
							const FPropertyBagPropertyDesc* PropertyDesc = MetaStory->GetDefaultParameters().FindPropertyDescByName(Pin->PinName);
							check(PropertyDesc);

							FGuid Default;
							FGuid TempValue = PropertyDesc->ID;
							TBaseStructure<FGuid>::Get()->ExportText(NewValuePin->DefaultValue, &TempValue, &Default, nullptr, PPF_None, nullptr);
						}
						{
							UEdGraphPin* NewValuePin = SetParametersPropertyNode->FindPinChecked(FName("NewValue"), EGPD_Input);
							NewValuePin->PinType = Pin->PinType;
							CompilerContext.MovePinLinksToIntermediate(*Pin, *NewValuePin);
						}
						// move last Then to new Then and link the last Then to new exec
						{
							UEdGraphPin* NewThenPin = SetParametersPropertyNode->GetThenPin();
							CompilerContext.MovePinLinksToIntermediate(*LastThen, *NewThenPin);
						}
						{
							UEdGraphPin* NewExecPin = SetParametersPropertyNode->GetExecPin();
							ensure(K2Schema->TryCreateConnection(LastThen, NewExecPin));
						}
						LastThen = SetParametersPropertyNode->GetThenPin();
				}
			}
		}
	}
	BreakAllNodeLinks();
}

#undef LOCTEXT_NAMESPACE
