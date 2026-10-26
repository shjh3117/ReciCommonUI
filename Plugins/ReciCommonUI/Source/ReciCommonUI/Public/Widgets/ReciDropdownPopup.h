// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CommonUserWidget.h"
#include "CoreMinimal.h"

#include "ReciDropdownPopup.generated.h"

class UReciList;

/**
 * The popup a Reci Dropdown opens.
 *
 * A Widget Blueprint derived from this class supplies the popup's presentation (panel, scrolling, size) and holds the
 * option list: a Reci List named Options (BindWidget). Options is the content of a Reci Vertical Box, Reci Horizontal
 * Box or Reci Tile Box, so navigation, Accept, pointer and wheel input follow the list rules; the Blueprint compiler
 * reports any other placement. While the popup is open the dropdown sets Options' items and its Selection Mode (Single),
 * and its Entry Widget Class when the dropdown has a Dropdown Entry Widget Class.
 */
UCLASS(
	Abstract,
	Blueprintable,
	ClassGroup = UI,
	meta = (
		DisplayName = "Reci Dropdown Popup",
		Category = "Reci Common UI"))
class RECICOMMONUI_API UReciDropdownPopup : public UCommonUserWidget
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	/**
	 * Reports, at Blueprint compile time, an Options list that is not the content of a Reci layout box (an error) or
	 * whose Selection Mode is not the Single the dropdown sets (a warning).
	 */
	virtual void ValidateCompiledWidgetTree(const UWidgetTree& BlueprintWidgetTree, IWidgetCompilerLog& CompileLog) const override;
#endif

protected:
	/** The option list, the content of a Reci Vertical Box, Reci Horizontal Box or Reci Tile Box. */
	UPROPERTY(BlueprintReadOnly, Category = "Dropdown", meta = (BindWidget))
	TObjectPtr<UReciList> Options;

private:
	friend class UReciDropdownBase;
};
