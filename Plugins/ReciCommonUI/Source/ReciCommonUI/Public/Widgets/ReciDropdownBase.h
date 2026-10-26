// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ReciFocusStyleButtonBase.h"

#include "ReciDropdownBase.generated.h"

class UMenuAnchor;
class UReciDropdownPopup;
class UReciList;
class UReciTextListItemData;
class UTextBlock;
class UEnum;
class UUserWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FReciDropdownSelectionChanged,
	UEnum*,
	SelectedEnum,
	FName,
	SelectedValue,
	int32,
	SelectedIndex);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FReciDropdownOpenChanged,
	bool,
	bIsOpen);

/**
 * CommonUI button that opens a Reci Dropdown Popup in a Menu Anchor; the popup's Options list shows the options.
 *
 * A Widget Blueprint derived from this class must contain a Menu Anchor named
 * DropdownAnchor. Its regular child is the visible button content. A text
 * block named SelectedValueText may be added to receive the selected value
 * automatically.
 *
 * The button is the popup's toggle: it needs Click Method Mouse Down (as Slate's combo button, so that a press on the
 * open anchor closes it rather than opening it again), Is Selectable, Is Toggleable and Interactable When Selected.
 * They are its defaults; one changed from them is a Blueprint compile warning, and is reported once and set back when
 * the dropdown is constructed. A touch, which clicks on its release, also closes the popup when it begins on the open
 * anchor: the press closes it, and its release does not open it again.
 */
UCLASS(
	Abstract,
	BlueprintType,
	Blueprintable,
	ClassGroup = UI,
	meta = (
		DisplayName = "Reci Dropdown Base",
		Category = "Reci Common UI",
		DisableNativeTick))
class RECICOMMONUI_API UReciDropdownBase
	: public UReciFocusStyleButtonBase
{
	GENERATED_BODY()

public:
	UReciDropdownBase(const FObjectInitializer& ObjectInitializer);

	/** Currently selected option index, or -1 when no option exists. */
	UPROPERTY(
		Transient,
		BlueprintReadOnly,
		Category = "Dropdown|Content")
	int32 SelectedIndex = INDEX_NONE;

	/**
	 * The popup, a Widget Blueprint derived from Reci Dropdown Popup: its Options list, the content of a Reci layout box,
	 * shows the options, so navigation, Accept, pointer and wheel input follow the usual list rules, and the class
	 * supplies the popup presentation (panel, scrolling, size). A dropdown placed in a Widget Blueprint without one is a
	 * compile warning.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Dropdown|Classes",
		meta = (ExposeOnSpawn = true))
	TSubclassOf<UReciDropdownPopup> DropdownPopupClass;

	/**
	 * Optional per-entry class override.
	 *
	 * Leave unset to use the Entry Widget Class of the popup's Options list.
	 * The class must implement Reci Text List Item. The entry itself or
	 * its Desired Focus Widget must also derive from CommonButtonBase so the
	 * dropdown can receive a selection.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Dropdown|Classes",
		meta = (
			ExposeOnSpawn = true,
			MustImplement = "/Script/ReciCommonUI.ReciTextListItem"))
	TSubclassOf<UUserWidget> DropdownEntryWidgetClass;

	/** Emitted when the selected option changes. Options set from texts report a None enum value. */
	UPROPERTY(
		BlueprintAssignable,
		Category = "Dropdown|Events",
		meta = (DisplayName = "On Selection Changed"))
	FReciDropdownSelectionChanged OnSelectionChanged;

	/** Emitted whenever the popup opens or closes. */
	UPROPERTY(
		BlueprintAssignable,
		Category = "Dropdown|Events",
		meta = (DisplayName = "On Dropdown Open Changed"))
	FReciDropdownOpenChanged OnDropdownOpenChanged;

	/**
	 * Builds dropdown options directly from a runtime enum. Hidden and MAX
	 * enum values are omitted, and Current Enum Value selects the initial item.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Dropdown")
	void SetDropdownEnum(
		UEnum* InEnum,
		FName CurrentEnumValue);

	/**
	 * Builds dropdown options from display texts, for choices that are not an enum. Selected Index picks the shown
	 * option without emitting On Selection Changed, so a widget can refresh its options from its data at any time.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Dropdown")
	void SetDropdownOptions(
		const TArray<FText>& Options,
		int32 InSelectedIndex);

	/** Selects an option by index and emits On Selection Changed. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Dropdown")
	bool SetSelectedIndex(int32 InSelectedIndex);

	/** Returns the selected enum member name, or None when nothing is selected. */
	UFUNCTION(BlueprintPure, Category = "Reci Common UI|Dropdown")
	FName GetSelectedEnumValue() const;

	/** Opens the dropdown and transfers focus to its list. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Dropdown")
	void OpenDropdown();

	/** Closes the dropdown and clears its selected state. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Dropdown")
	void CloseDropdown();

	/** Toggles the dropdown open state. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Dropdown")
	void ToggleDropdown();

	/** Returns whether the dropdown popup is currently open. */
	UFUNCTION(BlueprintPure, Category = "Reci Common UI|Dropdown")
	bool IsDropdownOpen() const;

#if WITH_EDITOR
	/**
	 * Warns at Blueprint compile time about a placed dropdown without a Dropdown Popup Class, and about a Click Method,
	 * Is Selectable, Is Toggleable or Interactable When Selected changed from what the dropdown needs.
	 */
	virtual void ValidateCompiledDefaults(class IWidgetCompilerLog& CompileLog) const override;
#endif

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void SynchronizeProperties() override;
	virtual void NativeOnSelected(bool bBroadcast) override;
	virtual void NativeOnDeselected(bool bBroadcast) override;
	virtual void NativeOnPressed() override;
	virtual void NativeOnReleased() override;

	/**
	 * Required Menu Anchor binding.
	 *
	 * Add a Menu Anchor named DropdownAnchor to the derived Widget Blueprint.
	 */
	UPROPERTY(
		BlueprintReadOnly,
		Category = "Dropdown|Bindings",
		meta = (BindWidget))
	TObjectPtr<UMenuAnchor> DropdownAnchor;

	/**
	 * Optional selected-value label.
	 *
	 * When a Text Block or Common Text Block named SelectedValueText exists,
	 * this class updates it automatically.
	 */
	UPROPERTY(
		BlueprintReadOnly,
		Category = "Dropdown|Bindings",
		meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SelectedValueText;

private:
	UFUNCTION()
	UUserWidget* CreateDropdownContent();

	UFUNCTION()
	void HandleDropdownOpenChanged(bool bIsOpen);

	/** A pointer click or an Accept on an option chooses it and closes the popup; moving the cursor does not. */
	UFUNCTION()
	void HandleListItemChosen(UObject* Item, int32 ItemIndex);

	int32 GetOwningUserIndex() const;

	/** The toggle settings the dropdown needs that differ now, named with their values, such as "Is Toggleable (off)". */
	TArray<FString> DescribeChangedToggleSettings() const;

	bool ApplySelectedIndex(int32 InSelectedIndex, bool bBroadcast);
	void BroadcastSelectionChanged();
	FText GetSelectedDisplayName() const;
	void RefreshOpenDropdown();
	void PopulateDropdownList(UReciList& DropdownList);
	void UpdateSelectedValueText();

	/** Enum source for the current options, if any. */
	UPROPERTY(Transient)
	TObjectPtr<UEnum> OptionEnum;

	/** Enum member name corresponding to each entry in Option Display Names. */
	UPROPERTY(Transient)
	TArray<FName> OptionEnumValueNames;

	/** Localized display name for each enum member in the popup. */
	UPROPERTY(Transient)
	TArray<FText> OptionDisplayNames;

	UPROPERTY(Transient)
	TObjectPtr<UReciList> ActiveDropdownList;

	/** Widget that held focus when the popup opened; closing returns focus to it. */
	TWeakPtr<SWidget> FocusReturnWidget;

	/**
	 * Set by a press that begins while the popup is open. Unless the button's own click closes the popup first (a mouse
	 * press), the press closes it by taking focus from its content, and the click on the release (a touch) must not open
	 * it again. Cleared by that click, by the button closing the popup itself, or on the tick after a release.
	 */
	bool bPressBeganOnOpenPopup = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UReciTextListItemData>>
		ActiveDropdownItems;
};
