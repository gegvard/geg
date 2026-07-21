// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DataRegistrySource.h"
#include "ExcelTypes.h"
#include "ExcelTableHeader.h"
#include "DataRegistrySource_Excel.generated.h"

class UExcelWorkbook;
class UDataRegistrySource_ExcelWorksheet;

USTRUCT()
struct DIRECTEXCEL_API FDataRegistrySource_ExcelRules
{
	GENERATED_BODY()
public:
	/** True if the entire table should be loaded into memory when the source is loaded, false if the table is loaded on demand */
	UPROPERTY(EditAnywhere, Category = DataRegistry)
		bool bPrecacheExcel = true;

	/** Time in seconds to keep cached table alive if hard reference is off. 0 will release immediately, -1 will never release */
	UPROPERTY(EditAnywhere, Category = DataRegistry)
		float CachedTableKeepSeconds = -1.0;
};

/** Meta source that will generate DataTable sources at runtime based on a directory scan or asset registration */
UCLASS(Meta = (DisplayName = "Excel Source"))
class DIRECTEXCEL_API UDataRegistrySource_Excel : public UDataRegistrySource
{
	GENERATED_BODY()
public:
	/** Constructor */
	UDataRegistrySource_Excel();

	UPROPERTY(EditAnywhere, Category = DataRegistry)
		FString ExcelFilePath;
	UPROPERTY(EditAnywhere, Category = DataRegistry)
		ExcelFileRelateiveDir ExcelFileRelativeDir = ExcelFileRelateiveDir::Absolute;

	UPROPERTY(EditAnywhere, Category = DataRegistry)
		TMap<FName, FExcelTableHeader> TableHeaders;
protected:
	UPROPERTY(Transient)
		UExcelWorkbook* SourceWorkbook = nullptr;

	/** Last time this was accessed */
	mutable float LastAccessTime;

	UPROPERTY(Transient)
		TMap<FName, UDataRegistrySource_ExcelWorksheet*> RuntimeChildren;

	void LoadWorkbook(bool bForceLoad = false);

	/** Clears cached table pointer so it can be GCd */
	virtual void ClearSourceWorkbook();

	/** Callback after table loads */
	virtual bool Initialize() override; //!!!


protected:
	// Source interface
	virtual void RefreshRuntimeSources()override;
	virtual void AddRuntimeSources(TArray<UDataRegistrySource*>& OutRuntimeSources) override;

	virtual void ResetRuntimeState() override;
	virtual FString GetDebugString() const override;

	// Object interface
	virtual void PostLoad() override;	//!!!
protected:

#if WITH_EDITOR 
	virtual void EditorRefreshSource();
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
#endif
};
