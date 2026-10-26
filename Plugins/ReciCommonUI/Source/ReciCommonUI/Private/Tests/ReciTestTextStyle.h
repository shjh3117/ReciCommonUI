// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CommonTextBlock.h"
#include "Styling/CoreStyle.h"
#include "ReciTestTextStyle.generated.h"

/** A Common Text Style with its own font, color, drop shadow and margin, which a key-text test sets as a key text's Style. */
UCLASS(Hidden, NotBlueprintable, Transient)
class UReciTestTextStyle : public UCommonTextStyle
{
	GENERATED_BODY()
public:
	UReciTestTextStyle()
	{
		Font = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 30);
		Color = FLinearColor(0.25f, 0.5f, 0.75f);
		bUsesDropShadow = true;
		ShadowOffset = FVector2D(2.0, 3.0);
		ShadowColor = FLinearColor(0.1f, 0.2f, 0.3f);
		Margin = FMargin(0.0f, 12.0f);
	}
};
