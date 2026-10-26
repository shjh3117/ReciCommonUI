// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Input/ReciScreenInputReports.h"
#include "Interaction/ReciInputFocusCoordinator.h"
#include "Modules/ModuleManager.h"

/**
 * The input coordinator installs its preprocessors on the first registration, not at startup (principle 4). The report
 * on a screen that does not register for its screen input follows CommonUI's events from startup on, in development
 * builds.
 */
class FReciCommonUIModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
#if !UE_BUILD_SHIPPING
		UE::ReciCommonUI::Private::StartUnregisteredScreenReports();
#endif
	}

	virtual void ShutdownModule() override
	{
#if !UE_BUILD_SHIPPING
		UE::ReciCommonUI::Private::StopUnregisteredScreenReports();
#endif
		FReciInputFocusCoordinator::Get().Shutdown();
	}
};

IMPLEMENT_MODULE(FReciCommonUIModule, ReciCommonUI)
