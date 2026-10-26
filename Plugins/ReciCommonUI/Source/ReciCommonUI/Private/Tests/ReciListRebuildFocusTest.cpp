// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Tests/ReciTestHarness.h"
#include "Tests/ReciTestWidgets.h"
#include "Widgets/ReciList.h"
#include "Widgets/ReciListLayoutBox.h"
#include "Widgets/ReciTextListItem.h"

// Set List Items replaces every entry, and so do Add, Insert and Remove Item through it. Focus the owning player had on
// an entry lands on the restored driven item's new entry, with the same items or new ones. Focus anywhere else stays
// where it is and the list takes none: on a widget beside the list, on one around it (as a row holds a child list), on
// the screen, while a screen drives the list (Focus On Navigation off), and while another user's focus is in the list.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciListRebuildFocusTest, "ReciCommonUI.List.RebuildFocus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciListRebuildFocusTest::RunTest(const FString& Parameters)
{
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	Harness.ActivateScreen(*Screen);
	UWidgetTree& Tree = *Screen->WidgetTree;
	// A list of four entries, a widget beside it, and a focusable widget around a second list.
	UReciVerticalBox* Layout = Tree.ConstructWidget<UReciVerticalBox>(UReciVerticalBox::StaticClass(), TEXT("Layout"));
	UReciList* List = Tree.ConstructWidget<UReciList>(UReciList::StaticClass(), TEXT("Entries"));
	List->ItemWidgetClass = UReciTestListEntry::StaticClass();
	Layout->SetContent(List);
	Screen->GetContent()->AddChild(Layout);
	UReciTestFocusTarget* Beside = Harness.CreateTarget(TEXT("Beside"), Screen);
	UReciTestFocusTarget* Around = Harness.CreateTarget(TEXT("Around"), Screen);
	UReciVerticalBox* InnerLayout = Tree.ConstructWidget<UReciVerticalBox>(UReciVerticalBox::StaticClass(), TEXT("InnerLayout"));
	UReciList* Inner = Tree.ConstructWidget<UReciList>(UReciList::StaticClass(), TEXT("Inner"));
	Inner->ItemWidgetClass = UReciTestListEntry::StaticClass();
	InnerLayout->SetContent(Inner);
	Around->SetContent(InnerLayout);
	const auto MakeItems = [List]()
	{
		TArray<UObject*> Items;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			Items.Add(NewObject<UReciTextListItemData>(List));
		}
		return Items;
	};
	const TArray<UObject*> Items = MakeItems();
	List->SetListItems(Items);
	Inner->SetListItems(MakeItems());
	Harness.Paint();

	APlayerController* Player = Harness.GetPlayerController();
	const auto HasFocus = [Player](const UWidget* Widget)
	{
		return Widget && (Widget->HasUserFocus(Player) || Widget->HasUserFocusedDescendants(Player));
	};
	// The entry at Index has the harness user's focus, and the list knows it.
	const auto EntryHasFocus = [List, &HasFocus](const int32 Index)
	{
		return List->GetFocusedItemIndex() == Index && HasFocus(List->GetHostedEntryButton(Index));
	};

	// Focus on an entry lands on the same item's new entry.
	TestTrue(TEXT("The third entry takes focus"), List->SetFocusedItemIndex(2));
	const UWidget* Previous = List->GetHostedEntryButton(2);
	List->SetListItems(Items);
	TestTrue(TEXT("Set List Items makes new entries"), List->GetHostedEntryButton(2) != Previous);
	TestEqual(TEXT("The driven item stays the third"), List->GetDrivenItemIndex(), 2);
	TestTrue(TEXT("Focus lands on its new entry"), EntryHasFocus(2));

	// New item objects: the driven item keeps its index, and focus lands there.
	List->SetListItems(MakeItems());
	TestTrue(TEXT("With new items, focus lands on the entry at the driven index"), EntryHasFocus(2));

	// Through Set List Items: Insert Item before the driven item moves it, and focus, down one; Remove Item of the driven
	// item leaves the driven index, and focus, on the item that takes its place.
	UObject* Driven = List->GetItemAt(2);
	TestTrue(TEXT("Insert Item inserts before the driven item"), List->InsertItem(NewObject<UReciTextListItemData>(List), 0));
	TestTrue(TEXT("Focus follows the driven item to its new index"), List->GetItemAt(3) == Driven && EntryHasFocus(3));
	TestTrue(TEXT("Remove Item removes the driven item"), List->RemoveItem(Driven));
	TestTrue(TEXT("Focus stays at its index, on the item after it"), List->GetDrivenItemIndex() == 3 && EntryHasFocus(3));
	List->SetListItems(Items);

	// Focus beside the list stays there, and the list takes none.
	TestTrue(TEXT("Focus moves beside the list"), Harness.Focus(*Beside));
	List->SetListItems(Items);
	TestTrue(TEXT("A rebuild leaves focus beside the list"), HasFocus(Beside));
	TestEqual(TEXT("and no entry has focus"), List->GetFocusedItemIndex(), INDEX_NONE);

	// Another user's focus in the list is not the owning player's: the player's focus stays beside the list.
	{
		const TSharedRef<FSlateVirtualUserHandle> Other = FSlateApplication::Get().FindOrCreateVirtualUser(FReciTestHarness::UserIndex);
		FSlateApplication::Get().SetUserFocus(Other->GetUserIndex(), List->GetHostedEntryButton(1)->TakeWidget());
		TestTrue(TEXT("Another user's focus is in the list"), List->HasFocusedDescendants());
		List->SetListItems(Items);
		TestTrue(TEXT("Its rebuild moves none of the player's focus"), HasFocus(Beside));
	}

	// A screen-driven selector, whose driven item moves while focus is elsewhere, takes none either.
	List->SetFocusOnNavigation(false);
	List->SetDrivenItemIndex(1, false);
	List->SetListItems(Items);
	TestTrue(TEXT("A screen-driven selector's rebuild leaves focus beside it"), HasFocus(Beside));
	TestEqual(TEXT("and keeps its driven item"), List->GetDrivenItemIndex(), 1);
	List->SetFocusOnNavigation(true);

	// Focus on a widget around a list, as a row holds its child list, stays on that widget.
	TestTrue(TEXT("Focus moves to the widget around the inner list"), Harness.Focus(*Around));
	Inner->SetListItems(MakeItems());
	TestTrue(TEXT("The inner list's rebuild leaves focus on the widget around it"), Around->HasUserFocus(Player));
	TestEqual(TEXT("and none of its entries has focus"), Inner->GetFocusedItemIndex(), INDEX_NONE);

	// Focus on the screen, around both lists, stays on the screen.
	TestTrue(TEXT("Focus moves to the screen"), Harness.Focus(*Screen));
	List->SetListItems(Items);
	TestTrue(TEXT("A rebuild leaves focus on the screen around the list"), Screen->HasUserFocus(Player));
	TestEqual(TEXT("and no entry has focus"), List->GetFocusedItemIndex(), INDEX_NONE);
	return true;
}

namespace
{
	/** A list of entries in a Reci Vertical Box in the screen, as a page holds one. */
	UReciList* CreateRebuildFocusList(UReciTestScreen& Screen, const TSubclassOf<UUserWidget> EntryClass)
	{
		UWidgetTree& Tree = *Screen.WidgetTree;
		UReciVerticalBox* Layout = Tree.ConstructWidget<UReciVerticalBox>(UReciVerticalBox::StaticClass(), TEXT("Layout"));
		UReciList* List = Tree.ConstructWidget<UReciList>(UReciList::StaticClass(), TEXT("Entries"));
		List->ItemWidgetClass = EntryClass;
		Layout->SetContent(List);
		Screen.GetContent()->AddChild(Layout);
		return List;
	}
}

// Focus that left the list for a widget beside it comes back after Set List Items to the driven item: the same item's new
// entry, the entry at the restored driven index when the items are new or that item is gone, and the item a screen key
// moved the driven item to while focus was away. When the driven item's entry cannot take focus (hidden, disabled, or
// a disabled item takes the removed driven item's index), focus goes where entering the list puts it, the first entry
// that can take focus. It does so when Slate's navigation moves focus to the list, and when focus lands on another entry
// first, as Slate's geometric search puts it on the nearest. As on the demo's Lists page: keyboard focus on Item 4, a
// click on Reset beside the list rebuilds it, and Down returns to Item 4.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciListRebuildFocusReturnTest, "ReciCommonUI.List.RebuildFocusReturn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciListRebuildFocusReturnTest::RunTest(const FString& Parameters)
{
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	Harness.ActivateScreen(*Screen);
	UReciList* List = CreateRebuildFocusList(*Screen, UReciTestStateEntry::StaticClass());
	// Down from the widget beside the list moves focus to the list. Slate does not hit-test the harness window, so an
	// explicit Navigation rule stands in for the geometric search.
	UReciTestFocusTarget* Beside = Harness.CreateTarget(TEXT("Beside"), Screen);
	Beside->SetNavigationRuleExplicit(EUINavigation::Down, List);
	const auto MakeItems = [List]()
	{
		TArray<UObject*> Items;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			Items.Add(NewObject<UReciTestStateItem>(List));
		}
		return Items;
	};
	const TArray<UObject*> Items = MakeItems();
	// The state of an item in Items, which its entry takes when the list makes it.
	const auto State = [&Items](const int32 Index)
	{
		return CastChecked<UReciTestStateItem>(Items[Index]);
	};
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

	// Focus leaves the fourth entry, and Set List Items rebuilds the list with the same items.
	TestTrue(TEXT("The fourth entry takes focus"), Harness.Focus(*List->GetHostedEntryButton(3)) && EntryHasFocus(3));
	TestTrue(TEXT("Focus moves beside the list"), Harness.Focus(*Beside));
	List->SetListItems(Items);
	TestEqual(TEXT("The rebuild keeps the fourth item driven"), List->GetDrivenItemIndex(), 3);
	TestTrue(TEXT("and leaves focus beside the list"), Beside->HasUserFocus(Player));
	PressDown();
	TestTrue(TEXT("Down returns focus to the fourth item's new entry"), EntryHasFocus(3));

	// New items: focus returns to the entry at the restored driven index.
	Harness.Focus(*Beside);
	List->SetListItems(MakeItems());
	PressDown();
	TestTrue(TEXT("With new items, Down returns focus to the entry at the driven index"), EntryHasFocus(3));

	// Focus that lands on the first entry moves on to the entry it left.
	Harness.Focus(*Beside);
	List->SetListItems(Items);
	Harness.Focus(*List->GetHostedEntryButton(0));
	TestTrue(TEXT("Focus landing on the first entry moves on to the fourth"), EntryHasFocus(3));

	// Remove Item of the item focus left: focus returns to the item at the restored driven index.
	Harness.Focus(*Beside);
	TestTrue(TEXT("Remove Item removes the fourth item"), List->RemoveItemAt(3));
	TestEqual(TEXT("The driven index stays in the list, on the third item"), List->GetDrivenItemIndex(), 2);
	PressDown();
	TestTrue(TEXT("Down returns focus to the third entry"), EntryHasFocus(2));

	// A screen key moves the driven item while focus is away, as it moves a key cursor: focus returns to that item, not to
	// the entry it left.
	Harness.Focus(*Beside);
	List->SetListItems(Items);
	TestTrue(TEXT("A screen key moves the driven item to the second"), List->SetDrivenItemIndex(1, false));
	List->SetListItems(Items);
	PressDown();
	TestTrue(TEXT("Down returns focus to the second entry, the driven item"), EntryHasFocus(1));

	// The driven item's entry cannot take focus once the list is rebuilt, and the first entry is disabled: focus goes to the
	// first entry that can take it, the second.
	State(0)->bDisabled = true;
	TestTrue(TEXT("The fourth entry takes focus"), Harness.Focus(*List->GetHostedEntryButton(3)) && EntryHasFocus(3));
	Harness.Focus(*Beside);
	State(3)->bHidden = true;
	List->SetListItems(Items);
	PressDown();
	TestTrue(TEXT("With the driven item's entry hidden, Down returns focus to the second entry"), EntryHasFocus(1));
	State(3)->bHidden = false;
	List->SetListItems(Items);
	TestTrue(TEXT("The fourth entry takes focus again"), Harness.Focus(*List->GetHostedEntryButton(3)) && EntryHasFocus(3));
	Harness.Focus(*Beside);
	State(3)->bDisabled = true;
	List->SetListItems(Items);
	PressDown();
	TestTrue(TEXT("With the driven item's entry disabled, Down returns focus to the second entry"), EntryHasFocus(1));

	// Remove Item of the driven item, whose index the disabled fourth item takes.
	TestTrue(TEXT("The third entry takes focus"), Harness.Focus(*List->GetHostedEntryButton(2)) && EntryHasFocus(2));
	Harness.Focus(*Beside);
	TestTrue(TEXT("Remove Item removes the third item"), List->RemoveItemAt(2));
	TestTrue(TEXT("The disabled fourth item takes the driven index"),
		List->GetDrivenItemIndex() == 2 && List->GetItemAt(2) == Items[3]);
	PressDown();
	TestTrue(TEXT("Down returns focus to the second entry"), EntryHasFocus(1));
	return true;
}

// Focus on an entry goes after Set List Items to the driven item's new entry, and when that entry cannot take focus, where
// entering the list puts it: the first entry that can take focus, past a disabled first entry, when the driven item's
// entry is disabled or hidden, also from focus on the list itself; the list itself when no entry is left and the list is
// focusable. A list that can take no focus leaves focus where it was, which Slate would move to a widget around the list
// that can.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciListRebuildFocusFallbackTest, "ReciCommonUI.List.RebuildFocusFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciListRebuildFocusFallbackTest::RunTest(const FString& Parameters)
{
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	Harness.ActivateScreen(*Screen);
	UReciList* List = CreateRebuildFocusList(*Screen, UReciTestStateEntry::StaticClass());
	TArray<UReciTestStateItem*> States;
	TArray<UObject*> Items;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		States.Add(NewObject<UReciTestStateItem>(List));
		Items.Add(States.Last());
	}
	States[0]->bDisabled = true;
	List->SetListItems(Items);
	Harness.Paint();

	APlayerController* Player = Harness.GetPlayerController();
	const auto HasFocus = [Player](const UWidget* Widget)
	{
		return Widget && (Widget->HasUserFocus(Player) || Widget->HasUserFocusedDescendants(Player));
	};
	const auto EntryHasFocus = [List, &HasFocus](const int32 Index)
	{
		return List->GetFocusedItemIndex() == Index && HasFocus(List->GetHostedEntryButton(Index));
	};

	// The driven third item's entry is disabled.
	TestTrue(TEXT("The third entry takes focus"), List->SetFocusedItemIndex(2) && EntryHasFocus(2));
	States[2]->bDisabled = true;
	List->SetListItems(Items);
	TestTrue(TEXT("With the driven item's entry disabled, focus goes to the first enabled entry, the second"), EntryHasFocus(1));

	// The driven second item's entry is hidden.
	States[2]->bDisabled = false;
	States[1]->bHidden = true;
	List->SetListItems(Items);
	TestTrue(TEXT("With the driven item's entry hidden, focus goes to the first entry that can take it, the third"), EntryHasFocus(2));

	// No entry left: a focusable list takes focus itself; refilled, its driven first entry is disabled.
	List->SetIsFocusable(true);
	List->SetListItems(TArray<UObject*>());
	TestTrue(TEXT("With no entry left, the focusable list takes focus"), List->HasUserFocus(Player));
	List->SetListItems(Items);
	TestTrue(TEXT("Refilled, its driven first item's entry is disabled, so focus goes to the first entry that can take it"),
		EntryHasFocus(2));

	// No entry left in a list that can take no focus: focus stays on the widget it was on.
	const TSharedPtr<SWidget> Focused = Harness.GetFocusedWidget();
	List->SetIsFocusable(false);
	List->SetListItems(TArray<UObject*>());
	TestTrue(TEXT("With no entry left in a list that cannot take focus, focus stays where it was"),
		Focused.IsValid() && Harness.GetFocusedWidget() == Focused);
	return true;
}

#endif
