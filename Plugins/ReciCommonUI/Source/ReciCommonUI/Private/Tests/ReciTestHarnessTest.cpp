// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/ReciTestHarness.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/CommonUIActionRouterBase.h"
#include "InputAction.h"
#include "Misc/AutomationTest.h"
#include "ReciReport.h"
#include "Settings/ReciCommonUISettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciTestHarnessTest, "ReciCommonUI.TestHarness.SetUpAndTearDown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciTestHarnessTest::RunTest(const FString& Parameters)
{
	using FAccess = FReciInputFocusCoordinatorTestAccess;
	// What a game would have set up, here a resolver and a popup of another user: each harness sets them aside and puts
	// them back. The game's own state is set aside for the whole test.
	const FReciScopedCoordinatorState GameState;
	FReciInputFocusCoordinator::Get().SetEventTimeResolver([](const FInputEvent&, const FKey&, bool) { return 42.0; });
	const TStrongObjectPtr<UReciTestPopupOwner> OtherPopup(NewObject<UReciTestPopupOwner>(GetTransientPackage()));
	FReciInputFocusCoordinator::Get().RegisterOpenPopup(OtherPopup.Get(), FReciTestHarness::UserIndex + 1, nullptr);
	UReciCommonUISettings* Settings = GetMutableDefault<UReciCommonUISettings>();
	const bool bTabNavigationBefore = Settings->bTabNavigation;
	const FKeyEvent Probe(EKeys::Enter, FModifierKeysState(), 0, false, 0, 0);

	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		{
			FReciTestHarness Harness(*this);
			if (!Harness.IsValid())
			{
				return false;
			}
			TestNotEqual(TEXT("The harness sets the resolver aside"), FAccess::ResolveEventTime(Probe, EKeys::Enter, false), 42.0);
			TestEqual(TEXT("The harness sets popups aside"), FAccess::GetPopupCount(), 0);

			FReciTestCallLog& Log = Harness.GetLog();
			UInputAction* Confirm = Harness.CreateAction(TEXT("IA_Confirm"));
			UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
			Screen->SetInputMapping(Harness.CreateMapping({{Confirm, EKeys::Enter}}));
			Screen->HandledActions.Add(Confirm);
			UReciTestScreen* Nested = Harness.CreateScreen(TEXT("Nested"), Screen);
			UReciTestFocusTarget* Target = Harness.CreateTarget(TEXT("Target"), Nested);
			Target->HandledDirections.Add(EUINavigation::Down);
			UReciTestPopupOwner* Dropdown = Harness.CreatePopupOwner(TEXT("Dropdown"), Screen);
			Harness.ActivateScreen(*Screen);
			Harness.ActivateScreen(*Nested);
			FReciInputFocusCoordinator::Get().RegisterScreenInput(Screen);
			Harness.Paint();

			// Real: the player's Slate user, focus, CommonUI's activatable tree and painted geometry.
			const TOptional<int32> ScreenUser = Harness.GetEnvironment().GetUserIndex(*Screen);
			TestTrue(TEXT("A screen belongs to the harness user"), ScreenUser.IsSet() && ScreenUser.GetValue() == FReciTestHarness::UserIndex);
			TestEqual(TEXT("A target reports the harness user"), Target->GetReciInputUserIndex(), FReciTestHarness::UserIndex);
			TestTrue(TEXT("Focus reaches the target"), Harness.Focus(*Target));
			TestTrue(TEXT("The harness user's focus is on the target"), Harness.GetFocusedWidget() == Target->GetCachedWidget());
			TestTrue(TEXT("The screen holds focus in its tree"), Screen->HasFocusedDescendants());
			TestTrue(TEXT("CommonUI finds the nested screen above the focus"),
				UCommonUIActionRouterBase::FindActivatable(Harness.GetFocusedWidget(), nullptr) == Nested);
			TestTrue(TEXT("CommonUI finds the screen above the nested screen"),
				UCommonUIActionRouterBase::FindOwningActivatable(Nested->GetCachedWidget(), nullptr) == Screen);
			TestEqual(TEXT("Painting lays the target out"), FVector2D(Target->GetCachedGeometry().GetLocalSize()), FVector2D(60, 40));
			// Answered by the test environment: the keys of the applied Input Mapping.
			TestEqual(TEXT("The screen's mapping is applied"), Harness.GetEnvironment().GetActionKeys(*Screen, *Confirm), TArray<FKey>{EKeys::Enter});

			// The coordinator reaches the native screen through ProcessEvent, and the target through its interface.
			TestTrue(TEXT("A screen action consumes its key"), Harness.KeyDown(EKeys::Enter));
			TestTrue(TEXT("A screen action consumes its key's release"), Harness.KeyUp(EKeys::Enter));
			TestEqual(TEXT("The screen sees the key, then its action"), Log.ToString(), FString(TEXT("Screen Key Enter, Screen Action IA_Confirm")));
			Log.Reset();
			TestTrue(TEXT("The focused target consumes a direction it takes"), Harness.KeyDown(EKeys::Down));
			Harness.KeyUp(EKeys::Down);
			TestEqual(TEXT("The screen sees the key, then the target"), Log.ToString(),
				FString(TEXT("Screen Key Down, Target Navigate Down, Target ReturnFocus")));
			Log.Reset();

			UReciTestFocusTarget* Popup = Dropdown->Open(FReciTestHarness::UserIndex);
			TestTrue(TEXT("The popup belongs to its owner"), Popup->IsIn(Dropdown));
			TestTrue(TEXT("Focus reaches the popup"), Harness.Focus(*Popup));
			TestTrue(TEXT("Back closes the popup"), Harness.KeyDown(EKeys::Escape));
			Harness.KeyUp(EKeys::Escape);
			TestEqual(TEXT("Only the popup sees Back"), Log.ToString(), FString(TEXT("Dropdown Close")));
			TestFalse(TEXT("The popup is closed"), Dropdown->IsOpen());

			// What a test attaches or changes is its own, and goes when the harness does.
			FReciInputFocusCoordinator::Get().SetEventTimeResolver([](const FInputEvent&, const FKey&, bool) { return 1000.0; });
			Harness.Focus(*Target);
			Harness.KeyDown(EKeys::Enter);
			Harness.KeyUp(EKeys::Enter);
			TestEqual(TEXT("The test's resolver times the action"), Screen->ActionTimestamps.Num() == 2 ? Screen->ActionTimestamps[1] : 0.0, 1000.0);
			Harness.Preserve(*Settings);
			Settings->bTabNavigation = !bTabNavigationBefore;
			TestTrue(TEXT("A harness starts with no reports"), UE::ReciCommonUI::Private::ShouldReportOnce(TEXT("Harness"), TEXT("SetUpAndTearDown")));
			TestFalse(TEXT("A report is made once"), UE::ReciCommonUI::Private::ShouldReportOnce(TEXT("Harness"), TEXT("SetUpAndTearDown")));
		}
		TestFalse(TEXT("The harness removes its Slate user"), FSlateApplication::Get().GetUser(FReciTestHarness::UserIndex).IsValid());
		TestEqual(TEXT("The harness puts the resolver back"), FAccess::ResolveEventTime(Probe, EKeys::Enter, false), 42.0);
		TestEqual(TEXT("The harness puts popups back"), FAccess::GetPopupCount(), 1);
		TestEqual(TEXT("The harness drops its targets"), FAccess::GetTargetCount(), 0);
		TestEqual(TEXT("The harness drops its screens"), FAccess::GetScreenCount(), 0);
		TestEqual(TEXT("The harness puts a preserved setting back"), Settings->bTabNavigation, bTabNavigationBefore);
	}
	return true;
}

#endif
