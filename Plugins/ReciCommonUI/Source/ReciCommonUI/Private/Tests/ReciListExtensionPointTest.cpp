// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tests/ReciTestHarness.h"
#include "Tests/ReciTestWidgets.h"
#include "Widgets/ReciListController.h"
#include "Widgets/ReciListLayoutBox.h"
#include "Widgets/ReciTextListItem.h"

namespace
{
	/** Records, in order, what a list hands its controller. */
	class FReciTestListController final : public IReciListController
	{
	public:
		TArray<FString> Calls;

		virtual bool SupportsEntryClass(const UClass* Class) const override { return Class == UReciTestListEntry::StaticClass(); }
		virtual void InitializeEntry(UUserWidget* Entry, UObject* Item, int32 Index) override { Calls.Add(FString::Printf(TEXT("Initialize %d"), Index)); }
		virtual bool IsSelectionOnly() const override { return false; }
		virtual void ClickItem(int32 Index) override { Calls.Add(FString::Printf(TEXT("Click %d"), Index)); }
		virtual void DrivenItemChanged(int32 Index, bool bGiveFocus) override { Calls.Add(FString::Printf(TEXT("Driven %d"), Index)); }
		virtual bool ConfirmItem() override { Calls.Add(TEXT("Confirm")); return true; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciListEmphasisTest, "ReciCommonUI.List.Emphasis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciListEmphasisTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	UObject* A = NewObject<UReciTextListItemData>(World);
	UObject* B = NewObject<UReciTextListItemData>(World);

	// The layout box tells the list while a viewport that drives emphasis hosts it.
	UReciTestEmphasisList* List = CreateWidget<UReciTestEmphasisList>(World);
	List->ItemWidgetClass = UReciTestEmphasisEntry::StaticClass();
	List->bLoopingEntries = true;
	UReciHorizontalBox* Row = NewObject<UReciHorizontalBox>(World);
	Row->SetContent(List);
	TestFalse(TEXT("With no viewport the list's emphasis is not driven"), List->bEmphasisDriven);
	UReciTestLoopingScrollBox* Viewport = NewObject<UReciTestLoopingScrollBox>(World);
	Viewport->AddChild(Row);
	TestTrue(TEXT("A viewport that drives emphasis drives the list's"), List->bEmphasisDriven);
	const TSharedRef<SWidget> Slate = List->TakeWidget();
	List->SetListItems({A, B});

	// The list passes an item's emphasis, within 0..1, to every widget presenting the item and to no other.
	TestTrue(TEXT("An item takes emphasis"), List->SetItemEmphasis(0, 0.5f));
	const TArray<UUserWidget*> Copies = List->GetEntryPresentations(0);
	TestEqual(TEXT("A looping item has three presentations"), Copies.Num(), 3);
	for (UUserWidget* Copy : Copies)
	{
		const UReciTestEmphasisEntry* Entry = Cast<UReciTestEmphasisEntry>(Copy);
		TestTrue(TEXT("Each copy receives the item's emphasis once"),
			Entry && Entry->EmphasisCount == 1 && FMath::IsNearlyEqual(Entry->Emphasis, 0.5f));
	}
	for (UUserWidget* Copy : List->GetEntryPresentations(1))
	{
		const UReciTestEmphasisEntry* Entry = Cast<UReciTestEmphasisEntry>(Copy);
		TestTrue(TEXT("Another item's entries receive nothing"), Entry && Entry->EmphasisCount == 0);
	}
	List->SetItemEmphasis(1, 2.0f);
	const UReciTestEmphasisEntry* Second = Cast<UReciTestEmphasisEntry>(List->GetHostedEntryWidget(1));
	TestTrue(TEXT("Emphasis above 1 arrives as 1"), Second && FMath::IsNearlyEqual(Second->Emphasis, 1.0f));
	TestFalse(TEXT("An index with no item takes no emphasis"), List->SetItemEmphasis(2, 1.0f));

	Viewport->RemoveChild(Row);
	TestFalse(TEXT("Leaving the viewport ends driven emphasis"), List->bEmphasisDriven);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciListControllerTest, "ReciCommonUI.List.Controller",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciListControllerTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	UObject* A = NewObject<UReciTextListItemData>(World);
	UObject* B = NewObject<UReciTextListItemData>(World);
	UObject* Owner = NewObject<UReciTestEventRecorder>(World);
	FReciTestListController Controller;

	// While attached, the controller gives each entry its data and takes the list's decisions.
	UReciList* List = CreateWidget<UReciList>(World);
	List->ItemWidgetClass = UReciTestListEntry::StaticClass();
	List->SetListController(Owner, &Controller);
	TestTrue(TEXT("The attached controller is the list's"), List->GetListController() == &Controller);
	const TSharedRef<SWidget> Slate = List->TakeWidget();
	List->SetListItems({A, B});
	const UReciTestListEntry* Entry = Cast<UReciTestListEntry>(List->GetHostedEntryWidget(0));
	TestTrue(TEXT("The controller gives an entry its data"), Entry && !Entry->Data);
	List->SetDrivenItemIndex(1, false);
	TestTrue(TEXT("The controller confirms"), List->ConfirmDrivenItem());
	List->SetItemEmphasis(0, 0.25f);
	List->HandleHostedItemClick(0);
	TestEqual(TEXT("The list hands the controller its decisions, and keeps emphasis to its entries"),
		FString::Join(Controller.Calls, TEXT(", ")),
		FString(TEXT("Initialize 0, Initialize 1, Driven 0, Driven 1, Confirm, Click 0")));

	// A controller whose owner is gone is never called.
	Owner->MarkAsGarbage();
	TestNull(TEXT("A controller outlives no owner"), List->GetListController());
	List->SetListController(nullptr, nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciListEntrySupportTest, "ReciCommonUI.List.EntrySupport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciListEntrySupportTest::RunTest(const FString& Parameters)
{
	FReciScopedReports Reports;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	UObject* A = NewObject<UReciTextListItemData>(World);
	UReciList* List = CreateWidget<UReciList>(World);

	// A list that creates entries of a second class gives them their items as Set List Items does.
	UReciTestListEntry* Entry = Cast<UReciTestListEntry>(UReciListBase::CreateEntryForItem(*List, *A, UReciTestListEntry::StaticClass()));
	TestTrue(TEXT("A second entry class's entry gets its item"), Entry && Entry->Data == A);
	TestNull(TEXT("A class with no Reci List Item interface makes no entry"),
		UReciListBase::CreateEntryForItem(*List, *A, UReciTestPlainScreen::StaticClass()));
	TestTrue(TEXT("An entry that is a button is its own entry button"), Entry && UReciListBase::FindEntryButton(Entry) == Entry);

	// And reports what it cannot serve once, as for Entry Widget Class.
	AddExpectedMessagePlain(TEXT("ReciTestActivatableEntry derives from Common Activatable Widget"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 1);
	List->ReportEntryWidgetClass(UReciTestActivatableEntry::StaticClass());
	List->ReportEntryWidgetClass(UReciTestActivatableEntry::StaticClass());
#if WITH_EDITOR
	FReciTestCompilerLog Log;
	List->ValidateEntryWidgetClass(UReciTestActivatableEntry::StaticClass(), Log);
	TestEqual(TEXT("An activatable second entry class is a compile warning"), Log.Warnings.Num(), 1);
#endif
	return true;
}

#endif
