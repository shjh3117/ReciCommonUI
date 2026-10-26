// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Input/ReciScreenInput.h"

class IReciRoutingEnvironment;
class UCommonActivatableWidget;

namespace UE::ReciCommonUI::Private
{
	/** Check Screen Reachability, with the player's keys as the environment answers them. */
	FReciScreenReachability CheckScreenReachability(
		const UCommonActivatableWidget& Screen,
		const IReciRoutingEnvironment& Environment);

	/**
	 * At registration, reports what screen input cannot serve: each mapping or action of the screen's Input Mapping that
	 * has triggers or modifiers, once per mapping context and action, and what Check Screen Reachability finds, once per
	 * screen class. It runs in every build, and only at registration.
	 */
	void ReportScreenInputSetup(const UCommonActivatableWidget& Screen, const IReciRoutingEnvironment& Environment);

#if !UE_BUILD_SHIPPING
	/**
	 * Development builds: from module startup to shutdown, reports once per class a screen that implements screen input
	 * but has not registered by the tick after it activates. It follows CommonUI's rebuild and activation events, so it
	 * does no work on input.
	 */
	void StartUnregisteredScreenReports();
	void StopUnregisteredScreenReports();

#if WITH_DEV_AUTOMATION_TESTS
	/** Tests: runs now the checks the next tick would run. */
	void RunUnregisteredScreenChecksForTests();
	/** Tests: swaps the activated screens waiting for their check for Pending, and returns the previous ones. */
	TArray<TWeakObjectPtr<UCommonActivatableWidget>> ExchangePendingScreenChecksForTests(
		TArray<TWeakObjectPtr<UCommonActivatableWidget>> Pending);
#endif
#endif
}
