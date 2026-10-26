// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ReciListItem.h"

#include "ReciTextListItem.generated.h"

/** Internal object adapter used to retain FText values in an object-backed list. */
UCLASS(BlueprintType)
class RECICOMMONUI_API UReciTextListItemData : public UObject
{
	GENERATED_BODY()

public:
	/** Text represented by this list item. */
	UPROPERTY(BlueprintReadOnly, Category = "Reci Common UI|List")
	FText ItemText;
};

/**
 * Text-specialized Reci List Item contract.
 *
 * A Widget Blueprint implementing this interface receives Set List Item Text
 * when populated through Set Text List Items.
 */
UINTERFACE(BlueprintType, Blueprintable)
class RECICOMMONUI_API UReciTextListItem : public UReciListItem
{
	GENERATED_BODY()
};

class RECICOMMONUI_API IReciTextListItem : public IReciListItem
{
	GENERATED_BODY()

public:
	/** Supplies the text represented by this item widget. */
	UFUNCTION(
		BlueprintNativeEvent,
		BlueprintCallable,
		Category = "Reci Common UI|List",
		meta = (DisplayName = "Set List Item Text"))
	void SetListItemText(const FText& ItemText);

	virtual void SetListItemText_Implementation(const FText& ItemText);
};
