// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Widgets/ReciListSelectionController.h"

#include "CommonButtonBase.h"
#include "Widgets/ReciListEntryRuntime.h"

void FReciListSelectionController::ClearSelection(
	const TArray<TObjectPtr<UReciListEntryRuntime>>& Entries)
{
	TGuardValue<bool> SelectionGuard(bUpdatingSelection, true);
	for (const UReciListEntryRuntime* Entry : Entries)
	{
		UCommonButtonBase* Button = Entry
			? Entry->GetSelectionButton()
			: nullptr;
		if (Button && Button->GetSelected())
		{
			Button->ClearSelection();
		}
	}
}

int32 FReciListSelectionController::GetSelectedItemCount(
	const TArray<TObjectPtr<UReciListEntryRuntime>>& Entries) const
{
	int32 SelectedItemCount = 0;
	for (const UReciListEntryRuntime* Entry : Entries)
	{
		const UCommonButtonBase* Button = Entry
			? Entry->GetSelectionButton()
			: nullptr;
		SelectedItemCount += Button && Button->GetSelected() ? 1 : 0;
	}
	return SelectedItemCount;
}

int32 FReciListSelectionController::GetFirstSelectedItemIndex(
	const TArray<TObjectPtr<UReciListEntryRuntime>>& Entries) const
{
	for (int32 ItemIndex = 0; ItemIndex < Entries.Num(); ++ItemIndex)
	{
		const UReciListEntryRuntime* Entry = Entries[ItemIndex];
		const UCommonButtonBase* Button = Entry
			? Entry->GetSelectionButton()
			: nullptr;
		if (Button && Button->GetSelected())
		{
			return ItemIndex;
		}
	}
	return INDEX_NONE;
}

void FReciListSelectionController::GetSelectedItemIndices(
	const TArray<TObjectPtr<UReciListEntryRuntime>>& Entries,
	TArray<int32>& OutIndices) const
{
	OutIndices.Reset();
	for (int32 ItemIndex = 0; ItemIndex < Entries.Num(); ++ItemIndex)
	{
		const UReciListEntryRuntime* Entry = Entries[ItemIndex];
		const UCommonButtonBase* Button = Entry
			? Entry->GetSelectionButton()
			: nullptr;
		if (Button && Button->GetSelected())
		{
			OutIndices.Add(ItemIndex);
		}
	}
}

void FReciListSelectionController::ApplyModeToButton(
	UCommonButtonBase* Button,
	const ESelectionMode::Type SelectionMode) const
{
	if (!Button)
	{
		return;
	}

	Button->SetIsToggleable(
		SelectionMode == ESelectionMode::SingleToggle
		|| SelectionMode == ESelectionMode::Multi);
	Button->SetIsSelectable(SelectionMode != ESelectionMode::None);
	// Reci's explicit navigation policy owns automatic selection, not a child button flag.
	Button->SetShouldSelectUponReceivingFocus(false);
	Button->SetIsInteractableWhenSelected(
		SelectionMode != ESelectionMode::None);
}

void FReciListSelectionController::ApplyModeToEntries(
	const TArray<TObjectPtr<UReciListEntryRuntime>>& Entries,
	const ESelectionMode::Type SelectionMode) const
{
	for (const UReciListEntryRuntime* Entry : Entries)
	{
		ApplyModeToButton(
			Entry ? Entry->GetSelectionButton() : nullptr,
			SelectionMode);
	}
}

bool FReciListSelectionController::NormalizeSelectionChange(
	const bool bIsSelected,
	UCommonButtonBase& ChangedButton,
	const ESelectionMode::Type SelectionMode,
	const TArray<TObjectPtr<UReciListEntryRuntime>>& Entries)
{
	if (bUpdatingSelection || !bIsSelected)
	{
		return true;
	}

	if (SelectionMode == ESelectionMode::None)
	{
		TGuardValue<bool> SelectionGuard(bUpdatingSelection, true);
		ChangedButton.ClearSelection();
		return false;
	}

	if (SelectionMode != ESelectionMode::Multi)
	{
		TGuardValue<bool> SelectionGuard(bUpdatingSelection, true);
		for (const UReciListEntryRuntime* Entry : Entries)
		{
			UCommonButtonBase* Button = Entry
				? Entry->GetSelectionButton()
				: nullptr;
			if (Button
				&& Button != &ChangedButton
				&& Button->GetSelected())
			{
				Button->ClearSelection();
			}
		}
	}
	return true;
}
