// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Widgets/ReciWidgetTreeUtils.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "CommonActivatableWidget.h"
#include "CommonButtonBase.h"
#include "Components/PanelWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Framework/Application/SlateApplication.h"
#include "ReciReport.h"
#include "Widgets/Input/SInputKeySelector.h"
#include "Widgets/ReciListItem.h"
#include "Widgets/ReciListLayoutBox.h"
#include "Widgets/ReciListPanel.h"
#include "Widgets/ReciTextListItem.h"
#if WITH_EDITOR
#include "Editor/WidgetCompilerLog.h"
#endif

namespace UE::ReciCommonUI::Private
{
	namespace
	{
		UWidget* FindFocusableDescendantImpl(
			UWidget* Candidate,
			TSet<const UWidget*>& VisitedWidgets)
		{
			if (!Candidate || VisitedWidgets.Contains(Candidate))
			{
				return nullptr;
			}

			VisitedWidgets.Add(Candidate);
			if (UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(Candidate))
			{
				return FindFocusableDescendantImpl(
					Switcher->GetActiveWidget(),
					VisitedWidgets);
			}

			if (UUserWidget* UserWidget = Cast<UUserWidget>(Candidate))
			{
				if (UWidget* DesiredFocusWidget = UserWidget->GetDesiredFocusWidget();
					DesiredFocusWidget && DesiredFocusWidget != Candidate)
				{
					if (UWidget* Result = FindFocusableDescendantImpl(
						DesiredFocusWidget,
						VisitedWidgets))
					{
						return Result;
					}
				}

				if (UWidget* RootWidget = UserWidget->GetRootWidget();
					RootWidget && RootWidget != Candidate)
				{
					if (UWidget* Result = FindFocusableDescendantImpl(
						RootWidget,
						VisitedWidgets))
					{
						return Result;
					}
				}
			}

			const TSharedRef<SWidget> CandidateSlate = Candidate->TakeWidget();
			if (Candidate->GetIsEnabled()
				&& Candidate->IsVisible()
				&& CandidateSlate->SupportsKeyboardFocus())
			{
				return Candidate;
			}

			if (UPanelWidget* Panel = Cast<UPanelWidget>(Candidate))
			{
				for (int32 ChildIndex = 0;
					ChildIndex < Panel->GetChildrenCount();
					++ChildIndex)
				{
					if (UWidget* Result = FindFocusableDescendantImpl(
						Panel->GetChildAt(ChildIndex),
						VisitedWidgets))
					{
						return Result;
					}
				}
			}

			return nullptr;
		}

		UCommonButtonBase* FindCommonButtonDescendantImpl(
			UWidget* Candidate,
			TSet<const UWidget*>& VisitedWidgets)
		{
			if (!Candidate || VisitedWidgets.Contains(Candidate))
			{
				return nullptr;
			}

			VisitedWidgets.Add(Candidate);
			if (UCommonButtonBase* Button = Cast<UCommonButtonBase>(Candidate))
			{
				return Button;
			}

			if (UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(Candidate))
			{
				return FindCommonButtonDescendantImpl(
					Switcher->GetActiveWidget(),
					VisitedWidgets);
			}

			if (UUserWidget* UserWidget = Cast<UUserWidget>(Candidate))
			{
				if (UWidget* DesiredFocusWidget = UserWidget->GetDesiredFocusWidget();
					DesiredFocusWidget && DesiredFocusWidget != Candidate)
				{
					if (UCommonButtonBase* Result =
						FindCommonButtonDescendantImpl(
							DesiredFocusWidget,
							VisitedWidgets))
					{
						return Result;
					}
				}

				if (UWidget* RootWidget = UserWidget->GetRootWidget();
					RootWidget && RootWidget != Candidate)
				{
					if (UCommonButtonBase* Result =
						FindCommonButtonDescendantImpl(
							RootWidget,
							VisitedWidgets))
					{
						return Result;
					}
				}
			}

			if (UPanelWidget* Panel = Cast<UPanelWidget>(Candidate))
			{
				for (int32 ChildIndex = 0;
					ChildIndex < Panel->GetChildrenCount();
					++ChildIndex)
				{
					if (UCommonButtonBase* Result =
						FindCommonButtonDescendantImpl(
							Panel->GetChildAt(ChildIndex),
							VisitedWidgets))
					{
						return Result;
					}
				}
			}

			return nullptr;
		}

		const SInputKeySelector* AsKeySelector(const SWidget* Widget)
		{
			static const FName KeySelectorType(TEXT("SInputKeySelector"));
			return Widget && Widget->GetType() == KeySelectorType ? static_cast<const SInputKeySelector*>(Widget) : nullptr;
		}

		/** The key selector that holds the user's focus: the focused widget, or the parent of the button inside it. */
		const SInputKeySelector* FindFocusedKeySelector(const TSharedPtr<SWidget>& Focused)
		{
			if (!Focused)
			{
				return nullptr;
			}
			if (const SInputKeySelector* KeySelector = AsKeySelector(Focused.Get()))
			{
				return KeySelector;
			}
			const TSharedPtr<SWidget> Parent = Focused->GetParentWidget();
			return AsKeySelector(Parent.Get());
		}
	}

	UWidget* FindFocusableDescendant(UWidget* Candidate)
	{
		TSet<const UWidget*> VisitedWidgets;
		return FindFocusableDescendantImpl(Candidate, VisitedWidgets);
	}

	UCommonButtonBase* FindCommonButtonDescendant(UWidget* Candidate)
	{
		TSet<const UWidget*> VisitedWidgets;
		return FindCommonButtonDescendantImpl(Candidate, VisitedWidgets);
	}

	UUserWidget* CreateListEntryWidget(
		UWidget& OwningWidget,
		UObject& ItemObject,
		const TSubclassOf<UUserWidget> EntryWidgetClass)
	{
		if (!EntryWidgetClass
			|| !EntryWidgetClass->ImplementsInterface(
				UReciListItem::StaticClass()))
		{
			return nullptr;
		}

		UUserWidget* EntryWidget = UUserWidget::CreateWidgetInstance(
			OwningWidget,
			EntryWidgetClass,
			NAME_None);
		if (!EntryWidget)
		{
			return nullptr;
		}

		if (const UReciTextListItemData* TextItemData =
				Cast<UReciTextListItemData>(&ItemObject);
			TextItemData
			&& EntryWidget->GetClass()->ImplementsInterface(
				UReciTextListItem::StaticClass()))
		{
			IReciTextListItem::Execute_SetListItemText(
				EntryWidget,
				TextItemData->ItemText);
		}
		else
		{
			IReciListItem::Execute_SetListItemObject(
				EntryWidget,
				&ItemObject);
		}

		return EntryWidget;
	}

	bool TakesRawInput(const uint32 UserIndex)
	{
		return HasTextEditingFocus(UserIndex) || IsSelectingKey(UserIndex);
	}

	bool HasTextEditingFocus(const uint32 UserIndex)
	{
		static const FName EditableTextType(TEXT("SEditableText"));
		static const FName MultiLineEditableTextType(TEXT("SMultiLineEditableText"));
		const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetUserFocusedWidget(UserIndex);
		return Focused && (Focused->GetType() == EditableTextType || Focused->GetType() == MultiLineEditableTextType);
	}

	bool IsSelectingKey(const uint32 UserIndex)
	{
		const SInputKeySelector* KeySelector = FindFocusedKeySelector(FSlateApplication::Get().GetUserFocusedWidget(UserIndex));
		return KeySelector && KeySelector->GetIsSelectingKey();
	}

	bool HasKeySelectorFocus(const uint32 UserIndex)
	{
		return FindFocusedKeySelector(FSlateApplication::Get().GetUserFocusedWidget(UserIndex)) != nullptr;
	}

	bool IsEnabledKeySelector(const SWidget& Widget)
	{
		return AsKeySelector(&Widget) && Widget.IsEnabled();
	}

	namespace
	{
		FString DescribeActivatableEntry(const UWidget& List, const UClass& EntryClass)
		{
			return FString::Printf(TEXT("%s: %s derives from Common Activatable Widget. Reci takes a widget's nearest Common "
				"Activatable Widget as its screen, so a list entry must not be one."), *List.GetName(), *EntryClass.GetName());
		}

		FString DescribeHoldEntry(const UWidget& List, const UClass& EntryClass)
		{
			return FString::Printf(TEXT("%s: %s requires hold, but a Reci list confirms on Accept key-down and on a click it "
				"recognises; Requires Hold and Hold Data do not apply."), *List.GetName(), *EntryClass.GetName());
		}
	}

	void ReportEntryClass(const UWidget& List, const UClass* EntryClass)
	{
		if (EntryClass && EntryClass->IsChildOf(UCommonActivatableWidget::StaticClass()))
		{
			RECI_WARN_ONCE(TEXT("ActivatableEntry"), EntryClass->GetPathName(), TEXT("%s"), *DescribeActivatableEntry(List, *EntryClass));
		}
	}

	void ReportEntryButton(const UWidget& List, const UCommonButtonBase& Button)
	{
		if (Button.GetRequiresHold())
		{
			RECI_WARN_ONCE(TEXT("HoldEntry"), Button.GetClass()->GetPathName(), TEXT("%s"), *DescribeHoldEntry(List, *Button.GetClass()));
		}
	}


#if WITH_EDITOR
	const UWidgetTree* GetAuthoredWidgetTree(const UClass* WidgetClass)
	{
		const UWidgetBlueprintGeneratedClass* BlueprintClass = Cast<UWidgetBlueprintGeneratedClass>(WidgetClass);
		const UWidgetBlueprintGeneratedClass* TreeOwner = BlueprintClass ? BlueprintClass->FindWidgetTreeOwningClass() : nullptr;
		return TreeOwner ? TreeOwner->GetWidgetTreeArchetype() : nullptr;
	}

	namespace
	{
		const UCommonButtonBase* FindAuthoredButton(const UWidget* Widget, const int32 Depth)
		{
			if (!Widget || Depth > 8)
			{
				return nullptr;
			}
			if (const UCommonButtonBase* Button = Cast<UCommonButtonBase>(Widget))
			{
				return Button;
			}
			if (const UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(Widget))
			{
				return FindAuthoredButton(Switcher->GetWidgetAtIndex(Switcher->GetActiveWidgetIndex()), Depth + 1);
			}
			if (Widget->IsA<UUserWidget>())
			{
				// A template in an authored tree has no tree of its own; its class's Blueprint has.
				const UWidgetTree* Tree = GetAuthoredWidgetTree(Widget->GetClass());
				return Tree ? FindAuthoredButton(Tree->RootWidget, Depth + 1) : nullptr;
			}
			if (const UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
			{
				for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
				{
					if (const UCommonButtonBase* Button = FindAuthoredButton(Panel->GetChildAt(Index), Depth + 1))
					{
						return Button;
					}
				}
			}
			return nullptr;
		}
	}

	const UCommonButtonBase* FindAuthoredEntryButton(const UClass* EntryClass)
	{
		if (!EntryClass)
		{
			return nullptr;
		}
		if (const UCommonButtonBase* Button = Cast<UCommonButtonBase>(EntryClass->GetDefaultObject()))
		{
			return Button;
		}
		const UWidgetTree* Tree = GetAuthoredWidgetTree(EntryClass);
		return Tree ? FindAuthoredButton(Tree->RootWidget, 0) : nullptr;
	}

	void ValidateEntryClass(const UWidget& List, const UClass* EntryClass, IWidgetCompilerLog& CompileLog)
	{
		if (!EntryClass)
		{
			return;
		}
		if (EntryClass->IsChildOf(UCommonActivatableWidget::StaticClass()))
		{
			CompileLog.Warning(FText::FromString(DescribeActivatableEntry(List, *EntryClass)));
		}
		const UCommonButtonBase* Button = FindAuthoredEntryButton(EntryClass);
		if (Button && Button->GetRequiresHold())
		{
			CompileLog.Warning(FText::FromString(DescribeHoldEntry(List, *EntryClass)));
		}
	}

	void ValidateHostedItemsBox(const UWidget& List, const UWidgetTree& ListTree, IWidgetCompilerLog& CompileLog)
	{
		const UReciListLayoutBoxBase* Layout = Cast<UReciListLayoutBoxBase>(List.GetParent());
		const UReciListPanel* Panel = Cast<UReciListPanel>(ListTree.FindWidget(TEXT("ItemsBox")));
		if (Layout && Panel && Panel->HasAuthoredLayoutSettings())
		{
			CompileLog.Warning(FText::FromString(FString::Printf(
				TEXT("%s: the Layout Settings set on the ItemsBox of %s have no effect, since its Reci layout box %s lays out the "
					"entries."), *List.GetName(), *List.GetClass()->GetName(), *Layout->GetName())));
		}
	}
#endif
}
