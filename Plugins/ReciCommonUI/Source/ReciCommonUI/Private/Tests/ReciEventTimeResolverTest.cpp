// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/ReciTestHarness.h"

#include "Blueprint/WidgetTree.h"
#include "Components/EditableText.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformProcess.h"
#include "InputAction.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

// Both tests replace the event-time resolver. The harness sets the game's resolver aside and puts it back when it ends.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciEventTimeResolverNothingAttachedTest, "ReciCommonUI.Input.EventTimeResolver.NothingAttached",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciEventTimeResolverNothingAttachedTest::RunTest(const FString& Parameters)
{
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	FReciInputFocusCoordinator::Get().SetEventTimeResolver(nullptr);
	UInputAction* Tap = Harness.CreateAction(TEXT("IA_Tap"));
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	Screen->SetInputMapping(Harness.CreateMapping({{Tap, EKeys::J}}));
	Screen->HandledActions.Add(Tap);
	Harness.ActivateScreen(*Screen);
	FReciInputFocusCoordinator::Get().RegisterScreenInput(Screen);
	Harness.Paint();
	TestTrue(TEXT("Focus reaches the screen"), Harness.Focus(*Screen));

	// Slate stamps a key event when it receives the key. Reci routes this one well after that, and the action must get
	// the stamp, not the time Reci routed the key.
	constexpr double RoutingDelaySeconds = 0.05;
	constexpr double ToleranceSeconds = 0.005;
	const double BeforePress = FPlatformTime::Seconds();
	const FKeyEvent Press(EKeys::J, FModifierKeysState(), FReciTestHarness::UserIndex, false, 0, 0);
	const double AfterPress = FPlatformTime::Seconds();
	while (FPlatformTime::Seconds() < AfterPress + RoutingDelaySeconds)
	{
		FPlatformProcess::Sleep(0.001f);
	}
	TestTrue(TEXT("The action takes its key with nothing attached"), FReciInputFocusCoordinatorTestAccess::KeyDown(Press));
	Harness.KeyUp(EKeys::J);
	TestEqual(TEXT("The screen sees the key, then its action"), Harness.GetLog().ToString(),
		FString(TEXT("Screen Key J, Screen Action IA_Tap")));
	if (TestEqual(TEXT("The action is timed once"), Screen->ActionTimestamps.Num(), 1))
	{
		const double Time = Screen->ActionTimestamps[0];
		TestTrue(FString::Printf(TEXT("The action's time (%.4f) is when Slate received the key (%.4f to %.4f), not when Reci routed it"),
				Time, BeforePress, AfterPress),
			Time >= BeforePress - ToleranceSeconds && Time <= AfterPress + ToleranceSeconds);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciEventTimeResolverFakeAttachedTest, "ReciCommonUI.Input.EventTimeResolver.FakeAttached",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	/** A fake resolver: it answers 1000 plus the number of calls so far, and logs each call with the screens' and controls'. */
	struct FReciFakeResolver
	{
		int32 Calls = 0;
		TSet<int32> Users;

		void Attach(FReciTestCallLog& Log)
		{
			FReciInputFocusCoordinator::Get().SetEventTimeResolver(
				[this, &Log](const FInputEvent& Event, const FKey& Key, const bool bIsRepeat)
				{
					++Calls;
					Users.Add(Event.GetUserIndex());
					Log.Calls.Add(TEXT("Resolver ") + Key.ToString() + (bIsRepeat ? TEXT(" repeat") : TEXT("")));
					return 1000.0 + Calls;
				});
		}
	};
}

// The call matrix SetEventTimeResolver documents: which key-downs and repeats call the resolver, once and before the
// first action, and which events Reci consumes or passes without calling it.
bool FReciEventTimeResolverFakeAttachedTest::RunTest(const FString& Parameters)
{
	{
		// Declared before the harness, which puts the game's resolver back when it ends.
		FReciFakeResolver Resolver;
		FReciTestHarness Harness(*this);
		if (!Harness.IsValid())
		{
			return false;
		}
		FReciTestCallLog& Log = Harness.GetLog();
		FReciTestRoutingEnvironment& Environment = Harness.GetEnvironment();
		Resolver.Attach(Log);
		// Two actions on J, the first declining it; an action on K, which the screen's key capture takes first; an action
		// on Down that declines it, so it goes on to the focused list; actions that take Left, Enter, Escape and the left
		// stick's Up; and nothing on L, Up or the gamepad's Accept button. The list takes Up, Down, Accept and the wheel, and
		// holds a text field.
		// A popup owner, and a Reci control of its own layer that no screen holds.
		UInputAction* Tap = Harness.CreateAction(TEXT("IA_Tap"));
		UInputAction* Hold = Harness.CreateAction(TEXT("IA_Hold"));
		UInputAction* Captured = Harness.CreateAction(TEXT("IA_Captured"));
		UInputAction* Browse = Harness.CreateAction(TEXT("IA_Browse"));
		UInputAction* Step = Harness.CreateAction(TEXT("IA_Step"));
		UInputAction* Confirm = Harness.CreateAction(TEXT("IA_Confirm"));
		UInputAction* Leave = Harness.CreateAction(TEXT("IA_Leave"));
		UInputAction* Stick = Harness.CreateAction(TEXT("IA_Stick"));
		UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
		Screen->SetInputMapping(Harness.CreateMapping({{Tap, EKeys::J}, {Hold, EKeys::J}, {Captured, EKeys::K}, {Browse, EKeys::Down},
			{Step, EKeys::Left}, {Confirm, EKeys::Enter}, {Leave, EKeys::Escape}, {Stick, EKeys::Gamepad_LeftStick_Up}}));
		Screen->HandledActions = {Hold, Captured, Step, Confirm, Leave, Stick};
		Screen->HandledKeys.Add(EKeys::K);
		UReciTestFocusTarget* List = Harness.CreateTarget(TEXT("List"), Screen);
		List->HandledDirections = {EUINavigation::Down, EUINavigation::Up};
		List->bHandlesAccept = true;
		List->bHandlesPointer = true;
		UEditableText* Field = List->WidgetTree->ConstructWidget<UEditableText>(UEditableText::StaticClass(), TEXT("Field"));
		List->SetContent(Field);
		UReciTestPopupOwner* Dropdown = Harness.CreatePopupOwner(TEXT("Dropdown"), Screen);
		UReciTestFocusTarget* Loose = Harness.CreateTarget(TEXT("Loose"), nullptr);
		Harness.ActivateScreen(*Screen);
		FReciInputFocusCoordinator::Get().RegisterScreenInput(Screen);
		Harness.Paint();
		TestTrue(TEXT("Focus reaches the list"), Harness.Focus(*List));

		// (1) Once for each key-down of a key the screen's actions map, before the first action; every action gets that
		// time. A swallowed release is not timed.
		TestTrue(TEXT("The second action takes J"), Harness.KeyDown(EKeys::J));
		TestTrue(TEXT("Its release is swallowed"), Harness.KeyUp(EKeys::J));
		TestTrue(TEXT("The second action takes J again"), Harness.KeyDown(EKeys::J));
		Harness.KeyUp(EKeys::J);
		TestEqual(TEXT("The resolver runs once for each press, before the actions"), Log.ToString(), FString(TEXT(
			"Screen Key J, Resolver J, Screen Action IA_Tap, Screen Action IA_Hold, "
			"Screen Key J, Resolver J, Screen Action IA_Tap, Screen Action IA_Hold")));
		TestEqual(TEXT("Every action of a press gets that press's time"), Screen->ActionTimestamps, TArray<double>{1001.0, 1001.0, 1002.0, 1002.0});
		Log.Reset();
		Screen->ActionTimestamps.Reset();

		// Also when no action takes the key and it goes on to the focused control.
		TestTrue(TEXT("The focused list takes Down after its action declines it"), Harness.KeyDown(EKeys::Down));
		Harness.KeyUp(EKeys::Down);
		TestEqual(TEXT("The resolver runs for a mapped key no action takes"), Log.ToString(),
			FString(TEXT("Screen Key Down, Resolver Down, Screen Action IA_Browse, List Navigate Down, List ReturnFocus")));
		TestEqual(TEXT("The declining action gets the resolver's time"), Screen->ActionTimestamps, TArray<double>{1003.0});
		Log.Reset();

		// Never for a key the screen's key capture takes, or a key no action maps.
		TestTrue(TEXT("The screen's key capture takes K"), Harness.KeyDown(EKeys::K));
		Harness.KeyUp(EKeys::K);
		TestFalse(TEXT("A key no action maps is not consumed"), Harness.KeyDown(EKeys::L));
		Harness.KeyUp(EKeys::L);
		TestEqual(TEXT("The resolver does not run for a captured or unmapped key"), Log.ToString(), FString(TEXT("Screen Key K, Screen Key L")));
		TestEqual(TEXT("The resolver ran once per resolved press"), Resolver.Calls, 3);
		Log.Reset();
		Screen->ActionTimestamps.Reset();

		// (2) Once for each repeat of a press a screen action took, with the repeat's own time.
		TestTrue(TEXT("The action takes Left"), Harness.KeyDown(EKeys::Left));
		TestTrue(TEXT("The action takes Left's repeat"), Harness.KeyDown(EKeys::Left, true));
		TestTrue(TEXT("And the next"), Harness.KeyDown(EKeys::Left, true));
		TestTrue(TEXT("Left's release is swallowed"), Harness.KeyUp(EKeys::Left));
		TestEqual(TEXT("The resolver runs once for each repeat of the action's press"), Log.ToString(), FString(TEXT(
			"Screen Key Left, Resolver Left, Screen Action IA_Step, Resolver Left repeat, Screen Action IA_Step repeat, "
			"Resolver Left repeat, Screen Action IA_Step repeat")));
		TestEqual(TEXT("Each repeat gets its own time"), Screen->ActionTimestamps, TArray<double>{1004.0, 1005.0, 1006.0});
		Log.Reset();

		// Never for the repeats of a press the focused control or the key capture took, nor for a held Accept's.
		TestTrue(TEXT("The focused list takes Down"), Harness.KeyDown(EKeys::Down));
		TestTrue(TEXT("The list takes Down's repeat"), Harness.KeyDown(EKeys::Down, true));
		Harness.KeyUp(EKeys::Down);
		TestTrue(TEXT("The key capture takes K"), Harness.KeyDown(EKeys::K));
		TestTrue(TEXT("The key capture takes K's repeat"), Harness.KeyDown(EKeys::K, true));
		Harness.KeyUp(EKeys::K);
		TestTrue(TEXT("The action takes Enter"), Harness.KeyDown(EKeys::Enter));
		TestTrue(TEXT("Enter's repeat is blocked"), Harness.KeyDown(EKeys::Enter, true));
		Harness.KeyUp(EKeys::Enter);
		TestEqual(TEXT("The resolver does not run for these repeats"), Log.ToString(), FString(TEXT(
			"Screen Key Down, Resolver Down, Screen Action IA_Browse, List Navigate Down, List ReturnFocus, "
			"List Navigate Down, List ReturnFocus, "
			"Screen Key K, Screen Key K repeat, "
			"Screen Key Enter, Resolver Enter, Screen Action IA_Confirm")));
		Log.Reset();

		// Nor for an action's repeats while its screen offers it no key: the action suspended, a popup open, a text field
		// focused, the Game input mode. The repeats are consumed all the same.
		TestTrue(TEXT("The action takes Left"), Harness.KeyDown(EKeys::Left));
		Screen->SuspendedActions = {Step};
		TestTrue(TEXT("A repeat while the action is suspended is consumed"), Harness.KeyDown(EKeys::Left, true));
		Screen->SuspendedActions.Reset();
		Dropdown->Open(FReciTestHarness::UserIndex);
		TestTrue(TEXT("A repeat while a popup is open is consumed"), Harness.KeyDown(EKeys::Left, true));
		Dropdown->Close();
		TestTrue(TEXT("Focus reaches the text field"), Harness.Focus(*Field));
		TestTrue(TEXT("A repeat while a text field has focus is consumed"), Harness.KeyDown(EKeys::Left, true));
		TestTrue(TEXT("Focus returns to the list"), Harness.Focus(*List));
		Environment.InputMode = ECommonInputMode::Game;
		TestTrue(TEXT("A repeat under the Game input mode is consumed"), Harness.KeyDown(EKeys::Left, true));
		Environment.InputMode = ECommonInputMode::All;
		TestTrue(TEXT("With all of that undone, the action takes the next repeat"), Harness.KeyDown(EKeys::Left, true));
		Harness.KeyUp(EKeys::Left);
		TestEqual(TEXT("Only the press and the last repeat are timed"), Log.ToString(), FString(TEXT(
			"Screen Key Left, Resolver Left, Screen Action IA_Step, Dropdown Close, Resolver Left repeat, Screen Action IA_Step repeat")));
		Log.Reset();

		// (3) Never for a key-down Reci does not offer the screens' actions, even of a key one maps: a Back that closes a
		// popup, any key while a popup is open, while a text field has focus, under the Game input mode, while no
		// registered screen holds focus, and a key-down during which focus moved before Reci saw it.
		Dropdown->Open(FReciTestHarness::UserIndex);
		TestTrue(TEXT("Back closes the popup"), Harness.KeyDown(EKeys::Escape));
		TestTrue(TEXT("Its repeat is blocked"), Harness.KeyDown(EKeys::Escape, true));
		TestTrue(TEXT("Its release is swallowed"), Harness.KeyUp(EKeys::Escape));
		Dropdown->Open(FReciTestHarness::UserIndex);
		TestFalse(TEXT("While a popup is open, J is not consumed"), Harness.KeyDown(EKeys::J));
		Harness.KeyUp(EKeys::J);
		Dropdown->Close();
		TestTrue(TEXT("Focus reaches the text field"), Harness.Focus(*Field));
		TestFalse(TEXT("While a text field has focus, J is not consumed"), Harness.KeyDown(EKeys::J));
		TestFalse(TEXT("Nor is its release"), Harness.KeyUp(EKeys::J));
		TestTrue(TEXT("Focus returns to the list"), Harness.Focus(*List));
		Environment.InputMode = ECommonInputMode::Game;
		TestFalse(TEXT("Under the Game input mode, J is not consumed"), Harness.KeyDown(EKeys::J));
		Harness.KeyUp(EKeys::J);
		Environment.InputMode = ECommonInputMode::All;
		TestTrue(TEXT("Focus reaches the control no screen holds"), Harness.Focus(*Loose));
		TestFalse(TEXT("With no registered screen holding focus, J is not consumed"), Harness.KeyDown(EKeys::J));
		Harness.KeyUp(EKeys::J);
		TestTrue(TEXT("Focus returns to the list"), Harness.Focus(*List));
		{
			const TSharedRef<FReciTestFocusMover> Mover = MakeShared<FReciTestFocusMover>();
			Mover->Key = EKeys::J;
			Mover->MoveTo = Screen;
			Mover->bArmed = true;
			FInputPreprocessorRegistrationKey MoverRegistration;
			MoverRegistration.Type = EInputPreProcessorType::PreGame;
			FSlateApplication::Get().RegisterInputPreProcessor(Mover, MoverRegistration);
			ON_SCOPE_EXIT
			{
				FSlateApplication::Get().UnregisterInputPreProcessor(Mover);
			};
			TestTrue(TEXT("A press of J during which focus moved is consumed"), Harness.SlateKeyDown(EKeys::J));
			TestTrue(TEXT("Focus moved to the screen"), Harness.GetFocusedWidget() == Screen->GetCachedWidget());
			TestTrue(TEXT("Its repeat is blocked"), Harness.SlateKeyDown(EKeys::J, true));
			TestTrue(TEXT("Its release is swallowed"), Harness.SlateKeyUp(EKeys::J));
		}
		TestTrue(TEXT("Focus returns to the list"), Harness.Focus(*List));
		// Nor for the focused control's navigation and Accept by keys no action maps.
		TestTrue(TEXT("The focused list takes Up"), Harness.KeyDown(EKeys::Up));
		Harness.KeyUp(EKeys::Up);
		const FKey GamepadAccept = EKeys::Virtual_Gamepad_Accept.GetVirtualKey();
		TestTrue(TEXT("The focused list takes the gamepad's Accept button"), Harness.KeyDown(GamepadAccept));
		Harness.KeyUp(GamepadAccept);
		TestEqual(TEXT("The resolver does not run for key-downs the screens' actions are not offered"), Log.ToString(), FString::Printf(TEXT(
			"Dropdown Close, Dropdown Close, "
			"Screen Key Up, List Navigate Up, List ReturnFocus, Screen Key %s, List Accept, List ReturnFocus"),
			*GamepadAccept.ToString()));
		Log.Reset();

		// (4) A stick's direction key is timed as any key; the stick's values and the wheel never are, whether Reci holds
		// them back or a focused control takes them.
		Screen->ActionTimestamps.Reset();
		TestTrue(TEXT("The action takes the stick's Up"), Harness.KeyDown(EKeys::Gamepad_LeftStick_Up));
		TestTrue(TEXT("The stick is held back while its direction is held"), Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.9f));
		TestTrue(TEXT("The direction's release is swallowed"), Harness.KeyUp(EKeys::Gamepad_LeftStick_Up));
		TestFalse(TEXT("The stick's return is not consumed"), Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.0f));
		TestTrue(TEXT("The focused list takes a deflection no action maps"), Harness.AnalogInput(EKeys::Gamepad_LeftY, -0.9f));
		TestFalse(TEXT("The stick's return is not consumed"), Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.0f));
		TestTrue(TEXT("The focused list takes a wheel turn"), Harness.MouseWheel(-1.0f));
		TestEqual(TEXT("The resolver runs only for the stick's direction key"), Log.ToString(), FString(TEXT(
			"Screen Key Gamepad_LeftStick_Up, Resolver Gamepad_LeftStick_Up, Screen Action IA_Stick, "
			"Screen Capturing, Screen Capturing, List Navigate Down, List ReturnFocus, Screen Capturing, List Wheel, List ReturnFocus")));
		TestEqual(TEXT("The stick's direction key carries the resolver's time"), Screen->ActionTimestamps, TArray<double>{1011.0});
		TestEqual(TEXT("The resolver ran once per timed event"), Resolver.Calls, 11);
		TestTrue(TEXT("The resolver gets the events of the harness user"),
			Resolver.Users.Num() == 1 && Resolver.Users.Contains(FReciTestHarness::UserIndex));
	}

	// (5) Nested screens: one call for a key-down offered to several screens, and a repeat timed for the screen that took
	// the press.
	{
		FReciFakeResolver Resolver;
		FReciTestHarness Harness(*this);
		if (!Harness.IsValid())
		{
			return false;
		}
		FReciTestCallLog& Log = Harness.GetLog();
		Resolver.Attach(Log);
		UInputAction* Tap = Harness.CreateAction(TEXT("IA_Tap"));
		UInputAction* Peek = Harness.CreateAction(TEXT("IA_Peek"));
		UReciTestScreen* Outer = Harness.CreateScreen(TEXT("Outer"));
		Outer->SetInputMapping(Harness.CreateMapping({{Tap, EKeys::J}}));
		Outer->HandledActions = {Tap};
		UReciTestScreen* Inner = Harness.CreateScreen(TEXT("Inner"), Outer);
		Inner->SetInputMapping(Harness.CreateMapping({{Peek, EKeys::J}}));
		Harness.ActivateScreen(*Outer);
		Harness.ActivateScreen(*Inner);
		FReciInputFocusCoordinator::Get().RegisterScreenInput(Outer);
		FReciInputFocusCoordinator::Get().RegisterScreenInput(Inner);
		Harness.Paint();
		TestTrue(TEXT("Focus reaches the inner screen"), Harness.Focus(*Inner));
		TestTrue(TEXT("The outer screen's action takes the J the inner one declines"), Harness.KeyDown(EKeys::J));
		TestTrue(TEXT("The outer screen's action takes its repeat"), Harness.KeyDown(EKeys::J, true));
		Harness.KeyUp(EKeys::J);
		TestEqual(TEXT("One call for the press, before the inner action, and one for the repeat"), Log.ToString(), FString(TEXT(
			"Inner Key J, Resolver J, Inner Action IA_Peek, Outer Key J, Outer Action IA_Tap, Resolver J repeat, Outer Action IA_Tap repeat")));
		TestEqual(TEXT("The inner action gets the press's time"), Inner->ActionTimestamps, TArray<double>{1001.0});
		TestEqual(TEXT("The outer action gets the same time, then the repeat's"), Outer->ActionTimestamps, TArray<double>{1001.0, 1002.0});
	}
	return true;
}

#endif
