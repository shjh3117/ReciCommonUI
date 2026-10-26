// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "ReciCommonUIBlueprintLibrary.generated.h"

class UCommonActivatableWidget;
class UInputAction;
class UWidget;

UCLASS()
class RECICOMMONUI_API UReciCommonUIBlueprintLibrary
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Whether Key is one of Action's keys for the player who owns SourceWidget: their bindings, in the mapping
	 * contexts applied now. A screen that captures raw keys uses it to recognize an action's keys without naming them.
	 */
	UFUNCTION(
		BlueprintPure,
		Category = "Reci Common UI|Input",
		meta = (DefaultToSelf = "SourceWidget"))
	static bool IsKeyMappedToAction(const UWidget* SourceWidget, const UInputAction* Action, FKey Key);

	/**
	 * For an activatable widget's Get Desired Focus Target, which CommonUI also asks when the input method changes (a
	 * gamepad press after the keyboard, a mouse move after the gamepad): None while a visible, enabled control of Screen
	 * itself (not of an activatable widget nested in it) holds its player's focus, so CommonUI leaves focus there, and
	 * Target otherwise. Leave Screen's Desired Focus Widget unset, since CommonUI focuses it whenever Get Desired Focus
	 * Target returns None; this reports it once if it is set. A graph that sets focus itself uses Target, not Get
	 * Desired Focus Target.
	 */
	UFUNCTION(
		BlueprintPure,
		Category = "Reci Common UI|Focus",
		meta = (DefaultToSelf = "Screen", DisplayName = "Keep Focus Or Target"))
	static UWidget* KeepFocusOrTarget(const UCommonActivatableWidget* Screen, UWidget* Target);
};
