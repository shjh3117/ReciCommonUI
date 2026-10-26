// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Widgets/ReciTextBlock.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ReciTextBlock)

#define LOCTEXT_NAMESPACE "ReciTextBlock"

TSharedRef<SWidget> UReciTextBlock::RebuildWidget()
{
	return SAssignNew(LineLimitBox, SBox)
		.MaxDesiredHeight(BIND_UOBJECT_ATTRIBUTE(FOptionalSize, GetLineLimitHeight))
		[
			Super::RebuildWidget()
		];
}

void UReciTextBlock::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	LineLimitBox.Reset();
}

void UReciTextBlock::SynchronizeProperties()
{
	ApplyLineLimit();
	Super::SynchronizeProperties();
	if (MyTextBlock.IsValid() && MaxLines > 0)
	{
		// The ellipsis lands on the last line inside the clip, so the text clips to the height it is given.
		MyTextBlock->SetClipping(EWidgetClipping::ClipToBounds);
	}
}

void UReciTextBlock::SetMaxLines(const int32 InMaxLines)
{
	const int32 NewMaxLines = FMath::Max(0, InMaxLines);
	if (MaxLines == NewMaxLines)
	{
		return;
	}
	MaxLines = NewMaxLines;
	ApplyLineLimit();
	if (MyTextBlock.IsValid())
	{
		MyTextBlock->SetClipping(MaxLines > 0 ? EWidgetClipping::ClipToBounds : EWidgetClipping::Inherit);
	}
	if (LineLimitBox.IsValid())
	{
		LineLimitBox->Invalidate(EInvalidateWidgetReason::Layout);
	}
}

void UReciTextBlock::ApplyLineLimit()
{
	if (MaxLines <= 0)
	{
		return;
	}
	const bool bWrap = MaxLines > 1;
	const ETextOverflowPolicy Overflow = bWrap ? ETextOverflowPolicy::MultilineEllipsis : ETextOverflowPolicy::Ellipsis;
	if (GetAutoWrapText() != bWrap)
	{
		SetAutoWrapText(bWrap);
	}
	// SetTextOverflowPolicy synchronizes all properties again, which comes back here: set it only on a change.
	if (GetTextOverflowPolicy() != Overflow)
	{
		SetTextOverflowPolicy(Overflow);
	}
}

FOptionalSize UReciTextBlock::GetLineLimitHeight() const
{
	if (MaxLines <= 1 || !FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
	{
		return FOptionalSize();
	}

	// A composite font's lines take the default typeface's height, fallback glyphs included, as the text layout does.
	const FSlateFontInfo LineFont = MyTextBlock.IsValid() ? MyTextBlock->GetFont() : GetFont();
	const float LineHeight = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->GetMaxCharacterHeight(LineFont, 1.0f);
	const float SpacedLine = LineHeight * LineHeightPercentage;
	const float BottomLine = ApplyLineHeightToBottomLine ? SpacedLine : LineHeight;
	// One unit of slack absorbs rounding at the paint scale; a partly visible extra line is never drawn.
	return SpacedLine * static_cast<float>(MaxLines - 1) + BottomLine + Margin.GetTotalSpaceAlong<Orient_Vertical>() + 1.0f;
}

#if WITH_EDITOR
const FText UReciTextBlock::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "Reci Common UI");
}
#endif

#undef LOCTEXT_NAMESPACE
