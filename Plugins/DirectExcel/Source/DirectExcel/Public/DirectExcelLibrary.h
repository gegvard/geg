// Copyright 2018 Jianzhao Fu. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ExcelTypes.h"
#include "ExcelVariant.h"
#include "Engine/DataTable.h"
#include "ExcelCellReference.h"
#include "DirectExcelLibrary.generated.h"
/**
* Helper functions to load/save excel file
*/
UCLASS(DisplayName = "DirectExcel", Category = "DirectExcel")
class DIRECTEXCEL_API UDirectExcelLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	//async load
public:
	UFUNCTION(BlueprintCallable, Category = "DirectExcel|DataRegistry")
		static TArray<FDataRegistryId> GetAllRegistryIds(FDataRegistryType registryType);
public:
	UFUNCTION(BlueprintCallable, Category = "DirectExcel")
		static class UExcelWorkbook* LoadExcel(FString path, ExcelFileRelateiveDir relativeDir = ExcelFileRelateiveDir::Absolute);
	UFUNCTION(BlueprintCallable, Category = "DirectExcel")
		static bool SaveExcel(UExcelWorkbook* workbook, FString path, ExcelFileRelateiveDir relativeDir = ExcelFileRelateiveDir::Absolute);
	UFUNCTION(BlueprintCallable, Category = "DirectExcel")
		static class UExcelWorkbook* CreateExcel();
	UFUNCTION(BlueprintPure, Category = "DirectExcel")
		static FString ToAbsolutePath(FString projectReleativePath, ExcelFileRelateiveDir relativeDir = ExcelFileRelateiveDir::Absolute);
	UFUNCTION(BlueprintPure, Category = "DirectExcel")
		static bool DoesExcelFileExists(FString path, ExcelFileRelateiveDir relativeDir = ExcelFileRelateiveDir::Absolute);

	/**
	 * Путь к Desktop текущего пользователя.
	 * Всегда со слешами '/' и с '/' в конце.
	 * Пример: C:/Users/Name/Desktop/
	 */
	/** Версия патча. Если видишь "3.4.0-BATCH-20260720" — стоит новая сборка. */
	UFUNCTION(BlueprintPure, Category = "DirectExcel",
		meta = (DisplayName = "Get DirectExcel Version",
			ToolTip = "Must return 3.4.0-BATCH-20260720 after Rebuild. If empty/old — wrong DLL."))
		static FString GetDirectExcelVersion();

	UFUNCTION(BlueprintPure, Category = "DirectExcel|File",
		meta = (DisplayName = "Get Desktop Path",
			ToolTip = "ALWAYS returns forward slashes with trailing slash: C:/Users/Name/Desktop/"))
		static FString GetDesktopPath();

	/**
	 * Копирует Excel-файл в другой путь/папку, с опциональным переименованием.
	 * DestPath — папка или полный путь к файлу.
	 * NewFileName — новое имя (например "report_2026.xlsx"). Пусто = имя как у Source.
	 * OutCopiedPath — итоговый полный путь копии.
	 */
	UFUNCTION(BlueprintCallable, Category = "DirectExcel|File",
		meta = (DisplayName = "Copy Excel File",
			AutoCreateRefTerm = "OutCopiedPath",
			ToolTip = "Copy .xlsx. Dest = folder or file path. NewFileName optional rename."))
		static bool CopyExcelFile(
			FString SourcePath,
			FString DestPath,
			UPARAM(ref) FString& OutCopiedPath,
			FString NewFileName = TEXT(""),
			bool bOverwrite = true,
			ExcelFileRelateiveDir SourceRelativeDir = ExcelFileRelateiveDir::Absolute,
			ExcelFileRelateiveDir DestRelativeDir = ExcelFileRelateiveDir::Absolute);

	/** Перемещает Excel-файл (copy + delete source), с опциональным переименованием. */
	UFUNCTION(BlueprintCallable, Category = "DirectExcel|File",
		meta = (DisplayName = "Move Excel File",
			AutoCreateRefTerm = "OutMovedPath",
			ToolTip = "Move .xlsx. Dest = folder or file path. NewFileName optional rename."))
		static bool MoveExcelFile(
			FString SourcePath,
			FString DestPath,
			UPARAM(ref) FString& OutMovedPath,
			FString NewFileName = TEXT(""),
			bool bOverwrite = true,
			ExcelFileRelateiveDir SourceRelativeDir = ExcelFileRelateiveDir::Absolute,
			ExcelFileRelateiveDir DestRelativeDir = ExcelFileRelateiveDir::Absolute);

	/**
	 * Явная копия с переименованием в папку.
	 * DestFolder + NewFileName → итоговый файл.
	 */
	UFUNCTION(BlueprintCallable, Category = "DirectExcel|File",
		meta = (DisplayName = "Copy Excel File As",
			AutoCreateRefTerm = "OutCopiedPath",
			ToolTip = "Copy into DestFolder under NewFileName (adds .xlsx if missing)."))
		static bool CopyExcelFileAs(
			FString SourcePath,
			FString DestFolder,
			FString NewFileName,
			UPARAM(ref) FString& OutCopiedPath,
			bool bOverwrite = true,
			ExcelFileRelateiveDir SourceRelativeDir = ExcelFileRelateiveDir::Absolute,
			ExcelFileRelateiveDir DestRelativeDir = ExcelFileRelateiveDir::Absolute);

public: 
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "DirectExcel|Worksheet", meta = (DisplayName = "ReadStructAtRowIndex", CustomStructureParam = "OutItem"))
		static bool ReadStructAtRowIndex(const UExcelWorksheet* sheet, int32 row, FTableRowBase& OutItem) { return false; }
	DECLARE_FUNCTION(execReadStructAtRowIndex);

	static bool ReadStructAtRowIndexRaw(const UExcelWorksheet* sheet, int32 row, const UScriptStruct* OutStruct, void* outStructData);
public:
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "DirectExcel|Worksheet", meta = (DisplayName = "ReadStructWithRowName", CustomStructureParam = "OutItem"))
		static bool ReadStructWithRowName(const UExcelWorksheet* sheet, FName rowName, FTableRowBase& OutItem) { return false; }
	DECLARE_FUNCTION(execReadStructWithRowName);
	static bool ReadStructWithRowNameRaw(const UExcelWorksheet* sheet, FName rowName, const UScriptStruct* OutStruct, void* outStructData);

public:
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "DirectExcel|Worksheet", meta = (DisplayName = "ReadItemAtCell", CustomStructureParam = "OutItem"))
		static bool ReadItemAtCell(const UExcelWorksheet* sheet, FExcelCellReference cellReference,FTableRowBase& OutItem) { return false; }
	DECLARE_FUNCTION(execReadItemAtCell);

	/**
	 * Лучший поток экспорта:
	 * StartIndex/EndIndex — индексы полей struct (0-based, порядок объявления / как пины Break).
	 * StartColumn/EndColumn — номера колонок Excel (1-based, включительно).
	 * Поле StartIndex → колонка StartColumn, следующее подходящее поле → +1 колонка, … до EndColumn.
	 * Массивы/Map/Set (Payments, Services) пропускаются без сдвига колонки — пишите их ForEach в 30/50.
	 * Потом: Write Variant Map(Row, ColumnValues).
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "DirectExcel|Worksheet|Batch",
		meta = (DisplayName = "Append Struct Fields To Variant Map",
			CustomStructureParam = "Struct",
			AutoCreateRefTerm = "ColumnValues",
			ToolTip = "StartIndex/EndIndex = struct field indices (0-based). StartColumn/EndColumn = Excel columns (1-based)."))
		static int32 AppendStructFieldsToVariantMap(
			int32 StartIndex,
			int32 EndIndex,
			int32 StartColumn,
			int32 EndColumn,
			const FTableRowBase& Struct,
			UPARAM(ref) TMap<int32, FExcelVariant>& ColumnValues) { return 0; }
	DECLARE_FUNCTION(execAppendStructFieldsToVariantMap);

	/**
	 * Одна линия struct → одна колонка Excel.
	 * Field Index — 0-based номер поля в struct; Column — номер колонки (1-based).
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "DirectExcel|Worksheet|Batch",
		meta = (DisplayName = "Append Struct Field To Variant Map",
			CustomStructureParam = "Struct",
			AutoCreateRefTerm = "ColumnValues",
			ToolTip = "FieldIndex = struct field (0-based). Column = Excel column (1-based)."))
		static bool AppendStructFieldToVariantMap(
			int32 FieldIndex,
			int32 Column,
			const FTableRowBase& Struct,
			UPARAM(ref) TMap<int32, FExcelVariant>& ColumnValues) { return false; }
	DECLARE_FUNCTION(execAppendStructFieldToVariantMap);

	static int32 AppendStructFieldsToVariantMapRaw(
		int32 StartIndex,
		int32 EndIndex,
		int32 StartColumn,
		int32 EndColumn,
		const UScriptStruct* StructType,
		const void* StructData,
		TMap<int32, FExcelVariant>& ColumnValues);

	static bool AppendStructFieldToVariantMapRaw(
		int32 FieldIndex,
		int32 Column,
		const UScriptStruct* StructType,
		const void* StructData,
		TMap<int32, FExcelVariant>& ColumnValues);

private:
	static FName GetPropertyColumnName(const FProperty& property);
	static bool TryPropertyToExcelVariant(const FProperty* Property, const void* StructData, FExcelVariant& OutVariant);
};
