// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "ReciListItem.generated.h"

/**
 * Receives an arbitrary data object from a Reci list.
 *
 * Each item Widget Blueprint interprets the object as its own data type.
 */
UINTERFACE(BlueprintType, Blueprintable)
class RECICOMMONUI_API UReciListItem : public UInterface
{
	GENERATED_BODY()
};

class RECICOMMONUI_API IReciListItem
{
	GENERATED_BODY()

public:
	/** Supplies the data object represented by this item widget. */
	UFUNCTION(
		BlueprintNativeEvent,
		BlueprintCallable,
		Category = "Reci Common UI|List",
		meta = (DisplayName = "Set List Item Object"))
	void SetListItemObject(UObject* ItemObject);

	virtual void SetListItemObject_Implementation(UObject* ItemObject);
};
