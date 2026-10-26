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

namespace
{
	UObject* GetCarouselPageVariable(UUserWidget& Widget, const TCHAR* Name)
	{
		const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Widget.GetClass(), Name);
		return Property ? Property->GetObjectPropertyValue_InContainer(&Widget) : nullptr;
	}
}

// The demo's carousel page (Content/Demo/Pages/WBP_DemoCarouselPage), laid out and painted: Accept on the centred card
// confirms it, and the page's status line names it. The grid moved to its own page (ReciDemoGridPageTest.cpp).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciDemoCarouselPageTest, "ReciCommonUI.Demo.CarouselPage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciDemoCarouselPageTest::RunTest(const FString& Parameters)
{
	UClass* PageClass = LoadClass<UUserWidget>(nullptr, TEXT("/ReciCommonUI/Demo/Pages/WBP_DemoCarouselPage.WBP_DemoCarouselPage_C"));
	if (!TestNotNull(TEXT("The page's Blueprint class loads"), PageClass))
	{
		return false;
	}
	// A card shows the click action's key, which CommonUI's input subsystem loads when a local player starts; the test
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

	UReciListBase* Cards = Cast<UReciListBase>(GetCarouselPageVariable(*Page, TEXT("Cards")));
	const UTextBlock* Confirmed = Cast<UTextBlock>(GetCarouselPageVariable(*Page, TEXT("ConfirmedText")));
	if (!TestNotNull(TEXT("The page holds its carousel"), Cards) || !TestNotNull(TEXT("and its confirmed text"), Confirmed))
	{
		return false;
	}
	TestNull(TEXT("The grid is no longer on this page"), GetCarouselPageVariable(*Page, TEXT("Grid")));

	TestTrue(TEXT("The carousel takes focus"), Cards->SetFocusedItemIndex(Cards->GetDrivenItemIndex()));
	Harness.KeyDown(EKeys::Enter);
	Harness.KeyUp(EKeys::Enter);
	TestTrue(TEXT("Accept on the centred card confirms it"), Confirmed->GetText().ToString().StartsWith(TEXT("Confirmed: Card")));

	Page->ReleaseSlateResources(true);
	return true;
}

#endif
