// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Views/ITypedTableView.h"

class UCommonButtonBase;
class UReciListEntryRuntime;

/** CommonButton selection policy for a non-virtualized Reci list. */
class FReciListSelectionController final
{
public:
	void ClearSelection(
		const TArray<TObjectPtr<UReciListEntryRuntime>>& Entries);
	int32 GetSelectedItemCount(
		const TArray<TObjectPtr<UReciListEntryRuntime>>& Entries) const;
	int32 GetFirstSelectedItemIndex(
		const TArray<TObjectPtr<UReciListEntryRuntime>>& Entries) const;
	void GetSelectedItemIndices(
		const TArray<TObjectPtr<UReciListEntryRuntime>>& Entries,
		TArray<int32>& OutIndices) const;

	void ApplyModeToButton(
		UCommonButtonBase* Button,
		ESelectionMode::Type SelectionMode) const;
	void ApplyModeToEntries(
		const TArray<TObjectPtr<UReciListEntryRuntime>>& Entries,
		ESelectionMode::Type SelectionMode) const;

	/** Returns false when the change was rejected by the current mode. */
	bool NormalizeSelectionChange(
		bool bIsSelected,
		UCommonButtonBase& ChangedButton,
		ESelectionMode::Type SelectionMode,
		const TArray<TObjectPtr<UReciListEntryRuntime>>& Entries);

private:
	bool bUpdatingSelection = false;
};
