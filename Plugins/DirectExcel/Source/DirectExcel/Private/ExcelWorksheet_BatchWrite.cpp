// Copyright 2018 Jianzhao Fu. All Rights Reserved.
// DirectExcel — пакетная запись строк/матриц для экспорта больших списков (студенты и т.п.).
// Цель: убрать тысячи Blueprint-вызовов SetStringAt, из‑за которых UE «зависает» и рвёт цикл.

#include "ExcelWorksheet.h"
#include "ExcelWorkbook.h"
#include "ExcelVariant.h"

#include "xlnt/worksheet/worksheet.hpp"
#include "xlnt/cell/cell.hpp"
#include "xlnt/cell/cell_reference.hpp"
#include "xlnt/utils/datetime.hpp"

namespace DirectExcelBatch
{
	static void WriteStringToCell(xlnt::worksheet& sheet, int32 column, int32 row, const FString& value)
	{
		xlnt::cell_reference cr((xlnt::column_t::index_t)column, (xlnt::row_t)row);
		xlnt::cell c = sheet.cell(cr);
		const std::string str = TCHAR_TO_UTF8(*value);
		c.value(str);
	}

	static void WriteVariantToCell(xlnt::worksheet& sheet, int32 column, int32 row, const FExcelVariant& value)
	{
		xlnt::cell_reference cr((xlnt::column_t::index_t)column, (xlnt::row_t)row);
		xlnt::cell c = sheet.cell(cr);

		switch (value.Type())
		{
		case ExcelVariantType::Bool:
			c.value(value.BoolValue());
			break;
		case ExcelVariantType::Int32:
			c.value(value.IntValue());
			break;
		case ExcelVariantType::String:
		{
			const std::string str = TCHAR_TO_UTF8(*value.StringValue());
			c.value(str);
			break;
		}
		case ExcelVariantType::DateTime:
		{
			const FDateTime dt = value.DateTimeValue();
			xlnt::datetime d(dt.GetYear(), dt.GetMonth(), dt.GetDay(), dt.GetHour(), dt.GetMinute(), dt.GetSecond(), dt.GetMillisecond());
			c.value(d);
			break;
		}
		default:
			// None / Array — пустая ячейка
			break;
		}
	}
}

void UExcelWorksheet::ReserveCapacity(int32 estimatedCells)
{
	if (mData == nullptr || estimatedCells <= 0)
	{
		return;
	}
	mData.reserve((std::size_t)estimatedCells);
}

void UExcelWorksheet::WriteStringRow(int32 row, int32 startColumn, const TArray<FString>& values)
{
	if (mData == nullptr || values.Num() == 0 || row < 1 || startColumn < 1)
	{
		return;
	}

	mData.reserve((std::size_t)FMath::Max(row * (startColumn + values.Num()), values.Num()));

	for (int32 i = 0; i < values.Num(); ++i)
	{
		DirectExcelBatch::WriteStringToCell(mData, startColumn + i, row, values[i]);
	}
}

void UExcelWorksheet::WriteIntRow(int32 row, int32 startColumn, const TArray<int32>& values)
{
	if (mData == nullptr || values.Num() == 0 || row < 1 || startColumn < 1)
	{
		return;
	}

	mData.reserve((std::size_t)FMath::Max(row * (startColumn + values.Num()), values.Num()));

	for (int32 i = 0; i < values.Num(); ++i)
	{
		xlnt::cell_reference cr((xlnt::column_t::index_t)(startColumn + i), (xlnt::row_t)row);
		mData.cell(cr).value(values[i]);
	}
}

void UExcelWorksheet::WriteFloatRow(int32 row, int32 startColumn, const TArray<float>& values)
{
	if (mData == nullptr || values.Num() == 0 || row < 1 || startColumn < 1)
	{
		return;
	}

	mData.reserve((std::size_t)FMath::Max(row * (startColumn + values.Num()), values.Num()));

	for (int32 i = 0; i < values.Num(); ++i)
	{
		xlnt::cell_reference cr((xlnt::column_t::index_t)(startColumn + i), (xlnt::row_t)row);
		mData.cell(cr).value((double)values[i]);
	}
}

void UExcelWorksheet::WriteStringMatrix(int32 startRow, int32 startColumn, int32 columnCount, const TArray<FString>& values)
{
	if (mData == nullptr || values.Num() == 0 || startRow < 1 || startColumn < 1 || columnCount < 1)
	{
		return;
	}

	const int32 rowCount = values.Num() / columnCount;
	if (rowCount <= 0)
	{
		UE_LOG(LogDirectExcel, Warning, TEXT("WriteStringMatrix: values.Num()=%d is smaller than columnCount=%d"), values.Num(), columnCount);
		return;
	}

	if (values.Num() % columnCount != 0)
	{
		UE_LOG(LogDirectExcel, Warning, TEXT("WriteStringMatrix: values.Num()=%d is not divisible by columnCount=%d; trailing cells ignored."), values.Num(), columnCount);
	}

	mData.reserve((std::size_t)(rowCount * columnCount));

	int32 index = 0;
	for (int32 r = 0; r < rowCount; ++r)
	{
		const int32 excelRow = startRow + r;
		for (int32 c = 0; c < columnCount; ++c)
		{
			DirectExcelBatch::WriteStringToCell(mData, startColumn + c, excelRow, values[index++]);
		}
	}
}

void UExcelWorksheet::WriteVariantRow(int32 row, int32 startColumn, const TArray<FExcelVariant>& values)
{
	if (mData == nullptr || values.Num() == 0 || row < 1 || startColumn < 1)
	{
		return;
	}

	mData.reserve((std::size_t)FMath::Max(row * (startColumn + values.Num()), values.Num()));

	for (int32 i = 0; i < values.Num(); ++i)
	{
		DirectExcelBatch::WriteVariantToCell(mData, startColumn + i, row, values[i]);
	}
}
