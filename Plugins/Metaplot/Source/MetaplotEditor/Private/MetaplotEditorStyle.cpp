#include "MetaplotEditorStyle.h"

#include "Styling/AppStyle.h"
#include "Styling/SlateStyleRegistry.h"

FMetaplotEditorStyle::FMetaplotEditorStyle()
	: FSlateStyleSet(TEXT("MetaplotEditorStyle"))
{
	SetParentStyleName(FAppStyle::GetAppStyleSetName());

	const FTextBlockStyle& NormalText = FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");
	const FTextBlockStyle& BoldText = FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");
	Set("Metaplot.Category", FTextBlockStyle(NormalText)
		.SetFont(FAppStyle::GetFontStyle(TEXT("PropertyWindow.BoldFont")))
		.SetFontSize(8));

	Set("Metaplot.Category.IconColor", FLinearColor(0.25f, 0.75f, 0.75f));

	// Route common details icons through Metaplot style keys.
	Set("MetaplotEditor.Tasks", new FSlateBrush(*FAppStyle::GetBrush("Icons.Details")));
	Set("MetaplotEditor.Tasks.Large", new FSlateBrush(*FAppStyle::GetBrush("Icons.Details")));
	Set("MetaplotEditor.Add", new FSlateBrush(*FAppStyle::GetBrush("Icons.PlusCircle")));
	Set("MetaplotEditor.TasksCompletion.Enabled", FLinearColor(0.20f, 0.72f, 0.35f));
	Set("MetaplotEditor.TasksCompletion.Disabled", FLinearColor(0.55f, 0.55f, 0.55f));

	Set("Metaplot.Task.Title", FTextBlockStyle(NormalText).SetFontSize(10));
	Set("Metaplot.Task.Title.Bold", FTextBlockStyle(BoldText)
		.SetFont(FAppStyle::GetFontStyle(TEXT("PropertyWindow.BoldFont")))
		.SetFontSize(10));
	Set("Metaplot.Task.Title.Subdued", FTextBlockStyle(NormalText)
		.SetColorAndOpacity(FSlateColor(FLinearColor(0.70f, 0.70f, 0.70f)))
		.SetFontSize(10));
}

FMetaplotEditorStyle& FMetaplotEditorStyle::Get()
{
	static FMetaplotEditorStyle Instance;
	return Instance;
}

void FMetaplotEditorStyle::Register()
{
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}

void FMetaplotEditorStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
}
