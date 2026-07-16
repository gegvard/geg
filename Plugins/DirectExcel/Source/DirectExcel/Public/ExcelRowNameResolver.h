// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataRegistryTypes.h"

struct FExcelRowNameResolver : public FDataRegistryResolver
{
	virtual ~FExcelRowNameResolver() = default;
	/** Override this function, if it returns true then OutResolvedName should be used, otherwise will check other resolvers and default behavior */
	virtual bool ResolveIdToName(FName& OutResolvedName, const FDataRegistryId& ItemId, const class UDataRegistry* Registry, const class UDataRegistrySource* RegistrySource) override;

	/** Return true if this resolver is considered volatile, which means that it should stop any long term caching of results. This should be true for most stack-based resolvers */
	virtual bool IsVolatile() { return true; }
};