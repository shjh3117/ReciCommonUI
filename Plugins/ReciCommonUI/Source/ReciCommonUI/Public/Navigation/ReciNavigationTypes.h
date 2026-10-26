// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"

#include "ReciNavigationTypes.generated.h"

/** Cardinal directions supported by Reci navigation. */
UENUM(BlueprintType)
enum class EReciNavigationDirection : uint8
{
	Up,
	Down,
	Left,
	Right
};
