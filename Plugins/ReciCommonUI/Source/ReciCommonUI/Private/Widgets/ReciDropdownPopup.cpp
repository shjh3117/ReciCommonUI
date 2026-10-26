// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Widgets/ReciDropdownPopup.h"

#include "Blueprint/WidgetTree.h"
#include "Widgets/ReciList.h"
#include "Widgets/ReciListLayoutBox.h"
#if WITH_EDITOR
#include "Editor/WidgetCompilerLog.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ReciDropdownPopup)

#if WITH_EDITOR
void UReciDropdownPopup::ValidateCompiledWidgetTree(const UWidgetTree& BlueprintWidgetTree, IWidgetCompilerLog& CompileLog) const
{
	Super::ValidateCompiledWidgetTree(BlueprintWidgetTree, CompileLog);
	// A missing or mistyped Options is the BindWidget check's to report.
	const UReciList* List = Cast<UReciList>(BlueprintWidgetTree.FindWidget(GET_MEMBER_NAME_CHECKED(UReciDropdownPopup, Options)));
	if (!List)
	{
		return;
	}
	if (!Cast<UReciListLayoutBoxBase>(List->GetParent()))
	{
		CompileLog.Error(FText::FromString(TEXT("Options must be the content of a Reci Vertical Box, Reci Horizontal Box or "
			"Reci Tile Box: the list's navigation, Accept, pointer and wheel input need that host.")));
	}
	if (List->SelectionMode != ESelectionMode::Single)
	{
		CompileLog.Warning(FText::FromString(TEXT("Options: the dropdown sets Selection Mode to Single while the popup is "
			"open, so the Selection Mode set here does not apply.")));
	}
}
#endif
