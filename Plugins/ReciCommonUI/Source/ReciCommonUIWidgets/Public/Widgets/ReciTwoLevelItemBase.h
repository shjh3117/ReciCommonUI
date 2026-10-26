// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.
#pragma once

#include "Widgets/ReciFocusStyleButtonBase.h"
#include "Widgets/ReciListItem.h"
#include "Widgets/ReciTwoLevelEntry.h"
#include "ReciTwoLevelItemBase.generated.h"

class UReciListBase;

/** Authored parent row. The Blueprint owns all layout around ChildList. */
UCLASS(Abstract, Blueprintable)
class RECICOMMONUIWIDGETS_API UReciTwoLevelItemBase : public UReciFocusStyleButtonBase,
	public IReciListItem, public IReciTwoLevelEntry
{
	GENERATED_BODY()
public:
	/**
	 * Required name in this row's WBP, directly inside a ReciVerticalBox or ReciHorizontalBox that runs across the
	 * parent list. No native widget is substituted. While bound, the two-level list sets its Selection Mode to Single,
	 * Clear Selection On Focus Return off, Allow Pointer Hover to the two-level list's Allow Pointer Hover and, on a
	 * Reci Advanced List, Main Presentation Follows Focus and Confirm Main Item On Click off, and restores them when it
	 * unbinds; it ignores Confirm On Item Click, Selection Follows Navigation and Looping Entries. Values the author
	 * changed from the default that are overwritten or ignored are reported.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Two Level List", meta = (BindWidget))
	TObjectPtr<UReciListBase> ChildList;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Two Level List")
	TObjectPtr<UObject> ParentItem;

	virtual void SetListItemObject_Implementation(UObject* ItemObject) override { ParentItem = ItemObject; }

#if WITH_EDITOR
	virtual void ValidateCompiledWidgetTree(const UWidgetTree& BlueprintWidgetTree, IWidgetCompilerLog& CompileLog) const override;
#endif
};
