// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Tests/ReciMotionTestScrollBox.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Motion/ReciMotionSubsystem.h"
#include "Settings/ReciCommonUISettings.h"
#include "Tests/ReciInteractionTestEntry.h"
#include "Tests/ReciTestHarness.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/ReciList.h"
#include "Widgets/ReciListLayoutBox.h"
#include "Widgets/ReciTextListItem.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciMotionScaleSettingTest, "ReciCommonUI.Motion.ScaleSetting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciMotionScaleSettingTest::RunTest(const FString& Parameters)
{
	// The settings and the warn-once reports go back when the test ends.
	UReciCommonUISettings* Settings = GetMutableDefault<UReciCommonUISettings>();
	const FReciScopedObjectState SettingsState(*Settings);
	const FReciScopedReports Reports;
	Settings->DefaultMotionScale = 0.25f;

	// A widget with no owning player follows the project default.
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	const UReciScrollBox* Scroll = NewObject<UReciScrollBox>(World);
	TestEqual(
		TEXT("A widget with no local player follows Default Motion Scale"),
		UReciMotionSubsystem::GetMotionScaleForWidget(Scroll),
		0.25f);
	TestEqual(TEXT("No widget follows Default Motion Scale"), UReciMotionSubsystem::GetMotionScaleForWidget(nullptr), 0.25f);

	// A player's own value, on a local player that is never added (which would start every player subsystem).
	const TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine));
	UReciMotionSubsystem* Subsystem = NewObject<UReciMotionSubsystem>(Player.Get());
	TestEqual(TEXT("A player who set nothing follows Default Motion Scale"), Subsystem->GetMotionScale(), 0.25f);
	TestTrue(TEXT("A player may set 0.5"), Subsystem->SetMotionScale(0.5f));
	TestEqual(TEXT("The player's value is read back"), Subsystem->GetMotionScale(), 0.5f);
	TestTrue(TEXT("A player may set 0"), Subsystem->SetMotionScale(0.0f));
	TestTrue(TEXT("A player may set 1"), Subsystem->SetMotionScale(1.0f));
	TestTrue(TEXT("A player may set 0.5 again"), Subsystem->SetMotionScale(0.5f));

	// Values outside 0 to 1 are rejected and reported once.
	AddExpectedMessagePlain(TEXT("Set Motion Scale was called with"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 1);
	TestFalse(TEXT("NaN is rejected"), Subsystem->SetMotionScale(std::numeric_limits<float>::quiet_NaN()));
	TestFalse(TEXT("-0.1 is rejected"), Subsystem->SetMotionScale(-0.1f));
	TestFalse(TEXT("1.5 is rejected"), Subsystem->SetMotionScale(1.5f));
	TestEqual(TEXT("A rejected value leaves the player's"), Subsystem->GetMotionScale(), 0.5f);

	// A project default that is not finite counts as 1, reported once.
	Settings->DefaultMotionScale = std::numeric_limits<float>::quiet_NaN();
	AddExpectedMessagePlain(TEXT("Default Motion Scale is"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 1);
	TestEqual(TEXT("A Default Motion Scale that is not finite counts as 1"), UReciMotionSubsystem::GetMotionScaleForWidget(Scroll), 1.0f);
	TestEqual(TEXT("It is reported once"), UReciMotionSubsystem::GetMotionScaleForWidget(Scroll), 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciScrollBoxMotionScaleTest, "ReciCommonUI.ScrollBox.MotionScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciScrollBoxMotionScaleTest::RunTest(const FString& Parameters)
{
	// The scroll box registers with the input coordinator while it has a widget: that state goes back afterwards.
	const FReciScopedCoordinatorState CoordinatorState;
	UReciCommonUISettings* Settings = GetMutableDefault<UReciCommonUISettings>();
	const FReciScopedObjectState SettingsState(*Settings);

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	const TArray<UObject*> Items = {
		NewObject<UReciTextListItemData>(World),
		NewObject<UReciTextListItemData>(World),
		NewObject<UReciTextListItemData>(World)};
	// Three 60x40 entries in a Reci Vertical Box: 120 units to scroll through.
	TArray<TSharedRef<SWidget>> SlateWidgets;
	const auto MakeScrollBox = [World, &Items, &SlateWidgets]()
	{
		UReciMotionTestScrollBox* Scroll = NewObject<UReciMotionTestScrollBox>(World);
		UReciVerticalBox* Box = NewObject<UReciVerticalBox>(World);
		UReciList* List = CreateWidget<UReciList>(World);
		List->ItemWidgetClass = UReciInteractionTestEntry::StaticClass();
		Box->SetContent(List);
		Scroll->AddChild(Box);
		Scroll->WheelScrollAmount = 32.0f;
		SlateWidgets.Add(Scroll->TakeWidget());
		List->SetListItems(Items);
		Scroll->ForceLayoutPrepass();
		return Scroll;
	};

	Settings->DefaultMotionScale = 0.0f;
	UReciMotionTestScrollBox* Off = MakeScrollBox();
	Off->RouteExternalMouseWheel(-1.0f, FVector2D::ZeroVector);
	TestEqual(TEXT("At motion scale 0 a wheel notch scrolls at once"), Off->GetScrollOffset(), 32.0f);
	TestFalse(TEXT("At motion scale 0 a wheel notch leaves no motion"), Off->IsRoutedMotionRunning());

	Settings->DefaultMotionScale = 1.0f;
	UReciMotionTestScrollBox* Full = MakeScrollBox();
	Full->RouteExternalMouseWheel(-1.0f, FVector2D::ZeroVector);
	TestEqual(TEXT("At motion scale 1 a wheel notch moves only as the timer ticks"), Full->GetScrollOffset(), 0.0f);
	TestTrue(TEXT("At motion scale 1 a wheel notch starts Scroll Motion"), Full->IsRoutedMotionRunning());

	// A fling: the pointer moves 100 units up and is released at once.
	const auto Fling = [](UReciMotionTestScrollBox& Scroll)
	{
		Scroll.BeginExternalDrag(0, FVector2D::ZeroVector);
		Scroll.UpdateExternalDrag(0, FVector2D(0.0, -100.0));
		Scroll.EndExternalDrag(0);
	};
	Settings->DefaultMotionScale = 0.0f;
	UReciMotionTestScrollBox* NoGlide = MakeScrollBox();
	Fling(*NoGlide);
	TestEqual(TEXT("At motion scale 0 a drag still moves the content 1:1"), NoGlide->GetScrollOffset(), 100.0f);
	TestFalse(TEXT("At motion scale 0 a released drag does not glide"), NoGlide->IsRoutedMotionRunning());

	Settings->DefaultMotionScale = 1.0f;
	UReciMotionTestScrollBox* Glide = MakeScrollBox();
	Fling(*Glide);
	TestEqual(TEXT("At motion scale 1 a drag moves the content 1:1"), Glide->GetScrollOffset(), 100.0f);
	TestTrue(TEXT("At motion scale 1 a released drag glides"), Glide->IsRoutedMotionRunning());

	for (UReciMotionTestScrollBox* Scroll : {Off, Full, NoGlide, Glide})
	{
		Scroll->ReleaseSlateResources(true);
	}
	return true;
}

#endif
