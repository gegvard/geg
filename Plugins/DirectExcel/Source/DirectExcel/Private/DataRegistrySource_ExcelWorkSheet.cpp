// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataRegistrySource_ExcelWorksheet.h"
#include "DataRegistrySettings.h"
#include "Interfaces/ITargetPlatform.h"
#include "Engine/AssetManager.h"
#include "DirectExcelLibrary.h"
#include "LogTypes.h"
#include "ExcelWorkbook.h"
#include "ExcelWorksheetDataTable.h"


UDataRegistrySource_ExcelWorksheet::UDataRegistrySource_ExcelWorksheet()
{

}

bool UDataRegistrySource_ExcelWorksheet::Initialize()
{
	if (Super::Initialize())
	{
		// Add custom logic

		return true;
	}

	return false;
}

void UDataRegistrySource_ExcelWorksheet::SetWorksheet(UExcelWorksheet* worksheet)
{
	SourceWorksheet = worksheet;
}

bool UDataRegistrySource_ExcelWorksheet::IsValidRowIndex(int32 rowIndex) const
{
	return SourceWorksheet.IsValid()&&SourceWorksheet->IsValidRow(rowIndex);
}

FName UDataRegistrySource_ExcelWorksheet::GetRowName(int32 rowIndex) const
{
	if (IsValidRowIndex(rowIndex))
	{
		FString str= SourceWorksheet->ToStringAt(SourceWorksheet->GetTableHeader().ColumnHeaderStartColumnIndex, rowIndex);
		return FName(str);
	}
	return NAME_None;
}

FString UDataRegistrySource_ExcelWorksheet::GetTitle() const
{
	return SourceWorksheet->Title();
}

bool UDataRegistrySource_ExcelWorksheet::IsValidSource() const
{
	return SourceWorksheet!=nullptr&& SourceWorksheet->HasValidDataTable();
}

EDataRegistryAvailability UDataRegistrySource_ExcelWorksheet::GetSourceAvailability() const
{
	if (IsValidSource())
	{
		return EDataRegistryAvailability::PreCached;
	}
	return EDataRegistryAvailability::DoesNotExist;
}

EDataRegistryAvailability UDataRegistrySource_ExcelWorksheet::GetItemAvailability(const FName& ResolvedName, const uint8** PrecachedDataPtr) const
{
	if (IsValidSource())
	{
		const UScriptStruct* ItemStruct = GetItemStruct();

		const UExcelWorksheetDataTable* dataTable= SourceWorksheet->GetOrCreateDataTable();
		uint8* FoundRow = dataTable->FindRowUnchecked(ResolvedName);

		if (FoundRow)
		{
			// Return struct if found
			if (PrecachedDataPtr)
			{
				*PrecachedDataPtr = FoundRow;
			}

			return EDataRegistryAvailability::PreCached;
		}
		else
		{
			return EDataRegistryAvailability::DoesNotExist;
		}
	}
	else
	{
		return EDataRegistryAvailability::Unknown;
	}
}

void UDataRegistrySource_ExcelWorksheet::GetResolvedNames(TArray<FName>& Names) const
{
	if (IsValidSource())
	{
		const UExcelWorksheetDataTable* dataTable = SourceWorksheet->GetOrCreateDataTable();
		if (dataTable)
		{
			Names = dataTable->GetRowNames();
		}
	}
}

void UDataRegistrySource_ExcelWorksheet::ResetRuntimeState()
{
	SourceWorksheet->ClearDataTable();

	Super::ResetRuntimeState();
}

bool UDataRegistrySource_ExcelWorksheet::AcquireItem(FDataRegistrySourceAcquireRequest&& Request)
{
	if (IsValidSource())
	{
		PendingAcquires.Add(Request);
		// Tell it to go next frame
		FStreamableHandle::ExecuteDelegate(FStreamableDelegate::CreateUObject(this, &UDataRegistrySource_ExcelWorksheet::HandlePendingAcquires));
		return true;
	}

	return false;
}

FString UDataRegistrySource_ExcelWorksheet::GetDebugString() const
{
	const UDataRegistry* Registry = GetRegistry();
	if (IsValidSource() && Registry)
	{
		return FString::Printf(TEXT("ExcelWorksheet:%s(%d)"), *SourceWorksheet->Title(), Registry->GetSourceIndex(this));
	}
	return Super::GetDebugString();
}

void UDataRegistrySource_ExcelWorksheet::HandlePendingAcquires()
{
	// Iterate manually to deal with recursive adds
	int32 NumRequests = PendingAcquires.Num();
	for (int32 i = 0; i < NumRequests; i++)
	{
		// Make a copy in case array changes
		FDataRegistrySourceAcquireRequest Request = PendingAcquires[i];

		uint8 Sourceindex = 255;
		FName ResolvedName;

		if (Request.Lookup.GetEntry(Sourceindex, ResolvedName, Request.LookupIndex))
		{
			if (SourceWorksheet.IsValid())
			{
				const UExcelWorksheetDataTable* dataTable = SourceWorksheet->GetOrCreateDataTable();

				const UScriptStruct* ItemStruct = GetItemStruct();
				if (ensure(ItemStruct && ItemStruct->GetStructureSize()))
				{
					uint8* FoundRow = dataTable->FindRowUnchecked(ResolvedName);

					if (FoundRow)
					{
						// Allocate new copy of struct, will be handed off to cache

						uint8* ItemStructMemory = (uint8*)FMemory::Malloc(ItemStruct->GetStructureSize());
						ItemStruct->InitializeStruct(ItemStructMemory);
						ItemStruct->CopyScriptStruct(ItemStructMemory, FoundRow);

						HandleAcquireResult(Request, EDataRegistryAcquireStatus::InitialAcquireFinished, ItemStructMemory);
						continue;
					}
				}
			}
		}
		else
		{
			// Invalid request
		}

		// Acquire failed for some reason, report failure for each one
		HandleAcquireResult(Request, EDataRegistryAcquireStatus::AcquireError, nullptr);

	}

	PendingAcquires.RemoveAt(0, NumRequests);
}