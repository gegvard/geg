// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ExcelRowNameResolver.h"
#include "DataRegistrySource_ExcelWorksheet.h"

bool FExcelRowNameResolver::ResolveIdToName(FName& OutResolvedName, const FDataRegistryId& ItemId, const class UDataRegistry* Registry, const class UDataRegistrySource* RegistrySource)
{
	//[SheetTitle.](RowName/RowIndex)
	const UDataRegistrySource_ExcelWorksheet* sheetSource = Cast<const UDataRegistrySource_ExcelWorksheet>(RegistrySource);
	if (sheetSource == nullptr)
	{
		return false;
	}

	FString ItemNameString = ItemId.ItemName.ToString();
	FString RowNameString;
	FString* RowNameStringPtr = &ItemNameString;

	if (ItemNameString.Contains(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd))
	{
		//SheetTitle.RowIndex/SheetTitle.RowName
		FString titleString;
		ItemNameString.Split(TEXT("."), &titleString, &RowNameString, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

		if (titleString != sheetSource->GetTitle())
		{
			return false;
		}
		RowNameStringPtr = &RowNameString;
	}

	if (RowNameStringPtr->IsNumeric())
	{
		//1,2,3....
		TArray<FName> outNames;
		RegistrySource->GetResolvedNames(outNames);

		int32 rowIndex = FCString::Atoi(**RowNameStringPtr);
		OutResolvedName = sheetSource->GetRowName(rowIndex);

		return OutResolvedName.IsValid();
	}
	else
	{
		OutResolvedName = FName(*RowNameStringPtr);
	}

	return false;
}
