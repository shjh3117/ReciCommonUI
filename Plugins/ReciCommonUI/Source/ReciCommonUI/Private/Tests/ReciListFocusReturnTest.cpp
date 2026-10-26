// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tests/ReciTestHarness.h"
#include "Tests/ReciTestWidgets.h"
#include "Widgets/ReciList.h"
#include "Widgets/ReciListLayoutBox.h"
#include "Widgets/ReciTextListItem.h"

// Focus that left a list comes back to its driven item, which may have moved while focus was away. A screen key drives a
// list whose Focus On Navigation is off, and focus that comes back lands on that item, not on the entry it left: when
// Slate's navigation moves focus to the list, when focus lands on another entry first (Slate's geometric search puts it on
// the nearest), and when it lands on the list's layout box, with no other entry focused on the way. With Focus On
// Navigation on, the driven item is the entry focus left. An item Set Focused Item Index requests and an entry a pointer
// focuses keep focus. Clear Selection On Focus Return clears the selection as focus comes back to the driven item; Reset
// Last Focused Item On Focus Return sends it to the first entry instead.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciListFocusReturnTest, "ReciCommonUI.List.FocusReturn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciListFocusReturnTest::RunTest(const FString& Parameters)
{
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	Harness.ActivateScreen(*Screen);
	UWidgetTree& Tree = *Screen->WidgetTree;
	UReciVerticalBox* Layout = Tree.ConstructWidget<UReciVerticalBox>(UReciVerticalBox::StaticClass(), TEXT("Layout"));
	UReciList* List = Tree.ConstructWidget<UReciList>(UReciList::StaticClass(), TEXT("Entries"));
	List->ItemWidgetClass = UReciTestListEntry::StaticClass();
	Layout->SetContent(List);
	Screen->GetContent()->AddChild(Layout);
	// Down from the widget beside the list moves focus to the list. Slate does not hit-test the harness window, so an
	// explicit Navigation rule stands in for the geometric search.
	UReciTestFocusTarget* Beside = Harness.CreateTarget(TEXT("Beside"), Screen);
	Beside->SetNavigationRuleExplicit(EUINavigation::Down, List);
	TArray<UObject*> Items;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Items.Add(NewObject<UReciTextListItemData>(List));
	}
	List->SetListItems(Items);
	Harness.Paint();

	APlayerController* Player = Harness.GetPlayerController();
	const auto EntryHasFocus = [List, Player](const int32 Index)
	{
		const UWidget* Button = List->GetHostedEntryButton(Index);
		return List->GetFocusedItemIndex() == Index && Button && (Button->HasUserFocus(Player) || Button->HasUserFocusedDescendants(Player));
	};
	const auto PressDown = [&Harness]()
	{
		Harness.SlateKeyDown(EKeys::Down);
		Harness.SlateKeyUp(EKeys::Down);
	};
	// The entries the harness user's focus moves to in Slate, and the ones the list reports focus on.
	TArray<int32> SlateFocusedEntries;
	TArray<int32> ReportedEntries;
	const auto EntryIndexOf = [List](const TSharedPtr<SWidget>& Widget)
	{
		for (const SWidget* Slate = Widget.Get(); Slate; Slate = Slate->GetParentWidget().Get())
		{
			for (int32 Index = 0; Index < List->GetItemCount(); ++Index)
			{
				const UWidget* Button = List->GetHostedEntryButton(Index);
				if (Button && Button->GetCachedWidget().Get() == Slate)
				{
					return Index;
				}
			}
		}
		return static_cast<int32>(INDEX_NONE);
	};
	const FDelegateHandle SlateHandle = FSlateApplication::Get().OnFocusChanging().AddLambda(
		[&SlateFocusedEntries, &EntryIndexOf](const FFocusEvent& Event, const FWeakWidgetPath&, const TSharedPtr<SWidget>&,
			const FWidgetPath&, const TSharedPtr<SWidget>& NewFocused)
		{
			const int32 Index = Event.GetUser() == static_cast<uint32>(FReciTestHarness::UserIndex) ? EntryIndexOf(NewFocused) : INDEX_NONE;
			if (Index != INDEX_NONE)
			{
				SlateFocusedEntries.Add(Index);
			}
		});
	const FDelegateHandle ListHandle = List->OnFocusedIndexChangedNative().AddLambda([&ReportedEntries](const int32 Index)
	{
		if (Index != INDEX_NONE)
		{
			ReportedEntries.Add(Index);
		}
	});
	ON_SCOPE_EXIT
	{
		FSlateApplication::Get().OnFocusChanging().Remove(SlateHandle);
		List->OnFocusedIndexChangedNative().Remove(ListHandle);
	};
	// Focus moved to the entry at Index alone, in Slate and as the list reports it.
	const auto OnlyEntryFocused = [&SlateFocusedEntries, &ReportedEntries](const int32 Index)
	{
		const bool bOnly = !SlateFocusedEntries.IsEmpty() && !SlateFocusedEntries.ContainsByPredicate([Index](const int32 Other) { return Other != Index; })
			&& ReportedEntries.Num() == 1 && ReportedEntries[0] == Index;
		SlateFocusedEntries.Reset();
		ReportedEntries.Reset();
		return bOnly;
	};

	// A screen drives the list: focus leaves its fourth entry, and a screen key moves the driven item to the second.
	List->SetFocusOnNavigation(false);
	TestTrue(TEXT("The fourth entry takes focus as the driven item"), List->SetDrivenItemIndex(3, true) && EntryHasFocus(3));
	TestTrue(TEXT("Focus moves beside the list"), Harness.Focus(*Beside));
	TestTrue(TEXT("A screen key moves the driven item to the second"), List->SetDrivenItemIndex(1, false));
	SlateFocusedEntries.Reset();
	ReportedEntries.Reset();
	PressDown();
	TestTrue(TEXT("Down returns focus to the driven second entry"), EntryHasFocus(1));
	TestTrue(TEXT("and to no other entry on the way"), OnlyEntryFocused(1));
	TestEqual(TEXT("The second item stays the driven item"), List->GetDrivenItemIndex(), 1);

	// Focus lands on the first entry, as Slate's geometric search can put it, and moves on to the driven third.
	Harness.Focus(*Beside);
	List->SetDrivenItemIndex(2, false);
	SlateFocusedEntries.Reset();
	ReportedEntries.Reset();
	Harness.Focus(*List->GetHostedEntryButton(0));
	TestTrue(TEXT("Focus landing on the first entry moves on to the driven third"), EntryHasFocus(2));
	TestTrue(TEXT("and the list reports focus on the third alone"), ReportedEntries.Num() == 1 && ReportedEntries[0] == 2);
	SlateFocusedEntries.Reset();
	ReportedEntries.Reset();

	// Focus lands on the list's layout box, which is focusable, and goes straight to the driven first entry.
	Harness.Focus(*Beside);
	List->SetDrivenItemIndex(0, false);
	SlateFocusedEntries.Reset();
	ReportedEntries.Reset();
	Harness.Focus(*Layout);
	TestTrue(TEXT("Focus landing on the layout box goes to the driven first entry"), EntryHasFocus(0));
	TestTrue(TEXT("and to no other entry on the way"), OnlyEntryFocused(0));
	List->SetFocusOnNavigation(true);

	// With Focus On Navigation on, the driven item follows focus, so focus comes back to the entry it left.
	Harness.Focus(*List->GetHostedEntryButton(2));
	TestEqual(TEXT("Focus on the third entry makes it the driven item"), List->GetDrivenItemIndex(), 2);
	Harness.Focus(*Beside);
	PressDown();
	TestTrue(TEXT("Down returns focus to the third entry"), EntryHasFocus(2));

	// Set Focused Item Index while focus is away: the requested item keeps focus.
	Harness.Focus(*Beside);
	TestTrue(TEXT("Set Focused Item Index moves focus to the fourth entry"), List->SetFocusedItemIndex(3));
	TestTrue(TEXT("The requested fourth entry keeps focus"), EntryHasFocus(3));

	// A pointer focuses the second entry while focus is away: it keeps focus.
	Harness.Focus(*Beside);
	FSlateApplication::Get().SetUserFocus(FReciTestHarness::UserIndex, List->GetHostedEntryButton(1)->TakeWidget(), EFocusCause::Mouse);
	TestTrue(TEXT("The entry a pointer focuses keeps focus"), EntryHasFocus(1));

	// Clear Selection On Focus Return: focus comes back to the driven item and the selection is cleared.
	List->bClearSelectionOnFocusReturn = true;
	TestTrue(TEXT("The third item is selected"), List->SetSelectedItemIndex(2));
	Harness.Focus(*Beside);
	PressDown();
	TestTrue(TEXT("With Clear Selection On Focus Return, Down returns focus to the driven second entry"), EntryHasFocus(1));
	TestEqual(TEXT("and clears the selection"), List->GetNumItemsSelected(), 0);
	List->bClearSelectionOnFocusReturn = false;

	// Reset Last Focused Item On Focus Return: focus comes back to the first entry instead of the driven item.
	List->bResetLastFocusedItemOnFocusReturn = true;
	Harness.Focus(*Beside);
	PressDown();
	TestTrue(TEXT("With Reset Last Focused Item On Focus Return, Down returns focus to the first entry"), EntryHasFocus(0));
	return true;
}

#endif
