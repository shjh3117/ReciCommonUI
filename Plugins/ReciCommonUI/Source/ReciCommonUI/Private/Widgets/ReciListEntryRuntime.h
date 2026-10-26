// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "ReciListEntryRuntime.generated.h"

class UCommonButtonBase;
class UWidget;

/** GC-safe runtime record for one logical Reci list entry. */
UCLASS(Transient)
class UReciListEntryRuntime final : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UObject& InItemObject, UWidget& InEntryWidget)
	{
		ItemObject = &InItemObject;
		EntryWidget = &InEntryWidget;
	}

	UObject* GetItemObject() const
	{
		return ItemObject;
	}

	UWidget* GetEntryWidget() const
	{
		return EntryWidget;
	}

	UCommonButtonBase* GetSelectionButton() const
	{
		return SelectionButton;
	}

	void SetSelectionButton(UCommonButtonBase* InSelectionButton)
	{
		SelectionButton = InSelectionButton;
	}

private:
	UPROPERTY(Transient)
	TObjectPtr<UObject> ItemObject;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> EntryWidget;

	UPROPERTY(Transient)
	TObjectPtr<UCommonButtonBase> SelectionButton;
};
