// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.
#pragma once

#include "CoreMinimal.h"

class UUserWidget;
class UClass;

/**
 * The list-controller seam, for a widget that composes Reci lists (a two-level list attaches one to each of its
 * lists). While a controller is attached (UReciListBase::SetListController), the list keeps its entries,
 * layout, driven item and focus, and hands the controller the decisions a composite makes for all its lists: which entry
 * classes it serves, how an entry gets its data, where focus that was in the list goes after Set List Items, and what a
 * click, a driven-item change, a confirm and an item's emphasis do. The list then applies no selection of its own on
 * navigation. A controller never owns layout.
 */
class IReciListController
{
public:
	virtual ~IReciListController() = default;
	/** Whether the list may create entries of Class, in place of the Reci List Item interface check. */
	virtual bool SupportsEntryClass(const UClass* Class) const = 0;
	/** Gives a new entry its data, in place of Set List Item Object. */
	virtual void InitializeEntry(UUserWidget* Entry, UObject* Item, int32 Index) = 0;
	/** Whether the list moves its driven item without focusing its entries (their buttons may be unfocusable). */
	virtual bool IsSelectionOnly() const = 0;
	/** A click on the item, after the list applied its selection mode; replaces the list's own click handling. */
	virtual void ClickItem(int32 Index) = 0;
	/** The list's driven item was set (Set Driven Item Index). */
	virtual void DrivenItemChanged(int32 Index, bool bGiveFocus) = 0;
	/** Confirms in place of the list (Confirm Driven Item, Accept); returns whether something was confirmed. */
	virtual bool ConfirmItem() = 0;
};
