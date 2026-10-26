// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#include "ReciReport.h"

DEFINE_LOG_CATEGORY(LogReciCommonUI);

namespace UE::ReciCommonUI
{
	namespace
	{
		TSet<FString>& GetReports()
		{
			static TSet<FString> Reports;
			return Reports;
		}
	}

	bool ShouldReportOnce(const FName Topic, const FString& Key)
	{
		bool bAlreadyReported = false;
		GetReports().Add(Topic.ToString() + TEXT(": ") + Key, &bAlreadyReported);
		return !bAlreadyReported;
	}

	FString GetAuthoredPath(const UObject& Object)
	{
		const UObject* Archetype = Object.GetArchetype();
		return Archetype ? Archetype->GetPathName() : Object.GetPathName();
	}
}

namespace UE::ReciCommonUI::Private
{
#if WITH_DEV_AUTOMATION_TESTS
	TSet<FString> ExchangeReportsForTests(TSet<FString> Reports)
	{
		Swap(GetReports(), Reports);
		return Reports;
	}
#endif
}
