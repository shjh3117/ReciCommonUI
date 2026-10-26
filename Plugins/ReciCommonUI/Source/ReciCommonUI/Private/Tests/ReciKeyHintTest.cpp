// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetTree.h"
#include "CommonActionWidget.h"
#include "CommonInputSettings.h"
#include "CommonUITypes.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "ICommonInputModule.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Misc/AutomationTest.h"
#include "Tests/ReciMappedKeyTextTestAccess.h"
#include "Tests/ReciTestHarness.h"
#include "Tests/ReciTestWidgets.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/ReciMappedKeyText.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciKeyHintRuleTest, "ReciCommonUI.KeyHint.Rule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciKeyHintRuleTest::RunTest(const FString& Parameters)
{
	FReciScopedReports Reports;
	const TStrongObjectPtr<UReciTestHintedButton> Button(NewObject<UReciTestHintedButton>(GetTransientPackage()));
	Button->InitializeForTest();
	UReciMappedKeyText* KeyText = Button->GetKeyText();
	if (!TestNotNull(TEXT("The button's key text is its Input Action Widget"), KeyText))
	{
		return false;
	}
	UInputAction* Own = NewObject<UInputAction>(Button.Get(), TEXT("IA_Own"));
	UInputAction* Hint = NewObject<UInputAction>(Button.Get(), TEXT("IA_Hint"));
	// CommonUI's input subsystem loads its click action when a local player starts; the test has none.
	ICommonInputModule::GetSettings().LoadData();
	const bool bEnhancedInput = CommonUI::IsEnhancedInputSupportEnabled();
	const UInputAction* Click = bEnhancedInput ? ICommonInputModule::GetSettings().GetEnhancedInputClickAction() : nullptr;
	auto Shown = [KeyText]() { return KeyText->GetEnhancedInputAction(); };

	// Hide When Unmapped off is turned on, and reported once per button class however often the rule runs.
	AddExpectedMessagePlain(TEXT("InputActionWidget has Hide When Unmapped off"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 1);
	KeyText->bHideWhenUnmapped = false;
	Button->UpdateHint();
	TestTrue(TEXT("Reci turns Hide When Unmapped on"), KeyText->bHideWhenUnmapped);
	KeyText->bHideWhenUnmapped = false;
	Button->UpdateHint();
	TestTrue(TEXT("Reci turns it on again"), KeyText->bHideWhenUnmapped);

	// No key of its own: CommonUI's click action, whose keys show only while the button holds focus; without focus the key
	// text keeps the action for their room (KeyHint.FocusWidth).
	if (!Click)
	{
		AddInfo(TEXT("The project sets no Enhanced Input click action, so the click action is not checked."));
	}
	TestTrue(TEXT("A button with no key of its own holds the click action while it does not hold focus"), Shown() == Click);
	Button->SimulateHover();
	TestTrue(TEXT("Pointer hover keeps it"), Shown() == Click);
	Button->SimulateFocus(true);
	TestTrue(TEXT("Focus shows CommonUI's click action"), Shown() == Click);
	Button->SetShouldUseFallbackDefaultInputAction(false);
	TestNull(TEXT("Without Should Use Fallback Default Input Action, focus shows no click key"), Shown());
	Button->SetShouldUseFallbackDefaultInputAction(true);
	Button->SimulateFocus(false);
	TestTrue(TEXT("Losing focus keeps the click action"), Shown() == Click);

	// Keys of its own show whatever has focus, in CommonUI's order extended by the Hint Input Action.
	Button->SetHintInputAction(Hint);
	TestTrue(TEXT("The Hint Input Action shows without focus"), Shown() == Hint);
	Button->SimulateFocus(true);
	TestTrue(TEXT("The Hint Input Action comes before the click action"), Shown() == Hint);
	if (bEnhancedInput)
	{
		Button->SetTriggeringEnhancedInputAction(Own);
		TestTrue(TEXT("The Triggering Input Action comes before the Hint Input Action"), Shown() == Own);
	}
	const UInputAction* OwnKey = bEnhancedInput ? Own : Hint;
	Button->SetIsEnabled(false);
	TestNull(TEXT("A disabled button shows no action"), Shown());
	Button->SetIsEnabled(true);
	TestTrue(TEXT("Enabled again, the button shows its own key"), Shown() == OwnKey);

	// CommonUI's flags apply, and Reci never hides the widget itself, so a state change does not undo them.
	TestFalse(TEXT("The hint is not hidden by default"), FReciMappedKeyTextTestAccess::IsHiddenByOwner(*KeyText));
	Button->SetHideInputAction(true);
	TestTrue(TEXT("Hide Input Action hides the hint"), FReciMappedKeyTextTestAccess::IsHiddenByOwner(*KeyText));
	Button->SimulateFocus(false);
	Button->SimulateFocus(true);
	Button->SetIsSelectable(true);
	Button->SetIsSelected(true, false);
	TestTrue(TEXT("Focus and selection changes keep it hidden"), FReciMappedKeyTextTestAccess::IsHiddenByOwner(*KeyText));
	TestTrue(TEXT("The hint still names the button's own key"), Shown() == OwnKey);
	Button->SetHideInputAction(false);
	TestFalse(TEXT("Clearing Hide Input Action shows it again"), FReciMappedKeyTextTestAccess::IsHiddenByOwner(*KeyText));

#if WITH_EDITOR
	// The Blueprint compiler's check reads the tree alone.
	auto Validate = [](const UWidgetTree& Tree)
	{
		FReciTestCompilerLog Log;
		GetDefault<UReciTestButton>()->ValidateCompiledWidgetTree(Tree, Log);
		return Log;
	};
	UWidgetTree* Tree = NewObject<UWidgetTree>(GetTransientPackage());
	FReciTestCompilerLog NoneLog = Validate(*Tree);
	TestTrue(TEXT("A button without an Input Action Widget compiles cleanly"), NoneLog.Warnings.IsEmpty() && NoneLog.Errors.IsEmpty());
	Tree->RootWidget = Tree->ConstructWidget<UCommonActionWidget>(UCommonActionWidget::StaticClass(), TEXT("InputActionWidget"));
	FReciTestCompilerLog StockLog = Validate(*Tree);
	TestTrue(TEXT("A stock Common Action Widget is a warning"),
		StockLog.Warnings.Num() == 1 && StockLog.Warnings[0].Contains(TEXT("Use a Reci Mapped Key Text")));
	UWidgetTree* KeyTextTree = NewObject<UWidgetTree>(GetTransientPackage());
	UReciMappedKeyText* Authored =
		KeyTextTree->ConstructWidget<UReciMappedKeyText>(UReciMappedKeyText::StaticClass(), TEXT("InputActionWidget"));
	KeyTextTree->RootWidget = Authored;
	FReciTestCompilerLog OffLog = Validate(*KeyTextTree);
	TestTrue(TEXT("A key text with Hide When Unmapped off is a warning"),
		OffLog.Warnings.Num() == 1 && OffLog.Warnings[0].Contains(TEXT("turn on Hide When Unmapped")));
	Authored->bHideWhenUnmapped = true;
	FReciTestCompilerLog OnLog = Validate(*KeyTextTree);
	TestTrue(TEXT("A key text with Hide When Unmapped on compiles cleanly"), OnLog.Warnings.IsEmpty() && OnLog.Errors.IsEmpty());
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciKeyHintActiveRootTest, "ReciCommonUI.KeyHint.ActiveRoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciKeyHintActiveRootTest::RunTest(const FString& Parameters)
{
	if (!CommonUI::IsEnhancedInputSupportEnabled())
	{
		AddInfo(TEXT("The project does not enable CommonUI's Enhanced Input support, so key hints of input actions are not checked."));
		return true;
	}
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	FReciTestRoutingEnvironment& Environment = Harness.GetEnvironment();
	Environment.SetInputType(ECommonInputType::MouseAndKeyboard);
	UInputAction* Own = Harness.CreateAction(TEXT("IA_Own"));
	UInputAction* Hint = Harness.CreateAction(TEXT("IA_Hint"));
	Environment.ApplyMapping(Harness.CreateMapping({{Own, EKeys::F}, {Hint, EKeys::H}}));
	// A screen with a button whose own key is F and one whose Hint Input Action shows H, and an Is Modal dialog in it.
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	UReciTestScreen* Dialog = Harness.CreateScreen(TEXT("Dialog"), Screen);
	Dialog->SetIsModal(true);
	Harness.ActivateScreen(*Screen);
	Environment.SetActiveRoot(*Screen);
	UReciTestHintedButton* Apply =
		CreateWidget<UReciTestHintedButton>(Harness.GetPlayerController(), UReciTestHintedButton::StaticClass(), TEXT("Apply"));
	UReciTestHintedButton* Back =
		CreateWidget<UReciTestHintedButton>(Harness.GetPlayerController(), UReciTestHintedButton::StaticClass(), TEXT("Back"));
	Screen->GetContent()->AddChild(Apply);
	Screen->GetContent()->AddChild(Back);
	Apply->SetTriggeringEnhancedInputAction(Own);
	Back->SetHintInputAction(Hint);
	Harness.Paint();
	const auto Hints = [Apply, Back]()
	{
		FString Shown;
		for (const UReciTestHintedButton* Button : {Apply, Back})
		{
			UReciMappedKeyText* KeyText = Button->GetKeyText();
			KeyText->RefreshMappedKeyText();
			Shown += Shown.IsEmpty() ? FString() : FString(TEXT(" | "));
			Shown += KeyText->GetVisibility() == ESlateVisibility::Hidden ? FString(TEXT("-")) : KeyText->GetMappedKeyText().ToString();
		}
		return Shown;
	};
	TestEqual(TEXT("In CommonUI's active root a button's own key and its Hint Input Action show"), Hints(), FString(TEXT("F | H")));

	// The Is Modal dialog becomes the active root: the screen's bindings are inactive and it gets no screen input.
	Harness.ActivateScreen(*Dialog);
	Environment.SetActiveRoot(*Dialog);
	TestEqual(TEXT("Beneath a nested Is Modal dialog the screen's hints show no key"), Hints(), FString(TEXT("- | -")));
	Harness.DeactivateScreen(*Dialog);
	Environment.SetActiveRoot(*Screen);
	TestEqual(TEXT("When the dialog closes they show again"), Hints(), FString(TEXT("F | H")));

	// A screen that deactivates while another root is active keeps its hints as it fades out.
	Environment.SetActiveRoot(*Dialog);
	Harness.DeactivateScreen(*Screen);
	TestEqual(TEXT("A deactivated screen's hints stay as they were"), Hints(), FString(TEXT("F | H")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciKeyHintFocusWidthTest, "ReciCommonUI.KeyHint.FocusWidth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciKeyHintFocusWidthTest::RunTest(const FString& Parameters)
{
	if (!CommonUI::IsEnhancedInputSupportEnabled())
	{
		AddInfo(TEXT("The project does not enable CommonUI's Enhanced Input support, so key hints of input actions are not checked."));
		return true;
	}
	// CommonUI's input subsystem loads its click action when a local player starts; the harness's need not have.
	ICommonInputModule::GetSettings().LoadData();
	UInputAction* Click = ICommonInputModule::GetSettings().GetEnhancedInputClickAction();
	if (!Click)
	{
		AddInfo(TEXT("The project sets no Enhanced Input click action, so the click key's width is not checked."));
		return true;
	}
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	FReciTestRoutingEnvironment& Environment = Harness.GetEnvironment();
	// ENTER is wider than A, so a click key that came and went with focus would move the button's edge.
	Environment.ApplyMapping(Harness.CreateMapping({{Click, EKeys::Enter}, {Click, EKeys::Gamepad_FaceButton_Bottom}}));
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	Harness.ActivateScreen(*Screen);
	// Two buttons with no key of their own that size to their content, as a labelled button does: one reserves room for
	// every device's keys (the default), one for the shown device's keys only.
	UReciTestHintedButton* Reserved =
		CreateWidget<UReciTestHintedButton>(Harness.GetPlayerController(), UReciTestHintedButton::StaticClass(), TEXT("Reserved"));
	UReciTestHintedButton* Tight =
		CreateWidget<UReciTestHintedButton>(Harness.GetPlayerController(), UReciTestHintedButton::StaticClass(), TEXT("Tight"));
	for (UReciTestHintedButton* Button : {Reserved, Tight})
	{
		Screen->GetContent()->AddChild(Button);
		USizeBox* Bounds = Cast<USizeBox>(Button->GetWidgetFromName(TEXT("Bounds")));
		if (!TestNotNull(TEXT("The test button has its bounds box"), Bounds))
		{
			return false;
		}
		Bounds->ClearWidthOverride();
	}
	Tight->GetKeyText()->bReserveAllDeviceWidths = false;
	Harness.Paint();
	const auto Width = [&Harness](UReciTestHintedButton& Button)
	{
		Button.GetKeyText()->RefreshMappedKeyText();
		Harness.Paint();
		const TSharedPtr<SWidget> Widget = Button.GetCachedWidget();
		return Widget ? Widget->GetDesiredSize().X : 0.0;
	};
	const auto Shown = [](const UReciTestHintedButton& Button)
	{
		const UReciMappedKeyText* KeyText = Button.GetKeyText();
		return KeyText->GetVisibility() == ESlateVisibility::Hidden ? FString(TEXT("-")) : KeyText->GetMappedKeyText().ToString();
	};

	TMap<ECommonInputType, double> TightWidths;
	for (const ECommonInputType InputType : {ECommonInputType::MouseAndKeyboard, ECommonInputType::Gamepad})
	{
		const bool bKeyboard = InputType == ECommonInputType::MouseAndKeyboard;
		const TCHAR* Device = bKeyboard ? TEXT("keyboard") : TEXT("gamepad");
		Environment.SetInputType(InputType);
		for (UReciTestHintedButton* Button : {Reserved, Tight})
		{
			const TCHAR* Name = Button == Reserved ? TEXT("reserved") : TEXT("tight");
			const double Before = Width(*Button);
			TestEqual(FString::Printf(TEXT("On %s an unfocused %s button shows no key"), Device, Name), Shown(*Button), FString(TEXT("-")));
			Button->SimulateFocus(true);
			const double Focused = Width(*Button);
			TestEqual(FString::Printf(TEXT("On %s the focused %s button shows its click key"), Device, Name), Shown(*Button),
				FString(bKeyboard ? TEXT("ENTER") : TEXT("A")));
			Button->SimulateFocus(false);
			const double After = Width(*Button);
			TestEqual(FString::Printf(TEXT("On %s the %s button is as wide before focus as with it"), Device, Name), Before, Focused, 0.01);
			TestEqual(FString::Printf(TEXT("On %s the %s button keeps its width when focus leaves"), Device, Name), After, Focused, 0.01);
			if (Button == Tight)
			{
				TightWidths.Add(InputType, Focused);
			}
		}
	}
	const double KeyboardWidth = TightWidths.FindRef(ECommonInputType::MouseAndKeyboard);
	TestTrue(TEXT("The tight button's room follows the device: A is narrower than ENTER"),
		TightWidths.FindRef(ECommonInputType::Gamepad) < KeyboardWidth - 1.0);
	TestEqual(TEXT("The reserved button is as wide as the widest device's key"), Width(*Reserved), KeyboardWidth, 0.01);

	// A device change without focus gives the room of that device's key.
	Environment.SetInputType(ECommonInputType::MouseAndKeyboard);
	TestEqual(TEXT("Unfocused, the tight button takes the keyboard key's room after the gamepad's"), Width(*Tight), KeyboardWidth, 0.01);

	// Touch shows no key, focused or not; the reserved button keeps the room it has on every device.
	Environment.SetInputType(ECommonInputType::Touch);
	const double TouchUnfocused = Width(*Reserved);
	Reserved->SimulateFocus(true);
	TestEqual(TEXT("On touch the focused button shows no key"), Shown(*Reserved), FString(TEXT("-")));
	TestEqual(TEXT("On touch the button has the same width focused and not"), Width(*Reserved), TouchUnfocused, 0.01);
	TestEqual(TEXT("That is its width on keyboard and gamepad"), TouchUnfocused, KeyboardWidth, 0.01);
	Reserved->SimulateFocus(false);
	return true;
}

#endif
