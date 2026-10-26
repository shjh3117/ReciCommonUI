// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CommonActivatableWidget.h"
#include "Input/CommonBoundActionButton.h"
#include "Tests/ReciInteractionTestEntry.h"
#include "Tests/ReciTestHarness.h"
#include "Widgets/ReciDropdownBase.h"
#include "Widgets/ReciDropdownPopup.h"
#include "Widgets/ReciFocusStyleButtonBase.h"
#include "Widgets/ReciList.h"
#include "Widgets/ReciListEntryEmphasis.h"
#include "Widgets/ReciListItem.h"
#include "Widgets/ReciScrollBox.h"
#include "Widgets/ReciTextListItem.h"
#include "Widgets/ReciTextListItemButtonBase.h"
#if WITH_EDITOR
#include "Editor/WidgetCompilerLog.h"
#endif
#include "ReciTestWidgets.generated.h"

class UReciMappedKeyText;
class UTextBlock;

/** A Reci button whose authored tree, a 60 x 40 box, exists at Initialize, as a Widget Blueprint's does. */
UCLASS(Hidden, NotBlueprintable, Transient)
class RECICOMMONUI_API UReciTestButton : public UReciFocusStyleButtonBase
{
	GENERATED_BODY()
public:
	/** Initialises a button made with NewObject, after the test set what initialisation reads. */
	void InitializeForTest() { Initialize(); }
protected:
	virtual bool Initialize() override;
	/** Adds the authored widgets inside the box. */
	virtual void BuildContent(UWidgetTree& Tree, class USizeBox& Box) {}
};

/**
 * A test button whose tree has a Reci Mapped Key Text named InputActionWidget, bound as CommonUI's Input Action Widget
 * as a Widget Blueprint's is.
 */
UCLASS(Hidden, NotBlueprintable, Transient)
class UReciTestHintedButton : public UReciTestButton
{
	GENERATED_BODY()
public:
	UReciMappedKeyText* GetKeyText() const;
	/** The button's own focus events, as its internal button sends them. */
	void SimulateFocus(bool bFocused) { bFocused ? HandleFocusReceived() : HandleFocusLost(); }
	void SimulateHover() { NativeOnHovered(); }
	/** Runs the key-hint rule, as CommonUI's NativeConstruct does. */
	void UpdateHint() { UpdateInputActionWidget(); }
protected:
	virtual void BuildContent(UWidgetTree& Tree, USizeBox& Box) override;
};

/**
 * A Bound Action Bar's button whose tree, a 60 x 40 box, has a Reci Mapped Key Text named InputActionWidget, bound as
 * CommonUI's Input Action Widget as a Widget Blueprint's is.
 */
UCLASS(Hidden, NotBlueprintable, Transient)
class UReciTestBoundActionButton : public UCommonBoundActionButton
{
	GENERATED_BODY()
public:
	UReciMappedKeyText* GetKeyText() const;
protected:
	virtual bool Initialize() override;
};

/** An activatable widget that does not take screen input. */
UCLASS(Hidden, NotBlueprintable, Transient)
class UReciTestPlainScreen : public UCommonActivatableWidget
{
	GENERATED_BODY()
};

/** A test screen that registers its screen input when it activates, as a game's screen does. */
UCLASS(Hidden, NotBlueprintable, Transient)
class UReciTestRegisteringScreen : public UReciTestScreen
{
	GENERATED_BODY()
protected:
	virtual void NativeOnActivated() override;
};

/**
 * A list entry built on the test button: its tree exists at Initialize, so CommonUI wraps it in its internal button as it
 * does a Widget Blueprint's, and focus reaches the button's own focus events.
 */
UCLASS(Hidden, NotBlueprintable, Transient)
class RECICOMMONUI_API UReciTestListEntry : public UReciTestButton, public IReciListItem
{
	GENERATED_BODY()
public:
	virtual void SetListItemObject_Implementation(UObject* Item) override { Data = Item; }
	/** Clicks the button as its internal button does on a pointer click. */
	void SimulateClick() { HandleButtonClicked(); }
	UPROPERTY() TObjectPtr<UObject> Data;
};

/** An item that says whether its entry is disabled or hidden, as a game's item state does. */
UCLASS(Hidden, NotBlueprintable, Transient)
class UReciTestStateItem : public UObject
{
	GENERATED_BODY()
public:
	bool bDisabled = false;
	bool bHidden = false;
};

/** A test list entry that is disabled or hidden as its UReciTestStateItem says when the list gives it the item. */
UCLASS(Hidden, NotBlueprintable, Transient)
class UReciTestStateEntry : public UReciTestListEntry
{
	GENERATED_BODY()
public:
	virtual void SetListItemObject_Implementation(UObject* Item) override;
};

/** A list entry that records the emphasis its list passes it. */
UCLASS(Hidden, NotBlueprintable, Transient)
class UReciTestEmphasisEntry : public UReciTestListEntry, public IReciListEntryEmphasis
{
	GENERATED_BODY()
public:
	virtual void OnItemEmphasisChanged_Implementation(float InEmphasis) override { Emphasis = InEmphasis; ++EmphasisCount; }
	float Emphasis = -1.0f;
	int32 EmphasisCount = 0;
};

/** A Reci List that records whether a viewport drives its item emphasis. */
UCLASS(Hidden, NotBlueprintable, Transient)
class UReciTestEmphasisList : public UReciList
{
	GENERATED_BODY()
public:
	virtual void SetItemEmphasisDriven(bool bDriven) override { Super::SetItemEmphasisDriven(bDriven); bEmphasisDriven = bDriven; }
	bool bEmphasisDriven = false;
};

/** A Reci Scroll Box that, as a carousel does, drives its list's emphasis and loops its entries, and moves nothing itself. */
UCLASS(Hidden, NotBlueprintable, Transient)
class UReciTestLoopingScrollBox : public UReciScrollBox
{
	GENERATED_BODY()
public:
	virtual bool DrivesItemEmphasis() const override { return true; }
	virtual bool LoopsHostedEntries() const override { return true; }
};

/** A list entry that requires CommonUI hold. */
UCLASS(Hidden, NotBlueprintable, Transient)
class UReciTestHoldEntry : public UReciInteractionTestEntry
{
	GENERATED_BODY()
public:
	UReciTestHoldEntry(const FObjectInitializer& ObjectInitializer);
};

/** A list entry that is, wrongly, an activatable widget. */
UCLASS(Hidden, NotBlueprintable, Transient)
class UReciTestActivatableEntry : public UCommonActivatableWidget, public IReciListItem
{
	GENERATED_BODY()
public:
	virtual void SetListItemObject_Implementation(UObject* Item) override {}
};

/** A native Reci List that builds its own tree, a 60 x 40 box, and binds no ItemsBox in it. */
UCLASS(Hidden, NotBlueprintable, Transient)
class UReciTestRootedList : public UReciList
{
	GENERATED_BODY()
protected:
	virtual bool Initialize() override;
};

/**
 * A native Reci List whose tree, built at Initialize as a Widget Blueprint derived from Reci List authors it, is an
 * ItemsBox with Layout Settings of its own: horizontal, 7 apart.
 */
UCLASS(Hidden, NotBlueprintable, Transient)
class UReciTestLayoutList : public UReciList
{
	GENERATED_BODY()
public:
	static FReciListLayoutSettings GetAuthoredLayout();
	UReciListPanel* GetItemsBox() const { return ItemsBox; }
protected:
	virtual bool Initialize() override;
};

/** A text entry button for a dropdown's options. */
UCLASS(Hidden, NotBlueprintable, Transient)
class UReciTestTextEntry : public UReciTestButton, public IReciTextListItem
{
	GENERATED_BODY()
};

/**
 * A dropdown whose tree, built at Initialize as a Widget Blueprint's is, holds a Menu Anchor named DropdownAnchor around
 * a Text Block named SelectedValueText.
 */
UCLASS(Hidden, NotBlueprintable, Transient)
class UReciTestDropdown : public UReciDropdownBase
{
	GENERATED_BODY()
public:
	UMenuAnchor* GetAnchor() const { return DropdownAnchor; }
	UTextBlock* GetValueText() const { return SelectedValueText; }
	/** Whether the button is set up as the popup's toggle: Click Method Mouse Down, selectable, toggleable, interactable. */
	bool IsToggleSetUp() const
	{
		return ClickMethod == EButtonClickMethod::MouseDown && bSelectable && bToggleable && bInteractableWhenSelected;
	}
protected:
	virtual bool Initialize() override;
};

/** A dropdown popup whose tree is Border > Reci Scroll Box > Reci Vertical Box > Reci List named Options. */
UCLASS(Hidden, NotBlueprintable, Transient)
class UReciTestDropdownPopup : public UReciDropdownPopup
{
	GENERATED_BODY()
protected:
	virtual bool Initialize() override;
};

/** A native dropdown popup that leaves Options unset. */
UCLASS(Hidden, NotBlueprintable, Transient)
class UReciTestBareDropdownPopup : public UReciDropdownPopup
{
	GENERATED_BODY()
};

/** A Reci Text List Item Button whose tree, a 60 x 40 box holding a Text Block named ItemTextBlock, exists at Initialize. */
UCLASS(Hidden, NotBlueprintable, Transient)
class UReciTestTextItemButton : public UReciTextListItemButtonBase
{
	GENERATED_BODY()
public:
	UTextBlock* GetItemTextBlock() const { return ItemTextBlock; }
protected:
	virtual bool Initialize() override;
};

/** Records the events a test binds to it, in order. */
UCLASS(Hidden, NotBlueprintable, Transient)
class RECICOMMONUI_API UReciTestEventRecorder : public UObject
{
	GENERATED_BODY()
public:
	/** One entry per event: "<name> <value>". */
	TArray<FString> Events;

	FString ToString() const { return FString::Join(Events, TEXT(", ")); }
	void Reset() { Events.Reset(); }

	/** For an item event (a list's or a carousel's): "Item <index>". */
	UFUNCTION() void RecordItem(UObject* Item, int32 ItemIndex);
	/** For a list's On Item Selected: "Selected <item name>". */
	UFUNCTION() void RecordSelected(UObject* Item);
	/** For a dropdown's On Selection Changed: "Selection <index> <enum value>". */
	UFUNCTION() void RecordDropdownSelection(UEnum* SelectedEnum, FName SelectedValue, int32 SelectedIndex);
	/** For a dropdown's On Dropdown Open Changed: "Open" or "Closed". */
	UFUNCTION() void RecordOpen(bool bIsOpen);
	/** For a list's On Navigation Input: "Navigation <direction>". */
	UFUNCTION() void RecordNavigation(EReciNavigationDirection Direction);
};

/** A native enum for a dropdown's options, with a value the dropdown leaves out. */
UENUM()
enum class EReciTestSize : uint8
{
	Small,
	Medium,
	Secret UMETA(Hidden),
	Large
};

#if WITH_EDITOR
/** Records what a Blueprint compile check reports, in place of the Widget Blueprint compiler's log. */
class FReciTestCompilerLog final : public IWidgetCompilerLog
{
public:
	TArray<FString> Errors;
	TArray<FString> Warnings;

	virtual TSubclassOf<UUserWidget> GetContextClass() const override { return nullptr; }

protected:
	virtual void InternalLogMessage(TSharedRef<FTokenizedMessage>& Message) override
	{
		if (Message->GetSeverity() == EMessageSeverity::Error)
		{
			Errors.Add(Message->ToText().ToString());
		}
		else if (Message->GetSeverity() == EMessageSeverity::Warning)
		{
			Warnings.Add(Message->ToText().ToString());
		}
	}
};
#endif
