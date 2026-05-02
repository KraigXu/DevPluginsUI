// Copyright Epic Games, Inc. All Rights Reserved.

#include "K2Node_MetaStoryNodeGetPropertyDescription.h"
#include "Blueprint/MetaStoryNodeBlueprintBase.h"
#include "EdGraphSchema_K2.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "K2Node_CallFunction.h"
#include "KismetCompiler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(K2Node_MetaStoryNodeGetPropertyDescription)

#define LOCTEXT_NAMESPACE "K2Node_MetaStoryNodeGetPropertyDescription"


void UK2Node_MetaStoryNodeGetPropertyDescription::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Text, UEdGraphSchema_K2::PN_ReturnValue);
	
	Super::AllocateDefaultPins();
}

FText UK2Node_MetaStoryNodeGetPropertyDescription::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	UBlueprint* Blueprint = HasValidBlueprint() ? GetBlueprint() : nullptr;
	const FProperty* Property = const_cast<FMemberReference&>(Variable).ResolveMember<FProperty>(Blueprint);
	const FText SelectedPropertyName = Property ? Property->GetDisplayNameText() : LOCTEXT("None", "<None>");

	return FText::Format(LOCTEXT("NodeTitle", "Get Description for {0}"), SelectedPropertyName);
}

FText UK2Node_MetaStoryNodeGetPropertyDescription::GetTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Returns text describing the specified member variable.");
}

FText UK2Node_MetaStoryNodeGetPropertyDescription::GetMenuCategory() const
{
	return LOCTEXT("NodeCategory", "MetaStory");
}

void UK2Node_MetaStoryNodeGetPropertyDescription::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	Super::GetMenuActions(ActionRegistrar);
	UClass* Action = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(Action))
    {
        UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create(Action);
        ActionRegistrar.AddBlueprintAction(Action, Spawner);
    }
}

void UK2Node_MetaStoryNodeGetPropertyDescription::ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	const UClass* BlueprintClass = GetBlueprintClassFromNode();
	if (!BlueprintClass->IsChildOf<UMetaStoryNodeBlueprintBase>())
	{
		const FText ErrorText = LOCTEXT("InvalidSelfType", "This blueprint (self) is not a 'MetaStory Blueprint Node'.");
		MessageLog.Error(*ErrorText.ToString(), this);
	}

	UBlueprint* Blueprint = HasValidBlueprint() ? GetBlueprint() : nullptr;
	const FProperty* Property = const_cast<FMemberReference&>(Variable).ResolveMember<FProperty>(Blueprint);
	if (!Property)
	{
		const FText ErrorText = FText::Format(LOCTEXT("InvalidProperty", "Cannot find property '{0}'."), FText::FromName(Variable.GetMemberName()));
		MessageLog.Error(*ErrorText.ToString(), this);
	}
}

void UK2Node_MetaStoryNodeGetPropertyDescription::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	UBlueprint* Blueprint = HasValidBlueprint() ? GetBlueprint() : nullptr;

	// Property name
	const FProperty* Property = Variable.ResolveMember<FProperty>(Blueprint);
	FString SelectedPropertyName = Property ? Property->GetName() : TEXT("");
	
	UK2Node_CallFunction* CallGetPropertyDescription = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallGetPropertyDescription->SetFromFunction(UMetaStoryNodeBlueprintBase::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UMetaStoryNodeBlueprintBase, GetPropertyDescriptionByPropertyName)));
	CallGetPropertyDescription->AllocateDefaultPins();

	UEdGraphPin* PropertyNamePin = CallGetPropertyDescription->FindPinChecked(TEXT("PropertyName"));
	check(PropertyNamePin);
	PropertyNamePin->DefaultValue = SelectedPropertyName;

	UEdGraphPin* OrgReturnPin = FindPinChecked(UEdGraphSchema_K2::PN_ReturnValue);
	UEdGraphPin* NewReturnPin = CallGetPropertyDescription->GetReturnValuePin();
	check(NewReturnPin);
	CompilerContext.MovePinLinksToIntermediate(*OrgReturnPin, *NewReturnPin);
}

#undef LOCTEXT_NAMESPACE
