// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/Interface.h"
#include "ReciScreenInput.generated.h"

class UCommonActivatableWidget;
class UInputAction;
class UWidget;

/**
 * Optional screen-level routing, before ordinary focused-widget navigation.
 *
 * A screen receives its keys as input actions: the actions of its Input Mapping (the CommonActivatableWidget mapping
 * context, applied while the screen is active) whose keys were pressed, so its keys follow the player's bindings and
 * its key hints can show the same actions. Triggers and modifiers do not apply: an action arrives on every key-down
 * and repeat of its keys, and registration warns once for each mapping or action of the screen's Input Mapping that
 * has them (not for those another applied mapping context adds). A stick reaches its actions as the platform's
 * stick-direction keys (Gamepad Left Stick Up and so on), pressed past the device's dead zone and repeated as the
 * platform repeats a held button. While a press of a stick direction one of its actions took is held, the stick moves
 * no focus; a direction no action takes goes on to focused navigation and CommonUI, as any key does.
 *
 * A screen implements this interface and calls Register Screen Input when it activates. A registered screen receives
 * its keys while it is activated, visible and enabled and focus is on it or inside it, in CommonUI's active root, while
 * CommonUI's active input mode is Menu or All and none of its user's Reci popups is open. A screen that implements the
 * interface but has not registered by the tick after it activates is reported once per class (development builds).
 * Registered screens nested in one another are offered each key in turn, innermost first (a screen's key capture,
 * then its actions, then the next screen out), and the first Is Modal activatable widget outward from focus ends that
 * chain: to silence a screen while a dialog nested in it is open, make the dialog Is Modal. An action the screen
 * suspends (Is Reci Screen Action Suspended) is offered no key.
 */
UINTERFACE(BlueprintType)
class RECICOMMONUI_API UReciScreenInput : public UInterface
{
	GENERATED_BODY()
};

class RECICOMMONUI_API IReciScreenInput
{
	GENERATED_BODY()
public:
	/**
	 * One of the screen's input actions was pressed, or repeats while held. A screen receives the repeats only of a press
	 * one of its actions took, and none of an Accept or Back. The action's triggers and modifiers do not apply: it
	 * arrives on every key-down and repeat (check bIsRepeat). TimestampSeconds is when the key went down, on the
	 * FPlatformTime::Seconds clock: the answer of the event-time resolver if one is set (SetEventTimeResolver on
	 * FReciInputFocusCoordinator, asked once per key-down or repeat however many actions are offered it), else when
	 * Slate received the key. Return true for the actions the screen handled. Key hints count an action that is not
	 * suspended (Is Reci Screen Action Suspended) as taking its keys, so return true for every such action, except one
	 * that is also a button's Triggering Input Action, which is left to the button.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="Reci Common UI|Screen Input")
	bool HandleReciScreenAction(UInputAction* Action, bool bIsRepeat, double TimestampSeconds);

	/**
	 * Whether the screen declines one of its actions now, for a decision that depends on the screen's state, such as
	 * where focus is. Reci then offers the action no key (Handle Reci Screen Action is not called for it, and its keys
	 * go on to focused navigation and CommonUI), and key hints do not count its keys as taken. Unimplemented, it
	 * returns false: every action takes its keys. Reci asks it at every key-down and, while key hints are shown and the
	 * screen holds focus, every frame, so keep it cheap.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="Reci Common UI|Screen Input")
	bool IsReciScreenActionSuspended(const UInputAction* Action) const;

	/**
	 * Every key-down, before the screen's actions, for a screen that captures any key (a key-binding editor), and the
	 * repeats of a key it took. Return true to consume the key, and only while Is Capturing Reci Screen Keys returns
	 * true: neither the screen's actions nor focused navigation then see it or its repeats. Key hints treat a capturing
	 * screen as taking every key, and any other screen as taking none here.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="Reci Common UI|Screen Input")
	bool HandleReciScreenKey(FKey Key, bool bIsRepeat);

	/**
	 * True while the screen captures every key in HandleReciScreenKey. Its player's sticks then move no focus; their
	 * direction keys still reach HandleReciScreenKey.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="Reci Common UI|Screen Input")
	bool IsCapturingReciScreenKeys() const;
};

/**
 * Whether a screen that takes directional keys keeps every control and action reachable (DESIGN.md principle 13): on
 * such a screen, each control and each of the screen's actions has its own key on keyboard and on gamepad, with a
 * visible hint.
 */
USTRUCT(BlueprintType)
struct RECICOMMONUI_API FReciScreenReachability
{
	GENERATED_BODY()

	/**
	 * Whether the screen's Input Mapping maps a key that navigates in a direction (by the player's navigation config) or
	 * a stick-direction key. When false, the screen is not checked and the lists below are empty.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Reci Common UI|Screen Input")
	bool bTakesDirectionalKeys = false;

	/**
	 * CommonUI buttons of the screen, outside its Reci lists and the activatable widgets nested in it, without their own
	 * key on keyboard and gamepad (a Triggering Input Action, or a Reci button's Hint Input Action) or without a widget
	 * that shows it (the button's Input Action Widget, a widget named InputActionWidget).
	 */
	UPROPERTY(BlueprintReadOnly, Category="Reci Common UI|Screen Input")
	TArray<TObjectPtr<UWidget>> ControlsWithoutKey;

	/** Actions of the screen's Input Mapping without a keyboard key or without a gamepad key. */
	UPROPERTY(BlueprintReadOnly, Category="Reci Common UI|Screen Input")
	TArray<TObjectPtr<UInputAction>> ActionsMissingDevice;

	/**
	 * Actions of the screen's Input Mapping that no Reci Mapped Key Text of the screen shows, and that are not the
	 * Triggering or Hint Input Action of a Reci button with an Input Action Widget.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Reci Common UI|Screen Input")
	TArray<TObjectPtr<UInputAction>> ActionsWithoutHint;
};

UCLASS()
class RECICOMMONUI_API UReciScreenInputLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/**
	 * Registers a screen that implements Reci Screen Input, when it activates. A null screen, or a widget without the
	 * interface, is ignored with a warning logged once. Registration also reports once per screen class what Reci cannot
	 * serve: triggers and modifiers in its Input Mapping, and, on a screen that takes directional keys, what Check Screen
	 * Reachability finds.
	 */
	UFUNCTION(BlueprintCallable, Category="Reci Common UI|Screen Input")
	static void RegisterScreenInput(UCommonActivatableWidget* Screen);
	UFUNCTION(BlueprintCallable, Category="Reci Common UI|Screen Input")
	static void UnregisterScreenInput(UCommonActivatableWidget* Screen);

	/**
	 * Checks that a screen that takes directional keys keeps every control and action reachable by its own hinted key on
	 * keyboard and gamepad. Keys are the player's current bindings plus the keys the screen's Input Mapping authors. It
	 * cannot see which screen action reaches a list's entries or the controls nested in them, so that stays a manual
	 * check. Register Screen Input runs it once per screen class and logs what it finds.
	 */
	UFUNCTION(BlueprintCallable, Category="Reci Common UI|Screen Input")
	static FReciScreenReachability CheckScreenReachability(const UCommonActivatableWidget* Screen);
};
