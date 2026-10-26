// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Input/ReciScreenInput.h"
#include "Input/ReciScreenInputReports.h"
#include "Interaction/ReciInputFocusCoordinator.h"
#include "Interaction/ReciRoutingEnvironment.h"

void UReciScreenInputLibrary::RegisterScreenInput(UCommonActivatableWidget* Screen)
{
	FReciInputFocusCoordinator::Get().RegisterScreenInput(Screen);
}

void UReciScreenInputLibrary::UnregisterScreenInput(UCommonActivatableWidget* Screen)
{
	FReciInputFocusCoordinator::Get().UnregisterScreenInput(Screen);
}

FReciScreenReachability UReciScreenInputLibrary::CheckScreenReachability(const UCommonActivatableWidget* Screen)
{
	return Screen
		? UE::ReciCommonUI::Private::CheckScreenReachability(*Screen, FReciStockRoutingEnvironment::Get())
		: FReciScreenReachability();
}
