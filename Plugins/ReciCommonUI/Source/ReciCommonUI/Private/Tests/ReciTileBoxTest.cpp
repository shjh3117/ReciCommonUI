// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Misc/AutomationTest.h"
#include "Tests/ReciInteractionTestEntry.h"
#include "Tests/ReciTestHarness.h"
#include "Widgets/ReciList.h"
#include "Widgets/ReciListLayoutBox.h"
#include "Widgets/ReciTextListItem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciTileNavigationTest, "ReciCommonUI.Layout.TileNavigation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciTileNavigationTest::RunTest(const FString& Parameters)
{
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	// Seven 60 x 40 entries in a Reci Tile Box of three columns, inside a 200 x 200 frame:
	//   0 1 2
	//   3 4 5
	//   6
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	Harness.ActivateScreen(*Screen);
	UWidgetTree& Tree = *Screen->WidgetTree;
	USizeBox* Frame = Tree.ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Frame"));
	Frame->SetWidthOverride(200.0f);
	Frame->SetHeightOverride(200.0f);
	UReciTileBox* Tiles = Tree.ConstructWidget<UReciTileBox>(UReciTileBox::StaticClass(), TEXT("Tiles"));
	Tiles->SetEntrySize(FVector2D(60.0, 40.0));
	Tiles->SetFixedColumns(3);
	UReciList* List = Tree.ConstructWidget<UReciList>(UReciList::StaticClass(), TEXT("Grid"));
	List->ItemWidgetClass = UReciInteractionTestEntry::StaticClass();
	Tiles->SetContent(List);
	Frame->SetContent(Tiles);
	UOverlaySlot* FrameSlot = Screen->GetContent()->AddChildToOverlay(Frame);
	FrameSlot->SetHorizontalAlignment(HAlign_Left);
	FrameSlot->SetVerticalAlignment(VAlign_Top);
	TArray<UObject*> Items;
	for (int32 Index = 0; Index < 7; ++Index)
	{
		Items.Add(NewObject<UReciTextListItemData>(List));
	}
	List->SetListItems(Items);
	Harness.Paint();
	TArray<UWidget*> Entries;
	for (int32 Index = 0; Index < 7; ++Index)
	{
		Entries.Add(List->GetHostedEntryWidget(Index));
	}
	const auto TestNear = [this](const TCHAR* What, const FVector2D& Actual, const FVector2D& Expected)
	{
		TestTrue(FString::Printf(TEXT("%s (%s)"), What, *Actual.ToString()), Actual.Equals(Expected, 0.5));
	};
	const auto Centre = [&Entries, Frame](const int32 Index)
	{
		const FGeometry& Entry = Entries[Index]->GetCachedGeometry();
		return Frame->GetCachedGeometry().AbsoluteToLocal(Entry.LocalToAbsolute(Entry.GetLocalSize() * 0.5));
	};
	TestNear(TEXT("Entries flow along rows of three"), Centre(4), FVector2D(90.0, 60.0));
	TestNear(TEXT("The seventh starts the third row"), Centre(6), FVector2D(30.0, 100.0));

	// Each direction goes to the nearest entry that way, by entry centres.
	const auto Press = [&Harness](const FKey& Key)
	{
		const bool bHandled = Harness.KeyDown(Key);
		Harness.KeyUp(Key);
		Harness.Paint();
		return bHandled;
	};
	TestTrue(TEXT("The first entry takes focus"), List->SetDrivenItemIndex(0, true));
	Harness.Paint();
	TestTrue(TEXT("Right is handled"), Press(EKeys::Right));
	TestEqual(TEXT("Right goes to the next column"), List->GetDrivenItemIndex(), 1);
	TestTrue(TEXT("Down is handled"), Press(EKeys::Down));
	TestEqual(TEXT("Down goes to the next row, same column"), List->GetDrivenItemIndex(), 4);
	TestTrue(TEXT("Left is handled"), Press(EKeys::Left));
	TestEqual(TEXT("Left goes to the previous column"), List->GetDrivenItemIndex(), 3);
	TestTrue(TEXT("Down is handled again"), Press(EKeys::Down));
	TestEqual(TEXT("Down from the second row reaches the short last row"), List->GetDrivenItemIndex(), 6);
	TestTrue(TEXT("Up is handled"), Press(EKeys::Up));
	TestEqual(TEXT("Up goes back"), List->GetDrivenItemIndex(), 3);
	TestTrue(TEXT("Focus follows the driven item"), Entries[3]->HasAnyUserFocus() || Entries[3]->HasFocusedDescendants());

	// At an edge, Wrap Navigation off leaves the key unhandled so focus can leave the grid; on, it wraps.
	List->SetDrivenItemIndex(2, true);
	Harness.Paint();
	TestFalse(TEXT("Right at the right edge is not handled with Wrap Navigation off"), Press(EKeys::Right));
	TestEqual(TEXT("The driven item stays"), List->GetDrivenItemIndex(), 2);
	Tiles->bWrapNavigation = true;
	TestTrue(TEXT("Right at the right edge is handled with Wrap Navigation on"), Press(EKeys::Right));
	TestEqual(TEXT("It wraps to the start of the row"), List->GetDrivenItemIndex(), 0);
	TestTrue(TEXT("Up at the top edge wraps"), Press(EKeys::Up));
	TestEqual(TEXT("It wraps to the bottom of the column"), List->GetDrivenItemIndex(), 6);

	// The layout changes arrange the same entries again.
	Tiles->SetFixedColumns(0);
	Frame->SetWidthOverride(130.0f);
	Harness.Paint();
	TestNear(TEXT("Fixed Columns 0 fits as many columns as the width holds: two in 130"), Centre(2), FVector2D(30.0, 60.0));
	Tiles->SetFlowOrientation(Orient_Vertical);
	Harness.Paint();
	TestNear(TEXT("A vertical flow fills columns first: the second entry is below the first"), Centre(1), FVector2D(30.0, 60.0));
	TestNear(TEXT("The fifth starts the second column"), Centre(4), FVector2D(90.0, 20.0));
	Tiles->SetEntrySize(FVector2D(50.0, 30.0));
	Harness.Paint();
	TestNear(TEXT("Entry Size sets every cell"), Entries[0]->GetCachedGeometry().GetLocalSize(), FVector2D(50.0, 30.0));
	bool bSameEntries = List->GetItemCount() == Entries.Num();
	for (int32 Index = 0; bSameEntries && Index < Entries.Num(); ++Index)
	{
		bSameEntries = List->GetHostedEntryWidget(Index) == Entries[Index];
	}
	TestTrue(TEXT("Layout changes keep every entry widget"), bSameEntries);
	return true;
}

#endif
