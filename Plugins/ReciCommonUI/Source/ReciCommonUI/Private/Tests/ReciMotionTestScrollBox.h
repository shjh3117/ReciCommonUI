// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "Widgets/ReciScrollBox.h"

#include "ReciMotionTestScrollBox.generated.h"

/** A Reci Scroll Box that tells a test whether its routed motion (Scroll Motion or a glide) is running. */
UCLASS(Hidden, NotBlueprintable, Transient)
class UReciMotionTestScrollBox : public UReciScrollBox
{
	GENERATED_BODY()

public:
	bool IsRoutedMotionRunning() const { return IsRoutedScrollMotionActive(); }
};
