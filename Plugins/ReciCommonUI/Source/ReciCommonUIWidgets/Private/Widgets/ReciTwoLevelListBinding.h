// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.
#pragma once
#include "UObject/Object.h"
#include "Widgets/ReciListController.h"
#include "ReciTwoLevelListBinding.generated.h"

class UReciTwoLevelListBase;
class UReciListBase;
class UReciListLayoutBoxBase;
class UReciScrollBox;
class UWidgetTree;
#if WITH_EDITOR
class IWidgetCompilerLog;
#endif

namespace UE::ReciCommonUIWidgets::Private
{
	/** A property of a bound list that the two-level list overwrites or ignores, as the author set it. */
	struct FReciDroppedListSetting
	{
		/** The property's display name. */
		FString Property;
		FString Message;
	};

	/**
	 * The properties of a list a two-level list binds that the binding overwrites with another value or ignores, among
	 * those the author changed from the list class's default (principle 4: a default list reports nothing). Overwritten:
	 * Selection Mode (None for ParentList, Single for a ChildList), Clear Selection On Focus Return (off), Allow Pointer
	 * Hover (the owner's, compared with the default alone when the owner is unknown), and on a Reci Advanced List Main
	 * Presentation Follows Focus and Confirm Main Item On Click (off). Ignored: Confirm On Item Click, Selection Follows
	 * Navigation and Looping Entries.
	 */
	TArray<FReciDroppedListSetting> CollectDroppedListSettings(
		const UReciListBase& List,
		bool bParentList,
		TOptional<bool> OwnerAllowsHover);

#if WITH_EDITOR
	/**
	 * Compile-time placement check of a bound list (ParentList or ChildList): it must be the content of a Reci Vertical
	 * Box or Reci Horizontal Box, and the nearest Reci Scroll Box around that box must hold the box as its only child.
	 * Reports each error and returns the box's axis when the placement is valid.
	 */
	TOptional<EOrientation> ValidateTwoLevelPlacement(const UReciListBase& List, IWidgetCompilerLog& CompileLog);

	/** Compile-time check that a bound list's entry classes keep the two-level entry contract. */
	void ValidateTwoLevelEntryClasses(const UReciListBase& List, bool bParentList, IWidgetCompilerLog& CompileLog);

	/** Compile-time check, in the two-level list's Blueprint, that a row class's ChildList runs across ParentList's axis. */
	void ValidateTwoLevelRowTree(
		const UReciListBase& ParentList,
		EOrientation ParentAxis,
		const UClass& RowClass,
		const UWidgetTree& RowTree,
		IWidgetCompilerLog& CompileLog);

	/** Reports each dropped setting of a bound list template as a compile warning. */
	void ValidateDroppedListSettings(
		const UReciListBase& List,
		bool bParentList,
		TOptional<bool> OwnerAllowsHover,
		IWidgetCompilerLog& CompileLog);
#endif
}

/** Runtime behavior attached to a WBP-authored list, not a widget or a data wrapper. */
UCLASS(Transient)
class UReciTwoLevelListBinding : public UObject, public IReciListController
{
	GENERATED_BODY()
public:
	bool Initialize(UReciTwoLevelListBase* InOwner, UReciListBase* InList, int32 InParentIndex);
	void Detach();
	virtual bool SupportsEntryClass(const UClass* Class) const override;
	virtual void InitializeEntry(UUserWidget* Entry, UObject* Item, int32 Index) override;
	virtual bool IsSelectionOnly() const override { return ParentIndex != INDEX_NONE; }
	virtual void ClickItem(int32 Index) override;
	virtual void DrivenItemChanged(int32 Index, bool bGiveFocus) override;
	virtual bool ConfirmItem() override;
	void UpdateDesignState(int32 FocusedParent, int32 SelectedChild);
	/**
	 * Applies the two-level list's Allow Pointer Hover to the list, reporting once when something changed the list's
	 * value since the last time it was applied.
	 */
	void ApplyPointerHover(bool bAllowPointerHover);
	UPROPERTY() TObjectPtr<UReciListBase> List;
	UPROPERTY() TObjectPtr<UReciListLayoutBoxBase> Layout;
	UPROPERTY() TObjectPtr<UReciScrollBox> Viewport;
	UPROPERTY() TWeakObjectPtr<UReciTwoLevelListBase> Owner;
	int32 ParentIndex = INDEX_NONE;
private:
	void FocusChanged(int32 Index);
	int32 LastFocused = -2;
	int32 LastSelected = -2;
	bool bLayoutInputWasEnabled = true;
	bool bViewportInputWasEnabled = true;
	bool bControllerAttached = false;
	bool bSavedClearSelection = false;
	bool bSavedHover = true;
	bool bSavedFollowFocus = true;
	bool bSavedConfirmClick = true;
	uint8 SavedSelectionMode = 0;
	/** The Allow Pointer Hover last applied while bound. */
	TOptional<bool> AppliedHover;
};
