// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DataRegistrySource.h"
#include "ExcelTypes.h"
#include "ExcelTableHeader.h"
#include "DataRegistrySource_ExcelWorksheet.generated.h"

class UExcelWorksheet;

UCLASS(Meta = (DisplayName = "Excel Worksheet Source"))
class DIRECTEXCEL_API UDataRegistrySource_ExcelWorksheet : public UDataRegistrySource
{
	GENERATED_BODY()
public:
	/** Constructor */
	UDataRegistrySource_ExcelWorksheet();

public:
	virtual bool Initialize() override; //!!!
	void SetWorksheet(UExcelWorksheet* worksheet);
	bool IsValidRowIndex(int32 rowIndex)const;
	FName GetRowName(int32 rowIndex)const;
	FString GetTitle()const;
	bool IsValidSource()const;
public:

	UPROPERTY(Transient)
		TWeakObjectPtr<UExcelWorksheet> SourceWorksheet;
protected:
	TArray<FDataRegistrySourceAcquireRequest> PendingAcquires;
protected:
	// Source interface
	virtual EDataRegistryAvailability GetSourceAvailability() const override;
	virtual EDataRegistryAvailability GetItemAvailability(const FName& ResolvedName, const uint8** PrecachedDataPtr) const override;
	virtual void GetResolvedNames(TArray<FName>& Names) const override;
	virtual void ResetRuntimeState() override;
	virtual bool AcquireItem(FDataRegistrySourceAcquireRequest&& Request) override;
	virtual FString GetDebugString() const override;
	void HandlePendingAcquires();

protected:

};


