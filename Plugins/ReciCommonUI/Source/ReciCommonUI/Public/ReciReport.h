// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.

#pragma once

#include "CoreMinimal.h"

/**
 * Where Reci reports a configuration it cannot serve (DESIGN.md, Reporting).
 * Widgets built on the core report here too, through the macros below, so one category and one set of reports covers
 * them all.
 */
RECICOMMONUI_API DECLARE_LOG_CATEGORY_EXTERN(LogReciCommonUI, Log, All);

namespace UE::ReciCommonUI
{
	/**
	 * True the first time this process reports Key under Topic, so a setup seen on every input or rebuild is reported
	 * once. Key names the setup, for example a class path, or a class and a property. Game thread only.
	 */
	RECICOMMONUI_API bool ShouldReportOnce(FName Topic, const FString& Key);

	/** A name for the authored widget: the path of its archetype, the same for every instance of one placed widget. */
	RECICOMMONUI_API FString GetAuthoredPath(const UObject& Object);
}

namespace UE::ReciCommonUI::Private
{
	using UE::ReciCommonUI::GetAuthoredPath;
	using UE::ReciCommonUI::ShouldReportOnce;

#if WITH_DEV_AUTOMATION_TESTS
	/** Tests: swaps the reports made so far for Reports, and returns the previous set for the test to put back. */
	RECICOMMONUI_API TSet<FString> ExchangeReportsForTests(TSet<FString> Reports);
#endif
}

/** Logs to LogReciCommonUI at Verbosity the first time this process reports Key under Topic. */
#define RECI_LOG_ONCE(Verbosity, Topic, Key, Format, ...) \
	do \
	{ \
		if (UE::ReciCommonUI::ShouldReportOnce(Topic, Key)) \
		{ \
			UE_LOG(LogReciCommonUI, Verbosity, Format, ##__VA_ARGS__); \
		} \
	} while (false)

/** Logs a warning to LogReciCommonUI the first time this process reports Key under Topic. */
#define RECI_WARN_ONCE(Topic, Key, Format, ...) RECI_LOG_ONCE(Warning, Topic, Key, Format, ##__VA_ARGS__)

/** Logs an error to LogReciCommonUI the first time this process reports Key under Topic: a setup that disables a control. */
#define RECI_ERROR_ONCE(Topic, Key, Format, ...) RECI_LOG_ONCE(Error, Topic, Key, Format, ##__VA_ARGS__)
