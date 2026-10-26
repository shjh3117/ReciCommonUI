// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Widgets/ReciListLayoutBox.h"

#include "CommonButtonBase.h"
#include "Components/PanelSlot.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateUser.h"
#include "Input/Events.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/ReciListBase.h"
#include "Widgets/ReciScrollBox.h"
#include "Widgets/SNullWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ReciListLayoutBox)

#define LOCTEXT_NAMESPACE "ReciCommonUI"

namespace
{
	class SReciListLayoutContentBox final : public SBox
	{
	public:
		void SetFocusReceivedHandler(
			TFunction<FReply(const FFocusEvent&)> InHandler)
		{
			FocusReceivedHandler = MoveTemp(InHandler);
		}

		virtual bool SupportsKeyboardFocus() const override
		{
			return true;
		}

		virtual FReply OnFocusReceived(
			const FGeometry& MyGeometry,
			const FFocusEvent& InFocusEvent) override
		{
			return FocusReceivedHandler
				? FocusReceivedHandler(InFocusEvent)
				: FReply::Unhandled();
		}

	private:
		TFunction<FReply(const FFocusEvent&)> FocusReceivedHandler;
	};
}

UReciListLayoutBoxBase::UReciListLayoutBoxBase(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = true;
	SetVisibilityInternal(ESlateVisibility::SelfHitTestInvisible);
}

void UReciListLayoutBoxBase::SetItemSpacing(const float InItemSpacing)
{
	const float ResolvedSpacing = FMath::IsFinite(InItemSpacing)
		? FMath::Max(InItemSpacing, 0.0f)
		: 0.0f;
	if (FMath::IsNearlyEqual(ItemSpacing, ResolvedSpacing))
	{
		return;
	}

	ItemSpacing = ResolvedSpacing;
	ApplyHostedLayout();
}

UReciListBase* UReciListLayoutBoxBase::GetListSource() const
{
	return Cast<UReciListBase>(GetContent());
}

bool UReciListLayoutBoxBase::AllowsPointerHover() const
{
	const UReciListBase* Source = GetListSource();
	return Source && Source->IsPointerHoverAllowed();
}

void UReciListLayoutBoxBase::SetInputRoutingEnabled(const bool bEnabled)
{
	bInputRoutingEnabled = bEnabled;
	UpdateInputTargetRegistration();
}

FReciListLayoutSettings
UReciListLayoutBoxBase::GetHostedLayoutSettings() const
{
	return BuildLayoutSettings();
}

void UReciListLayoutBoxBase::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	ItemSpacing = FMath::IsFinite(ItemSpacing)
		? FMath::Max(ItemSpacing, 0.0f)
		: 0.0f;
	BindListSource();
	ApplyHostedLayout();
	UpdateInputTargetRegistration();
}

void UReciListLayoutBoxBase::ReleaseSlateResources(
	const bool bReleaseChildren)
{
	FReciInputFocusCoordinator::Get().UnregisterTarget(this);
	bInputTargetRegistered = false;
	UnbindListSource();
	Super::ReleaseSlateResources(bReleaseChildren);
	MyContentBox.Reset();
}

TSharedRef<SWidget> UReciListLayoutBoxBase::RebuildWidget()
{
	TSharedRef<SReciListLayoutContentBox> ContentBox =
		SNew(SReciListLayoutContentBox);
	ContentBox->SetFocusReceivedHandler(
		[this](const FFocusEvent& FocusEvent)
		{
			return HandleSlateFocusReceived(FocusEvent);
		});
	MyContentBox = ContentBox;
	RefreshSlateContent();
	BindListSource();
	ApplyHostedLayout();
	UpdateInputTargetRegistration();
	return MyContentBox.ToSharedRef();
}

void UReciListLayoutBoxBase::OnSlotAdded(UPanelSlot* InSlot)
{
	Super::OnSlotAdded(InSlot);
	RefreshSlateContent();
	BindListSource();
	ApplyHostedLayout();
	UpdateInputTargetRegistration();
}

void UReciListLayoutBoxBase::OnSlotRemoved(UPanelSlot* InSlot)
{
	UnbindListSource();
	Super::OnSlotRemoved(InSlot);
	RefreshSlateContent();
	UpdateInputTargetRegistration();
}

void UReciListLayoutBoxBase::BindListSource()
{
	UReciListBase* NewListSource = GetListSource();
	if (BoundListSource == NewListSource)
	{
		return;
	}

	UnbindListSource();
	BoundListSource = NewListSource;
	if (BoundListSource)
	{
		BoundListSource->OnEntriesChangedNative().AddUObject(
			this,
			&UReciListLayoutBoxBase::HandleEntriesChanged);
		BoundListSource->OnFocusedIndexChangedNative().AddUObject(
			this,
			&UReciListLayoutBoxBase::HandleFocusedIndexChanged);
		BoundListSource->OnDrivenIndexChangedNative().AddUObject(
			this,
			&UReciListLayoutBoxBase::HandleDrivenIndexChanged);
		BoundListSource->SetItemEmphasisDriven(
			AttachedViewport
				&& AttachedViewport->DrivesItemEmphasis());
	}
}

void UReciListLayoutBoxBase::UnbindListSource()
{
	if (BoundListSource)
	{
		BoundListSource->OnEntriesChangedNative().RemoveAll(this);
		BoundListSource->OnFocusedIndexChangedNative().RemoveAll(this);
		BoundListSource->OnDrivenIndexChangedNative().RemoveAll(this);
		BoundListSource->SetItemEmphasisDriven(false);
		BoundListSource = nullptr;
	}
}

void UReciListLayoutBoxBase::ApplyHostedLayout()
{
	if (!BoundListSource)
	{
		BindListSource();
	}
	if (!BoundListSource)
	{
		return;
	}

	BoundListSource->ApplyHostedLayoutSettings(BuildLayoutSettings());
	BoundListSource->InvalidateLayoutAndVolatility();
	InvalidateLayoutAndVolatility();
}

void UReciListLayoutBoxBase::HandleEntriesChanged()
{
	ApplyHostedLayout();
	if (AttachedViewport)
	{
		AttachedViewport->NotifyHostedEntriesChanged();
	}
}

void UReciListLayoutBoxBase::HandleFocusedIndexChanged(
	const int32 ItemIndex)
{
	if (AttachedViewport)
	{
		AttachedViewport->NotifyHostedFocusChanged(ItemIndex);
	}
}

void UReciListLayoutBoxBase::HandleDrivenIndexChanged(
	const int32 ItemIndex)
{
	if (AttachedViewport)
	{
		AttachedViewport->NotifyHostedDrivenItemChanged(ItemIndex);
	}
}

void UReciListLayoutBoxBase::RefreshSlateContent()
{
	if (!MyContentBox.IsValid())
	{
		return;
	}

	UWidget* ContentWidget = GetContent();
	MyContentBox->SetContent(ContentWidget
		? ContentWidget->TakeWidget()
		: SNullWidget::NullWidget);
}

int32 UReciListLayoutBoxBase::GetHostedItemCount() const
{
	const UReciListBase* Source = GetListSource();
	return Source ? Source->GetHostedItemCount() : 0;
}

UWidget* UReciListLayoutBoxBase::GetHostedEntryWidget(
	const int32 ItemIndex) const
{
	const UReciListBase* Source = GetListSource();
	return Source ? Source->GetHostedEntryWidget(ItemIndex) : nullptr;
}

int32 UReciListLayoutBoxBase::FindHostedItemIndexAtScreenPosition(
	const FVector2D& ScreenPosition) const
{
	for (int32 ItemIndex = GetHostedItemCount() - 1;
		ItemIndex >= 0;
		--ItemIndex)
	{
		UWidget* EntryWidget = GetHostedEntryWidget(ItemIndex);
		if (IsHostedItemInteractable(ItemIndex)
			&& EntryWidget
			&& GetListSource()->GetHostedEntryGeometry(ItemIndex).IsUnderLocation(ScreenPosition))
		{
			return ItemIndex;
		}
		if(IsHostedItemInteractable(ItemIndex))
			if(auto* Source=GetListSource())for(auto* Presentation:Source->GetEntryPresentations(ItemIndex))
				if(Presentation&&Presentation->IsVisible()&&Source->GetEntryPresentationGeometry(Presentation).IsUnderLocation(ScreenPosition))return ItemIndex;
	}
	return INDEX_NONE;
}

bool UReciListLayoutBoxBase::RouteNavigationInput(
	const EUINavigation Direction)
{
	const bool bNavigationInputBroadcast =
		BroadcastNavigationInput(Direction);
	const bool bListNavigated = NavigateHostedList(Direction);
	return bListNavigated
		|| (bNavigationInputBroadcast
			&& IsCrossAxisNavigation(Direction));
}

bool UReciListLayoutBoxBase::NavigateHostedList(
	const EUINavigation Direction)
{
	UReciListBase* Source = GetListSource();
	const int32 ItemCount = GetHostedItemCount();
	if (!Source || ItemCount <= 0)
	{
		return false;
	}

	const FReciListLayoutSettings Settings = BuildLayoutSettings();
	if (Settings.Mode == EReciListLayoutMode::Tile)
	{
		int32 TargetIndex = FindDirectionalTileItemIndex(Direction);
		if (TargetIndex == INDEX_NONE && bWrapNavigation)
		{
			TargetIndex = FindWrappedTileItemIndex(Direction);
		}
		return TargetIndex != INDEX_NONE
			&& Source->SetDrivenItemIndex(TargetIndex, Source->bFocusOnNavigation);
	}

	int32 Step = 0;
	if (Settings.Mode == EReciListLayoutMode::Vertical)
	{
		Step = Direction == EUINavigation::Up
			? -1
			: (Direction == EUINavigation::Down ? 1 : 0);
	}
	else
	{
		Step = Direction == EUINavigation::Left
			? -1
			: (Direction == EUINavigation::Right ? 1 : 0);
	}
	if (Step == 0)
	{
		return false;
	}

	const int32 CurrentIndex = Source->GetDrivenItemIndex();
	int32 CandidateIndex = CurrentIndex == INDEX_NONE
		? (Step > 0 ? -1 : ItemCount)
		: CurrentIndex;
	for (int32 Attempt = 0; Attempt < ItemCount; ++Attempt)
	{
		CandidateIndex += Step;
		if (bWrapNavigation)
		{
			CandidateIndex =
				(CandidateIndex % ItemCount + ItemCount) % ItemCount;
		}
		else if (CandidateIndex < 0 || CandidateIndex >= ItemCount)
		{
			return false;
		}

		if (CandidateIndex != CurrentIndex
			&& IsHostedItemInteractable(CandidateIndex)
			&& Source->SetDrivenItemIndex(CandidateIndex, Source->bFocusOnNavigation))
		{
			return true;
		}
	}
	return false;
}

bool UReciListLayoutBoxBase::ConfirmHostedList()
{
	UReciListBase* Source = GetListSource();
	return Source
		&& IsHostedItemInteractable(Source->GetDrivenItemIndex())
		&& Source->ConfirmDrivenItem();
}

bool UReciListLayoutBoxBase::FocusHostedItem(const int32 ItemIndex)
{
	UReciListBase* Source = GetListSource();
	return Source
		&& IsHostedItemInteractable(ItemIndex)
		&& (!Source->bFocusOnNavigation || Source->SetFocusedItemIndex(ItemIndex));
}

bool UReciListLayoutBoxBase::DriveHostedItem(const int32 ItemIndex)
{
	UReciListBase* Source = GetListSource();
	return Source
		&& IsHostedItemInteractable(ItemIndex)
		&& Source->SetDrivenItemIndex(ItemIndex, Source->bFocusOnNavigation);
}

bool UReciListLayoutBoxBase::ClickHostedItem(const int32 ItemIndex)
{
	UReciListBase* Source = GetListSource();
	if (!Source
		|| !IsHostedItemInteractable(ItemIndex)
		|| !Source->HandleHostedItemClick(ItemIndex))
	{
		return false;
	}
	return true;
}

bool UReciListLayoutBoxBase::FocusDrivenItem()
{
	UReciListBase* Source = GetListSource();
	const int32 ItemIndex = Source ? Source->GetDrivenItemIndex() : INDEX_NONE;
	return Source
		&& IsHostedItemInteractable(ItemIndex)
		&& (!Source->bFocusOnNavigation || Source->SetFocusedItemIndex(ItemIndex));
}

EOrientation UReciListLayoutBoxBase::GetHostedScrollOrientation() const
{
	const FReciListLayoutSettings Settings = BuildLayoutSettings();
	return Settings.Mode == EReciListLayoutMode::Horizontal
		? Orient_Horizontal
		: Orient_Vertical;
}

bool UReciListLayoutBoxBase::HasNavigationInputHandler() const
{
	const UReciListBase* Source = GetListSource();
	return Source && Source->HasNavigationInputHandler();
}

bool UReciListLayoutBoxBase::IsCrossAxisNavigation(
	const EUINavigation Direction) const
{
	const FReciListLayoutSettings Settings = BuildLayoutSettings();
	if (Settings.Mode == EReciListLayoutMode::Vertical)
	{
		return Direction == EUINavigation::Left
			|| Direction == EUINavigation::Right;
	}
	if (Settings.Mode == EReciListLayoutMode::Horizontal)
	{
		return Direction == EUINavigation::Up
			|| Direction == EUINavigation::Down;
	}
	return false;
}

EUINavigation UReciListLayoutBoxBase::ResolvePointerNavigation(
	const FVector2D& ScreenDelta,
	const float TriggerDistance) const
{
	if (!HasNavigationInputHandler())
	{
		return EUINavigation::Invalid;
	}

	const float HorizontalDistance = FMath::Abs(ScreenDelta.X);
	const float VerticalDistance = FMath::Abs(ScreenDelta.Y);
	if (FMath::Max(HorizontalDistance, VerticalDistance)
		<= FMath::Max(TriggerDistance, KINDA_SMALL_NUMBER))
	{
		return EUINavigation::Invalid;
	}

	bool bHorizontalDominant = HorizontalDistance > VerticalDistance;
	if (FMath::IsNearlyEqual(HorizontalDistance, VerticalDistance))
	{
		bHorizontalDominant = GetHostedScrollOrientation()
			== Orient_Horizontal;
	}

	if (bHorizontalDominant)
	{
		return ScreenDelta.X < 0.0f
			? EUINavigation::Right
			: EUINavigation::Left;
	}
	return ScreenDelta.Y < 0.0f
		? EUINavigation::Down
		: EUINavigation::Up;
}

bool UReciListLayoutBoxBase::BroadcastNavigationInput(
	const EUINavigation Direction)
{
	UReciListBase* Source = GetListSource();
	return Source && Source->BroadcastNavigationInput(Direction);
}

void UReciListLayoutBoxBase::AttachViewport(UReciScrollBox* InViewport)
{
	if (AttachedViewport == InViewport)
	{
		return;
	}

	AttachedViewport = InViewport;
	if (UReciListBase* Source = GetListSource())
	{
		Source->SetItemEmphasisDriven(
			AttachedViewport
				&& AttachedViewport->DrivesItemEmphasis());
	}
	UpdateInputTargetRegistration();
}

void UReciListLayoutBoxBase::DetachViewport(UReciScrollBox* InViewport)
{
	if (AttachedViewport != InViewport)
	{
		return;
	}

	AttachedViewport = nullptr;
	if (UReciListBase* Source = GetListSource())
	{
		Source->SetItemEmphasisDriven(false);
	}
	UpdateInputTargetRegistration();
}

UReciScrollBox* UReciListLayoutBoxBase::GetAttachedViewport() const
{
	return AttachedViewport;
}

void UReciListLayoutBoxBase::UpdateInputTargetRegistration()
{
	const bool bShouldRegister = MyContentBox.IsValid()
		&& bInputRoutingEnabled
		&& GetListSource()
		&& !AttachedViewport
		&& !IsDesignTime();
	if (bShouldRegister == bInputTargetRegistered)
	{
		return;
	}

	if (bShouldRegister)
	{
		FReciInputFocusCoordinator::Get().RegisterTarget(this, this);
		bInputTargetRegistered = true;
	}
	else
	{
		FReciInputFocusCoordinator::Get().UnregisterTarget(this);
		bInputTargetRegistered = false;
	}
}

bool UReciListLayoutBoxBase::HasHostedInteractionFocus() const
{
	return HasAnyUserFocus() || HasFocusedDescendants();
}

int32 UReciListLayoutBoxBase::FindDirectionalTileItemIndex(
	const EUINavigation Direction) const
{
	const UReciListBase* Source = GetListSource();
	if (!Source)
	{
		return INDEX_NONE;
	}

	const int32 CurrentIndex = Source->GetDrivenItemIndex();
	FVector2D CurrentCenter;
	if (!TryGetHostedItemCenter(CurrentIndex, CurrentCenter))
	{
		for (int32 ItemIndex = 0;
			ItemIndex < GetHostedItemCount();
			++ItemIndex)
		{
			if (IsHostedItemInteractable(ItemIndex))
			{
				return ItemIndex;
			}
		}
		return INDEX_NONE;
	}

	int32 BestIndex = INDEX_NONE;
	float BestScore = TNumericLimits<float>::Max();
	for (int32 ItemIndex = 0; ItemIndex < GetHostedItemCount(); ++ItemIndex)
	{
		if (ItemIndex == CurrentIndex)
		{
			continue;
		}
		FVector2D CandidateCenter;
		if (!TryGetHostedItemCenter(ItemIndex, CandidateCenter))
		{
			continue;
		}

		const FVector2D Delta = CandidateCenter - CurrentCenter;
		float Forward = 0.0f;
		float Perpendicular = 0.0f;
		switch (Direction)
		{
		case EUINavigation::Up:
			Forward = -Delta.Y;
			Perpendicular = FMath::Abs(Delta.X);
			break;
		case EUINavigation::Down:
			Forward = Delta.Y;
			Perpendicular = FMath::Abs(Delta.X);
			break;
		case EUINavigation::Left:
			Forward = -Delta.X;
			Perpendicular = FMath::Abs(Delta.Y);
			break;
		case EUINavigation::Right:
			Forward = Delta.X;
			Perpendicular = FMath::Abs(Delta.Y);
			break;
		default:
			return INDEX_NONE;
		}
		if (Forward <= 0.5f)
		{
			continue;
		}
		const float Score = Forward + Perpendicular * 2.0f;
		if (Score < BestScore)
		{
			BestScore = Score;
			BestIndex = ItemIndex;
		}
	}
	return BestIndex;
}

int32 UReciListLayoutBoxBase::FindWrappedTileItemIndex(
	const EUINavigation Direction) const
{
	const UReciListBase* Source = GetListSource();
	FVector2D CurrentCenter;
	if (!Source
		|| !TryGetHostedItemCenter(
			Source->GetDrivenItemIndex(),
			CurrentCenter))
	{
		for (int32 ItemIndex = 0;
			ItemIndex < GetHostedItemCount();
			++ItemIndex)
		{
			if (IsHostedItemInteractable(ItemIndex))
			{
				return ItemIndex;
			}
		}
		return INDEX_NONE;
	}

	int32 BestIndex = INDEX_NONE;
	float BestScore = TNumericLimits<float>::Max();
	for (int32 ItemIndex = 0; ItemIndex < GetHostedItemCount(); ++ItemIndex)
	{
		FVector2D CandidateCenter;
		if (!TryGetHostedItemCenter(ItemIndex, CandidateCenter))
		{
			continue;
		}
		float Score = 0.0f;
		switch (Direction)
		{
		case EUINavigation::Up:
			Score = FMath::Abs(CandidateCenter.X - CurrentCenter.X)
				* 10000.0f - CandidateCenter.Y;
			break;
		case EUINavigation::Down:
			Score = FMath::Abs(CandidateCenter.X - CurrentCenter.X)
				* 10000.0f + CandidateCenter.Y;
			break;
		case EUINavigation::Left:
			Score = FMath::Abs(CandidateCenter.Y - CurrentCenter.Y)
				* 10000.0f - CandidateCenter.X;
			break;
		case EUINavigation::Right:
			Score = FMath::Abs(CandidateCenter.Y - CurrentCenter.Y)
				* 10000.0f + CandidateCenter.X;
			break;
		default:
			return INDEX_NONE;
		}
		if (Score < BestScore)
		{
			BestScore = Score;
			BestIndex = ItemIndex;
		}
	}
	return BestIndex;
}

bool UReciListLayoutBoxBase::TryGetHostedItemCenter(
	const int32 ItemIndex,
	FVector2D& OutCenter) const
{
	UWidget* EntryWidget = GetHostedEntryWidget(ItemIndex);
	if (!IsHostedItemInteractable(ItemIndex) || !EntryWidget)
	{
		return false;
	}
	const FGeometry& Geometry = EntryWidget->GetCachedGeometry();
	if (Geometry.GetLocalSize().IsNearlyZero())
	{
		return false;
	}
	OutCenter = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
	return true;
}

bool UReciListLayoutBoxBase::IsHostedItemInteractable(
	const int32 ItemIndex) const
{
	const UReciListBase* Source = GetListSource();
	UWidget* EntryWidget = Source
		? Source->GetHostedEntryWidget(ItemIndex)
		: nullptr;
	UCommonButtonBase* EntryButton = Source
		? Source->GetHostedEntryButton(ItemIndex)
		: nullptr;
	return EntryWidget
		&& EntryButton
		&& EntryWidget->GetIsEnabled()
		&& EntryWidget->IsVisible()
		&& EntryButton->GetIsEnabled()
		&& Source->IsHostedItemNavigable(ItemIndex);
}

FReply UReciListLayoutBoxBase::HandleSlateFocusReceived(
	const FFocusEvent& FocusEvent)
{
	UReciListBase* Source = GetListSource();
	UWidget* FocusTarget = Source ? Source->GetHostedFocusTarget() : nullptr;
	return FocusTarget
		? FReply::Handled().SetUserFocus(
			FocusTarget->TakeWidget(),
			FocusEvent.GetCause())
		: FReply::Unhandled();
}

const UWidget* UReciListLayoutBoxBase::GetReciInputSurfaceWidget() const
{
	return this;
}

int32 UReciListLayoutBoxBase::GetReciInputUserIndex() const
{
	const UReciListBase* Source = GetListSource();
	if (Source)
	{
		if (const ULocalPlayer* LocalPlayer = Source->GetOwningLocalPlayer())
		{
			if (const TSharedPtr<const FSlateUser> User =
				LocalPlayer->GetSlateUser())
			{
				return User->GetUserIndex();
			}
			return LocalPlayer->GetLocalPlayerIndex();
		}
	}
	return 0;
}

bool UReciListLayoutBoxBase::CanHandleReciFocusedInput() const
{
	return GetIsEnabled()
		&& IsVisible()
		&& GetHostedItemCount() > 0
		&& HasHostedInteractionFocus();
}

bool UReciListLayoutBoxBase::HandleReciNavigation(
	const EUINavigation Direction)
{
	return RouteNavigationInput(Direction);
}

bool UReciListLayoutBoxBase::HandleReciAccept()
{
	return ConfirmHostedList();
}

bool UReciListLayoutBoxBase::HandleReciMouseWheel(
	const float WheelDelta,
	const FVector2D& ScreenPosition)
{
	return false;
}

bool UReciListLayoutBoxBase::HandleReciPointerDown(
	const FGeometry& SurfaceGeometry,
	const FPointerEvent& PointerEvent)
{
	return false;
}

bool UReciListLayoutBoxBase::HandleReciPointerMove(
	const FGeometry& SurfaceGeometry,
	const FPointerEvent& PointerEvent)
{
	return false;
}

bool UReciListLayoutBoxBase::HandleReciPointerUp(
	const FPointerEvent& PointerEvent)
{
	return false;
}

bool UReciListLayoutBoxBase::HasReciPointerCapture(
	const int32 PointerIndex) const
{
	return false;
}

void UReciListLayoutBoxBase::HandleReciPointerCaptureLost()
{
}

void UReciListLayoutBoxBase::HandleReciPointerHover(
	const FVector2D& ScreenPosition)
{
	if (AllowsPointerHover() && GetListSource()->bFocusOnPointerHover)
	{
		FocusHostedItem(FindHostedItemIndexAtScreenPosition(
			ScreenPosition));
	}
}

void UReciListLayoutBoxBase::ReturnReciFocusToDrivenItem()
{
	// Only a list that already holds focus takes it back to its driven item (see UReciScrollBox).
	const UReciListBase* Source = GetListSource();
	if (Source && (Source->HasAnyUserFocus() || Source->HasFocusedDescendants()))
	{
		FocusDrivenItem();
	}
}

#if WITH_EDITOR
const FText UReciListLayoutBoxBase::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "RECI COMMON UI");
}
#endif

FReciListLayoutSettings UReciVerticalBox::BuildLayoutSettings() const
{
	FReciListLayoutSettings Settings;
	Settings.Mode = EReciListLayoutMode::Vertical;
	Settings.ItemSpacing = ItemSpacing;
	return Settings;
}

FReciListLayoutSettings UReciHorizontalBox::BuildLayoutSettings() const
{
	FReciListLayoutSettings Settings;
	Settings.Mode = EReciListLayoutMode::Horizontal;
	Settings.ItemSpacing = ItemSpacing;
	Settings.bFillEntries = bFillEntries;
	return Settings;
}

void UReciHorizontalBox::SetFillEntries(bool bInFillEntries)
{
	bFillEntries = bInFillEntries;
	ApplyHostedLayout();
}

FReciListLayoutSettings UReciTileBox::BuildLayoutSettings() const
{
	FReciListLayoutSettings Settings;
	Settings.Mode = EReciListLayoutMode::Tile;
	Settings.ItemSpacing = ItemSpacing;
	Settings.TileEntrySize = EntrySize;
	Settings.TileFixedColumns = FixedColumns;
	Settings.TileFlowOrientation = FlowOrientation;
	return Settings;
}

void UReciTileBox::SetEntrySize(const FVector2D InEntrySize)
{
	const FVector2D ResolvedSize(
		FMath::IsFinite(InEntrySize.X)
			? FMath::Max(InEntrySize.X, 1.0f)
			: 1.0f,
		FMath::IsFinite(InEntrySize.Y)
			? FMath::Max(InEntrySize.Y, 1.0f)
			: 1.0f);
	if (EntrySize.Equals(ResolvedSize))
	{
		return;
	}
	EntrySize = ResolvedSize;
	ApplyHostedLayout();
}

void UReciTileBox::SetFixedColumns(const int32 InFixedColumns)
{
	const int32 ResolvedColumns = FMath::Max(InFixedColumns, 0);
	if (FixedColumns == ResolvedColumns)
	{
		return;
	}
	FixedColumns = ResolvedColumns;
	ApplyHostedLayout();
}

void UReciTileBox::SetFlowOrientation(
	const EOrientation InFlowOrientation)
{
	if (FlowOrientation == InFlowOrientation)
	{
		return;
	}
	FlowOrientation = InFlowOrientation;
	ApplyHostedLayout();
}

#undef LOCTEXT_NAMESPACE
