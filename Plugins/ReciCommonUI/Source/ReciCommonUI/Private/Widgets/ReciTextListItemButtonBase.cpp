// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Widgets/ReciTextListItemButtonBase.h"

#include "Components/TextBlock.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ReciTextListItemButtonBase)

void UReciTextListItemButtonBase::SetListItemText_Implementation(
	const FText& InItemText)
{
	ItemText = InItemText;
	RefreshItemText();
}

void UReciTextListItemButtonBase::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	RefreshItemText();
}

void UReciTextListItemButtonBase::RefreshItemText()
{
	if (ItemTextBlock)
	{
		ItemTextBlock->SetText(ItemText);
	}
}
