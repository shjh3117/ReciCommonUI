// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Widgets/ReciScrollBox.h"
#include "Widgets/ReciListBase.h"

#include "CommonButtonBase.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/WidgetPath.h"
#include "Slate/SObjectWidget.h"
#include "HAL/PlatformTime.h"
#include "Input/Events.h"
#include "Motion/ReciMotionSubsystem.h"
#include "Widgets/ReciListLayoutBox.h"
#include "Widgets/ReciWidgetTreeUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ReciScrollBox)

DEFINE_LOG_CATEGORY_STATIC(LogReciScrollBox, Log, All);

namespace
{
	/** Ticks a pending into-view request waits for its entry and the viewport to have a size. */
	constexpr int32 MaxIntoViewRetryFrames = 8;

	/** Distance (Slate units) within which an entry counts as in view, below any visible clip. */
	constexpr float IntoViewTolerance = 0.1f;

	/**
	 * Arranges Descendant down its parent chain from Ancestor's geometry, as Slate's own scroll into view finds a
	 * descendant: where Slate places it now, even when it was not painted last frame.
	 */
	bool ArrangeDescendant(
		const SWidget& Ancestor,
		const FGeometry& AncestorGeometry,
		const TSharedRef<SWidget>& Descendant,
		FGeometry& OutGeometry)
	{
		TArray<TSharedRef<SWidget>, TInlineAllocator<16>> Chain;
		TSharedPtr<SWidget> Widget = Descendant;
		while (Widget.IsValid() && Widget.Get() != &Ancestor)
		{
			Chain.Add(Widget.ToSharedRef());
			Widget = Widget->GetParentWidget();
		}
		if (!Widget.IsValid())
		{
			return false;
		}

		const SWidget* Parent = &Ancestor;
		FGeometry Geometry = AncestorGeometry;
		for (int32 Index = Chain.Num() - 1; Index >= 0; --Index)
		{
			FArrangedChildren ArrangedChildren(EVisibility::Visible);
			Parent->ArrangeChildren(Geometry, ArrangedChildren);
			const FArrangedWidget* Child = nullptr;
			for (int32 ChildIndex = 0; ChildIndex < ArrangedChildren.Num() && !Child; ++ChildIndex)
			{
				if (ArrangedChildren[ChildIndex].Widget == Chain[Index])
				{
					Child = &ArrangedChildren[ChildIndex];
				}
			}
			if (!Child)
			{
				return false;
			}
			Geometry = Child->Geometry;
			Parent = &Chain[Index].Get();
		}
		OutGeometry = Geometry;
		return true;
	}
}

UReciScrollBox::UReciScrollBox(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetAnimateWheelScrolling(false);
	SetScrollWhenFocusChanges(EScrollWhenFocusChanges::NoScroll);
	SetNavigationDestination(EDescendantScrollDestination::IntoView);
	ScrollOffsetMotion.Reset(0.0f);
}

bool UReciScrollBox::BeginExternalDrag(
	const int32 PointerIndex,
	const FVector2D& ScreenPosition)
{
	const UReciListLayoutBoxBase* Layout = GetHostedLayout();
	if (ExternalPointerIndex != INDEX_NONE
		|| !Layout
		|| Layout->GetHostedItemCount() <= 0)
	{
		return false;
	}

	StopRoutedMotion();
	EndInertialScrolling();
	ExternalPointerIndex = PointerIndex;
	ExternalDragItemIndex = Layout->FindHostedItemIndexAtScreenPosition(ScreenPosition);
	ExternalDragStartScreenPosition = ScreenPosition;
	ExternalDragLastScreenPosition = ScreenPosition;
	ExternalDragLastTime = FPlatformTime::Seconds();
	bExternalDragging = false;
	bExternalCrossAxisDragging = false;
	bExternalNavigationInputEmitted = false;
	return true;
}

bool UReciScrollBox::UpdateExternalDrag(
	const int32 PointerIndex,
	const FVector2D& ScreenPosition)
{
	if (ExternalPointerIndex != PointerIndex)
	{
		return false;
	}

	UReciListLayoutBoxBase* Layout = GetHostedLayout();
	if (!Layout)
	{
		return false;
	}
	const UReciListBase* ListSource = Layout->GetListSource();
	if (!ListSource)
	{
		return false;
	}

	const bool bVertical = GetOrientation() == Orient_Vertical;
	const FVector2D TotalScreenDelta =
		ScreenPosition - ExternalDragStartScreenPosition;
	const float PrimaryScreenDelta = bVertical
		? TotalScreenDelta.Y
		: TotalScreenDelta.X;
	const float CrossScreenDelta = bVertical
		? TotalScreenDelta.X
		: TotalScreenDelta.Y;
	const float ResolvedTriggerDistance = FMath::Max(
		DragTriggerDistance,
		KINDA_SMALL_NUMBER);

	// A drag across the axis is resolved once: its later moves are consumed below.
	if (!bExternalNavigationInputEmitted && !bExternalCrossAxisDragging)
	{
		const EUINavigation PointerNavigation =
			Layout->ResolvePointerNavigation(
				TotalScreenDelta,
				ResolvedTriggerDistance);
		if (PointerNavigation != EUINavigation::Invalid)
		{
			const bool bAlongScrollAxis = bVertical
				? (PointerNavigation == EUINavigation::Up
					|| PointerNavigation == EUINavigation::Down)
				: (PointerNavigation == EUINavigation::Left
					|| PointerNavigation == EUINavigation::Right);
			bool bBroadcast = true;
			if (bAlongScrollAxis)
			{
				// A drag along the axis scrolls the content, and focus returns to the driven item.
				FReciInputFocusCoordinator::Get().ReturnFocusToDrivenItem(this);
			}
			else
			{
				// A drag across it does not scroll: like a press, it belongs to the entry it began on, which becomes the
				// driven item first, as keys would move it there, so a handler that acts on the focused entry acts on that
				// one. On an entry a viewport that drives emphasis has not centred, it only centres that entry, as a first
				// click does. One that began on no interactable entry, or whose entry cannot become the driven item, moves
				// nothing. Neither of those is broadcast. The box keeps every drag across the axis to its release, which
				// clicks nothing.
				const bool bOnSideItem = DrivesItemEmphasis()
					&& ListSource->GetDrivenItemIndex() != ExternalDragItemIndex;
				// Set before the driven item moves: a carousel that centres it leaves a drag across the axis held.
				bExternalCrossAxisDragging = true;
				DragInertia.Reset();
				bBroadcast = ExternalDragItemIndex != INDEX_NONE
					&& Layout->DriveHostedItem(ExternalDragItemIndex)
					&& !bOnSideItem;
			}
			if (ExternalPointerIndex != PointerIndex
				|| !IsValid(Layout)
				|| GetHostedLayout() != Layout
				|| Layout->GetListSource() != ListSource)
			{
				return false;
			}
			const bool bNavigationInputEmitted = bBroadcast
				&& Layout->BroadcastNavigationInput(PointerNavigation);
			if (ExternalPointerIndex != PointerIndex
				|| !IsValid(Layout)
				|| GetHostedLayout() != Layout
				|| Layout->GetListSource() != ListSource
				|| !GetIsEnabled()
				|| !IsVisible()
				|| !Layout->GetIsEnabled()
				|| !Layout->IsVisible())
			{
				return false;
			}
			bExternalNavigationInputEmitted = bNavigationInputEmitted;
		}
	}

	if (bExternalCrossAxisDragging)
	{
		ExternalDragLastScreenPosition = ScreenPosition;
		ExternalDragLastTime = FPlatformTime::Seconds();
		return true;
	}

	if (!bExternalDragging
		&& Layout->HasNavigationInputHandler()
		&& FMath::Abs(CrossScreenDelta) > FMath::Abs(PrimaryScreenDelta))
	{
		ExternalDragLastScreenPosition = ScreenPosition;
		ExternalDragLastTime = FPlatformTime::Seconds();
		return false;
	}

	if (!bExternalDragging
		&& FMath::Abs(PrimaryScreenDelta) <= ResolvedTriggerDistance)
	{
		ExternalDragLastScreenPosition = ScreenPosition;
		ExternalDragLastTime = FPlatformTime::Seconds();
		return false;
	}

	if (!bExternalDragging)
	{
		bExternalDragging = true;
		FReciInputFocusCoordinator::Get().ReturnFocusToDrivenItem(this);
		HandleHostedDragStarted();
	}

	const float ScreenDelta = bVertical
		? ScreenPosition.Y - ExternalDragLastScreenPosition.Y
		: ScreenPosition.X - ExternalDragLastScreenPosition.X;
	const double CurrentTime = FPlatformTime::Seconds();
	const double DeltaSeconds = FMath::Max(
		CurrentTime - ExternalDragLastTime,
		1.0 / 1000.0);
	ExternalDragLastScreenPosition = ScreenPosition;
	ExternalDragLastTime = CurrentTime;

	const float LayoutScale = FMath::Max(
		GetCachedGeometry().GetAccumulatedLayoutTransform().GetScale(),
		KINDA_SMALL_NUMBER);
	const float ScrollDelta = -ScreenDelta / LayoutScale;
	const float RequestedOffset = GetScrollOffset() + ScrollDelta;
	const float ResolvedOffset = FMath::Clamp(
		RequestedOffset,
		0.0f,
		GetScrollOffsetOfEnd());
	// The drag owns the offset: an entry focused before it no longer pulls the content.
	ClearPendingIntoView();
	SetScrollOffset(ResolvedOffset);
	DragInertia.AddDragSample(ScrollDelta, DeltaSeconds);
	HandleHostedDragUpdated();
	return true;
}

bool UReciScrollBox::EndExternalDrag(const int32 PointerIndex)
{
	if (ExternalPointerIndex != PointerIndex)
	{
		return false;
	}

	const bool bWasScrolling = bExternalDragging;
	const bool bWasDragging = bWasScrolling
		|| bExternalCrossAxisDragging;
	ExternalPointerIndex = INDEX_NONE;
	ExternalDragItemIndex = INDEX_NONE;
	bExternalDragging = false;
	bExternalCrossAxisDragging = false;
	bExternalNavigationInputEmitted = false;
	if (bWasScrolling
		&& DragInertia.Release(
			FPlatformTime::Seconds() - ExternalDragLastTime,
			MinimumInertiaSpeed,
			UReciMotionSubsystem::GetMotionScaleForWidget(this)))
	{
		RequestScrollInteractionTimer();
	}
	else
	{
		DragInertia.Reset();
	}
	HandleHostedDragEnded(bWasScrolling);
	return bWasDragging;
}

bool UReciScrollBox::HasExternalDrag(const int32 PointerIndex) const
{
	return ExternalPointerIndex == PointerIndex;
}

void UReciScrollBox::CancelExternalDrag()
{
	const bool bWasScrolling = bExternalDragging;
	ExternalPointerIndex = INDEX_NONE;
	ExternalDragItemIndex = INDEX_NONE;
	bExternalDragging = false;
	bExternalCrossAxisDragging = false;
	bExternalNavigationInputEmitted = false;
	DragInertia.Reset();
	if (bWasScrolling)
	{
		HandleHostedDragCanceled();
	}
}

int32 UReciScrollBox::FindHostedItemIndexAtScreenPosition(
	const FVector2D& ScreenPosition) const
{
	const UReciListLayoutBoxBase* Layout = GetHostedLayout();
	return Layout
		? Layout->FindHostedItemIndexAtScreenPosition(ScreenPosition)
		: INDEX_NONE;
}

UReciListLayoutBoxBase* UReciScrollBox::GetHostedLayout() const
{
	return GetChildrenCount() == 1
		? Cast<UReciListLayoutBoxBase>(GetChildAt(0))
		: nullptr;
}

bool UReciScrollBox::DrivesItemEmphasis() const
{
	return false;
}

bool UReciScrollBox::LoopsHostedEntries() const
{
	return false;
}

void UReciScrollBox::NotifyHostedEntriesChanged()
{
	// The index no longer names the entry a drag in progress began on.
	ExternalDragItemIndex = INDEX_NONE;
	ClearPendingIntoView();
	InvalidateLayoutAndVolatility();
}

void UReciScrollBox::NotifyHostedFocusChanged(const int32 ItemIndex)
{
	// Slate's animated scroll into view eases with FInterpTo, which depends on the frame rate, and leaves Slate
	// smoothing the offsets of later drags. The timer measures the entry each tick and moves through Scroll Motion.
	const UReciListLayoutBoxBase* Layout = GetHostedLayout();
	if (!MyScrollBox.IsValid()
		|| !Layout
		|| !Layout->GetHostedEntryWidget(ItemIndex))
	{
		return;
	}
	PendingIntoViewItemIndex = ItemIndex;
	IntoViewRetryFrames = 0;
	RequestScrollInteractionTimer();
}

void UReciScrollBox::NotifyHostedDrivenItemChanged(const int32 ItemIndex)
{
	NotifyHostedFocusChanged(ItemIndex);
}

void UReciScrollBox::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	SetAnimateWheelScrolling(false);
	SetScrollWhenFocusChanges(EScrollWhenFocusChanges::NoScroll);
	AttachHostedLayout();
	UpdateHostedInputRegistration();
}

void UReciScrollBox::ReleaseSlateResources(const bool bReleaseChildren)
{
	DetachHostedLayout();
	FReciInputFocusCoordinator::Get().UnregisterTarget(this);
	bInputTargetRegistered = false;
	CancelExternalDrag();
	StopScrollInteractionTimer();
	Super::ReleaseSlateResources(bReleaseChildren);
}

TSharedRef<SWidget> UReciScrollBox::RebuildWidget()
{
	TSharedRef<SWidget> RebuiltWidget = Super::RebuildWidget();
	AttachHostedLayout();
	UpdateHostedInputRegistration();
	return RebuiltWidget;
}

void UReciScrollBox::OnSlotAdded(UPanelSlot* InSlot)
{
	Super::OnSlotAdded(InSlot);
	AttachHostedLayout();
	UpdateHostedInputRegistration();
}

void UReciScrollBox::OnSlotRemoved(UPanelSlot* InSlot)
{
	DetachHostedLayout();
	Super::OnSlotRemoved(InSlot);
	AttachHostedLayout();
	UpdateHostedInputRegistration();
}

#if WITH_EDITOR
const FText UReciScrollBox::GetPaletteCategory()
{
	return NSLOCTEXT("ReciCommonUI", "PaletteCategory", "RECI COMMON UI");
}
#endif

bool UReciScrollBox::HandleHostedMouseWheel(
	const float WheelDelta,
	const FVector2D& ScreenPosition)
{
	if (FMath::IsNearlyZero(WheelDelta))
	{
		return false;
	}
	if (bNavigateWithMouseWheel)
	{
		UReciListLayoutBoxBase* Layout = GetHostedLayout();
		if (!Layout || Layout->GetHostedItemCount() <= 0) return false;
		const double Now = FPlatformTime::Seconds();
		if (Now - LastNavigationWheelTime > 0.2
			|| (!FMath::IsNearlyZero(NavigationWheelAccumulator)
				&& FMath::Sign(NavigationWheelAccumulator) != FMath::Sign(WheelDelta)))
			NavigationWheelAccumulator = 0.0f;
		LastNavigationWheelTime = Now;
		NavigationWheelAccumulator += WheelDelta;
		const int32 Steps = NavigationWheelAccumulator > 0.0f
			? FMath::FloorToInt(NavigationWheelAccumulator)
			: FMath::CeilToInt(NavigationWheelAccumulator);
		NavigationWheelAccumulator -= static_cast<float>(Steps);
		if (Steps == 0) return true;
		CancelExternalDrag();
		StopRoutedMotion();
		EndInertialScrolling();
		const bool bVertical = Layout->GetHostedScrollOrientation() == Orient_Vertical;
		const EUINavigation Direction = Steps > 0
			? (bVertical ? EUINavigation::Up : EUINavigation::Left)
			: (bVertical ? EUINavigation::Down : EUINavigation::Right);
		for (int32 Step = 0; Step < FMath::Abs(Steps); ++Step)
			if (!Layout->NavigateHostedList(Direction)) break;
		return true;
	}

	EndInertialScrolling();
	DragInertia.Reset();
	ClearPendingIntoView();
	const float CurrentOffset = GetScrollOffset();
	if (!FMath::IsNearlyEqual(
		ScrollOffsetMotion.GetCurrentValue(),
		CurrentOffset,
		0.01f))
	{
		ScrollOffsetMotion.SnapTo(CurrentOffset);
	}
	MoveRoutedScrollTo(
		FMath::Clamp(
			ScrollOffsetMotion.GetTargetValue()
				- WheelDelta
					* FMath::Max(WheelScrollAmount, 0.0f)
					* FMath::Max(GetWheelScrollMultiplier(), 0.0f),
			0.0f,
			GetScrollOffsetOfEnd()),
		UReciMotionSubsystem::GetMotionScaleForWidget(this));
	return true;
}

void UReciScrollBox::MoveRoutedScrollTo(
	const float TargetOffset,
	const float MotionScale)
{
	if (MotionScale <= 0.0f)
	{
		// Motion off: no frame shows the way there.
		SetScrollOffset(TargetOffset);
		ScrollOffsetMotion.SnapTo(TargetOffset);
		HandleHostedRoutedScrollUpdated();
		HandleHostedRoutedScrollSettled();
		return;
	}

	const float CurrentOffset = GetScrollOffset();
	if (!FMath::IsNearlyEqual(
		ScrollOffsetMotion.GetCurrentValue(),
		CurrentOffset,
		0.01f))
	{
		ScrollOffsetMotion.SnapTo(CurrentOffset);
	}
	ScrollOffsetMotion.Retarget(TargetOffset);
	RequestScrollInteractionTimer();
}

bool UReciScrollBox::UpdatePendingIntoView(const float MotionScale)
{
	if (PendingIntoViewItemIndex == INDEX_NONE)
	{
		return false;
	}

	const UReciListLayoutBoxBase* Layout = GetHostedLayout();
	UWidget* EntryWidget = Layout
		? Layout->GetHostedEntryWidget(PendingIntoViewItemIndex)
		: nullptr;
	if (!EntryWidget || bExternalDragging)
	{
		ClearPendingIntoView();
		return false;
	}

	float Delta = 0.0f;
	if (!MeasureIntoViewDelta(*EntryWidget, Delta))
	{
		if (++IntoViewRetryFrames < MaxIntoViewRetryFrames)
		{
			return true;
		}
		// Still nothing to measure: leave it to Slate, unanimated, so later drags stay 1:1.
		ClearPendingIntoView();
		ScrollWidgetIntoView(
			EntryWidget,
			false,
			EDescendantScrollDestination::IntoView,
			GetNavigationScrollPadding());
		return false;
	}
	IntoViewRetryFrames = 0;

	const float CurrentOffset = GetScrollOffset();
	const float TargetOffset = FMath::Clamp(
		CurrentOffset + Delta,
		0.0f,
		GetScrollOffsetOfEnd());
	if (FMath::Abs(TargetOffset - CurrentOffset) <= IntoViewTolerance)
	{
		// In view: a motion still running (the tail of this one, or a wheel's) goes on by itself.
		ClearPendingIntoView();
		return false;
	}

	// Measured again every tick, so an entry that changes size or moves on the way still ends fully in view.
	if (!ScrollOffsetMotion.IsActive()
		|| FMath::Abs(TargetOffset - ScrollOffsetMotion.GetTargetValue())
			> IntoViewTolerance)
	{
		DragInertia.Stop();
		MoveRoutedScrollTo(TargetOffset, MotionScale);
	}
	return true;
}

bool UReciScrollBox::MeasureIntoViewDelta(
	const UWidget& Entry,
	float& OutDelta) const
{
	const TSharedPtr<SWidget> EntrySlateWidget = Entry.GetCachedWidget();
	if (!MyScrollBox.IsValid() || !EntrySlateWidget.IsValid())
	{
		return false;
	}

	const FGeometry& ViewGeometry = MyScrollBox->GetTickSpaceGeometry();
	FGeometry EntryGeometry;
	if (!ArrangeDescendant(
		*MyScrollBox,
		ViewGeometry,
		EntrySlateWidget.ToSharedRef(),
		EntryGeometry))
	{
		return false;
	}

	const bool bVertical = GetOrientation() == Orient_Vertical;
	const auto AlongAxis = [bVertical](const FVector2D& Vector)
	{
		return static_cast<float>(bVertical ? Vector.Y : Vector.X);
	};
	const float EntryExtent = AlongAxis(EntryGeometry.GetLocalSize());
	const float ViewExtent = AlongAxis(ViewGeometry.GetLocalSize());
	if (EntryExtent <= 0.0f || ViewExtent <= 0.0f)
	{
		return false;
	}

	// Slate's Into View rule (SScrollBox::InternalScrollDescendantIntoView).
	const float Padding = GetNavigationScrollPadding();
	const float EntryStart = AlongAxis(
		ViewGeometry.AbsoluteToLocal(EntryGeometry.GetAbsolutePosition()));
	const float EntryEnd = EntryStart + EntryExtent;
	const float ViewStart = Padding;
	const float ViewEnd = ViewExtent - Padding;
	OutDelta = 0.0f;
	if (EntryStart < ViewStart)
	{
		OutDelta = EntryStart - ViewStart;
	}
	else if (EntryEnd > ViewEnd)
	{
		OutDelta = (EntryEnd - (ViewEnd - ViewStart)) - ViewStart;
	}
	return true;
}

void UReciScrollBox::ClearPendingIntoView()
{
	PendingIntoViewItemIndex = INDEX_NONE;
	IntoViewRetryFrames = 0;
}

void UReciScrollBox::HandleHostedDragStarted()
{
}

void UReciScrollBox::HandleHostedDragUpdated()
{
}

void UReciScrollBox::HandleHostedDragEnded(const bool bWasDragging)
{
}

void UReciScrollBox::HandleHostedDragCanceled()
{
}

void UReciScrollBox::HandleHostedRoutedScrollUpdated()
{
}

void UReciScrollBox::HandleHostedRoutedScrollSettled()
{
}

void UReciScrollBox::UpdateHostedInputRegistration()
{
	const bool bShouldRegister = MyScrollBox.IsValid()
		&& bInputRoutingEnabled
		&& GetHostedLayout()
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

void UReciScrollBox::SetInputRoutingEnabled(const bool bEnabled)
{
	bInputRoutingEnabled = bEnabled;
	UpdateHostedInputRegistration();
}

bool UReciScrollBox::RouteExternalMouseWheel(float Delta, const FVector2D& Position)
{
	return HandleHostedMouseWheel(Delta, Position);
}

bool UReciScrollBox::HasHostedInteractionFocus() const
{
	return HasAnyUserFocus() || HasFocusedDescendants();
}

bool UReciScrollBox::IsExternallyDragging() const
{
	return bExternalDragging;
}

bool UReciScrollBox::IsExternallyCrossAxisDragging() const
{
	return bExternalCrossAxisDragging;
}

bool UReciScrollBox::HasExternalPointerCapture() const
{
	return ExternalPointerIndex != INDEX_NONE;
}

bool UReciScrollBox::IsRoutedScrollMotionActive() const
{
	return ScrollOffsetMotion.IsActive() || DragInertia.IsActive();
}

void UReciScrollBox::RequestScrollInteractionTimer()
{
	if (!MyScrollBox.IsValid() || ScrollInteractionTimerHandle.IsValid())
	{
		return;
	}

	ScrollInteractionTimerHandle = MyScrollBox->RegisterActiveTimer(
		0.0f,
		FWidgetActiveTimerDelegate::CreateUObject(
			this,
			&UReciScrollBox::HandleScrollInteractionTimer));
}

void UReciScrollBox::StopScrollInteractionTimer()
{
	if (MyScrollBox.IsValid())
	{
		if (const TSharedPtr<FActiveTimerHandle> TimerHandle =
			ScrollInteractionTimerHandle.Pin())
		{
			MyScrollBox->UnRegisterActiveTimer(TimerHandle.ToSharedRef());
		}
	}
	ScrollInteractionTimerHandle.Reset();
}

const UWidget* UReciScrollBox::GetReciInputSurfaceWidget() const
{
	return this;
}

int32 UReciScrollBox::GetReciInputUserIndex() const
{
	if (const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (const TSharedPtr<const FSlateUser> User =
			LocalPlayer->GetSlateUser())
		{
			return User->GetUserIndex();
		}
		return LocalPlayer->GetLocalPlayerIndex();
	}
	return 0;
}

bool UReciScrollBox::CanHandleReciFocusedInput() const
{
	const UReciListLayoutBoxBase* Layout = GetHostedLayout();
	return GetIsEnabled()
		&& IsVisible()
		&& Layout
		&& Layout->GetHostedItemCount() > 0
		&& HasHostedInteractionFocus();
}

bool UReciScrollBox::HandleReciNavigation(const EUINavigation Direction)
{
	UReciListLayoutBoxBase* Layout = GetHostedLayout();
	return Layout && Layout->RouteNavigationInput(Direction);
}

bool UReciScrollBox::HandleReciAccept()
{
	UReciListLayoutBoxBase* Layout = GetHostedLayout();
	return Layout && Layout->ConfirmHostedList();
}

bool UReciScrollBox::HandleReciMouseWheel(
	const float WheelDelta,
	const FVector2D& ScreenPosition)
{
	return HandleHostedMouseWheel(WheelDelta, ScreenPosition);
}

bool UReciScrollBox::HandleReciPointerDown(
	const FGeometry& SurfaceGeometry,
	const FPointerEvent& PointerEvent)
{
	const FVector2D ScreenPosition = PointerEvent.GetScreenSpacePosition();
	PressedItemIndex = FindHostedItemIndexAtScreenPosition(ScreenPosition);
	// Pointer activation belongs to the actual hit target: leave the press to the nested button.
	if (PressedItemIndex != INDEX_NONE
		&& IsPressOnNestedButton(ScreenPosition, PressedItemIndex))
	{
		PressedItemIndex = INDEX_NONE;
		return false;
	}
	return BeginExternalDrag(
		PointerEvent.GetPointerIndex(),
		ScreenPosition);
}

bool UReciScrollBox::IsPressOnNestedButton(
	const FVector2D& ScreenPosition,
	const int32 ItemIndex) const
{
	const UReciListLayoutBoxBase* Layout = GetHostedLayout();
	const UReciListBase* Source = Layout ? Layout->GetListSource() : nullptr;
	const UCommonButtonBase* EntryButton = Source ? Source->GetHostedEntryButton(ItemIndex) : nullptr;
	if (!EntryButton)
	{
		return false;
	}

	TArray<TSharedRef<SWindow>> Windows;
	FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);
	const FWidgetPath Path = FSlateApplication::Get().LocateWindowUnderMouse(ScreenPosition, Windows);
	// The innermost button under the pointer decides: the entry itself keeps list semantics (drag, click). A key
	// selector is not a CommonUI button, and takes the click itself to start selecting a key.
	for (int32 Index = Path.Widgets.Num() - 1; Index >= 0; --Index)
	{
		const TSharedRef<SWidget>& Widget = Path.Widgets[Index].Widget;
		if (UE::ReciCommonUI::Private::IsEnabledKeySelector(*Widget))
		{
			return true;
		}
		if (Widget->GetType() != TEXT("SObjectWidget"))
		{
			continue;
		}
		const UCommonButtonBase* Button =
			Cast<UCommonButtonBase>(StaticCastSharedRef<SObjectWidget>(Widget)->GetWidgetObject());
		if (Button)
		{
			return Button != EntryButton
				&& Button->GetIsEnabled()
				&& Button->IsInteractionEnabled();
		}
	}
	return false;
}

bool UReciScrollBox::HandleReciPointerMove(
	const FGeometry& SurfaceGeometry,
	const FPointerEvent& PointerEvent)
{
	UpdateExternalDrag(
		PointerEvent.GetPointerIndex(),
		PointerEvent.GetScreenSpacePosition());
	return true;
}

bool UReciScrollBox::HandleReciPointerUp(
	const FPointerEvent& PointerEvent)
{
	if (!HasExternalDrag(PointerEvent.GetPointerIndex()))
	{
		return false;
	}

	const int32 ReleasedItemIndex = FindHostedItemIndexAtScreenPosition(
		PointerEvent.GetScreenSpacePosition());
	const bool bWasDragging = EndExternalDrag(
		PointerEvent.GetPointerIndex());
	if (!bWasDragging
		&& PressedItemIndex != INDEX_NONE
		&& PressedItemIndex == ReleasedItemIndex)
	{
		if (UReciListLayoutBoxBase* Layout = GetHostedLayout())
		{
			// A viewport that drives emphasis keeps the driven item emphasised: the first click only makes an item
			// driven (focus selects it), and clicking the driven item confirms it.
			const UReciListBase* Source = Layout->GetListSource();
			const bool bAlreadyCentred = !DrivesItemEmphasis()
				|| (Source && Source->GetDrivenItemIndex() == ReleasedItemIndex);
			Layout->FocusHostedItem(ReleasedItemIndex);
			if (bAlreadyCentred)
			{
				Layout->ClickHostedItem(ReleasedItemIndex);
			}
		}
	}
	PressedItemIndex = INDEX_NONE;
	return true;
}

bool UReciScrollBox::HasReciPointerCapture(const int32 PointerIndex) const
{
	return HasExternalDrag(PointerIndex);
}

void UReciScrollBox::HandleReciPointerCaptureLost()
{
	CancelExternalDrag();
	PressedItemIndex = INDEX_NONE;
}

void UReciScrollBox::HandleReciPointerHover(
	const FVector2D& ScreenPosition)
{
	UReciListLayoutBoxBase* Layout = GetHostedLayout();
	if (Layout && Layout->AllowsPointerHover() && Layout->GetListSource()->bFocusOnPointerHover)
	{
		Layout->FocusHostedItem(
			FindHostedItemIndexAtScreenPosition(
				ScreenPosition));
	}
}

void UReciScrollBox::ReturnReciFocusToDrivenItem()
{
	// Focus returns to the driven item only inside a list that already holds focus:
	// a pointer leaving or a wheel turning over some other list must not pull keyboard focus there.
	UReciListLayoutBoxBase* Layout = GetHostedLayout();
	if (Layout && HasHostedInteractionFocus())
	{
		Layout->FocusDrivenItem();
	}
}

EActiveTimerReturnType UReciScrollBox::HandleScrollInteractionTimer(
	const double CurrentTime,
	const float DeltaTime)
{
	const float MotionScale = UReciMotionSubsystem::GetMotionScaleForWidget(this);
	bool bContinue = UpdatePendingIntoView(MotionScale);
	if (ScrollOffsetMotion.IsActive())
	{
		ScrollOffsetMotion.Advance(DeltaTime, ScrollMotion.Scaled(MotionScale));
		SetScrollOffset(FMath::Clamp(
			ScrollOffsetMotion.GetCurrentValue(),
			0.0f,
			GetScrollOffsetOfEnd()));
		HandleHostedRoutedScrollUpdated();
		bContinue = bContinue || ScrollOffsetMotion.IsActive();
	}

	if (!bExternalDragging && DragInertia.IsActive())
	{
		const float PreviousOffset = GetScrollOffset();
		const float RequestedOffset = PreviousOffset
			+ static_cast<float>(DragInertia.Advance(
				DeltaTime,
				InertiaDeceleration,
				MinimumInertiaSpeed,
				MotionScale));
		const float NewOffset = FMath::Clamp(
			RequestedOffset,
			0.0f,
			GetScrollOffsetOfEnd());
		SetScrollOffset(NewOffset);
		HandleHostedRoutedScrollUpdated();
		if (NewOffset != RequestedOffset)
		{
			// The glide reached an end.
			DragInertia.Stop();
		}
		bContinue = bContinue || DragInertia.IsActive();
	}

	if (bContinue)
	{
		return EActiveTimerReturnType::Continue;
	}

	ScrollInteractionTimerHandle.Reset();
	HandleHostedRoutedScrollSettled();
	return EActiveTimerReturnType::Stop;
}

void UReciScrollBox::AttachHostedLayout()
{
	UReciListLayoutBoxBase* HostedLayout = GetHostedLayout();
	for (int32 ChildIndex = 0; ChildIndex < GetChildrenCount(); ++ChildIndex)
	{
		if (UReciListLayoutBoxBase* ChildLayout =
			Cast<UReciListLayoutBoxBase>(GetChildAt(ChildIndex)))
		{
			if (ChildLayout != HostedLayout
				&& ChildLayout->GetAttachedViewport() == this)
			{
				ChildLayout->DetachViewport(this);
			}
		}
	}

	if (HostedLayout)
	{
		HostedLayout->AttachViewport(this);
		SetOrientation(HostedLayout->GetHostedScrollOrientation());
	}
	else if (GetChildrenCount() > 0)
	{
		UE_LOG(
			LogReciScrollBox,
			Error,
			TEXT("%s requires exactly one ReciListLayoutBox child."),
			*GetName());
	}
}

void UReciScrollBox::DetachHostedLayout()
{
	for (int32 ChildIndex = 0; ChildIndex < GetChildrenCount(); ++ChildIndex)
	{
		if (UReciListLayoutBoxBase* Layout =
			Cast<UReciListLayoutBoxBase>(GetChildAt(ChildIndex)))
		{
			Layout->DetachViewport(this);
		}
	}
}

void UReciScrollBox::StopRoutedMotion()
{
	StopScrollInteractionTimer();
	ScrollOffsetMotion.SnapTo(GetScrollOffset());
	DragInertia.Reset();
	ClearPendingIntoView();
}
