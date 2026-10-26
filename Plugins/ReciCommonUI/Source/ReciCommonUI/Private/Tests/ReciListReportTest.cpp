// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tests/ReciInteractionTestEntry.h"
#include "Tests/ReciTestHarness.h"
#include "Tests/ReciTestWidgets.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/ReciList.h"
#include "Widgets/ReciListLayoutBox.h"
#include "Widgets/ReciListPanel.h"
#include "Widgets/ReciScrollBox.h"
#include "Widgets/ReciTextListItem.h"
#include "Widgets/ReciWidgetTreeUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciListHoldEntryReportedTest, "ReciCommonUI.List.HoldEntryReported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciListHoldEntryReportedTest::RunTest(const FString& Parameters)
{
	FReciScopedReports Reports;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	UReciInteractionTestEntry* Recorder = NewObject<UReciInteractionTestEntry>(World);
	UObject* A = NewObject<UReciTextListItemData>(World);
	UObject* B = NewObject<UReciTextListItemData>(World);
	UReciInteractionTestList* List = CreateWidget<UReciInteractionTestList>(World);
	List->ItemWidgetClass = UReciTestHoldEntry::StaticClass();
	List->OnItemConfirmed.AddDynamic(Recorder, &UReciInteractionTestEntry::RecordConfirm);
	const TSharedRef<SWidget> Slate = List->TakeWidget();

	// Reported once per entry class, however often the list rebuilds.
	AddExpectedMessagePlain(TEXT("ReciTestHoldEntry requires hold, but a Reci list confirms on Accept key-down"),
		ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	List->SetListItems({A, B});
	List->SetListItems({B, A});
	// The bypass stands, as documented: Accept confirms at once.
	List->SetDrivenItemIndex(1, false);
	TestTrue(TEXT("Accept confirms an entry that requires hold"), List->ConfirmDrivenItem());
	TestEqual(TEXT("One confirm"), Recorder->ConfirmCount, 1);
	TestTrue(TEXT("The driven item is confirmed"), Recorder->LastConfirmed == A);

#if WITH_EDITOR
	// The Blueprint compiler's check reads the entry class alone.
	FReciTestCompilerLog HoldLog;
	List->ValidateCompiledDefaults(HoldLog);
	TestEqual(TEXT("A hold entry class is a compile warning"), HoldLog.Warnings.Num(), 1);
	TestTrue(TEXT("The warning names the entry class"), HoldLog.Warnings.Num() == 1 && HoldLog.Warnings[0].Contains(TEXT("ReciTestHoldEntry requires hold")));
	List->ItemWidgetClass = UReciInteractionTestEntry::StaticClass();
	FReciTestCompilerLog PlainLog;
	List->ValidateCompiledDefaults(PlainLog);
	TestTrue(TEXT("A plain entry class compiles cleanly"), PlainLog.Warnings.IsEmpty() && PlainLog.Errors.IsEmpty());
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciListEntrySetupReportedTest, "ReciCommonUI.List.EntrySetupReported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciListEntrySetupReportedTest::RunTest(const FString& Parameters)
{
	FReciScopedReports Reports;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	UObject* A = NewObject<UReciTextListItemData>(World);
	UObject* B = NewObject<UReciTextListItemData>(World);

	// An entry class that is an activatable widget would become its controls' screen.
	UReciInteractionTestList* Activatable = CreateWidget<UReciInteractionTestList>(World);
	Activatable->ItemWidgetClass = UReciTestActivatableEntry::StaticClass();
	const TSharedRef<SWidget> ActivatableSlate = Activatable->TakeWidget();
	AddExpectedMessagePlain(TEXT("ReciTestActivatableEntry derives from Common Activatable Widget"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 1);
	Activatable->SetListItems({A});
	Activatable->SetListItems({A, B});
#if WITH_EDITOR
	FReciTestCompilerLog ActivatableLog;
	Activatable->ValidateCompiledDefaults(ActivatableLog);
	TestEqual(TEXT("An activatable entry class is a compile warning"), ActivatableLog.Warnings.Num(), 1);
#endif

	// Looping Entries in a Reci Tile Box does nothing, and says so once.
	UReciList* Tiled = CreateWidget<UReciList>(World);
	Tiled->ItemWidgetClass = UReciInteractionTestEntry::StaticClass();
	Tiled->bLoopingEntries = true;
	const TSharedRef<SWidget> TiledSlate = Tiled->TakeWidget();
	UReciTileBox* Tiles = NewObject<UReciTileBox>(World);
	Tiles->SetContent(Tiled);
	AddExpectedMessagePlain(TEXT("Looping Entries has no effect in a Reci Tile Box"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 1);
	Tiled->SetListItems({A, B});
	Tiled->SetListItems({B, A});
	TestEqual(TEXT("A tiled list makes no copies"), Tiled->GetEntryPresentations(0).Num(), 1);
#if WITH_EDITOR
	FReciTestCompilerLog TiledLog;
	Tiled->ValidateCompiledDefaults(TiledLog);
	TestEqual(TEXT("Looping Entries in a Reci Tile Box is a compile warning"), TiledLog.Warnings.Num(), 1);
#endif

	// Looping copies keep their entry class's Focus On Hover; the list's Focus On Pointer Hover gates it, as for any list.
	UReciList* Looping = CreateWidget<UReciList>(World);
	Looping->ItemWidgetClass = UReciInteractionTestEntry::StaticClass();
	Looping->bLoopingEntries = true;
	Looping->bFocusOnPointerHover = true;
	UReciHorizontalBox* LoopingRow = NewObject<UReciHorizontalBox>(World);
	LoopingRow->SetContent(Looping);
	NewObject<UReciTestLoopingScrollBox>(World)->AddChild(LoopingRow);
	const TSharedRef<SWidget> LoopingSlate = Looping->TakeWidget();
	Looping->SetListItems({A, B});
	const TArray<UUserWidget*> Copies = Looping->GetEntryPresentations(0);
	TestEqual(TEXT("A looping list shows three copies of an item"), Copies.Num(), 3);
	for (UUserWidget* Copy : Copies)
	{
		const UReciFocusStyleButtonBase* Button = Cast<UReciFocusStyleButtonBase>(Copy);
		TestTrue(TEXT("A looping copy keeps its own Focus On Hover"), Button && !Button->bFocusOnHover);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciListLoopingNeedsLoopingViewportTest, "ReciCommonUI.List.LoopingNeedsLoopingViewport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciListLoopingNeedsLoopingViewportTest::RunTest(const FString& Parameters)
{
	FReciScopedReports Reports;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	UObject* A = NewObject<UReciTextListItemData>(World);
	UObject* B = NewObject<UReciTextListItemData>(World);

	// Only a viewport that loops its entries moves the copies: in a plain Reci Scroll Box a looping list shows each item
	// once, and says so once.
	UReciList* Unhosted = CreateWidget<UReciList>(World);
	Unhosted->ItemWidgetClass = UReciInteractionTestEntry::StaticClass();
	Unhosted->bLoopingEntries = true;
	UReciHorizontalBox* Row = NewObject<UReciHorizontalBox>(World);
	Row->SetContent(Unhosted);
	NewObject<UReciScrollBox>(World)->AddChild(Row);
	const TSharedRef<SWidget> UnhostedSlate = Unhosted->TakeWidget();
	AddExpectedMessagePlain(TEXT("Looping Entries has no effect outside a scroll box that loops its entries, so the list shows each item once"),
		ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	Unhosted->SetListItems({A, B});
	Unhosted->SetListItems({B, A});
	TestEqual(TEXT("In a plain viewport the first item has one presentation"), Unhosted->GetEntryPresentations(0).Num(), 1);
	TestEqual(TEXT("In a plain viewport the second item has one presentation"), Unhosted->GetEntryPresentations(1).Num(), 1);
	TestTrue(TEXT("That presentation is the item's entry"),
		Unhosted->GetEntryPresentations(0).Num() == 1 && Unhosted->GetEntryPresentations(0)[0] == Unhosted->GetHostedEntryWidget(0));

	// In a viewport that loops its entries the same list makes its copies.
	UReciList* Hosted = CreateWidget<UReciList>(World);
	Hosted->ItemWidgetClass = UReciInteractionTestEntry::StaticClass();
	Hosted->bLoopingEntries = true;
	UReciHorizontalBox* LoopingRow = NewObject<UReciHorizontalBox>(World);
	LoopingRow->SetContent(Hosted);
	NewObject<UReciTestLoopingScrollBox>(World)->AddChild(LoopingRow);
	const TSharedRef<SWidget> HostedSlate = Hosted->TakeWidget();
	Hosted->SetListItems({A, B});
	TestEqual(TEXT("In a looping viewport an item has three presentations"), Hosted->GetEntryPresentations(0).Num(), 3);

#if WITH_EDITOR
	// The Blueprint compiler's check reads the placement; a Blueprint derived from Reci List leaves it to its places.
	FReciTestCompilerLog UnhostedLog;
	Unhosted->ValidateCompiledDefaults(UnhostedLog);
	TestTrue(TEXT("Looping Entries outside a looping viewport is a compile warning"), UnhostedLog.Warnings.Num() == 1
		&& UnhostedLog.Warnings[0].Contains(TEXT("no effect outside a scroll box that loops its entries")));
	FReciTestCompilerLog HostedLog;
	Hosted->ValidateCompiledDefaults(HostedLog);
	TestTrue(TEXT("Looping Entries in a looping viewport compiles cleanly"), HostedLog.Warnings.IsEmpty() && HostedLog.Errors.IsEmpty());
	UReciList* Defaults = GetMutableDefault<UReciList>();
	const FReciScopedObjectState DefaultsState(*Defaults);
	Defaults->bLoopingEntries = true;
	FReciTestCompilerLog DefaultsLog;
	Defaults->ValidateCompiledDefaults(DefaultsLog);
	TestTrue(TEXT("A list class's own Looping Entries compiles cleanly"), DefaultsLog.Warnings.IsEmpty());
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciListHostedLayoutReportedTest, "ReciCommonUI.List.HostedLayoutReported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciListHostedLayoutReportedTest::RunTest(const FString& Parameters)
{
	FReciScopedReports Reports;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	UObject* A = NewObject<UReciTextListItemData>(World);
	UObject* B = NewObject<UReciTextListItemData>(World);
	const auto Describe = [](const UReciListPanel& Panel)
	{
		const FReciListLayoutSettings Settings = Panel.GetLayoutSettings();
		return FString::Printf(TEXT("%s %.0f"),
			Settings.Mode == EReciListLayoutMode::Horizontal ? TEXT("Horizontal")
			: Settings.Mode == EReciListLayoutMode::Tile ? TEXT("Tile") : TEXT("Vertical"),
			Settings.ItemSpacing);
	};

	// A list that no Reci layout box hosts keeps the Layout Settings its ItemsBox was authored with.
	UReciTestLayoutList* Free = CreateWidget<UReciTestLayoutList>(World);
	Free->ItemWidgetClass = UReciInteractionTestEntry::StaticClass();
	const TSharedRef<SWidget> FreeSlate = Free->TakeWidget();
	Free->SetListItems({A, B});
	TestEqual(TEXT("A list outside a host keeps its ItemsBox's Layout Settings"), Describe(*Free->GetItemsBox()), FString(TEXT("Horizontal 7")));

	// A hosted list takes its box's layout, which owns it, and says once that its own Layout Settings went.
	UReciTestLayoutList* Hosted = CreateWidget<UReciTestLayoutList>(World);
	Hosted->ItemWidgetClass = UReciInteractionTestEntry::StaticClass();
	AddExpectedMessagePlain(TEXT("the Layout Settings set on its ItemsBox have no effect, since its Reci layout box"),
		ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	UReciVerticalBox* Column = NewObject<UReciVerticalBox>(World);
	Column->SetItemSpacing(3.0f);
	Column->SetContent(Hosted);
	const TSharedRef<SWidget> ColumnSlate = Column->TakeWidget();
	Hosted->SetListItems({A, B});
	Hosted->SetListItems({B, A});
	TestEqual(TEXT("The box's layout replaces the ItemsBox's"), Describe(*Hosted->GetItemsBox()), FString(TEXT("Vertical 3")));

	// A list whose panel has no Layout Settings of its own is hosted without a report.
	UReciList* Native = CreateWidget<UReciList>(World);
	Native->ItemWidgetClass = UReciInteractionTestEntry::StaticClass();
	UReciHorizontalBox* Row = NewObject<UReciHorizontalBox>(World);
	Row->SetContent(Native);
	const TSharedRef<SWidget> RowSlate = Row->TakeWidget();
	Native->SetListItems({A});

#if WITH_EDITOR
	// The Blueprint compiler's check reads the ItemsBox of the tree the list's class authors, which no host has touched.
	using UE::ReciCommonUI::Private::ValidateHostedItemsBox;
	const auto MakeTree = [](const FReciListLayoutSettings& Settings)
	{
		UWidgetTree* Tree = NewObject<UWidgetTree>(GetTransientPackage());
		UReciListPanel* Panel = Tree->ConstructWidget<UReciListPanel>(UReciListPanel::StaticClass(), TEXT("ItemsBox"));
		Panel->ApplyLayoutSettings(Settings);
		Tree->RootWidget = Panel;
		return TStrongObjectPtr<UWidgetTree>(Tree);
	};
	const TStrongObjectPtr<UWidgetTree> Authored = MakeTree(UReciTestLayoutList::GetAuthoredLayout());
	const TStrongObjectPtr<UWidgetTree> Plain = MakeTree(FReciListLayoutSettings());
	FReciTestCompilerLog HostedLog;
	ValidateHostedItemsBox(*Hosted, *Authored, HostedLog);
	TestTrue(TEXT("A hosted list whose ItemsBox sets Layout Settings is a compile warning"), HostedLog.Warnings.Num() == 1
		&& HostedLog.Warnings[0].Contains(TEXT("have no effect, since its Reci layout box")));
	FReciTestCompilerLog FreeLog;
	ValidateHostedItemsBox(*Free, *Authored, FreeLog);
	TestTrue(TEXT("The same ItemsBox outside a host compiles cleanly"), FreeLog.Warnings.IsEmpty() && FreeLog.Errors.IsEmpty());
	FReciTestCompilerLog PlainLog;
	ValidateHostedItemsBox(*Hosted, *Plain, PlainLog);
	TestTrue(TEXT("A hosted ItemsBox without Layout Settings of its own compiles cleanly"), PlainLog.Warnings.IsEmpty());
	FReciTestCompilerLog NativeLog;
	Native->ValidateCompiledDefaults(NativeLog);
	TestTrue(TEXT("A native list, which authors no tree, compiles cleanly"), NativeLog.Warnings.IsEmpty() && NativeLog.Errors.IsEmpty());
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciListItemsBoxBindingTest, "ReciCommonUI.List.ItemsBoxBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciListItemsBoxBindingTest::RunTest(const FString& Parameters)
{
	FReciScopedReports Reports;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };

	// A native list, as placed in a screen, makes its panel its root.
	UReciList* Native = CreateWidget<UReciList>(World);
	const TSharedRef<SWidget> NativeSlate = Native->TakeWidget();
	const UReciListPanel* Panel = Cast<UReciListPanel>(Native->GetRootWidget());
	TestTrue(TEXT("A native list's root is a Reci List Panel named ItemsBox"), Panel && Panel->GetFName() == TEXT("ItemsBox"));

	// A native subclass that builds its own tree without ItemsBox keeps that tree and is reported once.
	UReciTestRootedList* Rooted = CreateWidget<UReciTestRootedList>(World);
	AddExpectedMessagePlain(TEXT("ReciTestRootedList builds its own widget tree without setting ItemsBox"), ELogVerbosity::Error,
		EAutomationExpectedMessageFlags::Contains, 1);
	Rooted->TakeWidget();
	Rooted->ReleaseSlateResources(true);
	const TSharedRef<SWidget> RootedSlate = Rooted->TakeWidget();
	TestTrue(TEXT("The authored root stays"), Cast<USizeBox>(Rooted->GetRootWidget()) != nullptr);

#if WITH_METADATA
	// The Blueprint compiler requires the name in every Blueprint derived from Reci List.
	const FProperty* ItemsBox = UReciList::StaticClass()->FindPropertyByName(TEXT("ItemsBox"));
	TestTrue(TEXT("ItemsBox is a BindWidget property"), ItemsBox && ItemsBox->HasMetaData(TEXT("BindWidget")));
#endif
	return true;
}

#endif
