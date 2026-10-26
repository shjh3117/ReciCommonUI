// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CommonActionWidget.h"
#include "CoreMinimal.h"
#include "Widgets/SOverlay.h"

#include "ReciMappedKeyText.generated.h"

class SHorizontalBox;
class STextBlock;
class UCommonTextStyle;
class UInputAction;
class ULocalPlayer;
enum class ECommonInputType : uint8;
struct FReciInputMethod;
struct FSlateBrush;

namespace UE::ReciCommonUI::Private
{
	struct FReciKeySegment;
	struct FReciKeySegmentRules;
}

/** Whose keys a Reci Mapped Key Text shows. */
UENUM(BlueprintType)
enum class EReciKeyTextDevice : uint8
{
	/** The device the player is using; touch shows none. */
	Current,
	/** Keyboard and mouse keys, whatever the player is using. */
	KeyboardAndMouse,
	/** Gamepad keys, whatever the player is using. */
	Gamepad
};

/**
 * CommonUI action widget that shows every key the player's mappings bind to
 * its actions, in a row. Each key is drawn as a glyph from CommonUI's
 * controller data when the project provides one for the key and the shown
 * device (on gamepad, the current gamepad type), and as a name otherwise: the
 * instance's Key Name Overrides, else a built-in short name (ESC, ENTER, A,
 * LB, ...), else the key's display name. Without controller data CommonUI
 * cannot tell gamepad types apart, so the built-in gamepad names are the
 * XInput (Xbox) names. Keys always come from input actions, so they follow
 * the player's key bindings.
 *
 * A hint shows only keys that reach its action now (DESIGN.md principle 6): it
 * leaves out each key a handler earlier in the routing order takes first. In a
 * hint line (a text for the current device outside a CommonUI button), each
 * action's keys go to the screen action a registered screen on the focus path
 * maps it to, else to the focused widget for CommonUI's click action and, while
 * Reci follows a Reci UI Input Data, its navigation actions, else to CommonUI's
 * bindings after the focused widget; and while its activatable widget is
 * activated but outside CommonUI's active root (beneath a nested Is Modal
 * dialog, say), the line shows no key. In a Common Bound Action Button, the keys
 * go to CommonUI's binding after the focused widget. A Reci button sets its own
 * filter (Set Key Filter). A text fixed to a device (a key-binding row) and the
 * Input Action Widget of another CommonUI button show every key.
 *
 * @note In a CommonUI button (a Reci Focus Style Button, or the Common Bound
 * Action Button of an action bar), name this widget "InputActionWidget" so
 * CommonUI binds it as the button's Input Action Widget. There is no naming
 * requirement when this widget is used independently.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FReciKeyTextShowsKeysChanged, bool, bShowsKeys);

UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "Reci Mapped Key Text"))
class RECICOMMONUI_API UReciMappedKeyText final : public UCommonActionWidget
{
	GENERATED_BODY()

public:
	UReciMappedKeyText(const FObjectInitializer& ObjectInitializer);

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void SynchronizeProperties() override;

	/** Re-reads the active mappings and immediately updates the displayed text. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Input")
	void RefreshMappedKeyText();

	/** Returns the keys that would currently be displayed, as names: a text view, which names a key drawn as a glyph too. */
	UFUNCTION(BlueprintPure, Category = "Reci Common UI|Input")
	FText GetMappedKeyText() const;

	/** Whether the text shows at least one key now: not hidden, and not Unmapped Text. */
	UFUNCTION(BlueprintPure, Category = "Reci Common UI|Input")
	bool ShowsKeys() const { return LastShowsKeys.Get(false); }

	/**
	 * Fires when the text starts or stops showing keys, for example when the device changes or an earlier handler takes
	 * its keys, so a label beside the keys can hide with them.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Reci Common UI|Input")
	FReciKeyTextShowsKeysChanged OnShowsKeysChanged;

	/** Shows these actions' keys in order: the first becomes the represented action, the rest follow it. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Input")
	void SetEnhancedInputActions(const TArray<UInputAction*>& InActions);

	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Input")
	void SetKeyDevice(EReciKeyTextDevice InKeyDevice);

	/**
	 * C++: leaves out of the shown keys each key for which the filter returns false, such as a key another handler takes
	 * first (a Reci button's hint sets one). It applies before a name already shown is left out and before Maximum
	 * Displayed Keys counts, so with a maximum of 1 the first key the filter keeps shows. The reserved width still counts
	 * every key, so the layout does not shift, and a text whose keys the filter all leaves out hides, whatever Hide When
	 * Unmapped says. A filter replaces the text's own routing (see the class comment); an empty one puts it back.
	 * Refreshes the text.
	 */
	void SetKeyFilter(TFunction<bool(const FKey&)> InKeyFilter);

	/**
	 * Actions whose keys follow the represented action's, for one hint over several actions, such as previous and
	 * next, or the four directions. Each action's keys are joined by Key Separator, and the actions by Action
	 * Separator.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Text", meta = (EditCondition = "CommonInput.CommonInputSettings.IsEnhancedInputSupportEnabled", EditConditionHides))
	TArray<TObjectPtr<UInputAction>> AdditionalInputActions;

	/** Which device's keys show. Current follows the player; a fixed device suits a key-binding row. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Text")
	EReciKeyTextDevice KeyDevice = EReciKeyTextDevice::Current;

	/**
	 * Reserves room for the widest keys across keyboard and gamepad, glyphs included, so a button's layout never shifts.
	 * Turn off inside a running hint line, where the text should sit tight against its label.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Key Text")
	bool bReserveAllDeviceWidths = true;

	/**
	 * Text style for the mapped key names and separators, as a Common Text Block's Style. When set, its font, color and
	 * drop shadow are used instead of Font, Color And Opacity, Shadow Offset and Shadow Color And Opacity. Its margin and
	 * line height are not: Padding and Minimum Reserved Height space the keys, which stay on one line beside any glyph.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TSubclassOf<UCommonTextStyle> Style;

	/**
	 * Font used for the mapped key names and separators, unless Style is set. A glyph is drawn at its brush's own
	 * Image Size.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FSlateFontInfo Font;

	/** Text color used for the mapped key names and separators, unless Style is set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FSlateColor ColorAndOpacity = FSlateColor(FLinearColor::White);

	/** Offset applied to the text shadow, unless Style is set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FVector2D ShadowOffset = FVector2D::ZeroVector;

	/** Color and opacity of the text shadow, unless Style is set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FLinearColor ShadowColorAndOpacity = FLinearColor::Transparent;

	/** Internal padding around the text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FMargin Padding = FMargin(6.0f, 2.0f);

	/** Minimum hint row height. Width follows the parent fill slot and content. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (ClampMin = "0"))
	float MinimumReservedHeight = 24.0f;

	/** Horizontal alignment of the rendered key text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TEnumAsByte<ETextJustify::Type> Justification = ETextJustify::Center;

	/** Text inserted between one action's keys, for example "BS / ESC". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Key Text")
	FText KeySeparator;

	/** Text inserted between the actions' keys, for example the spaces in "W A S D". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Key Text")
	FText ActionSeparator;

	/** Text shown when the represented action currently has no matching mapping. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Key Text")
	FText UnmappedText;

	/** Preview text shown in the UMG designer before a runtime binding exists. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Key Text")
	FText DesignTimePreviewText;

	/** Uses FKey's long display name instead of its compact display name, for a key without a built-in name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Key Text")
	bool bUseLongDisplayNames = false;

	/** Converts the final key names to uppercase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Key Text")
	bool bUppercaseKeyNames = true;

	/** Hides the text while preserving its slot when no key is mapped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Key Text")
	bool bHideWhenUnmapped = false;

	/**
	 * A hint line for a key that a screen's key capture handles itself, such as the key that ends the capture: it shows
	 * the keys the capture gets while it captures (which takes every key) and none otherwise.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Key Text")
	bool bKeyCaptureHint = false;

	/** Maximum number of keys to display for each action. Zero displays every matching key. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Key Text", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaximumDisplayedKeys = 0;

	/**
	 * The author's own names for keys, keyed by FKey name. A key named here is drawn with this name, never as a glyph.
	 * Empty by default: the built-in short names (Escape -> ESC, BackSpace -> BS, mouse buttons LMB / RMB / MMB, arrows,
	 * the XInput gamepad names A / B / X / Y, LB, MENU, D-pad and stick directions) apply to a key that has no glyph.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Key Text")
	TMap<FName, FText> KeyNameOverrides;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
	/** As a Common Text Block does, keeps the properties a Style replaces read-only while one is set. */
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif

protected:
	virtual void UpdateActionWidget() override;

private:
#if WITH_DEV_AUTOMATION_TESTS
	/** Lets tests read what the owner set with CommonUI's SetHidden, and the rules the text draws with. */
	friend struct FReciMappedKeyTextTestAccess;
#endif

	using FKeySegment = UE::ReciCommonUI::Private::FReciKeySegment;

	/** Records whether keys show, and tells listeners when that changes. */
	void SetShowsKeys(bool bShowsKeys);

	/** Unset until the first update, so that update tells listeners either way. */
	TOptional<bool> LastShowsKeys;

	/** A row of segments: names and separators as text, glyphs as images. */
	struct FKeyRow
	{
		TSharedPtr<SHorizontalBox> Box;
		TArray<TSharedRef<STextBlock>> Texts;
		/** The brushes the row's images draw, kept for as long as the images are. */
		TArray<TSharedRef<FSlateBrush>> Brushes;
	};

	/**
	 * The keys of the represented actions for the input type, as a row of segments, through the key filter when
	 * bFiltered; none without an input subsystem.
	 */
	TArray<FKeySegment> BuildMappedKeySegments(ECommonInputType InputType, bool bFiltered) const;
	/** How this instance draws keys for the input type and gamepad, with its key filter when bFiltered. */
	UE::ReciCommonUI::Private::FReciKeySegmentRules MakeSegmentRules(ECommonInputType InputType, FName GamepadName, bool bFiltered) const;
	TArray<FKey> GetActionKeys(const UInputAction* Action) const;
	/** The player whose keys show: the one an action bar's binding belongs to, else the owning player. */
	const ULocalPlayer* GetKeyPlayer() const;
	/** That player's Slate user, whose routing the text's own filter reads. */
	TOptional<int32> GetKeyUserIndex() const;
	/** With no filter set: the Input Action Widget of a Common Bound Action Button, whose keys go to CommonUI's binding. */
	bool IsActionBarText() const;
	/** With no filter set: a text for the current device that is not a CommonUI button's Input Action Widget. */
	bool IsHintLineText() const;
	/** Whether the shown keys go through a filter: the owner's, or the text's own routing. */
	bool FiltersKeys() const;
	/** Leaves out of each action's keys, in KeyGroups, those that an earlier handler takes now, by the text's own routing. */
	void LeaveOutTakenKeys(TArray<TArray<FKey>>& KeyGroups, TConstArrayView<const UInputAction*> GroupActions,
		ECommonInputType InputType) const;
	/** Listens to the coordinator's routing changes while the text's own routing filters it and its Slate widget exists. */
	void UpdateKeyRoutingListener();
	/** That player's input method, as CommonInput detects it; unset without one. */
	TOptional<FReciInputMethod> GetInputMethod() const;
	ECommonInputType GetDisplayedInputType() const;
	void ReserveHintLayout();
	bool HasRepresentedAction() const;
	/** Style's defaults, or none while Style is unset. */
	const UCommonTextStyle* GetStyleCDO() const;
	void ApplyAppearance();
	/** Replaces the row's content with the segments, consecutive names and separators in one text. */
	void FillRow(FKeyRow& Row, TConstArrayView<FKeySegment> Segments, bool bVisibleRow);
	void ApplyTextAppearance(STextBlock& Text, bool bVisibleRow) const;

	/** The keys the widget shows. */
	FKeyRow MyKeyRow;
	SOverlay::FOverlaySlot* MyKeyRowSlot = nullptr;
	/** Hidden rows that reserve the width of the keyboard's and the gamepad's keys. */
	FKeyRow MyReservationRow;
	FKeyRow MyGamepadReservationRow;
	TSharedPtr<STextBlock> MyFallbackReservationText;
	bool bShowsMappedKeys = false;
	TFunction<bool(const FKey&)> KeyFilter;
	FDelegateHandle KeyRoutingListener;
};
