// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "Widgets/ReciKeyTextSegments.h"
#include "Widgets/ReciMappedKeyText.h"
#include "Widgets/Text/STextBlock.h"

/** A key text's private state, for tests. */
struct FReciMappedKeyTextTestAccess
{
	/** What the key text's owner set with CommonUI's SetHidden. */
	static bool IsHiddenByOwner(const UReciMappedKeyText& KeyText) { return KeyText.bAlwaysHideOverride; }

	/** The texts the key text shows its names and separators in. */
	static TArray<TSharedRef<STextBlock>> GetShownTexts(const UReciMappedKeyText& KeyText) { return KeyText.MyKeyRow.Texts; }

	/** The rules the key text draws its shown keys with (bFiltered), or reserves its width with. */
	static UE::ReciCommonUI::Private::FReciKeySegmentRules MakeSegmentRules(
		const UReciMappedKeyText& KeyText,
		const ECommonInputType InputType,
		const FName GamepadName,
		const bool bFiltered)
	{
		return KeyText.MakeSegmentRules(InputType, GamepadName, bFiltered);
	}
};

#endif
