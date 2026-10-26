// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CommonTextBlock.h"
#include "CoreMinimal.h"

#include "ReciTextBlock.generated.h"

class SBox;

/**
 * Common text block that shows at most Max Lines lines.
 *
 * One line never wraps and ends in an ellipsis when it runs long. More lines wrap, and the last line that fits
 * ends in the ellipsis. The height limit is the font's own line height times the line count (with Line Height
 * Percentage and Margin), so it follows the text style. Zero leaves wrapping and overflow as authored.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "Reci Text Block"))
class RECICOMMONUI_API UReciTextBlock : public UCommonTextBlock
{
	GENERATED_BODY()

public:
	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Text")
	void SetMaxLines(int32 InMaxLines);

	UFUNCTION(BlueprintPure, Category = "Reci Common UI|Text")
	int32 GetMaxLines() const { return MaxLines; }

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** Most lines shown; past them the last shown line ends in an ellipsis. Zero keeps the authored wrapping. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetMaxLines, BlueprintGetter = GetMaxLines, Category = "Reci Common UI|Text", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxLines = 0;

private:
	void ApplyLineLimit();
	FOptionalSize GetLineLimitHeight() const;

	TSharedPtr<SBox> LineLimitBox;
};
