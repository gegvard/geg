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
	 * Копирует Excel-файл в другой путь/папку.
	 * DestPath может быть полным путём к файлу или папке (тогда имя берётся из Source).
	 * bOverwrite = true — перезаписать, если файл уже есть.
	 * При необходимости создаёт папки назначения.
	 */
	UFUNCTION(BlueprintCallable, Category = "DirectExcel|File",
		meta = (DisplayName = "Copy Excel File",
			ToolTip = "Copy .xlsx to another folder/path. Dest can be a file path or a directory."))
		static bool CopyExcelFile(
			FString SourcePath,
			FString DestPath,
			ExcelFileRelateiveDir SourceRelativeDir = ExcelFileRelateiveDir::Absolute,
			ExcelFileRelateiveDir DestRelativeDir = ExcelFileRelateiveDir::Absolute,
			bool bOverwrite = true);

	/** Перемещает Excel-файл (copy + delete source). */
	UFUNCTION(BlueprintCallable, Category = "DirectExcel|File",
		meta = (DisplayName = "Move Excel File"))
		static bool MoveExcelFile(
			FString SourcePath,
			FString DestPath,
			ExcelFileRelateiveDir SourceRelativeDir = ExcelFileRelateiveDir::Absolute,
			ExcelFileRelateiveDir DestRelativeDir = ExcelFileRelateiveDir::Absolute,
			bool bOverwrite = true);

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
