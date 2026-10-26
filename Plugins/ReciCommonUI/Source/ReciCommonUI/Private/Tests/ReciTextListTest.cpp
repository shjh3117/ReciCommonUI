// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Misc/AutomationTest.h"
#include "Tests/ReciInteractionTestEntry.h"
#include "Tests/ReciTestHarness.h"
#include "Tests/ReciTestWidgets.h"
#include "Widgets/ReciList.h"
#include "Widgets/ReciListLayoutBox.h"
#include "Widgets/ReciTextListItem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciListTextItemsTest, "ReciCommonUI.List.TextItems",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciListTextItemsTest::RunTest(const FString& Parameters)
{
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	UReciVerticalBox* Layout = Screen->WidgetTree->ConstructWidget<UReciVerticalBox>(UReciVerticalBox::StaticClass(), TEXT("Layout"));
	UReciList* List = Screen->WidgetTree->ConstructWidget<UReciList>(UReciList::StaticClass(), TEXT("Choices"));
	List->ItemWidgetClass = UReciTestTextItemButton::StaticClass();
	Layout->SetContent(List);
	Screen->GetContent()->AddChild(Layout);

	// Each entry, a Reci Text List Item Button, gets its text and shows it in ItemTextBlock with no graph.
	const auto Texts = [List]()
	{
		TArray<FString> Shown;
		for (int32 Index = 0; Index < List->GetItemCount(); ++Index)
		{
			const UReciTestTextItemButton* Entry = Cast<UReciTestTextItemButton>(List->GetHostedEntryWidget(Index));
			const FString ItemText = Entry ? Entry->ItemText.ToString() : TEXT("?");
			const FString BlockText = Entry && Entry->GetItemTextBlock() ? Entry->GetItemTextBlock()->GetText().ToString() : TEXT("?");
			Shown.Add(ItemText == BlockText ? ItemText : ItemText + TEXT("/") + BlockText);
		}
		return FString::Join(Shown, TEXT(", "));
	};
	List->SetTextListItems({FText::FromString(TEXT("One")), FText::FromString(TEXT("Two")), FText::FromString(TEXT("Three"))});
	TestEqual(TEXT("Set Text List Items makes one entry per text, and each entry shows its text"), Texts(), FString(TEXT("One, Two, Three")));
	const UReciTextListItemData* TwoData = Cast<UReciTextListItemData>(List->GetItemAt(1));
	TestTrue(TEXT("Each item is a text item holding its text"), TwoData && TwoData->ItemText.ToString() == TEXT("Two"));

	// Editing the items keeps the selection on the same object.
	List->SetSelectedItemIndex(1);
	UObject* Two = List->GetItemAt(1);
	UReciTextListItemData* Four = NewObject<UReciTextListItemData>(List);
	Four->ItemText = FText::FromString(TEXT("Four"));
	TestTrue(TEXT("Add Item adds a new item"), List->AddItem(Four));
	TestFalse(TEXT("Add Item refuses an item the list holds"), List->AddItem(Four));
	TestEqual(TEXT("An added text item's entry shows its text"), Texts(), FString(TEXT("One, Two, Three, Four")));
	TestEqual(TEXT("Adding keeps the selection"), List->GetSelectedItemIndex(), 1);
	UReciTextListItemData* Zero = NewObject<UReciTextListItemData>(List);
	Zero->ItemText = FText::FromString(TEXT("Zero"));
	TestTrue(TEXT("Insert Item inserts at an index"), List->InsertItem(Zero, 0));
	TestEqual(TEXT("The inserted entry comes first"), Texts(), FString(TEXT("Zero, One, Two, Three, Four")));
	TestTrue(TEXT("Inserting keeps the selected object, now one further on"), List->GetSelectedItemIndex() == 2 && List->GetItemAt(2) == Two);
	TestFalse(TEXT("Insert Item refuses an index past the end"), List->InsertItem(NewObject<UReciTextListItemData>(List), 9));
	TestTrue(TEXT("Remove Item At removes by index"), List->RemoveItemAt(0));
	TestTrue(TEXT("Removing another item keeps the selected object"), List->GetSelectedItemIndex() == 1 && List->GetItemAt(1) == Two);
	TestTrue(TEXT("Remove Item removes by object"), List->RemoveItem(Four));
	TestFalse(TEXT("Remove Item refuses an item the list does not hold"), List->RemoveItem(Four));
	TestEqual(TEXT("The entries follow"), Texts(), FString(TEXT("One, Two, Three")));
	TestTrue(TEXT("The selection is still the same object"), List->GetItemAt(List->GetSelectedItemIndex()) == Two);
	List->ClearListItems();
	TestEqual(TEXT("Clear List Items leaves no item"), List->GetItemCount(), 0);

	// An entry class without the text contract cannot show texts: the list stays empty and says so.
	List->ItemWidgetClass = UReciInteractionTestEntry::StaticClass();
	AddExpectedMessagePlain(TEXT("SetTextListItems requires Entry Widget Class to implement ReciTextListItem"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 1);
	List->SetTextListItems({FText::FromString(TEXT("One"))});
	TestEqual(TEXT("A list whose entries take no text gets no text items"), List->GetItemCount(), 0);
	return true;
}

#endif
