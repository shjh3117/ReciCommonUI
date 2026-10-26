// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/UserWidget.h"
#include "CommonInputSettings.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "ICommonInputModule.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Tests/ReciTestHarness.h"
#include "Widgets/ReciFocusStyleButtonBase.h"
#include "Widgets/ReciListBase.h"
#include "Widgets/ReciTwoLevelListBase.h"

namespace
{
	UObject* GetWidgetVariable(UUserWidget& Widget, const TCHAR* Name)
	{
		const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Widget.GetClass(), Name);
		return Property ? Property->GetObjectPropertyValue_InContainer(&Widget) : nullptr;
	}

	/** The Display Scale of a child entry, or -1 when there is no such entry. */
	float GetChildDisplayScale(const UReciTwoLevelListBase& List, const int32 ParentIndex, const int32 ChildIndex)
	{
		const UReciListBase* Children = List.GetChildList(ParentIndex);
		const UReciFocusStyleButtonBase* Entry = Children
			? Cast<UReciFocusStyleButtonBase>(Children->GetHostedEntryWidget(ChildIndex)) : nullptr;
		return Entry ? Entry->GetDisplayScale() : -1.0f;
	}
}

// The demo's two-level page (Content/Demo/Pages/WBP_DemoTwoLevelPage), laid out and painted: a row for each category
// with its children in a carousel, the centred child grown by its emphasis, keys that move between rows and leave the
// list at its ends, and the confirmed pair shown on the page. Focus that left the list comes back after Refresh Items to
// the focused parent's row, which the game moved while focus was away.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciDemoTwoLevelPageTest, "ReciCommonUI.Demo.TwoLevelPage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciDemoTwoLevelPageTest::RunTest(const FString& Parameters)
{
	UClass* PageClass = LoadClass<UUserWidget>(nullptr, TEXT("/ReciCommonUI/Demo/Pages/WBP_DemoTwoLevelPage.WBP_DemoTwoLevelPage_C"));
	if (!TestNotNull(TEXT("The page's Blueprint class loads"), PageClass))
	{
		return false;
	}
	// Its Confirm button shows the click action's key, which CommonUI's input subsystem loads when a local player
	// starts; the test has none.
	ICommonInputModule::GetSettings().LoadData();
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	Harness.ActivateScreen(*Screen);
	UUserWidget* Page = CreateWidget<UUserWidget>(Harness.GetPlayerController(), PageClass);
	Screen->GetContent()->AddChild(Page);
	Harness.Advance(1.0f);

	UReciTwoLevelListBase* List = Cast<UReciTwoLevelListBase>(GetWidgetVariable(*Page, TEXT("TwoLevel")));
	const UTextBlock* Confirmed = Cast<UTextBlock>(GetWidgetVariable(*Page, TEXT("ConfirmedText")));
	if (!TestNotNull(TEXT("The page holds its two-level list"), List) || !TestNotNull(TEXT("and its confirmed text"), Confirmed))
	{
		return false;
	}
	TestTrue(TEXT("The page's composition is valid"), List->IsConfigurationValid());
	const UReciListBase* Rows = List->GetParentList();
	TestEqual(TEXT("A row for each category"), Rows ? Rows->GetHostedItemCount() : 0, 3);
	for (int32 Row = 0; Row < 3; ++Row)
	{
		const UReciListBase* Children = List->GetChildList(Row);
		TestTrue(FString::Printf(TEXT("Row %d has its children"), Row), Children && Children->GetHostedItemCount() > 1);
	}
	TestEqual(TEXT("The selected child is centred and grown to Max Display Scale"), GetChildDisplayScale(*List, 0, 0), 1.2f, 0.01f);
	TestEqual(TEXT("Its neighbour is not grown"), GetChildDisplayScale(*List, 0, 1), 1.0f, 0.01f);

	// Keys move between rows; at the first row and the first child a direction goes on to Slate navigation, so Up
	// reaches the card's Confirm button above the rows. Slate does not hit-test the harness window, so an explicit
	// Navigation rule on the list stands in for the geometric search.
	UWidget* FirstRow = Rows ? Rows->GetHostedEntryButton(0) : nullptr;
	UWidget* ConfirmButton = Cast<UWidget>(GetWidgetVariable(*Page, TEXT("Confirm")));
	if (TestNotNull(TEXT("The page holds its first row"), FirstRow) && TestNotNull(TEXT("and its Confirm button"), ConfirmButton))
	{
		TestTrue(TEXT("Focus reaches the first row"), Harness.Focus(*FirstRow));
		TestTrue(TEXT("Down is consumed"), Harness.KeyDown(EKeys::Down));
		Harness.KeyUp(EKeys::Down);
		TestEqual(TEXT("Down moves to the next row"), List->GetFocusedParentIndex(), 1);
		TestTrue(TEXT("Up is consumed"), Harness.KeyDown(EKeys::Up));
		Harness.KeyUp(EKeys::Up);
		TestEqual(TEXT("Up moves back to the first row"), List->GetFocusedParentIndex(), 0);
		TestFalse(TEXT("Up at the first row is left to Slate"), Harness.KeyDown(EKeys::Up));
		Harness.KeyUp(EKeys::Up);
		TestEqual(TEXT("The list stays on its first row"), List->GetFocusedParentIndex(), 0);
		TestFalse(TEXT("Left at the first child is left to Slate"), Harness.KeyDown(EKeys::Left));
		Harness.KeyUp(EKeys::Left);
		TestEqual(TEXT("The row keeps its first child"), List->GetSelectedChildIndex(), 0);
		List->SetNavigationRuleExplicit(EUINavigation::Up, ConfirmButton);
		Harness.SlateKeyDown(EKeys::Up);
		Harness.SlateKeyUp(EKeys::Up);
		TestTrue(TEXT("Slate navigation takes Up from the first row to Confirm"), ConfirmButton->HasAnyUserFocus() || ConfirmButton->HasFocusedDescendants());
		TestFalse(TEXT("Focus has left the list"), List->HasFocusedDescendants());
		List->SetNavigationRuleBase(EUINavigation::Up, EUINavigationRule::Escape);
	}

	const FString Before = Confirmed->GetText().ToString();
	TestTrue(TEXT("A child of another row can be selected"), List->SelectChild(1, 2, false));
	Harness.Advance(1.0f);
	TestEqual(TEXT("The newly selected child grows"), GetChildDisplayScale(*List, 1, 2), 1.2f, 0.01f);
	TestEqual(TEXT("Selecting does not confirm"), Confirmed->GetText().ToString(), Before);
	TestTrue(TEXT("Confirm Selection confirms the pair"), List->ConfirmSelection());
	TestEqual(TEXT("The page shows the confirmed pair"), Confirmed->GetText().ToString(), FString(TEXT("Confirmed: Shapes / Triangle")));

	// Focus left the list from the first row, and the second row became the focused parent without focus. Refresh Items
	// rebuilds the rows, and focus that comes back reaches the second row and keeps it the focused parent.
	List->RefreshItems();
	Harness.Advance(1.0f);
	TestTrue(TEXT("Focus comes back to the list"), Harness.Focus(*List));
	TestEqual(TEXT("The second row stays the focused parent"), List->GetFocusedParentIndex(), 1);
	const UWidget* SecondRow = Rows ? Rows->GetHostedEntryButton(1) : nullptr;
	TestTrue(TEXT("and has focus"), SecondRow && (SecondRow->HasAnyUserFocus() || SecondRow->HasFocusedDescendants()));

	Page->ReleaseSlateResources(true);
	return true;
}

#endif
