// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"

#include "ReciMotion.generated.h"

/**
 * Time model used by a Reci motion channel.
 *
 * Exponential motion is frame-rate independent and follows a moving target.
 * Duration-based modes restart from the current sampled value when retargeted.
 */
UENUM(BlueprintType)
enum class EReciMotionInterpolation : uint8
{
	Exponential,
	Linear,
	EaseIn,
	EaseOut,
	EaseInOut
};

/** Reusable timing and completion rules for one or more motion channels. */
USTRUCT(BlueprintType)
struct RECICOMMONUI_API FReciMotionSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	EReciMotionInterpolation Interpolation =
		EReciMotionInterpolation::Exponential;

	/** Exponential response speed. Zero snaps to the target. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Motion",
		meta = (ClampMin = "0.0"))
	float Speed = 14.0f;

	/** Playback time used by non-exponential interpolation modes. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Motion",
		meta = (ClampMin = "0.0"))
	float Duration = 0.2f;

	/** Curve exponent used by Ease In, Ease Out and Ease In Out. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Motion",
		meta = (ClampMin = "1.0"))
	float EaseExponent = 2.0f;

	/** Remaining distance at which the channel settles exactly on its target. */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Motion",
		meta = (ClampMin = "0.0"))
	float CompletionTolerance = 0.01f;

	static FReciMotionSettings MakeExponential(
		float InSpeed,
		float InCompletionTolerance = 0.01f);

	/**
	 * These settings under a motion scale (UReciMotionSubsystem): the target stays, and the time to reach it
	 * shrinks by the scale. Speed is divided by it and Duration multiplied by it; at 0 both are 0, which snaps to the
	 * target. A scale is clamped to 0 to 1, and one that is not finite counts as 1. A channel may change scale
	 * mid-motion: exponential motion has no memory, and duration motion continues from its sampled value.
	 */
	FReciMotionSettings Scaled(float MotionScale) const;
};

/**
 * Stateful float motion channel shared by ReciCommonUI widgets.
 *
 * Widgets own semantic state and presentation. This channel only owns the
 * sampled value, target and timing required to move between them.
 */
class RECICOMMONUI_API FReciFloatMotion final
{
public:
	void Reset(float InitialValue = 0.0f);

	float GetCurrentValue() const;
	float GetTargetValue() const;
	bool IsActive() const;

	/** Retargets from the current sampled value without introducing a jump. */
	bool Retarget(float NewTargetValue);

	/** Sets current and target to the same value and stops the channel. */
	void SnapTo(float Value);

	/** Completes the current target immediately. */
	void SnapToTarget();

	/** Pauses this channel without changing its value, target or elapsed time. */
	void Stop();

	/** Resumes the retained motion without restarting duration-based playback. */
	void Resume();

	/** Advances the sample. Returns true when an active sample was processed. */
	bool Advance(float DeltaTime, const FReciMotionSettings& Settings);

private:
	static float EvaluateDurationAlpha(
		EReciMotionInterpolation Interpolation,
		float NormalizedTime,
		float EaseExponent);

	float CurrentValue = 0.0f;
	float StartValue = 0.0f;
	float TargetValue = 0.0f;
	float ElapsedTime = 0.0f;
	EReciMotionInterpolation LastInterpolation =
		EReciMotionInterpolation::Exponential;
	float LastDuration = 0.0f;
	float LastEaseExponent = 1.0f;
	bool bActive = false;
	bool bPaused = false;
	bool bHasLastAdvanceSettings = false;
};

/**
 * Internal: the velocity a pointer or touch drag leaves, and the glide after it is released.
 *
 * Both are exact for any split of time, so they do not depend on the frame rate or the pointer's event rate. The
 * drag velocity is a low-pass of the pointer's velocity with a time constant of VelocitySmoothingTime, and the time
 * the pointer holds still before release counts as zero velocity. The glide decays exponentially, covers exactly
 * (release speed - minimum speed) / deceleration and ends when its speed reaches the minimum speed. Under a motion
 * scale s the deceleration is k / s, so the glide's distance and time both shrink by s; at 0 nothing glides.
 */
class RECICOMMONUI_API FReciInertiaMotion final
{
public:
	/**
	 * Time constant (s) of the drag velocity: (1/60) / ln(1/0.65), so one sample at 60 Hz moves the velocity 35% of
	 * the way to the pointer's.
	 */
	static constexpr double VelocitySmoothingTime = 0.0386892470524288;

	/** Lowest minimum speed (units/s), so that a glide always ends. */
	static constexpr double MinimumSpeedFloor = 0.01;

	/**
	 * Adds a drag sample: the pointer moved Delta units in DeltaSeconds. A sample with no time, or that is not
	 * finite, is ignored. A drag ends any glide.
	 */
	void AddDragSample(float Delta, double DeltaSeconds);

	/**
	 * Releases the drag after the pointer held still for IdleSeconds. Starts the glide, and returns true, only when
	 * the velocity left is at least MinimumSpeed (at least MinimumSpeedFloor) and MotionScale is above 0.
	 */
	bool Release(
		double IdleSeconds,
		float MinimumSpeed,
		float MotionScale = 1.0f);

	/**
	 * Advances the glide by DeltaTime and returns how far it moved. Deceleration is the exponential rate (1/s) at
	 * MotionScale 1; at 0 the glide keeps its speed. A time that is not finite or not positive moves nothing, and a
	 * MotionScale of 0 ends the glide.
	 */
	double Advance(
		float DeltaTime,
		float Deceleration,
		float MinimumSpeed,
		float MotionScale = 1.0f);

	/** Ends the glide where it is. */
	void Stop();

	/** Forgets the drag: no velocity and no glide. */
	void Reset();

	bool IsActive() const;
	double GetVelocity() const;

private:
	double Velocity = 0.0;
	bool bActive = false;
};
