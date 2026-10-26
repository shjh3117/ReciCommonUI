// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Focus/ReciFocusUtils.h"

#include "CommonActivatableWidget.h"
#include "Input/CommonUIActionRouterBase.h"
#include "Widgets/SWidget.h"

namespace UE::ReciCommonUI::Private
{
	bool HoldsKeepableFocus(const UCommonActivatableWidget& Screen, const TSharedPtr<SWidget>& Focused)
	{
		const TSharedPtr<SWidget> ScreenWidget = Screen.GetCachedWidget();
		if (!Focused || !ScreenWidget || Focused == ScreenWidget)
		{
			return false;
		}
		// The activatable tree CommonUI builds from Slate parents: focus inside a nested activatable widget is that
		// widget's, whether it is open or closing.
		if (UCommonUIActionRouterBase::FindActivatable(Focused, nullptr) != &Screen)
		{
			return false;
		}
		// A hidden, collapsed or disabled widget on the way up cannot hold focus that the player sees and acts on.
		for (TSharedPtr<SWidget> Widget = Focused; Widget; Widget = Widget->GetParentWidget())
		{
			if (!Widget->GetVisibility().IsVisible() || !Widget->IsEnabled())
			{
				return false;
			}
			if (Widget == ScreenWidget)
			{
				return true;
			}
		}
		return false;
	}
}
