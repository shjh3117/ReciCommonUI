// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "ReciCommonUIBlueprintLibrary.h"

#include "CommonActivatableWidget.h"
#include "Components/Widget.h"
#include "Engine/LocalPlayer.h"
#include "Focus/ReciFocusUtils.h"
#include "Framework/Application/SlateUser.h"
#include "InputAction.h"
#include "Interaction/ReciRoutingEnvironment.h"
#include "ReciReport.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ReciCommonUIBlueprintLibrary)

bool UReciCommonUIBlueprintLibrary::IsKeyMappedToAction(
	const UWidget* SourceWidget,
	const UInputAction* Action,
	const FKey Key)
{
	// Enhanced Input's answer, read as screen-input routing reads it.
	return SourceWidget && Action && Key.IsValid()
		&& UE::ReciCommonUI::Private::GetRoutingEnvironment().GetActionKeys(*SourceWidget, *Action).Contains(Key);
}

UWidget* UReciCommonUIBlueprintLibrary::KeepFocusOrTarget(const UCommonActivatableWidget* Screen, UWidget* Target)
{
	if (!Screen)
	{
		return Target;
	}
	if (Screen->GetDesiredFocusWidget())
	{
		RECI_WARN_ONCE(TEXT("KeepFocusOrTarget"), Screen->GetClass()->GetPathName(),
			TEXT("%s sets Desired Focus Widget, which CommonUI focuses whenever Keep Focus Or Target returns None, so focus "
				"does not stay where it is. Leave Desired Focus Widget unset."),
			*Screen->GetClass()->GetName());
	}
	const ULocalPlayer* LocalPlayer = Screen->GetOwningLocalPlayer();
	const TSharedPtr<const FSlateUser> User = LocalPlayer ? LocalPlayer->GetSlateUser() : nullptr;
	return User && UE::ReciCommonUI::Private::HoldsKeepableFocus(*Screen, User->GetFocusedWidget()) ? nullptr : Target;
}
