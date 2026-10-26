// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/UserWidget.h"
#include "CommonButtonBase.h"
#include "CommonInputSettings.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "ICommonInputModule.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Tests/ReciTestHarness.h"
#include "Widgets/ReciListBase.h"
#include "Widgets/ReciScrollBox.h"

namespace
{
	UObject* GetListPageVariable(UUserWidget& Widget, const TCHAR* Name)
	{
		const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Widget.GetClass(), Name);
		return Property ? Property->GetObjectPropertyValue_InContainer(&Widget) : nullptr;
	}

	/** The Value of the list's item at Index (a BP_DemoItem), or -1 when it has none. */
	int32 GetItemValue(const UReciListBase& List, const int32 Index)
	{
		const UObject* Item = List.GetItemAt(Index);
		const FIntProperty* Property = Item ? FindFProperty<FIntProperty>(Item->GetClass(), TEXT("Value")) : nullptr;
		return Property ? Property->GetPropertyValue_InContainer(Item) : -1;
	}

	/** The text the entry at Index shows in its Value block. */
	FString GetShownValue(const UReciListBase& List, const int32 Index)
	{
		const UUserWidget* Entry = Cast<UUserWidget>(List.GetHostedEntryWidget(Index));
		const UTextBlock* Value = Entry ? Cast<UTextBlock>(Entry->GetWidgetFromName(TEXT("Value"))) : nullptr;
		return Value ? Value->GetText().ToString() : FString();
	}
}

// The demo's list page (Content/Demo/Pages/WBP_DemoListPage), laid out and painted: its Event Construct fills the list,
// which takes no focus from the widget beside it that holds focus. Left and Right step the focused entry
// by one, because the page steps the focused entry on the list's On Navigation Input. The D-pad and the left stick do the
// same, a held key steps on each repeat, no other entry changes, and Up and Down still move between the entries; at the
// first entry Up is left to Slate. With the navigation config Reci installs, a diagonal stick steps the value or moves
// focus, never both. A sideways drag on another entry steps that entry, not the focused one: its scroll box gives it
// focus before it broadcasts the direction.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciDemoListPageTest, "ReciCommonUI.Demo.ListPage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciDemoListPageTest::RunTest(const FString& Parameters)
{
	UClass* PageClass = LoadClass<UUserWidget>(nullptr, TEXT("/ReciCommonUI/Demo/Pages/WBP_DemoListPage.WBP_DemoListPage_C"));
	if (!TestNotNull(TEXT("The page's Blueprint class loads"), PageClass))
	{
		return false;
	}
	// A focused entry shows the click action's key, which CommonUI's input subsystem loads when a local player starts;
	// the test has none.
	ICommonInputModule::GetSettings().LoadData();
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	Harness.ActivateScreen(*Screen);
	// The page fills its list in Event Construct (Set List Items) while focus is on a widget beside it.
	UReciTestFocusTarget* Beside = Harness.CreateTarget(TEXT("Beside"), Screen);
	Harness.Paint();
	Harness.Focus(*Beside);
	UUserWidget* Page = CreateWidget<UUserWidget>(Harness.GetPlayerController(), PageClass);
	Screen->GetContent()->AddChild(Page);
	Harness.Advance(1.0f);

	UReciListBase* Items = Cast<UReciListBase>(GetListPageVariable(*Page, TEXT("Items")));
	if (!TestNotNull(TEXT("The page holds its list"), Items) || !TestEqual(TEXT("The list has its twelve entries"), Items->GetItemCount(), 12))
	{
		return false;
	}
	TestTrue(TEXT("Filling the list in Event Construct leaves focus beside it"), Beside->HasUserFocus(Harness.GetPlayerController()));
	TestEqual(TEXT("and gives no entry focus"), Items->GetFocusedItemIndex(), INDEX_NONE);
	const int32 Stepped = 2;
	// Every entry other than the stepped one keeps the value the page gives it, (index + 1) * 5.
	const auto OthersUnchanged = [Items, Stepped]()
	{
		for (int32 Index = 0; Index < Items->GetItemCount(); ++Index)
		{
			if (Index != Stepped && (GetItemValue(*Items, Index) != (Index + 1) * 5 || GetShownValue(*Items, Index) != FString::FromInt((Index + 1) * 5)))
			{
				return false;
			}
		}
		return true;
	};
	TestEqual(TEXT("The third entry starts at 15"), GetItemValue(*Items, Stepped), 15);
	TestTrue(TEXT("Every other entry starts at its own value"), OthersUnchanged());
	UCommonButtonBase* Entry = Items->GetHostedEntryButton(Stepped);
	if (!TestNotNull(TEXT("The third entry exists"), Entry))
	{
		return false;
	}
	const auto Press = [&Harness](const FKey& Key)
	{
		const bool bHandled = Harness.KeyDown(Key);
		Harness.KeyUp(Key);
		return bHandled;
	};

	// Left and Right step the focused entry by one, and focus stays on it.
	TestTrue(TEXT("Focus reaches the third entry"), Harness.Focus(*Entry));
	TestEqual(TEXT("The list's focused entry is the third"), Items->GetFocusedItemIndex(), Stepped);
	TestTrue(TEXT("Right on the focused entry is taken"), Press(EKeys::Right));
	TestEqual(TEXT("Right adds one to its value"), GetItemValue(*Items, Stepped), 16);
	TestEqual(TEXT("The entry shows the new value"), GetShownValue(*Items, Stepped), FString(TEXT("16")));
	TestEqual(TEXT("Focus stays on the entry"), Items->GetFocusedItemIndex(), Stepped);
	TestTrue(TEXT("Left on the focused entry is taken"), Press(EKeys::Left));
	TestEqual(TEXT("Left takes one away"), GetItemValue(*Items, Stepped), 15);

	// A held key steps on its press and on each repeat.
	Harness.KeyDown(EKeys::Right);
	Harness.KeyDown(EKeys::Right, true);
	Harness.KeyDown(EKeys::Right, true);
	Harness.KeyUp(EKeys::Right);
	TestEqual(TEXT("A press and two repeats step three times"), GetItemValue(*Items, Stepped), 18);

	// The D-pad and the left stick step the same way; the stick steps once for a deflection.
	TestTrue(TEXT("D-pad Right is taken"), Press(EKeys::Gamepad_DPad_Right));
	TestEqual(TEXT("D-pad Right adds one"), GetItemValue(*Items, Stepped), 19);
	TestTrue(TEXT("D-pad Left is taken"), Press(EKeys::Gamepad_DPad_Left));
	TestEqual(TEXT("D-pad Left takes one away"), GetItemValue(*Items, Stepped), 18);
	TestTrue(TEXT("A stick deflection to the right is taken"), Harness.AnalogInput(EKeys::Gamepad_LeftX, 0.9f));
	TestEqual(TEXT("It adds one"), GetItemValue(*Items, Stepped), 19);
	Harness.AnalogInput(EKeys::Gamepad_LeftX, 0.0f);
	TestTrue(TEXT("A stick deflection to the left is taken"), Harness.AnalogInput(EKeys::Gamepad_LeftX, -0.9f));
	TestEqual(TEXT("It takes one away"), GetItemValue(*Items, Stepped), 18);
	Harness.AnalogInput(EKeys::Gamepad_LeftX, 0.0f);

	// With the navigation config Reci installs, whose Stick Dominant Axis is on by default, a stick moves one direction at
	// a time, along the axis further from rest: a diagonal steps the value or moves focus, never both.
	Harness.UseReciNavigationConfig(true);
	TestTrue(TEXT("A stick pushed right is taken"), Harness.AnalogInput(EKeys::Gamepad_LeftX, 0.6f));
	TestFalse(TEXT("Pushed on to the diagonal, further down, its down is not taken"), Harness.AnalogInput(EKeys::Gamepad_LeftY, -0.8f));
	TestEqual(TEXT("The diagonal adds one to the value"), GetItemValue(*Items, Stepped), 19);
	TestEqual(TEXT("and moves no focus"), Items->GetFocusedItemIndex(), Stepped);
	Harness.AnalogInput(EKeys::Gamepad_LeftX, 0.0f);
	Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.0f);
	TestTrue(TEXT("A stick pushed down is taken"), Harness.AnalogInput(EKeys::Gamepad_LeftY, -0.6f));
	TestFalse(TEXT("Pushed on to the diagonal, further right, its right is not taken"), Harness.AnalogInput(EKeys::Gamepad_LeftX, 0.8f));
	TestEqual(TEXT("The diagonal moves focus down one"), Items->GetFocusedItemIndex(), Stepped + 1);
	TestEqual(TEXT("and changes no value"), GetItemValue(*Items, Stepped + 1), (Stepped + 2) * 5);
	TestEqual(TEXT("The entry it left keeps its value"), GetItemValue(*Items, Stepped), 19);
	Harness.AnalogInput(EKeys::Gamepad_LeftX, 0.0f);
	Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.0f);
	// Back to the stepped entry, at the value the checks below expect.
	TestTrue(TEXT("Focus returns to the stepped entry"), Harness.Focus(*Entry));
	Press(EKeys::Left);
	TestEqual(TEXT("The entry shows its value"), GetShownValue(*Items, Stepped), FString(TEXT("18")));
	TestEqual(TEXT("Focus is still on the entry"), Items->GetFocusedItemIndex(), Stepped);
	TestTrue(TEXT("No other entry changed"), OthersUnchanged());

	// Up and Down still move between the entries and change no value; at the first entry Up is left to Slate.
	TestTrue(TEXT("Down is taken"), Press(EKeys::Down));
	TestEqual(TEXT("Down moves to the next entry"), Items->GetFocusedItemIndex(), Stepped + 1);
	TestEqual(TEXT("Moving keeps the stepped value"), GetItemValue(*Items, Stepped), 18);
	TestTrue(TEXT("Moving changes no other value"), OthersUnchanged());
	UCommonButtonBase* First = Items->GetHostedEntryButton(0);
	if (TestNotNull(TEXT("The first entry exists"), First))
	{
		TestTrue(TEXT("Focus reaches the first entry"), Harness.Focus(*First));
		TestFalse(TEXT("Up at the first entry is left to Slate"), Press(EKeys::Up));
		TestTrue(TEXT("Up changes no value"), OthersUnchanged());
	}

	// A sideways drag on an entry acts on that entry, as a press on it does, while another entry has focus: the list's
	// scroll box gives the dragged entry focus before it broadcasts the direction, so the page steps it and no other. A
	// drag to the left reads as Right, as on a carousel. The drag selects nothing and does not scroll.
	UReciScrollBox* Scroll = Cast<UReciScrollBox>(Page->GetWidgetFromName(TEXT("ItemsScroll")));
	if (TestNotNull(TEXT("The page's list is in its ItemsScroll"), Scroll)
		&& TestNotNull(TEXT("The second entry exists"), Items->GetHostedEntryWidget(1))
		&& TestNotNull(TEXT("The fourth entry exists"), Items->GetHostedEntryWidget(3)))
	{
		TestTrue(TEXT("Focus returns to the third entry"), Harness.Focus(*Entry));
		Harness.Advance(0.5f);
		const int32 SelectedBefore = Items->GetSelectedItemIndex();
		const float OffsetBefore = Scroll->GetScrollOffset();
		IReciInputFocusTarget* Target = Scroll;
		// A mouse press a quarter of the way along an entry, on its label and clear of its -/+, moved sideways by DeltaX
		// in four steps and released there.
		const auto DragSideways = [Items, Scroll, Target](const int32 Index, const double DeltaX)
		{
			const FGeometry& Geometry = Items->GetHostedEntryWidget(Index)->GetCachedGeometry();
			const FVector2D Start(Geometry.GetAbsolutePositionAtCoordinates(FVector2f(0.25f, 0.5f)));
			const TSet<FKey> Pressed = {EKeys::LeftMouseButton};
			Target->HandleReciPointerDown(Scroll->GetCachedGeometry(),
				FPointerEvent(FReciTestHarness::UserIndex, 0, Start, Start, Pressed, EKeys::LeftMouseButton, 0.0f, FModifierKeysState()));
			FVector2D Last = Start;
			for (int32 Step = 1; Step <= 4; ++Step)
			{
				const FVector2D Point = Start + FVector2D(DeltaX * Step / 4.0, 0.0);
				Target->HandleReciPointerMove(Scroll->GetCachedGeometry(),
					FPointerEvent(FReciTestHarness::UserIndex, 0, Point, Last, Pressed, FKey(), 0.0f, FModifierKeysState()));
				Last = Point;
			}
			Target->HandleReciPointerUp(
				FPointerEvent(FReciTestHarness::UserIndex, 0, Last, Last, TSet<FKey>(), EKeys::LeftMouseButton, 0.0f, FModifierKeysState()));
		};
		DragSideways(3, -40.0);
		TestEqual(TEXT("The dragged fourth entry takes focus"), Items->GetFocusedItemIndex(), 3);
		TestEqual(TEXT("A drag to the left adds one to the dragged entry"), GetItemValue(*Items, 3), 21);
		TestEqual(TEXT("and it shows the new value"), GetShownValue(*Items, 3), FString(TEXT("21")));
		TestEqual(TEXT("The entry that had focus keeps its value"), GetItemValue(*Items, Stepped), 18);
		DragSideways(1, 40.0);
		TestEqual(TEXT("A drag to the right on the second entry gives it focus"), Items->GetFocusedItemIndex(), 1);
		TestEqual(TEXT("and takes one from its value"), GetItemValue(*Items, 1), 9);
		TestEqual(TEXT("A sideways drag selects nothing"), Items->GetSelectedItemIndex(), SelectedBefore);
		TestEqual(TEXT("and does not scroll"), Scroll->GetScrollOffset(), OffsetBefore);
		bool bOthersKept = true;
		for (int32 Index = 0; Index < Items->GetItemCount(); ++Index)
		{
			const int32 Expected = Index == 1 ? 9 : Index == Stepped ? 18 : Index == 3 ? 21 : (Index + 1) * 5;
			bOthersKept &= GetItemValue(*Items, Index) == Expected && GetShownValue(*Items, Index) == FString::FromInt(Expected);
		}
		TestTrue(TEXT("No other entry changed"), bOthersKept);
	}

	Page->ReleaseSlateResources(true);
	return true;
}

#endif
