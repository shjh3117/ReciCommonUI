// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "Settings/ReciUIInputDataAccess.h"

#include "CommonInputSettings.h"
#include "ICommonInputModule.h"
#include "ReciReport.h"
#include "Settings/ReciUIInputData.h"

namespace UE::ReciCommonUI::Private
{
	const UReciUIInputData* GetReciUIInputData()
	{
		// CommonUI loads its Input Data when its input subsystem starts, which can come after a caller such as a
		// subsystem's ShouldCreateSubsystem. Loading again does nothing.
		ICommonInputModule::GetSettings().LoadData();

		// CommonUI reads its click and Back actions from the Input Data class it loaded, which it keeps private with no
		// getter. Reading that same property by reflection keeps the directions in step with click and Back; the asset
		// path in config can name another class than the one loaded, as after Input Data changes in the editor.
		static const FObjectPropertyBase* const InputDataClassProperty =
			FindFProperty<FObjectPropertyBase>(UCommonInputSettings::StaticClass(), TEXT("InputDataClass"));
		if (!InputDataClassProperty)
		{
			RECI_WARN_ONCE(TEXT("InputDataClassNotFound"), UCommonInputSettings::StaticClass()->GetPathName(),
				TEXT("Common Input Settings has no InputDataClass property, so Reci Common UI reads no Reci UI Input "
					"Data and follows no Enhanced Input action: only Slate's default keys navigate. CommonUI has "
					"changed; update Reci Common UI."));
			return nullptr;
		}

		const UClass* InputDataClass = Cast<UClass>(
			InputDataClassProperty->GetObjectPropertyValue_InContainer(&ICommonInputModule::GetSettings()));
		return InputDataClass ? Cast<UReciUIInputData>(InputDataClass->GetDefaultObject()) : nullptr;
	}

	const UReciUIInputData* GetFollowedInputData()
	{
		const UReciUIInputData* InputData = GetReciUIInputData();
		// As with CommonUI's Enhanced Input click and Back actions, Reci follows only with its Enhanced Input support
		// on; the Input Data hides its actions and Replace otherwise.
		return UCommonInputSettings::IsEnhancedInputSupportEnabled() ? InputData : nullptr;
	}
}
