// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Motion/ReciMotion.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"

#include <limits>

namespace
{
	constexpr EAutomationTestFlags ReciMotionTestFlags =
		EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EngineFilter;

	FReciMotionSettings MakeDurationSettings(
		const EReciMotionInterpolation Interpolation,
		const float Duration = 1.0f)
	{
		FReciMotionSettings Settings;
		Settings.Interpolation = Interpolation;
		Settings.Duration = Duration;
		Settings.EaseExponent = 2.0f;
		Settings.CompletionTolerance = 0.0f;
		return Settings;
	}

	constexpr float InertiaDeceleration = 8.0f;
	constexpr float InertiaMinimumSpeed = 5.0f;

	/** A glide released at Velocity: one long sample makes the drag velocity the segment's. */
	FReciInertiaMotion MakeGlide(const double Velocity)
	{
		FReciInertiaMotion Motion;
		Motion.AddDragSample(static_cast<float>(Velocity * 10.0), 10.0);
		Motion.Release(0.0, InertiaMinimumSpeed);
		return Motion;
	}

	/** A drag of 600 units/s sampled at 60 Hz for 0.2 s. */
	FReciInertiaMotion MakeSteadyDrag()
	{
		FReciInertiaMotion Motion;
		for (int32 Sample = 0; Sample < 12; ++Sample)
		{
			Motion.AddDragSample(10.0f, 1.0 / 60.0);
		}
		return Motion;
	}

	/** Frame times: 1 / FrameRate, or from FRandomStream(42) between 1/500 and 1/20 s for a rate of 0. */
	struct FGlideSteps
	{
		explicit FGlideSteps(const float InFrameRate)
			: FrameRate(InFrameRate)
		{
		}

		float Next()
		{
			return FrameRate > 0.0f
				? 1.0f / FrameRate
				: Stream.FRandRange(1.0f / 500.0f, 1.0f / 20.0f);
		}

		float FrameRate;
		FRandomStream Stream{42};
	};

	struct FGlideRun
	{
		double Displacement = 0.0;
		double Elapsed = 0.0;
		double LastStep = 0.0;
	};

	/** Advances a glide in Steps until it ends or Duration has passed, cutting the last step short. */
	FGlideRun RunGlide(
		FReciInertiaMotion& Motion,
		FGlideSteps Steps,
		const double Duration)
	{
		FGlideRun Run;
		while (Motion.IsActive() && Run.Elapsed < Duration)
		{
			const float Step = static_cast<float>(
				FMath::Min(static_cast<double>(Steps.Next()), Duration - Run.Elapsed));
			Run.Displacement += Motion.Advance(
				Step,
				InertiaDeceleration,
				InertiaMinimumSpeed);
			Run.Elapsed += Step;
			Run.LastStep = Step;
		}
		return Run;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FReciMotionExponentialTest,
	"ReciCommonUI.Motion.Exponential",
	ReciMotionTestFlags)

bool FReciMotionExponentialTest::RunTest(const FString& Parameters)
{
	const FReciMotionSettings Settings =
		FReciMotionSettings::MakeExponential(4.0f, 0.0f);

	FReciFloatMotion OneStepMotion;
	OneStepMotion.SnapTo(0.0f);
	OneStepMotion.Retarget(1.0f);
	OneStepMotion.Advance(1.0f, Settings);

	FReciFloatMotion SixtyStepMotion;
	SixtyStepMotion.SnapTo(0.0f);
	SixtyStepMotion.Retarget(1.0f);
	for (int32 StepIndex = 0; StepIndex < 60; ++StepIndex)
	{
		SixtyStepMotion.Advance(1.0f / 60.0f, Settings);
	}

	TestTrue(
		TEXT("Exponential motion is frame-rate independent"),
		FMath::IsNearlyEqual(
			OneStepMotion.GetCurrentValue(),
			SixtyStepMotion.GetCurrentValue(),
			0.00001f));
	TestTrue(
		TEXT("Exponential motion does not overshoot"),
		SixtyStepMotion.GetCurrentValue() > 0.0f
			&& SixtyStepMotion.GetCurrentValue() < 1.0f);

	for (int32 StepIndex = 0;
		StepIndex < 2000 && SixtyStepMotion.IsActive();
		++StepIndex)
	{
		SixtyStepMotion.Advance(1.0f / 60.0f, Settings);
	}
	TestFalse(
		TEXT("Zero tolerance still settles after float progress stops"),
		SixtyStepMotion.IsActive());
	TestEqual(
		TEXT("Settled exponential motion reaches its exact target"),
		SixtyStepMotion.GetCurrentValue(),
		1.0f);

	FReciFloatMotion ZeroSpeedMotion;
	ZeroSpeedMotion.SnapTo(0.0f);
	ZeroSpeedMotion.Retarget(10.0f);
	ZeroSpeedMotion.Advance(
		0.0f,
		FReciMotionSettings::MakeExponential(0.0f));
	TestEqual(
		TEXT("Zero exponential speed snaps immediately"),
		ZeroSpeedMotion.GetCurrentValue(),
		10.0f);

	FReciFloatMotion InvalidDeltaMotion;
	InvalidDeltaMotion.SnapTo(0.0f);
	InvalidDeltaMotion.Retarget(10.0f);
	InvalidDeltaMotion.Advance(-1.0f, Settings);
	TestEqual(
		TEXT("Negative delta time does not move the channel"),
		InvalidDeltaMotion.GetCurrentValue(),
		0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FReciMotionDurationTest,
	"ReciCommonUI.Motion.Duration",
	ReciMotionTestFlags)

bool FReciMotionDurationTest::RunTest(const FString& Parameters)
{
	struct FMidpointCase
	{
		EReciMotionInterpolation Interpolation;
		float ExpectedMidpoint;
	};
	const FMidpointCase Cases[] = {
		{EReciMotionInterpolation::Linear, 0.5f},
		{EReciMotionInterpolation::EaseIn, 0.25f},
		{EReciMotionInterpolation::EaseOut, 0.75f},
		{EReciMotionInterpolation::EaseInOut, 0.5f}};

	for (const FMidpointCase& Case : Cases)
	{
		FReciFloatMotion Motion;
		Motion.SnapTo(0.0f);
		Motion.Retarget(1.0f);
		const FReciMotionSettings Settings =
			MakeDurationSettings(Case.Interpolation);
		Motion.Advance(0.5f, Settings);
		TestTrue(
			TEXT("Duration curve has the expected midpoint"),
			FMath::IsNearlyEqual(
				Motion.GetCurrentValue(),
				Case.ExpectedMidpoint,
				0.00001f));
		Motion.Advance(0.5f, Settings);
		TestEqual(
			TEXT("Duration curve reaches its exact endpoint"),
			Motion.GetCurrentValue(),
			1.0f);
		TestFalse(
			TEXT("Completed duration curve is inactive"),
			Motion.IsActive());
	}

	FReciFloatMotion SettingsChangeMotion;
	SettingsChangeMotion.SnapTo(0.0f);
	SettingsChangeMotion.Retarget(100.0f);
	SettingsChangeMotion.Advance(
		0.5f,
		MakeDurationSettings(EReciMotionInterpolation::Linear));
	const float ValueBeforeSettingsChange =
		SettingsChangeMotion.GetCurrentValue();
	SettingsChangeMotion.Advance(
		0.0f,
		MakeDurationSettings(EReciMotionInterpolation::EaseOut, 2.0f));
	TestEqual(
		TEXT("Changing duration settings rebases without a value jump"),
		SettingsChangeMotion.GetCurrentValue(),
		ValueBeforeSettingsChange);
	SettingsChangeMotion.Advance(
		1.0f,
		MakeDurationSettings(EReciMotionInterpolation::EaseOut, 2.0f));
	TestTrue(
		TEXT("Rebased duration settings continue from the sampled value"),
		FMath::IsNearlyEqual(
			SettingsChangeMotion.GetCurrentValue(),
			87.5f,
			0.00001f));
	SettingsChangeMotion.Advance(
		1.0f,
		MakeDurationSettings(EReciMotionInterpolation::EaseOut, 2.0f));
	TestEqual(
		TEXT("Rebased duration settings still complete exactly"),
		SettingsChangeMotion.GetCurrentValue(),
		100.0f);

	FReciFloatMotion SubUlpMotion;
	SubUlpMotion.SnapTo(1000000.0f);
	SubUlpMotion.Retarget(1000001.0f);
	SubUlpMotion.Advance(
		1.0f / 60.0f,
		MakeDurationSettings(EReciMotionInterpolation::EaseIn, 1000.0f));
	TestEqual(
		TEXT("A sub-ULP duration sample does not snap to its target"),
		SubUlpMotion.GetCurrentValue(),
		1000000.0f);
	TestTrue(
		TEXT("A sub-ULP duration sample remains active"),
		SubUlpMotion.IsActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FReciMotionRetargetAndPauseTest,
	"ReciCommonUI.Motion.RetargetAndPause",
	ReciMotionTestFlags)

bool FReciMotionRetargetAndPauseTest::RunTest(
	const FString& Parameters)
{
	const FReciMotionSettings LinearSettings =
		MakeDurationSettings(EReciMotionInterpolation::Linear);
	FReciFloatMotion Motion;
	Motion.SnapTo(0.0f);
	Motion.Retarget(100.0f);
	Motion.Advance(0.25f, LinearSettings);
	TestEqual(TEXT("Motion reaches 25 percent"), Motion.GetCurrentValue(), 25.0f);

	TestFalse(
		TEXT("Retargeting the same target does not restart playback"),
		Motion.Retarget(100.0f));
	Motion.Advance(0.25f, LinearSettings);
	TestEqual(
		TEXT("Same-target retarget preserves elapsed time"),
		Motion.GetCurrentValue(),
		50.0f);

	Motion.Stop();
	TestFalse(TEXT("Stopped motion is inactive"), Motion.IsActive());
	TestFalse(
		TEXT("Stopped motion does not advance"),
		Motion.Advance(0.25f, LinearSettings));
	TestEqual(TEXT("Stopped value is retained"), Motion.GetCurrentValue(), 50.0f);
	Motion.Resume();
	Motion.Advance(0.25f, LinearSettings);
	TestEqual(
		TEXT("Resume continues retained duration playback"),
		Motion.GetCurrentValue(),
		75.0f);

	Motion.Stop();
	TestTrue(TEXT("Paused motion accepts a new target"), Motion.Retarget(200.0f));
	TestFalse(
		TEXT("Retarget does not implicitly resume a paused channel"),
		Motion.IsActive());
	Motion.Resume();
	Motion.Advance(0.5f, LinearSettings);
	TestEqual(
		TEXT("Paused retarget resumes from the retained value"),
		Motion.GetCurrentValue(),
		137.5f);

	FReciFloatMotion ActiveRetargetMotion;
	ActiveRetargetMotion.SnapTo(0.0f);
	ActiveRetargetMotion.Retarget(100.0f);
	ActiveRetargetMotion.Advance(0.25f, LinearSettings);
	const float ValueBeforeRetarget =
		ActiveRetargetMotion.GetCurrentValue();
	ActiveRetargetMotion.Retarget(200.0f);
	TestEqual(
		TEXT("Active retargeting does not jump the sampled value"),
		ActiveRetargetMotion.GetCurrentValue(),
		ValueBeforeRetarget);
	ActiveRetargetMotion.Advance(0.5f, LinearSettings);
	TestEqual(
		TEXT("Active retargeting restarts from the sampled value"),
		ActiveRetargetMotion.GetCurrentValue(),
		112.5f);

	FReciFloatMotion TinyRetargetMotion;
	TinyRetargetMotion.SnapTo(0.0f);
	TestTrue(
		TEXT("A sub-KINDA target change is retained"),
		TinyRetargetMotion.Retarget(0.00005f));
	TestEqual(
		TEXT("The exact small target is stored"),
		TinyRetargetMotion.GetTargetValue(),
		0.00005f);
	TinyRetargetMotion.Advance(
		1.0f,
		MakeDurationSettings(EReciMotionInterpolation::Linear));
	TestEqual(
		TEXT("A small duration target completes exactly"),
		TinyRetargetMotion.GetCurrentValue(),
		0.00005f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FReciMotionInvalidInputTest,
	"ReciCommonUI.Motion.InvalidInput",
	ReciMotionTestFlags)

bool FReciMotionInvalidInputTest::RunTest(const FString& Parameters)
{
	const float NaN = std::numeric_limits<float>::quiet_NaN();
	const float Infinity = std::numeric_limits<float>::infinity();

	FReciFloatMotion Motion;
	Motion.Reset(NaN);
	TestEqual(TEXT("NaN initial value resolves to zero"), Motion.GetCurrentValue(), 0.0f);
	TestFalse(TEXT("Infinite targets are rejected"), Motion.Retarget(Infinity));
	TestEqual(TEXT("Rejected target leaves state intact"), Motion.GetTargetValue(), 0.0f);

	Motion.Retarget(1.0f);
	FReciMotionSettings InvalidSpeedSettings =
		FReciMotionSettings::MakeExponential(1.0f);
	InvalidSpeedSettings.Speed = NaN;
	Motion.Advance(1.0f / 60.0f, InvalidSpeedSettings);
	TestTrue(
		TEXT("Invalid speed cannot contaminate the sampled value"),
		FMath::IsFinite(Motion.GetCurrentValue()));
	TestEqual(
		TEXT("Invalid exponential speed uses snap semantics"),
		Motion.GetCurrentValue(),
		1.0f);

	Motion.SnapTo(0.0f);
	Motion.Retarget(1.0f);
	FReciMotionSettings InvalidDeltaSettings =
		FReciMotionSettings::MakeExponential(1.0f, 0.0f);
	Motion.Advance(NaN, InvalidDeltaSettings);
	TestTrue(
		TEXT("Invalid delta cannot contaminate the sampled value"),
		FMath::IsFinite(Motion.GetCurrentValue()));
	TestEqual(
		TEXT("Invalid delta does not move the channel"),
		Motion.GetCurrentValue(),
		0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FReciMotionInertiaFrameRateTest,
	"ReciCommonUI.Motion.InertiaFrameRate",
	ReciMotionTestFlags)

bool FReciMotionInertiaFrameRateTest::RunTest(const FString& Parameters)
{
	// v0 1000, k 8, minimum 5: x(t) = v0 (1 - e^-kt) / k, until the speed reaches the minimum at ln(v0 / 5) / k.
	const double ExpectedAtQuarter = 1000.0 * (1.0 - FMath::Exp(-2.0)) / 8.0;
	const double ExpectedTotal = (1000.0 - 5.0) / 8.0;
	const double StopTime = FMath::Loge(200.0) / 8.0;
	for (const double Sign : {1.0, -1.0})
	{
		// 0 stands for steps from FRandomStream(42) between 1/500 and 1/20 s.
		for (const float FrameRate : {30.0f, 60.0f, 240.0f, 0.0f})
		{
			const FString Name = FString::Printf(
				TEXT("%s, v0 %.0f"),
				FrameRate > 0.0f ? *FString::Printf(TEXT("%.0f fps"), FrameRate) : TEXT("random steps"),
				Sign * 1000.0);

			FReciInertiaMotion Quarter = MakeGlide(Sign * 1000.0);
			const FGlideRun QuarterRun = RunGlide(Quarter, FGlideSteps(FrameRate), 0.25);
			TestTrue(
				FString::Printf(TEXT("%s: a glide covers v0 (1 - e^-2) / 8 in 0.25 s"), *Name),
				FMath::IsNearlyEqual(QuarterRun.Displacement, Sign * ExpectedAtQuarter, 1e-3));
			TestTrue(
				FString::Printf(TEXT("%s: the glide still runs at 0.25 s"), *Name),
				Quarter.IsActive());

			FReciInertiaMotion Full = MakeGlide(Sign * 1000.0);
			const FGlideRun FullRun = RunGlide(Full, FGlideSteps(FrameRate), 10.0);
			TestFalse(
				FString::Printf(TEXT("%s: the glide ends"), *Name),
				Full.IsActive());
			TestTrue(
				FString::Printf(TEXT("%s: the glide covers (v0 - minimum) / 8 in all"), *Name),
				FMath::IsNearlyEqual(FullRun.Displacement, Sign * ExpectedTotal, 1e-3));
			TestTrue(
				FString::Printf(TEXT("%s: the glide ends in the step that reaches ln(200) / 8 s"), *Name),
				FullRun.Elapsed >= StopTime - 1e-6
					&& FullRun.Elapsed - FullRun.LastStep < StopTime);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FReciMotionInertiaEdgesTest,
	"ReciCommonUI.Motion.InertiaEdges",
	ReciMotionTestFlags)

bool FReciMotionInertiaEdgesTest::RunTest(const FString& Parameters)
{
	FReciInertiaMotion Slow;
	Slow.AddDragSample(40.0f, 10.0);
	TestFalse(TEXT("A release below the minimum speed returns false"), Slow.Release(0.0, InertiaMinimumSpeed));
	TestFalse(TEXT("A release below the minimum speed leaves no glide"), Slow.IsActive());

	FReciInertiaMotion Endless = MakeGlide(1000.0);
	TestTrue(
		TEXT("With no deceleration a glide moves v dt"),
		FMath::IsNearlyEqual(Endless.Advance(0.5f, 0.0f, InertiaMinimumSpeed), 500.0, 1e-9));
	TestTrue(TEXT("With no deceleration a glide keeps going"), Endless.IsActive());
	TestEqual(TEXT("With no deceleration a glide keeps its speed"), Endless.GetVelocity(), 1000.0);

	FReciInertiaMotion Floored;
	Floored.AddDragSample(10000.0f, 10.0);
	TestTrue(TEXT("A minimum speed of 0 still glides"), Floored.Release(0.0, 0.0f));
	double FlooredDisplacement = 0.0;
	double FlooredTime = 0.0;
	while (Floored.IsActive() && FlooredTime < 10.0)
	{
		FlooredDisplacement += Floored.Advance(1.0f / 60.0f, InertiaDeceleration, 0.0f);
		FlooredTime += 1.0f / 60.0f;
	}
	TestFalse(TEXT("A minimum speed of 0 still ends the glide"), Floored.IsActive());
	TestTrue(
		TEXT("A minimum speed of 0 ends the glide at the 0.01 floor"),
		FMath::IsNearlyEqual(
			FlooredDisplacement,
			(1000.0 - FReciInertiaMotion::MinimumSpeedFloor) / 8.0,
			1e-3));
	TestTrue(
		TEXT("A minimum speed of 0 ends the glide in finite time"),
		FlooredTime < FMath::Loge(1000.0 / FReciInertiaMotion::MinimumSpeedFloor) / 8.0 + 1.0 / 30.0);

	FReciInertiaMotion Invalid = MakeGlide(1000.0);
	TestEqual(
		TEXT("A NaN step moves nothing"),
		Invalid.Advance(std::numeric_limits<float>::quiet_NaN(), InertiaDeceleration, InertiaMinimumSpeed),
		0.0);
	TestEqual(
		TEXT("A negative step moves nothing"),
		Invalid.Advance(-0.1f, InertiaDeceleration, InertiaMinimumSpeed),
		0.0);
	TestTrue(TEXT("Invalid steps leave the velocity finite"), FMath::IsFinite(Invalid.GetVelocity()));
	TestEqual(TEXT("Invalid steps leave the velocity as it was"), Invalid.GetVelocity(), 1000.0);

	FReciInertiaMotion Stopped = MakeGlide(1000.0);
	Stopped.Stop();
	TestFalse(TEXT("Stop ends the glide"), Stopped.IsActive());
	TestEqual(
		TEXT("A stopped glide moves nothing"),
		Stopped.Advance(0.1f, InertiaDeceleration, InertiaMinimumSpeed),
		0.0);
	FReciInertiaMotion Forgotten = MakeGlide(1000.0);
	Forgotten.Reset();
	TestFalse(TEXT("Reset ends the glide"), Forgotten.IsActive());
	TestEqual(TEXT("Reset forgets the velocity"), Forgotten.GetVelocity(), 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FReciMotionDragVelocityTest,
	"ReciCommonUI.Motion.DragVelocity",
	ReciMotionTestFlags)

bool FReciMotionDragVelocityTest::RunTest(const FString& Parameters)
{
	const double Tau = FReciInertiaMotion::VelocitySmoothingTime;
	const double SteadyVelocity = 600.0 * (1.0 - FMath::Exp(-0.2 / Tau));
	for (const int32 SampleRate : {30, 60, 240})
	{
		// 600 units/s for 0.2 s.
		FReciInertiaMotion Drag;
		const double Interval = 1.0 / SampleRate;
		for (int32 Sample = 0; Sample < SampleRate / 5; ++Sample)
		{
			Drag.AddDragSample(static_cast<float>(600.0 * Interval), Interval);
		}
		TestTrue(
			FString::Printf(TEXT("%d Hz: a steady drag leaves 600 (1 - e^(-0.2 / tau))"), SampleRate),
			FMath::IsNearlyEqual(Drag.GetVelocity(), SteadyVelocity, 1e-3));
		TestFalse(TEXT("A drag is no glide"), Drag.IsActive());
	}

	FReciInertiaMotion OneSample;
	OneSample.AddDragSample(10.0f, 1.0 / 60.0);
	TestTrue(
		TEXT("One 60 Hz sample moves the velocity 35% of the way, as before"),
		FMath::IsNearlyEqual(OneSample.GetVelocity(), 0.35 * 600.0, 1e-4));

	FReciInertiaMotion Held = MakeSteadyDrag();
	TestFalse(TEXT("Holding still for 0.2 s before release leaves no glide"), Held.Release(0.2, InertiaMinimumSpeed));
	TestTrue(
		TEXT("Holding still for 0.2 s leaves about 3.4 units/s"),
		FMath::IsNearlyEqual(Held.GetVelocity(), SteadyVelocity * FMath::Exp(-0.2 / Tau), 1e-3)
			&& FMath::IsNearlyEqual(Held.GetVelocity(), 3.4, 0.01));
	FReciInertiaMotion Moving = MakeSteadyDrag();
	TestTrue(TEXT("A release while moving glides"), Moving.Release(0.0, InertiaMinimumSpeed));

	FReciInertiaMotion Reversed = MakeSteadyDrag();
	double ReversedTime = 0.0;
	while (ReversedTime < 3.0 * Tau)
	{
		Reversed.AddDragSample(-10.0f, 1.0 / 60.0);
		ReversedTime += 1.0 / 60.0;
	}
	TestTrue(
		TEXT("A reversed drag reverses the velocity within 3 tau"),
		Reversed.GetVelocity() < -0.85 * 600.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FReciMotionScaleTest,
	"ReciCommonUI.Motion.Scale",
	ReciMotionTestFlags)

bool FReciMotionScaleTest::RunTest(const FString& Parameters)
{
	const FReciMotionSettings Exponential =
		FReciMotionSettings::MakeExponential(4.0f, 0.0f);
	const FReciMotionSettings Linear =
		MakeDurationSettings(EReciMotionInterpolation::Linear);
	TestEqual(TEXT("Scale 1 keeps Speed"), Exponential.Scaled(1.0f).Speed, Exponential.Speed);
	TestEqual(TEXT("Scale 1 keeps Duration"), Linear.Scaled(1.0f).Duration, Linear.Duration);

	FReciFloatMotion Half;
	Half.SnapTo(0.0f);
	Half.Retarget(1.0f);
	Half.Advance(0.5f, Exponential.Scaled(0.5f));
	FReciFloatMotion Full;
	Full.SnapTo(0.0f);
	Full.Retarget(1.0f);
	Full.Advance(1.0f, Exponential);
	const float Expected = 1.0f - FMath::Exp(-4.0f);
	TestTrue(
		TEXT("Exponential motion at scale 0.5 gets as far in half the time"),
		FMath::IsNearlyEqual(Half.GetCurrentValue(), Expected, 1e-5f)
			&& FMath::IsNearlyEqual(Full.GetCurrentValue(), Expected, 1e-5f));

	FReciFloatMotion ShortLinear;
	ShortLinear.SnapTo(0.0f);
	ShortLinear.Retarget(1.0f);
	ShortLinear.Advance(0.25f, Linear.Scaled(0.5f));
	TestTrue(
		TEXT("A 1 s linear motion at scale 0.5 is halfway at 0.25 s"),
		FMath::IsNearlyEqual(ShortLinear.GetCurrentValue(), 0.5f, 1e-5f));
	ShortLinear.Advance(0.25f, Linear.Scaled(0.5f));
	TestEqual(TEXT("A 1 s linear motion at scale 0.5 completes at 0.5 s"), ShortLinear.GetCurrentValue(), 1.0f);
	TestFalse(TEXT("A 1 s linear motion at scale 0.5 is done at 0.5 s"), ShortLinear.IsActive());

	for (const EReciMotionInterpolation Interpolation : {
		EReciMotionInterpolation::Exponential,
		EReciMotionInterpolation::Linear,
		EReciMotionInterpolation::EaseIn,
		EReciMotionInterpolation::EaseOut,
		EReciMotionInterpolation::EaseInOut})
	{
		FReciMotionSettings Settings = MakeDurationSettings(Interpolation);
		Settings.Speed = 14.0f;
		FReciFloatMotion Off;
		Off.SnapTo(0.0f);
		Off.Retarget(100.0f);
		Off.Advance(0.0f, Settings.Scaled(0.0f));
		const FString Name = StaticEnum<EReciMotionInterpolation>()->GetNameStringByValue(static_cast<int64>(Interpolation));
		TestEqual(FString::Printf(TEXT("%s at scale 0 lands on its target at once"), *Name), Off.GetCurrentValue(), 100.0f);
		TestFalse(FString::Printf(TEXT("%s at scale 0 is done at once"), *Name), Off.IsActive());
	}

	FReciMotionSettings Both = Linear;
	Both.Speed = 14.0f;
	const FReciMotionSettings NotFinite = Both.Scaled(std::numeric_limits<float>::quiet_NaN());
	TestTrue(TEXT("A scale that is not finite counts as 1"), NotFinite.Speed == 14.0f && NotFinite.Duration == 1.0f);
	const FReciMotionSettings Negative = Both.Scaled(-1.0f);
	TestTrue(TEXT("A negative scale counts as 0"), Negative.Speed == 0.0f && Negative.Duration == 0.0f);
	const FReciMotionSettings Above = Both.Scaled(2.0f);
	TestTrue(TEXT("A scale above 1 counts as 1"), Above.Speed == 14.0f && Above.Duration == 1.0f);

	// A scale change mid-motion continues from the sampled value.
	FReciFloatMotion Changed;
	Changed.SnapTo(0.0f);
	Changed.Retarget(1.0f);
	Changed.Advance(0.25f, Linear);
	Changed.Advance(0.0f, Linear.Scaled(0.5f));
	TestEqual(TEXT("Changing the scale mid-motion does not move the value"), Changed.GetCurrentValue(), 0.25f);
	Changed.Advance(0.25f, Linear.Scaled(0.5f));
	TestTrue(
		TEXT("After a scale change the motion continues from where it was"),
		FMath::IsNearlyEqual(Changed.GetCurrentValue(), 0.625f, 1e-5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FReciMotionInertiaScaleTest,
	"ReciCommonUI.Motion.InertiaScale",
	ReciMotionTestFlags)

bool FReciMotionInertiaScaleTest::RunTest(const FString& Parameters)
{
	FReciInertiaMotion Half;
	Half.AddDragSample(10000.0f, 10.0);
	TestTrue(TEXT("At scale 0.5 a release glides"), Half.Release(0.0, InertiaMinimumSpeed, 0.5f));
	double Displacement = 0.0;
	double Elapsed = 0.0;
	double LastStep = 0.0;
	while (Half.IsActive() && Elapsed < 10.0)
	{
		LastStep = 1.0f / 60.0f;
		Displacement += Half.Advance(1.0f / 60.0f, InertiaDeceleration, InertiaMinimumSpeed, 0.5f);
		Elapsed += LastStep;
	}
	TestTrue(
		TEXT("Scale 0.5 halves the glide's distance"),
		FMath::IsNearlyEqual(Displacement, (1000.0 - 5.0) / 16.0, 1e-3));
	const double StopTime = FMath::Loge(200.0) / 16.0;
	TestTrue(
		TEXT("Scale 0.5 halves the glide's time"),
		Elapsed >= StopTime - 1e-6 && Elapsed - LastStep < StopTime);

	FReciInertiaMotion Off;
	Off.AddDragSample(10000.0f, 10.0);
	TestFalse(TEXT("At scale 0 a release does not glide"), Off.Release(0.0, InertiaMinimumSpeed, 0.0f));
	TestEqual(
		TEXT("At scale 0 nothing glides"),
		Off.Advance(1.0f / 60.0f, InertiaDeceleration, InertiaMinimumSpeed, 0.0f),
		0.0);

	FReciInertiaMotion TurnedOff = MakeGlide(1000.0);
	TestEqual(
		TEXT("A glide moves nothing once the scale is 0"),
		TurnedOff.Advance(1.0f / 60.0f, InertiaDeceleration, InertiaMinimumSpeed, 0.0f),
		0.0);
	TestFalse(TEXT("A glide ends once the scale is 0"), TurnedOff.IsActive());
	return true;
}

#endif
