// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"
#include "ReciReport.h"

class SWidget;
class UCommonButtonBase;
class UUserWidget;
class UWidget;
class UWidgetTree;
#if WITH_EDITOR
class IWidgetCompilerLog;
#endif

namespace UE::ReciCommonUI::Private
{
	/** Resolves the preferred focus target inside a UMG entry tree. */
	UWidget* FindFocusableDescendant(UWidget* Candidate);

	/** Resolves the CommonUI selection button inside a UMG entry tree. */
	UCommonButtonBase* FindCommonButtonDescendant(UWidget* Candidate);

	/**
	 * Creates one list-entry widget and assigns the supplied object through the
	 * appropriate Reci list-item interface.
	 */
	UUserWidget* CreateListEntryWidget(
		UWidget& OwningWidget,
		UObject& ItemObject,
		TSubclassOf<UUserWidget> EntryWidgetClass);

	/**
	 * Whether the user's focused widget takes raw input itself, keys and mouse buttons as they are: a text field, or a
	 * stock key selector that is selecting a key. Reci's key, stick and pointer routing steps aside meanwhile. The stock
	 * widgets are recognised by their Slate type.
	 */
	bool TakesRawInput(uint32 UserIndex);

	/** Whether the user's focused widget is a text field (recognised by its Slate type), which takes keys as typing. */
	bool HasTextEditingFocus(uint32 UserIndex);

	/** Whether the user's focus is on a stock key selector that is selecting a key, which takes any key. */
	bool IsSelectingKey(uint32 UserIndex);

	/** Whether the user's focus is on a stock key selector, or on the button inside it, selecting a key or not. */
	bool HasKeySelectorFocus(uint32 UserIndex);

	/** Whether the Slate widget is an enabled stock key selector, which takes a click itself to start selecting a key. */
	bool IsEnabledKeySelector(const SWidget& Widget);

	/**
	 * Reports once per class, to LogReciCommonUI, what a Reci list cannot serve: an entry class deriving from
	 * CommonActivatableWidget (Reci takes a widget's nearest activatable widget as its screen, so an entry must not be
	 * one), and an entry button that requires CommonUI hold, which a Reci list's Accept and recognised clicks bypass.
	 */
	void ReportEntryClass(const UWidget& List, const UClass* EntryClass);
	void ReportEntryButton(const UWidget& List, const UCommonButtonBase& Button);

#if WITH_EDITOR
	/**
	 * The tree a widget class's Blueprint authors (its own, or the nearest parent Blueprint's), as the Blueprint compiler
	 * left it; null for a native class.
	 */
	const UWidgetTree* GetAuthoredWidgetTree(const UClass* WidgetClass);

	/**
	 * The CommonUI button an entry of this class shows, read from the class alone: its default object when the class is
	 * a button, else the first button in its authored tree (through the authored trees of the user widgets in it).
	 */
	const UCommonButtonBase* FindAuthoredEntryButton(const UClass* EntryClass);

	/** Compile-time form of ReportEntryClass and ReportEntryButton, for a list template the Blueprint compiler checks. */
	void ValidateEntryClass(const UWidget& List, const UClass* EntryClass, IWidgetCompilerLog& CompileLog);

	/**
	 * Warns about Layout Settings that ListTree, the tree the list's class authors, sets on its ItemsBox while a Reci layout
	 * box hosts the list: the box owns the layout and replaces them.
	 */
	void ValidateHostedItemsBox(const UWidget& List, const UWidgetTree& ListTree, IWidgetCompilerLog& CompileLog);
#endif
}
