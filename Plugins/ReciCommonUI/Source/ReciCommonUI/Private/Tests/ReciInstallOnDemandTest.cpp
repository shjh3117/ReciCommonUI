// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/ReciTestHarness.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReciCoordinatorInstallOnDemandTest, "ReciCommonUI.Interaction.Coordinator.InstallOnDemand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FReciCoordinatorInstallOnDemandTest::RunTest(const FString& Parameters)
{
	using FAccess = FReciInputFocusCoordinatorTestAccess;
	// The harness sets aside whatever the editor or an earlier test registered, and puts it back, with the processors it
	// needs, when the test ends.
	FReciTestHarness Harness(*this);
	if (!Harness.IsValid())
	{
		return false;
	}
	FReciTestCallLog& Log = Harness.GetLog();
	UReciTestScreen* Screen = Harness.CreateScreen(TEXT("Screen"));
	UReciTestFocusTarget* List = Harness.CreateTarget(TEXT("List"), Screen);
	List->HandledDirections = {EUINavigation::Up, EUINavigation::Down};
	Harness.ActivateScreen(*Screen);
	Harness.Paint();

	// A registration installs both processors; they stay until the next Tick after the last one goes.
	TestEqual(TEXT("A registered control installs both processors"), FAccess::CountInstalledProcessors(), 2);
	List->Unregister();
	TestEqual(TEXT("Unregistering leaves them installed until the next Tick"), FAccess::CountInstalledProcessors(), 2);
	FAccess::Tick();
	TestEqual(TEXT("With nothing registered, the next Tick removes them"), FAccess::CountInstalledProcessors(), 0);
	FReciInputFocusCoordinator::Get().RegisterScreenInput(Screen);
	TestEqual(TEXT("A registered screen installs them again"), FAccess::CountInstalledProcessors(), 2);
	FReciInputFocusCoordinator::Get().UnregisterScreenInput(Screen);
	FAccess::Tick();
	TestEqual(TEXT("They go again with the screen"), FAccess::CountInstalledProcessors(), 0);

	// A key the list took keeps them installed after the list goes, so its release is still swallowed.
	List->Register();
	TestTrue(TEXT("Focus reaches the list"), Harness.Focus(*List));
	TestTrue(TEXT("The list takes Down"), Harness.KeyDown(EKeys::Down));
	List->Unregister();
	FAccess::Tick();
	TestEqual(TEXT("A held key Reci took keeps them installed"), FAccess::CountInstalledProcessors(), 2);
	TestTrue(TEXT("Its release is swallowed"), Harness.KeyUp(EKeys::Down));
	FAccess::Tick();
	TestEqual(TEXT("Once it is released, the next Tick removes them"), FAccess::CountInstalledProcessors(), 0);

	// So does a stick deflection the list took, until the stick returns: Slate must not take its later steps.
	List->Register();
	TestTrue(TEXT("The list takes the stick's first step"), Harness.AnalogInput(EKeys::Gamepad_LeftY, 1.0f));
	List->Unregister();
	FAccess::Tick();
	TestEqual(TEXT("A deflection a control took keeps them installed"), FAccess::CountInstalledProcessors(), 2);
	TestTrue(TEXT("Its later values are still consumed"), Harness.AnalogInput(EKeys::Gamepad_LeftY, 1.0f));
	TestFalse(TEXT("The stick's return is not"), Harness.AnalogInput(EKeys::Gamepad_LeftY, 0.0f));
	FAccess::Tick();
	TestEqual(TEXT("Once the stick returns, the next Tick removes them"), FAccess::CountInstalledProcessors(), 0);
	TestEqual(TEXT("The list navigated once for the key and once for the stick"), Log.ToString(),
		FString(TEXT("List Navigate Down, List ReturnFocus, List Navigate Up, List ReturnFocus")));
	return true;
}

#endif
