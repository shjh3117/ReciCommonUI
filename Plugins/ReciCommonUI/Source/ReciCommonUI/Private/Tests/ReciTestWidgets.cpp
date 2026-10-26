// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Tests/ReciTestWidgets.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/MenuAnchor.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Interaction/ReciInputFocusCoordinator.h"
#include "Widgets/ReciListLayoutBox.h"
#include "Widgets/ReciListPanel.h"
#include "Widgets/ReciMappedKeyText.h"
#include "Widgets/ReciScrollBox.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ReciTestWidgets)

bool UReciTestButton::Initialize()
{
	// The tree a Widget Blueprint would author, in place before CommonUI wraps it in its internal button.
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (!WidgetTree->RootWidget)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Bounds"));
		Box->SetWidthOverride(60.0f);
		Box->SetHeightOverride(40.0f);
		WidgetTree->RootWidget = Box;
		BuildContent(*WidgetTree, *Box);
	}
	return Super::Initialize();
}

UReciMappedKeyText* UReciTestHintedButton::GetKeyText() const
{
	return Cast<UReciMappedKeyText>(InputActionWidget);
}

void UReciTestHintedButton::BuildContent(UWidgetTree& Tree, USizeBox& Box)
{
	UReciMappedKeyText* KeyText = Tree.ConstructWidget<UReciMappedKeyText>(UReciMappedKeyText::StaticClass(), TEXT("InputActionWidget"));
	// As a button's hint should be: an action with no keys shows nothing.
	KeyText->bHideWhenUnmapped = true;
	Box.SetContent(KeyText);
	// A Widget Blueprint's class binds it by name; a native tree assigns it.
	InputActionWidget = KeyText;
}

UReciMappedKeyText* UReciTestBoundActionButton::GetKeyText() const
{
	return Cast<UReciMappedKeyText>(InputActionWidget);
}

bool UReciTestBoundActionButton::Initialize()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (!WidgetTree->RootWidget)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Bounds"));
		Box->SetWidthOverride(60.0f);
		Box->SetHeightOverride(40.0f);
		UReciMappedKeyText* KeyText =
			WidgetTree->ConstructWidget<UReciMappedKeyText>(UReciMappedKeyText::StaticClass(), TEXT("InputActionWidget"));
		KeyText->bHideWhenUnmapped = true;
		Box->SetContent(KeyText);
		WidgetTree->RootWidget = Box;
		// A Widget Blueprint's class binds it by name; a native tree assigns it.
		InputActionWidget = KeyText;
	}
	return Super::Initialize();
}

UReciTestHoldEntry::UReciTestHoldEntry(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRequiresHold = true;
}

bool UReciTestRootedList::Initialize()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (!WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Bounds"));
	}
	return Super::Initialize();
}

FReciListLayoutSettings UReciTestLayoutList::GetAuthoredLayout()
{
	FReciListLayoutSettings Settings;
	Settings.Mode = EReciListLayoutMode::Horizontal;
	Settings.ItemSpacing = 7.0f;
	return Settings;
}

bool UReciTestLayoutList::Initialize()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (!WidgetTree->RootWidget)
	{
		ItemsBox = WidgetTree->ConstructWidget<UReciListPanel>(UReciListPanel::StaticClass(), TEXT("ItemsBox"));
		// What a Blueprint sets in the panel's Details.
		ItemsBox->ApplyLayoutSettings(GetAuthoredLayout());
		WidgetTree->RootWidget = ItemsBox;
	}
	return Super::Initialize();
}

bool UReciTestDropdown::Initialize()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (!WidgetTree->RootWidget)
	{
		DropdownAnchor = WidgetTree->ConstructWidget<UMenuAnchor>(UMenuAnchor::StaticClass(), TEXT("DropdownAnchor"));
		USizeBox* Field = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Field"));
		Field->SetWidthOverride(120.0f);
		Field->SetHeightOverride(40.0f);
		SelectedValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SelectedValueText"));
		Field->SetContent(SelectedValueText);
		DropdownAnchor->SetContent(Field);
		WidgetTree->RootWidget = DropdownAnchor;
	}
	return Super::Initialize();
}

bool UReciTestDropdownPopup::Initialize()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (!WidgetTree->RootWidget)
	{
		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel"));
		UReciScrollBox* Scroll = WidgetTree->ConstructWidget<UReciScrollBox>(UReciScrollBox::StaticClass(), TEXT("OptionScroll"));
		UReciVerticalBox* Layout = WidgetTree->ConstructWidget<UReciVerticalBox>(UReciVerticalBox::StaticClass(), TEXT("OptionLayout"));
		Options = WidgetTree->ConstructWidget<UReciList>(UReciList::StaticClass(), TEXT("Options"));
		Options->ItemWidgetClass = UReciTestTextEntry::StaticClass();
		Layout->SetContent(Options);
		Scroll->AddChild(Layout);
		Panel->SetContent(Scroll);
		WidgetTree->RootWidget = Panel;
	}
	return Super::Initialize();
}

bool UReciTestTextItemButton::Initialize()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (!WidgetTree->RootWidget)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Bounds"));
		Box->SetWidthOverride(60.0f);
		Box->SetHeightOverride(40.0f);
		// A Widget Blueprint's class binds ItemTextBlock by name; a native tree assigns it.
		ItemTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ItemTextBlock"));
		Box->SetContent(ItemTextBlock);
		WidgetTree->RootWidget = Box;
	}
	return Super::Initialize();
}

void UReciTestEventRecorder::RecordItem(UObject* Item, const int32 ItemIndex)
{
	Events.Add(FString::Printf(TEXT("Item %d"), ItemIndex));
}

void UReciTestEventRecorder::RecordSelected(UObject* Item)
{
	Events.Add(TEXT("Selected ") + GetNameSafe(Item));
}

void UReciTestEventRecorder::RecordDropdownSelection(UEnum* SelectedEnum, const FName SelectedValue, const int32 SelectedIndex)
{
	Events.Add(FString::Printf(TEXT("Selection %d %s"), SelectedIndex, *SelectedValue.ToString()));
}

void UReciTestEventRecorder::RecordOpen(const bool bIsOpen)
{
	Events.Add(bIsOpen ? TEXT("Open") : TEXT("Closed"));
}

void UReciTestEventRecorder::RecordNavigation(const EReciNavigationDirection Direction)
{
	Events.Add(TEXT("Navigation ") + StaticEnum<EReciNavigationDirection>()->GetNameStringByValue(static_cast<int64>(Direction)));
}

void UReciTestRegisteringScreen::NativeOnActivated()
{
	Super::NativeOnActivated();
	FReciInputFocusCoordinator::Get().RegisterScreenInput(this);
}

void UReciTestStateEntry::SetListItemObject_Implementation(UObject* Item)
{
	Super::SetListItemObject_Implementation(Item);
	if (const UReciTestStateItem* State = Cast<UReciTestStateItem>(Item))
	{
		if (State->bDisabled)
		{
			SetIsEnabled(false);
		}
		if (State->bHidden)
		{
			SetVisibility(ESlateVisibility::Hidden);
		}
	}
}
