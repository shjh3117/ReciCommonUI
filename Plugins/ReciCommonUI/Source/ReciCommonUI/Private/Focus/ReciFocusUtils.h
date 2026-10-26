// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"

class SWidget;
class UCommonActivatableWidget;

namespace UE::ReciCommonUI::Private
{
	/**
	 * Whether Focused is focus that Screen's Get Desired Focus Target should keep: a widget inside Screen, not Screen's
	 * own widget, whose nearest activatable widget is Screen (focus left in a nested or closing activatable widget does
	 * not count), and visible and enabled with every widget from it up to Screen's.
	 */
	bool HoldsKeepableFocus(const UCommonActivatableWidget& Screen, const TSharedPtr<SWidget>& Focused);
}
