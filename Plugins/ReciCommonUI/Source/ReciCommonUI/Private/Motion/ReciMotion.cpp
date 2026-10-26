// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Motion/ReciMotion.h"

#include <cmath>

namespace
{
	float ResolveFiniteNonNegative(
		const float Value,
		const float InvalidFallback = 0.0f)
	{
		return FMath::IsFinite(Value)
			? FMath::Max(Value, 0.0f)
			: InvalidFallback;
	}

	/** A motion scale in 0 to 1; one that is not finite counts as 1. */
	float ResolveMotionScale(const float MotionScale)
	{
		return FMath::IsFinite(MotionScale)
			? FMath::Clamp(MotionScale, 0.0f, 1.0f)
			: 1.0f;
	}

	double ResolveMinimumInertiaSpeed(const float MinimumSpeed)
	{
		return FMath::Max(
			static_cast<double>(ResolveFiniteNonNegative(MinimumSpeed)),
			FReciInertiaMotion::MinimumSpeedFloor);
	}

	float LerpFiniteFloat(
		const float Start,
		const float Target,
		const double Alpha)
	{
		return static_cast<float>(
			static_cast<double>(Start)
			+ (static_cast<double>(Target)
				- static_cast<double>(Start)) * Alpha);
	}
}

FReciMotionSettings FReciMotionSettings::MakeExponential(
	const float InSpeed,
	const float InCompletionTolerance)
{
	FReciMotionSettings Settings;
	Settings.Interpolation = EReciMotionInterpolation::Exponential;
	Settings.Speed = ResolveFiniteNonNegative(InSpeed);
	Settings.CompletionTolerance = ResolveFiniteNonNegative(
		InCompletionTolerance,
		0.0f);
	return Settings;
}

FReciMotionSettings FReciMotionSettings::Scaled(const float MotionScale) const
{
	const float Scale = ResolveMotionScale(MotionScale);
	FReciMotionSettings Result = *this;
	if (Scale <= 0.0f)
	{
		// Both mean snap to the target.
		Result.Speed = 0.0f;
		Result.Duration = 0.0f;
	}
	else if (Scale < 1.0f)
	{
		Result.Speed = Speed / Scale;
		Result.Duration = Duration * Scale;
	}
	return Result;
}

void FReciFloatMotion::Reset(const float InitialValue)
{
	const float SafeInitialValue = FMath::IsFinite(InitialValue)
		? InitialValue
		: 0.0f;
	CurrentValue = SafeInitialValue;
	StartValue = SafeInitialValue;
	TargetValue = SafeInitialValue;
	ElapsedTime = 0.0f;
	bActive = false;
	bPaused = false;
	bHasLastAdvanceSettings = false;
}

float FReciFloatMotion::GetCurrentValue() const
{
	return CurrentValue;
}

float FReciFloatMotion::GetTargetValue() const
{
	return TargetValue;
}

bool FReciFloatMotion::IsActive() const
{
	return bActive;
}

bool FReciFloatMotion::Retarget(const float NewTargetValue)
{
	if (!FMath::IsFinite(NewTargetValue))
	{
		return false;
	}

	const bool bTargetChanged = TargetValue != NewTargetValue;
	if (bTargetChanged)
	{
		StartValue = CurrentValue;
		TargetValue = NewTargetValue;
		ElapsedTime = 0.0f;
		bActive = !bPaused && CurrentValue != TargetValue;
		bHasLastAdvanceSettings = false;
	}
	return bTargetChanged;
}

void FReciFloatMotion::SnapTo(const float Value)
{
	const float SafeValue = FMath::IsFinite(Value) ? Value : 0.0f;
	CurrentValue = SafeValue;
	StartValue = SafeValue;
	TargetValue = SafeValue;
	ElapsedTime = 0.0f;
	bActive = false;
	bPaused = false;
	bHasLastAdvanceSettings = false;
}

void FReciFloatMotion::SnapToTarget()
{
	SnapTo(TargetValue);
}

void FReciFloatMotion::Stop()
{
	bActive = false;
	bPaused = true;
}

void FReciFloatMotion::Resume()
{
	bPaused = false;
	bActive = CurrentValue != TargetValue;
}

bool FReciFloatMotion::Advance(
	const float DeltaTime,
	const FReciMotionSettings& Settings)
{
	if (!bActive)
	{
		return false;
	}

	const float SafeDeltaTime = FMath::IsFinite(DeltaTime)
		? FMath::Max(DeltaTime, 0.0f)
		: 0.0f;
	const float SafeDuration = ResolveFiniteNonNegative(Settings.Duration);
	const float SafeEaseExponent = FMath::IsFinite(Settings.EaseExponent)
		? FMath::Max(Settings.EaseExponent, 1.0f)
		: 1.0f;
	const bool bDurationSettingsChanged = bHasLastAdvanceSettings
		&& (LastInterpolation != Settings.Interpolation
			|| (Settings.Interpolation
					!= EReciMotionInterpolation::Exponential
				&& (!FMath::IsNearlyEqual(LastDuration, SafeDuration)
					|| !FMath::IsNearlyEqual(
						LastEaseExponent,
						SafeEaseExponent))));
	if (bDurationSettingsChanged)
	{
		StartValue = CurrentValue;
		ElapsedTime = 0.0f;
	}
	LastInterpolation = Settings.Interpolation;
	LastDuration = SafeDuration;
	LastEaseExponent = SafeEaseExponent;
	bHasLastAdvanceSettings = true;

	const float PreviousValue = CurrentValue;
	if (Settings.Interpolation == EReciMotionInterpolation::Exponential)
	{
		const float SafeSpeed = ResolveFiniteNonNegative(Settings.Speed);
		const double Alpha = SafeSpeed <= 0.0f
			? 1.0
			: 1.0 - FMath::Exp(
				-static_cast<double>(SafeSpeed)
				* static_cast<double>(SafeDeltaTime));
		CurrentValue = LerpFiniteFloat(
			CurrentValue,
			TargetValue,
			Alpha);
	}
	else
	{
		ElapsedTime += SafeDeltaTime;
		const float NormalizedTime = SafeDuration <= 0.0f
			? 1.0f
			: FMath::Clamp(ElapsedTime / SafeDuration, 0.0f, 1.0f);
		const float Alpha = EvaluateDurationAlpha(
			Settings.Interpolation,
			NormalizedTime,
			SafeEaseExponent);
		CurrentValue = LerpFiniteFloat(
			StartValue,
			TargetValue,
			Alpha);

		if (NormalizedTime >= 1.0f)
		{
			SnapToTarget();
			return true;
		}
	}

	const float SafeCompletionTolerance = ResolveFiniteNonNegative(
		Settings.CompletionTolerance);
	const bool bWithinTolerance = FMath::Abs(
		CurrentValue - TargetValue) <= SafeCompletionTolerance;
	const bool bMadeNoProgress = Settings.Interpolation
			== EReciMotionInterpolation::Exponential
		&& SafeDeltaTime > 0.0f
		&& CurrentValue == PreviousValue
		&& CurrentValue != TargetValue;
	if (!FMath::IsFinite(CurrentValue)
		|| bWithinTolerance
		|| bMadeNoProgress)
	{
		SnapToTarget();
	}
	return true;
}

float FReciFloatMotion::EvaluateDurationAlpha(
	const EReciMotionInterpolation Interpolation,
	const float NormalizedTime,
	const float EaseExponent)
{
	const float Alpha = FMath::Clamp(NormalizedTime, 0.0f, 1.0f);
	const float Exponent = FMath::Max(EaseExponent, 1.0f);
	switch (Interpolation)
	{
	case EReciMotionInterpolation::EaseIn:
		return FMath::Pow(Alpha, Exponent);

	case EReciMotionInterpolation::EaseOut:
		return 1.0f - FMath::Pow(1.0f - Alpha, Exponent);

	case EReciMotionInterpolation::EaseInOut:
		return FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, Exponent);

	case EReciMotionInterpolation::Linear:
	case EReciMotionInterpolation::Exponential:
	default:
		return Alpha;
	}
}

void FReciInertiaMotion::AddDragSample(
	const float Delta,
	const double DeltaSeconds)
{
	bActive = false;
	if (!FMath::IsFinite(Delta)
		|| !FMath::IsFinite(DeltaSeconds)
		|| DeltaSeconds <= 0.0)
	{
		return;
	}

	// Exact for a pointer that moves at a constant velocity between samples, at any sample rate.
	const double SegmentVelocity = static_cast<double>(Delta) / DeltaSeconds;
	Velocity = SegmentVelocity
		+ (Velocity - SegmentVelocity)
			* FMath::Exp(-DeltaSeconds / VelocitySmoothingTime);
	if (!FMath::IsFinite(Velocity))
	{
		Velocity = 0.0;
	}
}

bool FReciInertiaMotion::Release(
	const double IdleSeconds,
	const float MinimumSpeed,
	const float MotionScale)
{
	// The time the pointer held still is a segment of zero velocity.
	if (FMath::IsFinite(IdleSeconds) && IdleSeconds > 0.0)
	{
		Velocity *= FMath::Exp(-IdleSeconds / VelocitySmoothingTime);
	}
	bActive = ResolveMotionScale(MotionScale) > 0.0f
		&& FMath::Abs(Velocity) >= ResolveMinimumInertiaSpeed(MinimumSpeed);
	return bActive;
}

double FReciInertiaMotion::Advance(
	const float DeltaTime,
	const float Deceleration,
	const float MinimumSpeed,
	const float MotionScale)
{
	if (!bActive
		|| !FMath::IsFinite(DeltaTime)
		|| DeltaTime <= 0.0f)
	{
		return 0.0;
	}

	const double Scale = ResolveMotionScale(MotionScale);
	const double Speed = FMath::Abs(Velocity);
	const double SafeMinimumSpeed = ResolveMinimumInertiaSpeed(MinimumSpeed);
	if (Scale <= 0.0 || Speed <= SafeMinimumSpeed)
	{
		Stop();
		return 0.0;
	}

	// A motion scale shortens the glide's time constant, and with it the glide's distance, by the scale.
	const double Time = static_cast<double>(DeltaTime);
	const double Rate = ResolveFiniteNonNegative(Deceleration) / Scale;
	if (Rate <= 0.0)
	{
		return Velocity * Time;
	}

	// v(t) = v0 e^(-kt) reaches the minimum speed at ln(|v0| / minimum) / k and is covered exactly up to there.
	const double StopTime = FMath::Loge(Speed / SafeMinimumSpeed) / Rate;
	const double Step = FMath::Min(Time, StopTime);
	const double Displacement = Velocity * -std::expm1(-Rate * Step) / Rate;
	if (Time >= StopTime)
	{
		Stop();
	}
	else
	{
		Velocity *= FMath::Exp(-Rate * Step);
	}
	return Displacement;
}

void FReciInertiaMotion::Stop()
{
	Velocity = 0.0;
	bActive = false;
}

void FReciInertiaMotion::Reset()
{
	Stop();
}

bool FReciInertiaMotion::IsActive() const
{
	return bActive;
}

double FReciInertiaMotion::GetVelocity() const
{
	return Velocity;
}
