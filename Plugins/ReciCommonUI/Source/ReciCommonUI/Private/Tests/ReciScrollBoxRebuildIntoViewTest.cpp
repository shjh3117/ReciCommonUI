// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Misc/AutomationTest.h"
#include "Tests/ReciTestHarness.h"
#include "Tests/ReciTestWidgets.h"
#include "Widgets/ReciList.h"
#include "Widgets/ReciListLayoutBox.h"
#include "Widgets/ReciScrollBox.h"
#include "Widgets/ReciTextListItem.h"

// A Reci Scroll Box keeps the driven item of its list in view as the driven item moves, also when the screen rebuilds the
// list in between (Set List Items), as a song browser does on every filter change: the rebuild drops the request made for
// the old entries, and the new ones must still be scrolled to. The list is driven by keys the screen owns (Focus On
// Navigation off, no focus in the list) and by focus (the focused entry is the driven one), in both directions.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciScrollBoxRebuildIntoViewTest, "ReciCommonUI.ScrollBox.RebuildIntoView",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciScrollBoxRebuildIntoViewTest::RunTest(const FString& Parameters)
{
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	Harness.ActivateScreen(*Screen);
	// A 200 x 100 Reci Scroll Box over a Reci Vertical Box of six entries: the first two are in view, the rest scroll.
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
	const auto Rebuild = [List]()
	{
		TArray<UObject*> Items;
		for (int32 Index = 0; Index < 6; ++Index)
		{
			Items.Add(NewObject<UReciTextListItemData>(List));
		}
		List->SetListItems(Items);
	};
	Rebuild();
	Harness.Paint();
	if (!TestNotNull(TEXT("The first entry exists"), List->GetHostedEntryWidget(0)))
	{
		return false;
	}
	// Entries that are out of view are not painted and have no geometry: the entries are stacked, so the extent of the
	// first one, taken now, places each along the content.
	const float EntryHeight = List->GetHostedEntryWidget(0)->GetCachedGeometry().GetLocalSize().Y;
	const float ViewHeight = Scroll->GetCachedGeometry().GetLocalSize().Y;
	const float EndOffset = 6.0f * EntryHeight - ViewHeight;
	const auto IsInView = [Scroll, EntryHeight, ViewHeight](const int32 ItemIndex)
	{
		const float Offset = Scroll->GetScrollOffset();
		return ItemIndex * EntryHeight >= Offset - 0.5f && (ItemIndex + 1) * EntryHeight <= Offset + ViewHeight + 0.5f;
	};
	TestTrue(TEXT("The content is longer than the view"), EndOffset > 0.0f);
	TestEqual(TEXT("The list starts at the top"), Scroll->GetScrollOffset(), 0.0f);
	TestFalse(TEXT("and the last entry is out of view"), IsInView(5));

	// Driven by screen keys: no focus anywhere in the list. The screen's selection callback rebuilds the list on the next
	// tick, so the rebuild comes between the move and the timer that scrolls to the new item.
	List->SetFocusOnNavigation(false);
	TestTrue(TEXT("The last entry becomes the driven item"), List->SetDrivenItemIndex(5, false));
	Rebuild();
	Harness.Advance(1.0f);
	TestEqual(TEXT("The rebuilt list keeps the driven item"), List->GetDrivenItemIndex(), 5);
	TestEqual(TEXT("The scroll box scrolls to the end"), Scroll->GetScrollOffset(), EndOffset, 1.5f);
	TestTrue(TEXT("and the driven item is fully in view"), IsInView(5));

	// And back: the first entry becomes the driven item through a rebuild again.
	TestTrue(TEXT("The first entry becomes the driven item"), List->SetDrivenItemIndex(0, false));
	Rebuild();
	Harness.Advance(1.0f);
	TestEqual(TEXT("The scroll box scrolls back to the start"), Scroll->GetScrollOffset(), 0.0f, 0.5f);
	TestTrue(TEXT("and the first entry is in view"), IsInView(0));

	// One step at a time, as keys move it: the driven item stays in view after each step.
	for (int32 Index = 1; Index < 6; ++Index)
	{
		TestTrue(TEXT("The next entry becomes the driven item"), List->SetDrivenItemIndex(Index, false));
		Rebuild();
		Harness.Advance(0.5f);
		TestTrue(FString::Printf(TEXT("Entry %d is in view after the rebuild"), Index), IsInView(Index));
	}

	// Driven by focus: the focused entry is the driven one, and focus returns to it after the rebuild.
	List->SetFocusOnNavigation(true);
	TestTrue(TEXT("The first entry takes focus"), List->SetDrivenItemIndex(0, true));
	Harness.Advance(1.0f);
	TestEqual(TEXT("The scroll box is at the start"), Scroll->GetScrollOffset(), 0.0f, 0.5f);
	TestTrue(TEXT("The last entry takes focus"), List->SetDrivenItemIndex(5, true));
	Rebuild();
	Harness.Advance(1.0f);
	TestEqual(TEXT("Focus is back on the last entry"), List->GetFocusedItemIndex(), 5);
	TestTrue(TEXT("and it is in view after the rebuild"), IsInView(5));
	return true;
}

#endif
