#include "MetaplotEditorStyle.h"

#include "Styling/AppStyle.h"
#include "Styling/SlateStyleRegistry.h"

FMetaplotEditorStyle::FMetaplotEditorStyle()
	: FSlateStyleSet(TEXT("MetaplotEditorStyle"))
{
	SetParentStyleName(FAppStyle::GetAppStyleSetName());

	const FTextBlockStyle& NormalText = FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");
	Set("Metaplot.Category", FTextBlockStyle(NormalText)
		.SetFont(FAppStyle::GetFontStyle(TEXT("PropertyWindow.BoldFont")))
		.SetFontSize(8));

	Set("Metaplot.Category.IconColor", FLinearColor(0.25f, 0.75f, 0.75f));

	// Route common details icons through Metaplot style keys.
	Set("MetaplotEditor.Tasks", new FSlateBrush(*FAppStyle::GetBrush("Icons.Details")));
	Set("MetaplotEditor.Add", new FSlateBrush(*FAppStyle::GetBrush("Icons.PlusCircle")));
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
