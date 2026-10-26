// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CommonButtonBase.h"
#include "CoreMinimal.h"
#include "Interaction/ReciInputFocusCoordinator.h"

#include "ReciFocusStyleButtonBase.generated.h"

class UHorizontalBoxSlot;
class UInputAction;
class UReciMappedKeyText;
class UScaleBox;

/**
 * CommonButtonBase variant that renders keyboard or controller focus with
 * the Hovered state of its single Common Button Style.
 *
 * Backgrounds come from NormalHovered / SelectedHovered. Text comes from
 * NormalHoveredTextStyle / SelectedHoveredTextStyle and is applied to every
 * descendant Common Text Block automatically when the button style defines
 * a normal-state text style. Otherwise each text block keeps its own style.
 */
UCLASS(
	Abstract,
	BlueprintType,
	Blueprintable,
	ClassGroup = UI,
	meta = (
		DisplayName = "Reci Focus Style Button Base",
		Category = "Reci Common UI",
		DisableNativeTick))
class RECICOMMONUI_API UReciFocusStyleButtonBase : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	/** Moves user focus to this button when CommonUI reports it as hovered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus")
	bool bFocusOnHover = false;

	/** Rounded style outlines identify keyboard/gamepad focus, not pointer hover. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus")
	bool bReserveOutlineForFocus = true;

	/**
	 * Uses the pointer-hover state for visual styling. Disable this for
	 * selection-driven controls such as carousels, where the pointer can be
	 * over a different entry while a drag is settling.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Focus")
	bool bUsePointerHoverVisual = true;

	/** Enables or disables pointer-hover styling and refreshes immediately. */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Focus")
	void SetUsePointerHoverVisual(bool bInUsePointerHoverVisual);

	/**
	 * Shows the focus look (focus fill and outline) without holding Slate focus: a key cursor that a
	 * screen drives itself, such as the current option of a row whose value the screen's own keys change
	 * while focus is elsewhere.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Focus")
	void SetKeyCursorVisual(bool bShow);

	UFUNCTION(BlueprintPure, Category = "Reci Common UI|Focus")
	bool IsShowingKeyCursor() const { return bKeyCursorVisual; }

	/**
	 * Key hint: Reci extends CommonUI's choice for the button's Input Action Widget. In CommonUI's order the widget
	 * shows the Triggering Input Action (the Enhanced Input action, else the data table row), else the Triggered Input
	 * Action; then this Hint Input Action; then CommonUI's click action while the button holds focus (CommonUI shows it
	 * on pointer hover instead, where keys do not act), if Should Use Fallback Default Input Action is on; without focus
	 * its keys are left out, so the button keeps their reserved width as focus moves. A disabled
	 * button shows none. CommonUI's Hide Input Action and Hide Input Action With Keyboard apply as on any CommonUI
	 * button. Keys come from input actions, so they follow remapping; an action with no keys shows nothing, and touch
	 * hides every hint. The hint leaves out each key that a handler earlier in the routing order takes now: an open
	 * popup's Back, a capturing screen (every key), the actions of the screens on the focus path that are not suspended
	 * (Is Reci Screen Action Suspended), other than the button's own action, and, for a Triggering Input Action, the
	 * Accept and direction keys while a widget holds focus. A Hint Input Action that a screen on the focus path maps shows
	 * only while that screen gets its keys and does not suspend it. While the button's activatable widget is activated but
	 * outside CommonUI's active root (beneath an Is Modal dialog nested in its screen, say), CommonUI's bindings there are
	 * inactive and the screen gets no screen input, so the hint shows no key.
	 *
	 * The Hint Input Action only shows its keys: it binds nothing. Use it for a key the screen handles on the
	 * button's behalf, such as Back on a screen that is a back handler.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Hint")
	TObjectPtr<UInputAction> HintInputAction;

	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Key Hint")
	void SetHintInputAction(UInputAction* InHintInputAction);

	/**
	 * Shows the Triggering Input Action in CommonUI's Bound Action Bar while the button is active, named by the
	 * action (an Enhanced Input action's Action Description). CommonUI's own buttons leave it out of the bar.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Key Hint")
	bool bShowTriggeringActionInActionBar = true;

	/**
	 * Largest Display Scale. Above 1 the whole button (style background, outline and content) is laid out at its
	 * own size and scaled, so it grows as one piece and its text stays sharp: at scale S it takes S / Max of the
	 * width it is given, centred, and S times its height. 1 turns display scaling off.
	 *
	 * Above 1, Initialize inserts a Horizontal Box named DisplayScaleRow above CommonUI's internal root button (the way
	 * CommonButtonBase wraps the authored root in that button). It holds a Spacer, a Scale Box named DisplayScaleBox
	 * (Stretch: User Specified) that holds the button, and a Spacer. The Designer's Hierarchy does not show them; the
	 * Designer preview does. The value at initialisation decides; changing it later neither adds nor removes them.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display Scale", meta = (ClampMin = "1.0"))
	float MaxDisplayScale = 1.0f;

	/**
	 * Scales the whole button between 1 and Max Display Scale (clamped to them), for example a list's selected entry.
	 * The scale applies at once and never animates; a Blueprint that animates it reads Get Motion Scale For Widget
	 * and follows it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reci Common UI|Display Scale")
	void SetDisplayScale(float InScale);

	UFUNCTION(BlueprintPure, Category = "Reci Common UI|Display Scale")
	float GetDisplayScale() const { return DisplayScale; }

#if WITH_EDITOR
	/**
	 * Warns, at Blueprint compile time, when InputActionWidget is not a Reci Mapped Key Text (a stock Common Action Widget
	 * draws only CommonUI's icon for an action's first key), or is one with Hide When Unmapped off.
	 */
	virtual void ValidateCompiledWidgetTree(const UWidgetTree& BlueprintWidgetTree, class IWidgetCompilerLog& CompileLog) const override;
#endif

protected:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void BindTriggeringInputActionToClick() override;
	/**
	 * Drives CommonUI's Input Action Widget, the optional widget named InputActionWidget in the button's tree, by the
	 * Hint Input Action rule above. A Reci Mapped Key Text there shows the keys as names; it keeps Hide When Unmapped on
	 * (Reci turns it on, and warns once, where it is off), so Unmapped Text never shows. Every hidden state is "no action",
	 * and CommonUI's visibility flags then apply.
	 */
	virtual void UpdateInputActionWidget() override;
	virtual UCommonButtonInternalBase* ConstructInternalButton() override;
	virtual void SynchronizeProperties() override;
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;
	virtual void NativeOnEnabled() override;
	virtual void NativeOnDisabled() override;
	virtual void HandleFocusReceived() override;
	virtual void HandleFocusLost() override;
	virtual void NativeOnCurrentTextStyleChanged() override;

private:
	bool IsPointerHoverAllowedByOwningList() const;
	void CaptureBaseButtonStyle();
	void ApplyInteractionStyle();
	void ApplyCurrentTextStyleToDescendants();
	TSubclassOf<UCommonTextStyle> ResolveInteractionTextStyleClass() const;
	bool IsCurrentInternalStyleProjected() const;
	/** Real focus or the key cursor: both draw the focus look. */
	bool ShowsFocusVisual() const;
	/** Hover, focus and the key cursor deliberately resolve to the same Hovered style, while enabled. */
	bool UsesHoveredVisual() const;
	/** Shows the action, else the row, else no action, on the Input Action Widget; false when it already showed it. */
	bool ShowHintAction(UInputAction* Action, const FDataTableRowHandle& Row);
	/** The key text's filter: whether no handler earlier than the hint's own takes the key now. */
	bool DoesHintKeyReach(const FKey& Key) const;
	void HandleKeyRoutingChanged(int32 UserIndex);
	void ApplyDisplayScale();

	/**
	 * The state the style and key hint follow, kept from the button's own events: enabled, hovered (only while
	 * pointer hover may show) and holding focus.
	 */
	bool bStateEnabled = true;
	bool bStateHovered = false;
	bool bStateFocused = false;
	bool bKeyCursorVisual = false;
	/** Where the shown action's keys go, and the action, for the key text's filter. */
	EReciKeyHintStage HintStage = EReciKeyHintStage::Stock;
	TWeakObjectPtr<const UInputAction> HintOwnAction;
	/** False while the shown action is the click action of a button without focus: none of its keys reach it. */
	bool bHintKeysReach = true;
	/** The key text the filter is set on. */
	TWeakObjectPtr<UReciMappedKeyText> FilteredKeyText;
	FDelegateHandle KeyRoutingListener;
	FButtonStyle BaseButtonStyle;
	FButtonStyle InteractionButtonStyle;
	TWeakObjectPtr<UCommonButtonInternalBase> InternalButton;
	bool bHasBaseButtonStyle = false;
	bool bInteractionButtonStyleApplied = false;

	UPROPERTY(Transient)
	TObjectPtr<UScaleBox> DisplayScaleBox;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBoxSlot> DisplayLeadingSlot;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBoxSlot> DisplayScaleSlot;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBoxSlot> DisplayTrailingSlot;

	float DisplayScale = 1.0f;
};
