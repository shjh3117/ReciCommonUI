// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#if WITH_DEV_AUTOMATION_TESTS

#include "InputAction.h"
#include "Interaction/ReciKeyRouting.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

using UE::ReciCommonUI::Private::FReciKeyRoutingQuery;
using UE::ReciCommonUI::Private::FReciKeyRoutingSnapshot;
using UE::ReciCommonUI::Private::IsKeyTakenBefore;

namespace
{
	constexpr EReciKeyHintStage AllStages[] = {
		EReciKeyHintStage::ScreenAction, EReciKeyHintStage::FocusedControl, EReciKeyHintStage::Stock};

	const TCHAR* DescribeStage(const EReciKeyHintStage Stage)
	{
		switch (Stage)
		{
		case EReciKeyHintStage::ScreenAction:
			return TEXT("ScreenAction");
		case EReciKeyHintStage::FocusedControl:
			return TEXT("FocusedControl");
		default:
			return TEXT("Stock");
		}
	}

	FReciKeyRoutingQuery Ask(
		const FKey& Key,
		const EReciKeyHintStage Stage,
		const UInputAction* OwnAction = nullptr,
		const EUINavigationAction NavigationAction = EUINavigationAction::Invalid,
		const bool bNavigationDirection = false)
	{
		FReciKeyRoutingQuery Query;
		Query.Key = Key;
		Query.Stage = Stage;
		Query.OwnAction = OwnAction;
		Query.NavigationAction = NavigationAction;
		Query.bNavigationDirection = bNavigationDirection;
		return Query;
	}

	FReciKeyRoutingSnapshot::FAction MapAction(const UInputAction* Action, TArray<FKey> Keys, const bool bSuspended = false)
	{
		FReciKeyRoutingSnapshot::FAction Entry;
		Entry.Action = Action;
		Entry.Keys = MoveTemp(Keys);
		Entry.bSuspended = bSuspended;
		return Entry;
	}

	FReciKeyRoutingSnapshot::FScreen MakeScreen(TArray<FReciKeyRoutingSnapshot::FAction> Actions, const bool bCapturing = false)
	{
		FReciKeyRoutingSnapshot::FScreen Screen;
		Screen.Actions = MoveTemp(Actions);
		Screen.bCapturing = bCapturing;
		return Screen;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciKeyHintRoutingStagesTest, "ReciCommonUI.KeyHint.RoutingStages",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FReciKeyHintRoutingStagesTest::RunTest(const FString& Parameters)
{
	// Transient actions that nothing else sees; the evaluator reads only the snapshot.
	const TStrongObjectPtr<UInputAction> First(NewObject<UInputAction>(GetTransientPackage()));
	const TStrongObjectPtr<UInputAction> Second(NewObject<UInputAction>(GetTransientPackage()));
	const TStrongObjectPtr<UInputAction> Third(NewObject<UInputAction>(GetTransientPackage()));
	const TStrongObjectPtr<UInputAction> Inner(NewObject<UInputAction>(GetTransientPackage()));

	const FReciKeyRoutingSnapshot Nothing;
	for (const EReciKeyHintStage Stage : AllStages)
	{
		TestFalse(FString::Printf(TEXT("With nothing in the way, a key reaches %s"), DescribeStage(Stage)),
			IsKeyTakenBefore(Nothing, Ask(EKeys::J, Stage)));
	}

	// An open popup takes Back at every stage, and its screens get no screen input meanwhile.
	FReciKeyRoutingSnapshot Popup;
	Popup.bPopupOpen = true;
	Popup.Screens = {MakeScreen({MapAction(First.Get(), {EKeys::J})})};
	for (const EReciKeyHintStage Stage : AllStages)
	{
		TestTrue(FString::Printf(TEXT("An open popup takes Back before %s"), DescribeStage(Stage)),
			IsKeyTakenBefore(Popup, Ask(EKeys::Escape, Stage, nullptr, EUINavigationAction::Back)));
		TestFalse(FString::Printf(TEXT("An open popup takes no other key before %s"), DescribeStage(Stage)),
			IsKeyTakenBefore(Popup, Ask(EKeys::J, Stage)));
	}
	TestTrue(TEXT("While a popup is open, a screen's action gets none of its keys"),
		IsKeyTakenBefore(Popup, Ask(EKeys::J, EReciKeyHintStage::ScreenAction, First.Get())));

	// A capturing screen takes every key.
	FReciKeyRoutingSnapshot Capture;
	Capture.Screens = {MakeScreen({MapAction(First.Get(), {EKeys::J})}, true)};
	for (const EReciKeyHintStage Stage : AllStages)
	{
		TestTrue(FString::Printf(TEXT("A capturing screen takes any key before %s"), DescribeStage(Stage)),
			IsKeyTakenBefore(Capture, Ask(EKeys::L, Stage)));
	}
	TestTrue(TEXT("A capturing screen takes its own action's key"),
		IsKeyTakenBefore(Capture, Ask(EKeys::J, EReciKeyHintStage::ScreenAction, First.Get())));

	// A screen's actions take their keys, unless suspended, and not from the hint's own action.
	FReciKeyRoutingSnapshot Actions;
	Actions.Screens = {MakeScreen({
		MapAction(First.Get(), {EKeys::J}), MapAction(Second.Get(), {EKeys::J}), MapAction(Third.Get(), {EKeys::K})})};
	TestTrue(TEXT("A screen action takes its key before the focused control"),
		IsKeyTakenBefore(Actions, Ask(EKeys::K, EReciKeyHintStage::FocusedControl)));
	TestTrue(TEXT("A screen action takes its key before CommonUI's bindings"),
		IsKeyTakenBefore(Actions, Ask(EKeys::K, EReciKeyHintStage::Stock)));
	TestFalse(TEXT("A screen action that is the hint's own action is not earlier"),
		IsKeyTakenBefore(Actions, Ask(EKeys::K, EReciKeyHintStage::Stock, Third.Get())));
	TestFalse(TEXT("A key no screen action maps is not taken"), IsKeyTakenBefore(Actions, Ask(EKeys::L, EReciKeyHintStage::Stock)));
	FReciKeyRoutingSnapshot Suspended = Actions;
	Suspended.Screens[0].Actions[2].bSuspended = true;
	TestFalse(TEXT("A suspended action does not take its key"), IsKeyTakenBefore(Suspended, Ask(EKeys::K, EReciKeyHintStage::Stock)));

	// At ScreenAction, only the actions before the hint's own, in mapping order, are earlier.
	TestTrue(TEXT("An earlier action on the same key takes it"),
		IsKeyTakenBefore(Actions, Ask(EKeys::J, EReciKeyHintStage::ScreenAction, Second.Get())));
	TestFalse(TEXT("A later action on the same key does not"),
		IsKeyTakenBefore(Actions, Ask(EKeys::J, EReciKeyHintStage::ScreenAction, First.Get())));
	TestFalse(TEXT("The hint's own action gets its key"),
		IsKeyTakenBefore(Actions, Ask(EKeys::K, EReciKeyHintStage::ScreenAction, Third.Get())));
	FReciKeyRoutingSnapshot FirstSuspended = Actions;
	FirstSuspended.Screens[0].Actions[0].bSuspended = true;
	TestFalse(TEXT("A suspended earlier action does not take the key"),
		IsKeyTakenBefore(FirstSuspended, Ask(EKeys::J, EReciKeyHintStage::ScreenAction, Second.Get())));
	TestTrue(TEXT("A suspended own action gets none of its keys"),
		IsKeyTakenBefore(FirstSuspended, Ask(EKeys::J, EReciKeyHintStage::ScreenAction, First.Get())));

	// Nested screens: the inner screen's actions come before the outer screen's.
	FReciKeyRoutingSnapshot Nested;
	Nested.Screens = {MakeScreen({MapAction(Inner.Get(), {EKeys::J})}), MakeScreen({MapAction(First.Get(), {EKeys::J})})};
	TestTrue(TEXT("An inner screen's action takes a key before the outer screen's"),
		IsKeyTakenBefore(Nested, Ask(EKeys::J, EReciKeyHintStage::ScreenAction, First.Get())));
	TestFalse(TEXT("The inner screen's own action gets the key"),
		IsKeyTakenBefore(Nested, Ask(EKeys::J, EReciKeyHintStage::ScreenAction, Inner.Get())));

	// Accept and the directions: taken by the focused widget, only on the stock path and only while a widget has focus.
	FReciKeyRoutingSnapshot Focused;
	Focused.bWidgetFocused = true;
	TestTrue(TEXT("A focused widget takes Accept before CommonUI's bindings"),
		IsKeyTakenBefore(Focused, Ask(EKeys::Enter, EReciKeyHintStage::Stock, nullptr, EUINavigationAction::Accept)));
	TestTrue(TEXT("A focused widget takes a direction before CommonUI's bindings"),
		IsKeyTakenBefore(Focused, Ask(EKeys::Down, EReciKeyHintStage::Stock, nullptr, EUINavigationAction::Invalid, true)));
	TestFalse(TEXT("Not before the focused control"),
		IsKeyTakenBefore(Focused, Ask(EKeys::Enter, EReciKeyHintStage::FocusedControl, nullptr, EUINavigationAction::Accept)));
	TestTrue(TEXT("A Hint Input Action no screen maps goes where CommonUI's bindings go"),
		IsKeyTakenBefore(Focused, Ask(EKeys::Enter, EReciKeyHintStage::ScreenAction, First.Get(), EUINavigationAction::Accept)));
	FReciKeyRoutingSnapshot FocusedScreen = Focused;
	FocusedScreen.Screens = {MakeScreen({MapAction(First.Get(), {EKeys::Enter})})};
	TestFalse(TEXT("A screen's action gets Accept before the focused widget"),
		IsKeyTakenBefore(FocusedScreen, Ask(EKeys::Enter, EReciKeyHintStage::ScreenAction, First.Get(), EUINavigationAction::Accept)));
	TestFalse(TEXT("Without a focused widget, Accept reaches CommonUI's bindings"),
		IsKeyTakenBefore(Nothing, Ask(EKeys::Enter, EReciKeyHintStage::Stock, nullptr, EUINavigationAction::Accept)));
	TestFalse(TEXT("Without a focused widget, a direction reaches CommonUI's bindings"),
		IsKeyTakenBefore(Nothing, Ask(EKeys::Down, EReciKeyHintStage::Stock, nullptr, EUINavigationAction::Invalid, true)));

	// Raw input comes before Reci: a text field takes keyboard keys, a selecting key selector every key.
	FReciKeyRoutingSnapshot Typing;
	Typing.bTextEditing = true;
	Typing.Screens = {MakeScreen({MapAction(First.Get(), {EKeys::Gamepad_FaceButton_Top})})};
	TestTrue(TEXT("A text field takes keyboard keys"), IsKeyTakenBefore(Typing, Ask(EKeys::J, EReciKeyHintStage::FocusedControl)));
	TestFalse(TEXT("A text field leaves gamepad keys to CommonUI's bindings"),
		IsKeyTakenBefore(Typing, Ask(EKeys::Gamepad_FaceButton_Top, EReciKeyHintStage::Stock)));
	TestTrue(TEXT("While a text field has focus, a screen's action gets none of its keys"),
		IsKeyTakenBefore(Typing, Ask(EKeys::Gamepad_FaceButton_Top, EReciKeyHintStage::ScreenAction, First.Get())));
	FReciKeyRoutingSnapshot Selecting;
	Selecting.bSelectingKey = true;
	TestTrue(TEXT("A selecting key selector takes every key"),
		IsKeyTakenBefore(Selecting, Ask(EKeys::Gamepad_FaceButton_Bottom, EReciKeyHintStage::FocusedControl)));

	// Under a Game input mode Reci routes nothing: no popup Back, no screen input.
	FReciKeyRoutingSnapshot Game = Capture;
	Game.bMenuInput = false;
	Game.bPopupOpen = true;
	TestFalse(TEXT("Under a Game input mode a popup takes no Back"),
		IsKeyTakenBefore(Game, Ask(EKeys::Escape, EReciKeyHintStage::Stock, nullptr, EUINavigationAction::Back)));
	TestFalse(TEXT("Under a Game input mode a capturing screen takes no key"), IsKeyTakenBefore(Game, Ask(EKeys::L, EReciKeyHintStage::Stock)));
	return true;
}

#endif
