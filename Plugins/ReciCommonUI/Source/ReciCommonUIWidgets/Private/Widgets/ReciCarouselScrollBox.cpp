// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Widgets/ReciCarouselScrollBox.h"

#include "CommonButtonBase.h"
#include "HAL/PlatformTime.h"
#include "Motion/ReciMotionSubsystem.h"
#include "Widgets/ReciListBase.h"
#include "Widgets/ReciList.h"
#include "Widgets/ReciListLayoutBox.h"
#include "Widgets/SWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ReciCarouselScrollBox)

namespace
{
	constexpr int32 MaxGeometryRetryFrames = 8;
}

UReciCarouselScrollBox::UReciCarouselScrollBox(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A carousel must be able to center its first and last direct children.
	InitFrontPadScrolling(true);
	InitBackPadScrolling(true);
	// Carousel centering is animated by this class. Keeping the underlying
	// SScrollBox offset immediate also keeps externally routed drag 1:1.
	SetAnimateWheelScrolling(false);
	SetScrollWhenFocusChanges(EScrollWhenFocusChanges::NoScroll);
	SetNavigationDestination(EDescendantScrollDestination::Center);
	OnUserScrolled.AddUniqueDynamic(
		this,
		&UReciCarouselScrollBox::HandleUserScrolled);
}

void UReciCarouselScrollBox::RefreshCarousel()
{
	LastPresentationBlends.Reset();
	++CarouselRevision;
	GeometryRetryFrameCount = 0;
	bPendingRefresh = true;
	RequestActiveTimer();
}

bool UReciCarouselScrollBox::SetCenteredItemIndex(
	const int32 ItemIndex,
	const bool bAnimate)
{
	UWidget* TargetItem = GetCarouselItemAt(ItemIndex);
	if (!TargetItem || !TargetItem->IsVisible())
	{
		return false;
	}

	// With motion off, every animated centring takes the immediate path.
	const bool bAnimateNow = bAnimate
		&& UReciMotionSubsystem::GetMotionScaleForWidget(this) > 0.0f;
	// A drag across the axis moves no content; it stays held to its release.
	if (!IsExternallyCrossAxisDragging())
	{
		CancelExternalDrag();
	}
	StopRoutedMotion();
	EndInertialScrolling();
	CenterOffsetMotion.SnapTo(GetScrollOffset());
	PendingCenteredItemIndex = ItemIndex;
	bUserDrivenScroll = false;
	bPendingCenterAnimation = bAnimateNow;
	bWaitingToSnap = false;
	CenterStableFrameCount = 0;
	GeometryRetryFrameCount = 0;
	// Timed on the timer's own clock, from its first tick after this request.
	CenterRequestStartTime = 0.0;
	if(!bAnimateNow&&GetViewportExtent()>0)
	{
		const auto* Layout=GetHostedLayout();const auto* Source=Layout?Layout->GetListSource():nullptr;
		const FGeometry Geometry=Source?Source->GetHostedEntryGeometry(ItemIndex):FGeometry();
		const float Extent=GetOrientation()==Orient_Vertical?Geometry.GetLocalSize().Y:Geometry.GetLocalSize().X;
		if(Extent>0)
		{
			TGuardValue<bool> Guard(bUpdatingCarouselScrollOffset,true);
			SetScrollOffset(FMath::Clamp(GetScrollOffset()+GetItemCenterDistance(ItemIndex),0.0f,GetScrollOffsetOfEnd()));
			CenterOffsetMotion.SnapTo(GetScrollOffset());
			// The entries' geometry describes the old offset until they are painted again.
			bAwaitingLayout = true;
		}
	}
	RequestActiveTimer();
	return true;
}

int32 UReciCarouselScrollBox::GetCenteredItemIndex() const
{
	return CenteredItemIndex >= 0
		&& CenteredItemIndex < GetCarouselItemCount()
		? CenteredItemIndex
		: INDEX_NONE;
}

UObject* UReciCarouselScrollBox::GetCenteredItem() const
{
	const int32 ItemIndex = GetCenteredItemIndex();
	const UReciListLayoutBoxBase* Layout = GetHostedLayout();
	const UReciListBase* Source = Layout ? Layout->GetListSource() : nullptr;
	return Source && ItemIndex != INDEX_NONE
		? Source->GetItemAt(ItemIndex)
		: nullptr;
}

UWidget* UReciCarouselScrollBox::GetCenteredEntryWidget() const
{
	const int32 ItemIndex = GetCenteredItemIndex();
	return ItemIndex != INDEX_NONE
		? GetCarouselItemAt(ItemIndex)
		: nullptr;
}

bool UReciCarouselScrollBox::IsCenteringProgrammatically() const
{
	return PendingCenteredItemIndex != INDEX_NONE;
}

bool UReciCarouselScrollBox::SnapToClosestItem(const bool bAnimate)
{
	const int32 ClosestItemIndex = FindClosestItemIndex();
	return ClosestItemIndex != INDEX_NONE
		&& SetCenteredItemIndex(ClosestItemIndex, bAnimate);
}

FReciCarouselCenteredIndexChangedNative&
UReciCarouselScrollBox::OnCenteredIndexChangedNative()
{
	return CenteredIndexChangedNative;
}

FReciCarouselItemPresentationNative&
UReciCarouselScrollBox::OnItemPresentationNative()
{
	return ItemPresentationNative;
}

bool UReciCarouselScrollBox::LoopsHostedEntries() const
{
	return true;
}

bool UReciCarouselScrollBox::DrivesItemEmphasis() const
{
	const UReciListLayoutBoxBase* Layout = GetHostedLayout();
	return Layout
		&& Layout->GetHostedLayoutSettings().Mode
			!= EReciListLayoutMode::Tile;
}

void UReciCarouselScrollBox::NotifyHostedEntriesChanged()
{
	Super::NotifyHostedEntriesChanged();

	const bool bHadCenteredItem = CenteredItemIndex != INDEX_NONE;
	++CarouselRevision;
	CenteredItemIndex = INDEX_NONE;
	CancelProgrammaticCenter();
	StopRoutedMotion();
	LastPresentationBlends.Reset();
	WheelDeltaAccumulator = 0.0f;
	LastWheelInputTime = 0.0;
	GeometryRetryFrameCount = 0;
	bWaitingToSnap = false;
	bPendingRefresh = true;
	if (bHadCenteredItem)
	{
		CenteredIndexChangedNative.Broadcast(INDEX_NONE);
		OnCenteredItemChanged.Broadcast(nullptr, INDEX_NONE);
	}

	RequestActiveTimer();
	UReciListLayoutBoxBase* Layout = GetHostedLayout();
	UReciListBase* Source = Layout ? Layout->GetListSource() : nullptr;
	const int32 ItemCount = GetCarouselItemCount();
	if (!Source || ItemCount <= 0)
	{
		return;
	}

	const int32 DrivenItemIndex = Source->GetDrivenItemIndex();
	const int32 InitialItemIndex = DrivenItemIndex >= 0
		&& DrivenItemIndex < ItemCount
		? DrivenItemIndex
		: 0;
	SetCenteredItemIndex(InitialItemIndex, false);
}

void UReciCarouselScrollBox::NotifyHostedFocusChanged(
	const int32 ItemIndex)
{
	// Focus and the semantic driven item are intentionally independent.
	// Pointer hover may move focus without scrolling the carousel.
	const UReciListLayoutBoxBase* Layout = GetHostedLayout();
	const UReciListBase* Source = Layout ? Layout->GetListSource() : nullptr;
	if (Source && ItemIndex == Source->GetDrivenItemIndex()
		&& !bApplyingCenteredItem && !HasExternalPointerCapture()
		&& !IsRoutedScrollMotionActive())
	{
		// Returning to the same item can reveal hints or change its text style
		// and height without a driven-index change. Measure its center again.
		SetCenteredItemIndex(ItemIndex, true);
	}
}

void UReciCarouselScrollBox::NotifyHostedDrivenItemChanged(
	const int32 ItemIndex)
{
	if (bApplyingCenteredItem
		|| ItemIndex < 0
		|| ItemIndex >= GetCarouselItemCount()
		|| PendingCenteredItemIndex == ItemIndex)
	{
		return;
	}
	SetCenteredItemIndex(ItemIndex, true);
}

void UReciCarouselScrollBox::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	SetAnimateWheelScrolling(false);
	SetScrollWhenFocusChanges(EScrollWhenFocusChanges::NoScroll);
	OnUserScrolled.AddUniqueDynamic(
		this,
		&UReciCarouselScrollBox::HandleUserScrolled);
	++CarouselRevision;
	GeometryRetryFrameCount = 0;
	bPendingRefresh = true;
	RequestActiveTimer();
}

void UReciCarouselScrollBox::SetVisibility(
	const ESlateVisibility InVisibility)
{
	const ESlateVisibility PreviousVisibility = GetVisibility();
	Super::SetVisibility(InVisibility);

	const bool bWasLayoutVisible = PreviousVisibility
		!= ESlateVisibility::Collapsed
		&& PreviousVisibility != ESlateVisibility::Hidden;
	const bool bIsLayoutVisible = InVisibility
		!= ESlateVisibility::Collapsed
		&& InVisibility != ESlateVisibility::Hidden;
	if (!bWasLayoutVisible && bIsLayoutVisible)
	{
		++CarouselRevision;
		GeometryRetryFrameCount = 0;
		bPendingRefresh = true;
		RequestActiveTimer();
	}
}

void UReciCarouselScrollBox::ReleaseSlateResources(
	const bool bReleaseChildren)
{
	bWaitingToSnap = false;
	CancelProgrammaticCenter();
	StopActiveTimer();
	Super::ReleaseSlateResources(bReleaseChildren);
}

TSharedRef<SWidget> UReciCarouselScrollBox::RebuildWidget()
{
	TSharedRef<SWidget> RebuiltWidget = Super::RebuildWidget();
	LastObservedScrollOffset = GetScrollOffset();
	NotifyHostedEntriesChanged();
	return RebuiltWidget;
}

void UReciCarouselScrollBox::OnSlotAdded(UPanelSlot* InSlot)
{
	Super::OnSlotAdded(InSlot);
	NotifyHostedEntriesChanged();
}

void UReciCarouselScrollBox::OnSlotRemoved(UPanelSlot* InSlot)
{
	Super::OnSlotRemoved(InSlot);
	NotifyHostedEntriesChanged();
}

#if WITH_EDITOR
const FText UReciCarouselScrollBox::GetPaletteCategory()
{
	return NSLOCTEXT("ReciCommonUI", "PaletteCategory", "RECI COMMON UI");
}
#endif

void UReciCarouselScrollBox::HandleUserScrolled(
	const float CurrentOffset)
{
	if (!bUpdatingCarouselScrollOffset)
	{
		bUserDrivenScroll = true;
		CancelProgrammaticCenter();
	}
	LastObservedScrollOffset = CurrentOffset;
	LastMovementTime = FPlatformTime::Seconds();
	bWaitingToSnap = bSnapToClosestItem
		&& !bUpdatingCarouselScrollOffset;
	GeometryRetryFrameCount = 0;
	bPendingRefresh = true;
	RequestActiveTimer();
}

EActiveTimerReturnType UReciCarouselScrollBox::HandleActiveTimer(
	const double CurrentTime,
	const float DeltaTime)
{
	// After a jump made outside this timer, the timer runs before the entries are painted at the new offset, and
	// measuring them now would apply the jump twice.
	if (bAwaitingLayout)
	{
		bAwaitingLayout = false;
		bPendingRefresh = true;
		return EActiveTimerReturnType::Continue;
	}
	// Identical repeated views let the physical offset return to the middle
	// cycle without a visual jump, interrupting neither drag nor inertia.
	if(RebaseLoopScroll())return EActiveTimerReturnType::Continue;
	const float CurrentOffset = GetScrollOffset();
	const bool bIsScrolling = GetIsScrolling();
	const bool bOffsetChanged = !FMath::IsNearlyEqual(
		CurrentOffset,
		LastObservedScrollOffset,
		0.01f);
	if (bOffsetChanged)
	{
		LastObservedScrollOffset = CurrentOffset;
		LastMovementTime = CurrentTime;
		bWaitingToSnap = bSnapToClosestItem
			&& !IsCenteringProgrammatically()
			&& !HasExternalPointerCapture()
			&& !IsRoutedScrollMotionActive();
		bPendingRefresh = true;
	}
	if (bIsScrolling
		|| PendingCenteredItemIndex != INDEX_NONE
		|| IsExternallyDragging()
		|| IsRoutedScrollMotionActive())
	{
		bPendingRefresh = true;
	}

	if (bPendingRefresh)
	{
		// Consume this request before broadcasting state changes so a delegate
		// can enqueue another refresh without having it overwritten here.
		bPendingRefresh = false;
		if (UpdateCarouselState())
		{
			GeometryRetryFrameCount = 0;
		}
		else if (++GeometryRetryFrameCount <= MaxGeometryRetryFrames)
		{
			bPendingRefresh = true;
		}
	}

	if (PendingCenteredItemIndex != INDEX_NONE)
	{
		if (CenterRequestStartTime <= 0.0)
		{
			CenterRequestStartTime = CurrentTime;
		}
		UWidget* TargetItem = GetCarouselItemAt(
			PendingCenteredItemIndex);
		const bool bVertical = GetOrientation() == Orient_Vertical;
		const FGeometry TargetGeometry=GetHostedLayout()->GetListSource()->GetHostedEntryGeometry(PendingCenteredItemIndex);
		const float TargetExtent = TargetItem
			? (bVertical
				? TargetGeometry.GetLocalSize().Y
				: TargetGeometry.GetLocalSize().X)
			: 0.0f;
		if (!TargetItem || !TargetItem->IsVisible())
		{
			CancelProgrammaticCenter();
			bPendingRefresh = true;
		}
		else if (GetViewportExtent() > 0.0f && TargetExtent > 0.0f)
		{
			const float CenterDistance = GetItemCenterDistance(
				PendingCenteredItemIndex);
			if (FMath::Abs(CenterDistance) <= 0.5f)
			{
				++CenterStableFrameCount;
				if (CenterStableFrameCount >= 2)
				{
					CancelProgrammaticCenter();
				}
			}
			else
			{
				CenterStableFrameCount = 0;
				const float CurrentScrollOffset = GetScrollOffset();
				if (!FMath::IsNearlyEqual(
					CenterOffsetMotion.GetCurrentValue(),
					CurrentScrollOffset,
					0.01f))
				{
					CenterOffsetMotion.SnapTo(CurrentScrollOffset);
				}
				const float TargetScrollOffset = FMath::Clamp(
					CurrentScrollOffset + CenterDistance,
					0.0f,
					GetScrollOffsetOfEnd());
				if (!bPendingCenterAnimation)
				{
					CenterOffsetMotion.SnapTo(TargetScrollOffset);
				}
				else
				{
					CenterOffsetMotion.Retarget(TargetScrollOffset);
					CenterOffsetMotion.Advance(
						DeltaTime,
						CenterMotion.Scaled(
							UReciMotionSubsystem::GetMotionScaleForWidget(this)));
				}
				TGuardValue<bool> ScrollUpdateGuard(
					bUpdatingCarouselScrollOffset,
					true);
				SetScrollOffset(CenterOffsetMotion.GetCurrentValue());
				bPendingRefresh = true;
			}

		}
		if (PendingCenteredItemIndex != INDEX_NONE
			&& CurrentTime - CenterRequestStartTime > 3.0)
		{
			CancelProgrammaticCenter();
			bPendingRefresh = true;
		}
	}

	if (!bSnapToClosestItem)
	{
		bWaitingToSnap = false;
	}
	if (bWaitingToSnap
		&& !HasExternalPointerCapture()
		&& !IsRoutedScrollMotionActive()
		&& !bIsScrolling
		&& CurrentTime - LastMovementTime
			>= static_cast<double>(FMath::Max(SnapDelay, 0.0f)))
	{
		bWaitingToSnap = false;
		SnapToClosestItem(true);
	}

	if (bPendingRefresh
		|| PendingCenteredItemIndex != INDEX_NONE
		|| bWaitingToSnap
		|| bIsScrolling
		|| IsExternallyDragging()
		|| IsRoutedScrollMotionActive()
		|| bOffsetChanged)
	{
		return EActiveTimerReturnType::Continue;
	}

	ActiveTimerHandle.Reset();
	bUserDrivenScroll = false;
	return EActiveTimerReturnType::Stop;
}

bool UReciCarouselScrollBox::RebaseLoopScroll()
{
	auto* Layout=GetHostedLayout();auto* Source=Layout?Cast<UReciList>(Layout->GetListSource()):nullptr;
	if(!Source||!Source->bLoopingEntries)return false;
	if(Source->EnsureLoopCoverage(GetViewportExtent(),GetOrientation()))
	{bPendingRefresh=true;GeometryRetryFrameCount=0;return true;}
	const float Cycle=Source->GetLoopCycleExtent(GetOrientation());
	auto* Anchor=Source->GetLoopAnchorEntry();
	const auto Geometry=Source->GetLoopAnchorGeometry();
	if(Cycle<=0||!Anchor||Geometry.GetLocalSize().IsNearlyZero())return false;
	const FVector2D Position=GetCachedGeometry().AbsoluteToLocal(Geometry.LocalToAbsolute(Geometry.GetLocalSize()*.5f));
	const float Distance=GetOrientation()==Orient_Vertical?Position.Y-GetViewportExtent()*.5f:Position.X-GetViewportExtent()*.5f;
	const float Shift=FMath::RoundToFloat(Distance/Cycle)*Cycle;
	Source->RefreshLoopPresentations(GetCachedGeometry(),Shift);
	if(FMath::IsNearlyZero(Shift))return false;
	{
		TGuardValue<bool> Guard(bUpdatingCarouselScrollOffset,true);
		SetScrollOffset(GetScrollOffset()+Shift);
	}
	CenterOffsetMotion.SnapTo(GetScrollOffset());LastObservedScrollOffset=GetScrollOffset();
	bPendingRefresh=true;GeometryRetryFrameCount=0;
	return true;
}

void UReciCarouselScrollBox::CancelProgrammaticCenter()
{
	CenterOffsetMotion.SnapTo(GetScrollOffset());
	PendingCenteredItemIndex = INDEX_NONE;
	CenterStableFrameCount = 0;
	CenterRequestStartTime = 0.0;
}

void UReciCarouselScrollBox::RequestActiveTimer()
{
	if (!MyScrollBox.IsValid() || ActiveTimerHandle.IsValid())
	{
		return;
	}

	ActiveTimerHandle = MyScrollBox->RegisterActiveTimer(
		0.0f,
		FWidgetActiveTimerDelegate::CreateUObject(
			this,
			&UReciCarouselScrollBox::HandleActiveTimer));
}

bool UReciCarouselScrollBox::UpdateCarouselState()
{
	const uint32 UpdateRevision = CarouselRevision;
	const int32 ItemCount = GetCarouselItemCount();
	if (ItemCount <= 0)
	{
		LastPresentationBlends.Reset();
		if (CenteredItemIndex != INDEX_NONE)
		{
			CenteredItemIndex = INDEX_NONE;
			CenteredIndexChangedNative.Broadcast(INDEX_NONE);
			if (CarouselRevision != UpdateRevision)
			{
				return true;
			}
			OnCenteredItemChanged.Broadcast(nullptr, INDEX_NONE);
		}
		return true;
	}

	const float ViewportExtent = GetViewportExtent();
	if (ViewportExtent <= 0.0f)
	{
		return false;
	}

	UReciListLayoutBoxBase* Layout = GetHostedLayout();
	UReciListBase* Source = Layout ? Layout->GetListSource() : nullptr;
	const float BlendDistance = ResolvePresentationBlendDistance();
	if (LastPresentationBlends.Num() != ItemCount)
	{
		LastPresentationBlends.Init(-1.0f, ItemCount);
	}
	bool bPresentationChanged = false;
	for (int32 ItemIndex = 0;
		ItemIndex < ItemCount;
		++ItemIndex)
	{
		UWidget* EntryWidget = GetCarouselItemAt(ItemIndex);
		const float Distance = EntryWidget && EntryWidget->IsVisible()
			? FMath::Abs(GetItemCenterDistance(ItemIndex))
			: TNumericLimits<float>::Max();
		const float MainBlend = BlendDistance > KINDA_SMALL_NUMBER
			? 1.0f - FMath::Clamp(Distance / BlendDistance, 0.0f, 1.0f)
			: 0.0f;
		if (!FMath::IsNearlyEqual(
			LastPresentationBlends[ItemIndex],
			MainBlend,
			0.001f))
		{
			LastPresentationBlends[ItemIndex] = MainBlend;
			bPresentationChanged = true;
			if (Source)
			{
				Source->SetItemEmphasis(
					ItemIndex,
					MainBlend);
			}
			ItemPresentationNative.Broadcast(ItemIndex, MainBlend);
			if (CarouselRevision != UpdateRevision)
			{
				return true;
			}
		}
	}
	if (bPresentationChanged)
	{
		// Advanced entries interpolate their desired size from this blend. Give
		// layout one more frame and then measure again until it converges.
		bPendingRefresh = true;
	}

	const int32 ClosestItemIndex = FindClosestItemIndex();
	if (ClosestItemIndex != CenteredItemIndex)
	{
		CenteredItemIndex = ClosestItemIndex;
		CenteredIndexChangedNative.Broadcast(CenteredItemIndex);
		if (CarouselRevision != UpdateRevision)
		{
			return true;
		}
		OnCenteredItemChanged.Broadcast(
			Source && CenteredItemIndex != INDEX_NONE
				? Source->GetItemAt(CenteredItemIndex)
				: nullptr,
			CenteredItemIndex);
		if (CarouselRevision != UpdateRevision) return true;
		if (CenteredItemIndex != INDEX_NONE
			&& bUserDrivenScroll
			&& !IsCenteringProgrammatically())
		{
			if (Source)
			{
				TGuardValue<bool> ApplyingGuard(
					bApplyingCenteredItem,
					true);
				Source->SetDrivenItemIndex(
					CenteredItemIndex,
					Source->bFocusOnNavigation && HasHostedInteractionFocus());
			}
		}
	}
	return true;
}

int32 UReciCarouselScrollBox::FindClosestItemIndex(
	float* OutSignedDistance) const
{
	int32 ClosestIndex = INDEX_NONE;
	float ClosestAbsoluteDistance = TNumericLimits<float>::Max();
	float ClosestSignedDistance = 0.0f;
	for (int32 ItemIndex = 0;
		ItemIndex < GetCarouselItemCount();
		++ItemIndex)
	{
		if (!IsItemEligibleForCentering(ItemIndex))
		{
			continue;
		}

		const float SignedDistance = GetItemCenterDistance(ItemIndex);
		const float AbsoluteDistance = FMath::Abs(SignedDistance);
		if (AbsoluteDistance < ClosestAbsoluteDistance)
		{
			ClosestIndex = ItemIndex;
			ClosestAbsoluteDistance = AbsoluteDistance;
			ClosestSignedDistance = SignedDistance;
		}
	}

	if (OutSignedDistance)
	{
		*OutSignedDistance = ClosestSignedDistance;
	}
	return ClosestIndex;
}

bool UReciCarouselScrollBox::IsItemEligibleForCentering(
	const int32 ItemIndex) const
{
	UWidget* EntryWidget = GetCarouselItemAt(ItemIndex);
	const UReciListLayoutBoxBase* Layout = GetHostedLayout();
	const UReciListBase* Source = Layout ? Layout->GetListSource() : nullptr;
	const UCommonButtonBase* EntryButton = Source
		? Source->GetHostedEntryButton(ItemIndex)
		: nullptr;
	if (!EntryWidget
		|| !EntryWidget->IsVisible()
		|| !EntryWidget->GetIsEnabled()
		|| !EntryButton
		|| !EntryButton->GetIsEnabled()
		|| !Source->IsHostedItemNavigable(ItemIndex))
	{
		return false;
	}

	const FVector2D ItemSize = Source->GetHostedEntryGeometry(ItemIndex).GetLocalSize();
	const float ItemExtent = GetOrientation() == Orient_Vertical
		? ItemSize.Y
		: ItemSize.X;
	return ItemExtent > 0.0f;
}

float UReciCarouselScrollBox::GetItemCenterDistance(
	const int32 ItemIndex) const
{
	UWidget* EntryWidget = GetCarouselItemAt(ItemIndex);
	if (!EntryWidget)
	{
		return 0.0f;
	}

	const FGeometry& ScrollGeometry = GetCachedGeometry();
	const FGeometry ItemGeometry = GetHostedLayout()->GetListSource()->GetHostedEntryGeometry(ItemIndex);
	const FVector2D ItemCenterAbsolute = ItemGeometry.LocalToAbsolute(
		ItemGeometry.GetLocalSize() * 0.5f);
	const FVector2D ItemCenterLocal = ScrollGeometry.AbsoluteToLocal(
		ItemCenterAbsolute);
	const FVector2D ViewportCenter =
		ScrollGeometry.GetLocalSize() * 0.5f;
	return GetOrientation() == Orient_Vertical
		? ItemCenterLocal.Y - ViewportCenter.Y
		: ItemCenterLocal.X - ViewportCenter.X;
}

float UReciCarouselScrollBox::GetViewportExtent() const
{
	const FVector2D ViewportSize = GetCachedGeometry().GetLocalSize();
	return GetOrientation() == Orient_Vertical
		? ViewportSize.Y
		: ViewportSize.X;
}

float UReciCarouselScrollBox::ResolvePresentationBlendDistance() const
{
	return PresentationBlendDistance > KINDA_SMALL_NUMBER
		? PresentationBlendDistance
		: GetViewportExtent() * 0.5f;
}

void UReciCarouselScrollBox::StopActiveTimer()
{
	if (MyScrollBox.IsValid())
	{
		if (const TSharedPtr<FActiveTimerHandle> TimerHandle =
			ActiveTimerHandle.Pin())
		{
			MyScrollBox->UnRegisterActiveTimer(TimerHandle.ToSharedRef());
		}
	}
	ActiveTimerHandle.Reset();
}

int32 UReciCarouselScrollBox::GetCarouselItemCount() const
{
	const UReciListLayoutBoxBase* Layout = GetHostedLayout();
	if (!Layout
		|| Layout->GetHostedLayoutSettings().Mode
			== EReciListLayoutMode::Tile)
	{
		return 0;
	}
	return Layout->GetHostedItemCount();
}

UWidget* UReciCarouselScrollBox::GetCarouselItemAt(
	const int32 ItemIndex) const
{
	const UReciListLayoutBoxBase* Layout = GetHostedLayout();
	return Layout
		&& Layout->GetHostedLayoutSettings().Mode
			!= EReciListLayoutMode::Tile
		? Layout->GetHostedEntryWidget(ItemIndex)
		: nullptr;
}

int32 UReciCarouselScrollBox::ConsumeWheelSteps(const float WheelDelta)
{
	const double CurrentTime = FPlatformTime::Seconds();
	const bool bDirectionChanged = !FMath::IsNearlyZero(WheelDelta)
		&& !FMath::IsNearlyZero(WheelDeltaAccumulator)
		&& FMath::Sign(WheelDelta) != FMath::Sign(WheelDeltaAccumulator);
	if (CurrentTime - LastWheelInputTime > 0.2 || bDirectionChanged)
	{
		WheelDeltaAccumulator = 0.0f;
	}
	LastWheelInputTime = CurrentTime;
	WheelDeltaAccumulator += WheelDelta;

	const int32 Steps = WheelDeltaAccumulator > 0.0f
		? FMath::FloorToInt(WheelDeltaAccumulator)
		: FMath::CeilToInt(WheelDeltaAccumulator);
	WheelDeltaAccumulator -= static_cast<float>(Steps);
	return Steps;
}

bool UReciCarouselScrollBox::HandleHostedMouseWheel(
	const float WheelDelta,
	const FVector2D& ScreenPosition)
{
	UReciListLayoutBoxBase* Layout = GetHostedLayout();
	if (!Layout || GetCarouselItemCount() <= 0)
	{
		return Super::HandleHostedMouseWheel(WheelDelta, ScreenPosition);
	}
	if (FMath::IsNearlyZero(WheelDelta))
	{
		return false;
	}

	CancelExternalDrag();
	StopRoutedMotion();
	EndInertialScrolling();
	const int32 WheelSteps = ConsumeWheelSteps(WheelDelta);
	if (WheelSteps == 0)
	{
		return true;
	}

	const bool bVertical = Layout->GetHostedScrollOrientation()
		== Orient_Vertical;
	const EUINavigation Direction = WheelSteps > 0
		? (bVertical ? EUINavigation::Up : EUINavigation::Left)
		: (bVertical ? EUINavigation::Down : EUINavigation::Right);
	bool bMoved = false;
	for (int32 StepIndex = 0;
		StepIndex < FMath::Abs(WheelSteps);
		++StepIndex)
	{
		if (!Layout->NavigateHostedList(Direction))
		{
			break;
		}
		bMoved = true;
	}

	if (!bMoved)
	{
		if (UReciListBase* Source = Layout->GetListSource())
		{
			SetCenteredItemIndex(Source->GetDrivenItemIndex(), true);
		}
	}
	return true;
}

void UReciCarouselScrollBox::HandleHostedDragStarted()
{
	bUserDrivenScroll = true;
	CancelProgrammaticCenter();
	bWaitingToSnap = false;
	LastMovementTime = FPlatformTime::Seconds();
	GeometryRetryFrameCount = 0;
	bPendingRefresh = true;
	RequestActiveTimer();
}

void UReciCarouselScrollBox::HandleHostedDragUpdated()
{
	LastMovementTime = FPlatformTime::Seconds();
	bWaitingToSnap = false;
	GeometryRetryFrameCount = 0;
	bPendingRefresh = true;
	RequestActiveTimer();
}

void UReciCarouselScrollBox::HandleHostedDragEnded(
	const bool bWasDragging)
{
	if (!bWasDragging)
	{
		return;
	}
	LastMovementTime = FPlatformTime::Seconds();
	bWaitingToSnap = bSnapToClosestItem
		&& !IsRoutedScrollMotionActive();
	bPendingRefresh = true;
	RequestActiveTimer();
}

void UReciCarouselScrollBox::HandleHostedDragCanceled()
{
	LastMovementTime = FPlatformTime::Seconds();
	bWaitingToSnap = bSnapToClosestItem;
	bPendingRefresh = true;
	RequestActiveTimer();
}

void UReciCarouselScrollBox::HandleHostedRoutedScrollUpdated()
{
	bUserDrivenScroll = true;
	CancelProgrammaticCenter();
	LastMovementTime = FPlatformTime::Seconds();
	bWaitingToSnap = false;
	bPendingRefresh = true;
	RequestActiveTimer();
}

void UReciCarouselScrollBox::HandleHostedRoutedScrollSettled()
{
	LastMovementTime = FPlatformTime::Seconds();
	bWaitingToSnap = bSnapToClosestItem
		&& !HasExternalPointerCapture();
	bPendingRefresh = true;
	RequestActiveTimer();
}
