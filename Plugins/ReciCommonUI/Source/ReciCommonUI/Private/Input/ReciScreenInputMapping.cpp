// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Input/ReciScreenInputMapping.h"

#include "CommonActivatableWidget.h"
#include "Framework/Application/NavigationConfig.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "InputAction.h"
#include "InputMappingContext.h"

namespace UE::ReciCommonUI::Private
{
	namespace
	{
		/**
		 * Names CommonUI's protected InputMapping through a derived class, which C++ allows when it forms a pointer to
		 * the member ([class.protected]); the pointer then reads the member of any activatable widget. Never
		 * instantiated. If CommonUI adds a getter, use it instead.
		 */
		struct FActivatableInputMappingAccess : UCommonActivatableWidget
		{
			static const UInputMappingContext* Get(const UCommonActivatableWidget& Screen)
			{
				constexpr TObjectPtr<UInputMappingContext> UCommonActivatableWidget::* InputMappingMember =
					&FActivatableInputMappingAccess::InputMapping;
				return Screen.*InputMappingMember;
			}
		};

		struct FStickAxis
		{
			FKey Axis;
			FKey Negative;
			FKey Positive;
		};

		/** The platform's stick axes, the left stick's X and Y then the right stick's, with their stick-direction keys. */
		TConstArrayView<FStickAxis> GetStickAxes()
		{
			static const FStickAxis Axes[] = {
				{EKeys::Gamepad_LeftX, EKeys::Gamepad_LeftStick_Left, EKeys::Gamepad_LeftStick_Right},
				{EKeys::Gamepad_LeftY, EKeys::Gamepad_LeftStick_Down, EKeys::Gamepad_LeftStick_Up},
				{EKeys::Gamepad_RightX, EKeys::Gamepad_RightStick_Left, EKeys::Gamepad_RightStick_Right},
				{EKeys::Gamepad_RightY, EKeys::Gamepad_RightStick_Down, EKeys::Gamepad_RightStick_Up}};
			return Axes;
		}
	}

	TSharedPtr<FNavigationConfig> ResolveNavigationConfig(const uint32 UserIndex)
	{
		if (!FSlateApplication::IsInitialized())
		{
			return nullptr;
		}
		if (const TSharedPtr<const FSlateUser> User = FSlateApplication::Get().GetUser(UserIndex))
		{
			if (const TSharedPtr<FNavigationConfig> UserConfig = User->GetUserNavigationConfig())
			{
				return UserConfig;
			}
		}
		return FSlateApplication::Get().GetNavigationConfig();
	}

	bool GetStickDirectionKeys(const FKey& Axis, FKey& OutNegative, FKey& OutPositive)
	{
		for (const FStickAxis& Entry : GetStickAxes())
		{
			if (Entry.Axis == Axis)
			{
				OutNegative = Entry.Negative;
				OutPositive = Entry.Positive;
				return true;
			}
		}
		return false;
	}

	bool GetStickAxis(const FKey& Key, int32& OutStick, int32& OutAxis, int32& OutEnd)
	{
		const TConstArrayView<FStickAxis> Axes = GetStickAxes();
		for (int32 Index = 0; Index < Axes.Num(); ++Index)
		{
			if (Axes[Index].Axis == Key || Axes[Index].Negative == Key || Axes[Index].Positive == Key)
			{
				OutStick = Index / 2;
				OutAxis = Index % 2;
				OutEnd = Axes[Index].Negative == Key ? -1 : Axes[Index].Positive == Key ? 1 : 0;
				return true;
			}
		}
		return false;
	}

	bool IsStickDirectionKey(const FKey& Key)
	{
		for (const FKey& Axis : {EKeys::Gamepad_LeftX, EKeys::Gamepad_LeftY, EKeys::Gamepad_RightX, EKeys::Gamepad_RightY})
		{
			FKey Negative;
			FKey Positive;
			if (GetStickDirectionKeys(Axis, Negative, Positive) && (Key == Negative || Key == Positive))
			{
				return true;
			}
		}
		return false;
	}

	const UInputMappingContext* GetScreenInputMapping(const UCommonActivatableWidget& Screen)
	{
		return FActivatableInputMappingAccess::Get(Screen);
	}

	TArray<UInputAction*> GetScreenActions(const UCommonActivatableWidget& Screen)
	{
		TArray<UInputAction*> Actions;
		if (const UInputMappingContext* Mapping = GetScreenInputMapping(Screen))
		{
			for (const FEnhancedActionKeyMapping& KeyMapping : Mapping->GetMappings())
			{
				if (KeyMapping.Action)
				{
					Actions.AddUnique(const_cast<UInputAction*>(KeyMapping.Action.Get()));
				}
			}
		}
		return Actions;
	}
}
