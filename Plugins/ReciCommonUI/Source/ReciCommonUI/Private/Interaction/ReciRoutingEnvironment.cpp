// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Interaction/ReciRoutingEnvironment.h"

#include "CommonActivatableWidget.h"
#include "CommonInputSubsystem.h"
#include "Components/Widget.h"
#include "EnhancedInputSubsystems.h"
#include "Input/CommonUIActionRouterBase.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"

bool UE::ReciCommonUI::Private::IsLayerInActiveRoot(const UWidget& Widget)
{
	const UCommonActivatableWidget* Layer = UCommonUIActionRouterBase::FindActivatable(Widget.GetCachedWidget(), nullptr);
	return !Layer || !Layer->IsActivated() || GetRoutingEnvironment().IsInActiveRoot(*Layer);
}

const FReciStockRoutingEnvironment& FReciStockRoutingEnvironment::Get()
{
	static const FReciStockRoutingEnvironment Environment;
	return Environment;
}

bool FReciStockRoutingEnvironment::IsInActiveRoot(const UCommonActivatableWidget& Widget) const
{
	// CommonUI's router walks up from the widget to its active root, or, with action domains, the domain's root.
	const UCommonUIActionRouterBase* Router = UCommonUIActionRouterBase::Get(Widget);
	return !Router || Router->IsWidgetInActiveRoot(&Widget);
}

ECommonInputMode FReciStockRoutingEnvironment::GetActiveInputMode(const UWidget& Widget) const
{
	// Each player has its own router, so split screen asks the widget's player, not the first one.
	const UCommonUIActionRouterBase* Router = UCommonUIActionRouterBase::Get(Widget);
	return Router ? Router->GetActiveInputMode(ECommonInputMode::All) : ECommonInputMode::All;
}

TOptional<int32> FReciStockRoutingEnvironment::GetUserIndex(const UWidget& Widget) const
{
	const ULocalPlayer* Player = Widget.GetOwningLocalPlayer();
	if (!Player)
	{
		return {};
	}
	return FSlateApplication::Get().GetUserIndexForController(Player->GetControllerId());
}

TArray<FKey> FReciStockRoutingEnvironment::GetActionKeys(const UWidget& Widget, const UInputAction& Action) const
{
	const ULocalPlayer* Player = Widget.GetOwningLocalPlayer();
	return Player ? GetPlayerActionKeys(*Player, Action) : TArray<FKey>();
}

TArray<FKey> FReciStockRoutingEnvironment::GetPlayerActionKeys(
	const ULocalPlayer& Player,
	const UInputAction& Action) const
{
	const UEnhancedInputLocalPlayerSubsystem* EnhancedInput = Player.GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	return EnhancedInput ? EnhancedInput->QueryKeysMappedToAction(&Action) : TArray<FKey>();
}

TOptional<FReciInputMethod> FReciStockRoutingEnvironment::GetInputMethod(const ULocalPlayer& Player) const
{
	// As CommonUI's action widget asks: a player without a game instance shows no keys.
	const UCommonInputSubsystem* CommonInput = Player.GetGameInstance() ? UCommonInputSubsystem::Get(&Player) : nullptr;
	if (!CommonInput)
	{
		return {};
	}
	FReciInputMethod Method;
	Method.InputType = CommonInput->GetCurrentInputType();
	Method.GamepadName = CommonInput->GetCurrentGamepadName();
	Method.bShowsInputKeys = CommonInput->ShouldShowInputKeys();
	return Method;
}
