// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Interaction/ReciKeyRouting.h"

namespace UE::ReciCommonUI::Private
{
	namespace
	{
		/** A key a text field takes as typing. */
		bool IsTypingKey(const FKey& Key)
		{
			return !Key.IsGamepadKey() && !Key.IsMouseButton() && !Key.IsTouch();
		}

		bool IsActionOfScreens(const FReciKeyRoutingSnapshot& Snapshot, const UInputAction* Action)
		{
			return Action && Snapshot.Screens.ContainsByPredicate([Action](const FReciKeyRoutingSnapshot::FScreen& Screen)
			{
				return Screen.Actions.ContainsByPredicate([Action](const FReciKeyRoutingSnapshot::FAction& Entry)
				{
					return Entry.Action == Action;
				});
			});
		}
	}

	bool IsKeyTakenBefore(const FReciKeyRoutingSnapshot& Snapshot, const FReciKeyRoutingQuery& Query)
	{
		// A key selector that is selecting, and a text field, get keys before Reci routes any.
		if (Snapshot.bSelectingKey || (Snapshot.bTextEditing && IsTypingKey(Query.Key)))
		{
			return true;
		}
		const bool bReciRoutes = Snapshot.bMenuInput && !Snapshot.bTextEditing;
		// An open popup takes Back first, and no screen gets screen input while it is open.
		if (bReciRoutes && Snapshot.bPopupOpen && Query.NavigationAction == EUINavigationAction::Back)
		{
			return true;
		}
		if (Query.Stage == EReciKeyHintStage::ScreenKey)
		{
			// The capture gets the key unless a screen inside it maps the key to an action that takes it first.
			if (bReciRoutes && !Snapshot.bPopupOpen)
			{
				for (const FReciKeyRoutingSnapshot::FScreen& Screen : Snapshot.Screens)
				{
					if (Screen.bCapturing)
					{
						return false;
					}
					if (Screen.Actions.ContainsByPredicate([&Query](const FReciKeyRoutingSnapshot::FAction& Entry)
						{
							return !Entry.bSuspended && Entry.Keys.Contains(Query.Key);
						}))
					{
						return true;
					}
				}
			}
			return true;
		}
		if (bReciRoutes && !Snapshot.bPopupOpen)
		{
			// Innermost first: each screen's key capture, then its actions in mapping order, then the next screen out.
			for (const FReciKeyRoutingSnapshot::FScreen& Screen : Snapshot.Screens)
			{
				if (Screen.bCapturing)
				{
					return true;
				}
				const int32 OwnIndex = Query.Stage == EReciKeyHintStage::ScreenAction && Query.OwnAction
					? Screen.Actions.IndexOfByPredicate([&Query](const FReciKeyRoutingSnapshot::FAction& Entry)
						{
							return Entry.Action == Query.OwnAction;
						})
					: INDEX_NONE;
				for (int32 Index = 0; Index < Screen.Actions.Num() && (OwnIndex == INDEX_NONE || Index < OwnIndex); ++Index)
				{
					const FReciKeyRoutingSnapshot::FAction& Entry = Screen.Actions[Index];
					// The hint's own action is not earlier than itself: a button's key that its screen also maps is left
					// to the button (DESIGN.md, A button's own key).
					if (Entry.Action != Query.OwnAction && !Entry.bSuspended && Entry.Keys.Contains(Query.Key))
					{
						return true;
					}
				}
				if (OwnIndex != INDEX_NONE)
				{
					// The key reaches the hint's action, unless the screen suspends it now.
					return Screen.Actions[OwnIndex].bSuspended;
				}
			}
		}
		else if (Query.Stage == EReciKeyHintStage::ScreenAction && IsActionOfScreens(Snapshot, Query.OwnAction))
		{
			// The screen's action gets no key meanwhile.
			return true;
		}
		if (Query.Stage == EReciKeyHintStage::FocusedControl)
		{
			return false;
		}
		// The stock path: Slate gives the focused widget the key first, and it takes Accept (a button clicks) and the
		// directions (navigation) before CommonUI's bindings see them.
		return Snapshot.bWidgetFocused
			&& (Query.NavigationAction == EUINavigationAction::Accept || Query.bNavigationDirection);
	}
}
