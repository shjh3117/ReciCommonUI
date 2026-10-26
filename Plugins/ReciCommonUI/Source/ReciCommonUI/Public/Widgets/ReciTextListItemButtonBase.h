// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ReciFocusStyleButtonBase.h"
#include "Widgets/ReciTextListItem.h"

#include "ReciTextListItemButtonBase.generated.h"

class UTextBlock;

/**
 * Clickable Reci Text List Item with automatic text binding.
 *
 * A Widget Blueprint derived from this class must contain a Text Block or
 * Common Text Block named ItemTextBlock. Reci Vertical List supplies
 * its text without requiring a Blueprint event graph.
 */
UCLASS(
	Abstract,
	BlueprintType,
	Blueprintable,
	ClassGroup = UI,
	meta = (
		DisplayName = "Reci Text List Item Button Base",
		Category = "Reci Common UI",
		DisableNativeTick))
class RECICOMMONUI_API UReciTextListItemButtonBase
	: public UReciFocusStyleButtonBase
	, public IReciTextListItem
{
	GENERATED_BODY()

public:
	/** Text currently represented by this list item. */
	UPROPERTY(BlueprintReadOnly, Category = "List Item|Content")
	FText ItemText;

	virtual void SetListItemText_Implementation(
		const FText& InItemText) override;

protected:
	virtual void SynchronizeProperties() override;

	/**
	 * Required automatic text binding.
	 *
	 * Add a Text Block or Common Text Block named ItemTextBlock to update it
	 * automatically whenever the list supplies new text.
	 */
	UPROPERTY(
		BlueprintReadOnly,
		Category = "List Item|Bindings",
		meta = (BindWidget))
	TObjectPtr<UTextBlock> ItemTextBlock;

private:
	void RefreshItemText();
};
