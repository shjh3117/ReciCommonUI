// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/MenuAnchor.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/AutomationTest.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"
#include "Tests/ReciTestHarness.h"
#include "Tests/ReciTestWidgets.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/ReciList.h"
#include "Widgets/ReciListLayoutBox.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciDropdownPopupBindingTest, "ReciCommonUI.Dropdown.PopupBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciDropdownPopupBindingTest::RunTest(const FString& Parameters)
{
	// The harness gives the dropdown a player, whose context its popup and the popup's list take, as in a game.
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	UReciTestDropdown* Dropdown = CreateWidget<UReciTestDropdown>(Harness.GetPlayerController(), UReciTestDropdown::StaticClass());
	Dropdown->DropdownPopupClass = UReciTestDropdownPopup::StaticClass();
	const TSharedRef<SWidget> DropdownSlate = Dropdown->TakeWidget();
	Dropdown->SetDropdownOptions({FText::FromString(TEXT("Low")), FText::FromString(TEXT("Medium")), FText::FromString(TEXT("High"))}, 1);
	UMenuAnchor* Anchor = Dropdown->GetAnchor();
	if (!TestTrue(TEXT("The dropdown gives its anchor the popup"), Anchor && Anchor->OnGetUserMenuContentEvent.IsBound()))
	{
		return false;
	}

	// Opening creates the popup class and fills its Options list, with the current option selected.
	UReciTestDropdownPopup* Popup = Cast<UReciTestDropdownPopup>(Anchor->OnGetUserMenuContentEvent.Execute());
	if (!TestNotNull(TEXT("The popup is the dropdown's popup class"), Popup))
	{
		return false;
	}
	const UReciList* Options = Cast<UReciList>(Popup->WidgetTree->FindWidget(TEXT("Options")));
	TestTrue(TEXT("Options holds every option"), Options && Options->GetItemCount() == 3);
	TestTrue(TEXT("The current option is selected"), Options && Options->GetSelectedItemIndex() == 1);
	TestTrue(TEXT("The dropdown shows its options with Selection Mode Single"), Options && Options->SelectionMode == ESelectionMode::Single);

	// A popup class that binds no Options cannot open, and says so once.
	Dropdown->DropdownPopupClass = UReciTestBareDropdownPopup::StaticClass();
	AddExpectedMessagePlain(TEXT("its popup ReciTestBareDropdownPopup has no Options list"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 1);
	TestNull(TEXT("A popup without Options opens nothing"), Anchor->OnGetUserMenuContentEvent.Execute());
	TestNull(TEXT("A popup without Options opens nothing again"), Anchor->OnGetUserMenuContentEvent.Execute());

#if WITH_EDITOR
	// The Blueprint compiler's checks: Options must be the content of a Reci layout box, with Selection Mode Single.
	const TStrongObjectPtr<UWidgetTree> Outside(NewObject<UWidgetTree>(GetTransientPackage()));
	UBorder* OutsidePanel = Outside->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
	OutsidePanel->SetContent(Outside->ConstructWidget<UReciList>(UReciList::StaticClass(), TEXT("Options")));
	Outside->RootWidget = OutsidePanel;
	FReciTestCompilerLog OutsideLog;
	GetDefault<UReciDropdownPopup>()->ValidateCompiledWidgetTree(*Outside, OutsideLog);
	TestEqual(TEXT("Options outside a Reci layout box is a compile error"), OutsideLog.Errors.Num(), 1);

	const TStrongObjectPtr<UWidgetTree> Hosted(NewObject<UWidgetTree>(GetTransientPackage()));
	UReciVerticalBox* Layout = Hosted->ConstructWidget<UReciVerticalBox>(UReciVerticalBox::StaticClass(), TEXT("OptionLayout"));
	UReciList* HostedOptions = Hosted->ConstructWidget<UReciList>(UReciList::StaticClass(), TEXT("Options"));
	Layout->SetContent(HostedOptions);
	Hosted->RootWidget = Layout;
	FReciTestCompilerLog HostedLog;
	GetDefault<UReciDropdownPopup>()->ValidateCompiledWidgetTree(*Hosted, HostedLog);
	TestTrue(TEXT("Options in a Reci layout box compiles cleanly"), HostedLog.Errors.IsEmpty() && HostedLog.Warnings.IsEmpty());
	HostedOptions->SelectionMode = ESelectionMode::None;
	FReciTestCompilerLog ModeLog;
	GetDefault<UReciDropdownPopup>()->ValidateCompiledWidgetTree(*Hosted, ModeLog);
	TestEqual(TEXT("A Selection Mode the dropdown replaces is a compile warning"), ModeLog.Warnings.Num(), 1);

	// A dropdown placed in a Blueprint without a popup class cannot open.
	Dropdown->DropdownPopupClass = nullptr;
	FReciTestCompilerLog DropdownLog;
	Dropdown->ValidateCompiledDefaults(DropdownLog);
	TestEqual(TEXT("A placed dropdown without a popup class is a compile warning"), DropdownLog.Warnings.Num(), 1);
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciDropdownToggleSettingsTest, "ReciCommonUI.Dropdown.ToggleSettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciDropdownToggleSettingsTest::RunTest(const FString& Parameters)
{
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	// The button is the popup's toggle: what that needs is a dropdown's default.
	TestTrue(TEXT("A dropdown's defaults make its button the popup's toggle"), GetDefault<UReciTestDropdown>()->IsToggleSetUp());
	UReciTestDropdown* Plain = CreateWidget<UReciTestDropdown>(Harness.GetPlayerController(), UReciTestDropdown::StaticClass());
	Plain->DropdownPopupClass = UReciTestDropdownPopup::StaticClass();
#if WITH_EDITOR
	FReciTestCompilerLog PlainLog;
	Plain->ValidateCompiledDefaults(PlainLog);
	TestTrue(TEXT("A dropdown with its defaults compiles cleanly"), PlainLog.Warnings.IsEmpty() && PlainLog.Errors.IsEmpty());
#endif

	// A dropdown whose Blueprint changed them is a compile warning, and is set back, with one report, when constructed.
	UReciTestDropdown* Changed = CreateWidget<UReciTestDropdown>(Harness.GetPlayerController(), UReciTestDropdown::StaticClass());
	Changed->DropdownPopupClass = UReciTestDropdownPopup::StaticClass();
	Changed->SetClickMethod(EButtonClickMethod::DownAndUp);
	Changed->SetIsToggleable(false);
	TestFalse(TEXT("The changed dropdown is not set up as a toggle yet"), Changed->IsToggleSetUp());
#if WITH_EDITOR
	FReciTestCompilerLog ChangedLog;
	Changed->ValidateCompiledDefaults(ChangedLog);
	TestTrue(TEXT("Changed toggle settings are one compile warning naming them"), ChangedLog.Warnings.Num() == 1
		&& ChangedLog.Warnings[0].Contains(TEXT("so it sets back Click Method (Down And Up), Is Toggleable (off) when it is constructed")));
#endif
	// Reported once per authored dropdown, however many are constructed.
	AddExpectedMessagePlain(TEXT("so it sets back Click Method (Down And Up), Is Toggleable (off)."), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 1);
	const TSharedRef<SWidget> ChangedSlate = Changed->TakeWidget();
	TestTrue(TEXT("Constructing it sets the toggle settings back"), Changed->IsToggleSetUp());
	UReciTestDropdown* Again = CreateWidget<UReciTestDropdown>(Harness.GetPlayerController(), UReciTestDropdown::StaticClass());
	Again->SetIsInteractableWhenSelected(false);
	const TSharedRef<SWidget> AgainSlate = Again->TakeWidget();
	TestTrue(TEXT("Another dropdown of the same authored widget is set back too, without another report"), Again->IsToggleSetUp());
	const TSharedRef<SWidget> PlainSlate = Plain->TakeWidget();
	TestTrue(TEXT("A dropdown with its defaults keeps them"), Plain->IsToggleSetUp());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciDropdownOptionsTest, "ReciCommonUI.Dropdown.Options",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciDropdownOptionsTest::RunTest(const FString& Parameters)
{
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	UReciTestDropdown* Dropdown = CreateWidget<UReciTestDropdown>(Harness.GetPlayerController(), UReciTestDropdown::StaticClass());
	Dropdown->DropdownPopupClass = UReciTestDropdownPopup::StaticClass();
	const TSharedRef<SWidget> DropdownSlate = Dropdown->TakeWidget();
	const TStrongObjectPtr<UReciTestEventRecorder> Events(NewObject<UReciTestEventRecorder>());
	Dropdown->OnSelectionChanged.AddDynamic(Events.Get(), &UReciTestEventRecorder::RecordDropdownSelection);
	const auto ValueText = [Dropdown]() { return Dropdown->GetValueText()->GetText().ToString(); };

	// Options from texts: Selected Index is clamped to the options and picks the shown one without an event.
	const TArray<FText> Texts = {FText::FromString(TEXT("Low")), FText::FromString(TEXT("Medium")), FText::FromString(TEXT("High"))};
	Dropdown->SetDropdownOptions(Texts, 5);
	TestEqual(TEXT("A Selected Index past the end picks the last option"), Dropdown->SelectedIndex, 2);
	TestEqual(TEXT("Selected Value Text shows it"), ValueText(), FString(TEXT("High")));
	Dropdown->SetDropdownOptions(Texts, -3);
	TestEqual(TEXT("A negative Selected Index picks the first option"), Dropdown->SelectedIndex, 0);
	TestEqual(TEXT("Set Dropdown Options raises no On Selection Changed"), Events->ToString(), FString());
	TestEqual(TEXT("Options from texts have no enum value"), Dropdown->GetSelectedEnumValue(), FName());

	// Set Selected Index chooses and reports a change, once.
	TestTrue(TEXT("Set Selected Index chooses another option"), Dropdown->SetSelectedIndex(1));
	TestFalse(TEXT("Choosing the option already chosen changes nothing"), Dropdown->SetSelectedIndex(1));
	TestFalse(TEXT("An index outside the options is refused"), Dropdown->SetSelectedIndex(3));
	TestEqual(TEXT("On Selection Changed reports the change once, with a None enum value"), Events->ToString(), FString(TEXT("Selection 1 None")));
	TestEqual(TEXT("Selected Value Text follows"), ValueText(), FString(TEXT("Medium")));
	Events->Reset();

	// Options from an enum: hidden values and the enum's MAX are left out, and the current value is selected.
	UEnum* SizeEnum = StaticEnum<EReciTestSize>();
	Dropdown->SetDropdownEnum(SizeEnum, TEXT("EReciTestSize::Large"));
	TestEqual(TEXT("The current enum value is selected: Large, after the hidden value is left out"), Dropdown->SelectedIndex, 2);
	TestEqual(TEXT("Get Selected Enum Value names it"), Dropdown->GetSelectedEnumValue(), FName(TEXT("EReciTestSize::Large")));
	TestEqual(TEXT("Selected Value Text shows its display name"), ValueText(), FString(TEXT("Large")));
	UReciTestDropdownPopup* Popup = Cast<UReciTestDropdownPopup>(Dropdown->GetAnchor()->OnGetUserMenuContentEvent.Execute());
	const UReciList* Options = Popup ? Cast<UReciList>(Popup->WidgetTree->FindWidget(TEXT("Options"))) : nullptr;
	TestTrue(TEXT("The popup lists the three values that are not hidden"), Options && Options->GetItemCount() == 3);
	TestEqual(TEXT("Set Dropdown Enum raises no On Selection Changed"), Events->ToString(), FString());
	TestTrue(TEXT("Set Selected Index on enum options"), Dropdown->SetSelectedIndex(0));
	TestEqual(TEXT("On Selection Changed names the enum value"), Events->ToString(), FString(TEXT("Selection 0 EReciTestSize::Small")));
	Dropdown->SetDropdownEnum(SizeEnum, TEXT("EReciTestSize::Secret"));
	TestEqual(TEXT("A current value that is hidden selects the first option"), Dropdown->SelectedIndex, 0);

	// With no options there is nothing to open, which is reported once.
	Dropdown->SetDropdownOptions({}, 0);
	TestEqual(TEXT("No options select nothing"), Dropdown->SelectedIndex, static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("Selected Value Text is empty"), ValueText(), FString());
	AddExpectedMessagePlain(TEXT("cannot open because it has no options"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	Dropdown->OpenDropdown();
	Dropdown->OpenDropdown();
	TestFalse(TEXT("A dropdown without options does not open"), Dropdown->IsDropdownOpen());
	Dropdown->ReleaseSlateResources(true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciDropdownPopupTest, "ReciCommonUI.Dropdown.Popup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciDropdownPopupTest::RunTest(const FString& Parameters)
{
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	FReciTestCallLog& Log = Harness.GetLog();
	// A dropdown on a registered screen, which logs every key its key handler sees.
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	Harness.ActivateScreen(*Screen);
	FReciInputFocusCoordinator::Get().RegisterScreenInput(Screen);
	UReciTestDropdown* Dropdown = Screen->WidgetTree->ConstructWidget<UReciTestDropdown>(UReciTestDropdown::StaticClass(), TEXT("Dropdown"));
	Dropdown->DropdownPopupClass = UReciTestDropdownPopup::StaticClass();
	UOverlaySlot* DropdownSlot = Screen->GetContent()->AddChildToOverlay(Dropdown);
	DropdownSlot->SetHorizontalAlignment(HAlign_Left);
	DropdownSlot->SetVerticalAlignment(VAlign_Top);
	Dropdown->SetDropdownOptions({FText::FromString(TEXT("Low")), FText::FromString(TEXT("Medium")), FText::FromString(TEXT("High"))}, 1);
	const TStrongObjectPtr<UReciTestEventRecorder> Events(NewObject<UReciTestEventRecorder>());
	Dropdown->OnSelectionChanged.AddDynamic(Events.Get(), &UReciTestEventRecorder::RecordDropdownSelection);
	Dropdown->OnDropdownOpenChanged.AddDynamic(Events.Get(), &UReciTestEventRecorder::RecordOpen);
	Harness.Paint();
	TestTrue(TEXT("The dropdown takes focus"), Harness.Focus(*Dropdown));
	const auto HoldsFocus = [](const UWidget& Widget) { return Widget.HasAnyUserFocus() || Widget.HasFocusedDescendants(); };
	// The popup moves focus onto the current option on the tick after it opens.
	const auto Open = [&Harness, Dropdown]()
	{
		Dropdown->OpenDropdown();
		Harness.Paint();
		Harness.GetWorld()->GetTimerManager().Tick(0.01f);
		Harness.Paint();
	};
	const auto FindOptions = [Dropdown]() -> UReciList*
	{
		for (TObjectIterator<UReciTestDropdownPopup> Popup; Popup; ++Popup)
		{
			if (Popup->GetTypedOuter<UReciTestDropdown>() == Dropdown && IsValid(*Popup) && Popup->GetCachedWidget())
			{
				return Cast<UReciList>(Popup->WidgetTree->FindWidget(TEXT("Options")));
			}
		}
		return nullptr;
	};
	const auto Press = [&Harness](const FKey& Key)
	{
		const bool bHandled = Harness.KeyDown(Key);
		Harness.KeyUp(Key);
		Harness.Paint();
		return bHandled;
	};

	// Opening registers the popup layer and puts focus on the current option.
	Open();
	TestTrue(TEXT("Open Dropdown opens the popup"), Dropdown->IsDropdownOpen());
	TestEqual(TEXT("On Dropdown Open Changed reports it once"), Events->ToString(), FString(TEXT("Open")));
	TestEqual(TEXT("The open popup is the user's popup layer"), FReciInputFocusCoordinatorTestAccess::GetPopupCount(), 1);
	UReciList* Options = FindOptions();
	if (!TestNotNull(TEXT("The open popup has its Options list"), Options))
	{
		return false;
	}
	TestEqual(TEXT("Focus is on the current option"), Options->GetFocusedItemIndex(), 1);
	Events->Reset();
	Log.Reset();

	// Down moves the cursor only; Accept chooses, closes and returns focus to the dropdown.
	TestTrue(TEXT("The popup's list takes Down"), Press(EKeys::Down));
	TestEqual(TEXT("Down moves focus to the next option"), Options->GetFocusedItemIndex(), 2);
	TestEqual(TEXT("Moving the cursor chooses nothing"), Dropdown->SelectedIndex, 1);
	TestEqual(TEXT("Moving the cursor raises no event"), Events->ToString(), FString());
	TestTrue(TEXT("The popup's list takes Accept"), Press(EKeys::Enter));
	TestEqual(TEXT("Accept chooses the option under the cursor"), Dropdown->SelectedIndex, 2);
	TestEqual(TEXT("It reports the choice once and closes"), Events->ToString(), FString(TEXT("Closed, Selection 2 None")));
	TestFalse(TEXT("The popup is closed"), Dropdown->IsDropdownOpen());
	TestEqual(TEXT("The popup layer is gone"), FReciInputFocusCoordinatorTestAccess::GetPopupCount(), 0);
	TestTrue(TEXT("Focus is back on the dropdown"), HoldsFocus(*Dropdown));
	Events->Reset();

	// Back closes the popup through the coordinator's popup layer, chooses nothing, and the screen's key handler never
	// sees it.
	Open();
	TestTrue(TEXT("The dropdown opens again"), Dropdown->IsDropdownOpen());
	Log.Reset();
	TestTrue(TEXT("Back is consumed"), Press(EKeys::Escape));
	TestFalse(TEXT("Back closes the popup"), Dropdown->IsDropdownOpen());
	TestEqual(TEXT("It chooses nothing"), Dropdown->SelectedIndex, 2);
	TestEqual(TEXT("Only the open and the close are reported"), Events->ToString(), FString(TEXT("Open, Closed")));
	TestFalse(TEXT("The screen's key handler does not see Back"), Log.ToString().Contains(TEXT("Key Escape")));
	TestTrue(TEXT("Focus is back on the dropdown"), HoldsFocus(*Dropdown));
	FReciInputFocusCoordinator::Get().UnregisterScreenInput(Screen);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciDropdownAnchorToggleTest, "ReciCommonUI.Dropdown.AnchorToggle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciDropdownAnchorToggleTest::RunTest(const FString& Parameters)
{
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	Harness.ActivateScreen(*Screen);
	UReciTestDropdown* Dropdown = Screen->WidgetTree->ConstructWidget<UReciTestDropdown>(UReciTestDropdown::StaticClass(), TEXT("Dropdown"));
	Dropdown->DropdownPopupClass = UReciTestDropdownPopup::StaticClass();
	UOverlaySlot* DropdownSlot = Screen->GetContent()->AddChildToOverlay(Dropdown);
	DropdownSlot->SetHorizontalAlignment(HAlign_Left);
	DropdownSlot->SetVerticalAlignment(VAlign_Top);
	Dropdown->SetDropdownOptions({FText::FromString(TEXT("Low")), FText::FromString(TEXT("Medium")), FText::FromString(TEXT("High"))}, 1);
	const TStrongObjectPtr<UReciTestEventRecorder> Events(NewObject<UReciTestEventRecorder>());
	Dropdown->OnDropdownOpenChanged.AddDynamic(Events.Get(), &UReciTestEventRecorder::RecordOpen);
	Harness.Paint();
	TestTrue(TEXT("The dropdown takes focus"), Harness.Focus(*Dropdown));
	// Paints around the timers the popup sets for the next tick.
	const auto Tick = [&Harness]()
	{
		Harness.Paint();
		Harness.GetWorld()->GetTimerManager().Tick(0.01f);
		Harness.Paint();
	};

	// A press or a release on the anchor's text, routed as Slate routes it to the widgets under the pointer: the button
	// under it takes the press, and then focus, from the popup's option.
	const auto Pointer = [this, &Harness, Dropdown](const bool bTouch, const bool bDown)
	{
		FWidgetPath Found;
		FSlateApplication::Get().GeneratePathToWidgetUnchecked(Dropdown->GetValueText()->TakeWidget(), Found, EVisibility::All);
		if (!TestTrue(TEXT("The anchor's text is in the window"), Found.IsValid()))
		{
			return;
		}
		// Routing reads a pointer position per widget, which only a hit test's path has: give each widget none.
		TArray<FWidgetAndPointer> Widgets;
		for (int32 Index = 0; Index < Found.Widgets.Num(); ++Index)
		{
			Widgets.Emplace(Found.Widgets[Index]);
		}
		const FWidgetPath Path(Widgets);
		const FGeometry& Geometry = Dropdown->GetValueText()->GetCachedGeometry();
		const FVector2D Point(Geometry.GetAbsolutePositionAtCoordinates(FVector2f(0.5f, 0.5f)));
		const FPointerEvent Event = bTouch
			? FPointerEvent(FReciTestHarness::UserIndex, 0, Point, Point, bDown ? 1.0f : 0.0f, bDown)
			: FPointerEvent(FReciTestHarness::UserIndex, FSlateApplication::CursorPointerIndex, Point, Point,
				bDown ? TSet<FKey>{EKeys::LeftMouseButton} : TSet<FKey>(), EKeys::LeftMouseButton, 0.0f, FModifierKeysState());
		if (bDown)
		{
			FSlateApplication::Get().RoutePointerDownEvent(Path, Event);
		}
		else
		{
			FSlateApplication::Get().RoutePointerUpEvent(Path, Event);
		}
		Harness.Paint();
	};
	const auto Tap = [&Pointer, &Tick](const bool bTouch)
	{
		Pointer(bTouch, true);
		Pointer(bTouch, false);
		Tick();
	};

	for (const bool bTouch : {true, false})
	{
		const FString Device = bTouch ? TEXT("Touch: ") : TEXT("Mouse: ");
		Dropdown->OpenDropdown();
		Tick();
		TestTrue(Device + TEXT("the popup is open"), Dropdown->IsDropdownOpen());
		Events->Reset();

		// A press on the open popup's anchor closes it, and its release does not open it again.
		Pointer(bTouch, true);
		TestFalse(Device + TEXT("a press on the open popup's anchor closes it"), Dropdown->IsDropdownOpen());
		Pointer(bTouch, false);
		Tick();
		TestFalse(Device + TEXT("its release leaves it closed"), Dropdown->IsDropdownOpen());
		TestEqual(Device + TEXT("it closes once and does not open again"), Events->ToString(), FString(TEXT("Closed")));
		TestFalse(Device + TEXT("the dropdown's button is not selected"), Dropdown->GetSelected());
		Events->Reset();

		// The next press and release on the closed anchor opens it, and one more closes it.
		Tap(bTouch);
		TestTrue(Device + TEXT("the next press and release opens it"), Dropdown->IsDropdownOpen());
		Tap(bTouch);
		TestFalse(Device + TEXT("one more closes it"), Dropdown->IsDropdownOpen());
		TestEqual(Device + TEXT("each opens or closes it once"), Events->ToString(), FString(TEXT("Open, Closed")));
	}
	return true;
}

#endif
