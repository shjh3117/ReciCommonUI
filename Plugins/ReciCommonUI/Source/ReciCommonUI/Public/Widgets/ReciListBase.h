// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CommonUserWidget.h"
#include "CoreMinimal.h"
#include "Framework/Views/ITypedTableView.h"
#include "Input/NavigationReply.h"
#include "Navigation/ReciNavigationTypes.h"
#include "Widgets/ReciListPanel.h"

#include "ReciListBase.generated.h"

class UCommonButtonBase;
class FReciListFocusReturnController;
class FReciListSelectionController;
class UPanelWidget;
class UReciListEntryRuntime;
class UUserWidget;
class IReciListController;
#if WITH_EDITOR
class IWidgetCompilerLog;
#endif

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FReciListItemSelectionChanged,
	UObject*,
	Item,
	bool,
	bIsSelected);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FReciListItemSelected,
	UObject*,
	Item);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FReciListItemEvent,
	UObject*,
	Item,
	int32,
	ItemIndex);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FReciNavigationInputEvent,
	EReciNavigationDirection,
	Direction);

DECLARE_MULTICAST_DELEGATE(FReciListEntriesChangedNative);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FReciListFocusedIndexChangedNative,
	int32);

DECLARE_MULTICAST_DELEGATE_OneParam(
	FReciListDrivenIndexChangedNative,
	int32);

/**
 * Shared data, selection and focus contract for Reci lists.
 *
 * Unreal's ListView is not used because it cannot focus widgets inside
 * individual list items.
 *
 * Concrete widgets decide how entries are laid out. This base owns item
 * lifetime, CommonButtonBase selection and focus-return behavior.
 */
UCLASS(
	Abstract,
	BlueprintType,
	NotBlueprintable,
	ClassGroup = UI,
	meta = (
		DisplayName = "Reci List Base",
		Category = "Reci Common UI"))
class RECICOMMONUI_API UReciListBase : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UReciListBase(const FObjectInitializer& ObjectInitializer);
	virtual ~UReciListBase() override;

	/** Number of live entries exposed to a Reci layout host. */
	int32 GetHostedItemCount() const;

	/** Entry widget exposed to a Reci layout host, or null. */
	virtual UWidget* GetHostedEntryWidget(int32 ItemIndex) const;
	/** Layout geometry is also available for repeated views outside the clip rect. */
	virtual FGeometry GetHostedEntryGeometry(int32 ItemIndex) const;

	/**
	 * Every widget that presents the item: its entry, each copy of a looping item, or each presentation a list derived
	 * from this one adds (an advanced list's normal and main widgets). Empty for an index with no item.
	 */
	virtual TArray<UUserWidget*> GetEntryPresentations(int32 Index) const;

	/** Where Presentation, one of Get Entry Presentations, is laid out, also while it is outside the clip rect. */
	virtual FGeometry GetEntryPresentationGeometry(UUserWidget* Presentation) const;

	/** Entry button exposed to a Reci layout host, or null. */
	UFUNCTION(BlueprintPure, Category = "Reci Common UI|List")
	UCommonButtonBase* GetHostedEntryButton(int32 ItemIndex) const;

	/** Selection-only sources can navigate without giving their buttons focus. */
	virtual bool IsHostedItemNavigable(int32 ItemIndex) const;

	/** Focus target used when a surrounding layout receives focus. */
	UFUNCTION(BlueprintPure, Category = "Reci Common UI|List")
	UWidget* GetHostedFocusTarget() const;

	/**
	 * Runs the list's click on the item, as a click on its entry's button does: for a layout host, viewport or
	 * composite that recognises clicks on entries itself. Returns whether the list took it.
	 */
	bool HandleHostedItemClick(int32 ItemIndex);

	/** Internal: native notification used by layout hosts to rebuild their Slate slots. */
	FReciListEntriesChangedNative& OnEntriesChangedNative();

	/** Broadcast with the item's index when one of the list's entries receives focus, for a host that follows focus. */
	FReciListFocusedIndexChangedNative& OnFocusedIndexChangedNative();

	/** Internal: native logical-item notification used by layout and viewport hosts. */
	FReciListDrivenIndexChangedNative& OnDrivenIndexChangedNative();

	/** Internal: whether Blueprint is currently listening for navigation input. */
	bool HasNavigationInputHandler() const;

	/** Internal: emits one cardinal navigation input to Blueprint listeners. */
	bool BroadcastNavigationInput(EUINavigation Direction);

	/**
	 * Updates the logical item driven by a surrounding layout or viewport.
	 * Advanced sources override this to drive their main presentation.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|List")
	virtual bool SetDrivenItemIndex(int32 ItemIndex, bool bGiveFocus);

	/** Logical item currently driven by a surrounding layout or viewport. */
	UFUNCTION(BlueprintPure, Category = "Reci Common UI|List")
	virtual int32 GetDrivenItemIndex() const;

	/** Confirms the item currently driven by a surrounding layout or viewport (screens that own their keys call this for Accept). */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|List")
	virtual bool ConfirmDrivenItem();

	/**
	 * The emphasis protocol: a viewport that drives item emphasis (UReciScrollBox::DrivesItemEmphasis, such as a
	 * carousel by how near each item is to its centre) sets how much each item is emphasised, from 0 (none) to 1 (full),
	 * whenever it changes. The list passes it to each of Get Entry Presentations that implements Reci List Entry
	 * Emphasis, and to its list controller; a list derived from this one may also show it (an advanced list blends its
	 * main presentation). Returns false for an index with no item. With no such viewport the list sends no emphasis.
	 */
	virtual bool SetItemEmphasis(int32 ItemIndex, float Emphasis);

	/**
	 * Called by the list's layout box when a viewport that drives item emphasis starts hosting it (true) and when it
	 * stops (false). A list that shows emphasis on its own while no viewport drives it (an advanced list's main item
	 * follows focus) leaves it to the viewport meanwhile.
	 */
	virtual void SetItemEmphasisDriven(bool bDriven);

	/**
	 * Internal (layout boxes): stores and applies the layout policy supplied by a Reci layout box, which owns the hosted
	 * list's layout. The first time, it reports once Layout Settings the list's own panel was given, which it replaces.
	 */
	bool ApplyHostedLayoutSettings(
		const FReciListLayoutSettings& InSettings);

#if WITH_EDITOR
	/**
	 * Warns at Blueprint compile time about an Entry Widget Class the list cannot serve: one deriving from
	 * CommonActivatableWidget, and one whose button requires hold (a list's Accept and the clicks a Reci Scroll Box
	 * recognises confirm without CommonUI's hold).
	 */
	virtual void ValidateCompiledDefaults(class IWidgetCompilerLog& CompileLog) const override;
#endif

	/**
	 * The widget class created for each object supplied to Set List Items. It must not derive from Common Activatable
	 * Widget, since Reci takes a widget's nearest one as its screen. An entry button that requires hold is reported:
	 * a list's Accept and the clicks a Reci Scroll Box recognises confirm without CommonUI's hold.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "List Entries",
		meta = (
			DisplayName = "Entry Widget Class"))
	TSubclassOf<UUserWidget> ItemWidgetClass;

	/**
	 * When disabled, hosted entries ignore pointer hover: pointer movement does
	 * not move list focus, emit Reci hover events or apply hover styling.
	 * Clicking, dragging and keyboard/gamepad navigation remain independent.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		BlueprintSetter = SetAllowPointerHover,
		Category = "Input",
		meta = (DisplayName = "Allow Pointer Hover"))
	bool bAllowPointerHover = true;

	/** Hover visuals may remain enabled without moving keyboard/gamepad focus. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bFocusOnPointerHover = false;

	/**
	 * Disable for lists driven independently by screen-level input routing: the driven item then moves while
	 * Slate focus is elsewhere, and shows a key cursor (Show Driven Item Cursor).
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		BlueprintSetter = SetFocusOnNavigation,
		Category = "Input")
	bool bFocusOnNavigation = true;

	/** Opt-in automatic selection for single-choice lists. Never applies to Multi or None. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection")
	bool bSelectionFollowsNavigation = false;

	/**
	 * Also confirms (On Item Confirmed) when an item is clicked, as an action menu does (Selection Mode None).
	 * Off by default: a click selects (Explicit single choice). In a carousel only a click on the centred item
	 * counts as the click.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bConfirmOnItemClick = false;

	/**
	 * A list with Focus On Navigation off is driven by screen keys: its driven item wears the focus look (a key
	 * cursor) while focus is elsewhere. A screen that drives several such lists one at a time turns this off on
	 * the lists its keys are not driving. No effect while Focus On Navigation is on: that list's driven item is
	 * where focus is whenever keys act on it. Set it through SetShowDrivenItemCursor so the entries refresh.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Input",
		meta = (EditCondition = "!bFocusOnNavigation"))
	bool bShowDrivenItemCursor = true;

	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|List")
	void SetShowDrivenItemCursor(bool bShow);

	/** Sets Focus On Navigation and redraws the key cursor. */
	UFUNCTION(BlueprintSetter, Category = "Reci Common UI|List|Input")
	void SetFocusOnNavigation(bool bInFocusOnNavigation);

	/**
	 * Clears the current selection when focus returns after leaving this list.
	 * Focus still returns where it would without this.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus")
	bool bClearSelectionOnFocusReturn = false;

	/**
	 * Focus that returns after leaving this list goes to the driven item, which follows focus while Focus On Navigation
	 * is on and may have moved while focus was away (a screen key, a carousel drag); the list keeps no last focused item.
	 * When enabled, it goes to the first entry that can take focus instead. An entry a pointer focuses and an item Set
	 * Focused Item Index requests are not redirected. This does not change selection.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus")
	bool bResetLastFocusedItemOnFocusReturn = false;

	/** Matches the selection modes exposed by Unreal's standard list views. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection")
	TEnumAsByte<ESelectionMode::Type> SelectionMode =
		ESelectionMode::Single;

	/** Emitted whenever one of the generic item objects changes selection. */
	UPROPERTY(
		BlueprintAssignable,
		Category = "Events",
		meta = (DisplayName = "On Item Selection Changed"))
	FReciListItemSelectionChanged OnItemSelectionChanged;

	/** Emitted only when a generic item object becomes selected. */
	UPROPERTY(
		BlueprintAssignable,
		Category = "Events",
		meta = (DisplayName = "On Item Selected"))
	FReciListItemSelected OnItemSelected;

	/** Emitted when focus moves to another item or leaves the list. */
	UPROPERTY(
		BlueprintAssignable,
		Category = "Events",
		meta = (DisplayName = "On Focused Item Changed"))
	FReciListItemEvent OnFocusedItemChanged;

	/** Logical navigation/centering changed the item; pointer hover does not emit this. */
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FReciListItemEvent OnDrivenItemChanged;

	/** Explicit pointer/programmatic entry click, independent of selection and confirmation. */
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FReciListItemEvent OnItemClicked;

	/** Emitted when the focused item is explicitly confirmed. */
	UPROPERTY(
		BlueprintAssignable,
		Category = "Events",
		meta = (DisplayName = "On Item Confirmed"))
	FReciListItemEvent OnItemConfirmed;

	/**
	 * Emitted for every cardinal navigation input routed to this list.
	 * Keyboard, gamepad and ReciScrollBox pointer gestures share this event. A drag across the scroll box's axis first
	 * makes the entry it began on the driven item, as keys would (with focus while Focus On Navigation is on). On a
	 * carousel card that is not centred it only centres that card, and on no interactable entry it does nothing; neither
	 * is broadcast.
	 */
	UPROPERTY(
		BlueprintAssignable,
		Category = "Events",
		meta = (DisplayName = "On Navigation Input"))
	FReciNavigationInputEvent OnNavigationInput;

	/** Changes the selection mode while preserving valid selection where possible. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|List")
	void SetSelectionMode(
		TEnumAsByte<ESelectionMode::Type> InSelectionMode);

	/** Enables or disables every Reci hover response for this list's entries. */
	UFUNCTION(BlueprintSetter, Category = "Reci Common UI|List|Input")
	void SetAllowPointerHover(bool bInAllowPointerHover);

	/** Returns whether this list's entries may process pointer hover. */
	UFUNCTION(BlueprintPure, Category = "Reci Common UI|List|Input")
	bool IsPointerHoverAllowed() const;

	/**
	 * Changes the widget class used by every list entry.
	 * Existing item objects are preserved and their widgets are rebuilt.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|List")
	virtual void SetEntryWidgetClass(
		TSubclassOf<UUserWidget> InEntryWidgetClass);

	/** Deselects every item in the list. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|List")
	void ClearSelection();

	/** Moves focus to the entry representing Item. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|List")
	bool SetFocusedItem(UObject* Item);

	/** Moves focus to the entry at Item Index. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|List")
	bool SetFocusedItemIndex(int32 ItemIndex);

	/** Returns the item that currently owns focus, or null. */
	UFUNCTION(BlueprintPure, Category = "Reci Common UI|List")
	UObject* GetFocusedItem() const;

	/** Returns the currently focused item index, or -1. */
	UFUNCTION(BlueprintPure, Category = "Reci Common UI|List")
	int32 GetFocusedItemIndex() const;

	/** Confirms the currently focused item without changing focus. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|List")
	bool ConfirmFocusedItem();

	/** Selects the entry representing Item. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|List")
	bool SetSelectedItem(UObject* Item, bool bGiveFocus = false);

	/** Selects the entry at Item Index. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|List")
	bool SetSelectedItemIndex(
		int32 ItemIndex,
		bool bGiveFocus = false);

	/** Returns the first selected item object, or null when none is selected. */
	UFUNCTION(
		BlueprintCallable,
		Category = "Reci Common UI|List")
	UObject* GetSelectedItem() const;

	/** Returns every selected item object in visual order. */
	UFUNCTION(
		BlueprintCallable,
		Category = "Reci Common UI|List")
	bool GetSelectedItems(TArray<UObject*>& OutItems) const;

	/** Returns the number of selected items. */
	UFUNCTION(
		BlueprintCallable,
		Category = "Reci Common UI|List")
	int32 GetNumItemsSelected() const;

	/**
	 * Replaces the list with one entry widget per non-null item object. Focus the owning player had on the list or
	 * inside it moves to the driven item's new entry, or, when that entry cannot take focus, where entering the list puts
	 * it: the first enabled entry, or a focusable list itself when no entry can take focus. Focus elsewhere stays where it
	 * is, and when it comes back to the list it returns to the driven item's entry, or to the first enabled entry when
	 * that entry cannot take focus.
	 */
	UFUNCTION(
		BlueprintCallable,
		Category = "Reci Common UI|List",
		meta = (DisplayName = "Set List Items"))
	virtual void SetListItems(const TArray<UObject*>& InListItems);

	/** Adds a non-null, unique item and rebuilds this small list. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|List")
	bool AddItem(UObject* Item);

	/** Inserts a non-null, unique item at Item Index and rebuilds the list. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|List")
	bool InsertItem(UObject* Item, int32 ItemIndex);

	/** Removes Item and rebuilds the list. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|List")
	bool RemoveItem(UObject* Item);

	/** Removes the item at Item Index and rebuilds the list. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|List")
	bool RemoveItemAt(int32 ItemIndex);

	/** Removes every item from the list. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|List")
	void ClearListItems();

	/** Returns the number of items in the list. */
	UFUNCTION(BlueprintPure, Category = "Reci Common UI|List")
	int32 GetItemCount() const;

	/** Returns the index of the first selected item, or INDEX_NONE. */
	UFUNCTION(BlueprintPure, Category = "Reci Common UI|List")
	int32 GetSelectedItemIndex() const;

	/** Returns the item at Item Index, or null. */
	UFUNCTION(BlueprintPure, Category = "Reci Common UI|List")
	UObject* GetItemAt(int32 ItemIndex) const;

	/**
	 * Replaces the list with one text entry per supplied value.
	 * Entry Widget Class must implement Reci Text List Item.
	 */
	UFUNCTION(
		BlueprintCallable,
		Category = "Reci Common UI|List",
		meta = (DisplayName = "Set Text List Items"))
	void SetTextListItems(const TArray<FText>& InListItems);

	/**
	 * Sets the item that receives focus when focus first enters this list; Set List Items clears it. Focus that returns
	 * after leaving the list goes to the driven item instead. An invalid index keeps the normal first-item behavior.
	 */
	void SetInitialFocusItemIndex(int32 InItemIndex);

protected:
	/** Specialized sources may supply a different entry data contract. */
	virtual bool SupportsEntryWidgetClass(const UClass* EntryClass) const;

	virtual void NativeOnInitialized() override;
	virtual bool NativeSupportsKeyboardFocus() const override;
	virtual FReply NativeOnFocusReceived(
		const FGeometry& InGeometry,
		const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnAddedToFocusPath(
		const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(
		const FFocusEvent& InFocusEvent) override;

	/**
	 * Returns the panel that owns generated entries. Concrete list widgets own
	 * their layout binding and expose only the generated-items panel here.
	 */
	virtual UPanelWidget* GetGeneratedItemsPanel() const PURE_VIRTUAL(
		UReciListBase::GetGeneratedItemsPanel,
		return nullptr;);

	/** Creates and initializes one entry without adding it to the panel. */
	virtual UWidget* NativeCreateEntryWidget(
		UObject* ItemObject,
		int32 ItemIndex);

	/** Resolves the CommonUI button that represents one generated entry. */
	virtual UCommonButtonBase* ResolveEntrySelectionButton(
		UWidget* EntryWidget,
		int32 ItemIndex) const;

	/**
	 * Replaces the registered button for a stable logical entry. This is used
	 * by composite entries whose active presentation changes without rebuilding
	 * the list.
	 */
	void ReplaceEntrySelectionButton(
		int32 ItemIndex,
		UCommonButtonBase* NewButton);

	/** Called after Set List Items has finished rebuilding all entry widgets. */
	virtual void NativeOnListItemsRebuilt();

	/** Called whenever one of this list's entry buttons receives focus. */
	virtual void NativeOnItemFocusReceived(UCommonButtonBase* FocusedButton);

	/** Called whenever one of this list's entry buttons is clicked. */
	virtual void NativeOnItemClicked(UCommonButtonBase* ClickedButton);

	/** Broadcasts a semantic driven-item change from a specialized source. */
	void NotifyDrivenItemIndexChanged(int32 ItemIndex);

	/** Puts the key cursor on the driven item (or clears it) per Show Driven Item Cursor and Focus On Navigation. */
	void RefreshDrivenItemCursor();

	/**
	 * Whether the owning player's focus is on the list or inside it. A rebuild that replaces the entries checks this
	 * first and afterwards returns that focus to the driven item; another user's focus is not the player's to move.
	 */
	bool HasOwningPlayerFocus() const;

	/**
	 * After a rebuild that replaced the entries the owning player's focus was in: focus goes to the driven item's entry,
	 * or, when that entry cannot take focus, where entering the list puts it.
	 */
	void ReturnFocusAfterRebuild();

	/** Resolves the widget class used for one item during a list rebuild. */
	virtual TSubclassOf<UUserWidget> ResolveEntryWidgetClass(
		UObject* ItemObject,
		int32 ItemIndex) const;

	int32 GetItemButtonIndex(const UCommonButtonBase* ItemButton) const;
	UWidget* GetEntryWidgetAtIndex(int32 ItemIndex) const;
	UCommonButtonBase* GetItemButtonAtIndex(int32 ItemIndex) const;
	UObject* GetItemObjectAtIndex(int32 ItemIndex) const;
	bool SelectItemAtIndex(int32 ItemIndex, bool bGiveFocus);
	bool ConfirmItemAtIndex(int32 ItemIndex);

public:
	/**
	 * Attaches the list-controller seam (IReciListController) for a widget that composes lists, or detaches it with
	 * null. The controller is used only while Owner lives, so a destroyed composite is never called back. Set it before
	 * Set List Items, since it decides which entry classes the list serves.
	 */
	void SetListController(UObject* Owner, IReciListController* Controller);

	/** The attached list controller, while its owner lives; null otherwise. */
	IReciListController* GetListController() const;

	/**
	 * For a list that creates entries of a second class too (an advanced list's main presentation): creates an entry
	 * of EntryClass owned by Owner and gives it Item as Set List Items gives an entry its item, through Reci Text List
	 * Item for text item data or Reci List Item otherwise. Null when EntryClass implements neither.
	 */
	static UUserWidget* CreateEntryForItem(UWidget& Owner, UObject& Item, TSubclassOf<UUserWidget> EntryClass);

	/** The CommonUI button an entry presents: the entry itself, or the first button in its tree. */
	static UCommonButtonBase* FindEntryButton(UWidget* Entry);

	/** Reports once, as for Entry Widget Class, what the list cannot serve in another entry class it creates. */
	void ReportEntryWidgetClass(const UClass* EntryClass) const;

#if WITH_EDITOR
	/** Compile-time form of Report Entry Widget Class, for a list template the Blueprint compiler checks. */
	void ValidateEntryWidgetClass(const UClass* EntryClass, IWidgetCompilerLog& CompileLog) const;
#endif

private:
	TWeakObjectPtr<UObject> ListControllerOwner;
	IReciListController* ListController = nullptr;
	struct FPendingSelectionChange
	{
		TWeakObjectPtr<UCommonButtonBase> Button;
		bool bIsSelected = false;
	};

	UWidget* FindFirstItemFocusTarget() const;
	UWidget* FindItemFocusTargetAtIndex(int32 ItemIndex) const;
	UWidget* FindFocusReturnTarget() const;
	bool IsRegisteredItemButton(
		const UCommonButtonBase* ItemButton) const;
	void RegisterItemSelection(UReciListEntryRuntime& Entry, int32 ItemIndex);
	void BindItemSelectionButton(UCommonButtonBase* ItemButton);
	void UnbindItemSelectionButton(UCommonButtonBase* ItemButton);
	void HandleItemClicked(UCommonButtonBase* ClickedButton);
	void ApplySelectionModeToItem(UCommonButtonBase* ItemButton);
	void ApplySelectionModeToItems();
	void HandleItemFocusReceived(UCommonButtonBase* FocusedButton);
	void CompleteFocusReturn();
	void HandleItemSelectionChanged(
		bool bIsSelected,
		UCommonButtonBase* ChangedButton);
	void ProcessPendingSelectionChanges();
	void BroadcastItemSelectionChanged(
		UCommonButtonBase* ChangedButton,
		bool bIsSelected);
	void CommitItemFocus(UCommonButtonBase* FocusedButton);
	void SetFocusedItemState(int32 ItemIndex);
	int32 FindItemIndex(const UObject* Item) const;
	void CopyListItems(TArray<UObject*>& OutItems) const;
	void ClearListItemsInternal();

	/** One GC-safe record per logical entry; indices never span parallel arrays. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UReciListEntryRuntime>> EntryRuntimes;

	TUniquePtr<FReciListFocusReturnController> FocusReturnController;
	TUniquePtr<FReciListSelectionController> SelectionController;
	FReciListEntriesChangedNative EntriesChangedNative;
	FReciListFocusedIndexChangedNative FocusedIndexChangedNative;
	FReciListDrivenIndexChangedNative DrivenIndexChangedNative;
	FReciListLayoutSettings HostedLayoutSettings;
	/** A Reci layout box gave HostedLayoutSettings; without one, the list's panel keeps its own. */
	bool bHasHostedLayoutSettings = false;
	/** HostedLayoutSettings reached the panel once, which reported any Layout Settings of its own. */
	bool bPanelTookHostedLayout = false;
	TArray<FPendingSelectionChange> PendingSelectionChanges;
	int32 DrivenItemIndex = INDEX_NONE;
	/** True while SetFocusedItemIndex moves focus; focus return must not redirect that item. */
	bool bFocusingRequestedItem = false;
	int32 FocusedItemIndex = INDEX_NONE;
	bool bProcessingSelectionChanges = false;
};
