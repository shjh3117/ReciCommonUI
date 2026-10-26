// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Navigation/ReciEnhancedInputNavigationConfig.h"

#include "Input/ReciScreenInputMapping.h"
#include "Misc/App.h"

namespace
{
	/** Every Reci navigation config alive, for Find. Game thread only, as Slate's navigation is. */
	TSet<const FNavigationConfig*>& GetLiveConfigs()
	{
		static TSet<const FNavigationConfig*> Configs;
		return Configs;
	}
}

FReciEnhancedInputNavigationConfig::FReciEnhancedInputNavigationConfig(
	TSharedPtr<FNavigationConfig> InFallbackConfig,
	const bool bInManageFallbackRegistration,
	const bool bInReplaceDefaultKeys,
	const bool bInTabNavigation,
	const bool bInStickDominantAxis)
	: FallbackConfig(MoveTemp(InFallbackConfig))
	, bManageFallbackRegistration(bInManageFallbackRegistration)
	, bReplaceDefaultKeys(bInReplaceDefaultKeys)
	, bTabMovesFocus(bInTabNavigation)
	, bStickDominantAxis(bInStickDominantAxis)
{
	// The fallback takes the analog steps, so its analog settings are this config's too: Slate reads them to tell
	// whether a stick counts as interaction, and Reci to decide a focused control's first step.
	if (FallbackConfig)
	{
		bAnalogNavigation = FallbackConfig->bAnalogNavigation;
		AnalogNavigationHorizontalThreshold = FallbackConfig->AnalogNavigationHorizontalThreshold;
		AnalogNavigationVerticalThreshold = FallbackConfig->AnalogNavigationVerticalThreshold;
		AnalogHorizontalKey = FallbackConfig->AnalogHorizontalKey;
		AnalogVerticalKey = FallbackConfig->AnalogVerticalKey;
	}
	GetLiveConfigs().Add(this);
}

FReciEnhancedInputNavigationConfig::~FReciEnhancedInputNavigationConfig()
{
	GetLiveConfigs().Remove(this);
}

FReciEnhancedInputNavigationConfig* FReciEnhancedInputNavigationConfig::Find(const FNavigationConfig* Config)
{
	return Config && GetLiveConfigs().Contains(Config)
		? static_cast<FReciEnhancedInputNavigationConfig*>(const_cast<FNavigationConfig*>(Config))
		: nullptr;
}

EUINavigation FReciEnhancedInputNavigationConfig::DetermineMoveDirection(
	const FVector2f& Values,
	const float ThresholdX,
	const float ThresholdY)
{
	if (FMath::Abs(Values.X) > FMath::Abs(Values.Y))
	{
		return Values.X > ThresholdX ? EUINavigation::Right
			: Values.X < -ThresholdX ? EUINavigation::Left
			: EUINavigation::Invalid;
	}
	return Values.Y > ThresholdY ? EUINavigation::Up
		: Values.Y < -ThresholdY ? EUINavigation::Down
		: EUINavigation::Invalid;
}

const FReciEnhancedInputNavigationConfig::FStick* FReciEnhancedInputNavigationConfig::FindStick(
	const uint32 UserIndex,
	const FKey& Key,
	int32& OutAxis,
	int32& OutEnd) const
{
	int32 StickIndex = 0;
	if (!bStickDominantAxis || !UE::ReciCommonUI::Private::GetStickAxis(Key, StickIndex, OutAxis, OutEnd))
	{
		return nullptr;
	}
	const TStaticArray<FStick, 2>* UserSticks = Sticks.Find(UserIndex);
	return UserSticks ? &(*UserSticks)[StickIndex] : nullptr;
}

void FReciEnhancedInputNavigationConfig::NoteStickValue(const FAnalogInputEvent& Event)
{
	int32 StickIndex = 0;
	int32 Axis = 0;
	int32 End = 0;
	if (!bStickDominantAxis || !UE::ReciCommonUI::Private::GetStickAxis(Event.GetKey(), StickIndex, Axis, End))
	{
		return;
	}
	FStick& Stick = Sticks.FindOrAdd(Event.GetUserIndex())[StickIndex];
	Stick.Values[Axis] = Event.GetAnalogValue();
	// At rest the stick starts over.
	if (DetermineMoveDirection(Stick.Values, AnalogNavigationHorizontalThreshold, AnalogNavigationVerticalThreshold)
		== EUINavigation::Invalid)
	{
		Stick.LastMove = FVector2f::ZeroVector;
		Stick.LastMoveTime = 0.0;
		Stick.Moves = 0;
	}
}

EUINavigation FReciEnhancedInputNavigationConfig::GetStickDirection(const FAnalogInputEvent& Event) const
{
	const bool bHorizontal = Event.GetKey() == AnalogHorizontalKey;
	if (!bAnalogNavigation || (!bHorizontal && Event.GetKey() != AnalogVerticalKey))
	{
		return EUINavigation::Invalid;
	}
	int32 Axis = 0;
	int32 End = 0;
	const FStick* Stick = FindStick(Event.GetUserIndex(), Event.GetKey(), Axis, End);
	const FVector2f Values = Stick
		? Stick->Values
		: FVector2f(bHorizontal ? Event.GetAnalogValue() : 0.0f, bHorizontal ? 0.0f : Event.GetAnalogValue());
	const EUINavigation Direction =
		DetermineMoveDirection(Values, AnalogNavigationHorizontalThreshold, AnalogNavigationVerticalThreshold);
	const bool bHorizontalDirection = Direction == EUINavigation::Left || Direction == EUINavigation::Right;
	return bHorizontalDirection == bHorizontal ? Direction : EUINavigation::Invalid;
}

bool FReciEnhancedInputNavigationConfig::IsStickKeyDominant(const FKeyEvent& Event) const
{
	int32 Axis = 0;
	int32 End = 0;
	if (!bStickDominantAxis || !UE::ReciCommonUI::Private::IsStickDirectionKey(Event.GetKey()))
	{
		return true;
	}
	const FStick* Stick = FindStick(Event.GetUserIndex(), Event.GetKey(), Axis, End);
	FVector2f KeyValues = FVector2f::ZeroVector;
	KeyValues[Axis] = static_cast<float>(End);
	return DetermineMoveDirection(KeyValues, 0.0f, 0.0f)
		== DetermineMoveDirection(Stick ? Stick->Values : FVector2f::ZeroVector, 0.0f, 0.0f);
}

EUINavigation FReciEnhancedInputNavigationConfig::GetNavigationDirectionFromAnalog(const FAnalogInputEvent& InAnalogEvent)
{
	const double Now = FApp::GetCurrentTime();
	if (LastAsked
		&& LastAsked->Key == InAnalogEvent.GetKey()
		&& LastAsked->UserIndex == InAnalogEvent.GetUserIndex()
		&& LastAsked->Timestamp == InAnalogEvent.GetEventTimestamp()
		&& LastAsked->Time == Now)
	{
		return LastAsked->Direction;
	}
	LastAsked = FAskedValue{
		InAnalogEvent.GetKey(),
		InAnalogEvent.GetUserIndex(),
		InAnalogEvent.GetEventTimestamp(),
		Now,
		EUINavigation::Invalid};

	NoteStickValue(InAnalogEvent);
	int32 Axis = 0;
	int32 End = 0;
	// Noted just above, with the setting on.
	FStick* Stick = const_cast<FStick*>(FindStick(InAnalogEvent.GetUserIndex(), InAnalogEvent.GetKey(), Axis, End));
	// An axis the fallback's analog navigation does not read keeps the fallback's answer.
	if (!Stick || (InAnalogEvent.GetKey() != AnalogHorizontalKey && InAnalogEvent.GetKey() != AnalogVerticalKey))
	{
		LastAsked->Direction = GetFallbackDirectionFromAnalog(InAnalogEvent);
		return LastAsked->Direction;
	}
	const EUINavigation Direction = GetStickDirection(InAnalogEvent);
	if (Direction == EUINavigation::Invalid)
	{
		return EUINavigation::Invalid;
	}
	// Within 90 degrees of the last move, the second move waits the repeat delay; a first move, a turn of 90 degrees or
	// more, and every later move wait the repeat rate.
	const bool bSameWay = FVector2f::DotProduct(Stick->Values, Stick->LastMove) > 0.0f;
	const float Wait =
		GetRepeatRateForPressure(FMath::Abs(InAnalogEvent.GetAnalogValue()), bSameWay && Stick->Moves == 1 ? 0 : 1);
	if (Now - Stick->LastMoveTime <= Wait)
	{
		return EUINavigation::Invalid;
	}
	Stick->Moves = bSameWay ? Stick->Moves + 1 : 1;
	Stick->LastMove = Stick->Values;
	Stick->LastMoveTime = Now;
	LastAsked->Direction = Direction;
	return Direction;
}
