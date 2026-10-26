// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#if WITH_DEV_AUTOMATION_TESTS

#include "Input/ReciScreenInputMapping.h"
#include "Input/ReciScreenInputReports.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "Misc/AutomationTest.h"
#include "ReciCommonUIBlueprintLibrary.h"
#include "Tests/ReciInteractionTestEntry.h"
#include "Tests/ReciTestHarness.h"
#include "Tests/ReciTestWidgets.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/ReciListLayoutBox.h"
#include "Widgets/ReciMappedKeyText.h"
#include "Widgets/ReciTextListItem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciScreenInputActionsFromInputMappingTest, "ReciCommonUI.ScreenInput.ActionsFromInputMapping",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FReciScreenInputActionsFromInputMappingTest::RunTest(const FString& Parameters)
{
	using UE::ReciCommonUI::Private::GetScreenActions;
	using UE::ReciCommonUI::Private::GetScreenInputMapping;

	// Transient objects that nothing else sees: the test changes no global, so it has nothing to put back. Each screen
	// has a unique name, and its actions and mapping are named inside it, so a second run makes new objects.
	const TStrongObjectPtr<UReciTestScreen> Screen(NewObject<UReciTestScreen>(GetTransientPackage()));
	const TStrongObjectPtr<UReciTestScreen> Unmapped(NewObject<UReciTestScreen>(GetTransientPackage()));
	UInputAction* Browse = NewObject<UInputAction>(Screen.Get(), TEXT("IA_Browse"));
	UInputAction* Back = NewObject<UInputAction>(Screen.Get(), TEXT("IA_Back"));
	// Back is mapped first and twice, and one row maps no action.
	UInputMappingContext* Mapping = NewObject<UInputMappingContext>(Screen.Get(), TEXT("IMC_Screen"));
	Mapping->MapKey(Back, EKeys::Escape);
	Mapping->MapKey(nullptr, EKeys::Tab);
	Mapping->MapKey(Browse, EKeys::Down);
	Mapping->MapKey(Back, EKeys::Gamepad_FaceButton_Right);
	Screen->SetInputMapping(Mapping);

	TestTrue(TEXT("The screen's Input Mapping is read"), GetScreenInputMapping(*Screen) == Mapping);
	const TArray<UInputAction*> Actions = GetScreenActions(*Screen);
	const FString ActionNames = FString::JoinBy(Actions, TEXT(", "), [](const UInputAction* Action) { return GetNameSafe(Action); });
	TestEqual(TEXT("Each action comes once, in mapping order"), ActionNames, FString(TEXT("IA_Back, IA_Browse")));
	TestTrue(TEXT("The actions are the mapped objects"), Actions == TArray<UInputAction*>{Back, Browse});

	TestNull(TEXT("A screen with no Input Mapping has none"), GetScreenInputMapping(*Unmapped));
	TestTrue(TEXT("A screen with no Input Mapping has no actions"), GetScreenActions(*Unmapped).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciScreenInputActionsTest, "ReciCommonUI.ScreenInput.Actions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciScreenInputActionsTest::RunTest(const FString& Parameters)
{
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	FReciTestCallLog& Log = Harness.GetLog();
	// Two actions on Down, mapped in the reverse order of their names, the first also on the D-pad; one action on Up;
	// and a Reci control on the screen that takes Down.
	UInputAction* Scroll = Harness.CreateAction(TEXT("IA_Scroll"));
	UInputAction* Browse = Harness.CreateAction(TEXT("IA_Browse"));
	UInputAction* Rise = Harness.CreateAction(TEXT("IA_Rise"));
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	Screen->SetInputMapping(Harness.CreateMapping(
		{{Scroll, EKeys::Down}, {Scroll, EKeys::Gamepad_DPad_Down}, {Browse, EKeys::Down}, {Rise, EKeys::Up}}));
	UReciTestFocusTarget* List = Harness.CreateTarget(TEXT("List"), Screen);
	List->HandledDirections.Add(EUINavigation::Down);
	Harness.ActivateScreen(*Screen);
	FReciInputFocusCoordinator::Get().RegisterScreenInput(Screen);
	Harness.Paint();
	TestTrue(TEXT("Focus reaches the list"), Harness.Focus(*List));

	// No action takes Down: each action on it is offered it once, in mapping order, and then the focused control.
	TestTrue(TEXT("The focused list takes Down after the actions decline it"), Harness.KeyDown(EKeys::Down));
	Harness.KeyUp(EKeys::Down);
	TestEqual(TEXT("Every action on the key, in mapping order, then the focused control"), Log.ToString(),
		FString(TEXT("Screen Key Down, Screen Action IA_Scroll, Screen Action IA_Browse, List Navigate Down, List ReturnFocus")));
	Log.Reset();

	// An action that takes the key stops it there, with its release.
	Screen->HandledActions = {Scroll};
	TestTrue(TEXT("An action that takes its key consumes it"), Harness.KeyDown(EKeys::Down));
	TestTrue(TEXT("An action that takes its key consumes its release"), Harness.KeyUp(EKeys::Down));
	TestEqual(TEXT("The key stops at the first action that takes it"), Log.ToString(), FString(TEXT("Screen Key Down, Screen Action IA_Scroll")));
	Log.Reset();
	Screen->HandledActions = {Browse};
	TestTrue(TEXT("A later action that takes the key consumes it"), Harness.KeyDown(EKeys::Down));
	Harness.KeyUp(EKeys::Down);
	TestEqual(TEXT("A declining action passes the key to the next"), Log.ToString(),
		FString(TEXT("Screen Key Down, Screen Action IA_Scroll, Screen Action IA_Browse")));
	Log.Reset();

	// Each action is offered only its own keys.
	Screen->HandledActions.Reset();
	TestTrue(TEXT("The focused list takes the D-pad's Down after the action declines it"), Harness.KeyDown(EKeys::Gamepad_DPad_Down));
	Harness.KeyUp(EKeys::Gamepad_DPad_Down);
	TestEqual(TEXT("Only the action on the D-pad is offered it"), Log.ToString(),
		FString(TEXT("Screen Key Gamepad_DPad_Down, Screen Action IA_Scroll, List Navigate Down, List ReturnFocus")));
	Log.Reset();

	// Neither the action nor the focused control takes Up, so the stock path gets the key and its release.
	TestFalse(TEXT("A key no handler takes is not consumed"), Harness.KeyDown(EKeys::Up));
	TestFalse(TEXT("The release of a key no handler took is not consumed"), Harness.KeyUp(EKeys::Up));
	TestEqual(TEXT("A declined action passes the key on to the focused control"), Log.ToString(),
		FString(TEXT("Screen Key Up, Screen Action IA_Rise, List Navigate Up")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciScreenInputIgnoredSetupReportedTest, "ReciCommonUI.ScreenInput.IgnoredSetupReported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciScreenInputIgnoredSetupReportedTest::RunTest(const FString& Parameters)
{
	FReciScopedCoordinatorState CoordinatorState;
	FReciScopedReports Reports;
	const TStrongObjectPtr<UReciTestScreen> Screen(NewObject<UReciTestScreen>(GetTransientPackage()));
	// One mapping with a trigger, one action with a modifier of its own, one action with neither; none on a direction.
	UInputAction* Tap = NewObject<UInputAction>(Screen.Get(), TEXT("IA_Tap"));
	UInputAction* Steer = NewObject<UInputAction>(Screen.Get(), TEXT("IA_Steer"));
	UInputAction* Plain = NewObject<UInputAction>(Screen.Get(), TEXT("IA_Plain"));
	Steer->Modifiers.Add(NewObject<UInputModifierNegate>(Steer));
	UInputMappingContext* Mapping = NewObject<UInputMappingContext>(Screen.Get(), TEXT("IMC_Setup"));
	Mapping->MapKey(Tap, EKeys::Q).Triggers.Add(NewObject<UInputTriggerPressed>(Mapping));
	Mapping->MapKey(Tap, EKeys::Gamepad_FaceButton_Bottom);
	Mapping->MapKey(Steer, EKeys::E);
	Mapping->MapKey(Plain, EKeys::R);
	Screen->SetInputMapping(Mapping);

	// One warning per offending pair (a log line counts toward one expected message), none for IA_Plain.
	AddExpectedMessagePlain(TEXT("ignores the triggers and modifiers of IA_Tap in IMC_Setup"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 1);
	AddExpectedMessagePlain(TEXT("ignores the triggers and modifiers of IA_Steer in IMC_Setup"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 1);
	FReciInputFocusCoordinator::Get().RegisterScreenInput(Screen.Get());
	FReciInputFocusCoordinator::Get().RegisterScreenInput(Screen.Get());
	TestTrue(TEXT("Registration still registers the screen"), FReciInputFocusCoordinator::Get().IsScreenInputRegistered(*Screen));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciScreenInputRegisterWithoutInterfaceReportedTest,
	"ReciCommonUI.ScreenInput.RegisterWithoutInterfaceReported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciScreenInputRegisterWithoutInterfaceReportedTest::RunTest(const FString& Parameters)
{
	FReciScopedCoordinatorState CoordinatorState;
	FReciScopedReports Reports;
	const TStrongObjectPtr<UReciTestPlainScreen> Plain(NewObject<UReciTestPlainScreen>(GetTransientPackage()));

	AddExpectedMessagePlain(TEXT("does not implement Reci Screen Input; Register Screen Input ignores it"),
		ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	AddExpectedMessagePlain(TEXT("Register Screen Input was called with no screen"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 1);
	FReciInputFocusCoordinator::Get().RegisterScreenInput(Plain.Get());
	FReciInputFocusCoordinator::Get().RegisterScreenInput(Plain.Get());
	TestEqual(TEXT("A widget without the interface is not registered"), FReciInputFocusCoordinatorTestAccess::GetScreenCount(), 0);
	TestFalse(TEXT("A widget without the interface is not routed"), FReciInputFocusCoordinator::Get().IsScreenInputRegistered(*Plain));
	FReciInputFocusCoordinator::Get().RegisterScreenInput(nullptr);
	FReciInputFocusCoordinator::Get().RegisterScreenInput(nullptr);
	TestEqual(TEXT("No screen is not registered"), FReciInputFocusCoordinatorTestAccess::GetScreenCount(), 0);
	return true;
}

#if !UE_BUILD_SHIPPING
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciScreenInputUnregisteredScreenReportedTest, "ReciCommonUI.ScreenInput.UnregisteredScreenReported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciScreenInputUnregisteredScreenReportedTest::RunTest(const FString& Parameters)
{
	using UE::ReciCommonUI::Private::RunUnregisteredScreenChecksForTests;

	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	// Built (CommonUI's rebuild event) and then activated, as a screen pushed onto a stack is; it never registers.
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Unregistered"));
	AddExpectedMessagePlain(TEXT("implements Reci Screen Input but did not call Register Screen Input"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 1);
	Harness.ActivateScreen(*Screen);
	RunUnregisteredScreenChecksForTests();
	// Once per class: activating it again reports nothing more.
	Harness.DeactivateScreen(*Screen);
	Harness.ActivateScreen(*Screen);
	RunUnregisteredScreenChecksForTests();

	// A screen of another class that registers when it activates is not reported.
	UReciTestScreen* Registering = Harness.CreateScreen(TEXT("Registering"), nullptr, UReciTestRegisteringScreen::StaticClass());
	Harness.ActivateScreen(*Registering);
	TestTrue(TEXT("The screen registered itself"), FReciInputFocusCoordinator::Get().IsScreenInputRegistered(*Registering));
	RunUnregisteredScreenChecksForTests();
	// Nor is one that registers after it activates and before the next tick, as a handler of On Widget Activated does.
	{
		FReciScopedReports FreshReports;
		Harness.DeactivateScreen(*Screen);
		Harness.ActivateScreen(*Screen);
		FReciInputFocusCoordinator::Get().RegisterScreenInput(Screen);
		RunUnregisteredScreenChecksForTests();
	}
	return true;
}
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciScreenInputReachabilityTest, "ReciCommonUI.ScreenInput.Reachability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciScreenInputReachabilityTest::RunTest(const FString& Parameters)
{
	using UE::ReciCommonUI::Private::CheckScreenReachability;

	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	UInputAction* Move = Harness.CreateAction(TEXT("IA_Move"));
	UInputAction* KeyboardOnly = Harness.CreateAction(TEXT("IA_KeyboardOnly"));
	UInputAction* Shown = Harness.CreateAction(TEXT("IA_Shown"));
	UInputAction* ButtonKey = Harness.CreateAction(TEXT("IA_ButtonKey"));
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));

	// A screen whose Input Mapping takes no direction is not checked.
	Screen->SetInputMapping(Harness.CreateMapping({{KeyboardOnly, EKeys::Q}}));
	FReciScreenReachability Result = CheckScreenReachability(*Screen, Harness.GetEnvironment());
	TestFalse(TEXT("A screen without directional keys does not take them"), Result.bTakesDirectionalKeys);
	TestTrue(TEXT("A screen without directional keys is not checked"),
		Result.ControlsWithoutKey.IsEmpty() && Result.ActionsMissingDevice.IsEmpty() && Result.ActionsWithoutHint.IsEmpty());

	// Arrow Up navigates by the player's navigation config, so the screen takes directional keys.
	Screen->SetInputMapping(Harness.CreateMapping({
		{Move, EKeys::Up}, {Move, EKeys::Gamepad_DPad_Up},
		{KeyboardOnly, EKeys::Q},
		{Shown, EKeys::E}, {Shown, EKeys::Gamepad_FaceButton_Left},
		{ButtonKey, EKeys::R}, {ButtonKey, EKeys::Gamepad_FaceButton_Top}}));
	UOverlay* Content = Screen->GetContent();
	// A button with no key of its own.
	UReciTestButton* Unkeyed = Screen->WidgetTree->ConstructWidget<UReciTestButton>(UReciTestButton::StaticClass(), TEXT("Unkeyed"));
	Content->AddChild(Unkeyed);
	// A Reci button whose Hint Input Action has a key on both devices, shown by its Input Action Widget.
	UReciTestHintedButton* Hinted =
		Screen->WidgetTree->ConstructWidget<UReciTestHintedButton>(UReciTestHintedButton::StaticClass(), TEXT("Hinted"));
	Hinted->SetHintInputAction(ButtonKey);
	Content->AddChild(Hinted);
	// A key text in the screen's hint line.
	UReciMappedKeyText* HintLine =
		Screen->WidgetTree->ConstructWidget<UReciMappedKeyText>(UReciMappedKeyText::StaticClass(), TEXT("HintLine"));
	HintLine->SetEnhancedInputActions({Shown});
	Content->AddChild(HintLine);
	// Entries of a Reci list, and a button of an activatable widget nested in the screen: neither is checked here.
	UReciVerticalBox* ListBox = Screen->WidgetTree->ConstructWidget<UReciVerticalBox>(UReciVerticalBox::StaticClass(), TEXT("ListBox"));
	UReciInteractionTestList* List =
		Screen->WidgetTree->ConstructWidget<UReciInteractionTestList>(UReciInteractionTestList::StaticClass(), TEXT("List"));
	List->ItemWidgetClass = UReciInteractionTestEntry::StaticClass();
	ListBox->SetContent(List);
	Content->AddChild(ListBox);
	List->SetListItems({NewObject<UReciTextListItemData>(List), NewObject<UReciTextListItemData>(List)});
	UReciTestScreen* Nested = Harness.CreateScreen(TEXT("Nested"), Screen);
	Nested->GetContent()->AddChild(Nested->WidgetTree->ConstructWidget<UReciTestButton>(UReciTestButton::StaticClass(), TEXT("NestedButton")));
	TestEqual(TEXT("The list holds entries"), List->GetItemCount(), 2);

	Result = CheckScreenReachability(*Screen, Harness.GetEnvironment());
	TestTrue(TEXT("Arrow Up makes the screen take directional keys"), Result.bTakesDirectionalKeys);
	TestTrue(TEXT("Only the button with no key of its own lacks one"), Result.ControlsWithoutKey == TArray<TObjectPtr<UWidget>>{Unkeyed});
	TestTrue(TEXT("Only the keyboard-only action lacks a device"),
		Result.ActionsMissingDevice == TArray<TObjectPtr<UInputAction>>{KeyboardOnly});
	TestTrue(TEXT("An action a key text or a hinted Reci button shows has a hint"),
		Result.ActionsWithoutHint == (TArray<TObjectPtr<UInputAction>>{Move, KeyboardOnly}));

	// Without a player, Slate's own navigation config decides what a direction is.
	const TStrongObjectPtr<UReciTestScreen> Unowned(NewObject<UReciTestScreen>(GetTransientPackage()));
	Unowned->SetInputMapping(Harness.CreateMapping({{Move, EKeys::Down}}));
	TestTrue(TEXT("A screen without a player is checked with Slate's navigation config"),
		CheckScreenReachability(*Unowned, Harness.GetEnvironment()).bTakesDirectionalKeys);
	TestFalse(TEXT("The query takes no screen"), UReciScreenInputLibrary::CheckScreenReachability(nullptr).bTakesDirectionalKeys);

	// Registration reports the screen class once, naming what it found.
	AddExpectedMessagePlain(TEXT("Check Screen Reachability). Controls without their own hinted key on keyboard and gamepad: Unkeyed."),
		ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	FReciInputFocusCoordinator::Get().RegisterScreenInput(Screen);
	FReciInputFocusCoordinator::Get().RegisterScreenInput(Screen);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciScreenInputSuspendedTest, "ReciCommonUI.ScreenInput.Suspended",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciScreenInputSuspendedTest::RunTest(const FString& Parameters)
{
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	FReciTestCallLog& Log = Harness.GetLog();
	FReciInputFocusCoordinator& Coordinator = FReciInputFocusCoordinator::Get();
	int32 ResolverCalls = 0;
	Coordinator.SetEventTimeResolver([&Log, &ResolverCalls](const FInputEvent& Event, const FKey& Key, const bool bIsRepeat)
	{
		++ResolverCalls;
		Log.Calls.Add(TEXT("Resolver ") + Key.ToString());
		return 0.0;
	});
	// A screen whose actions take Down and J, and a focused list that takes Down too.
	UInputAction* Browse = Harness.CreateAction(TEXT("IA_Browse"));
	UInputAction* Tap = Harness.CreateAction(TEXT("IA_Tap"));
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	Screen->SetInputMapping(Harness.CreateMapping({{Browse, EKeys::Down}, {Tap, EKeys::J}}));
	Screen->HandledActions = {Browse, Tap};
	Screen->SuspendedActions = {Browse};
	UReciTestFocusTarget* List = Harness.CreateTarget(TEXT("List"), Screen);
	List->HandledDirections.Add(EUINavigation::Down);
	UReciTestPopupOwner* Dropdown = Harness.CreatePopupOwner(TEXT("Dropdown"), Screen);
	Harness.ActivateScreen(*Screen);
	Coordinator.RegisterScreenInput(Screen);
	Harness.Paint();
	TestTrue(TEXT("Focus reaches the list"), Harness.Focus(*List));

	// A suspended action is offered no key: its key goes on to focused navigation, and is not timed for it.
	TestTrue(TEXT("The list takes Down while the screen suspends its action"), Harness.KeyDown(EKeys::Down));
	Harness.KeyUp(EKeys::Down);
	TestEqual(TEXT("The suspended action and the resolver are skipped"), Log.ToString(),
		FString(TEXT("Screen Key Down, List Navigate Down, List ReturnFocus")));
	Log.Reset();
	Screen->SuspendedActions.Reset();
	TestTrue(TEXT("The action takes Down again once the screen stops suspending it"), Harness.KeyDown(EKeys::Down));
	Harness.KeyUp(EKeys::Down);
	TestEqual(TEXT("The action gets the key, timed, before the list"), Log.ToString(),
		FString(TEXT("Screen Key Down, Resolver Down, Screen Action IA_Browse")));
	TestEqual(TEXT("The resolver ran only for the action that took the key"), ResolverCalls, 1);

	// What a key hint reads from the routing now.
	const int32 User = FReciTestHarness::UserIndex;
	TestTrue(TEXT("A screen action takes its key before CommonUI's bindings"),
		Coordinator.IsKeyTakenBefore(User, EKeys::J, EReciKeyHintStage::Stock, nullptr));
	TestFalse(TEXT("The action's own hint keeps its key"),
		Coordinator.IsKeyTakenBefore(User, EKeys::J, EReciKeyHintStage::ScreenAction, Tap));
	Screen->SuspendedActions = {Tap};
	TestFalse(TEXT("A suspended action takes no key"), Coordinator.IsKeyTakenBefore(User, EKeys::J, EReciKeyHintStage::Stock, nullptr));
	TestTrue(TEXT("A suspended action's own hint shows none of its keys"),
		Coordinator.IsKeyTakenBefore(User, EKeys::J, EReciKeyHintStage::ScreenAction, Tap));
	Screen->SuspendedActions.Reset();
	Screen->bCapturingKeys = true;
	TestTrue(TEXT("A capturing screen takes every key"), Coordinator.IsKeyTakenBefore(User, EKeys::L, EReciKeyHintStage::FocusedControl, nullptr));
	Screen->bCapturingKeys = false;
	TestTrue(TEXT("The focused list takes a direction before CommonUI's bindings"),
		Coordinator.IsKeyTakenBefore(User, EKeys::Up, EReciKeyHintStage::Stock, nullptr));
	TestFalse(TEXT("Not before the focused control"), Coordinator.IsKeyTakenBefore(User, EKeys::Up, EReciKeyHintStage::FocusedControl, nullptr));
	Dropdown->Open(User);
	TestTrue(TEXT("An open popup takes Back"), Coordinator.IsKeyTakenBefore(User, EKeys::Escape, EReciKeyHintStage::Stock, nullptr));
	TestTrue(TEXT("While it is open, the screen's action gets no key"),
		Coordinator.IsKeyTakenBefore(User, EKeys::J, EReciKeyHintStage::ScreenAction, Tap));
	Dropdown->Close();
	TestFalse(TEXT("Once it closes, Back reaches CommonUI's bindings"),
		Coordinator.IsKeyTakenBefore(User, EKeys::Escape, EReciKeyHintStage::Stock, nullptr));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciIsKeyMappedToActionTest, "ReciCommonUI.Input.IsKeyMappedToAction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciIsKeyMappedToActionTest::RunTest(const FString& Parameters)
{
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	using FLibrary = UReciCommonUIBlueprintLibrary;
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	UInputAction* Select = Harness.CreateAction(TEXT("IA_Select"));
	UInputAction* Other = Harness.CreateAction(TEXT("IA_Other"));
	UInputMappingContext* Mapping = Harness.CreateMapping({{Select, EKeys::Enter}, {Select, EKeys::Gamepad_FaceButton_Bottom}});

	// The keys are the player's, in the mapping contexts applied now.
	TestFalse(TEXT("A key of a mapping context the player has not applied is not mapped"), FLibrary::IsKeyMappedToAction(Screen, Select, EKeys::Enter));
	Harness.GetEnvironment().ApplyMapping(Mapping);
	TestTrue(TEXT("A key mapped to the action is"), FLibrary::IsKeyMappedToAction(Screen, Select, EKeys::Enter));
	TestTrue(TEXT("So is its gamepad key"), FLibrary::IsKeyMappedToAction(Screen, Select, EKeys::Gamepad_FaceButton_Bottom));
	TestFalse(TEXT("Another key is not"), FLibrary::IsKeyMappedToAction(Screen, Select, EKeys::Escape));
	TestFalse(TEXT("Nor is the key for another action"), FLibrary::IsKeyMappedToAction(Screen, Other, EKeys::Enter));

	// Without an action, a key or a player there is nothing to match.
	TestFalse(TEXT("No action"), FLibrary::IsKeyMappedToAction(Screen, nullptr, EKeys::Enter));
	TestFalse(TEXT("An invalid key"), FLibrary::IsKeyMappedToAction(Screen, Select, FKey()));
	TestFalse(TEXT("No widget"), FLibrary::IsKeyMappedToAction(nullptr, Select, EKeys::Enter));
	const TStrongObjectPtr<UReciTestScreen> Unowned(NewObject<UReciTestScreen>(GetTransientPackage()));
	TestFalse(TEXT("A widget with no player"), FLibrary::IsKeyMappedToAction(Unowned.Get(), Select, EKeys::Enter));
	Harness.GetEnvironment().RemoveMapping(Mapping);
	TestFalse(TEXT("A mapping context the player removed no longer maps the key"), FLibrary::IsKeyMappedToAction(Screen, Select, EKeys::Enter));
	return true;
}

#endif
