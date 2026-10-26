// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CommonInputTypeEnum.h"
#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Styling/SlateBrush.h"

namespace UE::ReciCommonUI::Private
{
	/** One piece of a key text's row: a name or a separator, drawn as text, or a glyph, drawn as an image. */
	struct FReciKeySegment
	{
		/** The name or separator. For a glyph, the names of the keys it draws, for the text view (Get Mapped Key Text). */
		FText Text;
		/** CommonUI's brush for the key, or for the group of keys: set for a glyph. */
		TOptional<FSlateBrush> Glyph;
	};

	/** How a key text draws its keys: the instance's settings, and the device whose glyphs it looks up. */
	struct FReciKeySegmentRules
	{
		ECommonInputType InputType = ECommonInputType::MouseAndKeyboard;
		/** The gamepad type CommonUI's controller data is looked up for. */
		FName GamepadName;
		/** The instance's own names, which beat a glyph. */
		const TMap<FName, FText>* KeyNameOverrides = nullptr;
		bool bUseLongDisplayNames = false;
		bool bUppercaseKeyNames = true;
		int32 MaximumDisplayedKeys = 0;
		FText KeySeparator;
		FText ActionSeparator;
		/** The keys shown: a key for which it returns false is left out before anything else. Null keeps every key. */
		const TFunction<bool(const FKey&)>* KeyFilter = nullptr;
	};

	/**
	 * The row for a key text's actions, one key group per action, in order. Each key is drawn as the instance's own name,
	 * else a glyph from CommonUI's controller data for the input type and gamepad, else the built-in name, else FKey's
	 * display name. Keys not valid for the input type or left out by the key filter are left out first, then a key an
	 * earlier group shows or a name already shown (glyphs are not compared by name), and Maximum Displayed Keys counts a
	 * group's keys after that. A group of two
	 * or more keys without a name of the instance's own is one glyph where the controller data has one for that set.
	 */
	TArray<FReciKeySegment> BuildKeySegments(TArray<TArray<FKey>> KeyGroups, const FReciKeySegmentRules& Rules);

	/** The row as text: each segment's text in order, a glyph's as the names of its keys. */
	FText JoinKeySegmentText(TConstArrayView<FReciKeySegment> Segments);
}
