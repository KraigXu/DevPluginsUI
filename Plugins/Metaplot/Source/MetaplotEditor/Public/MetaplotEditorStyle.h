#pragma once

#include "Styling/SlateStyle.h"

class FMetaplotEditorStyle : public FSlateStyleSet
{
public:
	static FMetaplotEditorStyle& Get();

	static void Register();
	static void Unregister();

private:
	FMetaplotEditorStyle();
};
