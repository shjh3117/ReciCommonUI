// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "ReciListEntryEmphasis.generated.h"

UINTERFACE(BlueprintType, Blueprintable)
class RECICOMMONUI_API UReciListEntryEmphasis : public UInterface
{
	GENERATED_BODY()
};

/**
 * Lets one entry widget show its item's emphasis itself, with no second presentation and no advanced list.
 *
 * A viewport that drives item emphasis (a carousel, by how near each item is to its centre) sets each item's emphasis
 * on the list, from 0 to 1, whenever it changes, and the list passes it to every widget presenting the item that
 * implements this interface, each copy of a looping item included. Outside such a viewport the entry receives none. What
 * emphasis looks like (scale, opacity, an animation's current time) belongs to the entry's Widget Blueprint.
 */
class RECICOMMONUI_API IReciListEntryEmphasis
{
	GENERATED_BODY()

public:
	/** The item's emphasis changed: 0 is none, 1 is full (a carousel's centred item). */
	UFUNCTION(
		BlueprintNativeEvent,
		Category = "Reci Common UI|List",
		meta = (DisplayName = "On Item Emphasis Changed"))
	void OnItemEmphasisChanged(float Emphasis);

	virtual void OnItemEmphasisChanged_Implementation(float Emphasis) {}
};
