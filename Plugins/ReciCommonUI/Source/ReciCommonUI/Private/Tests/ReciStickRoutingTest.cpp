// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/ReciTestHarness.h"

#include "Framework/Application/NavigationConfig.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Input/Events.h"
#include "InputAction.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

namespace
{
	FAnalogInputEvent MakeStickEvent(const FKey& Axis, const float Value)
	{
		return FAnalogInputEvent(Axis, FModifierKeysState(), FReciTestHarness::UserIndex, false, 0, 0, Value);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciStickRoutingTest, "ReciCommonUI.Input.Routing.Sticks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciStickRoutingTest::RunTest(const FString& Parameters)
{
	// A screen's stick actions come from the platform's stick-direction keys, as any key's do; the stick's values only
	// stay out of focused navigation while a screen owns the deflection or may still take it.
	{
		FReciTestHarness Harness(*this);
		if (!Harness.IsValid())
		{
			return false;
		}
		FReciTestCallLog& Log = Harness.GetLog();
		// A registered screen whose action takes the left stick's Up, and a Reci control in it that takes every
		// direction.
		UInputAction* Up = Harness.CreateAction(TEXT("IA_Up"));
		UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
		Screen->SetInputMapping(Harness.CreateMapping({{Up, EKeys::Gamepad_LeftStick_Up}}));
		UReciTestFocusTarget* List = Harness.CreateTarget(TEXT("List"), Screen);
		List->HandledDirections = {EUINavigation::Up, EUINavigation::Down, EUINavigation::Left, EUINavigation::Right};
		Harness.ActivateScreen(*Screen);
		FReciInputFocusCoordinator::Get().RegisterScreenInput(Screen);
		Harness.Paint();

		// (1) The action declines the direction: the key and the deflection go on, to the stock path with no Reci control
		// focused, and to the focused control otherwise.
		TestTrue(TEXT("Focus reaches the screen"), Harness.Focus(*Screen));
		TestFalse(TEXT("A stick direction the screen declines is not consumed"), Harness.KeyDown(EKeys::Gamepad_LeftStick_Up));
		TestFalse(TEXT("The stick that follows is not consumed"), Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.9f));
		TestFalse(TEXT("Nor is its next value"), Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.9f));
		TestFalse(TEXT("The direction's repeat is not consumed"), Harness.KeyDown(EKeys::Gamepad_LeftStick_Up, true));
		TestFalse(TEXT("Its release is not consumed"), Harness.KeyUp(EKeys::Gamepad_LeftStick_Up));
		TestFalse(TEXT("The stick's return is not consumed"), Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.0f));
		TestEqual(TEXT("The declined direction reaches the screen once, and its repeat nothing"), Log.ToString(),
			FString(TEXT("Screen Key Gamepad_LeftStick_Up, Screen Action IA_Up, Screen Capturing, Screen Capturing, Screen Capturing")));
		Log.Reset();
		TestTrue(TEXT("Focus reaches the control"), Harness.Focus(*List));
		TestFalse(TEXT("With a control focused, the declined direction is not consumed"), Harness.KeyDown(EKeys::Gamepad_LeftStick_Up));
		TestTrue(TEXT("The focused control takes the deflection the screen declined"), Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.9f));
		Harness.KeyUp(EKeys::Gamepad_LeftStick_Up);
		Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.0f);
		TestEqual(TEXT("The deflection the screen declined steps the focused control"), Log.ToString(),
			FString(TEXT("Screen Key Gamepad_LeftStick_Up, Screen Action IA_Up, Screen Capturing, List Navigate Up, List ReturnFocus, "
				"Screen Capturing")));
		Log.Reset();

		// (2) The action takes the direction: it gets the platform's repeats with the key's time, as any key, and the
		// stick is consumed until the direction's release, whatever the focused control would take.
		Screen->HandledActions = {Up};
		FReciInputFocusCoordinator::Get().SetEventTimeResolver(
			[](const FInputEvent&, const FKey&, const bool bIsRepeat) { return bIsRepeat ? 2.0 : 1.0; });
		Screen->ActionTimestamps.Reset();
		TestTrue(TEXT("A stick direction the screen takes is consumed"), Harness.KeyDown(EKeys::Gamepad_LeftStick_Up));
		TestTrue(TEXT("The stick is consumed while the direction is held"), Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.9f));
		TestTrue(TEXT("So is a lesser value"), Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.4f));
		TestTrue(TEXT("The direction's repeat is consumed"), Harness.KeyDown(EKeys::Gamepad_LeftStick_Up, true));
		TestTrue(TEXT("Its release is consumed"), Harness.KeyUp(EKeys::Gamepad_LeftStick_Up));
		TestFalse(TEXT("After the release, the stick's return is not consumed"), Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.0f));
		TestEqual(TEXT("The press and its repeat reach the action, and the stick no control"), Log.ToString(),
			FString(TEXT("Screen Key Gamepad_LeftStick_Up, Screen Action IA_Up, Screen Action IA_Up repeat, Screen Capturing")));
		TestTrue(TEXT("The press and its repeat carry the resolver's times"),
			Screen->ActionTimestamps == TArray<double>({1.0, 2.0}));
		Log.Reset();

		// (3) Undecided: past the navigation threshold toward a direction the screen maps, before that direction's key
		// arrives, the stick is consumed. Below the threshold, and toward a direction no screen maps, it goes on.
		TestTrue(TEXT("The stick toward a mapped direction is consumed before its key arrives"),
			Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.9f));
		TestFalse(TEXT("Below the navigation threshold it is not consumed"), Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.3f));
		TestTrue(TEXT("Toward a direction no screen maps, the focused control takes it"),
			Harness.AnalogInput(EKeys::Gamepad_LeftY, -0.9f));
		Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.0f);
		TestEqual(TEXT("Only the unmapped direction reaches the control"), Log.ToString(), FString(TEXT(
			"Screen Capturing, Screen Capturing, Screen Capturing, List Navigate Down, List ReturnFocus, Screen Capturing")));
		Log.Reset();

		// (4) A capturing screen: the stick is consumed and moves nothing, and its direction keys reach the key capture.
		Screen->bCapturingKeys = true;
		Screen->HandledKeys = {EKeys::Gamepad_LeftStick_Up};
		TestTrue(TEXT("While the screen captures, the stick is consumed"), Harness.AnalogInput(EKeys::Gamepad_LeftX, 0.9f));
		TestTrue(TEXT("The key capture takes the stick's direction"), Harness.KeyDown(EKeys::Gamepad_LeftStick_Up));
		TestTrue(TEXT("Its repeat is consumed"), Harness.KeyDown(EKeys::Gamepad_LeftStick_Up, true));
		TestTrue(TEXT("The stick is consumed while the direction is held"), Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.9f));
		TestTrue(TEXT("Its release is consumed"), Harness.KeyUp(EKeys::Gamepad_LeftStick_Up));
		TestTrue(TEXT("The stick's return is consumed while the screen captures"), Harness.AnalogInput(EKeys::Gamepad_LeftX, 0.0f));
		TestEqual(TEXT("The direction keys reach only the key capture"), Log.ToString(), FString(TEXT(
			"Screen Capturing, Screen Key Gamepad_LeftStick_Up, Screen Key Gamepad_LeftStick_Up repeat, Screen Capturing")));
		Screen->bCapturingKeys = false;
		Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.0f);
	}

	// (5) A focused Reci control decides a deflection's first step without touching the user's navigation config, so a
	// step it declines is still Slate's to take; one it takes is its own until the stick returns.
	{
		FReciTestHarness Harness(*this);
		if (!Harness.IsValid())
		{
			return false;
		}
		FReciTestCallLog& Log = Harness.GetLog();
		// The config times analog repeats by the application's clock; the test moves it and puts it back.
		const double SavedTime = FApp::GetCurrentTime();
		ON_SCOPE_EXIT
		{
			FApp::SetCurrentTime(SavedTime);
		};
		FApp::SetCurrentTime(SavedTime + 10.0);
		UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
		UReciTestFocusTarget* List = Harness.CreateTarget(TEXT("List"), Screen);
		Harness.ActivateScreen(*Screen);
		Harness.Paint();
		TestTrue(TEXT("Focus reaches the control"), Harness.Focus(*List));
		const TSharedPtr<FSlateUser> User = FSlateApplication::Get().GetUser(FReciTestHarness::UserIndex);
		const TSharedPtr<FNavigationConfig> Config = User ? User->GetUserNavigationConfig() : nullptr;
		if (!Config)
		{
			AddError(TEXT("The harness user has a navigation config."));
			return false;
		}
		const FAnalogInputEvent Down = MakeStickEvent(EKeys::Gamepad_LeftY, -0.9f);
		const FAnalogInputEvent Return = MakeStickEvent(EKeys::Gamepad_LeftY, 0.0f);

		TestFalse(TEXT("A deflection the focused control declines is not consumed"), FReciInputFocusCoordinatorTestAccess::AnalogInput(Down));
		TestTrue(TEXT("Slate's analog navigation still takes the declined step"), Config->GetNavigationDirectionFromAnalog(Down) == EUINavigation::Down);
		List->HandledDirections = {EUINavigation::Down};
		TestFalse(TEXT("The rest of a declined deflection stays Slate's"), FReciInputFocusCoordinatorTestAccess::AnalogInput(Down));
		TestEqual(TEXT("The control is asked once per deflection"), Log.ToString(), FString(TEXT("List Navigate Down")));
		Log.Reset();
		// The stick returns, and Slate's config resets its repeat state, as Slate does with a value it sees.
		TestFalse(TEXT("The stick's return is not consumed"), FReciInputFocusCoordinatorTestAccess::AnalogInput(Return));
		Config->GetNavigationDirectionFromAnalog(Return);

		TestTrue(TEXT("A deflection the focused control takes is consumed"), FReciInputFocusCoordinatorTestAccess::AnalogInput(Down));
		TestTrue(TEXT("Its next value within the config's repeat delay is consumed"), FReciInputFocusCoordinatorTestAccess::AnalogInput(Down));
		TestEqual(TEXT("The control takes the first step only"), Log.ToString(), FString(TEXT("List Navigate Down, List ReturnFocus")));
		Log.Reset();
		FApp::SetCurrentTime(SavedTime + 11.0);
		TestTrue(TEXT("A value after the repeat delay is consumed"), FReciInputFocusCoordinatorTestAccess::AnalogInput(Down));
		List->HandledDirections.Reset();
		FApp::SetCurrentTime(SavedTime + 12.0);
		TestTrue(TEXT("A step the control declines at its end is consumed too"), FReciInputFocusCoordinatorTestAccess::AnalogInput(Down));
		TestEqual(TEXT("The config's repeat paces the control's later steps"), Log.ToString(),
			FString(TEXT("List Navigate Down, List ReturnFocus, List Navigate Down")));
		TestFalse(TEXT("The stick's return ends the deflection"), FReciInputFocusCoordinatorTestAccess::AnalogInput(Return));
	}
	return true;
}

namespace
{
	/** A screen with a focused Reci control in it that takes every direction. */
	UReciTestFocusTarget* MakeFocusedList(FReciTestHarness& Harness)
	{
		UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
		UReciTestFocusTarget* List = Harness.CreateTarget(TEXT("List"), Screen);
		List->HandledDirections = {EUINavigation::Up, EUINavigation::Down, EUINavigation::Left, EUINavigation::Right};
		Harness.ActivateScreen(*Screen);
		Harness.Paint();
		Harness.Focus(*List);
		Harness.GetLog().Reset();
		return List;
	}

	/** A focused registered screen whose actions IA_Up and IA_Right take the left stick's Up and Right. */
	UReciTestScreen* MakeStickScreen(FReciTestHarness& Harness)
	{
		UInputAction* Up = Harness.CreateAction(TEXT("IA_Up"));
		UInputAction* Right = Harness.CreateAction(TEXT("IA_Right"));
		UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
		Screen->SetInputMapping(Harness.CreateMapping({{Up, EKeys::Gamepad_LeftStick_Up}, {Right, EKeys::Gamepad_LeftStick_Right}}));
		Screen->HandledActions = {Up, Right};
		Harness.ActivateScreen(*Screen);
		FReciInputFocusCoordinator::Get().RegisterScreenInput(Screen);
		Harness.Paint();
		Harness.Focus(*Screen);
		Harness.GetLog().Reset();
		return Screen;
	}
}

// Stick Dominant Axis, with the navigation config Reci installs. A focused Reci control (1): a diagonal steps it along the
// axis further from rest; held, it repeats along the axis that is further at each repeat; a stick-direction key held back
// at its press leaves the stick's step along its axis to the control, before the key repeats; Reci asks the config before
// Slate does, and a step the control declines still goes to Slate, with the stick moved once. A screen's stick-direction
// keys (2): only the key of the stick's direction reaches the screen; a held key that becomes the stick's direction fires
// at its next repeat as a press, and the other key's repeats are consumed. With the setting off (3), and under Slate's
// own config (4), each axis navigates on its own. The test is named beside Sticks, not under it: a path under another
// test's makes that test a group, which then never runs.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciStickDominantAxisRoutingTest, "ReciCommonUI.Input.Routing.StickDominantAxis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciStickDominantAxisRoutingTest::RunTest(const FString& Parameters)
{
	// (1) A focused Reci control.
	{
		FReciTestHarness Harness(*this);
		if (!Harness.IsValid())
		{
			return false;
		}
		Harness.UseReciNavigationConfig(true);
		FReciTestCallLog& Log = Harness.GetLog();
		// The config times a stick's moves by the application's clock; the test moves it and puts it back.
		const double SavedTime = FApp::GetCurrentTime();
		ON_SCOPE_EXIT
		{
			FApp::SetCurrentTime(SavedTime);
		};
		FApp::SetCurrentTime(SavedTime + 10.0);
		UReciTestFocusTarget* List = MakeFocusedList(Harness);
		const TSharedPtr<FSlateUser> User = FSlateApplication::Get().GetUser(FReciTestHarness::UserIndex);
		const TSharedPtr<FNavigationConfig> Config = User ? User->GetUserNavigationConfig() : nullptr;
		if (!Config)
		{
			AddError(TEXT("The harness user has a navigation config."));
			return false;
		}
		TestTrue(TEXT("Y pushed up is taken"), Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.7f));
		TestFalse(TEXT("X pushed on to the diagonal, less far, is not taken"), Harness.AnalogInput(EKeys::Gamepad_LeftX, 0.6f));
		TestEqual(TEXT("and the config gives Slate no step for it"),
			Config->GetNavigationDirectionFromAnalog(MakeStickEvent(EKeys::Gamepad_LeftX, 0.6f)), EUINavigation::Invalid);
		TestEqual(TEXT("The diagonal steps the control up only"), Log.ToString(), FString(TEXT("List Navigate Up, List ReturnFocus")));
		Log.Reset();
		FApp::SetCurrentTime(SavedTime + 10.6);
		TestTrue(TEXT("Held past the repeat delay, X pushed further than Y is taken"), Harness.AnalogInput(EKeys::Gamepad_LeftX, 0.9f));
		TestFalse(TEXT("and Y, less far, is not"), Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.7f));
		TestEqual(TEXT("The repeat steps the control right"), Log.ToString(), FString(TEXT("List Navigate Right, List ReturnFocus")));
		Log.Reset();
		Harness.AnalogInput(EKeys::Gamepad_LeftX, 0.0f);
		Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.0f);

		Harness.AnalogInput(EKeys::Gamepad_LeftX, 0.25f);
		Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.3f);
		TestTrue(TEXT("Right, pressed while Y is further, is held back"), Harness.KeyDown(EKeys::Gamepad_LeftStick_Right));
		TestTrue(TEXT("Once X is further, its value is taken before Right repeats"), Harness.AnalogInput(EKeys::Gamepad_LeftX, 0.9f));
		TestEqual(TEXT("A held-back key leaves the stick's step to the control"), Log.ToString(),
			FString(TEXT("List Navigate Right, List ReturnFocus")));
		Log.Reset();
		Harness.KeyUp(EKeys::Gamepad_LeftStick_Right);
		Harness.AnalogInput(EKeys::Gamepad_LeftX, 0.0f);
		Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.0f);

		List->HandledDirections = {EUINavigation::Up, EUINavigation::Down, EUINavigation::Left};
		const FAnalogInputEvent Push = MakeStickEvent(EKeys::Gamepad_LeftX, 0.9f);
		TestFalse(TEXT("A push the control declines is not consumed"), FReciInputFocusCoordinatorTestAccess::AnalogInput(Push));
		TestEqual(TEXT("Slate, asking about the same value, gets its step"),
			Config->GetNavigationDirectionFromAnalog(Push), EUINavigation::Right);
		FApp::SetCurrentTime(SavedTime + 10.61);
		TestEqual(TEXT("The stick moved once: its next value waits the repeat delay"),
			Config->GetNavigationDirectionFromAnalog(MakeStickEvent(EKeys::Gamepad_LeftX, 0.9f)), EUINavigation::Invalid);
		TestEqual(TEXT("The control was offered the push"), Log.ToString(), FString(TEXT("List Navigate Right")));
		Harness.AnalogInput(EKeys::Gamepad_LeftX, 0.0f);
	}

	// (2) A screen's stick-direction keys, sent after the stick's values as a device sends them.
	{
		FReciTestHarness Harness(*this);
		if (!Harness.IsValid())
		{
			return false;
		}
		Harness.UseReciNavigationConfig(true);
		FReciTestCallLog& Log = Harness.GetLog();
		MakeStickScreen(Harness);
		Harness.AnalogInput(EKeys::Gamepad_LeftX, 0.6f);
		Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.4f);
		TestTrue(TEXT("Up, not the stick's direction, is consumed"), Harness.KeyDown(EKeys::Gamepad_LeftStick_Up));
		TestTrue(TEXT("Right, the stick's direction, is taken"), Harness.KeyDown(EKeys::Gamepad_LeftStick_Right));
		TestTrue(TEXT("Up's repeat is consumed"), Harness.KeyDown(EKeys::Gamepad_LeftStick_Up, true));
		TestTrue(TEXT("Right's repeat is taken"), Harness.KeyDown(EKeys::Gamepad_LeftStick_Right, true));
		TestEqual(TEXT("Only Right reaches the screen, and its repeat"), Log.ToString(), FString(TEXT(
			"Screen Capturing, Screen Capturing, Screen Key Gamepad_LeftStick_Right, Screen Action IA_Right, "
			"Screen Action IA_Right repeat")));
		Log.Reset();
		TestTrue(TEXT("Y pushed further is held for the screen that maps Up, whose key was held back"),
			Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.8f));
		TestTrue(TEXT("With Y further, Right's repeat is consumed"), Harness.KeyDown(EKeys::Gamepad_LeftStick_Right, true));
		TestTrue(TEXT("Up's next repeat is taken"), Harness.KeyDown(EKeys::Gamepad_LeftStick_Up, true));
		TestTrue(TEXT("and so is its repeat after that"), Harness.KeyDown(EKeys::Gamepad_LeftStick_Up, true));
		TestTrue(TEXT("Right's release goes to the screen that took its press"), Harness.KeyUp(EKeys::Gamepad_LeftStick_Right));
		TestTrue(TEXT("Up's release is consumed"), Harness.KeyUp(EKeys::Gamepad_LeftStick_Up));
		TestEqual(TEXT("Up reaches the screen as a press at its next repeat, then repeats"), Log.ToString(), FString(TEXT(
			"Screen Capturing, Screen Key Gamepad_LeftStick_Up, Screen Action IA_Up, Screen Action IA_Up repeat")));
		Harness.AnalogInput(EKeys::Gamepad_LeftX, 0.0f);
		Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.0f);
	}

	// (3) With the setting off, and (4) with Slate's own config: each axis navigates on its own, as in Slate, so both
	// directions of a diagonal step the focused control, and both of its keys reach the screen's actions.
	for (const bool bReciConfig : {true, false})
	{
		const FString Config = bReciConfig ? TEXT("With Stick Dominant Axis off") : TEXT("With Slate's config");
		{
			FReciTestHarness Harness(*this);
			if (!Harness.IsValid())
			{
				return false;
			}
			if (bReciConfig)
			{
				Harness.UseReciNavigationConfig(false);
			}
			MakeFocusedList(Harness);
			TestTrue(Config + TEXT(", Y pushed up is taken"), Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.7f));
			TestTrue(Config + TEXT(", X pushed on to the diagonal is taken too"), Harness.AnalogInput(EKeys::Gamepad_LeftX, 0.8f));
			TestEqual(Config + TEXT(", both axes step the control"), Harness.GetLog().ToString(),
				FString(TEXT("List Navigate Up, List ReturnFocus, List Navigate Right, List ReturnFocus")));
			Harness.AnalogInput(EKeys::Gamepad_LeftX, 0.0f);
			Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.0f);
		}
		{
			FReciTestHarness Harness(*this);
			if (!Harness.IsValid())
			{
				return false;
			}
			if (bReciConfig)
			{
				Harness.UseReciNavigationConfig(false);
			}
			MakeStickScreen(Harness);
			Harness.AnalogInput(EKeys::Gamepad_LeftX, 0.6f);
			Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.4f);
			TestTrue(Config + TEXT(", Up is taken"), Harness.KeyDown(EKeys::Gamepad_LeftStick_Up));
			TestTrue(Config + TEXT(", Right is taken too"), Harness.KeyDown(EKeys::Gamepad_LeftStick_Right));
			TestEqual(Config + TEXT(", both keys reach the screen's actions"), Harness.GetLog().ToString(), FString(TEXT(
				"Screen Capturing, Screen Capturing, Screen Key Gamepad_LeftStick_Up, Screen Action IA_Up, "
				"Screen Key Gamepad_LeftStick_Right, Screen Action IA_Right")));
			Harness.KeyUp(EKeys::Gamepad_LeftStick_Up);
			Harness.KeyUp(EKeys::Gamepad_LeftStick_Right);
			Harness.AnalogInput(EKeys::Gamepad_LeftX, 0.0f);
			Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.0f);
		}
	}
	return true;
}

#endif
