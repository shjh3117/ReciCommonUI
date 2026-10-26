// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/ReciTestHarness.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Focus/ReciFocusUtils.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "ReciCommonUIBlueprintLibrary.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	UButton* AddButton(UReciTestScreen& Screen, UPanelWidget& Panel, const FName Name)
	{
		UButton* Button = Screen.WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Panel.AddChild(Button);
		return Button;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciKeepFocusOrTargetTest, "ReciCommonUI.Focus.KeepFocusOrTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciKeepFocusOrTargetTest::RunTest(const FString& Parameters)
{
	using UE::ReciCommonUI::Private::HoldsKeepableFocus;
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	// A screen holding button A, button C in a collapsed size box, and an activatable widget nested in it holding
	// button N; and another screen holding button O.
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	UButton* A = AddButton(*Screen, *Screen->GetContent(), TEXT("A"));
	USizeBox* Collapsed = Screen->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Collapsed"));
	Collapsed->SetVisibility(ESlateVisibility::Collapsed);
	Screen->GetContent()->AddChild(Collapsed);
	UButton* C = AddButton(*Screen, *Collapsed, TEXT("C"));
	UReciTestScreen* Nested = Harness.CreateScreen(TEXT("Nested"), Screen);
	UButton* N = AddButton(*Nested, *Nested->GetContent(), TEXT("N"));
	UReciTestScreen* Other = Harness.CreateScreen(TEXT("Other"));
	UButton* O = AddButton(*Other, *Other->GetContent(), TEXT("O"));
	Harness.ActivateScreen(*Screen);
	Harness.ActivateScreen(*Nested);
	Harness.ActivateScreen(*Other);
	Harness.Paint();

	// Focus worth keeping is on a visible, enabled widget of the screen itself.
	TestFalse(TEXT("No focus is not kept"), HoldsKeepableFocus(*Screen, nullptr));
	TestFalse(TEXT("Focus on the screen's own widget is not kept"), HoldsKeepableFocus(*Screen, Screen->GetCachedWidget()));
	TestTrue(TEXT("Focus on a visible, enabled button of the screen is kept"), HoldsKeepableFocus(*Screen, A->GetCachedWidget()));
	A->SetIsEnabled(false);
	TestFalse(TEXT("Focus on a disabled button is not kept"), HoldsKeepableFocus(*Screen, A->GetCachedWidget()));
	A->SetIsEnabled(true);
	TestTrue(TEXT("The button in the collapsed size box is built"), C->GetCachedWidget().IsValid());
	TestFalse(TEXT("Focus inside a collapsed widget is not kept"), HoldsKeepableFocus(*Screen, C->GetCachedWidget()));
	TestFalse(TEXT("Focus inside a nested activatable widget is not the screen's to keep"), HoldsKeepableFocus(*Screen, N->GetCachedWidget()));
	TestTrue(TEXT("It is the nested widget's"), HoldsKeepableFocus(*Nested, N->GetCachedWidget()));
	TestFalse(TEXT("Focus on another screen is not kept"), HoldsKeepableFocus(*Screen, O->GetCachedWidget()));

	// Keep Focus Or Target answers from the player's focus: None keeps it, otherwise the authored target.
	TestTrue(TEXT("Focus reaches A"), Harness.Focus(*A));
	TestNull(TEXT("While A holds focus, the screen keeps it"), UReciCommonUIBlueprintLibrary::KeepFocusOrTarget(Screen, C));
	TestTrue(TEXT("Focus reaches N"), Harness.Focus(*N));
	TestTrue(TEXT("While N holds focus, the screen's target is returned"), UReciCommonUIBlueprintLibrary::KeepFocusOrTarget(Screen, A) == A);
	TestNull(TEXT("While N holds focus, the nested widget keeps it"), UReciCommonUIBlueprintLibrary::KeepFocusOrTarget(Nested, N));
	TestTrue(TEXT("Focus reaches O"), Harness.Focus(*O));
	TestTrue(TEXT("While another screen holds focus, the target is returned"), UReciCommonUIBlueprintLibrary::KeepFocusOrTarget(Screen, A) == A);
	TestTrue(TEXT("With no screen, the target is returned"), UReciCommonUIBlueprintLibrary::KeepFocusOrTarget(nullptr, A) == A);
	const TStrongObjectPtr<UReciTestScreen> Unowned(NewObject<UReciTestScreen>(GetTransientPackage()));
	TestTrue(TEXT("For a screen with no player, the target is returned"), UReciCommonUIBlueprintLibrary::KeepFocusOrTarget(Unowned.Get(), A) == A);

	// CommonUI focuses a set Desired Focus Widget whenever Get Desired Focus Target returns None: reported once per class.
	AddExpectedMessagePlain(TEXT("sets Desired Focus Widget"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	Screen->SetDesiredFocusWidget(A);
	TestTrue(TEXT("Focus returns to A"), Harness.Focus(*A));
	TestNull(TEXT("With Desired Focus Widget set, focus on A is still kept"), UReciCommonUIBlueprintLibrary::KeepFocusOrTarget(Screen, C));
	UReciCommonUIBlueprintLibrary::KeepFocusOrTarget(Screen, C);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciDeviceChangePressTest, "ReciCommonUI.Input.DeviceChangePress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciDeviceChangePressTest::RunTest(const FString& Parameters)
{
	using FAccess = FReciInputFocusCoordinatorTestAccess;
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	FReciTestCallLog& Log = Harness.GetLog();
	// A screen with two Reci lists that take Accept: Primary, where CommonUI's desired focus target puts focus, and
	// Other, where the player's focus is. The mover sits in the PreGame bucket, after Reci's snapshot and before Reci.
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	UReciTestFocusTarget* Primary = Harness.CreateTarget(TEXT("Primary"), Screen);
	Primary->bHandlesAccept = true;
	UReciTestFocusTarget* Other = Harness.CreateTarget(TEXT("Other"), Screen);
	Other->bHandlesAccept = true;
	Harness.ActivateScreen(*Screen);
	Harness.Paint();
	const TSharedRef<FReciTestFocusMover> Mover = MakeShared<FReciTestFocusMover>();
	Mover->Key = EKeys::Enter;
	Mover->MoveTo = Primary;
	FInputPreprocessorRegistrationKey MoverRegistration;
	MoverRegistration.Type = EInputPreProcessorType::PreGame;
	FSlateApplication::Get().RegisterInputPreProcessor(Mover, MoverRegistration);
	ON_SCOPE_EXIT
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(Mover);
	};
	TestTrue(TEXT("Focus reaches the other list"), Harness.Focus(*Other));

	// The key-down moves focus before Reci sees it: the press, its repeat and its release reach no control.
	Mover->bArmed = true;
	TestTrue(TEXT("The press during which focus moved is consumed"), Harness.SlateKeyDown(EKeys::Enter));
	TestTrue(TEXT("Reci noted the focus before the move"), Mover->bSawSnapshot);
	TestTrue(TEXT("Focus moved to the desired target"), Harness.GetFocusedWidget() == Primary->GetCachedWidget());
	TestTrue(TEXT("Its repeat is blocked"), Harness.SlateKeyDown(EKeys::Enter, true));
	TestTrue(TEXT("Its release is swallowed"), Harness.SlateKeyUp(EKeys::Enter));
	TestEqual(TEXT("No list confirms on a press that moved focus"), Log.ToString(), FString());

	// Without the focus move, the same key goes to the focused list.
	Mover->bArmed = false;
	TestTrue(TEXT("Without a focus move, the focused list takes Accept"), Harness.SlateKeyDown(EKeys::Enter));
	TestTrue(TEXT("Its release is swallowed with it"), Harness.SlateKeyUp(EKeys::Enter));
	TestEqual(TEXT("Without a focus move, the focused list confirms"), Log.ToString(), FString(TEXT("Primary Accept, Primary ReturnFocus")));
	Log.Reset();

	// With nothing registered, Reci notes nothing and records nothing.
	Primary->Unregister();
	Other->Unregister();
	TestTrue(TEXT("Focus returns to the other list"), Harness.Focus(*Other));
	Mover->bArmed = true;
	Harness.SlateKeyDown(EKeys::Enter);
	TestFalse(TEXT("With nothing registered, no focus snapshot is taken"), Mover->bSawSnapshot);
	TestEqual(TEXT("With nothing registered, no press is recorded"), FAccess::GetPressCount(FReciTestHarness::UserIndex), 0);
	TestFalse(TEXT("With nothing registered, the release is not swallowed"), FAccess::KeyUp(
		FKeyEvent(EKeys::Enter, FModifierKeysState(), FReciTestHarness::UserIndex, false, 0, 0)));
	TestEqual(TEXT("With nothing registered, no list is called"), Log.ToString(), FString());
	return true;
}

#endif
