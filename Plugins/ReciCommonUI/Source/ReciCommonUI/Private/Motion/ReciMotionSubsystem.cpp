// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Motion/ReciMotionSubsystem.h"

#include "Components/Widget.h"
#include "Engine/LocalPlayer.h"
#include "ReciReport.h"
#include "Settings/ReciCommonUISettings.h"

bool UReciMotionSubsystem::SetMotionScale(const float Scale)
{
	if (!FMath::IsFinite(Scale) || Scale < 0.0f || Scale > 1.0f)
	{
		RECI_WARN_ONCE(TEXT("MotionScaleRejected"), GetClass()->GetPathName(),
			TEXT("Set Motion Scale was called with %f, but a motion scale is from 0 (off) to 1 (full motion). The "
				"player's motion scale stays %f."),
			Scale, GetMotionScale());
		return false;
	}
	PlayerMotionScale = Scale;
	return true;
}

float UReciMotionSubsystem::GetMotionScale() const
{
	return PlayerMotionScale.IsSet()
		? PlayerMotionScale.GetValue()
		: GetDefaultMotionScale();
}

float UReciMotionSubsystem::GetMotionScaleForWidget(const UWidget* Widget)
{
	if (const ULocalPlayer* LocalPlayer = Widget ? Widget->GetOwningLocalPlayer() : nullptr)
	{
		if (const UReciMotionSubsystem* Subsystem = LocalPlayer->GetSubsystem<UReciMotionSubsystem>())
		{
			return Subsystem->GetMotionScale();
		}
	}
	return GetDefaultMotionScale();
}

float UReciMotionSubsystem::GetDefaultMotionScale()
{
	const UReciCommonUISettings* Settings = GetDefault<UReciCommonUISettings>();
	const float Scale = Settings->DefaultMotionScale;
	if (!FMath::IsFinite(Scale) || Scale < 0.0f || Scale > 1.0f)
	{
		RECI_WARN_ONCE(TEXT("DefaultMotionScaleOutOfRange"), Settings->GetClass()->GetPathName(),
			TEXT("Reci Common UI's Default Motion Scale is %f, but a motion scale is from 0 (off) to 1 (full motion). "
				"Reci uses %f. Set it in Project Settings > Plugins > Reci Common UI > Motion."),
			Scale, FMath::IsFinite(Scale) ? FMath::Clamp(Scale, 0.0f, 1.0f) : 1.0f);
	}
	return FMath::IsFinite(Scale)
		? FMath::Clamp(Scale, 0.0f, 1.0f)
		: 1.0f;
}
