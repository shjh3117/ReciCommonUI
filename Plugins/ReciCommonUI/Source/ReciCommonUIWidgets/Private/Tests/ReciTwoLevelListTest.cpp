// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.
#include "Tests/ReciTwoLevelTestEntry.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Widgets/ReciAdvancedList.h"
#include "Widgets/ReciListLayoutBox.h"
#include "Widgets/ReciCarouselScrollBox.h"
#include "Widgets/ReciTwoLevelListBinding.h"

EOrientation UReciTwoLevelTestRow::ChildAxis = Orient_Horizontal;
int32 UReciTwoLevelTestRow::ViewportMode = 0;
bool UReciTwoLevelTestRow::bAdvanced = false;
bool UReciTwoLevelTestRow::bOmitChild = false;
bool UReciTwoLevelTestRow::bLoopingChild = false;
bool UReciTwoLevelTestRow::bTreeAtInitialize = false;

namespace
{
	UWidget* MakeAuthoredLayout(UWidgetTree* Tree, UReciListBase* List, EOrientation Axis, int32 Mode)
	{
		UReciListLayoutBoxBase* Box = Axis == Orient_Vertical
			? static_cast<UReciListLayoutBoxBase*>(Tree->ConstructWidget<UReciVerticalBox>())
			: Tree->ConstructWidget<UReciHorizontalBox>();
		Box->SetItemSpacing(19.0f);
		Box->SetContent(List);
		if (!Mode) return Box;
		UReciScrollBox* Scroll = Mode == 2
			? static_cast<UReciScrollBox*>(Tree->ConstructWidget<UReciCarouselScrollBox>())
			: Tree->ConstructWidget<UReciScrollBox>();
		Scroll->WheelScrollAmount = 83.0f;
		Scroll->AddChild(Box);
		return Scroll;
	}
}

TSharedRef<SWidget> UReciTwoLevelTestEntry::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		auto* Bounds = WidgetTree->ConstructWidget<USizeBox>();
		Bounds->SetWidthOverride(60);
		Bounds->SetHeightOverride(40);
		WidgetTree->RootWidget = Bounds;
	}
	return Super::RebuildWidget();
}

bool UReciTwoLevelTestRow::Initialize()
{
	if (bTreeAtInitialize)
	{
		if (!WidgetTree)
		{
			WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
		}
		SetIsFocusable(true);
		BuildTree();
	}
	return Super::Initialize();
}

TSharedRef<SWidget> UReciTwoLevelTestRow::RebuildWidget()
{
	SetIsFocusable(true);
	BuildTree();
	return Super::RebuildWidget();
}

void UReciTwoLevelTestRow::BuildTree()
{
	if (!WidgetTree->RootWidget)
	{
		auto* Border = WidgetTree->ConstructWidget<UBorder>();
		Border->SetPadding(FMargin(7, 11));
		WidgetTree->RootWidget = Border;
		if (!bOmitChild)
		{
			ChildList = bAdvanced
				? static_cast<UReciListBase*>(WidgetTree->ConstructWidget<UReciAdvancedList>(UReciAdvancedList::StaticClass(), TEXT("ChildList")))
				: WidgetTree->ConstructWidget<UReciList>(UReciList::StaticClass(), TEXT("ChildList"));
			ChildList->ItemWidgetClass = UReciTwoLevelTestEntry::StaticClass();
			if (auto* Advanced = Cast<UReciAdvancedList>(ChildList)) Advanced->MainEntryWidgetClass = UReciTwoLevelTestEntry::StaticClass();
			if (auto* Plain = Cast<UReciList>(ChildList)) Plain->bLoopingEntries = bLoopingChild;
			Border->SetContent(MakeAuthoredLayout(WidgetTree, ChildList, ChildAxis, ViewportMode));
		}
	}
}

void UReciTwoLevelTestList::BuildAuthoredTree(EOrientation Axis, int32 Mode, bool bAdvanced)
{
	auto* Border = WidgetTree->ConstructWidget<UBorder>();
	Border->SetPadding(FMargin(13, 17));
	WidgetTree->RootWidget = Border;
	ParentList = bAdvanced
		? static_cast<UReciListBase*>(WidgetTree->ConstructWidget<UReciAdvancedList>(UReciAdvancedList::StaticClass(), TEXT("ParentList")))
		: WidgetTree->ConstructWidget<UReciList>(UReciList::StaticClass(), TEXT("ParentList"));
	ParentList->ItemWidgetClass = UReciTwoLevelTestRow::StaticClass();
	if (auto* Advanced = Cast<UReciAdvancedList>(ParentList)) Advanced->MainEntryWidgetClass = UReciTwoLevelTestRow::StaticClass();
	Border->SetContent(MakeAuthoredLayout(WidgetTree, ParentList, Axis, Mode));
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Components/Overlay.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/ScopeExit.h"
#include "Tests/ReciTestHarness.h"
#include "Tests/ReciTestWidgets.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/ReciTextListItem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciTwoLevelListTest,
	"ReciCommonUI.TwoLevelList.AuthoredComposition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciTwoLevelListTest::RunTest(const FString& Parameters)
{
	// Configuration errors are logged once per class: each run starts with none reported.
	FReciScopedReports Reports;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	auto* Provider = NewObject<UReciTwoLevelTestEntry>(World);
	UObject* P0 = NewObject<UReciTextListItemData>(Provider);
	UObject* P1 = NewObject<UReciTextListItemData>(Provider);
	UObject* Empty = NewObject<UReciTextListItemData>(Provider);
	Provider->Counts.Add(P0, 3);
	Provider->Counts.Add(P1, 2);
	FReciTwoLevelChildCount Count;
	Count.BindDynamic(Provider, &UReciTwoLevelTestEntry::CountChildren);
	for (EOrientation Axis : { Orient_Vertical, Orient_Horizontal })
	for (bool bAdvanced : { false, true })
	for (int32 ParentMode = 0; ParentMode < 3; ++ParentMode)
	for (int32 ChildMode = 0; ChildMode < 3; ++ChildMode)
	{
		UReciTwoLevelTestRow::ChildAxis = Axis == Orient_Vertical ? Orient_Horizontal : Orient_Vertical;
		UReciTwoLevelTestRow::ViewportMode = ChildMode;
		UReciTwoLevelTestRow::bAdvanced = bAdvanced;
		auto* List = CreateWidget<UReciTwoLevelTestList>(World);
		List->BuildAuthoredTree(Axis, ParentMode, bAdvanced);
		UWidget* Root = List->GetRootWidget();
		UReciListBase* Parent = List->ParentList;
		const auto SlateRoot = List->TakeWidget();
		List->SetItems({ P0, P1, Empty, P0, nullptr }, Count);
		List->ForceLayoutPrepass();
		if (!TestTrue(TEXT("Authored hierarchy valid"), List->IsConfigurationValid())) continue;
		TestEqual(TEXT("Root retained"), List->GetRootWidget(), Root);
		TestEqual(TEXT("Authored list retained"), List->ParentList.Get(), Parent);
		TestEqual(TEXT("Direction read from box"), List->ParentOrientation, Axis);
		TestEqual(TEXT("Item spacing retained"), List->ParentLayout->ItemSpacing, 19.0f);
		TestEqual(TEXT("Padding retained"), Cast<UBorder>(Root)->GetPadding(), FMargin(13, 17));
		TestEqual(TEXT("Parents filtered"), List->Items.Num(), 3);
		if (bAdvanced)
		{
			const float RowHeight = List->ParentList->GetEntryPresentations(0)[0]->GetDesiredSize().Y;
			TestTrue(TEXT("Advanced parent remeasured after children populated"), List->ParentList->GetHostedEntryWidget(0)->GetDesiredSize().Y >= RowHeight);
		}
		TestEqual(TEXT("Parent viewport detected"), List->ParentViewport != nullptr, ParentMode != 0);
		if (List->ParentViewport) TestEqual(TEXT("Scroll settings retained"), List->ParentViewport->WheelScrollAmount, 83.0f);
		for (UReciTwoLevelListBinding* Binding : List->ChildBindings)
		{
			TestEqual(TEXT("Child spacing retained"), Binding->Layout->ItemSpacing, 19.0f);
			TestEqual(TEXT("Child viewport detected"), Binding->Viewport != nullptr, ChildMode != 0);
			for (int32 Index = 0; Index < Binding->List->GetHostedItemCount(); ++Index)
			for (UUserWidget* Widget : Binding->List->GetEntryPresentations(Index))
			{
				auto* Entry = CastChecked<UReciTwoLevelTestEntry>(Widget);
				TestEqual(TEXT("Child receives parent"), Entry->Data.Get(), List->Items[Binding->ParentIndex].Get());
				TestEqual(TEXT("Child receives index"), Entry->ChildIndex, Index);
				TestFalse(TEXT("Child nonfocusable"), Entry->GetIsFocusable());
			}
		}
		const auto CrossNext = Axis == Orient_Vertical ? EReciNavigationDirection::Right : EReciNavigationDirection::Down;
		const auto ParentNext = Axis == Orient_Vertical ? EReciNavigationDirection::Down : EReciNavigationDirection::Right;
		TestTrue(TEXT("Cross axis navigates"), List->Navigate(CrossNext));
		TestEqual(TEXT("Child selected"), List->GetSelectedChildIndex(), 1);
		TestTrue(TEXT("Parent axis navigates"), List->Navigate(ParentNext));
		TestEqual(TEXT("Parent changed"), List->GetFocusedParent(), P1);
		// A key or stick direction is consumed only when the list moves; at an end its box does not wrap it goes on to
		// Slate navigation, as for any Reci list.
		const EUINavigation ParentPrevious = Axis == Orient_Vertical ? EUINavigation::Up : EUINavigation::Left;
		const EUINavigation CrossPrevious = Axis == Orient_Vertical ? EUINavigation::Left : EUINavigation::Up;
		TestTrue(TEXT("A direction the list moves in is consumed"), List->HandleReciNavigation(ParentPrevious));
		TestEqual(TEXT("Parent moved back"), List->GetFocusedParentIndex(), 0);
		TestFalse(TEXT("The parent axis's start is left to Slate"), List->HandleReciNavigation(ParentPrevious));
		TestEqual(TEXT("Parent stays at its start"), List->GetFocusedParentIndex(), 0);
		TestTrue(TEXT("The child axis steps back"), List->HandleReciNavigation(CrossPrevious));
		TestEqual(TEXT("First child selected"), List->GetSelectedChildIndex(), 0);
		TestFalse(TEXT("The child axis's start is left to Slate"), List->HandleReciNavigation(CrossPrevious));
		TestEqual(TEXT("Child stays at its start"), List->GetSelectedChildIndex(), 0);
		List->ParentLayout->bWrapNavigation = true;
		TestTrue(TEXT("A wrapping axis consumes its end"), List->HandleReciNavigation(ParentPrevious));
		TestEqual(TEXT("Parent wrapped to the last row"), List->GetFocusedParentIndex(), 2);
		List->ParentLayout->bWrapNavigation = false;
		List->SetFocusedParentIndex(1, false);
		List->SelectChild(0, 2, false);
		List->SetSynchronizeChildSelection(true);
		TestEqual(TEXT("Short row clamps"), List->SelectedChildren[1], 1);
		TestEqual(TEXT("Empty row"), List->SelectedChildren[2], INDEX_NONE);
		List->SetFocusedParentIndex(1, false);
		List->GetChildList(0)->SetDrivenItemIndex(0, false);
		TestEqual(TEXT("Inactive inertia cannot change shared selection"), List->SelectedChildren[0], 2);
		TestEqual(TEXT("Inactive driver restored"), List->GetChildList(0)->GetDrivenItemIndex(), 2);
		TestTrue(TEXT("Restored child stays selected"), List->GetChildList(0)->GetHostedEntryButton(2)->GetSelected());
		List->SelectChild(0, 2, false);
		List->OnConfirmed.AddDynamic(Provider, &UReciTwoLevelTestEntry::RecordPair);
		const int32 Before = Provider->PairEvents;
		List->GetChildList(1)->HandleHostedItemClick(0);
		TestEqual(TEXT("Click focuses row"), List->GetFocusedParent(), P1);
		TestEqual(TEXT("Click synchronizes"), List->SelectedChildren[0], 0);
		TestEqual(TEXT("Click does not confirm"), Provider->PairEvents, Before);
		TestTrue(TEXT("Explicit confirm"), List->ConfirmSelection());
		TestEqual(TEXT("One confirm"), Provider->PairEvents, Before + 1);
		List->PointerIndex = 0;
		List->PressedParent = 1;
		List->PressPosition = FVector2D::ZeroVector;
		const FVector2D Delta = Axis == Orient_Vertical ? FVector2D(-50, 0) : FVector2D(0, -50);
		const FPointerEvent Swipe(0, Delta, FVector2D::ZeroVector, TSet<FKey>{ EKeys::LeftMouseButton }, EKeys::LeftMouseButton, 0, FModifierKeysState());
		List->HandleReciPointerMove(FGeometry(), Swipe);
		if (ChildMode == 0) TestEqual(TEXT("Cross-axis swipe selects next child"), List->GetSelectedChildIndex(), 1);
		else TestEqual(TEXT("Cross-axis drag routed to authored child viewport"), List->GestureViewport.Get(), List->GetChildBinding(1)->Viewport.Get());
		List->CancelGesture();
		List->SetSynchronizeChildSelection(false);
		List->SetSelectedChildIndex(1);
		List->SetItems({ P1, P0 }, Count);
		TestEqual(TEXT("Reorder retains parent"), List->GetFocusedParent(), P1);
		TestEqual(TEXT("Reorder retains child"), List->GetSelectedChildIndex(), 1);
		const int32 StateBefore = CastChecked<UReciTwoLevelTestEntry>(List->GetChildList(0)->GetHostedEntryButton(0))->StateUpdates;
		List->ApplyState();
		TestEqual(TEXT("Unchanged state does not restart animations"), CastChecked<UReciTwoLevelTestEntry>(List->GetChildList(0)->GetHostedEntryButton(0))->StateUpdates, StateBefore);
		List->SetItems({}, Count);
		TestFalse(TEXT("Empty navigation"), List->Navigate(CrossNext));
		TestFalse(TEXT("Empty confirm"), List->ConfirmSelection());
		List->ReleaseSlateResources(true);
	}
	UReciTwoLevelTestRow::bAdvanced = false;
	UReciTwoLevelTestRow::ChildAxis = Orient_Vertical;
	UReciTwoLevelTestRow::ViewportMode = 0;
	auto* Invalid = CreateWidget<UReciTwoLevelTestList>(World);
	Invalid->BuildAuthoredTree(Orient_Vertical, 0, false);
	const auto InvalidSlate = Invalid->TakeWidget();
	AddExpectedError(TEXT("have the same direction"), EAutomationExpectedErrorFlags::Contains, 1);
	Invalid->SetItems({ P0 }, Count);
	TestFalse(TEXT("Same axes rejected"), Invalid->IsConfigurationValid());
	TestFalse(TEXT("Invalid navigation blocked"), Invalid->Navigate(EReciNavigationDirection::Down));
	TestFalse(TEXT("Invalid confirm blocked"), Invalid->ConfirmSelection());
	TestFalse(TEXT("Invalid navigation is left to Slate"), Invalid->HandleReciNavigation(EUINavigation::Down));
	Invalid->ReleaseSlateResources(true);
	UReciTwoLevelTestRow::ChildAxis = Orient_Horizontal;
	UReciTwoLevelTestRow::bOmitChild = true;
	auto* Missing = CreateWidget<UReciTwoLevelTestList>(World);
	Missing->BuildAuthoredTree(Orient_Vertical, 0, false);
	const auto MissingSlate = Missing->TakeWidget();
	AddExpectedError(TEXT("requires a ReciList named ChildList"), EAutomationExpectedErrorFlags::Contains, 1);
	Missing->SetItems({ P0 }, Count);
	TestFalse(TEXT("Missing child rejected"), Missing->IsConfigurationValid());
	Missing->ReleaseSlateResources(true);
	UReciTwoLevelTestRow::bOmitChild = false;
	World->DestroyWorld(false);

	// A child list in a carousel, laid out and painted: the selected child is centred, and its entry's presentation
	// blend is 1 while the blend falls off with distance from the centre.
	{
		FReciTestHarness Harness(*this);
		if (!Harness.IsValid())
		{
			return false;
		}
		UReciTwoLevelTestRow::ChildAxis = Orient_Horizontal;
		UReciTwoLevelTestRow::ViewportMode = 2;
		ON_SCOPE_EXIT
		{
			UReciTwoLevelTestRow::ViewportMode = 0;
		};
		UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
		Harness.ActivateScreen(*Screen);
		USizeBox* Frame = Screen->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Frame"));
		Frame->SetWidthOverride(300.0f);
		Frame->SetHeightOverride(200.0f);
		auto* Carousels = CreateWidget<UReciTwoLevelTestList>(Harness.GetPlayerController());
		Carousels->BuildAuthoredTree(Orient_Vertical, 0, false);
		Frame->SetContent(Carousels);
		Screen->GetContent()->AddChild(Frame);
		auto* Rows = NewObject<UReciTwoLevelTestEntry>(Harness.GetWorld());
		UObject* Row = NewObject<UReciTextListItemData>(Rows);
		Rows->Counts.Add(Row, 3);
		FReciTwoLevelChildCount RowCount;
		RowCount.BindDynamic(Rows, &UReciTwoLevelTestEntry::CountChildren);
		Carousels->SetItems({Row}, RowCount);
		Harness.Advance(0.5f);
		const UReciTwoLevelListBinding* Binding = Carousels->GetChildBinding(0);
		const UReciCarouselScrollBox* ChildCarousel = Binding ? Cast<UReciCarouselScrollBox>(Binding->Viewport.Get()) : nullptr;
		const auto Blend = [Carousels](const int32 Index)
		{
			const TArray<UUserWidget*> Entries = Carousels->GetChildList(0)->GetEntryPresentations(Index);
			const UReciTwoLevelTestEntry* Entry = Entries.IsEmpty() ? nullptr : Cast<UReciTwoLevelTestEntry>(Entries[0]);
			return Entry ? Entry->CarouselBlend : -1.0f;
		};
		if (TestNotNull(TEXT("The child list's viewport is a carousel"), ChildCarousel))
		{
			TestEqual(TEXT("The selected child is centred"), ChildCarousel->GetCenteredItemIndex(), 0);
			TestEqual(TEXT("The centred child's blend is 1"), Blend(0), 1.0f, 0.01f);
			TestTrue(TEXT("A child further from the centre has a lower blend"), Blend(2) >= 0.0f && Blend(2) < 0.5f);
			Carousels->SelectChild(0, 2, false);
			Harness.Advance(1.0f);
			TestEqual(TEXT("Selecting another child centres it"), ChildCarousel->GetCenteredItemIndex(), 2);
			TestEqual(TEXT("Its blend is 1"), Blend(2), 1.0f, 0.01f);
			TestTrue(TEXT("The child it left falls off"), Blend(0) >= 0.0f && Blend(0) < 0.5f);
		}
		Carousels->ReleaseSlateResources(true);
	}
	return true;
}

#if WITH_EDITOR
namespace
{
	/** A two-level list's authored tree: ParentList (rows of the test row class) in Layout, which Wrap holds. */
	UWidgetTree* MakeTwoLevelTree(UWidget*& OutWrap, UReciListBase*& OutParent, const TFunctionRef<UWidget*(UWidgetTree&)> MakeLayout)
	{
		UWidgetTree* Tree = NewObject<UWidgetTree>(GetTransientPackage());
		UBorder* Root = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Root"));
		OutParent = Tree->ConstructWidget<UReciList>(UReciList::StaticClass(), TEXT("ParentList"));
		OutParent->ItemWidgetClass = UReciTwoLevelTestRow::StaticClass();
		OutWrap = MakeLayout(*Tree);
		Root->SetContent(OutWrap);
		Tree->RootWidget = Root;
		return Tree;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciTwoLevelPlacementCompileChecksTest,
	"ReciCommonUI.TwoLevelList.PlacementCompileChecks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciTwoLevelPlacementCompileChecksTest::RunTest(const FString& Parameters)
{
	const UReciTwoLevelListBase* Defaults = GetDefault<UReciTwoLevelTestList>();
	auto ErrorsOf = [Defaults](const UWidgetTree& Tree)
	{
		FReciTestCompilerLog Log;
		Defaults->ValidateCompiledWidgetTree(Tree, Log);
		return Log.Errors.Num();
	};
	UWidget* Wrap = nullptr;
	UReciListBase* Parent = nullptr;

	// Valid: ParentList in a Reci Vertical Box that a Reci Scroll Box holds alone.
	const TStrongObjectPtr<UWidgetTree> Valid(MakeTwoLevelTree(Wrap, Parent, [&Parent](UWidgetTree& Tree) -> UWidget*
	{
		UReciScrollBox* Scroll = Tree.ConstructWidget<UReciScrollBox>(UReciScrollBox::StaticClass(), TEXT("Scroll"));
		UReciVerticalBox* Box = Tree.ConstructWidget<UReciVerticalBox>(UReciVerticalBox::StaticClass(), TEXT("Box"));
		Box->SetContent(Parent);
		Scroll->AddChild(Box);
		return Scroll;
	}));
	TestEqual(TEXT("A valid placement compiles cleanly"), ErrorsOf(*Valid), 0);

	// ParentList outside a Reci layout box.
	const TStrongObjectPtr<UWidgetTree> Outside(MakeTwoLevelTree(Wrap, Parent, [&Parent](UWidgetTree& Tree) -> UWidget* { return Parent; }));
	TestEqual(TEXT("ParentList outside a Reci layout box is one error"), ErrorsOf(*Outside), 1);

	// ParentList in a Reci Tile Box, which has no single axis.
	const TStrongObjectPtr<UWidgetTree> Tiled(MakeTwoLevelTree(Wrap, Parent, [&Parent](UWidgetTree& Tree) -> UWidget*
	{
		UReciTileBox* Tiles = Tree.ConstructWidget<UReciTileBox>(UReciTileBox::StaticClass(), TEXT("Tiles"));
		Tiles->SetContent(Parent);
		return Tiles;
	}));
	TestEqual(TEXT("ParentList in a Reci Tile Box is one error"), ErrorsOf(*Tiled), 1);

	// A Reci Scroll Box around the layout box that does not hold it alone keeps its own routing. The scroll box itself
	// logs that it needs a Reci layout box child as it is built.
	AddExpectedMessagePlain(TEXT("requires exactly one ReciListLayoutBox child"), ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 0);
	const TStrongObjectPtr<UWidgetTree> Wrapped(MakeTwoLevelTree(Wrap, Parent, [&Parent](UWidgetTree& Tree) -> UWidget*
	{
		UReciScrollBox* Scroll = Tree.ConstructWidget<UReciScrollBox>(UReciScrollBox::StaticClass(), TEXT("Scroll"));
		USizeBox* Size = Tree.ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Size"));
		UReciVerticalBox* Box = Tree.ConstructWidget<UReciVerticalBox>(UReciVerticalBox::StaticClass(), TEXT("Box"));
		Box->SetContent(Parent);
		Size->SetContent(Box);
		Scroll->AddChild(Size);
		return Scroll;
	}));
	TestEqual(TEXT("A Reci Scroll Box that does not hold the layout box alone is one error"), ErrorsOf(*Wrapped), 1);

	// A row class whose ChildList runs along ParentList's axis. A native row has no authored tree for the two-level
	// list's compile to read, so the check that reads a row Blueprint's tree is given the row's tree here.
	UReciTwoLevelTestRow::ChildAxis = Orient_Vertical;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT
	{
		UReciTwoLevelTestRow::ChildAxis = Orient_Horizontal;
		World->DestroyWorld(false);
	};
	UReciTwoLevelTestRow* Row = CreateWidget<UReciTwoLevelTestRow>(World);
	Row->TakeWidget();
	FReciTestCompilerLog SameAxisLog;
	UE::ReciCommonUIWidgets::Private::ValidateTwoLevelRowTree(*Parent, Orient_Vertical, *Row->GetClass(), *Row->WidgetTree, SameAxisLog);
	TestEqual(TEXT("A row whose ChildList shares ParentList's axis is one error"), SameAxisLog.Errors.Num(), 1);
	FReciTestCompilerLog CrossAxisLog;
	UE::ReciCommonUIWidgets::Private::ValidateTwoLevelRowTree(*Parent, Orient_Horizontal, *Row->GetClass(), *Row->WidgetTree, CrossAxisLog);
	TestEqual(TEXT("A row whose ChildList runs across ParentList compiles cleanly"), CrossAxisLog.Errors.Num(), 0);
	// The row's own compile checks its ChildList's placement.
	FReciTestCompilerLog RowLog;
	Row->ValidateCompiledWidgetTree(*Row->WidgetTree, RowLog);
	TestTrue(TEXT("A row with its ChildList in a Reci layout box compiles cleanly"), RowLog.Errors.IsEmpty() && RowLog.Warnings.IsEmpty());
	return true;
}
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciTwoLevelOverridesReportedTest,
	"ReciCommonUI.TwoLevelList.OverridesReported", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciTwoLevelOverridesReportedTest::RunTest(const FString& Parameters)
{
	FReciScopedReports Reports;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT
	{
		UReciTwoLevelTestRow::bLoopingChild = false;
		UReciTwoLevelTestRow::ChildAxis = Orient_Vertical;
		World->DestroyWorld(false);
	};
	auto* Provider = NewObject<UReciTwoLevelTestEntry>(World);
	UObject* P0 = NewObject<UReciTextListItemData>(Provider);
	UObject* P1 = NewObject<UReciTextListItemData>(Provider);
	Provider->Counts.Add(P0, 2);
	Provider->Counts.Add(P1, 2);
	FReciTwoLevelChildCount Count;
	Count.BindDynamic(Provider, &UReciTwoLevelTestEntry::CountChildren);
	UReciTwoLevelTestRow::ChildAxis = Orient_Horizontal;

	// Default lists under a default two-level list report nothing.
	auto* Plain = CreateWidget<UReciTwoLevelTestList>(World);
	Plain->BuildAuthoredTree(Orient_Vertical, 0, false);
	TestTrue(TEXT("A default ParentList drops nothing it was given"),
		UE::ReciCommonUIWidgets::Private::CollectDroppedListSettings(*Plain->ParentList, true, Plain->bAllowPointerHover).IsEmpty());

	// ParentList in Multi selection, and rows whose ChildList loops: one report each, however many rows bind.
	UReciTwoLevelTestRow::bLoopingChild = true;
	auto* List = CreateWidget<UReciTwoLevelTestList>(World);
	List->BuildAuthoredTree(Orient_Vertical, 0, false);
	List->ParentList->SetSelectionMode(ESelectionMode::Multi);
	// Constructing the list binds it (with no items yet), and setting the items binds it again.
	AddExpectedMessagePlain(TEXT("sets ParentList's Selection Mode to None while it binds the list, over the Multi"),
		ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	AddExpectedMessagePlain(TEXT("ignores ChildList's Looping Entries (on)"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 1);
	const auto Slate = List->TakeWidget();
	List->SetItems({P0, P1}, Count);
	TestTrue(TEXT("The authored hierarchy binds"), List->IsConfigurationValid());
	TestEqual(TEXT("ParentList's Selection Mode is the binding's while bound"), List->ParentList->SelectionMode.GetValue(), ESelectionMode::None);
	// Binding again reports nothing more.
	List->RefreshPresentation();
	UReciTwoLevelTestRow* Row = Cast<UReciTwoLevelTestRow>(List->ParentList->GetEntryPresentations(0)[0]);

	// Unbinding restores what the author set.
	static_cast<UWidget*>(List)->ReleaseSlateResources(true);
	TestEqual(TEXT("ParentList's Selection Mode is restored"), List->ParentList->SelectionMode.GetValue(), ESelectionMode::Multi);

#if WITH_EDITOR
	// The same two, from the Blueprint compiler's checks of the two-level list and of the row, on the authored values.
	FReciTestCompilerLog ListLog;
	List->ValidateCompiledWidgetTree(*List->WidgetTree, ListLog);
	TestEqual(TEXT("ParentList's Multi is one compile warning"), ListLog.Warnings.Num(), 1);
	TestTrue(TEXT("The two-level list's tree compiles without error"), ListLog.Errors.IsEmpty());
	if (TestNotNull(TEXT("A row was built"), Row))
	{
		FReciTestCompilerLog RowLog;
		Row->ValidateCompiledWidgetTree(*Row->WidgetTree, RowLog);
		TestEqual(TEXT("ChildList's Looping Entries is one compile warning"), RowLog.Warnings.Num(), 1);
	}
#endif
	return true;
}

// Set Items and Refresh Items rebuild ParentList, and the two-level list returns focus that was in it to the focused
// parent's row; ParentList leaves that to it. Without the focused row, Set Items moves focus to the first row, which is
// then the focused parent, and to no other row on the way. Refresh Items focuses the focused parent's new row once.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciTwoLevelRebuildFocusTest,
	"ReciCommonUI.TwoLevelList.RebuildFocus", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciTwoLevelRebuildFocusTest::RunTest(const FString& Parameters)
{
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	const EOrientation SavedChildAxis = UReciTwoLevelTestRow::ChildAxis;
	const int32 SavedViewportMode = UReciTwoLevelTestRow::ViewportMode;
	UReciTwoLevelTestRow::ChildAxis = Orient_Horizontal;
	UReciTwoLevelTestRow::ViewportMode = 0;
	ON_SCOPE_EXIT
	{
		UReciTwoLevelTestRow::ChildAxis = SavedChildAxis;
		UReciTwoLevelTestRow::ViewportMode = SavedViewportMode;
	};
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	Harness.ActivateScreen(*Screen);
	auto* List = CreateWidget<UReciTwoLevelTestList>(Harness.GetPlayerController());
	List->BuildAuthoredTree(Orient_Vertical, 0, false);
	Screen->GetContent()->AddChild(List);
	const TStrongObjectPtr<UReciTwoLevelTestEntry> Provider(NewObject<UReciTwoLevelTestEntry>());
	UObject* A = NewObject<UReciTextListItemData>(Provider.Get());
	UObject* B = NewObject<UReciTextListItemData>(Provider.Get());
	UObject* C = NewObject<UReciTextListItemData>(Provider.Get());
	for (UObject* Parent : {A, B, C})
	{
		Provider->Counts.Add(Parent, 2);
	}
	FReciTwoLevelChildCount Count;
	Count.BindDynamic(Provider.Get(), &UReciTwoLevelTestEntry::CountChildren);
	List->SetItems({A, B, C}, Count);
	Harness.Paint();
	UReciListBase* Rows = List->GetParentList();
	if (!TestTrue(TEXT("The two-level list binds"), List->IsConfigurationValid()) || !TestNotNull(TEXT("It has its rows"), Rows))
	{
		return false;
	}
	APlayerController* Player = Harness.GetPlayerController();
	const auto RowHasFocus = [Rows, Player](const int32 Index)
	{
		const UWidget* Row = Rows->GetHostedEntryButton(Index);
		return Row && (Row->HasUserFocus(Player) || Row->HasUserFocusedDescendants(Player));
	};
	// The rows ParentList reports focus moving to, and the Slate widgets the harness user's focus moves to.
	TArray<int32> FocusedRows;
	TArray<TWeakPtr<SWidget>> FocusedWidgets;
	const FDelegateHandle RowsHandle = Rows->OnFocusedIndexChangedNative().AddLambda([&FocusedRows](const int32 Index)
	{
		FocusedRows.Add(Index);
	});
	const FDelegateHandle SlateHandle = FSlateApplication::Get().OnFocusChanging().AddLambda(
		[&FocusedWidgets](const FFocusEvent& Event, const FWeakWidgetPath&, const TSharedPtr<SWidget>&, const FWidgetPath&,
			const TSharedPtr<SWidget>& NewFocused)
		{
			if (Event.GetUser() == static_cast<uint32>(FReciTestHarness::UserIndex))
			{
				FocusedWidgets.Add(NewFocused);
			}
		});
	ON_SCOPE_EXIT
	{
		Rows->OnFocusedIndexChangedNative().Remove(RowsHandle);
		FSlateApplication::Get().OnFocusChanging().Remove(SlateHandle);
	};

	// Focus on the last row; Set Items without its parent.
	TestTrue(TEXT("The last row takes focus"), List->SetFocusedParentIndex(2, true) && RowHasFocus(2));
	FocusedRows.Reset();
	List->SetItems({A, B}, Count);
	TestEqual(TEXT("Without the focused parent, the first parent is focused"), List->GetFocusedParentIndex(), 0);
	TestTrue(TEXT("and focus is on its row"), RowHasFocus(0));
	TestTrue(TEXT("ParentList reports focus on that row last"), !FocusedRows.IsEmpty() && FocusedRows.Last() == 0);
	TestFalse(TEXT("and on no other row on the way"), FocusedRows.Contains(1));

	// Focus on the second row; Refresh Items.
	TestTrue(TEXT("The second row takes focus"), List->SetFocusedParentIndex(1, true) && RowHasFocus(1));
	FocusedWidgets.Reset();
	List->RefreshItems();
	TestTrue(TEXT("Refresh Items leaves focus on the second row"), RowHasFocus(1));
	const UWidget* Row = Rows->GetHostedEntryButton(1);
	const TSharedPtr<SWidget> RowWidget = Row ? Row->GetCachedWidget() : nullptr;
	int32 RowFocusCount = 0;
	for (const TWeakPtr<SWidget>& Focused : FocusedWidgets)
	{
		RowFocusCount += RowWidget && Focused.Pin() == RowWidget ? 1 : 0;
	}
	TestEqual(TEXT("and focuses its new row once"), RowFocusCount, 1);
	return true;
}

// Focus leaves the two-level list's first row, and the game makes the third parent the focused one without focus (Set
// Focused Parent Index, bGiveFocus false). Focus that comes back reaches the focused parent's row and keeps that parent
// focused, when Slate moves focus to the two-level list, to ParentList, or to another row first; ParentList reports focus
// on that row alone. With ParentList a Reci List and a Reci Advanced List, whose main item is the focused parent.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciTwoLevelFocusReturnTest,
	"ReciCommonUI.TwoLevelList.FocusReturn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciTwoLevelFocusReturnTest::RunTest(const FString& Parameters)
{
	const EOrientation SavedChildAxis = UReciTwoLevelTestRow::ChildAxis;
	const int32 SavedViewportMode = UReciTwoLevelTestRow::ViewportMode;
	const bool bSavedTreeAtInitialize = UReciTwoLevelTestRow::bTreeAtInitialize;
	UReciTwoLevelTestRow::ChildAxis = Orient_Horizontal;
	UReciTwoLevelTestRow::ViewportMode = 0;
	// Rows as a Widget Blueprint makes them, whose focus reaches ParentList.
	UReciTwoLevelTestRow::bTreeAtInitialize = true;
	ON_SCOPE_EXIT
	{
		UReciTwoLevelTestRow::ChildAxis = SavedChildAxis;
		UReciTwoLevelTestRow::ViewportMode = SavedViewportMode;
		UReciTwoLevelTestRow::bTreeAtInitialize = bSavedTreeAtInitialize;
	};
	for (const bool bAdvanced : {false, true})
	{
		FReciTestHarness Harness(*this);
		if (!Harness.IsValid())
		{
			return false;
		}
		const FString Kind = bAdvanced ? TEXT("Advanced ParentList: ") : TEXT("ParentList: ");
		UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
		Harness.ActivateScreen(*Screen);
		UReciTestFocusTarget* Beside = Harness.CreateTarget(TEXT("Beside"), Screen);
		auto* List = CreateWidget<UReciTwoLevelTestList>(Harness.GetPlayerController());
		List->BuildAuthoredTree(Orient_Vertical, 0, bAdvanced);
		Screen->GetContent()->AddChild(List);
		const TStrongObjectPtr<UReciTwoLevelTestEntry> Provider(NewObject<UReciTwoLevelTestEntry>());
		UObject* A = NewObject<UReciTextListItemData>(Provider.Get());
		UObject* B = NewObject<UReciTextListItemData>(Provider.Get());
		UObject* C = NewObject<UReciTextListItemData>(Provider.Get());
		for (UObject* Parent : {A, B, C})
		{
			Provider->Counts.Add(Parent, 2);
		}
		FReciTwoLevelChildCount Count;
		Count.BindDynamic(Provider.Get(), &UReciTwoLevelTestEntry::CountChildren);
		List->SetItems({A, B, C}, Count);
		Harness.Paint();
		UReciListBase* Rows = List->GetParentList();
		if (!TestTrue(Kind + TEXT("The two-level list binds"), List->IsConfigurationValid()) || !TestNotNull(Kind + TEXT("It has its rows"), Rows))
		{
			return false;
		}
		APlayerController* Player = Harness.GetPlayerController();
		const auto RowHasFocus = [Rows, Player](const int32 Index)
		{
			const UWidget* Row = Rows->GetHostedEntryButton(Index);
			return Row && (Row->HasUserFocus(Player) || Row->HasUserFocusedDescendants(Player));
		};
		// The rows ParentList reports focus moving to.
		TArray<int32> FocusedRows;
		const FDelegateHandle RowsHandle = Rows->OnFocusedIndexChangedNative().AddLambda([&FocusedRows](const int32 Index)
		{
			if (Index != INDEX_NONE)
			{
				FocusedRows.Add(Index);
			}
		});
		ON_SCOPE_EXIT
		{
			Rows->OnFocusedIndexChangedNative().Remove(RowsHandle);
		};
		const auto OnlyRowFocused = [&FocusedRows](const int32 Index)
		{
			const bool bOnly = FocusedRows.Num() == 1 && FocusedRows[0] == Index;
			FocusedRows.Reset();
			return bOnly;
		};

		// Focus moves to the two-level list.
		TestTrue(Kind + TEXT("The first row takes focus"), List->SetFocusedParentIndex(0, true) && RowHasFocus(0));
		TestTrue(Kind + TEXT("Focus moves beside the list"), Harness.Focus(*Beside));
		TestTrue(Kind + TEXT("The game makes the third parent focused without focus"), List->SetFocusedParentIndex(2, false));
		TestTrue(Kind + TEXT("and focus stays beside the list"), Beside->HasUserFocus(Player));
		FocusedRows.Reset();
		TestTrue(Kind + TEXT("Focus comes back to the two-level list"), Harness.Focus(*List));
		TestEqual(Kind + TEXT("The third parent stays focused"), List->GetFocusedParentIndex(), 2);
		TestTrue(Kind + TEXT("and its row has focus"), RowHasFocus(2));
		TestTrue(Kind + TEXT("ParentList reports focus on that row alone"), OnlyRowFocused(2));

		// Focus moves to ParentList.
		Harness.Focus(*Beside);
		List->SetFocusedParentIndex(1, false);
		FocusedRows.Reset();
		Harness.Focus(*Rows);
		TestTrue(Kind + TEXT("Focus that moves to ParentList reaches the focused second parent's row"),
			List->GetFocusedParentIndex() == 1 && RowHasFocus(1));
		TestTrue(Kind + TEXT("ParentList reports focus on that row alone"), OnlyRowFocused(1));

		// Focus lands on another row first.
		Harness.Focus(*Beside);
		List->SetFocusedParentIndex(0, false);
		FocusedRows.Reset();
		Harness.Focus(*Rows->GetHostedEntryButton(2));
		TestTrue(Kind + TEXT("Focus landing on the third row moves on to the focused first parent's row"),
			List->GetFocusedParentIndex() == 0 && RowHasFocus(0));
		TestTrue(Kind + TEXT("ParentList reports focus on that row alone"), OnlyRowFocused(0));
	}
	return true;
}
#endif
