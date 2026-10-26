// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.
#pragma once
#include "UObject/Object.h"
#include "ReciDemoTestComposer.generated.h"

/**
 * Stands in for the demo's composer (Content/Demo/WBP_DemoComposer) in a test: the screen's and the dialog's event
 * dispatchers are bound to these handlers, which run what the test gives them.
 */
UCLASS(Hidden, NotBlueprintable, Transient)
class UReciDemoTestComposer : public UObject
{
	GENERATED_BODY()
public:
	TFunction<void()> OnDialogRequested;
	TFunction<void(bool bConfirmed)> OnDialogClosed;

	UFUNCTION() void HandleDialogRequested() { if (OnDialogRequested) { OnDialogRequested(); } }
	UFUNCTION() void HandleDialogClosed(bool bConfirmed) { if (OnDialogClosed) { OnDialogClosed(bConfirmed); } }
};
