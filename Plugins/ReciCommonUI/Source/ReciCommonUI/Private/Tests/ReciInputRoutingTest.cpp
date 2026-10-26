// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/ReciTestHarness.h"

#include "Algo/Reverse.h"
#include "Blueprint/WidgetTree.h"
#include "Components/EditableText.h"
#include "Components/InputKeySelector.h"
#include "InputAction.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciFocusedListRoutingTest, "ReciCommonUI.Input.FocusedListRouting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciFocusedListRoutingTest::RunTest(const FString& Parameters)
{
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	FReciTestCallLog& Log = Harness.GetLog();
	// Two Reci controls on a screen that takes no screen input. The one registered last is asked first, so it must
	// lose to the one that holds focus.
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	UReciTestFocusTarget* List = Harness.CreateTarget(TEXT("List"), Screen);
	List->HandledDirections.Add(EUINavigation::Down);
	List->bHandlesAccept = true;
	UReciTestFocusTarget* Other = Harness.CreateTarget(TEXT("Other"), Screen);
	Other->HandledDirections = {EUINavigation::Up, EUINavigation::Down};
	Other->bHandlesAccept = true;
	Harness.ActivateScreen(*Screen);
	Harness.Paint();
	const FKey GamepadAccept = EKeys::Virtual_Gamepad_Accept.GetVirtualKey();
	TestTrue(TEXT("Focus reaches the list"), Harness.Focus(*List));

	// A direction or Accept the focused control takes is consumed, and the control then puts focus on its driven item.
	TestTrue(TEXT("The focused list consumes a direction it takes"), Harness.KeyDown(EKeys::Down));
	Harness.KeyUp(EKeys::Down);
	TestTrue(TEXT("The focused list consumes the gamepad's direction"), Harness.KeyDown(EKeys::Gamepad_DPad_Down));
	Harness.KeyUp(EKeys::Gamepad_DPad_Down);
	TestEqual(TEXT("Only the focused list navigates"), Log.ToString(),
		FString(TEXT("List Navigate Down, List ReturnFocus, List Navigate Down, List ReturnFocus")));
	Log.Reset();
	TestTrue(TEXT("The focused list consumes Accept"), Harness.KeyDown(EKeys::Enter));
	TestTrue(TEXT("Accept's release is consumed with it"), Harness.KeyUp(EKeys::Enter));
	TestTrue(TEXT("The focused list consumes the gamepad's Accept"), Harness.KeyDown(GamepadAccept));
	Harness.KeyUp(GamepadAccept);
	TestEqual(TEXT("Only the focused list takes Accept"), Log.ToString(),
		FString(TEXT("List Accept, List ReturnFocus, List Accept, List ReturnFocus")));
	Log.Reset();

	// What the focused control declines, and a key that is neither a direction nor Accept, go on to the stock path.
	TestFalse(TEXT("A direction the focused list declines is not consumed"), Harness.KeyDown(EKeys::Up));
	Harness.KeyUp(EKeys::Up);
	TestFalse(TEXT("A key that is not navigation is not consumed"), Harness.KeyDown(EKeys::A));
	Harness.KeyUp(EKeys::A);
	TestEqual(TEXT("The focused list is asked about its direction, the other control about nothing"), Log.ToString(),
		FString(TEXT("List Navigate Up")));
	Log.Reset();

	// Another user's keys never reach the harness user's controls.
	const FKeyEvent OtherUserDown(EKeys::Down, FModifierKeysState(), FReciTestHarness::UserIndex + 1, false, 0, 0);
	TestFalse(TEXT("Another user's direction is not consumed"), FReciInputFocusCoordinatorTestAccess::KeyDown(OtherUserDown));
	FReciInputFocusCoordinatorTestAccess::KeyUp(OtherUserDown);
	TestEqual(TEXT("Another user's direction reaches no control"), Log.ToString(), FString());

	// Focus decides: on the other control, it takes the keys; on the screen itself, no control does.
	TestTrue(TEXT("Focus reaches the other control"), Harness.Focus(*Other));
	TestTrue(TEXT("The newly focused control consumes a direction it takes"), Harness.KeyDown(EKeys::Up));
	Harness.KeyUp(EKeys::Up);
	TestEqual(TEXT("Only the newly focused control navigates"), Log.ToString(), FString(TEXT("Other Navigate Up, Other ReturnFocus")));
	Log.Reset();
	TestTrue(TEXT("Focus reaches the screen"), Harness.Focus(*Screen));
	TestFalse(TEXT("With no control focused, a direction is not consumed"), Harness.KeyDown(EKeys::Down));
	Harness.KeyUp(EKeys::Down);
	TestFalse(TEXT("With no control focused, Accept is not consumed"), Harness.KeyDown(EKeys::Enter));
	TestFalse(TEXT("With no control focused, Accept's release is not consumed"), Harness.KeyUp(EKeys::Enter));
	TestEqual(TEXT("No control is asked while none holds focus"), Log.ToString(), FString());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciPopupBackFirstTest, "ReciCommonUI.Input.PopupBackFirst",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciPopupBackFirstTest::RunTest(const FString& Parameters)
{
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	FReciTestCallLog& Log = Harness.GetLog();
	// A registered screen whose own action takes Back, two popups of the harness user (the upper opened last), and,
	// opened after both, a popup of another user.
	UInputAction* BackAction = Harness.CreateAction(TEXT("IA_Back"));
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	Screen->SetInputMapping(Harness.CreateMapping({{BackAction, EKeys::Escape}}));
	Screen->HandledActions.Add(BackAction);
	UReciTestPopupOwner* Lower = Harness.CreatePopupOwner(TEXT("Lower"), Screen);
	UReciTestPopupOwner* Upper = Harness.CreatePopupOwner(TEXT("Upper"), Screen);
	UReciTestPopupOwner* OtherUser = Harness.CreatePopupOwner(TEXT("OtherUser"), Screen);
	Harness.ActivateScreen(*Screen);
	FReciInputFocusCoordinator::Get().RegisterScreenInput(Screen);
	UReciTestFocusTarget* LowerList = Lower->Open(FReciTestHarness::UserIndex);
	UReciTestFocusTarget* UpperList = Upper->Open(FReciTestHarness::UserIndex);
	OtherUser->Open(FReciTestHarness::UserIndex + 1);
	Harness.Paint();
	TestTrue(TEXT("Focus reaches the upper popup"), Harness.Focus(*UpperList));

	// Back closes the user's top popup, one layer at a time, before the screen or a control sees it.
	TestTrue(TEXT("Back is consumed"), Harness.KeyDown(EKeys::Escape));
	TestTrue(TEXT("Back's release is consumed with it"), Harness.KeyUp(EKeys::Escape));
	TestEqual(TEXT("Back closes the upper popup and reaches nothing else"), Log.ToString(), FString(TEXT("Upper Close")));
	TestTrue(TEXT("The lower popup stays open"), Lower->IsOpen());
	Log.Reset();
	TestTrue(TEXT("Focus reaches the lower popup"), Harness.Focus(*LowerList));
	TestTrue(TEXT("The next Back is consumed"), Harness.KeyDown(EKeys::Escape));
	Harness.KeyUp(EKeys::Escape);
	TestEqual(TEXT("The next Back closes the lower popup and reaches nothing else"), Log.ToString(), FString(TEXT("Lower Close")));
	Log.Reset();

	// With none of its own popups open, the user's Back takes the usual order; another user's popup never takes it.
	TestTrue(TEXT("Focus reaches the screen"), Harness.Focus(*Screen));
	TestTrue(TEXT("The screen's action consumes Back"), Harness.KeyDown(EKeys::Escape));
	Harness.KeyUp(EKeys::Escape);
	TestEqual(TEXT("Back reaches the screen once its user's popups are closed"), Log.ToString(),
		FString(TEXT("Screen Key Escape, Screen Action IA_Back")));
	TestTrue(TEXT("Another user's popup stays open"), OtherUser->IsOpen());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciRawInputTextFieldTest, "ReciCommonUI.Input.Routing.RawInput.TextField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciRawInputTextFieldTest::RunTest(const FString& Parameters)
{
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	FReciTestCallLog& Log = Harness.GetLog();
	// A registered screen with actions on a letter and on a stick direction, a Reci control that takes Down and Accept
	// with a text field inside it, as an entry holds one, and an open popup.
	UInputAction* Type = Harness.CreateAction(TEXT("IA_Type"));
	UInputAction* Stick = Harness.CreateAction(TEXT("IA_Stick"));
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	Screen->SetInputMapping(Harness.CreateMapping({{Type, EKeys::A}, {Stick, EKeys::Gamepad_LeftStick_Up}}));
	Screen->HandledActions = {Type, Stick};
	UReciTestFocusTarget* Entry = Harness.CreateTarget(TEXT("Entry"), Screen);
	Entry->HandledDirections.Add(EUINavigation::Down);
	Entry->bHandlesAccept = true;
	UEditableText* Field = Entry->WidgetTree->ConstructWidget<UEditableText>(UEditableText::StaticClass(), TEXT("Field"));
	Entry->SetContent(Field);
	UReciTestPopupOwner* Dropdown = Harness.CreatePopupOwner(TEXT("Dropdown"), Screen);
	Harness.ActivateScreen(*Screen);
	FReciInputFocusCoordinator::Get().RegisterScreenInput(Screen);
	Dropdown->Open(FReciTestHarness::UserIndex);
	Harness.Paint();

	// Focus on the field activates its text input method context, as on any text field; moving focus off deactivates it.
	TestTrue(TEXT("Focus reaches the text field"), Harness.Focus(*Field));
	const TSharedPtr<SWidget> Focused = Harness.GetFocusedWidget();
	TestTrue(TEXT("The user's focus is on an editable text"), Focused && Focused->GetType() == FName(TEXT("SEditableText")));
	TestTrue(TEXT("The control around the field would take focused input"), Entry->CanHandleReciFocusedInput());

	for (const FKey& Key : {EKeys::A, EKeys::Down, EKeys::Enter, EKeys::Escape, EKeys::Gamepad_LeftStick_Up})
	{
		TestFalse(FString::Printf(TEXT("%s is not consumed while a text field has focus"), *Key.ToString()), Harness.KeyDown(Key));
		TestFalse(FString::Printf(TEXT("The release of %s is not consumed"), *Key.ToString()), Harness.KeyUp(Key));
	}
	TestFalse(TEXT("A stick the screen uses is not consumed while a text field has focus"), Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.9f));
	TestFalse(TEXT("The stick's return is not consumed"), Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.0f));
	TestEqual(TEXT("No screen, control or popup is called while a text field has focus"), Log.ToString(), FString());
	TestTrue(TEXT("Back leaves the popup open while a text field has focus"), Dropdown->IsOpen());

	// The same keys with focus on the control itself: popup Back first, then the screen's action and the control.
	TestTrue(TEXT("Focus reaches the control"), Harness.Focus(*Entry));
	TestTrue(TEXT("Back closes the popup"), Harness.KeyDown(EKeys::Escape));
	Harness.KeyUp(EKeys::Escape);
	TestTrue(TEXT("The screen's action consumes its key"), Harness.KeyDown(EKeys::A));
	Harness.KeyUp(EKeys::A);
	TestTrue(TEXT("The control consumes a direction"), Harness.KeyDown(EKeys::Down));
	Harness.KeyUp(EKeys::Down);
	TestEqual(TEXT("Without the text field's focus, the keys are routed"), Log.ToString(),
		FString(TEXT("Dropdown Close, Screen Key A, Screen Action IA_Type, Screen Key Down, Entry Navigate Down, Entry ReturnFocus")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciRawInputKeySelectorTest, "ReciCommonUI.Input.Routing.RawInput.KeySelector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciRawInputKeySelectorTest::RunTest(const FString& Parameters)
{
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	FReciTestCallLog& Log = Harness.GetLog();
	// A registered screen with an action on K and a Reci control that takes Down and Accept with a stock key selector
	// inside it, as a key-binding entry holds one, and a popup owner.
	UInputAction* Bind = Harness.CreateAction(TEXT("IA_Bind"));
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	Screen->SetInputMapping(Harness.CreateMapping({{Bind, EKeys::K}}));
	Screen->HandledActions = {Bind};
	UReciTestFocusTarget* Entry = Harness.CreateTarget(TEXT("Entry"), Screen);
	Entry->HandledDirections.Add(EUINavigation::Down);
	Entry->bHandlesAccept = true;
	UInputKeySelector* Selector =
		Entry->WidgetTree->ConstructWidget<UInputKeySelector>(UInputKeySelector::StaticClass(), TEXT("Selector"));
	Selector->OnKeySelected.AddDynamic(Entry, &UReciTestFocusTarget::LogKeySelected);
	Entry->SetContent(Selector);
	UReciTestPopupOwner* Dropdown = Harness.CreatePopupOwner(TEXT("Dropdown"), Screen);
	Harness.ActivateScreen(*Screen);
	FReciInputFocusCoordinator::Get().RegisterScreenInput(Screen);
	Harness.Paint();
	TestTrue(TEXT("Focus reaches the key selector"), Harness.Focus(*Selector));
	TestTrue(TEXT("The control around the key selector would take focused input"), Entry->CanHandleReciFocusedInput());

	// Not selecting: the screen still comes first, but the control around the key selector takes neither Accept nor a
	// direction. They go on to the key selector and Slate navigation.
	TestFalse(TEXT("Accept on a key selector is not consumed"), Harness.KeyDown(EKeys::Enter));
	TestFalse(TEXT("Its release is not consumed"), Harness.KeyUp(EKeys::Enter));
	TestFalse(TEXT("A direction on a key selector is not consumed"), Harness.KeyDown(EKeys::Down));
	Harness.KeyUp(EKeys::Down);
	TestEqual(TEXT("The control around a key selector is not asked"), Log.ToString(), FString(TEXT("Screen Key Enter, Screen Key Down")));
	Log.Reset();

	// Through Slate, Accept presses the key selector's button and its release clicks it: the key selector starts selecting.
	TestTrue(TEXT("The key selector takes Accept"), Harness.SlateKeyDown(EKeys::Enter));
	Harness.SlateKeyUp(EKeys::Enter);
	TestTrue(TEXT("Accept starts selecting a key"), Selector->GetIsSelectingKey());
	TestEqual(TEXT("Accept reaches only the screen's key capture before the key selector"), Log.ToString(), FString(TEXT("Screen Key Enter")));
	Log.Reset();

	// Selecting: Reci steps aside for every key and stick, Back with a popup open included, since Escape cancels the
	// selection.
	Dropdown->Open(FReciTestHarness::UserIndex);
	TestFalse(TEXT("While a key selector selects, Back is not consumed"), Harness.KeyDown(EKeys::Escape));
	TestFalse(TEXT("Nor its release"), Harness.KeyUp(EKeys::Escape));
	TestTrue(TEXT("Back leaves the popup open while a key selector selects"), Dropdown->IsOpen());
	TestFalse(TEXT("While a key selector selects, a stick is not consumed"), Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.9f));
	Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.0f);
	Dropdown->Close();
	TestTrue(TEXT("The key selector still selects"), Selector->GetIsSelectingKey());
	Log.Reset();
	TestTrue(TEXT("The key selector takes the screen's action key"), Harness.SlateKeyDown(EKeys::K));
	TestTrue(TEXT("Its release selects the key"), Harness.SlateKeyUp(EKeys::K));
	TestFalse(TEXT("The key selector has stopped selecting"), Selector->GetIsSelectingKey());
	TestTrue(TEXT("The key selector holds the key"), Selector->GetSelectedKey().Key == EKeys::K);
	TestEqual(TEXT("The key reaches the key selector and no screen or control"), Log.ToString(), FString(TEXT("Entry KeySelected K")));
	return true;
}

namespace
{
	/** The screens in the order a pass registers them: the first pass as listed, the second the other way round. */
	TArray<UReciTestScreen*> InRegistrationOrder(const TArray<UReciTestScreen*>& Screens, const bool bReversed)
	{
		TArray<UReciTestScreen*> Ordered = Screens;
		if (bReversed)
		{
			Algo::Reverse(Ordered);
		}
		return Ordered;
	}

	FString DescribeOrder(const bool bReversed)
	{
		return bReversed ? TEXT(" (registered inner first)") : TEXT(" (registered outer first)");
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciNestedScreensRoutingTest, "ReciCommonUI.Input.Routing.NestedScreens",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciNestedScreensRoutingTest::RunTest(const FString& Parameters)
{
	// Which screen gets a key first follows the activatable tree, never the order the screens registered in.
	for (const bool bInnerFirst : {false, true})
	{
		const FString Order = DescribeOrder(bInnerFirst);
		FReciTestHarness Harness(*this);
		if (!Harness.IsValid())
		{
			return false;
		}
		FReciTestCallLog& Log = Harness.GetLog();
		// An outer screen with a screen nested in it that is not Is Modal, both registered. Each has an action on J, and
		// the outer screen has one on the stick.
		UInputAction* OuterTap = Harness.CreateAction(TEXT("IA_OuterTap"));
		UInputAction* OuterStick = Harness.CreateAction(TEXT("IA_OuterStick"));
		UInputAction* InnerTap = Harness.CreateAction(TEXT("IA_InnerTap"));
		UReciTestScreen* Outer = Harness.CreateScreen(TEXT("Outer"));
		Outer->SetInputMapping(Harness.CreateMapping({{OuterTap, EKeys::J}, {OuterStick, EKeys::Gamepad_LeftStick_Up}}));
		UReciTestScreen* Inner = Harness.CreateScreen(TEXT("Inner"), Outer);
		Inner->SetInputMapping(Harness.CreateMapping({{InnerTap, EKeys::J}}));
		Harness.ActivateScreen(*Outer);
		Harness.ActivateScreen(*Inner);
		for (UReciTestScreen* Screen : InRegistrationOrder({Outer, Inner}, bInnerFirst))
		{
			FReciInputFocusCoordinator::Get().RegisterScreenInput(Screen);
		}
		Harness.Paint();
		TestTrue(TEXT("Focus reaches the inner screen") + Order, Harness.Focus(*Inner));

		// Focus in the inner screen: its key capture and its actions, then the outer screen's.
		TestFalse(TEXT("A key neither screen takes is not consumed") + Order, Harness.KeyDown(EKeys::J));
		TestFalse(TEXT("Its release is not consumed") + Order, Harness.KeyUp(EKeys::J));
		TestEqual(TEXT("Each screen's key capture, then its actions, innermost first") + Order, Log.ToString(),
			FString(TEXT("Inner Key J, Inner Action IA_InnerTap, Outer Key J, Outer Action IA_OuterTap")));
		Log.Reset();
		Inner->HandledActions = {InnerTap};
		Outer->HandledActions = {OuterTap};
		TestTrue(TEXT("The inner screen's action consumes the key") + Order, Harness.KeyDown(EKeys::J));
		TestTrue(TEXT("Its release is consumed with it") + Order, Harness.KeyUp(EKeys::J));
		TestEqual(TEXT("A key the inner screen takes never reaches the outer one") + Order, Log.ToString(),
			FString(TEXT("Inner Key J, Inner Action IA_InnerTap")));
		Log.Reset();
		Inner->HandledActions.Reset();
		TestTrue(TEXT("The outer screen's action consumes what the inner one declines") + Order, Harness.KeyDown(EKeys::J));
		Harness.KeyUp(EKeys::J);
		TestEqual(TEXT("A key the inner screen declines goes on to the outer one") + Order, Log.ToString(),
			FString(TEXT("Inner Key J, Inner Action IA_InnerTap, Outer Key J, Outer Action IA_OuterTap")));
		Log.Reset();
		Inner->HandledKeys.Add(EKeys::J);
		TestTrue(TEXT("The inner screen's key capture consumes the key") + Order, Harness.KeyDown(EKeys::J));
		Harness.KeyUp(EKeys::J);
		TestEqual(TEXT("A key the inner capture takes reaches neither its actions nor the outer screen") + Order, Log.ToString(),
			FString(TEXT("Inner Key J")));
		Inner->HandledKeys.Reset();
		Log.Reset();

		// Focus in the outer screen, outside the inner one: only the outer screen.
		TestTrue(TEXT("Focus reaches the outer screen") + Order, Harness.Focus(*Outer));
		TestTrue(TEXT("The outer screen's action consumes the key") + Order, Harness.KeyDown(EKeys::J));
		Harness.KeyUp(EKeys::J);
		TestEqual(TEXT("Focus outside the inner screen reaches only the outer one") + Order, Log.ToString(),
			FString(TEXT("Outer Key J, Outer Action IA_OuterTap")));
		Log.Reset();

		// A stick direction that only the outer screen uses reaches its action from inside the inner screen, as any key
		// does, and while any screen of the chain captures, the stick only stops.
		TestTrue(TEXT("Focus returns to the inner screen") + Order, Harness.Focus(*Inner));
		Outer->HandledActions.Add(OuterStick);
		TestTrue(TEXT("The outer screen's stick direction is consumed") + Order, Harness.KeyDown(EKeys::Gamepad_LeftStick_Up));
		TestTrue(TEXT("The stick is consumed while its direction is held") + Order, Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.9f));
		TestTrue(TEXT("The direction's release is consumed") + Order, Harness.KeyUp(EKeys::Gamepad_LeftStick_Up));
		TestFalse(TEXT("The stick's return is not consumed") + Order, Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.0f));
		TestEqual(TEXT("The stick direction reaches the outer screen's action") + Order, Log.ToString(),
			FString(TEXT("Inner Key Gamepad_LeftStick_Up, Outer Key Gamepad_LeftStick_Up, Outer Action IA_OuterStick, "
				"Inner Capturing, Outer Capturing")));
		Log.Reset();
		Inner->bCapturingKeys = true;
		TestTrue(TEXT("While the inner screen captures, the stick is consumed") + Order, Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.9f));
		TestEqual(TEXT("A capturing inner screen stops the stick") + Order, Log.ToString(), FString(TEXT("Inner Capturing")));
		Log.Reset();
		Inner->bCapturingKeys = false;
		Outer->bCapturingKeys = true;
		TestTrue(TEXT("While the outer screen captures, the stick is consumed") + Order, Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.9f));
		TestEqual(TEXT("A capturing outer screen stops the stick, and no action steps") + Order, Log.ToString(),
			FString(TEXT("Inner Capturing, Outer Capturing")));
		Outer->bCapturingKeys = false;
		Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.0f);
		Log.Reset();

		// A deactivated inner screen that still holds focus is skipped.
		Harness.DeactivateScreen(*Inner);
		TestTrue(TEXT("Focus stays on the deactivated inner screen") + Order, Harness.GetFocusedWidget() == Inner->GetCachedWidget());
		TestTrue(TEXT("The outer screen's action consumes the key") + Order, Harness.KeyDown(EKeys::J));
		Harness.KeyUp(EKeys::J);
		TestEqual(TEXT("A deactivated inner screen is skipped") + Order, Log.ToString(), FString(TEXT("Outer Key J, Outer Action IA_OuterTap")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciModalityRoutingTest, "ReciCommonUI.Input.Routing.Modality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciModalityRoutingTest::RunTest(const FString& Parameters)
{
	for (const bool bInnerFirst : {false, true})
	{
		const FString Order = DescribeOrder(bInnerFirst);
		FReciTestHarness Harness(*this);
		if (!Harness.IsValid())
		{
			return false;
		}
		FReciTestCallLog& Log = Harness.GetLog();
		FReciTestRoutingEnvironment& Environment = Harness.GetEnvironment();
		// A registered outer screen with an activatable widget nested in it and a Reci control in that widget, and a
		// screen of its own layer that takes no screen input. Each screen's action takes J; the control takes Down.
		UInputAction* OuterTap = Harness.CreateAction(TEXT("IA_OuterTap"));
		UInputAction* InnerTap = Harness.CreateAction(TEXT("IA_InnerTap"));
		UReciTestScreen* Outer = Harness.CreateScreen(TEXT("Outer"));
		Outer->SetInputMapping(Harness.CreateMapping({{OuterTap, EKeys::J}}));
		Outer->HandledActions = {OuterTap};
		UReciTestScreen* Inner = Harness.CreateScreen(TEXT("Inner"), Outer);
		Inner->SetInputMapping(Harness.CreateMapping({{InnerTap, EKeys::J}}));
		Inner->HandledActions = {InnerTap};
		UReciTestFocusTarget* List = Harness.CreateTarget(TEXT("List"), Inner);
		List->HandledDirections.Add(EUINavigation::Down);
		UReciTestScreen* Unrelated = Harness.CreateScreen(TEXT("Unrelated"));
		Harness.ActivateScreen(*Outer);
		Harness.ActivateScreen(*Inner);
		Harness.ActivateScreen(*Unrelated);
		FReciInputFocusCoordinator::Get().RegisterScreenInput(Outer);
		Harness.Paint();
		TestTrue(TEXT("Focus reaches the control in the nested widget") + Order, Harness.Focus(*List));

		// (a) The outer screen's root is active, and the nested widget is neither Is Modal nor registered.
		Environment.SetActiveRoot(*Outer);
		TestTrue(TEXT("The outer screen's action consumes the key") + Order, Harness.KeyDown(EKeys::J));
		Harness.KeyUp(EKeys::J);
		TestEqual(TEXT("Focus in a nested widget that is not Is Modal reaches the outer screen") + Order, Log.ToString(),
			FString(TEXT("Outer Key J, Outer Action IA_OuterTap")));
		Log.Reset();

		// (b) Is Modal makes the nested widget a root of its own, here the active one: the outer screen gets nothing, and
		// keys go on to the focused control and the stock path.
		Inner->SetIsModal(true);
		Environment.SetActiveRoot(*Inner);
		TestFalse(TEXT("Under an Is Modal widget, the outer screen's key is not consumed") + Order, Harness.KeyDown(EKeys::J));
		TestFalse(TEXT("Its release is not consumed") + Order, Harness.KeyUp(EKeys::J));
		TestTrue(TEXT("The focused control in the Is Modal widget takes its direction") + Order, Harness.KeyDown(EKeys::Down));
		Harness.KeyUp(EKeys::Down);
		TestEqual(TEXT("The outer screen gets nothing under an Is Modal widget") + Order, Log.ToString(),
			FString(TEXT("List Navigate Down, List ReturnFocus")));
		Log.Reset();

		// (c) Registered, the Is Modal widget takes its own keys, whichever screen registered first, and the outer screen
		// none. Focus beside it, still in the outer screen, is outside the active root and reaches neither.
		FReciInputFocusCoordinator::Get().UnregisterScreenInput(Outer);
		for (UReciTestScreen* Screen : InRegistrationOrder({Outer, Inner}, bInnerFirst))
		{
			FReciInputFocusCoordinator::Get().RegisterScreenInput(Screen);
		}
		TestTrue(TEXT("The Is Modal screen's action consumes the key") + Order, Harness.KeyDown(EKeys::J));
		Harness.KeyUp(EKeys::J);
		TestEqual(TEXT("A registered Is Modal screen takes its keys, and the outer screen none") + Order, Log.ToString(),
			FString(TEXT("Inner Key J, Inner Action IA_InnerTap")));
		Log.Reset();
		TestTrue(TEXT("Focus reaches the outer screen beside the Is Modal one") + Order, Harness.Focus(*Outer));
		TestFalse(TEXT("Beside the active root, the key is not consumed") + Order, Harness.KeyDown(EKeys::J));
		Harness.KeyUp(EKeys::J);
		TestEqual(TEXT("Focus outside the active root reaches no screen") + Order, Log.ToString(), FString());

		// (d) Another root is active: no screen gets screen input, Is Modal or not.
		TestTrue(TEXT("Focus returns to the control") + Order, Harness.Focus(*List));
		Environment.SetActiveRoot(*Unrelated);
		TestFalse(TEXT("Under an unrelated active root, the key is not consumed") + Order, Harness.KeyDown(EKeys::J));
		Harness.KeyUp(EKeys::J);
		Inner->SetIsModal(false);
		TestFalse(TEXT("Nor once the nested widget is not Is Modal") + Order, Harness.KeyDown(EKeys::J));
		Harness.KeyUp(EKeys::J);
		TestEqual(TEXT("No screen gets input outside the active root") + Order, Log.ToString(), FString());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciInputModeRoutingTest, "ReciCommonUI.Input.Routing.InputMode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciInputModeRoutingTest::RunTest(const FString& Parameters)
{
	// The screen and the control register in either order.
	for (const bool bControlFirst : {false, true})
	{
		const FString Order = bControlFirst ? TEXT(" (control registered first)") : TEXT(" (screen registered first)");
		FReciTestHarness Harness(*this);
		if (!Harness.IsValid())
		{
			return false;
		}
		FReciTestCallLog& Log = Harness.GetLog();
		FReciTestRoutingEnvironment& Environment = Harness.GetEnvironment();
		// A registered screen whose actions take J and the stick, a Reci control in it that takes Down and Accept, and a
		// popup owner.
		UInputAction* Tap = Harness.CreateAction(TEXT("IA_Tap"));
		UInputAction* Stick = Harness.CreateAction(TEXT("IA_Stick"));
		UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
		Screen->SetInputMapping(Harness.CreateMapping({{Tap, EKeys::J}, {Stick, EKeys::Gamepad_LeftStick_Up}}));
		Screen->HandledActions = {Tap, Stick};
		UReciTestFocusTarget* List = Harness.CreateTarget(TEXT("List"), Screen);
		List->HandledDirections.Add(EUINavigation::Down);
		List->bHandlesAccept = true;
		UReciTestPopupOwner* Dropdown = Harness.CreatePopupOwner(TEXT("Dropdown"), Screen);
		Harness.ActivateScreen(*Screen);
		if (!bControlFirst)
		{
			List->Unregister();
		}
		FReciInputFocusCoordinator::Get().RegisterScreenInput(Screen);
		List->Register();
		Harness.Paint();
		TestTrue(TEXT("Focus reaches the control") + Order, Harness.Focus(*List));

		// (a) Under Game, Reci routes none of the user's keys or sticks: the stock path gets them, repeats and releases
		// included, and the open popup's Back with them.
		Environment.InputMode = ECommonInputMode::Game;
		for (const FKey& Key : {EKeys::J, EKeys::Down, EKeys::Enter, EKeys::Gamepad_LeftStick_Up})
		{
			TestFalse(FString::Printf(TEXT("Under Game, %s is not consumed"), *Key.ToString()) + Order, Harness.KeyDown(Key));
			TestFalse(FString::Printf(TEXT("Under Game, a repeat of %s is not consumed"), *Key.ToString()) + Order, Harness.KeyDown(Key, true));
			TestFalse(FString::Printf(TEXT("Under Game, the release of %s is not consumed"), *Key.ToString()) + Order, Harness.KeyUp(Key));
		}
		TestFalse(TEXT("Under Game, the screen's stick is not consumed") + Order, Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.9f));
		TestFalse(TEXT("Under Game, the stick's return is not consumed") + Order, Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.0f));
		Dropdown->Open(FReciTestHarness::UserIndex);
		TestFalse(TEXT("Under Game, Back is not consumed") + Order, Harness.KeyDown(EKeys::Escape));
		TestFalse(TEXT("Under Game, Back's release is not consumed") + Order, Harness.KeyUp(EKeys::Escape));
		TestTrue(TEXT("Under Game, Back leaves the popup open") + Order, Dropdown->IsOpen());
		TestEqual(TEXT("Under Game, no screen, control or popup is called") + Order, Log.ToString(), FString());

		// (b) Under Menu and under All, the usual order.
		for (const ECommonInputMode Mode : {ECommonInputMode::Menu, ECommonInputMode::All})
		{
			const FString ModeName = Mode == ECommonInputMode::Menu ? TEXT("Menu") : TEXT("All");
			Environment.InputMode = Mode;
			if (Mode == ECommonInputMode::Menu)
			{
				TestTrue(TEXT("Under Menu, Back closes the popup") + Order, Harness.KeyDown(EKeys::Escape));
				Harness.KeyUp(EKeys::Escape);
			}
			TestTrue(FString::Printf(TEXT("Under %s, the screen's action consumes its key"), *ModeName) + Order, Harness.KeyDown(EKeys::J));
			TestTrue(FString::Printf(TEXT("Under %s, its release is consumed"), *ModeName) + Order, Harness.KeyUp(EKeys::J));
			TestTrue(FString::Printf(TEXT("Under %s, the control consumes its direction"), *ModeName) + Order, Harness.KeyDown(EKeys::Down));
			Harness.KeyUp(EKeys::Down);
			TestTrue(FString::Printf(TEXT("Under %s, the screen's stick direction is consumed"), *ModeName) + Order,
				Harness.KeyDown(EKeys::Gamepad_LeftStick_Up));
			TestTrue(FString::Printf(TEXT("Under %s, the stick is consumed while its direction is held"), *ModeName) + Order,
				Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.9f));
			Harness.KeyUp(EKeys::Gamepad_LeftStick_Up);
			Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.0f);
			TestEqual(FString::Printf(TEXT("Under %s, the keys and the stick are routed"), *ModeName) + Order, Log.ToString(), FString(
				(Mode == ECommonInputMode::Menu ? TEXT("Dropdown Close, ") : TEXT(""))
				+ FString(TEXT("Screen Key J, Screen Action IA_Tap, Screen Key Down, List Navigate Down, List ReturnFocus, "
					"Screen Key Gamepad_LeftStick_Up, Screen Action IA_Stick, Screen Capturing"))));
			Log.Reset();
		}

		// (c) A press Reci took under Menu keeps its record when the mode changes while it is held: its repeats are still
		// blocked and its release still swallowed. A press under Game then is the stock path's again.
		Environment.InputMode = ECommonInputMode::Menu;
		TestTrue(TEXT("Under Menu, the control consumes Accept") + Order, Harness.KeyDown(EKeys::Enter));
		TestTrue(TEXT("Under Menu, the screen's action consumes its key") + Order, Harness.KeyDown(EKeys::J));
		Environment.InputMode = ECommonInputMode::Game;
		TestTrue(TEXT("A repeat of the Accept taken under Menu is still blocked") + Order, Harness.KeyDown(EKeys::Enter, true));
		TestTrue(TEXT("The release of the Accept taken under Menu is still swallowed") + Order, Harness.KeyUp(EKeys::Enter));
		TestTrue(TEXT("The release of the key taken under Menu is still swallowed") + Order, Harness.KeyUp(EKeys::J));
		TestFalse(TEXT("A new Accept under Game is not consumed") + Order, Harness.KeyDown(EKeys::Enter));
		TestFalse(TEXT("Its repeat is not consumed") + Order, Harness.KeyDown(EKeys::Enter, true));
		TestFalse(TEXT("Its release is not consumed") + Order, Harness.KeyUp(EKeys::Enter));
		TestEqual(TEXT("Only the presses under Menu reach a handler") + Order, Log.ToString(),
			FString(TEXT("Screen Key Enter, List Accept, List ReturnFocus, Screen Key J, Screen Action IA_Tap")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciPopupLayerRoutingTest, "ReciCommonUI.Input.Routing.PopupLayer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciPopupLayerRoutingTest::RunTest(const FString& Parameters)
{
	// The list beneath the popup and the popup's own list register in either order.
	for (const bool bPopupFirst : {false, true})
	{
		const FString Order = bPopupFirst ? TEXT(" (popup registered first)") : TEXT(" (list registered first)");
		FReciTestHarness Harness(*this);
		if (!Harness.IsValid())
		{
			return false;
		}
		FReciTestCallLog& Log = Harness.GetLog();
		// A registered screen whose actions take Left, Up and the stick, and a Reci list in it that holds a dropdown, as a
		// list holds a dropdown row. The dropdown's popup is a Reci control created with the dropdown as its outer and
		// hosted under it, so focus in the popup is focus in the list and in the screen too.
		UInputAction* Left = Harness.CreateAction(TEXT("IA_Left"));
		UInputAction* Up = Harness.CreateAction(TEXT("IA_Up"));
		UInputAction* Stick = Harness.CreateAction(TEXT("IA_Stick"));
		UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
		Screen->SetInputMapping(Harness.CreateMapping({{Left, EKeys::Left}, {Up, EKeys::Up}, {Stick, EKeys::Gamepad_LeftStick_Up}}));
		Screen->HandledActions = {Left, Up, Stick};
		UReciTestFocusTarget* List = Harness.CreateTarget(TEXT("List"), Screen);
		List->HandledDirections = {EUINavigation::Left, EUINavigation::Up};
		auto* Dropdown = List->WidgetTree->ConstructWidget<UReciTestPopupOwner>(UReciTestPopupOwner::StaticClass(), TEXT("Dropdown"));
		Dropdown->Log = List->Log;
		List->SetContent(Dropdown);
		Harness.ActivateScreen(*Screen);
		FReciInputFocusCoordinator::Get().RegisterScreenInput(Screen);
		UReciTestFocusTarget* Popup = Dropdown->Open(FReciTestHarness::UserIndex);
		Popup->HandledDirections.Add(EUINavigation::Up);
		if (bPopupFirst)
		{
			List->Unregister();
			List->Register();
		}
		Harness.Paint();
		TestTrue(TEXT("The popup belongs to the dropdown") + Order, Popup->IsIn(Dropdown));
		TestTrue(TEXT("Focus reaches the popup") + Order, Harness.Focus(*Popup));
		TestTrue(TEXT("The list beneath holds the popup's focus in its tree") + Order, List->CanHandleReciFocusedInput());

		// The popup is its user's top layer: the screen gets none of the keys or the stick, and only the popup's list is
		// offered focused keys. What it declines goes on to the stock path.
		TestFalse(TEXT("A direction the popup declines is not consumed") + Order, Harness.KeyDown(EKeys::Left));
		Harness.KeyUp(EKeys::Left);
		TestTrue(TEXT("The popup's list consumes Up") + Order, Harness.KeyDown(EKeys::Up));
		Harness.KeyUp(EKeys::Up);
		TestFalse(TEXT("The stick-direction key the screen uses is not consumed") + Order, Harness.KeyDown(EKeys::Gamepad_LeftStick_Up));
		Harness.KeyUp(EKeys::Gamepad_LeftStick_Up);
		TestTrue(TEXT("The popup's list takes the stick's direction") + Order, Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.9f));
		TestFalse(TEXT("The stick's return is not consumed") + Order, Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.0f));
		TestEqual(TEXT("Only the popup's list is offered keys while it is open") + Order, Log.ToString(), FString(TEXT(
			"DropdownPopup Navigate Left, DropdownPopup Navigate Up, DropdownPopup ReturnFocus, "
			"DropdownPopup Navigate Up, DropdownPopup ReturnFocus")));
		Log.Reset();

		// Back closes it once.
		TestTrue(TEXT("Back is consumed") + Order, Harness.KeyDown(EKeys::Escape));
		Harness.KeyUp(EKeys::Escape);
		TestEqual(TEXT("Back closes the popup, once") + Order, Log.ToString(), FString(TEXT("Dropdown Close")));
		TestFalse(TEXT("The popup is closed") + Order, Dropdown->IsOpen());
		Log.Reset();

		// Closed, the screen takes its keys again.
		TestTrue(TEXT("Focus returns to the list") + Order, Harness.Focus(*List));
		TestTrue(TEXT("The screen's action consumes Left") + Order, Harness.KeyDown(EKeys::Left));
		Harness.KeyUp(EKeys::Left);
		TestTrue(TEXT("The screen's action consumes Up") + Order, Harness.KeyDown(EKeys::Up));
		Harness.KeyUp(EKeys::Up);
		TestEqual(TEXT("With the popup closed, the screen takes its keys") + Order, Log.ToString(),
			FString(TEXT("Screen Key Left, Screen Action IA_Left, Screen Key Up, Screen Action IA_Up")));
	}
	return true;
}

namespace
{
	/**
	 * What a repeat case works with: a registered screen with actions on Enter, Left and J, a Reci list and two popup
	 * owners in it, a screen of its own layer that takes no screen input and holds no Reci control, and a Reci control
	 * that no activatable widget holds. No handler takes anything until a case says so.
	 */
	struct FReciRepeatScene
	{
		explicit FReciRepeatScene(FReciTestHarness& Harness)
		{
			Confirm = Harness.CreateAction(TEXT("IA_Confirm"));
			Left = Harness.CreateAction(TEXT("IA_Left"));
			Tap = Harness.CreateAction(TEXT("IA_Tap"));
			Screen = Harness.CreateScreen(TEXT("Screen"));
			Screen->SetInputMapping(Harness.CreateMapping({{Confirm, EKeys::Enter}, {Left, EKeys::Left}, {Tap, EKeys::J}}));
			List = Harness.CreateTarget(TEXT("List"), Screen);
			Lower = Harness.CreatePopupOwner(TEXT("Lower"), Screen);
			Upper = Harness.CreatePopupOwner(TEXT("Upper"), Screen);
			Plain = Harness.CreateScreen(TEXT("Plain"));
			Loose = Harness.CreateTarget(TEXT("Loose"), nullptr);
			Harness.ActivateScreen(*Screen);
			Harness.ActivateScreen(*Plain);
			FReciInputFocusCoordinator::Get().RegisterScreenInput(Screen);
			Harness.Paint();
		}

		UInputAction* Confirm = nullptr;
		UInputAction* Left = nullptr;
		UInputAction* Tap = nullptr;
		UReciTestScreen* Screen = nullptr;
		UReciTestFocusTarget* List = nullptr;
		UReciTestPopupOwner* Lower = nullptr;
		UReciTestPopupOwner* Upper = nullptr;
		UReciTestScreen* Plain = nullptr;
		UReciTestFocusTarget* Loose = nullptr;
	};

	/** A press of Key, three repeats and the release: which of them Reci consumes, and the calls they make. */
	struct FReciRepeatCase
	{
		const TCHAR* Name;
		FKey Key;
		/** Sets the handlers and the focus before the press. */
		TFunction<void(FReciTestHarness&, FReciRepeatScene&)> Arrange;
		/** Runs after the press, before the repeats. */
		TFunction<void(FReciTestHarness&, FReciRepeatScene&)> WhileHeld;
		bool bPressConsumed = false;
		bool bRepeatsConsumed = false;
		bool bReleaseConsumed = false;
		const TCHAR* Calls = TEXT("");
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciRepeatsRoutingTest, "ReciCommonUI.Input.Routing.Repeats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciRepeatsRoutingTest::RunTest(const FString& Parameters)
{
	using FScene = FReciRepeatScene;
	const auto Nothing = [](FReciTestHarness&, FScene&) {};
	const FReciRepeatCase Cases[] = {
		{TEXT("Back that closes a popup closes one, with its repeats"), EKeys::Escape,
			[](FReciTestHarness& Harness, FScene& Scene)
			{
				Scene.Lower->Open(FReciTestHarness::UserIndex);
				Harness.Focus(*Scene.Upper->Open(FReciTestHarness::UserIndex));
			},
			Nothing, true, true, true, TEXT("Upper Close")},
		{TEXT("A key the screen's key capture took repeats only into its key capture, even once it declines"), EKeys::J,
			[](FReciTestHarness& Harness, FScene& Scene)
			{
				Scene.Screen->HandledKeys = {EKeys::J};
				Scene.Screen->HandledActions = {Scene.Tap};
				Harness.Focus(*Scene.Screen);
			},
			[](FReciTestHarness&, FScene& Scene) { Scene.Screen->HandledKeys.Reset(); },
			true, true, true, TEXT("Screen Key J, Screen Key J repeat, Screen Key J repeat, Screen Key J repeat")},
		{TEXT("A screen action on Accept is called once"), EKeys::Enter,
			[](FReciTestHarness& Harness, FScene& Scene)
			{
				Scene.Screen->HandledActions = {Scene.Confirm};
				Scene.List->bHandlesAccept = true;
				Harness.Focus(*Scene.List);
			},
			Nothing, true, true, true, TEXT("Screen Key Enter, Screen Action IA_Confirm")},
		{TEXT("A screen action on Left gets its repeats"), EKeys::Left,
			[](FReciTestHarness& Harness, FScene& Scene)
			{
				Scene.Screen->HandledActions = {Scene.Left};
				Harness.Focus(*Scene.Screen);
			},
			Nothing, true, true, true,
			TEXT("Screen Key Left, Screen Action IA_Left, Screen Action IA_Left repeat, Screen Action IA_Left repeat, Screen Action IA_Left repeat")},
		{TEXT("A focused control's Accept is called once"), EKeys::Enter,
			[](FReciTestHarness& Harness, FScene& Scene)
			{
				Scene.List->bHandlesAccept = true;
				Harness.Focus(*Scene.List);
			},
			Nothing, true, true, true, TEXT("Screen Key Enter, Screen Action IA_Confirm, List Accept, List ReturnFocus")},
		{TEXT("Accept is called once on a focused control that no activatable widget holds"), EKeys::Enter,
			[](FReciTestHarness& Harness, FScene& Scene)
			{
				Scene.Loose->bHandlesAccept = true;
				Harness.Focus(*Scene.Loose);
			},
			Nothing, true, true, true, TEXT("Loose Accept, Loose ReturnFocus")},
		{TEXT("A focused control's direction gets its repeats, and its release is swallowed"), EKeys::Down,
			[](FReciTestHarness& Harness, FScene& Scene)
			{
				Scene.List->HandledDirections = {EUINavigation::Down};
				Harness.Focus(*Scene.List);
			},
			Nothing, true, true, true,
			TEXT("Screen Key Down, List Navigate Down, List ReturnFocus, List Navigate Down, List ReturnFocus, "
				"List Navigate Down, List ReturnFocus, List Navigate Down, List ReturnFocus")},
		{TEXT("Repeats the focused control declines at its end are consumed"), EKeys::Down,
			[](FReciTestHarness& Harness, FScene& Scene)
			{
				Scene.List->HandledDirections = {EUINavigation::Down};
				Harness.Focus(*Scene.List);
			},
			[](FReciTestHarness&, FScene& Scene) { Scene.List->HandledDirections.Reset(); },
			true, true, true,
			TEXT("Screen Key Down, List Navigate Down, List ReturnFocus, List Navigate Down, List Navigate Down, List Navigate Down")},
		{TEXT("The stock path's Accept outside a Reci layer passes, with its repeats and release"), EKeys::Enter,
			[](FReciTestHarness& Harness, FScene& Scene) { Harness.Focus(*Scene.Plain); },
			Nothing, false, false, false, TEXT("")},
		{TEXT("The stock path's Accept in a registered screen has its repeats blocked"), EKeys::Enter,
			[](FReciTestHarness& Harness, FScene& Scene) { Harness.Focus(*Scene.Screen); },
			Nothing, false, true, false, TEXT("Screen Key Enter, Screen Action IA_Confirm")},
		{TEXT("The stock path's Accept in a registered screen has its release swallowed once focus moves"), EKeys::Enter,
			[](FReciTestHarness& Harness, FScene& Scene) { Harness.Focus(*Scene.Screen); },
			[](FReciTestHarness& Harness, FScene& Scene) { Harness.Focus(*Scene.List); },
			false, true, true, TEXT("Screen Key Enter, Screen Action IA_Confirm")},
		{TEXT("The stock path's Back in a registered screen has its repeats blocked"), EKeys::Escape,
			[](FReciTestHarness& Harness, FScene& Scene) { Harness.Focus(*Scene.Screen); },
			Nothing, false, true, false, TEXT("Screen Key Escape")},
	};

	for (const FReciRepeatCase& Case : Cases)
	{
		const FString Name = FString(TEXT(" (")) + Case.Name + TEXT(")");
		FReciTestHarness Harness(*this);
		if (!Harness.IsValid())
		{
			return false;
		}
		FScene Scene(Harness);
		Case.Arrange(Harness, Scene);
		TestEqual(TEXT("The press") + Name, Harness.KeyDown(Case.Key), Case.bPressConsumed);
		Case.WhileHeld(Harness, Scene);
		for (int32 Repeat = 1; Repeat <= 3; ++Repeat)
		{
			TestEqual(FString::Printf(TEXT("Repeat %d"), Repeat) + Name, Harness.KeyDown(Case.Key, true), Case.bRepeatsConsumed);
		}
		TestEqual(TEXT("The release") + Name, Harness.KeyUp(Case.Key), Case.bReleaseConsumed);
		TestEqual(TEXT("The calls") + Name, Harness.GetLog().ToString(), FString(Case.Calls));
		if (Case.Key == EKeys::Escape && Case.bPressConsumed)
		{
			TestTrue(TEXT("The lower popup stays open") + Name, Scene.Lower->IsOpen());
		}
	}

	// A screen action's repeats go to that screen alone: once it unregisters and another registered screen takes focus,
	// they are consumed and the new screen is never called. A new press goes to the new screen.
	{
		FReciTestHarness Harness(*this);
		if (!Harness.IsValid())
		{
			return false;
		}
		FReciTestCallLog& Log = Harness.GetLog();
		FScene Scene(Harness);
		Scene.Screen->HandledActions = {Scene.Left};
		UInputAction* NextLeft = Harness.CreateAction(TEXT("IA_NextLeft"));
		UReciTestScreen* Next = Harness.CreateScreen(TEXT("Next"));
		Next->SetInputMapping(Harness.CreateMapping({{NextLeft, EKeys::Left}}));
		Next->HandledActions = {NextLeft};
		Harness.ActivateScreen(*Next);
		FReciInputFocusCoordinator::Get().RegisterScreenInput(Next);
		TestTrue(TEXT("Focus reaches the screen"), Harness.Focus(*Scene.Screen));
		TestTrue(TEXT("The screen's action takes Left"), Harness.KeyDown(EKeys::Left));
		TestTrue(TEXT("Its repeat is consumed"), Harness.KeyDown(EKeys::Left, true));
		FReciInputFocusCoordinator::Get().UnregisterScreenInput(Scene.Screen);
		Harness.DeactivateScreen(*Scene.Screen);
		TestTrue(TEXT("Focus reaches the next screen"), Harness.Focus(*Next));
		TestTrue(TEXT("A repeat after the screen changed is consumed"), Harness.KeyDown(EKeys::Left, true));
		TestTrue(TEXT("So is the next one"), Harness.KeyDown(EKeys::Left, true));
		TestTrue(TEXT("The release is swallowed"), Harness.KeyUp(EKeys::Left));
		TestEqual(TEXT("The repeats reach only the screen that took the press"), Log.ToString(),
			FString(TEXT("Screen Key Left, Screen Action IA_Left, Screen Action IA_Left repeat")));
		Log.Reset();
		TestTrue(TEXT("A new press goes to the next screen"), Harness.KeyDown(EKeys::Left));
		Harness.KeyUp(EKeys::Left);
		TestEqual(TEXT("The next screen takes a new press"), Log.ToString(), FString(TEXT("Next Key Left, Next Action IA_NextLeft")));
	}

	// A press whose release was lost (the window lost focus, say) leaves nothing behind: the next press of the key starts
	// clean, and here goes to the stock path with its repeats and release.
	{
		FReciTestHarness Harness(*this);
		if (!Harness.IsValid())
		{
			return false;
		}
		FScene Scene(Harness);
		Scene.List->bHandlesAccept = true;
		TestTrue(TEXT("Focus reaches the list"), Harness.Focus(*Scene.List));
		TestTrue(TEXT("The list takes Accept, whose release is then lost"), Harness.KeyDown(EKeys::Enter));
		TestTrue(TEXT("Focus reaches the plain screen"), Harness.Focus(*Scene.Plain));
		TestFalse(TEXT("A new press outside a Reci layer is not consumed"), Harness.KeyDown(EKeys::Enter));
		TestFalse(TEXT("Its repeat is not consumed"), Harness.KeyDown(EKeys::Enter, true));
		TestFalse(TEXT("Its release is not consumed"), Harness.KeyUp(EKeys::Enter));
		TestEqual(TEXT("Only the first press reaches a handler"), Harness.GetLog().ToString(),
			FString(TEXT("Screen Key Enter, Screen Action IA_Confirm, List Accept, List ReturnFocus")));
	}
	return true;
}

#endif
