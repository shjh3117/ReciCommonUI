// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/NavigationConfig.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"

/**
 * A local player's Slate navigation config while the navigation subsystem is installed. The config it replaced (the
 * fallback) decides every key and stick, with the changes the project's settings ask for: keys mapped to the
 * navigation actions and to CommonUI's click and Back actions also give their direction, Accept or Back (while Reci
 * follows a Reci UI Input Data); a mapped key of a device turns off the fallback's keys for the same direction or action
 * on that device (the Input Data's Mapped Keys Replace Default Keys); Tab gives no next or previous (Tab Navigation
 * off); and a stick moves one direction at a time (Stick Dominant Axis).
 *
 * Stick Dominant Axis follows Unity uGUI's StandaloneInputModule, with Slate's own numbers (the analog thresholds and
 * GetRepeatRateForPressure). Each of a user's sticks is one 2D value: a move goes along its axis further from rest, if
 * that axis is past its threshold; the second move waits the repeat delay while the stick stays within 90 degrees of the
 * last move, and any other move the repeat rate; at rest the stick starts over.
 */
class FReciEnhancedInputNavigationConfig final : public FNavigationConfig
{
public:
	FReciEnhancedInputNavigationConfig(
		TSharedPtr<FNavigationConfig> InFallbackConfig,
		bool bInManageFallbackRegistration,
		bool bInReplaceDefaultKeys,
		bool bInTabNavigation,
		bool bInStickDominantAxis);
	virtual ~FReciEnhancedInputNavigationConfig() override;

	/**
	 * The Reci config Config is, or null for any other config: Reci's input coordinator asks the user's config about its
	 * sticks. Read from the configs alive now, since UE builds have no RTTI.
	 */
	static FReciEnhancedInputNavigationConfig* Find(const FNavigationConfig* Config);

	/** Whether a mapped key can stand for a direction, Accept or Back: a digital key, and not a mouse button or touch. */
	static bool IsNavigationKey(const FKey& Key)
	{
		return Key.IsValid()
			&& Key != EKeys::AnyKey
			&& !Key.IsAnalog()
			&& !Key.IsMouseButton()
			&& !Key.IsTouch();
	}

	/** Adds each of the keys that is a navigation key to the rules, for Value. */
	template <typename ValueType>
	static void AddNavigationKeys(TMap<FKey, ValueType>& Rules, const TArray<FKey>& Keys, const ValueType Value)
	{
		for (const FKey& Key : Keys)
		{
			if (IsNavigationKey(Key))
			{
				Rules.Add(Key, Value);
			}
		}
	}

	void SetMappedKeyEventRules(
		const TMap<FKey, EUINavigation>& InMappedKeyEventRules)
	{
		MappedKeyEventRules = InMappedKeyEventRules;
	}

	void SetMappedKeyActionRules(
		const TMap<FKey, EUINavigationAction>& InMappedKeyActionRules)
	{
		MappedKeyActionRules = InMappedKeyActionRules;
	}

	virtual EUINavigation GetNavigationDirectionFromKey(
		const FKeyEvent& InKeyEvent) const override
	{
		if (const EUINavigation* Direction =
			MappedKeyEventRules.Find(InKeyEvent.GetKey()))
		{
			const bool bNavigationEnabled = FallbackConfig
				? FallbackConfig->bKeyNavigation
				: bKeyNavigation;
			if (bNavigationEnabled)
			{
				return *Direction;
			}
		}

		const EUINavigation FallbackDirection = FallbackConfig
			? FallbackConfig->GetNavigationDirectionFromKey(InKeyEvent)
			: FNavigationConfig::GetNavigationDirectionFromKey(InKeyEvent);
		// Tab's next and previous jump by widget order, through every focusable widget, with no gamepad equivalent. Off,
		// Tab moves nothing; another key the fallback maps to next or previous keeps it.
		if (!bTabMovesFocus
			&& InKeyEvent.GetKey() == EKeys::Tab
			&& (FallbackDirection == EUINavigation::Next
				|| FallbackDirection == EUINavigation::Previous))
		{
			return EUINavigation::Invalid;
		}
		if (bReplaceDefaultKeys
			&& IsCardinalDirection(FallbackDirection)
			&& HasMappedKeyForDevice(
				FallbackDirection,
				InKeyEvent.GetKey().IsGamepadKey()))
		{
			return EUINavigation::Invalid;
		}

		return FallbackDirection;
	}

	/**
	 * The step a stick's value takes: Stick Dominant Axis's move, or with the setting off the fallback's. Slate may ask
	 * more than once for one value, and Reci's input coordinator asks before it: each ask gets the first answer, and the
	 * repeat state steps once.
	 */
	virtual EUINavigation GetNavigationDirectionFromAnalog(const FAnalogInputEvent& InAnalogEvent) override;

	/** Stick Dominant Axis: notes a stick's value as its stick's latest. Reci's input coordinator notes every value. */
	void NoteStickValue(const FAnalogInputEvent& Event);
	/**
	 * The direction a stick's value may step now, without touching the repeat state: with Stick Dominant Axis, its stick's
	 * direction from the latest values noted, when that lies on the value's own axis (each move is taken by the value of
	 * the axis it goes along); with the setting off, the value's axis past its threshold.
	 */
	EUINavigation GetStickDirection(const FAnalogInputEvent& Event) const;
	/**
	 * Stick Dominant Axis, for a stick-direction key's press or repeat: whether its direction is its stick's, from the
	 * latest values noted. The platform presses the key past its dead zone, so any deflection counts. True with the
	 * setting off, and for any other key.
	 */
	bool IsStickKeyDominant(const FKeyEvent& Event) const;

	virtual EUINavigationAction GetNavigationActionFromKey(
		const FKeyEvent& InKeyEvent) const override
	{
		const FNavigationConfig* BaseConfig =
			FallbackConfig ? FallbackConfig.Get() : this;
		const bool bModifierHeld =
			InKeyEvent.IsControlDown()
			|| InKeyEvent.IsAltDown()
			|| InKeyEvent.IsCommandDown()
			|| InKeyEvent.IsShiftDown();
		if (BaseConfig->bIgnoreModifiersForNavigationActions
			|| !bModifierHeld)
		{
			if (const EUINavigationAction* Action =
				MappedKeyActionRules.Find(InKeyEvent.GetKey()))
			{
				return *Action;
			}
		}

		const EUINavigationAction FallbackAction = FallbackConfig
			? FallbackConfig->GetNavigationActionFromKey(InKeyEvent)
			: FNavigationConfig::GetNavigationActionFromKey(InKeyEvent);
		if (bReplaceDefaultKeys
			&& FallbackAction != EUINavigationAction::Invalid
			&& HasMappedActionForDevice(
				FallbackAction,
				InKeyEvent.GetKey().IsGamepadKey()))
		{
			return EUINavigationAction::Invalid;
		}

		return FallbackAction;
	}

	virtual void OnRegister() override
	{
		FNavigationConfig::OnRegister();
		if (FallbackConfig && bManageFallbackRegistration)
		{
			FallbackConfig->OnRegister();
		}
	}

	virtual void OnUnregister() override
	{
		if (FallbackConfig && bManageFallbackRegistration)
		{
			FallbackConfig->OnUnregister();
		}
		FNavigationConfig::OnUnregister();
	}

	virtual void OnUserRemoved(const int32 UserIndex) override
	{
		FNavigationConfig::OnUserRemoved(UserIndex);
		Sticks.Remove(UserIndex);
		if (FallbackConfig && bManageFallbackRegistration)
		{
			FallbackConfig->OnUserRemoved(UserIndex);
		}
	}

	virtual void OnNavigationChangedFocus(
		TSharedPtr<SWidget> OldWidget,
		TSharedPtr<SWidget> NewWidget,
		const FFocusEvent FocusEvent) override
	{
		if (FallbackConfig)
		{
			FallbackConfig->OnNavigationChangedFocus(
				MoveTemp(OldWidget),
				MoveTemp(NewWidget),
				FocusEvent);
		}
	}

private:
	EUINavigation GetFallbackDirectionFromAnalog(const FAnalogInputEvent& InAnalogEvent)
	{
		return FallbackConfig
			? FallbackConfig->GetNavigationDirectionFromAnalog(InAnalogEvent)
			: FNavigationConfig::GetNavigationDirectionFromAnalog(
				InAnalogEvent);
	}

	/**
	 * Unity uGUI's BaseInputModule.DetermineMoveDirection, with a threshold for each axis: along the axis further from
	 * rest (Y on a tie), if it is past its threshold.
	 */
	static EUINavigation DetermineMoveDirection(const FVector2f& Values, float ThresholdX, float ThresholdY);

	static bool IsCardinalDirection(const EUINavigation Direction)
	{
		return Direction == EUINavigation::Up
			|| Direction == EUINavigation::Down
			|| Direction == EUINavigation::Left
			|| Direction == EUINavigation::Right;
	}

	bool HasMappedKeyForDevice(
		const EUINavigation Direction,
		const bool bGamepadKey) const
	{
		for (const TPair<FKey, EUINavigation>& Rule : MappedKeyEventRules)
		{
			if (Rule.Value == Direction
				&& Rule.Key.IsGamepadKey() == bGamepadKey)
			{
				return true;
			}
		}

		return false;
	}

	bool HasMappedActionForDevice(
		const EUINavigationAction Action,
		const bool bGamepadKey) const
	{
		for (const TPair<FKey, EUINavigationAction>& Rule :
			MappedKeyActionRules)
		{
			if (Rule.Value == Action
				&& Rule.Key.IsGamepadKey() == bGamepadKey)
			{
				return true;
			}
		}

		return false;
	}

	TSharedPtr<FNavigationConfig> FallbackConfig;
	bool bManageFallbackRegistration = false;
	/** The Reci UI Input Data's Mapped Keys Replace Default Keys. */
	bool bReplaceDefaultKeys = false;
	/** Tab Navigation. */
	bool bTabMovesFocus = true;
	/** Stick Dominant Axis. */
	bool bStickDominantAxis = true;
	TMap<FKey, EUINavigation> MappedKeyEventRules;
	TMap<FKey, EUINavigationAction> MappedKeyActionRules;
	/** A stick, for Stick Dominant Axis. */
	struct FStick
	{
		/** X and Y, as last noted. */
		FVector2f Values = FVector2f::ZeroVector;
		/** The values at the last move, and its time. */
		FVector2f LastMove = FVector2f::ZeroVector;
		double LastMoveTime = 0.0;
		/** Unity's consecutive moves: since the stick left rest, or last turned 90 degrees or more. */
		int32 Moves = 0;
	};
	/** Each user's left and right sticks, from their first value; removed with the user. */
	TMap<int32, TStaticArray<FStick, 2>> Sticks;
	/**
	 * The user's stick of a stick axis or stick-direction key, with the key's axis and end (GetStickAxis); null with the
	 * setting off, for any other key, and before the stick's first value.
	 */
	const FStick* FindStick(uint32 UserIndex, const FKey& Key, int32& OutAxis, int32& OutEnd) const;
	/** The last value asked about, by its event and the application's time then, and its answer. */
	struct FAskedValue
	{
		FKey Key;
		uint32 UserIndex = 0;
		uint64 Timestamp = 0;
		double Time = 0.0;
		EUINavigation Direction = EUINavigation::Invalid;
	};
	TOptional<FAskedValue> LastAsked;
};
