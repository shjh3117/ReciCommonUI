// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Widgets/ReciListFocusReturnController.h"

void FReciListFocusReturnController::SetInitialItemIndex(
	const int32 ItemIndex)
{
	InitialFocusItemIndex = ItemIndex;
}

int32 FReciListFocusReturnController::GetInitialItemIndex() const
{
	return InitialFocusItemIndex;
}

void FReciListFocusReturnController::NotifyRemovedFromFocusPath()
{
	bAwaitingFocusReturn = true;
	bApplyingFocusReturn = false;
	bClearSelectionForFocusReturn = false;
	bPointerFocusReturn = false;
	bRedirectingItemFocus = false;
}

bool FReciListFocusReturnController::BeginFocusReturn(
	const EFocusCause Cause,
	const uint32 UserIndex,
	const bool bClearSelection)
{
	if (!bAwaitingFocusReturn)
	{
		return false;
	}

	bAwaitingFocusReturn = false;
	bApplyingFocusReturn = true;
	bClearSelectionForFocusReturn = bClearSelection;
	bPointerFocusReturn = Cause == EFocusCause::Mouse;
	FocusReturnUserIndex = UserIndex;
	return true;
}

void FReciListFocusReturnController::CompleteFocusReturn()
{
	bApplyingFocusReturn = false;
	bClearSelectionForFocusReturn = false;
	bPointerFocusReturn = false;
	bRedirectingItemFocus = false;
}

void FReciListFocusReturnController::ResetForEntries()
{
	// Focus that left the list still returns after a rebuild, so bAwaitingFocusReturn stays. It returns to the driven
	// item, as without a rebuild, which the list (or its controller) restores after the rebuild.
	InitialFocusItemIndex = INDEX_NONE;
	FocusReturnUserIndex = 0;
	bApplyingFocusReturn = false;
	bClearSelectionForFocusReturn = false;
	bPointerFocusReturn = false;
	bRedirectingItemFocus = false;
}

bool FReciListFocusReturnController::IsAwaitingFocusReturn() const
{
	return bAwaitingFocusReturn;
}

bool FReciListFocusReturnController::IsApplyingFocusReturn() const
{
	return bApplyingFocusReturn;
}

bool FReciListFocusReturnController::IsPointerFocusReturn() const
{
	return bPointerFocusReturn;
}

bool FReciListFocusReturnController::ShouldClearSelection() const
{
	return bClearSelectionForFocusReturn;
}

uint32 FReciListFocusReturnController::GetFocusReturnUserIndex() const
{
	return FocusReturnUserIndex;
}

bool FReciListFocusReturnController::IsRedirectingItemFocus() const
{
	return bRedirectingItemFocus;
}

void FReciListFocusReturnController::BeginItemFocusRedirect()
{
	bRedirectingItemFocus = true;
}

void FReciListFocusReturnController::EndItemFocusRedirect()
{
	bRedirectingItemFocus = false;
}
