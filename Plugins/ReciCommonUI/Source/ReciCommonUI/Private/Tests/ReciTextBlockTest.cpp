// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Widgets/ReciTextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FReciTextBlockMaxLinesTest,
	"ReciCommonUI.Text.MaxLines",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FReciTextBlockMaxLinesTest::RunTest(const FString& Parameters)
{
	UReciTextBlock* Text = NewObject<UReciTextBlock>(GetTransientPackage());
	Text->SetAutoWrapText(true);
	Text->SetTextOverflowPolicy(ETextOverflowPolicy::Clip);

	Text->SetMaxLines(1);
	TestFalse(TEXT("One line never wraps"), Text->GetAutoWrapText());
	TestEqual(TEXT("One line ends in an ellipsis"), Text->GetTextOverflowPolicy(), ETextOverflowPolicy::Ellipsis);

	Text->SetMaxLines(2);
	TestTrue(TEXT("More lines wrap"), Text->GetAutoWrapText());
	TestEqual(TEXT("More lines end the last shown line in an ellipsis"), Text->GetTextOverflowPolicy(), ETextOverflowPolicy::MultilineEllipsis);

	Text->SetMaxLines(-3);
	TestEqual(TEXT("Negative counts mean no limit"), Text->GetMaxLines(), 0);
	TestTrue(TEXT("No limit keeps the last wrapping"), Text->GetAutoWrapText());
	return true;
}

#endif
