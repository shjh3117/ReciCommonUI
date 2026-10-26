// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetTree.h"
#include "CommonInputBaseTypes.h"
#include "CommonUITypes.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Engine/PlatformSettingsManager.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tests/ReciMappedKeyTextTestAccess.h"
#include "Tests/ReciTestControllerData.h"
#include "Tests/ReciTestHarness.h"
#include "Tests/ReciTestTextStyle.h"
#include "Tests/ReciTestWidgets.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/ReciKeyTextSegments.h"
#include "Widgets/ReciMappedKeyText.h"
#include "Widgets/Text/STextBlock.h"

using UE::ReciCommonUI::Private::BuildKeySegments;
using UE::ReciCommonUI::Private::FReciKeySegment;
using UE::ReciCommonUI::Private::FReciKeySegmentRules;
using UE::ReciCommonUI::Private::JoinKeySegmentText;

namespace
{
	/** Gives the platform's Common Input settings these controller data entries for a scope, and puts the project's back. */
	class FReciScopedControllerData
	{
	public:
		explicit FReciScopedControllerData(TArray<TSoftClassPtr<UCommonInputBaseControllerData>> InEntries)
			: Settings(UCommonInputPlatformSettings::Get())
			, Entries(MoveTemp(InEntries))
		{
			if (!Settings)
			{
				return;
			}
			Saved = Settings->GetControllerData();
			for (const TSoftClassPtr<UCommonInputBaseControllerData>& Entry : Saved)
			{
				Settings->RemoveControllerDataEntry(Entry);
			}
			for (const TSoftClassPtr<UCommonInputBaseControllerData>& Entry : Entries)
			{
				Settings->AddControllerDataEntry(Entry);
			}
		}

		~FReciScopedControllerData()
		{
			if (!Settings)
			{
				return;
			}
			for (const TSoftClassPtr<UCommonInputBaseControllerData>& Entry : Entries)
			{
				Settings->RemoveControllerDataEntry(Entry);
			}
			// In their order: the first that matches a key draws it.
			for (const TSoftClassPtr<UCommonInputBaseControllerData>& Entry : Saved)
			{
				Settings->AddControllerDataEntry(Entry);
			}
		}

		bool IsValid() const { return Settings != nullptr; }

	private:
		UCommonInputPlatformSettings* Settings;
		TArray<TSoftClassPtr<UCommonInputBaseControllerData>> Entries;
		TArray<TSoftClassPtr<UCommonInputBaseControllerData>> Saved;
	};

	FReciKeySegmentRules MakeRules(const ECommonInputType InputType, const FName GamepadName)
	{
		FReciKeySegmentRules Rules;
		Rules.InputType = InputType;
		Rules.GamepadName = GamepadName;
		Rules.KeySeparator = FText::FromString(TEXT(" / "));
		Rules.ActionSeparator = FText::FromString(TEXT(" | "));
		return Rules;
	}

	FSlateBrush MakeBrush(const double Width, const double Height)
	{
		FSlateBrush Brush;
		Brush.SetImageSize(FVector2D(Width, Height));
		return Brush;
	}

	/** "glyph(<width>x<height>:<names>)" for a glyph and the text for a name, joined: one readable comparison per row. */
	FString Describe(const TArray<FReciKeySegment>& Segments)
	{
		FString Description;
		for (const FReciKeySegment& Segment : Segments)
		{
			if (Segment.Glyph.IsSet())
			{
				const FVector2D Size = Segment.Glyph->GetImageSize();
				Description += FString::Printf(TEXT("glyph(%.0fx%.0f:%s)"), Size.X, Size.Y, *Segment.Text.ToString());
			}
			else
			{
				Description += Segment.Text.ToString();
			}
		}
		return Description;
	}

	/** A key text at the top left of the screen's content, so that it is laid out at its desired size. */
	UReciMappedKeyText* AddKeyText(UReciTestScreen& Screen, const FName Name, UInputAction* Action)
	{
		UReciMappedKeyText* KeyText = Screen.WidgetTree->ConstructWidget<UReciMappedKeyText>(UReciMappedKeyText::StaticClass(), Name);
		UOverlaySlot* KeyTextSlot = Screen.GetContent()->AddChildToOverlay(KeyText);
		KeyTextSlot->SetHorizontalAlignment(HAlign_Left);
		KeyTextSlot->SetVerticalAlignment(VAlign_Top);
		KeyText->SetEnhancedInputAction(Action);
		return KeyText;
	}

	/** The keys a key text shows, as names, or "-" while it is hidden. */
	FString Shown(const UReciMappedKeyText& KeyText)
	{
		return KeyText.GetVisibility() == ESlateVisibility::Hidden ? FString(TEXT("-")) : KeyText.GetMappedKeyText().ToString();
	}

	/** Key texts of input actions need CommonUI's Enhanced Input support, which is a project setting. */
	bool HasEnhancedInputSupport(FAutomationTestBase& Test)
	{
		if (!CommonUI::IsEnhancedInputSupportEnabled())
		{
			Test.AddInfo(TEXT("The project does not enable CommonUI's Enhanced Input support, so key texts of input actions are not checked."));
			return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciKeyTextGlyphsTest, "ReciCommonUI.KeyText.Glyphs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciKeyTextGlyphsTest::RunTest(const FString& Parameters)
{
	const UCommonInputPlatformSettings* PlatformSettings = UCommonInputPlatformSettings::Get();
	if (!TestNotNull(TEXT("The platform has Common Input settings"), PlatformSettings))
	{
		return false;
	}
	// Controller data for the platform's default gamepad type: a glyph for the bottom face button and for Enter, and one
	// for the left stick's up and down together.
	const FName GamepadName = PlatformSettings->GetDefaultGamepadName();
	UReciTestControllerData* Data = GetMutableDefault<UReciTestControllerData>();
	ON_SCOPE_EXIT
	{
		Data->InputBrushDataMap.Reset();
		Data->InputBrushKeySets.Reset();
	};
	Data->InputType = ECommonInputType::Gamepad;
	Data->GamepadName = GamepadName;
	FCommonInputKeyBrushConfiguration Bottom;
	Bottom.Key = EKeys::Gamepad_FaceButton_Bottom;
	Bottom.KeyBrush = MakeBrush(20.0, 20.0);
	FCommonInputKeyBrushConfiguration Enter;
	Enter.Key = EKeys::Enter;
	Enter.KeyBrush = MakeBrush(30.0, 20.0);
	Data->InputBrushDataMap = {Bottom, Enter};
	FCommonInputKeySetBrushConfiguration Stick;
	Stick.Keys = {EKeys::Gamepad_LeftStick_Up, EKeys::Gamepad_LeftStick_Down};
	Stick.KeyBrush = MakeBrush(20.0, 40.0);
	Data->InputBrushKeySets = {Stick};
	const FReciScopedControllerData ControllerData({TSoftClassPtr<UCommonInputBaseControllerData>(UReciTestControllerData::StaticClass())});
	if (!TestTrue(TEXT("The test's controller data is in place"), ControllerData.IsValid()))
	{
		return false;
	}

	const FReciKeySegmentRules Gamepad = MakeRules(ECommonInputType::Gamepad, GamepadName);
	TestEqual(TEXT("A key with a glyph draws it, at the brush's size"),
		Describe(BuildKeySegments({{EKeys::Gamepad_FaceButton_Bottom}}, Gamepad)), FString(TEXT("glyph(20x20:A)")));
	TestEqual(TEXT("A key without a glyph draws its name"),
		Describe(BuildKeySegments({{EKeys::Gamepad_FaceButton_Right}}, Gamepad)), FString(TEXT("B")));
	TestEqual(TEXT("A glyph and a name share a row"),
		Describe(BuildKeySegments({{EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Right}}, Gamepad)),
		FString(TEXT("glyph(20x20:A) / B")));
	TestEqual(TEXT("A group the controller data has a glyph for draws one glyph"),
		Describe(BuildKeySegments({{EKeys::Gamepad_LeftStick_Up, EKeys::Gamepad_LeftStick_Down}}, Gamepad)),
		FString(TEXT("glyph(20x40:L↑ / L↓)")));
	TestEqual(TEXT("A key an earlier action shows as a glyph is not repeated"),
		Describe(BuildKeySegments({{EKeys::Gamepad_FaceButton_Bottom}, {EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Right}}, Gamepad)),
		FString(TEXT("glyph(20x20:A) | B")));
	const TArray<FReciKeySegment> Mixed = BuildKeySegments({{EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Right}}, Gamepad);
	TestEqual(TEXT("The text view names a key drawn as a glyph"), JoinKeySegmentText(Mixed).ToString(), FString(TEXT("A / B")));

	// The author's own name beats the glyph, for the key alone and for a group it is in.
	FReciKeySegmentRules Named = Gamepad;
	const TMap<FName, FText> Overrides = {{EKeys::Gamepad_FaceButton_Bottom.GetFName(), FText::FromString(TEXT("ok"))},
		{EKeys::Gamepad_LeftStick_Up.GetFName(), FText::FromString(TEXT("UP"))}};
	Named.KeyNameOverrides = &Overrides;
	TestEqual(TEXT("A name of the author's own beats the glyph"),
		Describe(BuildKeySegments({{EKeys::Gamepad_FaceButton_Bottom}}, Named)), FString(TEXT("OK")));
	TestEqual(TEXT("A group with a name of the author's own draws each key"),
		Describe(BuildKeySegments({{EKeys::Gamepad_LeftStick_Up, EKeys::Gamepad_LeftStick_Down}}, Named)), FString(TEXT("UP / L↓")));

	// Glyphs come from controller data for the input type shown.
	const FReciKeySegmentRules Keyboard = MakeRules(ECommonInputType::MouseAndKeyboard, GamepadName);
	TestEqual(TEXT("Keyboard keys draw names when no keyboard controller data has them"),
		Describe(BuildKeySegments({{EKeys::Enter}}, Keyboard)), FString(TEXT("ENTER")));
	TestEqual(TEXT("A gamepad key is not shown for the keyboard"),
		Describe(BuildKeySegments({{EKeys::Gamepad_FaceButton_Bottom, EKeys::Enter}}, Keyboard)), FString(TEXT("ENTER")));
	TestEqual(TEXT("Another gamepad type's keys draw names"),
		Describe(BuildKeySegments({{EKeys::Gamepad_FaceButton_Bottom}}, MakeRules(ECommonInputType::Gamepad, TEXT("ReciTestOtherPad")))),
		FString(TEXT("A")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciKeyTextNamesTest, "ReciCommonUI.KeyText.Names",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciKeyTextNamesTest::RunTest(const FString& Parameters)
{
	const FReciScopedControllerData NoControllerData{TArray<TSoftClassPtr<UCommonInputBaseControllerData>>()};
	if (!TestTrue(TEXT("The platform has Common Input settings"), NoControllerData.IsValid()))
	{
		return false;
	}
	TestTrue(TEXT("Key Name Overrides is empty by default"), GetDefault<UReciMappedKeyText>()->KeyNameOverrides.IsEmpty());

	// The built-in names, each key alone, without controller data.
	const FName GamepadName = UCommonInputPlatformSettings::Get()->GetDefaultGamepadName();
	struct FExpectedName
	{
		FKey Key;
		const TCHAR* Name;
	};
	const FExpectedName Expected[] = {
		{EKeys::Escape, TEXT("ESC")}, {EKeys::BackSpace, TEXT("BS")}, {EKeys::Enter, TEXT("ENTER")},
		{EKeys::SpaceBar, TEXT("SPACE")}, {EKeys::PageUp, TEXT("PGUP")}, {EKeys::PageDown, TEXT("PGDN")},
		{EKeys::LeftMouseButton, TEXT("LMB")}, {EKeys::RightMouseButton, TEXT("RMB")}, {EKeys::MiddleMouseButton, TEXT("MMB")},
		{EKeys::Up, TEXT("↑")}, {EKeys::Down, TEXT("↓")}, {EKeys::Left, TEXT("←")}, {EKeys::Right, TEXT("→")},
		{EKeys::F, TEXT("F")},
		{EKeys::Gamepad_FaceButton_Bottom, TEXT("A")}, {EKeys::Gamepad_FaceButton_Right, TEXT("B")},
		{EKeys::Gamepad_FaceButton_Left, TEXT("X")}, {EKeys::Gamepad_FaceButton_Top, TEXT("Y")},
		{EKeys::Gamepad_DPad_Up, TEXT("D↑")}, {EKeys::Gamepad_DPad_Down, TEXT("D↓")},
		{EKeys::Gamepad_DPad_Left, TEXT("D←")}, {EKeys::Gamepad_DPad_Right, TEXT("D→")},
		{EKeys::Gamepad_LeftY, TEXT("L-STICK")}, {EKeys::Gamepad_LeftX, TEXT("L-STICK")},
		{EKeys::Gamepad_Special_Right, TEXT("MENU")}, {EKeys::Gamepad_Special_Left, TEXT("VIEW")},
		{EKeys::Gamepad_LeftShoulder, TEXT("LB")}, {EKeys::Gamepad_RightShoulder, TEXT("RB")},
		{EKeys::Gamepad_LeftTrigger, TEXT("LT")}, {EKeys::Gamepad_RightTrigger, TEXT("RT")},
		{EKeys::Gamepad_LeftThumbstick, TEXT("L3")}, {EKeys::Gamepad_RightThumbstick, TEXT("R3")},
		{EKeys::Gamepad_LeftStick_Up, TEXT("L↑")}, {EKeys::Gamepad_LeftStick_Down, TEXT("L↓")},
		{EKeys::Gamepad_LeftStick_Left, TEXT("L←")}, {EKeys::Gamepad_LeftStick_Right, TEXT("L→")},
		{EKeys::Gamepad_RightStick_Up, TEXT("R↑")}, {EKeys::Gamepad_RightStick_Down, TEXT("R↓")},
		{EKeys::Gamepad_RightStick_Left, TEXT("R←")}, {EKeys::Gamepad_RightStick_Right, TEXT("R→")}};
	for (const FExpectedName& Entry : Expected)
	{
		const ECommonInputType InputType = Entry.Key.IsGamepadKey() ? ECommonInputType::Gamepad : ECommonInputType::MouseAndKeyboard;
		const TArray<FReciKeySegment> Segments = BuildKeySegments({{Entry.Key}}, MakeRules(InputType, GamepadName));
		TestEqual(FString::Printf(TEXT("%s is named %s"), *Entry.Key.ToString(), Entry.Name), Describe(Segments), FString(Entry.Name));
	}

	// Two keys with one name show it once, and the maximum counts the keys shown.
	const FReciKeySegmentRules Gamepad = MakeRules(ECommonInputType::Gamepad, GamepadName);
	TestEqual(TEXT("A name already shown is not repeated"),
		Describe(BuildKeySegments({{EKeys::Gamepad_LeftY, EKeys::Gamepad_LeftX, EKeys::Gamepad_FaceButton_Bottom}}, Gamepad)),
		FString(TEXT("L-STICK / A")));
	FReciKeySegmentRules One = Gamepad;
	One.MaximumDisplayedKeys = 1;
	TestEqual(TEXT("Maximum Displayed Keys counts each action's keys"),
		Describe(BuildKeySegments({{EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Right}, {EKeys::Gamepad_FaceButton_Top}}, One)),
		FString(TEXT("A | Y")));
	FReciKeySegmentRules Lower = Gamepad;
	Lower.bUppercaseKeyNames = false;
	const TMap<FName, FText> Overrides = {{EKeys::Gamepad_FaceButton_Right.GetFName(), FText::FromString(TEXT("Back"))}};
	Lower.KeyNameOverrides = &Overrides;
	TestEqual(TEXT("A name of the author's own is used as written when names are not uppercased"),
		Describe(BuildKeySegments({{EKeys::Gamepad_FaceButton_Right}}, Lower)), FString(TEXT("Back")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciKeyTextFilterTest, "ReciCommonUI.KeyText.Filter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciKeyTextFilterTest::RunTest(const FString& Parameters)
{
	const FReciScopedControllerData NoControllerData{TArray<TSoftClassPtr<UCommonInputBaseControllerData>>()};
	const TFunction<bool(const FKey&)> NotJ = [](const FKey& Key) { return Key != EKeys::J; };
	const TFunction<bool(const FKey&)> NotK = [](const FKey& Key) { return Key != EKeys::K; };
	const TFunction<bool(const FKey&)> NotLeftY = [](const FKey& Key) { return Key != EKeys::Gamepad_LeftY; };

	FReciKeySegmentRules Keyboard = MakeRules(ECommonInputType::MouseAndKeyboard, NAME_None);
	Keyboard.KeyFilter = &NotK;
	TestEqual(TEXT("A key the filter leaves out leaves the text"),
		Describe(BuildKeySegments({{EKeys::J, EKeys::K, EKeys::L}}, Keyboard)), FString(TEXT("J / L")));
	TestTrue(TEXT("An action whose keys the filter all leaves out shows nothing"),
		BuildKeySegments({{EKeys::K}}, Keyboard).IsEmpty());
	Keyboard.KeyFilter = &NotJ;
	Keyboard.MaximumDisplayedKeys = 1;
	TestEqual(TEXT("Maximum Displayed Keys counts the keys the filter keeps"),
		Describe(BuildKeySegments({{EKeys::J, EKeys::K}}, Keyboard)), FString(TEXT("K")));
	FReciKeySegmentRules Gamepad = MakeRules(ECommonInputType::Gamepad, NAME_None);
	Gamepad.KeyFilter = &NotLeftY;
	TestEqual(TEXT("A key the filter leaves out does not hide another key's name"),
		Describe(BuildKeySegments({{EKeys::Gamepad_LeftY, EKeys::Gamepad_LeftX}}, Gamepad)), FString(TEXT("L-STICK")));

	// A key text: its shown keys go through its filter, its reserved width counts every key.
	const TStrongObjectPtr<UReciMappedKeyText> KeyText(NewObject<UReciMappedKeyText>(GetTransientPackage()));
	KeyText->SetKeyFilter(NotK);
	const FReciKeySegmentRules Shown =
		FReciMappedKeyTextTestAccess::MakeSegmentRules(*KeyText, ECommonInputType::MouseAndKeyboard, NAME_None, true);
	const FReciKeySegmentRules Reserved =
		FReciMappedKeyTextTestAccess::MakeSegmentRules(*KeyText, ECommonInputType::MouseAndKeyboard, NAME_None, false);
	TestEqual(TEXT("The key text shows the keys its filter keeps"),
		Describe(BuildKeySegments({{EKeys::J, EKeys::K}}, Shown)), FString(TEXT("J")));
	TestEqual(TEXT("The key text reserves room for every key"),
		Describe(BuildKeySegments({{EKeys::J, EKeys::K}}, Reserved)), FString(TEXT("J / K")));
	KeyText->SetKeyFilter(nullptr);
	TestEqual(TEXT("Without a filter it shows every key"),
		Describe(BuildKeySegments({{EKeys::J, EKeys::K}},
			FReciMappedKeyTextTestAccess::MakeSegmentRules(*KeyText, ECommonInputType::MouseAndKeyboard, NAME_None, true))),
		FString(TEXT("J / K")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciKeyTextDevicesTest, "ReciCommonUI.KeyText.Devices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciKeyTextDevicesTest::RunTest(const FString& Parameters)
{
	if (!HasEnhancedInputSupport(*this))
	{
		return true;
	}
	const FReciScopedControllerData NoControllerData{TArray<TSoftClassPtr<UCommonInputBaseControllerData>>()};
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	FReciTestRoutingEnvironment& Environment = Harness.GetEnvironment();
	// An activated screen: one that is fading out keeps the keys it showed.
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	Harness.ActivateScreen(*Screen);
	UInputAction* Confirm = Harness.CreateAction(TEXT("IA_Confirm"));
	UInputAction* Next = Harness.CreateAction(TEXT("IA_Next"));
	UInputAction* Unbound = Harness.CreateAction(TEXT("IA_Unbound"));
	UInputMappingContext* Mapping = Harness.CreateMapping({{Confirm, EKeys::Enter}, {Confirm, EKeys::Gamepad_FaceButton_Bottom},
		{Next, EKeys::E}, {Next, EKeys::Tab}, {Next, EKeys::Gamepad_RightShoulder}});
	Environment.ApplyMapping(Mapping);
	UReciMappedKeyText* Current = AddKeyText(*Screen, TEXT("Current"), Confirm);
	UReciMappedKeyText* Keyboard = AddKeyText(*Screen, TEXT("Keyboard"), Confirm);
	Keyboard->SetKeyDevice(EReciKeyTextDevice::KeyboardAndMouse);
	UReciMappedKeyText* Gamepad = AddKeyText(*Screen, TEXT("Gamepad"), Confirm);
	Gamepad->SetKeyDevice(EReciKeyTextDevice::Gamepad);
	// CommonUI's action widget refreshes on every input-method change; the test's input method changes by hand.
	const auto Row = [Current, Keyboard, Gamepad]()
	{
		for (UReciMappedKeyText* KeyText : {Current, Keyboard, Gamepad})
		{
			KeyText->RefreshMappedKeyText();
		}
		return FString::Printf(TEXT("%s | %s | %s"), *Shown(*Current), *Shown(*Keyboard), *Shown(*Gamepad));
	};

	// Current follows the player's device and shows nothing on touch; a fixed device shows its keys on every device.
	TestEqual(TEXT("Without CommonInput's input method no key text shows"), Row(), FString(TEXT("- | - | -")));
	Environment.SetInputType(ECommonInputType::MouseAndKeyboard);
	TestEqual(TEXT("On keyboard: Current, Keyboard and Mouse, Gamepad"), Row(), FString(TEXT("ENTER | ENTER | A")));
	Environment.SetInputType(ECommonInputType::Gamepad);
	TestEqual(TEXT("On gamepad"), Row(), FString(TEXT("A | ENTER | A")));
	Environment.SetInputType(ECommonInputType::Touch);
	TestEqual(TEXT("On touch only the fixed devices show"), Row(), FString(TEXT("- | ENTER | A")));
	Environment.SetInputType(ECommonInputType::MouseAndKeyboard);
	Environment.InputMethod->bShowsInputKeys = false;
	TestEqual(TEXT("Where the platform shows no input keys, only the fixed devices show"), Row(), FString(TEXT("- | ENTER | A")));
	Environment.InputMethod->bShowsInputKeys = true;
	Current->SetKeyDevice(EReciKeyTextDevice::Gamepad);
	TestEqual(TEXT("Set Key Device shows the device's keys at once"), Shown(*Current), FString(TEXT("A")));
	Current->SetKeyDevice(EReciKeyTextDevice::Current);
	TestEqual(TEXT("Back to Current"), Shown(*Current), FString(TEXT("ENTER")));

	// Several actions, an action's keys, the maximum and an action with no key.
	Current->KeySeparator = FText::FromString(TEXT(" / "));
	Current->ActionSeparator = FText::FromString(TEXT(" | "));
	Current->SetEnhancedInputActions({Confirm, Next});
	TestEqual(TEXT("Additional Input Actions follow, joined by Action Separator, and an action's keys by Key Separator"),
		Shown(*Current), FString(TEXT("ENTER | E / TAB")));
	Current->MaximumDisplayedKeys = 1;
	Current->RefreshMappedKeyText();
	TestEqual(TEXT("Maximum Displayed Keys counts each action's keys"), Shown(*Current), FString(TEXT("ENTER | E")));
	Current->MaximumDisplayedKeys = 0;
	Current->bHideWhenUnmapped = false;
	Current->SetEnhancedInputActions({Unbound});
	TestTrue(TEXT("An action with no key shows Unmapped Text"),
		Current->GetVisibility() != ESlateVisibility::Hidden && Current->GetMappedKeyText().IsEmpty());
	Current->bHideWhenUnmapped = true;
	Current->RefreshMappedKeyText();
	TestEqual(TEXT("With Hide When Unmapped it hides"), Shown(*Current), FString(TEXT("-")));

	// Keys come from the player's mappings: after a remap, the next refresh shows the new key.
	Current->SetEnhancedInputActions({Confirm});
	Environment.RemoveMapping(Mapping);
	Environment.ApplyMapping(Harness.CreateMapping({{Confirm, EKeys::F}, {Confirm, EKeys::Gamepad_FaceButton_Left}}));
	TestEqual(TEXT("After a remap every key text shows the new keys"), Row(), FString(TEXT("F | F | X")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciKeyTextRoutingTest, "ReciCommonUI.KeyText.Routing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciKeyTextRoutingTest::RunTest(const FString& Parameters)
{
	if (!HasEnhancedInputSupport(*this))
	{
		return true;
	}
	const FReciScopedControllerData NoControllerData{TArray<TSoftClassPtr<UCommonInputBaseControllerData>>()};
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	FReciTestRoutingEnvironment& Environment = Harness.GetEnvironment();
	Environment.SetInputType(ECommonInputType::MouseAndKeyboard);
	const FReciScopedInputData InputData(*this);
	// A registered screen whose action Next takes E, holding a focused Reci control and a dropdown stand-in.
	UInputAction* Next = Harness.CreateAction(TEXT("IA_Next"));
	UInputAction* Back = Harness.CreateAction(TEXT("IA_Back"));
	UInputAction* Down = Harness.CreateAction(TEXT("IA_Down"));
	UInputAction* Apply = Harness.CreateAction(TEXT("IA_Apply"));
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	Screen->SetInputMapping(Harness.CreateMapping({{Next, EKeys::E}}));
	Screen->HandledActions.Add(Next);
	UReciTestPopupOwner* Dropdown = Harness.CreatePopupOwner(TEXT("Dropdown"), Screen);
	UReciTestFocusTarget* Control = Harness.CreateTarget(TEXT("Control"), Screen);
	Harness.ActivateScreen(*Screen);
	FReciInputFocusCoordinator::Get().RegisterScreenInput(Screen);
	Environment.ApplyMapping(Harness.CreateMapping(
		{{Back, EKeys::Escape}, {Down, EKeys::Down}, {Apply, EKeys::Enter}, {Apply, EKeys::G}, {Apply, EKeys::E}}));
	// Hint-line texts for the screen's action and for Back, a key-binding row's text fixed to the keyboard, and an action
	// bar's button for Apply.
	UReciMappedKeyText* NextHint = AddKeyText(*Screen, TEXT("NextHint"), Next);
	UReciMappedKeyText* BackHint = AddKeyText(*Screen, TEXT("BackHint"), Back);
	UReciMappedKeyText* Row = AddKeyText(*Screen, TEXT("Row"), Next);
	Row->SetKeyDevice(EReciKeyTextDevice::KeyboardAndMouse);
	UReciMappedKeyText* CancelHint = AddKeyText(*Screen, TEXT("CancelHint"), Back);
	CancelHint->bKeyCaptureHint = true;
	UReciTestBoundActionButton* BarButton = CreateWidget<UReciTestBoundActionButton>(
		Harness.GetPlayerController(), UReciTestBoundActionButton::StaticClass(), TEXT("BarButton"));
	Screen->GetContent()->AddChild(BarButton);
	UReciMappedKeyText* BarKeys = BarButton->GetKeyText();
	BarKeys->SetEnhancedInputAction(Apply);
	Harness.Paint();
	TestTrue(TEXT("Focus reaches the control"), Harness.Focus(*Control));
	const auto Line = [NextHint, BackHint, Row, BarKeys]()
	{
		for (UReciMappedKeyText* KeyText : {NextHint, BackHint, Row, BarKeys})
		{
			KeyText->RefreshMappedKeyText();
		}
		return FString::Printf(TEXT("%s | %s | %s | %s"), *Shown(*NextHint), *Shown(*BackHint), *Shown(*Row), *Shown(*BarKeys));
	};

	// A hint shows the keys that reach its action now; the bar leaves out the focused widget's Accept and the screen's E.
	TestEqual(TEXT("Hint line, Back, key-binding row and action bar"), Line(), FString(TEXT("E | ESC | E | G")));
	TestTrue(TEXT("A hint that shows a key says so"), NextHint->ShowsKeys());
	Screen->SuspendedActions.Add(Next);
	TestEqual(TEXT("A suspended screen action's hint shows no key, and its key reaches the bar"), Line(), FString(TEXT("- | ESC | E | G / E")));
	TestFalse(TEXT("A hint whose keys are all left out shows no keys"), NextHint->ShowsKeys());
	Screen->SuspendedActions.Reset();
	CancelHint->RefreshMappedKeyText();
	TestEqual(TEXT("A key-capture hint shows no key while nothing captures"), Shown(*CancelHint), FString(TEXT("-")));
	Screen->bCapturingKeys = true;
	TestEqual(TEXT("A capturing screen takes every key but the key-binding row's"), Line(), FString(TEXT("- | - | E | -")));
	CancelHint->RefreshMappedKeyText();
	TestEqual(TEXT("A key-capture hint shows the key the capture gets"), Shown(*CancelHint), FString(TEXT("ESC")));
	Screen->bCapturingKeys = false;
	Dropdown->Open(FReciTestHarness::UserIndex);
	TestEqual(TEXT("An open popup takes Back and suspends the screen's actions"), Line(), FString(TEXT("- | - | E | G / E")));
	Dropdown->Close();
	TestEqual(TEXT("Closed again"), Line(), FString(TEXT("E | ESC | E | G")));

	// A navigation action's keys go to the focused widget while Reci follows its Reci UI Input Data.
	UReciMappedKeyText* DownHint = AddKeyText(*Screen, TEXT("DownHint"), Down);
	Harness.Paint();
	DownHint->RefreshMappedKeyText();
	TestEqual(TEXT("Otherwise the focused widget takes the direction ahead of the action"), Shown(*DownHint), FString(TEXT("-")));
	InputData.GetDefaults().NavigationDownAction = Down;
	DownHint->RefreshMappedKeyText();
	TestEqual(TEXT("A navigation action's hint shows its key to the focused widget"), Shown(*DownHint), FString(TEXT("↓")));
	InputData.SetInputDataClass(UCommonUIInputData::StaticClass());
	DownHint->RefreshMappedKeyText();
	TestEqual(TEXT("With stock Input Data, which Reci does not follow, the focused widget takes it again"),
		Shown(*DownHint), FString(TEXT("-")));
	InputData.SetInputDataClass(UReciTestInputData::StaticClass());

	// An Is Modal dialog nested in the screen becomes the active root: the screen's hint line shows no key.
	UReciTestScreen* Dialog = Harness.CreateScreen(TEXT("Dialog"), Screen);
	Dialog->SetIsModal(true);
	UReciMappedKeyText* DialogHint = AddKeyText(*Dialog, TEXT("DialogHint"), Back);
	Harness.ActivateScreen(*Dialog);
	Environment.SetActiveRoot(*Dialog);
	Harness.Paint();
	TestTrue(TEXT("Focus reaches the dialog"), Harness.Focus(*Dialog));
	DialogHint->RefreshMappedKeyText();
	TestEqual(TEXT("Beneath the dialog only the key-binding row and the bar show keys"), Line(), FString(TEXT("- | - | E | G / E")));
	TestEqual(TEXT("The dialog's own hint line shows its keys"), Shown(*DialogHint), FString(TEXT("ESC")));
	Harness.DeactivateScreen(*Dialog);
	Environment.SetActiveRoot(*Screen);
	TestTrue(TEXT("Focus returns to the control"), Harness.Focus(*Control));
	TestEqual(TEXT("When the dialog closes the hint line shows again"), Line(), FString(TEXT("E | ESC | E | G")));

	// A screen that deactivates while another root is active keeps its hints as it fades out.
	Environment.SetActiveRoot(*Dialog);
	Harness.DeactivateScreen(*Screen);
	BackHint->RefreshMappedKeyText();
	TestEqual(TEXT("A deactivated screen's hint line still shows its keys"), Shown(*BackHint), FString(TEXT("ESC")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciKeyTextLayoutTest, "ReciCommonUI.KeyText.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciKeyTextLayoutTest::RunTest(const FString& Parameters)
{
	if (!HasEnhancedInputSupport(*this))
	{
		return true;
	}
	const UCommonInputPlatformSettings* PlatformSettings = UCommonInputPlatformSettings::Get();
	if (!TestNotNull(TEXT("The platform has Common Input settings"), PlatformSettings))
	{
		return false;
	}
	const FReciScopedControllerData NoControllerData{TArray<TSoftClassPtr<UCommonInputBaseControllerData>>()};
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	FReciTestRoutingEnvironment& Environment = Harness.GetEnvironment();
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	Harness.ActivateScreen(*Screen);
	UInputAction* Confirm = Harness.CreateAction(TEXT("IA_Confirm"));
	Environment.ApplyMapping(Harness.CreateMapping({{Confirm, EKeys::Enter}, {Confirm, EKeys::Gamepad_FaceButton_Bottom}}));
	// Reserve All Device Widths on (the default), off, and a key text fixed to the gamepad; all hide when unmapped, as a
	// button's hint does.
	UReciMappedKeyText* Reserved = AddKeyText(*Screen, TEXT("Reserved"), Confirm);
	UReciMappedKeyText* Tight = AddKeyText(*Screen, TEXT("Tight"), Confirm);
	Tight->bReserveAllDeviceWidths = false;
	UReciMappedKeyText* PadOnly = AddKeyText(*Screen, TEXT("PadOnly"), Confirm);
	PadOnly->SetKeyDevice(EReciKeyTextDevice::Gamepad);
	for (UReciMappedKeyText* KeyText : {Reserved, Tight, PadOnly})
	{
		KeyText->bHideWhenUnmapped = true;
	}
	const auto Measure = [&Harness](UReciMappedKeyText& KeyText)
	{
		KeyText.RefreshMappedKeyText();
		Harness.Paint();
		const TSharedPtr<SWidget> Widget = KeyText.GetCachedWidget();
		return Widget ? Widget->GetDesiredSize() : FVector2D::ZeroVector;
	};

	Environment.SetInputType(ECommonInputType::MouseAndKeyboard);
	const FVector2D ReservedOnKeyboard = Measure(*Reserved);
	const FVector2D TightOnKeyboard = Measure(*Tight);
	Environment.SetInputType(ECommonInputType::Gamepad);
	const FVector2D ReservedOnGamepad = Measure(*Reserved);
	const FVector2D TightOnGamepad = Measure(*Tight);
	const FVector2D PadOnlySize = Measure(*PadOnly);
	TestTrue(TEXT("A key text without reserved room is as wide as its keys: A is narrower than ENTER"), TightOnGamepad.X < TightOnKeyboard.X - 1.0);
	TestEqual(TEXT("Reserve All Device Widths keeps one width on keyboard and gamepad"), ReservedOnGamepad.X, ReservedOnKeyboard.X, 0.01);
	TestEqual(TEXT("That width is the widest device's keys"), ReservedOnKeyboard.X, TightOnKeyboard.X, 0.01);
	TestEqual(TEXT("A key text fixed to a device reserves room for that device's keys only"), PadOnlySize.X, TightOnGamepad.X, 0.01);
	TestTrue(TEXT("Padding is part of the width"), TightOnGamepad.X >= Reserved->Padding.Left + Reserved->Padding.Right + 1.0);
	TestTrue(TEXT("A row is at least Minimum Reserved Height high"), ReservedOnKeyboard.Y >= Reserved->MinimumReservedHeight);
	Reserved->MinimumReservedHeight = 60.0f;
	TestEqual(TEXT("A taller Minimum Reserved Height raises the row"), Measure(*Reserved).Y, 60.0, 0.01);
	Reserved->MinimumReservedHeight = 24.0f;

	// With Hide When Unmapped off, the room also fits Unmapped Text, which the key text shows when the keys go.
	Reserved->bHideWhenUnmapped = false;
	Reserved->UnmappedText = FText::FromString(TEXT("NO KEY IS BOUND TO THIS"));
	TestTrue(TEXT("The room fits Unmapped Text"), Measure(*Reserved).X > ReservedOnGamepad.X + 1.0);
	Reserved->bHideWhenUnmapped = true;
	TestEqual(TEXT("With Hide When Unmapped on it fits the keys alone"), Measure(*Reserved).X, ReservedOnGamepad.X, 0.01);

	// Keys a filter leaves out keep their room, so the layout does not move while another handler takes them.
	Environment.SetInputType(ECommonInputType::MouseAndKeyboard);
	Reserved->SetKeyFilter([](const FKey& Key) { return Key != EKeys::Enter; });
	const FVector2D Filtered = Measure(*Reserved);
	TestEqual(TEXT("A key text whose keys are all left out hides"), Shown(*Reserved), FString(TEXT("-")));
	TestEqual(TEXT("It keeps its width"), Filtered.X, ReservedOnKeyboard.X, 0.01);
	Reserved->SetKeyFilter(nullptr);

	// A glyph from controller data is drawn at its brush's size, and the reserved room counts it on every device.
	UReciTestControllerData* Data = GetMutableDefault<UReciTestControllerData>();
	ON_SCOPE_EXIT
	{
		Data->InputBrushDataMap.Reset();
	};
	Data->InputType = ECommonInputType::Gamepad;
	Data->GamepadName = PlatformSettings->GetDefaultGamepadName();
	FCommonInputKeyBrushConfiguration Wide;
	Wide.Key = EKeys::Gamepad_FaceButton_Bottom;
	Wide.KeyBrush = MakeBrush(200.0, 20.0);
	Data->InputBrushDataMap = {Wide};
	const FReciScopedControllerData ControllerData({TSoftClassPtr<UCommonInputBaseControllerData>(UReciTestControllerData::StaticClass())});
	const FVector2D WithGlyphOnKeyboard = Measure(*Reserved);
	TestEqual(TEXT("On keyboard the room covers the gamepad's glyph"), WithGlyphOnKeyboard.X,
		200.0 + Reserved->Padding.Left + Reserved->Padding.Right, 0.01);
	Environment.SetInputType(ECommonInputType::Gamepad);
	TestEqual(TEXT("On gamepad the glyph shows in the same room"), Measure(*Reserved).X, WithGlyphOnKeyboard.X, 0.01);
	TestEqual(TEXT("Get Mapped Key Text still names a key drawn as a glyph"), Reserved->GetMappedKeyText().ToString(), FString(TEXT("A")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciKeyTextStyleTest, "ReciCommonUI.KeyText.Style",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciKeyTextStyleTest::RunTest(const FString& Parameters)
{
	if (!HasEnhancedInputSupport(*this))
	{
		return true;
	}
	const FReciScopedControllerData NoControllerData{TArray<TSoftClassPtr<UCommonInputBaseControllerData>>()};
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	FReciTestRoutingEnvironment& Environment = Harness.GetEnvironment();
	Environment.SetInputType(ECommonInputType::MouseAndKeyboard);
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	Harness.ActivateScreen(*Screen);
	UInputAction* Confirm = Harness.CreateAction(TEXT("IA_Confirm"));
	Environment.ApplyMapping(Harness.CreateMapping({{Confirm, EKeys::Enter}, {Confirm, EKeys::Gamepad_FaceButton_Bottom}}));
	const UCommonTextStyle* TextStyle = GetDefault<UReciTestTextStyle>();
	// A key text with literals of its own, and a twin whose literal font is the style's.
	UReciMappedKeyText* KeyText = AddKeyText(*Screen, TEXT("KeyText"), Confirm);
	const FSlateFontInfo LiteralFont = FCoreStyle::GetDefaultFontStyle(TEXT("Italic"), 18);
	const FLinearColor LiteralColor(0.75f, 0.5f, 0.25f);
	const FVector2D LiteralShadowOffset(1.0, 1.0);
	const FLinearColor LiteralShadowColor(0.3f, 0.2f, 0.1f);
	KeyText->Font = LiteralFont;
	KeyText->ColorAndOpacity = FSlateColor(LiteralColor);
	KeyText->ShadowOffset = LiteralShadowOffset;
	KeyText->ShadowColorAndOpacity = LiteralShadowColor;
	UReciMappedKeyText* Twin = AddKeyText(*Screen, TEXT("Twin"), Confirm);
	Twin->Font = TextStyle->Font;
	for (UReciMappedKeyText* Text : {KeyText, Twin})
	{
		Text->SynchronizeProperties();
	}
	const auto Measure = [&Harness](UReciMappedKeyText& Text)
	{
		Text.RefreshMappedKeyText();
		Harness.Paint();
		const TSharedPtr<SWidget> Widget = Text.GetCachedWidget();
		return Widget ? Widget->GetDesiredSize() : FVector2D::ZeroVector;
	};
	// The one text that shows the key's name, or none.
	const auto ShownText = [](const UReciMappedKeyText& Text)
	{
		const TArray<TSharedRef<STextBlock>> Texts = FReciMappedKeyTextTestAccess::GetShownTexts(Text);
		return Texts.Num() == 1 ? Texts[0].ToSharedPtr() : TSharedPtr<STextBlock>();
	};
	const auto Draws = [this](const TSharedPtr<STextBlock>& Text, const TCHAR* What, const FSlateFontInfo& Font,
		const FLinearColor& Color, const FVector2D& ShadowOffset, const FLinearColor& ShadowColor)
	{
		if (!TestTrue(FString::Printf(TEXT("%s: the key's name shows in one text"), What), Text.IsValid()))
		{
			return;
		}
		TestTrue(FString::Printf(TEXT("%s: its font"), What), Text->GetFont().IsIdenticalTo(Font));
		TestTrue(FString::Printf(TEXT("%s: its color"), What), Text->GetColorAndOpacity().GetSpecifiedColor().Equals(Color));
		TestTrue(FString::Printf(TEXT("%s: its shadow offset"), What), FVector2D(Text->GetShadowOffset()).Equals(ShadowOffset));
		TestTrue(FString::Printf(TEXT("%s: its shadow color"), What), Text->GetShadowColorAndOpacity().Equals(ShadowColor));
	};

	// Without a Style the literals draw the keys, as before Style existed.
	const FVector2D LiteralSize = Measure(*KeyText);
	Draws(ShownText(*KeyText), TEXT("Without a Style the literals draw the key"), LiteralFont, LiteralColor, LiteralShadowOffset,
		LiteralShadowColor);

	// A Style's font, color and shadow replace the literals; its margin does not apply, and the room is measured in its font.
	KeyText->Style = UReciTestTextStyle::StaticClass();
	KeyText->SynchronizeProperties();
	const TSharedPtr<STextBlock> Styled = ShownText(*KeyText);
	Draws(Styled, TEXT("With a Style the style draws the key"), TextStyle->Font, TextStyle->Color, TextStyle->ShadowOffset,
		TextStyle->ShadowColor);
	TestTrue(TEXT("The style's margin does not apply: Padding spaces the keys"), Styled.IsValid() && Styled->GetMargin() == FMargin(0.0f));
	const FVector2D StyledSize = Measure(*KeyText);
	TestTrue(TEXT("The key text is laid out in the style's font"), StyledSize.X > LiteralSize.X + 1.0);
	TestEqual(TEXT("as wide as a key text whose literal font is the style's"), StyledSize.X, Measure(*Twin).X, 0.01);
	// There the hidden Unmapped Text sets the room. The hidden rows reserve each device's keys in the style's font too: on
	// gamepad, with Hide When Unmapped on, the keyboard's ENTER sets the room, not the A shown.
	Environment.SetInputType(ECommonInputType::Gamepad);
	for (UReciMappedKeyText* Text : {KeyText, Twin})
	{
		Text->bHideWhenUnmapped = true;
	}
	TestEqual(TEXT("On gamepad the keyboard's keys reserve the room in the style's font"), Measure(*KeyText).X, Measure(*Twin).X, 0.01);
	for (UReciMappedKeyText* Text : {KeyText, Twin})
	{
		Text->bHideWhenUnmapped = false;
	}
	Environment.SetInputType(ECommonInputType::MouseAndKeyboard);
	TestTrue(TEXT("The literals stay as they were"), KeyText->Font.IsIdenticalTo(LiteralFont)
		&& KeyText->ColorAndOpacity.GetSpecifiedColor().Equals(LiteralColor));
#if WITH_EDITOR
	const FProperty* FontProperty = FindFProperty<FProperty>(UReciMappedKeyText::StaticClass(), GET_MEMBER_NAME_CHECKED(UReciMappedKeyText, Font));
	const FProperty* PaddingProperty = FindFProperty<FProperty>(UReciMappedKeyText::StaticClass(), GET_MEMBER_NAME_CHECKED(UReciMappedKeyText, Padding));
	TestFalse(TEXT("The details panel keeps a literal the style replaces read-only"), KeyText->CanEditChange(FontProperty));
	TestTrue(TEXT("but not Padding"), KeyText->CanEditChange(PaddingProperty));
#endif

	// Clearing the Style brings the literals back.
	KeyText->Style = nullptr;
	KeyText->SynchronizeProperties();
	Draws(ShownText(*KeyText), TEXT("With the Style cleared the literals draw the key"), LiteralFont, LiteralColor,
		LiteralShadowOffset, LiteralShadowColor);
	TestEqual(TEXT("in the literal font's room"), Measure(*KeyText).X, LiteralSize.X, 0.01);
#if WITH_EDITOR
	TestTrue(TEXT("and the details panel edits the literals again"), KeyText->CanEditChange(FontProperty));
#endif
	return true;
}

#endif
