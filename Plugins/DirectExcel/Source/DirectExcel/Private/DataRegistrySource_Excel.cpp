// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataRegistrySource_Excel.h"
#include "DataRegistrySettings.h"
#include "Interfaces/ITargetPlatform.h"
#include "Engine/AssetManager.h"
#include "DirectExcelLibrary.h"
#include "LogTypes.h"
#include "ExcelWorkbook.h"
#include "DataRegistrySource_ExcelWorksheet.h"

UDataRegistrySource_Excel::UDataRegistrySource_Excel()
{

}

void UDataRegistrySource_Excel::LoadWorkbook(bool bForceLoad /*= false*/)
{
	if (bForceLoad || SourceWorkbook == nullptr)
	{
		SourceWorkbook = UDirectExcelLibrary::LoadExcel(ExcelFilePath, ExcelFileRelativeDir);	//async load?
		if (!SourceWorkbook)
		{
			UE_LOG(LogDirectExcel, Error, TEXT("Cannot load DataRegistry source %s, Cannot load excel:%s"), *GetPathName(), *ExcelFilePath);
			return;
		}
	}

	if (SourceWorkbook != nullptr)
	{
		const TArray<UExcelWorksheet*>& allSheets = SourceWorkbook->AllSheets();
		for (UExcelWorksheet* sheet : allSheets)
		{
			FName titleName = FName(sheet->Title());
			FExcelTableHeader& header = TableHeaders.FindOrAdd(titleName);
			header.RowStruct = GetItemStruct();
			sheet->SetTableHeader(header);
		}
	}

	LastAccessTime = UDataRegistry::GetCurrentTime();
}

void UDataRegistrySource_Excel::ClearSourceWorkbook()
{
	SourceWorkbook = nullptr;
}

void UDataRegistrySource_Excel::PostLoad()
{
	Super::PostLoad();

	LoadWorkbook(false);
}


void UDataRegistrySource_Excel::RefreshRuntimeSources()
{
	if (!IsInitialized())
	{
		return;
	}

	LoadWorkbook(false);

	const TArray<UExcelWorksheet*>& allSheets = SourceWorkbook->AllSheets();
	for (UExcelWorksheet* sheet : allSheets)
	{
		FName titleName = FName(sheet->Title());
		UDataRegistrySource_ExcelWorksheet** FoundSource = RuntimeChildren.Find(titleName);
		FExcelTableHeader* header = TableHeaders.Find(titleName);
		if (header == nullptr || !header->Enabled)
		{
			if (FoundSource && *FoundSource)
			{
				(*FoundSource)->SetWorksheet(nullptr);
			}
			continue;
		}

		if (FoundSource && *FoundSource)
		{
			(*FoundSource)->SetWorksheet(sheet);
		}
		else
		{
			UDataRegistrySource_ExcelWorksheet* NewSource = (UDataRegistrySource_ExcelWorksheet*)CreateTransientSource(UDataRegistrySource_ExcelWorksheet::StaticClass());
			NewSource->SetWorksheet(sheet);
			RuntimeChildren.Add(titleName, NewSource);

			NewSource->Initialize();
		}
	}

	for (auto MapIt = RuntimeChildren.CreateIterator(); MapIt; ++MapIt)
	{
		UDataRegistrySource_ExcelWorksheet* source = MapIt.Value();
		if (!source->IsValidSource())
		{
			source->Deinitialize();
			MapIt.RemoveCurrent();
		}
	}

}

void UDataRegistrySource_Excel::AddRuntimeSources(TArray<UDataRegistrySource*>& OutRuntimeSources)
{
	if (IsInitialized())
	{
		for (const auto& kv : RuntimeChildren)
		{
			OutRuntimeSources.Add(kv.Value);
		}
	}
}

void UDataRegistrySource_Excel::ResetRuntimeState()
{
	ClearSourceWorkbook();

	Super::ResetRuntimeState();
}

FString UDataRegistrySource_Excel::GetDebugString() const
{
	const UDataRegistry* Registry = GetRegistry();
	//if (!SourceWorkbook.isv() && Registry)
	{
		//return FString::Printf(TEXT("%s(%d)"), *SourceWorkbook.GetAssetName(), Registry->GetSourceIndex(this));
	}
	return Super::GetDebugString();
}

bool UDataRegistrySource_Excel::Initialize()
{
	if (Super::Initialize())
	{
		// Add custom logic

		return true;
	}

	return false;
}

#if WITH_EDITOR

void UDataRegistrySource_Excel::EditorRefreshSource()
{
	LoadWorkbook(true);
}
void UDataRegistrySource_Excel::PreSave(const ITargetPlatform* TargetPlatform)
{
	Super::PreSave(TargetPlatform);
	LoadWorkbook(true);
}

#endif // WITH_EDITOR