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
#include "Widgets/ReciListLayoutBox.h"

namespace
{
	UObject* GetGridPageVariable(UUserWidget& Widget, const TCHAR* Name)
	{
		const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Widget.GetClass(), Name);
		return Property ? Property->GetObjectPropertyValue_InContainer(&Widget) : nullptr;
	}
}

// The demo's grid page (Content/Demo/Pages/WBP_DemoGridPage), laid out and painted: Accept on a focused tile of the
// Multi grid toggles it, as a click does, because the page toggles the tile on the grid's On Item Confirmed (a Multi
// list's confirm leaves the set alone). The status line lists the selected tiles in order, and asks for one when none is.
// The grid does not wrap, so from its edges focus leaves by Slate navigation (Up from the top row reaches the tabs).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciDemoGridPageTest, "ReciCommonUI.Demo.GridPage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciDemoGridPageTest::RunTest(const FString& Parameters)
{
	UClass* PageClass = LoadClass<UUserWidget>(nullptr, TEXT("/ReciCommonUI/Demo/Pages/WBP_DemoGridPage.WBP_DemoGridPage_C"));
	if (!TestNotNull(TEXT("The page's Blueprint class loads"), PageClass))
	{
		return false;
	}
	// A tile shows the click action's key, which CommonUI's input subsystem loads when a local player starts; the test
	// has none.
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

	UReciListBase* Grid = Cast<UReciListBase>(GetGridPageVariable(*Page, TEXT("Grid")));
	const UTextBlock* Status = Cast<UTextBlock>(GetGridPageVariable(*Page, TEXT("StatusText")));
	if (!TestNotNull(TEXT("The page holds its grid"), Grid) || !TestNotNull(TEXT("and its status line"), Status))
	{
		return false;
	}
	TestTrue(TEXT("The grid is a Multi list"), Grid->SelectionMode == ESelectionMode::Multi);
	const UReciTileBox* Tiles = Cast<UReciTileBox>(Page->GetWidgetFromName(TEXT("Tiles")));
	if (!TestNotNull(TEXT("The grid is laid out by its Reci Tile Box"), Tiles))
	{
		return false;
	}
	TestFalse(TEXT("The grid's navigation does not wrap"), Tiles->bWrapNavigation);
	UCommonButtonBase* Tile = Grid->GetHostedEntryButton(2);
	UCommonButtonBase* Other = Grid->GetHostedEntryButton(5);
	if (!TestNotNull(TEXT("The grid has its tiles"), Tile) || !TestNotNull(TEXT("more than one"), Other))
	{
		return false;
	}
	TestEqual(TEXT("No tile starts selected"), Grid->GetNumItemsSelected(), 0);
	TestEqual(TEXT("The status line asks for a selection"), Status->GetText().ToString(), FString(TEXT("Select tiles.")));

	TestTrue(TEXT("Focus reaches a tile"), Harness.Focus(*Tile));
	TestTrue(TEXT("Accept on the tile is taken"), Harness.KeyDown(EKeys::Enter));
	Harness.KeyUp(EKeys::Enter);
	TestTrue(TEXT("Accept selects the focused tile"), Tile->GetSelected());
	TestEqual(TEXT("One tile is selected"), Grid->GetNumItemsSelected(), 1);
	TestEqual(TEXT("The status line names it"), Status->GetText().ToString(), FString(TEXT("Selected: Tile 3")));
	TestTrue(TEXT("Focus reaches another tile"), Harness.Focus(*Other));
	Harness.KeyDown(EKeys::Enter);
	Harness.KeyUp(EKeys::Enter);
	TestTrue(TEXT("Accept selects that tile too"), Other->GetSelected());
	TestEqual(TEXT("Multi keeps both"), Grid->GetNumItemsSelected(), 2);
	TestEqual(TEXT("The status line names both, in order"), Status->GetText().ToString(), FString(TEXT("Selected: Tile 3, Tile 6")));
	TestTrue(TEXT("Focus returns to the first tile"), Harness.Focus(*Tile));
	Harness.KeyDown(EKeys::Enter);
	Harness.KeyUp(EKeys::Enter);
	TestFalse(TEXT("Accept on a selected tile deselects it"), Tile->GetSelected());
	TestEqual(TEXT("The other tile stays selected"), Grid->GetNumItemsSelected(), 1);
	TestEqual(TEXT("The status line drops the deselected tile"), Status->GetText().ToString(), FString(TEXT("Selected: Tile 6")));
	TestTrue(TEXT("Focus reaches the other tile again"), Harness.Focus(*Other));
	Harness.KeyDown(EKeys::Gamepad_FaceButton_Bottom);
	Harness.KeyUp(EKeys::Gamepad_FaceButton_Bottom);
	TestFalse(TEXT("Gamepad Accept deselects it too"), Other->GetSelected());
	TestEqual(TEXT("No tile is selected"), Grid->GetNumItemsSelected(), 0);
	TestEqual(TEXT("The status line asks for a selection again"), Status->GetText().ToString(), FString(TEXT("Select tiles.")));

	Page->ReleaseSlateResources(true);
	return true;
}

#endif
