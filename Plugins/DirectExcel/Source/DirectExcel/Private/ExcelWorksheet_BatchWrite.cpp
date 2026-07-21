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
		case ExcelVariantType::Float:
			c.value((double)value.FloatValue());
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
	// ВНИМАНИЕ: xlnt reserve(n) резервирует число СТРОК в cell_map (не ячеек).
	// Вызывать ОДИН раз перед экспортом. Небольшой перезаказ не вреден.
	mData.reserve((std::size_t)estimatedCells);
}

void UExcelWorksheet::WriteStringRow(int32 row, int32 startColumn, const TArray<FString>& values)
{
	if (mData == nullptr || values.Num() == 0 || row < 1 || startColumn < 1)
	{
		return;
	}

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

	mData.reserve((std::size_t)FMath::Max(rowCount, 1));

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

	for (int32 i = 0; i < values.Num(); ++i)
	{
		DirectExcelBatch::WriteVariantToCell(mData, startColumn + i, row, values[i]);
	}
}

void UExcelWorksheet::WriteVariantMap(int32 row, const TMap<int32, FExcelVariant>& columnValues)
{
	if (mData == nullptr || columnValues.Num() == 0 || row < 1)
	{
		return;
	}

	for (const TPair<int32, FExcelVariant>& pair : columnValues)
	{
		const int32 column = pair.Key;
		if (column < 1)
		{
			UE_LOG(LogDirectExcel, Warning, TEXT("WriteVariantMap: skip invalid column %d (must be >= 1)"), column);
			continue;
		}
		if (pair.Value.Type() == ExcelVariantType::None)
		{
			continue;
		}
		DirectExcelBatch::WriteVariantToCell(mData, column, row, pair.Value);
	}
}

void UExcelWorksheet::WriteVariantAt(int32 row, int32 column, const FExcelVariant& value)
{
	if (mData == nullptr || row < 1 || column < 1)
	{
		return;
	}
	if (value.Type() == ExcelVariantType::None)
	{
		return;
	}
	DirectExcelBatch::WriteVariantToCell(mData, column, row, value);
}

int32 UExcelWorksheet::GetHighestRow() const
{
	if (mData == nullptr) { return 0; }
	const int32 low = mData.lowest_row();
	const int32 high = mData.highest_row();
	return (high < low) ? 0 : high;
}

int32 UExcelWorksheet::GetHighestColumn() const
{
	if (mData == nullptr) { return 0; }
	const int32 low = mData.lowest_column().index;
	const int32 high = mData.highest_column().index;
	return (high < low) ? 0 : high;
}

int32 UExcelWorksheet::GetNonEmptyCellCount() const
{
	if (mData == nullptr) { return 0; }

	const int32 lowCol = mData.lowest_column().index;
	const int32 highCol = mData.highest_column().index;
	const int32 lowRow = mData.lowest_row();
	const int32 highRow = mData.highest_row();
	if (highCol < lowCol || highRow < lowRow) { return 0; }

	const int64 boundingBox = (int64)(highRow - lowRow + 1) * (int64)(highCol - lowCol + 1);
	if (boundingBox > 5000000)
	{
		// Слишком большой bounding box — не сканируем поячеечно, чтобы не тормозить.
		return -1;
	}

	int32 count = 0;
	for (int32 row = lowRow; row <= highRow; ++row)
	{
		for (int32 col = lowCol; col <= highCol; ++col)
		{
			const xlnt::cell_reference ref((xlnt::column_t::index_t)col, (xlnt::row_t)row);
			if (mData.has_cell(ref))
			{
				++count;
			}
		}
	}
	return count;
}

void UExcelWorksheet::LogSheetStats(FString label)
{
	const int32 rows = GetHighestRow();
	const int32 cols = GetHighestColumn();
	const int32 cells = GetNonEmptyCellCount();
	const FString cellsStr = cells < 0 ? TEXT("(too large to count)") : FString::FromInt(cells);
	UE_LOG(LogDirectExcel, Log, TEXT("[STATS] %s | rows=%d cols=%d nonEmptyCells=%s"),
		*label, rows, cols, *cellsStr);
}
