// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#if WITH_DEV_AUTOMATION_TESTS

#include "Navigation/ReciEnhancedInputNavigationConfig.h"

#include "CommonInputSettings.h"
#include "CommonUITypes.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "ICommonInputModule.h"
#include "InputAction.h"
#include "Input/Events.h"
#include "InputMappingContext.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Navigation/ReciEnhancedInputNavigationSubsystem.h"
#include "Settings/ReciCommonUISettings.h"
#include "Settings/ReciUIInputDataAccess.h"
#include "Tests/ReciTestHarness.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

/** Starts and stops a navigation subsystem on the harness's local player, which has no subsystem collection. */
struct FReciNavigationSubsystemTestAccess
{
	static void Start(UReciEnhancedInputNavigationSubsystem& Subsystem) { Subsystem.Start(); }
	/** As Enhanced Input's Control Mappings Rebuilt does after the player's mappings change. */
	static void RebuildMappings(UReciEnhancedInputNavigationSubsystem& Subsystem) { Subsystem.HandleControlMappingsRebuilt(); }
	static void Stop(UReciEnhancedInputNavigationSubsystem& Subsystem) { Subsystem.Deinitialize(); }
};

namespace
{
	FKeyEvent MakeKeyEvent(const FKey& Key, const bool bShiftDown = false)
	{
		const FModifierKeysState Modifiers(bShiftDown, false, false, false, false, false, false, false, false);
		return FKeyEvent(Key, Modifiers, 0, false, 0, 0);
	}

	/**
	 * A config over Slate's default keys (or the given fallback), with W mapped to Up and F to Accept, as the
	 * subsystem maps the keys of the navigation actions and of CommonUI's click action.
	 */
	TSharedRef<FReciEnhancedInputNavigationConfig> MakeConfig(
		const bool bReplaceDefaultKeys,
		const bool bTabNavigation,
		const TSharedRef<FNavigationConfig>& Fallback = MakeShared<FNavigationConfig>())
	{
		const TSharedRef<FReciEnhancedInputNavigationConfig> Config =
			MakeShared<FReciEnhancedInputNavigationConfig>(Fallback, false, bReplaceDefaultKeys, bTabNavigation, true);
		TMap<FKey, EUINavigation> DirectionRules;
		FReciEnhancedInputNavigationConfig::AddNavigationKeys(DirectionRules, {EKeys::W}, EUINavigation::Up);
		Config->SetMappedKeyEventRules(DirectionRules);
		TMap<FKey, EUINavigationAction> ActionRules;
		FReciEnhancedInputNavigationConfig::AddNavigationKeys(ActionRules, {EKeys::F}, EUINavigationAction::Accept);
		Config->SetMappedKeyActionRules(ActionRules);
		return Config;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciNavigationConfigAddsMappedKeysTest, "ReciCommonUI.Navigation.Config.AddsMappedKeys",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FReciNavigationConfigAddsMappedKeysTest::RunTest(const FString& Parameters)
{
	// Following without Replace: the mapped keys add to Slate's, which all keep working, Tab included.
	const TSharedRef<FReciEnhancedInputNavigationConfig> Config = MakeConfig(false, true);
	TestEqual(TEXT("A key mapped to Up gives Up"), Config->GetNavigationDirectionFromKey(MakeKeyEvent(EKeys::W)), EUINavigation::Up);
	TestEqual(TEXT("The Up arrow still gives Up"), Config->GetNavigationDirectionFromKey(MakeKeyEvent(EKeys::Up)), EUINavigation::Up);
	TestEqual(TEXT("Tab gives Next"), Config->GetNavigationDirectionFromKey(MakeKeyEvent(EKeys::Tab)), EUINavigation::Next);
	TestEqual(TEXT("Shift+Tab gives Previous"), Config->GetNavigationDirectionFromKey(MakeKeyEvent(EKeys::Tab, true)), EUINavigation::Previous);
	TestEqual(TEXT("A key mapped to Accept gives Accept"), Config->GetNavigationActionFromKey(MakeKeyEvent(EKeys::F)), EUINavigationAction::Accept);
	TestEqual(TEXT("Enter still gives Accept"), Config->GetNavigationActionFromKey(MakeKeyEvent(EKeys::Enter)), EUINavigationAction::Accept);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciNavigationConfigReplacesDefaultKeysTest, "ReciCommonUI.Navigation.Config.ReplacesDefaultKeys",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FReciNavigationConfigReplacesDefaultKeysTest::RunTest(const FString& Parameters)
{
	// A mapped key of a device turns off Slate's keys for the same direction or action on that device only.
	const TSharedRef<FReciEnhancedInputNavigationConfig> Config = MakeConfig(true, true);
	TestEqual(TEXT("A key mapped to Up gives Up"), Config->GetNavigationDirectionFromKey(MakeKeyEvent(EKeys::W)), EUINavigation::Up);
	TestEqual(TEXT("The Up arrow gives nothing"), Config->GetNavigationDirectionFromKey(MakeKeyEvent(EKeys::Up)), EUINavigation::Invalid);
	TestEqual(TEXT("The D-pad's Up still gives Up"), Config->GetNavigationDirectionFromKey(MakeKeyEvent(EKeys::Gamepad_DPad_Up)), EUINavigation::Up);
	TestEqual(TEXT("The Down arrow, whose direction maps no key, still gives Down"),
		Config->GetNavigationDirectionFromKey(MakeKeyEvent(EKeys::Down)), EUINavigation::Down);
	TestEqual(TEXT("A key mapped to Accept gives Accept"), Config->GetNavigationActionFromKey(MakeKeyEvent(EKeys::F)), EUINavigationAction::Accept);
	TestEqual(TEXT("Enter gives nothing"), Config->GetNavigationActionFromKey(MakeKeyEvent(EKeys::Enter)), EUINavigationAction::Invalid);
	TestEqual(TEXT("With Back unmapped, Escape still gives Back"), Config->GetNavigationActionFromKey(MakeKeyEvent(EKeys::Escape)), EUINavigationAction::Back);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciNavigationConfigIgnoresPointerKeysTest, "ReciCommonUI.Navigation.Config.IgnoresPointerKeys",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FReciNavigationConfigIgnoresPointerKeysTest::RunTest(const FString& Parameters)
{
	using FConfig = FReciEnhancedInputNavigationConfig;
	TestTrue(TEXT("A keyboard key is a navigation key"), FConfig::IsNavigationKey(EKeys::Enter));
	TestTrue(TEXT("A gamepad button is a navigation key"), FConfig::IsNavigationKey(EKeys::Gamepad_FaceButton_Bottom));
	TestFalse(TEXT("A mouse button is not"), FConfig::IsNavigationKey(EKeys::LeftMouseButton));
	TestFalse(TEXT("Touch is not"), FConfig::IsNavigationKey(EKeys::TouchKeys[ETouchIndex::Touch1]));
	TestFalse(TEXT("A stick axis is not"), FConfig::IsNavigationKey(EKeys::Gamepad_LeftX));
	TestFalse(TEXT("Any Key is not"), FConfig::IsNavigationKey(EKeys::AnyKey));

	// The click action mapped only to the left mouse button maps no navigation key, so the keyboard keeps Slate's
	// Accept keys even with Mapped Keys Replace Default Keys on.
	TMap<FKey, EUINavigationAction> ActionRules;
	FConfig::AddNavigationKeys(ActionRules, {EKeys::LeftMouseButton}, EUINavigationAction::Accept);
	TestTrue(TEXT("A click action on the mouse alone maps no key"), ActionRules.IsEmpty());
	TMap<FKey, EUINavigation> DirectionRules;
	FConfig::AddNavigationKeys(DirectionRules, {EKeys::TouchKeys[ETouchIndex::Touch1], EKeys::W, EKeys::Gamepad_LeftY}, EUINavigation::Up);
	TestTrue(TEXT("Of a direction's keys, only the digital, non-pointer ones are mapped"),
		DirectionRules.Num() == 1 && DirectionRules.Contains(EKeys::W));

	const TSharedRef<FConfig> Config = MakeShared<FConfig>(MakeShared<FNavigationConfig>(), false, true, true, true);
	Config->SetMappedKeyActionRules(ActionRules);
	TestEqual(TEXT("Enter stays Accept"), Config->GetNavigationActionFromKey(MakeKeyEvent(EKeys::Enter)), EUINavigationAction::Accept);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciNavigationConfigTabOffTest, "ReciCommonUI.Navigation.Config.TabOff",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FReciNavigationConfigTabOffTest::RunTest(const FString& Parameters)
{
	// A project's own config that also moves to the next widget on Page Down.
	const TSharedRef<FNavigationConfig> Fallback = MakeShared<FNavigationConfig>();
	Fallback->KeyEventRules.Add(EKeys::PageDown, EUINavigation::Next);
	const TSharedRef<FReciEnhancedInputNavigationConfig> Config = MakeConfig(false, false, Fallback);
	TestEqual(TEXT("Tab gives nothing"), Config->GetNavigationDirectionFromKey(MakeKeyEvent(EKeys::Tab)), EUINavigation::Invalid);
	TestEqual(TEXT("Shift+Tab gives nothing"), Config->GetNavigationDirectionFromKey(MakeKeyEvent(EKeys::Tab, true)), EUINavigation::Invalid);
	TestEqual(TEXT("Another key the project maps to Next keeps it"),
		Config->GetNavigationDirectionFromKey(MakeKeyEvent(EKeys::PageDown)), EUINavigation::Next);
	TestEqual(TEXT("The arrows are unchanged"), Config->GetNavigationDirectionFromKey(MakeKeyEvent(EKeys::Left)), EUINavigation::Left);
	TestEqual(TEXT("A mapped key still navigates"), Config->GetNavigationDirectionFromKey(MakeKeyEvent(EKeys::W)), EUINavigation::Up);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciNavigationConfigStickDominantAxisTest, "ReciCommonUI.Navigation.Config.StickDominantAxis",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FReciNavigationConfigStickDominantAxisTest::RunTest(const FString& Parameters)
{
	// The config times a stick's moves by the application's clock; the test moves it and puts it back.
	const double SavedTime = FApp::GetCurrentTime();
	ON_SCOPE_EXIT
	{
		FApp::SetCurrentTime(SavedTime);
	};
	// Two asks of one key at one event timestamp and application time are one event to the config, and two events made
	// back to back can share a timestamp: each ask meant as a new value gets its own time.
	const auto At = [SavedTime](const double Seconds)
	{
		FApp::SetCurrentTime(SavedTime + Seconds);
	};
	const auto Stick = [](FNavigationConfig& Config, const FKey& Axis, const float Value)
	{
		return Config.GetNavigationDirectionFromAnalog(FAnalogInputEvent(Axis, FModifierKeysState(), 0, false, 0, 0, Value));
	};
	const FKey X = EKeys::Gamepad_LeftX;
	const FKey Y = EKeys::Gamepad_LeftY;
	const TSharedRef<FReciEnhancedInputNavigationConfig> Config =
		MakeShared<FReciEnhancedInputNavigationConfig>(MakeShared<FNavigationConfig>(), false, false, true, true);

	// A diagonal moves one way: along the axis further from rest, once that axis is past its threshold.
	At(10.0);
	TestEqual(TEXT("X under its threshold moves nothing"), Stick(*Config, X, 0.3f), EUINavigation::Invalid);
	TestEqual(TEXT("Y, further, moves Up"), Stick(*Config, Y, 0.8f), EUINavigation::Up);
	At(10.01);
	TestEqual(TEXT("X past its threshold, less far than Y, moves nothing"), Stick(*Config, X, 0.6f), EUINavigation::Invalid);

	// Held, the stick repeats along the axis that is further at each repeat. Within 90 degrees of the last move the
	// second move waits the repeat delay (0.5 s), and the later ones the repeat rate (0.25 s).
	At(10.4);
	TestEqual(TEXT("Before the repeat delay, X pushed further than Y moves nothing"), Stick(*Config, X, 0.85f), EUINavigation::Invalid);
	At(10.6);
	TestEqual(TEXT("After it, X, further now, repeats Right"), Stick(*Config, X, 0.85f), EUINavigation::Right);
	TestEqual(TEXT("and Y, less far, moves nothing"), Stick(*Config, Y, 0.8f), EUINavigation::Invalid);
	At(10.8);
	TestEqual(TEXT("Before the repeat rate, X moves nothing"), Stick(*Config, X, 0.85f), EUINavigation::Invalid);
	At(10.9);
	TestEqual(TEXT("After it, X repeats Right"), Stick(*Config, X, 0.85f), EUINavigation::Right);

	// At rest the stick starts over: its next push moves at once.
	At(10.91);
	Stick(*Config, X, 0.0f);
	Stick(*Config, Y, 0.0f);
	At(10.92);
	TestEqual(TEXT("Pushed down from rest, Y moves Down at once"), Stick(*Config, Y, -0.8f), EUINavigation::Down);

	// A turn of 90 degrees moves at the repeat rate, before the repeat delay is over.
	At(11.2);
	TestEqual(TEXT("X pushed as far as Y moves nothing: a tie goes to Y"), Stick(*Config, X, 0.8f), EUINavigation::Invalid);
	TestEqual(TEXT("Y back at 0 leaves X the direction, which Y's value does not move"), Stick(*Config, Y, 0.0f), EUINavigation::Invalid);
	At(11.22);
	TestEqual(TEXT("Turned to the right, 0.3 s after Down, X moves Right"), Stick(*Config, X, 0.8f), EUINavigation::Right);

	// Slate may ask more than once for one value, and Reci's input coordinator asks before it: the same value gets the
	// same answer, and moves the stick once.
	At(11.23);
	Stick(*Config, X, 0.0f);
	At(11.24);
	const FAnalogInputEvent Push(X, FModifierKeysState(), 0, false, 0, 0, -0.8f);
	TestEqual(TEXT("A push from rest moves Left"), Config->GetNavigationDirectionFromAnalog(Push), EUINavigation::Left);
	TestEqual(TEXT("Asked again, the same value moves Left"), Config->GetNavigationDirectionFromAnalog(Push), EUINavigation::Left);
	At(11.25);
	TestEqual(TEXT("The stick moved once: its next value waits the repeat delay"), Stick(*Config, X, -0.8f), EUINavigation::Invalid);

	// Off, each axis navigates on its own, as in Slate.
	const TSharedRef<FReciEnhancedInputNavigationConfig> Off =
		MakeShared<FReciEnhancedInputNavigationConfig>(MakeShared<FNavigationConfig>(), false, false, true, false);
	TestEqual(TEXT("With the setting off, X gives Right"), Stick(*Off, X, 0.7f), EUINavigation::Right);
	TestEqual(TEXT("and Y then gives Up"), Stick(*Off, Y, 0.7f), EUINavigation::Up);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciNavigationSubsystemOptInTest, "ReciCommonUI.Navigation.Subsystem.OptIn",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FReciNavigationSubsystemOptInTest::RunTest(const FString& Parameters)
{
	// The settings, the Input Data and the warn-once reports go back when the test ends.
	UReciCommonUISettings* Settings = GetMutableDefault<UReciCommonUISettings>();
	const FReciScopedObjectState SettingsState(*Settings);
	const FReciScopedInputData InputData(*this);
	const FReciScopedReports Reports;
	const UReciEnhancedInputNavigationSubsystem* Subsystem = GetDefault<UReciEnhancedInputNavigationSubsystem>();
	Settings->bTabNavigation = true;

	// A project that only installs the plugin keeps stock Input Data: no player gets the subsystem.
	InputData.SetInputDataClass(UCommonUIInputData::StaticClass());
	TestFalse(TEXT("With stock Input Data and the defaults, no player gets the subsystem"), Subsystem->ShouldCreateSubsystem(nullptr));
	Settings->bTabNavigation = false;
	TestTrue(TEXT("Tab Navigation off alone creates it"), Subsystem->ShouldCreateSubsystem(nullptr));
	Settings->bTabNavigation = true;

	// A Reci UI Input Data is followed while CommonUI's Enhanced Input support is on; with it off, that is reported,
	// once.
	InputData.SetInputDataClass(UReciTestInputData::StaticClass());
	if (UCommonInputSettings::IsEnhancedInputSupportEnabled())
	{
		TestTrue(TEXT("Reci UI Input Data creates it"), Subsystem->ShouldCreateSubsystem(nullptr));
	}
	else
	{
		AddExpectedMessagePlain(TEXT("Enhanced Input support is off"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		TestFalse(TEXT("With Enhanced Input support off, Reci UI Input Data does not create it"), Subsystem->ShouldCreateSubsystem(nullptr));
		Subsystem->ShouldCreateSubsystem(nullptr);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciNavigationInputDataTest, "ReciCommonUI.Navigation.InputData",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FReciNavigationInputDataTest::RunTest(const FString& Parameters)
{
	using UE::ReciCommonUI::Private::GetFollowedInputData;
	UCommonInputSettings& CommonInputSettings = ICommonInputModule::GetSettings();
	CommonInputSettings.LoadData();
	const UInputAction* ClickBefore = CommonInputSettings.GetEnhancedInputClickAction();
	const UInputAction* BackBefore = CommonInputSettings.GetEnhancedInputBackAction();
	const UReciUIInputData* FollowedBefore = GetFollowedInputData();
	const TStrongObjectPtr<UInputAction> Up(NewObject<UInputAction>(GetTransientPackage()));
	const TStrongObjectPtr<UInputAction> Right(NewObject<UInputAction>(GetTransientPackage()));
	{
		const FReciScopedInputData InputData(*this);
		InputData.GetDefaults().NavigationUpAction = Up.Get();
		InputData.GetDefaults().NavigationRightAction = Right.Get();
		const UReciUIInputData* Followed = GetFollowedInputData();
		if (TestTrue(TEXT("Reci follows the Reci UI Input Data's defaults"), Followed == &InputData.GetDefaults()))
		{
			TestTrue(TEXT("Up is the Input Data's"), Followed->NavigationUpAction == Up.Get());
			TestNull(TEXT("Down, which the Input Data does not set, is null"), Followed->NavigationDownAction.Get());
			TestNull(TEXT("Left, which the Input Data does not set, is null"), Followed->NavigationLeftAction.Get());
			TestTrue(TEXT("Right is the Input Data's"), Followed->NavigationRightAction == Right.Get());
			TestTrue(TEXT("Mapped Keys Replace Default Keys is on by default"), Followed->bMappedKeysReplaceDefaultKeys);
		}
		TestTrue(TEXT("The test Input Data keeps the project's click action"),
			CommonInputSettings.GetEnhancedInputClickAction() == ClickBefore);
		TestTrue(TEXT("The test Input Data keeps the project's Back action"),
			CommonInputSettings.GetEnhancedInputBackAction() == BackBefore);

		InputData.SetInputDataClass(UCommonUIInputData::StaticClass());
		TestNull(TEXT("Reci follows no stock Input Data"), GetFollowedInputData());
	}
	TestTrue(TEXT("The project's Input Data comes back"), GetFollowedInputData() == FollowedBefore);
	TestTrue(TEXT("The project's click action comes back"), CommonInputSettings.GetEnhancedInputClickAction() == ClickBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciNavigationSubsystemInstallsConfigTest, "ReciCommonUI.Navigation.Subsystem.InstallsConfig",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciNavigationSubsystemInstallsConfigTest::RunTest(const FString& Parameters)
{
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	UReciCommonUISettings* Settings = GetMutableDefault<UReciCommonUISettings>();
	Harness.Preserve(*Settings);
	Settings->bTabNavigation = true;
	// A Reci UI Input Data, which Reci follows, with Mapped Keys Replace Default Keys on.
	const FReciScopedInputData InputData(*this);
	UInputAction* Up = Harness.CreateAction(TEXT("IA_NavigateUp"));
	InputData.GetDefaults().NavigationUpAction = Up;
	InputData.GetDefaults().bMappedKeysReplaceDefaultKeys = true;
	// CommonUI's click action, when the project has one: the subsystem maps its keys to Accept.
	ICommonInputModule::GetSettings().LoadData();
	UInputAction* Click = CommonUI::IsEnhancedInputSupportEnabled()
		? const_cast<UInputAction*>(ICommonInputModule::GetSettings().GetEnhancedInputClickAction())
		: nullptr;
	TArray<TPair<UInputAction*, FKey>> Keys = {{Up, EKeys::W}};
	if (Click)
	{
		Keys.Add({Click, EKeys::F});
	}
	else
	{
		AddInfo(TEXT("The project sets no Enhanced Input click action, so the Accept keys are not checked."));
	}
	UInputMappingContext* Mapping = Harness.CreateMapping(Keys);
	Harness.GetEnvironment().ApplyMapping(Mapping);

	const TSharedPtr<FSlateUser> User = FSlateApplication::Get().GetUser(FReciTestHarness::UserIndex);
	if (!TestTrue(TEXT("The harness player has a Slate user"), User.IsValid()))
	{
		return false;
	}
	const TSharedPtr<FNavigationConfig> PlayerConfig = User->GetUserNavigationConfig();
	const auto Direction = [&User](const FKey& Key)
	{
		return User->GetUserNavigationConfig()->GetNavigationDirectionFromKey(MakeKeyEvent(Key));
	};
	const auto Action = [&User](const FKey& Key)
	{
		return User->GetUserNavigationConfig()->GetNavigationActionFromKey(MakeKeyEvent(Key));
	};

	// Starting replaces the player's config with one built from the player's mappings.
	UReciEnhancedInputNavigationSubsystem* Subsystem = NewObject<UReciEnhancedInputNavigationSubsystem>(Harness.GetLocalPlayer());
	FReciNavigationSubsystemTestAccess::Start(*Subsystem);
	const TSharedPtr<FNavigationConfig> Installed = User->GetUserNavigationConfig();
	TestTrue(TEXT("The subsystem installs its config for its player's Slate user"), Installed.IsValid() && Installed != PlayerConfig);
	TestEqual(TEXT("W, the navigation-up action's key, moves up"), Direction(EKeys::W), EUINavigation::Up);
	TestEqual(TEXT("The Up arrow, replaced on the keyboard, does not"), Direction(EKeys::Up), EUINavigation::Invalid);
	TestEqual(TEXT("The gamepad's D-pad Up still moves up"), Direction(EKeys::Gamepad_DPad_Up), EUINavigation::Up);
	TestEqual(TEXT("Down, which no action maps, keeps Slate's keys"), Direction(EKeys::Down), EUINavigation::Down);
	if (Click)
	{
		TestEqual(TEXT("F, the click action's key, is Accept"), Action(EKeys::F), EUINavigationAction::Accept);
		TestEqual(TEXT("Enter, replaced on the keyboard, is not"), Action(EKeys::Enter), EUINavigationAction::Invalid);
	}

	// A change to the player's mappings rebuilds the same config.
	Harness.GetEnvironment().RemoveMapping(Mapping);
	Harness.GetEnvironment().ApplyMapping(Harness.CreateMapping({{Up, EKeys::I}}));
	FReciNavigationSubsystemTestAccess::RebuildMappings(*Subsystem);
	TestTrue(TEXT("Rebuilding keeps the installed config"), User->GetUserNavigationConfig() == Installed);
	TestEqual(TEXT("After the remap, I moves up"), Direction(EKeys::I), EUINavigation::Up);
	TestEqual(TEXT("W no longer does"), Direction(EKeys::W), EUINavigation::Invalid);
	if (Click)
	{
		TestEqual(TEXT("With the click action unmapped, Enter is Accept again"), Action(EKeys::Enter), EUINavigationAction::Accept);
	}

	// Stopping puts the player's own config back.
	FReciNavigationSubsystemTestAccess::Stop(*Subsystem);
	TestTrue(TEXT("Deinitialize restores the config it replaced"), User->GetUserNavigationConfig() == PlayerConfig);

	// Replace off on the Input Data: the mapped keys add to Slate's, which keep working.
	InputData.GetDefaults().bMappedKeysReplaceDefaultKeys = false;
	UReciEnhancedInputNavigationSubsystem* NoReplace = NewObject<UReciEnhancedInputNavigationSubsystem>(Harness.GetLocalPlayer());
	FReciNavigationSubsystemTestAccess::Start(*NoReplace);
	TestEqual(TEXT("Without Replace, I still moves up"), Direction(EKeys::I), EUINavigation::Up);
	TestEqual(TEXT("And so does the Up arrow, Slate's key"), Direction(EKeys::Up), EUINavigation::Up);
	FReciNavigationSubsystemTestAccess::Stop(*NoReplace);

	// Stock Input Data, even with click and Back actions, is not followed. Tab Navigation off alone: the config wraps
	// the player's own and only takes Tab out.
	// UCommonUIInputData is abstract, so its defaults are put back by hand rather than through a snapshot.
	UCommonUIInputData& Stock = *GetMutableDefault<UCommonUIInputData>();
	const TObjectPtr<UInputAction> StockClickBefore = Stock.EnhancedInputClickAction;
	const TObjectPtr<UInputAction> StockBackBefore = Stock.EnhancedInputBackAction;
	ON_SCOPE_EXIT
	{
		Stock.EnhancedInputClickAction = StockClickBefore;
		Stock.EnhancedInputBackAction = StockBackBefore;
	};
	UInputAction* StockClick = Harness.CreateAction(TEXT("IA_StockClick"));
	UInputAction* StockBack = Harness.CreateAction(TEXT("IA_StockBack"));
	Stock.EnhancedInputClickAction = StockClick;
	Stock.EnhancedInputBackAction = StockBack;
	Harness.GetEnvironment().ApplyMapping(Harness.CreateMapping({{StockClick, EKeys::F}, {StockBack, EKeys::B}}));
	InputData.SetInputDataClass(UCommonUIInputData::StaticClass());
	TestTrue(TEXT("The stock Input Data's click action is CommonUI's"),
		ICommonInputModule::GetSettings().GetEnhancedInputClickAction() == StockClick);
	Settings->bTabNavigation = false;
	UReciEnhancedInputNavigationSubsystem* TabOff = NewObject<UReciEnhancedInputNavigationSubsystem>(Harness.GetLocalPlayer());
	FReciNavigationSubsystemTestAccess::Start(*TabOff);
	TestTrue(TEXT("Tab Navigation off installs a config too"), User->GetUserNavigationConfig() != PlayerConfig);
	TestEqual(TEXT("Tab moves no focus"), Direction(EKeys::Tab), EUINavigation::Invalid);
	TestEqual(TEXT("The mapped key is not followed"), Direction(EKeys::I), EUINavigation::Invalid);
	TestEqual(TEXT("Nor is the stock click action's key"), Action(EKeys::F), EUINavigationAction::Invalid);
	TestEqual(TEXT("Nor the stock Back action's key"), Action(EKeys::B), EUINavigationAction::Invalid);
	TestEqual(TEXT("The arrows are the player's"), Direction(EKeys::Up), EUINavigation::Up);
	TestEqual(TEXT("Enter is the player's Accept"), Action(EKeys::Enter), EUINavigationAction::Accept);
	TestEqual(TEXT("Escape is the player's Back"), Action(EKeys::Escape), EUINavigationAction::Back);
	FReciNavigationSubsystemTestAccess::Stop(*TabOff);
	TestTrue(TEXT("Its config goes when it stops"), User->GetUserNavigationConfig() == PlayerConfig);
	return true;
}

#endif
