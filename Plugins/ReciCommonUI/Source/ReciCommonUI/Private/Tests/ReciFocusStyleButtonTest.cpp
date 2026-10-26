// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#if WITH_DEV_AUTOMATION_TESTS

#include "CommonButtonBase.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/Spacer.h"
#include "Misc/AutomationTest.h"
#include "Tests/ReciTestWidgets.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciFocusStyleButtonDisplayScaleWrapperTest, "ReciCommonUI.FocusStyleButton.DisplayScaleWrapper",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciFocusStyleButtonDisplayScaleWrapperTest::RunTest(const FString& Parameters)
{
	// Max Display Scale above 1 at initialisation inserts the documented wrapper above CommonUI's internal button.
	const TStrongObjectPtr<UReciTestButton> Scaled(NewObject<UReciTestButton>(GetTransientPackage()));
	Scaled->MaxDisplayScale = 1.25f;
	Scaled->InitializeForTest();
	const UHorizontalBox* Row = Cast<UHorizontalBox>(Scaled->GetRootWidget());
	if (!TestTrue(TEXT("The root is a Horizontal Box named DisplayScaleRow"), Row && Row->GetFName() == TEXT("DisplayScaleRow")))
	{
		return false;
	}
	if (!TestEqual(TEXT("DisplayScaleRow holds three widgets"), Row->GetChildrenCount(), 3))
	{
		return false;
	}
	UScaleBox* ScaleBox = Cast<UScaleBox>(Row->GetChildAt(1));
	TestTrue(TEXT("A Spacer leads"), Row->GetChildAt(0)->IsA<USpacer>());
	TestTrue(TEXT("The middle is a Scale Box named DisplayScaleBox"), ScaleBox && ScaleBox->GetFName() == TEXT("DisplayScaleBox"));
	TestTrue(TEXT("A Spacer trails"), Row->GetChildAt(2)->IsA<USpacer>());
	if (!ScaleBox)
	{
		return false;
	}
	TestEqual(TEXT("The Scale Box stretches as the user specifies"), ScaleBox->GetStretch(), EStretch::UserSpecified);
	TestTrue(TEXT("The Scale Box holds CommonUI's internal button"), ScaleBox->GetChildAt(0) && ScaleBox->GetChildAt(0)->IsA<UCommonButtonInternalBase>());

	auto Shares = [Row]()
	{
		TArray<float> Values;
		for (int32 Index = 0; Index < Row->GetChildrenCount(); ++Index)
		{
			const UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(Row->GetChildAt(Index)->Slot);
			Values.Add(Slot ? Slot->GetSize().Value : -1.0f);
		}
		return Values;
	};
	Scaled->SetDisplayScale(1.25f);
	TestTrue(TEXT("At the largest scale the button fills the row"), Shares() == TArray<float>{0.0f, 1.25f, 0.0f});
	TestEqual(TEXT("The Scale Box scales by the display scale"), ScaleBox->GetUserSpecifiedScale(), 1.25f);
	Scaled->SetDisplayScale(1.0f);
	TestTrue(TEXT("At scale 1 the rest is split around the button"), Shares() == TArray<float>{0.125f, 1.0f, 0.125f});
	Scaled->SetDisplayScale(0.5f);
	TestEqual(TEXT("A scale below 1 is clamped to 1"), Scaled->GetDisplayScale(), 1.0f);
	Scaled->SetDisplayScale(2.0f);
	TestEqual(TEXT("A scale above the maximum is clamped to it"), Scaled->GetDisplayScale(), 1.25f);

	// At 1 there is no wrapper: the root stays CommonUI's internal button.
	const TStrongObjectPtr<UReciTestButton> Plain(NewObject<UReciTestButton>(GetTransientPackage()));
	Plain->InitializeForTest();
	TestTrue(TEXT("Without display scaling the root is CommonUI's internal button"),
		Plain->GetRootWidget() && Plain->GetRootWidget()->IsA<UCommonButtonInternalBase>());
	return true;
}

#endif
