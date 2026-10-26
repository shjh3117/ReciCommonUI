// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Interaction/ReciInputFocusCoordinator.h"
#include "Types/SlateEnums.h"

class UCommonActivatableWidget;
class UInputAction;

namespace UE::ReciCommonUI::Private
{
	/**
	 * What decides, for one user, which handler gets a key first (FReciInputFocusProcessor's key-down order), read at one
	 * moment so that any key can be asked about without a player.
	 */
	struct FReciKeyRoutingSnapshot
	{
		struct FAction
		{
			const UInputAction* Action = nullptr;
			/** IsReciScreenActionSuspended's answer: Reci offers the action no key. */
			bool bSuspended = false;
			/** The keys the player maps to the action. */
			TArray<FKey> Keys;
		};

		struct FScreen
		{
			TWeakObjectPtr<const UCommonActivatableWidget> Screen;
			/** IsCapturingReciScreenKeys's answer: the screen takes every key first. */
			bool bCapturing = false;
			/** The actions of the screen's Input Mapping, in mapping order. */
			TArray<FAction> Actions;
		};

		/** The registered screens on the focus path that take screen input when no popup is open, innermost first. */
		TArray<FScreen> Screens;
		/** CommonUI's input mode is Menu or All, so Reci routes keys at all. */
		bool bMenuInput = true;
		/** One of the user's Reci popups is open: Back closes it, and no screen gets screen input. */
		bool bPopupOpen = false;
		/** A widget other than the game viewport holds the user's focus: Slate gives it the key before CommonUI's bindings. */
		bool bWidgetFocused = false;
		/** A text field holds focus: it takes keyboard keys as typing, and Reci routes no key. */
		bool bTextEditing = false;
		/** A key selector that is selecting holds focus: it takes every key, and Reci routes none. */
		bool bSelectingKey = false;
	};

	/** One key a hint asks about, read through the user's navigation config. */
	struct FReciKeyRoutingQuery
	{
		FKey Key;
		/** What the navigation config makes of the key: Back, Accept or neither. */
		EUINavigationAction NavigationAction = EUINavigationAction::Invalid;
		/** Whether the navigation config makes the key a direction. */
		bool bNavigationDirection = false;
		EReciKeyHintStage Stage = EReciKeyHintStage::Stock;
		/** The action the hint shows; a screen action that is the same action does not count as earlier. */
		const UInputAction* OwnAction = nullptr;
	};

	/**
	 * Whether a handler earlier than the query's stage takes the key now: the order of FReciInputFocusProcessor's
	 * key-down, then Slate's focused widget ahead of CommonUI's bindings. A screen action that is not suspended counts as
	 * taking its keys, and a capturing screen as taking every key. At the ScreenAction stage, a key of an action of a
	 * screen counts as taken when the screen does not get it (a popup, raw input or a Game input mode) or suspends it.
	 */
	bool IsKeyTakenBefore(const FReciKeyRoutingSnapshot& Snapshot, const FReciKeyRoutingQuery& Query);
}
