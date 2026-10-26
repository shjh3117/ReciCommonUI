// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Tests/ReciInteractionTestEntry.h"
#include "Tests/ReciTestHarness.h"
#include "Tests/ReciTestWidgets.h"
#include "Widgets/ReciList.h"
#include "Widgets/ReciListLayoutBox.h"
#include "Widgets/ReciScrollBox.h"
#include "Widgets/ReciTextListItem.h"

// A list's On Navigation Input: every direction routed to the focused list is broadcast. While a handler is bound, the
// list keeps its cross-axis directions (Left and Right in a vertical list), from keys and the stick, so they never reach
// Slate navigation; with none bound, they go on to Slate. A direction along the axis is broadcast too: it moves the
// list, and at an end it still goes on to Slate.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciListNavigationInputTest, "ReciCommonUI.List.NavigationInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciListNavigationInputTest::RunTest(const FString& Parameters)
{
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	Harness.ActivateScreen(*Screen);
	UReciVerticalBox* Layout = Screen->WidgetTree->ConstructWidget<UReciVerticalBox>(UReciVerticalBox::StaticClass(), TEXT("Layout"));
	UReciList* List = Screen->WidgetTree->ConstructWidget<UReciList>(UReciList::StaticClass(), TEXT("Entries"));
	List->ItemWidgetClass = UReciInteractionTestEntry::StaticClass();
	Layout->SetContent(List);
	Screen->GetContent()->AddChild(Layout);
	TArray<UObject*> Items;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Items.Add(NewObject<UReciTextListItemData>(List));
	}
	List->SetListItems(Items);
	Harness.Paint();
	UReciTestEventRecorder* Recorder = NewObject<UReciTestEventRecorder>(Harness.GetWorld());
	const auto Press = [&Harness](const FKey& Key)
	{
		const bool bHandled = Harness.KeyDown(Key);
		Harness.KeyUp(Key);
		Harness.Paint();
		return bHandled;
	};
	TestTrue(TEXT("The second entry takes focus"), List->SetDrivenItemIndex(1, true));
	Harness.Paint();

	// With no handler bound, Left and Right are not the vertical list's: they go on to Slate navigation.
	TestFalse(TEXT("With no handler, Left is left to Slate"), Press(EKeys::Left));
	TestFalse(TEXT("and so is Right"), Press(EKeys::Right));
	TestEqual(TEXT("Focus stays on the second entry"), List->GetFocusedItemIndex(), 1);

	// With a handler bound, the list broadcasts each cross-axis direction and keeps it: keys, the D-pad and the stick.
	List->OnNavigationInput.AddDynamic(Recorder, &UReciTestEventRecorder::RecordNavigation);
	TestTrue(TEXT("With a handler, Left is taken"), Press(EKeys::Left));
	TestTrue(TEXT("D-pad Right is taken"), Press(EKeys::Gamepad_DPad_Right));
	TestTrue(TEXT("A stick deflection to the right is taken"), Harness.AnalogInput(EKeys::Gamepad_LeftX, 0.9f));
	Harness.AnalogInput(EKeys::Gamepad_LeftX, 0.0f);
	TestEqual(TEXT("Each is broadcast once"), Recorder->ToString(), FString(TEXT("Navigation Left, Navigation Right, Navigation Right")));
	TestEqual(TEXT("None of them moves the list"), List->GetFocusedItemIndex(), 1);
	Recorder->Reset();

	// Along the axis the list still moves, and the direction is broadcast; at the first entry it is broadcast and left to
	// Slate.
	TestTrue(TEXT("Up moves to the first entry"), Press(EKeys::Up));
	TestEqual(TEXT("The first entry has focus"), List->GetFocusedItemIndex(), 0);
	TestFalse(TEXT("Up at the first entry is left to Slate"), Press(EKeys::Up));
	TestEqual(TEXT("Focus stays on the first entry"), List->GetFocusedItemIndex(), 0);
	TestEqual(TEXT("Both are broadcast"), Recorder->ToString(), FString(TEXT("Navigation Up, Navigation Up")));
	Recorder->Reset();

	// With the handler removed, Left goes on to Slate again.
	List->OnNavigationInput.RemoveDynamic(Recorder, &UReciTestEventRecorder::RecordNavigation);
	TestFalse(TEXT("With the handler removed, Left is left to Slate"), Press(EKeys::Left));
	TestEqual(TEXT("and nothing is broadcast"), Recorder->ToString(), FString());
	return true;
}

// A drag on a list's Reci Scroll Box broadcasts On Navigation Input once per drag past the Drag Trigger Distance; a
// shorter drag is a click. A drag across the box's axis acts on the entry it began on, as a press on it does: that entry
// becomes the driven item, with focus, before the direction is broadcast, so a handler that acts on the focused entry acts
// on the dragged one; the content does not scroll and nothing is clicked. A drag across that begins on a disabled entry is
// not broadcast and moves nothing. A drag along the axis scrolls, and focus stays on the driven item. With no handler
// bound, a sideways drag on an entry is a click.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciListNavigationInputDragTest, "ReciCommonUI.List.NavigationInputDrag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciListNavigationInputDragTest::RunTest(const FString& Parameters)
{
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	Harness.ActivateScreen(*Screen);
	// A 200 x 100 Reci Scroll Box over a Reci Vertical Box of four 40 px entries: the first two are in view.
	UWidgetTree& Tree = *Screen->WidgetTree;
	USizeBox* Frame = Tree.ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Frame"));
	Frame->SetWidthOverride(200.0f);
	Frame->SetHeightOverride(100.0f);
	UReciScrollBox* Scroll = Tree.ConstructWidget<UReciScrollBox>(UReciScrollBox::StaticClass(), TEXT("Scroll"));
	UReciVerticalBox* Layout = Tree.ConstructWidget<UReciVerticalBox>(UReciVerticalBox::StaticClass(), TEXT("Layout"));
	UReciList* List = Tree.ConstructWidget<UReciList>(UReciList::StaticClass(), TEXT("Entries"));
	List->ItemWidgetClass = UReciTestListEntry::StaticClass();
	Layout->SetContent(List);
	Scroll->AddChild(Layout);
	Frame->SetContent(Scroll);
	UOverlaySlot* FrameSlot = Screen->GetContent()->AddChildToOverlay(Frame);
	FrameSlot->SetHorizontalAlignment(HAlign_Left);
	FrameSlot->SetVerticalAlignment(VAlign_Top);
	TArray<UObject*> Items;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Items.Add(NewObject<UReciTextListItemData>(List));
	}
	List->SetListItems(Items);
	Harness.Paint();
	if (!TestNotNull(TEXT("The first entry exists"), List->GetHostedEntryWidget(0))
		|| !TestNotNull(TEXT("The second entry exists"), List->GetHostedEntryWidget(1)))
	{
		return false;
	}

	// A mouse press at an entry's centre, moved by Delta in four steps and released there, as the input coordinator routes
	// them to the scroll box.
	IReciInputFocusTarget* Target = Scroll;
	const auto Drag = [&Harness, Target, Scroll, List](const int32 ItemIndex, const FVector2D& Delta)
	{
		const FGeometry& Entry = List->GetHostedEntryWidget(ItemIndex)->GetCachedGeometry();
		const FVector2D Start = Entry.LocalToAbsolute(Entry.GetLocalSize() * 0.5);
		const TSet<FKey> Pressed = {EKeys::LeftMouseButton};
		Target->HandleReciPointerDown(Scroll->GetCachedGeometry(),
			FPointerEvent(FReciTestHarness::UserIndex, 0, Start, Start, Pressed, EKeys::LeftMouseButton, 0.0f, FModifierKeysState()));
		FVector2D Last = Start;
		for (int32 Step = 1; Step <= 4; ++Step)
		{
			const FVector2D Point = Start + Delta * (Step / 4.0);
			Target->HandleReciPointerMove(Scroll->GetCachedGeometry(),
				FPointerEvent(FReciTestHarness::UserIndex, 0, Point, Last, Pressed, FKey(), 0.0f, FModifierKeysState()));
			Last = Point;
		}
		Target->HandleReciPointerUp(
			FPointerEvent(FReciTestHarness::UserIndex, 0, Last, Last, TSet<FKey>(), EKeys::LeftMouseButton, 0.0f, FModifierKeysState()));
		Harness.Paint();
	};

	// With no handler bound, a sideways drag on an entry neither scrolls nor drags: its release is a click, which selects
	// the entry.
	TestTrue(TEXT("The first entry takes focus"), List->SetDrivenItemIndex(0, true));
	Harness.Paint();
	Drag(1, FVector2D(-20.0, 0.0));
	TestEqual(TEXT("With no handler, a sideways drag on the second entry clicks it: it is selected"), List->GetSelectedItemIndex(), 1);
	TestEqual(TEXT("and focused"), List->GetFocusedItemIndex(), 1);
	TestEqual(TEXT("and nothing scrolls"), Scroll->GetScrollOffset(), 0.0f);

	// With a handler bound, a drag across the list on the first entry, while the second has focus, first moves focus to
	// the first, then broadcasts; a drag to the left reads as Right, as on a carousel.
	UReciTestEventRecorder* Recorder = NewObject<UReciTestEventRecorder>(Harness.GetWorld());
	List->OnNavigationInput.AddDynamic(Recorder, &UReciTestEventRecorder::RecordNavigation);
	List->OnFocusedItemChanged.AddDynamic(Recorder, &UReciTestEventRecorder::RecordItem);
	Drag(0, FVector2D(-20.0, 0.0));
	TestEqual(TEXT("Focus moves to the dragged entry before the direction is broadcast"), Recorder->ToString(), FString(TEXT("Item 0, Navigation Right")));
	TestEqual(TEXT("The dragged entry has focus"), List->GetFocusedItemIndex(), 0);
	TestEqual(TEXT("and is the driven item"), List->GetDrivenItemIndex(), 0);
	TestEqual(TEXT("The drag clicks nothing: the selection stays on the second entry"), List->GetSelectedItemIndex(), 1);
	TestEqual(TEXT("and it does not scroll"), Scroll->GetScrollOffset(), 0.0f);
	Recorder->Reset();

	// A drag mostly across the axis does the same on the entry it began on, and does not scroll.
	Drag(1, FVector2D(20.0, 5.0));
	TestEqual(TEXT("A drag to the right on the second entry focuses it, then reads as Left"), Recorder->ToString(), FString(TEXT("Item 1, Navigation Left")));
	TestEqual(TEXT("A drag mostly across the axis does not scroll"), Scroll->GetScrollOffset(), 0.0f);
	Recorder->Reset();

	// A drag across the list that begins on a disabled entry begins on no interactable entry: it is not broadcast, and the
	// driven item, focus and the content stay as they are.
	List->GetHostedEntryWidget(0)->SetIsEnabled(false);
	Harness.Paint();
	Drag(0, FVector2D(-20.0, 0.0));
	TestEqual(TEXT("A drag across a disabled entry broadcasts nothing and moves no focus"), Recorder->ToString(), FString());
	TestEqual(TEXT("The driven item stays the second entry"), List->GetDrivenItemIndex(), 1);
	TestEqual(TEXT("and so does focus"), List->GetFocusedItemIndex(), 1);
	TestEqual(TEXT("It clicks nothing"), List->GetSelectedItemIndex(), 1);
	TestEqual(TEXT("and does not scroll"), Scroll->GetScrollOffset(), 0.0f);
	List->GetHostedEntryWidget(0)->SetIsEnabled(true);
	Harness.Paint();

	// A drag along the axis scrolls the content; it is broadcast too, and focus stays on the driven item, not the entry the
	// drag began on.
	Drag(0, FVector2D(0.0, -30.0));
	TestEqual(TEXT("A drag up on the first entry is broadcast as Down, and focus does not move"), Recorder->ToString(), FString(TEXT("Navigation Down")));
	TestEqual(TEXT("Focus stays on the second entry"), List->GetFocusedItemIndex(), 1);
	TestTrue(TEXT("The drag along the axis scrolls"), Scroll->GetScrollOffset() > 0.0f);
	TestEqual(TEXT("and clicks nothing"), List->GetSelectedItemIndex(), 1);
	Recorder->Reset();

	// A drag shorter than the Drag Trigger Distance is a click, with a handler bound too: it focuses and selects the entry,
	// and nothing is broadcast.
	Drag(2, FVector2D(-6.0, 0.0));
	TestEqual(TEXT("A 6 px sideways drag on the third entry focuses it and broadcasts nothing"), Recorder->ToString(), FString(TEXT("Item 2")));
	TestEqual(TEXT("It clicks the entry: it is selected"), List->GetSelectedItemIndex(), 2);
	return true;
}

#endif
