// Copyright (c) 2026 shjh3117. Licensed under the MIT License; see LICENSE.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ReciTwoLevelEntry.generated.h"

/** Child data assignment plus optional design hooks for either level. */
UINTERFACE(BlueprintType, Blueprintable)
class RECICOMMONUIWIDGETS_API UReciTwoLevelEntry : public UInterface
{
	GENERATED_BODY()
};

class RECICOMMONUIWIDGETS_API IReciTwoLevelEntry
{
	GENERATED_BODY()
public:
	/** Children read their data from Parent using a zero-based index. No child UObject is created. */
	UFUNCTION(BlueprintNativeEvent, Category = "Reci Common UI|Two Level Entry")
	void SetChildItem(UObject* Parent, int32 ChildIndex);
	virtual void SetChildItem_Implementation(UObject*, int32) {}

	/** Both normal and main designs receive the same state. ChildIndex=-1 denotes a parent design. */
	UFUNCTION(BlueprintNativeEvent, Category = "Reci Common UI|Two Level Entry")
	void UpdateTwoLevelState(bool bParentFocused, bool bChildSelected, int32 ParentIndex, int32 ChildIndex);
	virtual void UpdateTwoLevelState_Implementation(bool, bool, int32, int32) {}
};
