// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ReciListBase.h"

#include "ReciList.generated.h"

class UPanelWidget;
class UReciListPanel;

/**
 * Layout-neutral list source backed by a Reci List Panel.
 *
 * A native instance, such as a Reci List placed in a screen, creates its own ItemsBox root. A Widget Blueprint derived
 * from Reci List must contain a Reci List Panel named ItemsBox (BindWidget).
 */
UCLASS(
	BlueprintType,
	Blueprintable,
	ClassGroup = UI,
	meta = (
		DisplayName = "Reci List",
		Category = "Reci Common UI"))
class RECICOMMONUI_API UReciList : public UReciListBase
{
	GENERATED_BODY()

public:
	/**
	 * Repeats presentations while preserving one source object and index per item, for a carousel's row with no end. It
	 * takes effect when the items are set while the list's Reci Vertical or Horizontal Box is the content of a scroll box
	 * that loops its entries (UReciScrollBox::LoopsHostedEntries, such as a carousel), which moves the copies. Anywhere
	 * else (in a Reci Tile Box, or in a plain Reci Scroll Box) the list shows each item once, which is reported. Each copy
	 * keeps its entry class's own Focus On Hover.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="List Entries")
	bool bLoopingEntries = false;

#if WITH_EDITOR
	/**
	 * Also warns about Looping Entries in a Reci Tile Box or outside a scroll box that loops its entries, and about Layout Settings
	 * set on the ItemsBox of a list a Reci layout box hosts, which the box replaces.
	 */
	virtual void ValidateCompiledDefaults(class IWidgetCompilerLog& CompileLog) const override;
#endif

	virtual UWidget* GetHostedEntryWidget(int32 ItemIndex) const override;
	virtual FGeometry GetHostedEntryGeometry(int32 ItemIndex) const override;
	virtual FGeometry GetEntryPresentationGeometry(UUserWidget* Presentation) const override;
	virtual TArray<UUserWidget*> GetEntryPresentations(int32 Index) const override;
	/**
	 * The loop extension point, for a scroll box whose Loops Hosted Entries is true, which calls these as it scrolls.
	 * With Looping Entries on, the list lays out a whole number of cycles of copies of its items (three at first).
	 * Ensure Loop Coverage adds cycles until they cover ViewportExtent on both sides of the middle cycle and returns
	 * whether it rebuilt them (the viewport then waits for the next arrangement). Refresh Loop Presentations, after an
	 * arrangement, makes the copy of each item nearest Viewport's centre that item's live entry (its button, geometry
	 * and focus), measuring as if the content had already moved by OffsetShift: a viewport that moves its offset by
	 * whole cycles to stay in the middle one passes that move. Get Loop Cycle Extent is the length of one cycle along
	 * Orientation (0 before the copies are laid out), and the anchor is the first item's copy in the middle cycle,
	 * which the viewport keeps within half a cycle of its centre.
	 */
	void RefreshLoopPresentations(const FGeometry& Viewport, float OffsetShift = 0);
	float GetLoopCycleExtent(EOrientation Orientation) const;
	UWidget* GetLoopAnchorEntry() const;
	FGeometry GetLoopAnchorGeometry() const;
	bool EnsureLoopCoverage(float ViewportExtent, EOrientation Orientation);

protected:
	/**
	 * The panel that holds the generated entries. A Blueprint derived from Reci List binds a Reci List Panel named
	 * ItemsBox; a native instance with no tree creates one as its root. Its Layout Settings arrange a list that no Reci
	 * layout box hosts; a hosting box owns the layout and replaces them.
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category="List Entries", meta=(BindWidget))
	TObjectPtr<UReciListPanel> ItemsBox;

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual UPanelWidget* GetGeneratedItemsPanel() const override;
	virtual void NativeOnListItemsRebuilt() override;
	virtual void NativeOnItemFocusReceived(UCommonButtonBase* FocusedButton) override;

	/**
	 * A looping list made another copy the item's live entry (Refresh Loop Presentations, after a viewport's rebase).
	 * A derived list carries per-entry presentation state from PreviousEntry over to NewEntry.
	 */
	virtual void NativeOnLoopEntryReassigned(int32 ItemIndex, UWidget* PreviousEntry, UWidget* NewEntry) {}

private:
	void ResolveItemsBox();
	/** Whether the list's layout box is the one a scroll box that loops its entries hosts, so Looping Entries takes effect. */
	bool IsHostedByLoopingViewport() const;
	void BuildLoopPresentations(int32 CycleCount);
	FGeometry GetLinearEntryGeometry(int32 PresentationIndex) const;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> LoopEntries;
	TArray<int32> ActiveLoopEntries;
	int32 LoopCycles = 0;
};
