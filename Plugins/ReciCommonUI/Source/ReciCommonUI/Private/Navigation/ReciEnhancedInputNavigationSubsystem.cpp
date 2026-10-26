// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Navigation/ReciEnhancedInputNavigationSubsystem.h"

#include "CommonInputSettings.h"
#include "CommonInputSubsystem.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/NavigationConfig.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "InputAction.h"
#include "ICommonInputModule.h"
#include "Interaction/ReciRoutingEnvironment.h"
#include "Navigation/ReciEnhancedInputNavigationConfig.h"
#include "ReciReport.h"
#include "Settings/ReciCommonUISettings.h"
#include "Settings/ReciUIInputData.h"
#include "Settings/ReciUIInputDataAccess.h"

bool UReciEnhancedInputNavigationSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	const bool bFollow = UE::ReciCommonUI::Private::GetFollowedInputData() != nullptr;
	if (!bFollow)
	{
		if (const UReciUIInputData* InputData = UE::ReciCommonUI::Private::GetReciUIInputData())
		{
			// A Reci UI Input Data with CommonUI's Enhanced Input support off: its fields are hidden and ignored.
			RECI_WARN_ONCE(TEXT("EnhancedInputSupportOff"), InputData->GetClass()->GetPathName(),
				TEXT("The project's Input Data is a Reci UI Input Data, but CommonUI's Enhanced Input support is off, so "
					"its actions and Mapped Keys Replace Default Keys are ignored and only Slate's default keys navigate. "
					"Turn on Enable Enhanced Input Support in Project Settings > Game > Common Input Settings."));
		}
	}

	// With stock Input Data and the settings' defaults, no player's navigation config is touched (DESIGN.md principle 4).
	return bFollow || !GetDefault<UReciCommonUISettings>()->bTabNavigation;
}

void UReciEnhancedInputNavigationSubsystem::Initialize(
	FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency<UEnhancedInputLocalPlayerSubsystem>();
	Collection.InitializeDependency<UCommonInputSubsystem>();
	Start();
}

void UReciEnhancedInputNavigationSubsystem::Start()
{
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		EnhancedInputSubsystem =
			LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		ControllerIdChangedHandle =
			LocalPlayer->OnControllerIdChanged().AddUObject(
				this,
				&ThisClass::HandleControllerIdChanged);
	}

	if (EnhancedInputSubsystem)
	{
		EnhancedInputSubsystem->ControlMappingsRebuiltDelegate
			.AddUniqueDynamic(
				this,
				&ThisClass::HandleControlMappingsRebuilt);
	}

	if (FSlateApplication::IsInitialized())
	{
		SlateUserRegisteredHandle =
			FSlateApplication::Get().OnUserRegistered().AddUObject(
				this,
				&ThisClass::HandleSlateUserRegistered);
	}

	InstallNavigationConfig();
	RefreshNavigationMappings();
}

void UReciEnhancedInputNavigationSubsystem::Deinitialize()
{
	if (EnhancedInputSubsystem)
	{
		EnhancedInputSubsystem->ControlMappingsRebuiltDelegate.RemoveDynamic(
			this,
			&ThisClass::HandleControlMappingsRebuilt);
	}

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		LocalPlayer->OnControllerIdChanged().Remove(
			ControllerIdChangedHandle);
	}
	ControllerIdChangedHandle.Reset();

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnUserRegistered().Remove(
			SlateUserRegisteredHandle);
	}
	SlateUserRegisteredHandle.Reset();

	RestoreNavigationConfig();
	EnhancedInputSubsystem = nullptr;

	Super::Deinitialize();
}

void UReciEnhancedInputNavigationSubsystem::RefreshNavigationMappings()
{
	InstallNavigationConfig();

	TMap<FKey, EUINavigation> NewNavigationRules;
	TMap<FKey, EUINavigationAction> NewNavigationActionRules;
	// The player's keys, as Enhanced Input maps them now.
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const auto GetActionKeys = [LocalPlayer](const UInputAction& Action)
	{
		return UE::ReciCommonUI::Private::GetRoutingEnvironment().GetPlayerActionKeys(*LocalPlayer, Action);
	};
	// Not following (Tab Navigation off alone), the config wraps the one it replaced, changing only Tab.
	const UReciUIInputData* InputData = UE::ReciCommonUI::Private::GetFollowedInputData();
	if (LocalPlayer && InputData)
	{
		const auto AddActionMappings =
			[&GetActionKeys, &NewNavigationRules](
				const UInputAction* Action,
				const EUINavigation Direction)
		{
			if (Action)
			{
				FReciEnhancedInputNavigationConfig::AddNavigationKeys(
					NewNavigationRules,
					GetActionKeys(*Action),
					Direction);
			}
		};

		AddActionMappings(
			InputData->NavigationUpAction,
			EUINavigation::Up);
		AddActionMappings(
			InputData->NavigationDownAction,
			EUINavigation::Down);
		AddActionMappings(
			InputData->NavigationLeftAction,
			EUINavigation::Left);
		AddActionMappings(
			InputData->NavigationRightAction,
			EUINavigation::Right);

		const auto AddNavigationActionMappings =
			[&GetActionKeys, &NewNavigationActionRules](
				const UInputAction* Action,
				const EUINavigationAction NavigationAction)
		{
			if (!Action)
			{
				return;
			}

			FReciEnhancedInputNavigationConfig::AddNavigationKeys(
				NewNavigationActionRules,
				GetActionKeys(*Action),
				NavigationAction);
		};

		const UCommonInputSettings& CommonInputSettings =
			ICommonInputModule::GetSettings();
		AddNavigationActionMappings(
			CommonInputSettings.GetEnhancedInputClickAction(),
			EUINavigationAction::Accept);
		AddNavigationActionMappings(
			CommonInputSettings.GetEnhancedInputBackAction(),
			EUINavigationAction::Back);
	}

	if (NavigationConfig)
	{
		NavigationConfig->SetMappedKeyEventRules(
			NewNavigationRules);
		NavigationConfig->SetMappedKeyActionRules(
			NewNavigationActionRules);
	}
}

void UReciEnhancedInputNavigationSubsystem::HandleControlMappingsRebuilt()
{
	RefreshNavigationMappings();
}

void UReciEnhancedInputNavigationSubsystem::HandleControllerIdChanged(
	const int32,
	const int32)
{
	RestoreNavigationConfig();
	RefreshNavigationMappings();
}

void UReciEnhancedInputNavigationSubsystem::HandleSlateUserRegistered(
	const int32)
{
	if (!NavigationConfig || !SlateUser.IsValid())
	{
		RestoreNavigationConfig();
		RefreshNavigationMappings();
	}
}

void UReciEnhancedInputNavigationSubsystem::InstallNavigationConfig()
{
	if (NavigationConfig || !FSlateApplication::IsInitialized())
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (LocalPlayer == nullptr)
	{
		return;
	}

	const TSharedPtr<FSlateUser> User = LocalPlayer->GetSlateUser();
	if (!User)
	{
		return;
	}

	SlateUser = User;
	PreviousNavigationConfig = User->GetUserNavigationConfig();
	const TSharedPtr<FNavigationConfig> BehaviorFallbackConfig =
		PreviousNavigationConfig
			? PreviousNavigationConfig
			: FSlateApplication::Get().GetNavigationConfig();
	// Not following, no key is mapped, so Replace has nothing to replace.
	const UReciUIInputData* InputData = UE::ReciCommonUI::Private::GetFollowedInputData();
	const UReciCommonUISettings* Settings = GetDefault<UReciCommonUISettings>();
	NavigationConfig =
		MakeShared<FReciEnhancedInputNavigationConfig>(
			BehaviorFallbackConfig,
			PreviousNavigationConfig.IsValid(),
			InputData && InputData->bMappedKeysReplaceDefaultKeys,
			Settings->bTabNavigation,
			Settings->bStickDominantAxis);
	User->SetUserNavigationConfig(NavigationConfig);
}

void UReciEnhancedInputNavigationSubsystem::RestoreNavigationConfig()
{
	const TSharedPtr<FSlateUser> User = SlateUser.Pin();
	if (User
		&& User->GetUserNavigationConfig() == NavigationConfig)
	{
		User->SetUserNavigationConfig(PreviousNavigationConfig);
	}

	SlateUser.Reset();
	PreviousNavigationConfig.Reset();
	NavigationConfig.Reset();
}
