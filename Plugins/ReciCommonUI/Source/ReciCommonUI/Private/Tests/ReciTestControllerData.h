// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CommonInputBaseTypes.h"
#include "ReciTestControllerData.generated.h"

/** CommonUI controller data that a key-text test fills and adds to the platform's Common Input settings while it runs. */
UCLASS(Hidden, NotBlueprintable, Transient)
class UReciTestControllerData : public UCommonInputBaseControllerData
{
	GENERATED_BODY()
};
