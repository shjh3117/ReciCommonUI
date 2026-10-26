// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Widgets/ReciAdvancedListEntryHost.h"

#include "Blueprint/UserWidget.h"
#include "CommonButtonBase.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Widgets/ReciListBase.h"
#include "Widgets/ReciListItem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ReciAdvancedListEntryHost)

namespace
{
	constexpr float MainPresentationActivateBlend = 0.55f;
	constexpr float MainPresentationDeactivateBlend = 0.45f;

	FVector2D GetPresentationDesiredSize(const UUserWidget* Widget)
	{
		if (!Widget)
		{
			return FVector2D::ZeroVector;
		}

		const FVector2D DesiredSize = Widget->GetDesiredSize();
		return FVector2D(
			FMath::Max(DesiredSize.X, 0.0f),
			FMath::Max(DesiredSize.Y, 0.0f));
	}
}

bool UReciAdvancedListEntryHost::Initialize(
	UWidget& OwningWidget,
	UObject* ItemObject,
	const TSubclassOf<UUserWidget> NormalEntryClass,
	const TSubclassOf<UUserWidget> MainEntryClass,
	const TFunction<void(UUserWidget*)>& InitializeEntry)
{
	SetContent(nullptr);
	PresentationOverlay = nullptr;
	NormalEntryWidget = nullptr;
	MainEntryWidget = nullptr;
	NormalButton = nullptr;
	MainButton = nullptr;
	CachedNormalDesiredSize = FVector2D::ZeroVector;
	CachedMainDesiredSize = FVector2D::ZeroVector;
	bMainPresentationActive = false;
	bNormalPresentationFocusable = false;
	bMainPresentationFocusable = false;
	LastMainBlend = 0.0f;

	if (!ItemObject)
	{
		return false;
	}

	PresentationOverlay = NewObject<UOverlay>(this);
	if (!PresentationOverlay)
	{
		return false;
	}
	SetContent(PresentationOverlay);

	NormalEntryWidget = CreatePresentation(
		OwningWidget,
		ItemObject,
		NormalEntryClass,
		InitializeEntry);
	if (!NormalEntryWidget)
	{
		return false;
	}

	const bool bUsesSeparateMainPresentation = MainEntryClass != nullptr;
	if (bUsesSeparateMainPresentation)
	{
		MainEntryWidget = CreatePresentation(
			OwningWidget,
			ItemObject,
			MainEntryClass,
			InitializeEntry);
	}

	NormalButton = UReciListBase::FindEntryButton(
		NormalEntryWidget);
	MainButton = UReciListBase::FindEntryButton(
		MainEntryWidget);
	bNormalPresentationFocusable = NormalButton
		&& NormalButton->GetIsFocusable();
	bMainPresentationFocusable = MainButton
		&& MainButton->GetIsFocusable();

	SetMainPresentationActive(false);
	SetMainBlend(0.0f);
	RefreshPresentationSizes();
	return true;
}

bool UReciAdvancedListEntryHost::ApplyPresentation(
	const float MainBlend,
	FReciAdvancedPresentationChange& OutChange)
{
	OutChange = {};
	SetMainBlend(MainBlend);

	const bool bUseMain = HasMainPresentation()
		&& (IsMainPresentationActive()
			? LastMainBlend > MainPresentationDeactivateBlend
			: LastMainBlend >= MainPresentationActivateBlend);
	if (IsMainPresentationActive() == bUseMain)
	{
		return false;
	}

	OutChange.PreviousButton = GetActiveButton();
	OutChange.bPreviousButtonHadFocus = OutChange.PreviousButton
		&& (OutChange.PreviousButton->HasAnyUserFocus()
			|| OutChange.PreviousButton->HasFocusedDescendants());
	SetMainPresentationActive(bUseMain);
	OutChange.NewButton = GetActiveButton();
	return OutChange.PreviousButton != OutChange.NewButton;
}

void UReciAdvancedListEntryHost::RefreshPresentationSizes()
{
	CachedNormalDesiredSize = GetPresentationDesiredSize(NormalEntryWidget);
	CachedMainDesiredSize = HasMainPresentation()
		? GetPresentationDesiredSize(MainEntryWidget)
		: CachedNormalDesiredSize;

	if (CachedMainDesiredSize.IsNearlyZero())
	{
		CachedMainDesiredSize = CachedNormalDesiredSize;
	}
	ApplyBlendedDesiredSize();
}

void UReciAdvancedListEntryHost::SetFallbackDesiredHeight(
	const float InFallbackDesiredHeight)
{
	FallbackDesiredHeight = FMath::Max(InFallbackDesiredHeight, 0.0f);
	ApplyBlendedDesiredSize();
}

void UReciAdvancedListEntryHost::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	SetMainPresentationActive(bMainPresentationActive);
	SetMainBlend(LastMainBlend);
}

UUserWidget* UReciAdvancedListEntryHost::CreatePresentation(
	UWidget& OwningWidget,
	UObject* ItemObject,
	const TSubclassOf<UUserWidget> EntryClass,
	const TFunction<void(UUserWidget*)>& InitializeEntry)
{
	if (!EntryClass
		|| !ItemObject
		|| (!InitializeEntry && !EntryClass->ImplementsInterface(UReciListItem::StaticClass())))
	{
		return nullptr;
	}

	UUserWidget* EntryWidget = InitializeEntry
		? UUserWidget::CreateWidgetInstance(OwningWidget, EntryClass, NAME_None)
		: UReciListBase::CreateEntryForItem(
			OwningWidget,
			*ItemObject,
			EntryClass);
	if (!EntryWidget || !PresentationOverlay)
	{
		return nullptr;
	}
	if (InitializeEntry) InitializeEntry(EntryWidget);

	if (UOverlaySlot* OverlaySlot =
		PresentationOverlay->AddChildToOverlay(EntryWidget))
	{
		OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
		OverlaySlot->SetVerticalAlignment(VAlign_Center);
	}
	return EntryWidget;
}

void UReciAdvancedListEntryHost::SetMainPresentationActive(
	const bool bInMainPresentationActive)
{
	const bool bUseMainPresentation = bInMainPresentationActive
		&& HasMainPresentation();
	bMainPresentationActive = bUseMainPresentation;

	if (NormalEntryWidget)
	{
		NormalEntryWidget->SetVisibility(
			bMainPresentationActive && MainButton
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Visible);
	}
	if (MainEntryWidget)
	{
		MainEntryWidget->SetVisibility(
			bMainPresentationActive
				? (MainButton
					? ESlateVisibility::Visible
					: ESlateVisibility::HitTestInvisible)
				: ESlateVisibility::HitTestInvisible);
	}
	if (NormalButton)
	{
		const bool bMainOwnsFocus = bMainPresentationActive && MainButton;
		NormalButton->SetIsFocusable(
			!bMainOwnsFocus && bNormalPresentationFocusable);
	}
	if (MainButton)
	{
		MainButton->SetIsFocusable(
			bMainPresentationActive
				&& (bMainPresentationFocusable
					|| bNormalPresentationFocusable));
	}
}

void UReciAdvancedListEntryHost::SetMainBlend(const float InMainBlend)
{
	const float MainBlend = HasMainPresentation()
		? FMath::Clamp(InMainBlend, 0.0f, 1.0f)
		: 0.0f;
	LastMainBlend = MainBlend;
	if (NormalEntryWidget)
	{
		NormalEntryWidget->SetRenderOpacity(1.0f - MainBlend);
	}
	if (MainEntryWidget)
	{
		MainEntryWidget->SetRenderOpacity(MainBlend);
	}
	ApplyBlendedDesiredSize();
}

void UReciAdvancedListEntryHost::ApplyBlendedDesiredSize()
{
	FVector2D NormalSize = CachedNormalDesiredSize;
	if (NormalSize.Y <= KINDA_SMALL_NUMBER)
	{
		NormalSize.Y = FallbackDesiredHeight;
	}
	if (NormalSize.IsNearlyZero())
	{
		return;
	}

	const FVector2D MainSize = CachedMainDesiredSize.IsNearlyZero()
		? NormalSize
		: CachedMainDesiredSize;
	FVector2D ResolvedMainSize = MainSize;
	if (ResolvedMainSize.Y <= KINDA_SMALL_NUMBER)
	{
		ResolvedMainSize.Y = FallbackDesiredHeight;
	}
	const FVector2D BlendedSize = FMath::Lerp(
		NormalSize,
		ResolvedMainSize,
		LastMainBlend);

	if (BlendedSize.X > KINDA_SMALL_NUMBER)
	{
		SetWidthOverride(BlendedSize.X);
	}
	else
	{
		ClearWidthOverride();
	}
	if (BlendedSize.Y > KINDA_SMALL_NUMBER)
	{
		SetHeightOverride(BlendedSize.Y);
	}
	else
	{
		ClearHeightOverride();
	}
}

bool UReciAdvancedListEntryHost::IsMainPresentationActive() const
{
	return bMainPresentationActive;
}

bool UReciAdvancedListEntryHost::HasMainPresentation() const
{
	return MainEntryWidget != nullptr;
}

UCommonButtonBase* UReciAdvancedListEntryHost::GetActiveButton() const
{
	return bMainPresentationActive && MainButton
		? MainButton
		: NormalButton;
}

UCommonButtonBase* UReciAdvancedListEntryHost::GetNormalButton() const
{
	return NormalButton;
}

UCommonButtonBase* UReciAdvancedListEntryHost::GetMainButton() const
{
	return MainButton;
}

FVector2D UReciAdvancedListEntryHost::GetNormalDesiredSize() const
{
	return CachedNormalDesiredSize;
}

FVector2D UReciAdvancedListEntryHost::GetMainDesiredSize() const
{
	return CachedMainDesiredSize.IsNearlyZero()
		? CachedNormalDesiredSize
		: CachedMainDesiredSize;
}

FVector2D UReciAdvancedListEntryHost::GetMaximumDesiredSize() const
{
	const FVector2D MainSize = GetMainDesiredSize();
	return FVector2D(
		FMath::Max(CachedNormalDesiredSize.X, MainSize.X),
		FMath::Max(CachedNormalDesiredSize.Y, MainSize.Y));
}
