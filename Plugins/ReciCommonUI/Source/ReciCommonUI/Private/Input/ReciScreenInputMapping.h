// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class FNavigationConfig;
class UCommonActivatableWidget;
class UInputAction;
class UInputMappingContext;

namespace UE::ReciCommonUI::Private
{
	/** The navigation config Slate navigates a user's input with: the user's own, else the application's. */
	TSharedPtr<FNavigationConfig> ResolveNavigationConfig(uint32 UserIndex);

	/** The stick-direction keys the platform presses for an analog stick axis, toward its negative and positive ends. */
	bool GetStickDirectionKeys(const FKey& Axis, FKey& OutNegative, FKey& OutPositive);

	/** Whether the key is one of the platform's stick-direction keys (Gamepad Left Stick Up and so on). */
	bool IsStickDirectionKey(const FKey& Key);

	/**
	 * The stick (0 left, 1 right) and the axis (0 X, 1 Y) of an analog stick axis or of one of its stick-direction keys,
	 * and the end of the axis a stick-direction key stands for (-1 or 1; 0 for the axis itself); false for any other key.
	 */
	bool GetStickAxis(const FKey& Key, int32& OutStick, int32& OutAxis, int32& OutEnd);

	/**
	 * The screen's Input Mapping, the mapping context CommonUI applies while the screen is active; null when it has none.
	 * CommonUI keeps it protected with no getter. It is read through a class derived from UCommonActivatableWidget, so
	 * a rename or type change in CommonUI breaks the build instead of leaving every screen without actions.
	 */
	const UInputMappingContext* GetScreenInputMapping(const UCommonActivatableWidget& Screen);

	/** The actions of the screen's Input Mapping, each once, in mapping order: the actions screen input offers a key to. */
	TArray<UInputAction*> GetScreenActions(const UCommonActivatableWidget& Screen);
}
