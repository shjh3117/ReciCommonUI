// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Input/Events.h"

/**
 * Focus-return state machine shared by concrete Reci lists: whether focus that left the list is to come back, and how it
 * is coming back. The list decides where it lands, its driven item, so the controller remembers no entry.
 */
class FReciListFocusReturnController final
{
public:
	void SetInitialItemIndex(int32 ItemIndex);
	int32 GetInitialItemIndex() const;

	void NotifyRemovedFromFocusPath();
	bool BeginFocusReturn(
		EFocusCause Cause,
		uint32 UserIndex,
		bool bClearSelection);
	void CompleteFocusReturn();
	void ResetForEntries();

	/** Focus left the list and has not come back yet. */
	bool IsAwaitingFocusReturn() const;
	bool IsApplyingFocusReturn() const;
	bool IsPointerFocusReturn() const;
	bool ShouldClearSelection() const;
	uint32 GetFocusReturnUserIndex() const;

	bool IsRedirectingItemFocus() const;
	void BeginItemFocusRedirect();
	void EndItemFocusRedirect();

private:
	int32 InitialFocusItemIndex = INDEX_NONE;
	uint32 FocusReturnUserIndex = 0;
	bool bAwaitingFocusReturn = false;
	bool bApplyingFocusReturn = false;
	bool bClearSelectionForFocusReturn = false;
	bool bPointerFocusReturn = false;
	bool bRedirectingItemFocus = false;
};
