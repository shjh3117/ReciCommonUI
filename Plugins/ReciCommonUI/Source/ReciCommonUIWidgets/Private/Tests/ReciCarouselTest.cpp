// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Settings/ReciCommonUISettings.h"
#include "Tests/ReciTestHarness.h"
#include "Tests/ReciTestWidgets.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/ReciCarouselScrollBox.h"
#include "Widgets/ReciList.h"
#include "Widgets/ReciListLayoutBox.h"
#include "Widgets/ReciTextListItem.h"

namespace
{
	/** A 200 x 40 carousel in the screen's content: Reci Carousel Scroll Box > Reci Horizontal Box > Reci List of 60 x 40 entries. */
	struct FReciTestCarousel
	{
		UReciCarouselScrollBox* Carousel = nullptr;
		UReciList* List = nullptr;

		FReciTestCarousel(UReciTestScreen& Screen, const bool bLoopingEntries, const int32 ItemCount)
		{
			UWidgetTree& Tree = *Screen.WidgetTree;
			USizeBox* Frame = Tree.ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Frame"));
			Frame->SetWidthOverride(200.0f);
			Frame->SetHeightOverride(40.0f);
			Carousel = Tree.ConstructWidget<UReciCarouselScrollBox>(UReciCarouselScrollBox::StaticClass(), TEXT("Carousel"));
			Carousel->SetOrientation(Orient_Horizontal);
			UReciHorizontalBox* Layout = Tree.ConstructWidget<UReciHorizontalBox>(UReciHorizontalBox::StaticClass(), TEXT("Layout"));
			List = Tree.ConstructWidget<UReciList>(UReciList::StaticClass(), TEXT("Cards"));
			List->ItemWidgetClass = UReciTestListEntry::StaticClass();
			List->bLoopingEntries = bLoopingEntries;
			List->bSelectionFollowsNavigation = true;
			Layout->SetContent(List);
			Carousel->AddChild(Layout);
			Frame->SetContent(Carousel);
			UOverlaySlot* FrameSlot = Screen.GetContent()->AddChildToOverlay(Frame);
			FrameSlot->SetHorizontalAlignment(HAlign_Left);
			FrameSlot->SetVerticalAlignment(VAlign_Top);
			TArray<UObject*> Items;
			for (int32 Index = 0; Index < ItemCount; ++Index)
			{
				Items.Add(NewObject<UReciTextListItemData>(List, *FString::Printf(TEXT("Item%d"), Index)));
			}
			List->SetListItems(Items);
		}

		/** How far an entry's centre is from the viewport's, along the carousel. */
		float GetCentreOffset(const FGeometry& Entry) const
		{
			const FGeometry& View = Carousel->GetCachedGeometry();
			const FVector2D Centre = View.AbsoluteToLocal(Entry.LocalToAbsolute(Entry.GetLocalSize() * 0.5));
			return static_cast<float>(Centre.X - View.GetLocalSize().X * 0.5);
		}

		float GetItemCentreOffset(const int32 ItemIndex) const
		{
			return GetCentreOffset(List->GetHostedEntryWidget(ItemIndex)->GetCachedGeometry());
		}

		/** A pointer press and release at the entry's centre, as the input coordinator routes them to the carousel. */
		void Click(UWidget& Entry) const
		{
			const FGeometry& Geometry = Entry.GetCachedGeometry();
			const FVector2D At = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5);
			const TSet<FKey> Pressed = {EKeys::LeftMouseButton};
			const TSet<FKey> Released;
			const FPointerEvent Down(FReciTestHarness::UserIndex, 0, At, At, Pressed, EKeys::LeftMouseButton, 0.0f, FModifierKeysState());
			const FPointerEvent Up(FReciTestHarness::UserIndex, 0, At, At, Released, EKeys::LeftMouseButton, 0.0f, FModifierKeysState());
			IReciInputFocusTarget* Target = Carousel;
			Target->HandleReciPointerDown(Carousel->GetCachedGeometry(), Down);
			Target->HandleReciPointerUp(Up);
		}

		/**
		 * A pointer press at the entry's centre, moved by Delta in four steps and released there, as the input coordinator
		 * routes them to the carousel. Returns whether the carousel held the pointer after every move and took the release.
		 */
		bool Drag(UWidget& Entry, const FVector2D& Delta) const
		{
			const FGeometry& Geometry = Entry.GetCachedGeometry();
			const FVector2D Start = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5);
			const TSet<FKey> Pressed = {EKeys::LeftMouseButton};
			const TSet<FKey> Released;
			IReciInputFocusTarget* Target = Carousel;
			Target->HandleReciPointerDown(Carousel->GetCachedGeometry(),
				FPointerEvent(FReciTestHarness::UserIndex, 0, Start, Start, Pressed, EKeys::LeftMouseButton, 0.0f, FModifierKeysState()));
			bool bHeld = true;
			FVector2D Last = Start;
			for (int32 Step = 1; Step <= 4; ++Step)
			{
				const FVector2D Point = Start + Delta * (Step / 4.0);
				Target->HandleReciPointerMove(Carousel->GetCachedGeometry(),
					FPointerEvent(FReciTestHarness::UserIndex, 0, Point, Last, Pressed, FKey(), 0.0f, FModifierKeysState()));
				bHeld = bHeld && Target->HasReciPointerCapture(0);
				Last = Point;
			}
			const bool bReleaseTaken = Target->HandleReciPointerUp(
				FPointerEvent(FReciTestHarness::UserIndex, 0, Last, Last, Released, EKeys::LeftMouseButton, 0.0f, FModifierKeysState()));
			return bHeld && bReleaseTaken;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciCarouselCentringTest, "ReciCommonUI.Carousel.Centring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciCarouselCentringTest::RunTest(const FString& Parameters)
{
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	UReciCommonUISettings* Settings = GetMutableDefault<UReciCommonUISettings>();
	Harness.Preserve(*Settings);
	Settings->DefaultMotionScale = 1.0f;
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	Harness.ActivateScreen(*Screen);
	const FReciTestCarousel Rig(*Screen, false, 5);
	UReciCarouselScrollBox* Carousel = Rig.Carousel;
	UReciList* List = Rig.List;
	const TStrongObjectPtr<UReciTestEventRecorder> Centred(NewObject<UReciTestEventRecorder>());
	const TStrongObjectPtr<UReciTestEventRecorder> Confirmed(NewObject<UReciTestEventRecorder>());
	Carousel->OnCenteredItemChanged.AddDynamic(Centred.Get(), &UReciTestEventRecorder::RecordItem);
	List->OnItemConfirmed.AddDynamic(Confirmed.Get(), &UReciTestEventRecorder::RecordItem);
	Harness.Advance(0.5f);

	// The driven item is centred from the start: the first, since the box pads its ends so that it can be.
	TestEqual(TEXT("The first item is centred at the start"), Carousel->GetCenteredItemIndex(), 0);
	TestEqual(TEXT("Its centre is the viewport's"), Rig.GetItemCentreOffset(0), 0.0f, 0.5f);
	TestEqual(TEXT("The start is reported as a centred item change"), Centred->ToString(), FString(TEXT("Item 0")));
	Centred->Reset();

	// Set Centered Item Index without animation jumps to the item.
	TestTrue(TEXT("Set Centered Item Index accepts a hosted item"), Carousel->SetCenteredItemIndex(3, false));
	Harness.Advance(0.1f);
	TestEqual(TEXT("Item 3 is centred"), Carousel->GetCenteredItemIndex(), 3);
	TestEqual(TEXT("Its centre is the viewport's"), Rig.GetItemCentreOffset(3), 0.0f, 0.5f);
	TestEqual(TEXT("The change is reported once, with no item passed on the way"), Centred->ToString(), FString(TEXT("Item 3")));
	TestFalse(TEXT("An item index the list does not host is refused"), Carousel->SetCenteredItemIndex(7, false));
	Centred->Reset();

	// Driving the list centres its driven item, with the carousel's Center Motion.
	List->SetDrivenItemIndex(1, false);
	Harness.Advance(1.0f / 60.0f);
	TestTrue(TEXT("Centring a driven item takes time"), Carousel->IsCenteringProgrammatically());
	Harness.Advance(1.0f);
	TestEqual(TEXT("The driven item is centred within a second"), Carousel->GetCenteredItemIndex(), 1);
	TestEqual(TEXT("Its centre is the viewport's"), Rig.GetItemCentreOffset(1), 0.0f, 0.5f);
	TestFalse(TEXT("The centring has ended"), Carousel->IsCenteringProgrammatically());
	TestTrue(TEXT("The last centred item reported is the driven one"), Centred->Events.Num() > 0 && Centred->Events.Last() == TEXT("Item 1"));

	// At motion scale 0 an animated centring lands at once: within the frames that lay it out, not Center Motion's time.
	Settings->DefaultMotionScale = 0.0f;
	Centred->Reset();
	List->SetDrivenItemIndex(4, false);
	Harness.Advance(3.0f / 60.0f);
	TestEqual(TEXT("At motion scale 0 the driven item is centred at once"), Carousel->GetCenteredItemIndex(), 4);
	TestEqual(TEXT("Its centre is the viewport's"), Rig.GetItemCentreOffset(4), 0.0f, 0.5f);
	TestEqual(TEXT("No item on the way is reported"), Centred->ToString(), FString(TEXT("Item 4")));

	// A click on an entry that is not centred only centres it; a click on the centred entry clicks it, which confirms a
	// list with Confirm On Item Click.
	List->bConfirmOnItemClick = true;
	Rig.Click(*List->GetHostedEntryWidget(3));
	Harness.Advance(0.1f);
	TestEqual(TEXT("A click on an entry that is not centred centres it"), Carousel->GetCenteredItemIndex(), 3);
	TestEqual(TEXT("It becomes the driven item"), List->GetDrivenItemIndex(), 3);
	TestEqual(TEXT("That click confirms nothing"), Confirmed->ToString(), FString());
	Rig.Click(*List->GetHostedEntryWidget(3));
	Harness.Advance(0.1f);
	TestEqual(TEXT("A click on the centred entry confirms it, once"), Confirmed->ToString(), FString(TEXT("Item 3")));
	TestEqual(TEXT("The centred item stays"), Carousel->GetCenteredItemIndex(), 3);
	return true;
}

// A drag across a looping carousel whose list binds On Navigation Input: on a card that is not centred it only centres
// that card, as a first click does, and broadcasts nothing; on the centred card it is broadcast, as on a plain list.
// Either way the carousel keeps the gesture to its release, which clicks nothing, and the content does not follow it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciCarouselNavigationInputDragTest, "ReciCommonUI.Carousel.NavigationInputDrag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciCarouselNavigationInputDragTest::RunTest(const FString& Parameters)
{
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	UReciCommonUISettings* Settings = GetMutableDefault<UReciCommonUISettings>();
	Harness.Preserve(*Settings);
	Settings->DefaultMotionScale = 1.0f;
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	Harness.ActivateScreen(*Screen);
	const FReciTestCarousel Rig(*Screen, true, 4);
	UReciCarouselScrollBox* Carousel = Rig.Carousel;
	UReciList* List = Rig.List;
	List->bConfirmOnItemClick = true;
	const TStrongObjectPtr<UReciTestEventRecorder> Navigation(NewObject<UReciTestEventRecorder>());
	const TStrongObjectPtr<UReciTestEventRecorder> Confirmed(NewObject<UReciTestEventRecorder>());
	List->OnNavigationInput.AddDynamic(Navigation.Get(), &UReciTestEventRecorder::RecordNavigation);
	List->OnItemConfirmed.AddDynamic(Confirmed.Get(), &UReciTestEventRecorder::RecordItem);
	Harness.Advance(0.5f);
	if (!TestEqual(TEXT("The first card is centred"), Carousel->GetCenteredItemIndex(), 0)
		|| !TestNotNull(TEXT("The second card exists"), List->GetHostedEntryWidget(1)))
	{
		return false;
	}

	// A drag up across the second card, which is not centred, only centres it.
	TestTrue(TEXT("The carousel holds a drag across a card that is not centred to its release"),
		Rig.Drag(*List->GetHostedEntryWidget(1), FVector2D(0.0, -15.0)));
	TestEqual(TEXT("The drag broadcasts nothing"), Navigation->ToString(), FString());
	TestEqual(TEXT("It makes the card the driven item"), List->GetDrivenItemIndex(), 1);
	TestEqual(TEXT("Its release confirms nothing, though the card is driven by then"), Confirmed->ToString(), FString());
	Harness.Advance(1.0f);
	TestEqual(TEXT("The card is centred"), Carousel->GetCenteredItemIndex(), 1);
	TestEqual(TEXT("Its centre is the viewport's"), Rig.GetItemCentreOffset(1), 0.0f, 0.5f);

	// The same drag across the centred card is broadcast; a drag up reads as Down.
	const float Offset = Carousel->GetScrollOffset();
	TestTrue(TEXT("The carousel holds a drag across the centred card to its release"),
		Rig.Drag(*List->GetHostedEntryWidget(1), FVector2D(0.0, -15.0)));
	TestEqual(TEXT("The drag is broadcast once"), Navigation->ToString(), FString(TEXT("Navigation Down")));
	TestEqual(TEXT("The card stays the driven item"), List->GetDrivenItemIndex(), 1);
	TestEqual(TEXT("Its release confirms nothing"), Confirmed->ToString(), FString());
	TestEqual(TEXT("The content does not move"), Carousel->GetScrollOffset(), Offset, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciListLoopingEntriesTest, "ReciCommonUI.List.LoopingEntries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciListLoopingEntriesTest::RunTest(const FString& Parameters)
{
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	UReciCommonUISettings* Settings = GetMutableDefault<UReciCommonUISettings>();
	Harness.Preserve(*Settings);
	Settings->DefaultMotionScale = 1.0f;
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	Harness.ActivateScreen(*Screen);
	const FReciTestCarousel Rig(*Screen, true, 4);
	UReciCarouselScrollBox* Carousel = Rig.Carousel;
	UReciList* List = Rig.List;
	const TStrongObjectPtr<UReciTestEventRecorder> Centred(NewObject<UReciTestEventRecorder>());
	const TStrongObjectPtr<UReciTestEventRecorder> Selected(NewObject<UReciTestEventRecorder>());
	Carousel->OnCenteredItemChanged.AddDynamic(Centred.Get(), &UReciTestEventRecorder::RecordItem);
	List->OnItemSelected.AddDynamic(Selected.Get(), &UReciTestEventRecorder::RecordSelected);
	Harness.Advance(0.5f);

	// Every item keeps one index; its presentations are three copies, each given the item.
	TestEqual(TEXT("The list holds its four items"), List->GetItemCount(), 4);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const TArray<UUserWidget*> Copies = List->GetEntryPresentations(Index);
		TestEqual(FString::Printf(TEXT("Item %d has three copies"), Index), Copies.Num(), 3);
		for (const UUserWidget* Copy : Copies)
		{
			const UReciTestListEntry* Entry = Cast<UReciTestListEntry>(Copy);
			TestTrue(FString::Printf(TEXT("Each copy of item %d is given item %d"), Index, Index),
				Entry && Entry->Data == List->GetItemAt(Index));
		}
		TestTrue(FString::Printf(TEXT("The hosted entry of item %d is one of its copies"), Index),
			Copies.Contains(List->GetHostedEntryWidget(Index)));
	}
	TestEqual(TEXT("The first item is centred"), Carousel->GetCenteredItemIndex(), 0);
	TestEqual(TEXT("Its hosted copy is at the viewport's centre"), Rig.GetItemCentreOffset(0), 0.0f, 0.5f);
	const float StartOffset = Carousel->GetScrollOffset();

	// Past the end: stepping forward through every item comes back to the first by the next copy, and the carousel
	// rebases its scroll offset by a cycle on the way without a visible jump.
	Centred->Reset();
	for (const int32 Index : {1, 2, 3, 0})
	{
		List->SetDrivenItemIndex(Index, false);
		Harness.Advance(1.0f);
		TestEqual(FString::Printf(TEXT("Driving item %d centres it"), Index), Carousel->GetCenteredItemIndex(), Index);
		TestEqual(FString::Printf(TEXT("The hosted copy of item %d is at the viewport's centre"), Index), Rig.GetItemCentreOffset(Index), 0.0f, 0.5f);
	}
	TestEqual(TEXT("Each step moved forward to the next item"), Centred->ToString(), FString(TEXT("Item 1, Item 2, Item 3, Item 0")));
	TestEqual(TEXT("The scroll offset is back where it started"), Carousel->GetScrollOffset(), StartOffset, 0.5f);

	// A pointer click on a copy that is not hosted selects its item, once.
	UReciTestListEntry* OtherCopy = nullptr;
	for (UUserWidget* Copy : List->GetEntryPresentations(1))
	{
		if (Copy != List->GetHostedEntryWidget(1))
		{
			OtherCopy = Cast<UReciTestListEntry>(Copy);
			break;
		}
	}
	if (TestNotNull(TEXT("Item 1 has a copy that is not hosted"), OtherCopy))
	{
		Selected->Reset();
		OtherCopy->SimulateClick();
		TestEqual(TEXT("A click on another copy selects the item"), List->GetSelectedItemIndex(), 1);
		TestEqual(TEXT("It selects it once"), Selected->ToString(), FString(TEXT("Selected Item1")));
	}

	// One item has nothing to loop.
	List->SetListItems({NewObject<UReciTextListItemData>(List, TEXT("Only"))});
	TestEqual(TEXT("A list of one item makes no copies"), List->GetEntryPresentations(0).Num(), 1);
	return true;
}

// A looping carousel wider than three cycles of its copies makes more once it is laid out, which replaces every entry;
// so does Set List Items. Either way focus on the driven card lands on one of that card's new copies.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciCarouselLoopRebuildFocusTest, "ReciCommonUI.Carousel.LoopRebuildFocus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciCarouselLoopRebuildFocusTest::RunTest(const FString& Parameters)
{
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	UReciCommonUISettings* Settings = GetMutableDefault<UReciCommonUISettings>();
	Harness.Preserve(*Settings);
	Settings->DefaultMotionScale = 1.0f;
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	Harness.ActivateScreen(*Screen);
	const FReciTestCarousel Rig(*Screen, true, 4);
	UReciList* List = Rig.List;
	USizeBox* Frame = Cast<USizeBox>(Rig.Carousel->GetParent());
	if (!TestNotNull(TEXT("The carousel is in its frame"), Frame))
	{
		return false;
	}
	APlayerController* Player = Harness.GetPlayerController();
	// Whether one of the item's copies has the harness user's focus.
	const auto CardHasFocus = [List, Player](const int32 Index)
	{
		for (const UUserWidget* Copy : List->GetEntryPresentations(Index))
		{
			if (Copy->HasUserFocus(Player) || Copy->HasUserFocusedDescendants(Player))
			{
				return true;
			}
		}
		return false;
	};

	// Focus on the second card before the first layout, in a carousel too wide for three cycles.
	Frame->SetWidthOverride(600.0f);
	TestTrue(TEXT("The second card takes focus"), List->SetDrivenItemIndex(1, true));
	TestEqual(TEXT("Each card starts with three copies"), List->GetEntryPresentations(1).Num(), 3);
	Harness.Advance(1.0f);
	TestTrue(TEXT("Laid out, the carousel makes more copies"), List->GetEntryPresentations(1).Num() > 3);
	TestEqual(TEXT("The second card stays the driven item"), List->GetDrivenItemIndex(), 1);
	TestTrue(TEXT("and one of its new copies has focus"), CardHasFocus(1));

	// Set List Items with the same items.
	TArray<UObject*> Items;
	for (int32 Index = 0; Index < List->GetItemCount(); ++Index)
	{
		Items.Add(List->GetItemAt(Index));
	}
	List->SetListItems(Items);
	Harness.Advance(1.0f);
	TestEqual(TEXT("Set List Items keeps the driven card"), List->GetDrivenItemIndex(), 1);
	TestTrue(TEXT("and focus on one of its new copies"), CardHasFocus(1));
	return true;
}

// Insert Item before the focused, centred card rebuilds the entries and moves the card's index by one, so the scroll
// offset it had centres the card's neighbour. Focus returns to the card, and the carousel jumps back to it, as after any
// rebuild, instead of gliding there with Center Motion. In a looping carousel and in one that does not loop.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciCarouselRebuildFocusCentringTest, "ReciCommonUI.Carousel.RebuildFocusCentring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciCarouselRebuildFocusCentringTest::RunTest(const FString& Parameters)
{
	for (const bool bLooping : {false, true})
	{
		FReciTestHarness Harness(*this);
		if (!Harness.IsValid())
		{
			return false;
		}
		UReciCommonUISettings* Settings = GetMutableDefault<UReciCommonUISettings>();
		Harness.Preserve(*Settings);
		Settings->DefaultMotionScale = 1.0f;
		UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
		Harness.ActivateScreen(*Screen);
		const FReciTestCarousel Rig(*Screen, bLooping, 5);
		UReciCarouselScrollBox* Carousel = Rig.Carousel;
		UReciList* List = Rig.List;
		const FString Kind = bLooping ? TEXT("Looping: ") : TEXT("Not looping: ");

		TestTrue(Kind + TEXT("the third card takes focus"), List->SetDrivenItemIndex(2, true));
		Harness.Advance(1.0f);
		TestEqual(Kind + TEXT("it is centred"), Carousel->GetCenteredItemIndex(), 2);
		UObject* Card = List->GetItemAt(2);
		TestTrue(Kind + TEXT("Insert Item inserts before the card"), List->InsertItem(NewObject<UReciTextListItemData>(List), 0));
		TestTrue(Kind + TEXT("the card is the driven item at its new index, with focus"),
			List->GetItemAt(3) == Card && List->GetDrivenItemIndex() == 3 && List->GetFocusedItemIndex() == 3);
		Harness.Advance(3.0f / 60.0f);
		TestEqual(Kind + TEXT("the carousel centres the card within the frames that lay it out"), Carousel->GetCenteredItemIndex(), 3);
		TestEqual(Kind + TEXT("its centre is the viewport's"), Rig.GetItemCentreOffset(3), 0.0f, 0.5f);
	}
	return true;
}

// Focus leaves the carousel's centred card for a widget beside it, and a drag along the carousel scrolls two cards on: the
// card it settles on is the driven item, without focus. Focus that comes back lands on that card, which stays centred and
// stays the driven item: the carousel does not glide back to the card focus left. In a carousel that does not loop and in
// a looping one.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciCarouselDragFocusReturnTest, "ReciCommonUI.Carousel.DragFocusReturn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciCarouselDragFocusReturnTest::RunTest(const FString& Parameters)
{
	for (const bool bLooping : {false, true})
	{
		FReciTestHarness Harness(*this);
		if (!Harness.IsValid())
		{
			return false;
		}
		UReciCommonUISettings* Settings = GetMutableDefault<UReciCommonUISettings>();
		Harness.Preserve(*Settings);
		// Motion off for the drag: its release flings nothing, so it settles where it ends, two cards on.
		Settings->DefaultMotionScale = 0.0f;
		UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
		Harness.ActivateScreen(*Screen);
		const FReciTestCarousel Rig(*Screen, bLooping, 5);
		UReciCarouselScrollBox* Carousel = Rig.Carousel;
		UReciList* List = Rig.List;
		// Down from the widget beside the carousel moves focus to its list. Slate does not hit-test the harness window,
		// so an explicit Navigation rule stands in for the geometric search.
		UReciTestFocusTarget* Beside = Harness.CreateTarget(TEXT("Beside"), Screen);
		Beside->SetNavigationRuleExplicit(EUINavigation::Down, List);
		const TStrongObjectPtr<UReciTestEventRecorder> Centred(NewObject<UReciTestEventRecorder>());
		Carousel->OnCenteredItemChanged.AddDynamic(Centred.Get(), &UReciTestEventRecorder::RecordItem);
		APlayerController* Player = Harness.GetPlayerController();
		const auto CardHasFocus = [List, Player](const int32 Index)
		{
			for (const UUserWidget* Copy : List->GetEntryPresentations(Index))
			{
				if (Copy->HasUserFocus(Player) || Copy->HasUserFocusedDescendants(Player))
				{
					return List->GetFocusedItemIndex() == Index;
				}
			}
			return false;
		};
		const FString Kind = bLooping ? TEXT("Looping: ") : TEXT("Not looping: ");
		Harness.Advance(0.5f);

		TestTrue(Kind + TEXT("The centred first card takes focus"), List->SetDrivenItemIndex(0, true) && CardHasFocus(0));
		TestTrue(Kind + TEXT("Focus moves beside the carousel"), Harness.Focus(*Beside));
		// A drag along the carousel by the distance from the first card's centre to the third's, as laid out: the third card
		// is outside the viewport, where its cached geometry can be stale.
		const double Distance = Rig.GetCentreOffset(List->GetHostedEntryGeometry(2)) - Rig.GetCentreOffset(List->GetHostedEntryGeometry(0));
		TestTrue(Kind + TEXT("The carousel holds a drag along it to its release"),
			Rig.Drag(*List->GetHostedEntryWidget(0), FVector2D(-Distance, 0.0)));
		Harness.Advance(1.0f);
		TestEqual(Kind + TEXT("The drag centres the third card"), Carousel->GetCenteredItemIndex(), 2);
		TestEqual(Kind + TEXT("which becomes the driven item"), List->GetDrivenItemIndex(), 2);
		TestTrue(Kind + TEXT("Focus stays beside the carousel"), Beside->HasUserFocus(Player));

		// Motion on again, so that a glide would show.
		Settings->DefaultMotionScale = 1.0f;
		const float Offset = Carousel->GetScrollOffset();
		Centred->Reset();
		Harness.SlateKeyDown(EKeys::Down);
		Harness.SlateKeyUp(EKeys::Down);
		TestTrue(Kind + TEXT("Down returns focus to the centred third card"), CardHasFocus(2));
		TestEqual(Kind + TEXT("which stays the driven item"), List->GetDrivenItemIndex(), 2);
		Harness.Advance(1.0f);
		TestEqual(Kind + TEXT("The carousel keeps the third card centred"), Carousel->GetCenteredItemIndex(), 2);
		TestEqual(Kind + TEXT("and centres no other card on the way"), Centred->ToString(), FString());
		TestEqual(Kind + TEXT("Its content does not move"), Carousel->GetScrollOffset(), Offset, 0.5f);
	}
	return true;
}

#endif
