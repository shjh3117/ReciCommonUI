// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"

class UReciUIInputData;

namespace UE::ReciCommonUI::Private
{
	/**
	 * The defaults of the Input Data class CommonUI loaded (Project Settings > Game > Common Input Settings), when that
	 * class is a Reci UI Input Data; null otherwise, whether or not CommonUI's Enhanced Input support is on.
	 */
	const UReciUIInputData* GetReciUIInputData();

	/**
	 * The Reci UI Input Data defaults Reci follows: GetReciUIInputData() while CommonUI's Enhanced Input support is on,
	 * else null. Reci follows Enhanced Input exactly when this is set: the keys of its four navigation actions become
	 * directions, and the keys of CommonUI's Enhanced Input click and Back actions become Accept and Back.
	 */
	const UReciUIInputData* GetFollowedInputData();
}
