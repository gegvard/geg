// Copyright 2018 Jianzhao Fu. All Rights Reserved.
// DirectExcel — реализации нод улучшения вида листа (категория DirectExcel|Beautify).
// Методы являются членами UExcelWorksheet и имеют доступ к private mData.

#include "ExcelWorksheet.h"
#include "ExcelWorkbook.h"

#include "xlnt/worksheet/worksheet.hpp"
#include "xlnt/worksheet/column_properties.hpp"
#include "xlnt/cell/cell.hpp"
#include "xlnt/cell/cell_reference.hpp"
#include "xlnt/styles/number_format.hpp"
#include "xlnt/styles/font.hpp"

// «Видимая» длина содержимого ячейки в символах.
// Для дат — длина короткого формата (даты ужимаем заранее).
// Для текста — число СИМВОЛОВ через FString (корректно для кириллицы).
// Важно: избегаем c.to_string() для чисел — на 100+ строк это заметно тормозит Game Thread.
static int32 DE_StringDisplayLen(const FString& fs)
{
	int32 best = 0;
	int32 cur = 0;
	for (const TCHAR ch : fs)
	{
		if (ch == TEXT('\n'))
		{
			best = FMath::Max(best, cur);
			cur = 0;
		}
		else if (ch != TEXT('\r'))
		{
			++cur;
		}
	}
	return FMath::Max(best, cur);
}

static int32 DE_CellDisplayLen(const xlnt::cell& c, int32 dateLen)
{
	if (!c.has_value())
	{
		return 0;
	}
	if (c.is_date())
	{
		return dateLen;
	}

	switch (c.data_type())
	{
	case xlnt::cell::type::boolean:
		return 5; // TRUE/FALSE
	case xlnt::cell::type::number:
	{
		// Оценка длины без полного форматирования number_format.
		const double v = c.value<double>();
		if (FMath::IsNearlyZero(v)) { return 1; }
		const double absV = FMath::Abs(v);
		int32 digits = (absV >= 1.0) ? (int32)FMath::FloorToInt(FMath::LogX(10.0, absV)) + 1 : 1;
		digits += 3; // знак / дробная часть с запасом
		return FMath::Clamp(digits, 1, 24);
	}
	case xlnt::cell::type::shared_string:
	case xlnt::cell::type::inline_string:
	case xlnt::cell::type::formula_string:
	{
		const std::string s = c.value<std::string>();
		return DE_StringDisplayLen(UTF8_TO_TCHAR(s.c_str()));
	}
	default:
	{
		const std::string s = c.to_string();
		return DE_StringDisplayLen(UTF8_TO_TCHAR(s.c_str()));
	}
	}
}

void UExcelWorksheet::AutoFitColumns(float CharWidthFactor, float Padding, float MinWidth, float MaxWidth, bool bIncludeHeader)
{
	if (mData == nullptr) { return; }

	const int32 lowCol = mData.lowest_column().index;
	const int32 highCol = mData.highest_column().index;
	const int32 lowRow = mData.lowest_row();
	const int32 highRow = mData.highest_row();
	if (highCol < lowCol || highRow < lowRow) { return; }

	const int32 firstDataRow = bIncludeHeader ? lowRow : (lowRow + 1);
	const int32 dateLen = 10; // "dd.mm.yyyy"

	// На очень больших листах полный проход всех строк дорогой; семплируем хвост.
	// Заголовок + первые 40 + последние 40 строк обычно достаточно для ширины.
	const int32 totalRows = highRow - firstDataRow + 1;
	const bool bSample = totalRows > 120;
	const int32 sampleHead = 40;
	const int32 sampleTail = 40;

	for (int32 col = lowCol; col <= highCol; ++col)
	{
		int32 maxLen = 0;

		auto considerRow = [&](int32 row)
		{
			const xlnt::cell_reference ref((xlnt::column_t::index_t)col, (xlnt::row_t)row);
			if (!mData.has_cell(ref)) { return; }
			maxLen = FMath::Max(maxLen, DE_CellDisplayLen(mData.cell(ref), dateLen));
		};

		if (!bSample)
		{
			for (int32 row = firstDataRow; row <= highRow; ++row)
			{
				considerRow(row);
			}
		}
		else
		{
			const int32 headEnd = FMath::Min(highRow, firstDataRow + sampleHead - 1);
			for (int32 row = firstDataRow; row <= headEnd; ++row)
			{
				considerRow(row);
			}
			const int32 tailStart = FMath::Max(headEnd + 1, highRow - sampleTail + 1);
			for (int32 row = tailStart; row <= highRow; ++row)
			{
				considerRow(row);
			}
		}

		float width = (float)maxLen * CharWidthFactor + Padding;
		width = FMath::Clamp(width, MinWidth, MaxWidth);

		xlnt::column_properties props;
		if (mData.has_column_properties((xlnt::column_t::index_t)col))
		{
			props = mData.column_properties((xlnt::column_t::index_t)col);
		}
		props.width = (double)width;
		props.custom_width = true;
		props.best_fit = false;
		mData.add_column_properties((xlnt::column_t::index_t)col, props);
	}
}

void UExcelWorksheet::SetColumnWidth(int32 columnIndex, float width, bool bBestFit)
{
	if (mData == nullptr || columnIndex < 1) { return; }

	xlnt::column_properties props;
	if (mData.has_column_properties((xlnt::column_t::index_t)columnIndex))
	{
		props = mData.column_properties((xlnt::column_t::index_t)columnIndex);
	}
	props.width = (double)width;
	props.custom_width = true;
	props.best_fit = bBestFit;
	mData.add_column_properties((xlnt::column_t::index_t)columnIndex, props);
}

void UExcelWorksheet::SetColumnWidthByString(FString columnString, float width, bool bBestFit)
{
	const std::string s = TCHAR_TO_UTF8(*columnString);
	const int32 columnIndex = (int32)xlnt::column_t::column_index_from_string(s);
	SetColumnWidth(columnIndex, width, bBestFit);
}

void UExcelWorksheet::SetColumnNumberFormat(int32 columnIndex, FString formatCode, int32 startRow)
{
	if (mData == nullptr || columnIndex < 1) { return; }

	const std::string code = TCHAR_TO_UTF8(*formatCode);
	const xlnt::number_format nf(code);

	const int32 highRow = mData.highest_row();
	const int32 from = FMath::Max(1, startRow);
	for (int32 row = from; row <= highRow; ++row)
	{
		const xlnt::cell_reference ref((xlnt::column_t::index_t)columnIndex, (xlnt::row_t)row);
		if (!mData.has_cell(ref)) { continue; }
		mData.cell(ref).number_format(nf);
	}
}

void UExcelWorksheet::FormatColumnAsShortDate(int32 columnIndex, FString dateFormat, int32 startRow)
{
	SetColumnNumberFormat(columnIndex, dateFormat, startRow);
}

void UExcelWorksheet::FormatAllDatesShort(FString dateFormat)
{
	if (mData == nullptr) { return; }

	const std::string code = TCHAR_TO_UTF8(*dateFormat);
	const xlnt::number_format nf(code);

	const int32 lowCol = mData.lowest_column().index;
	const int32 highCol = mData.highest_column().index;
	const int32 lowRow = mData.lowest_row();
	const int32 highRow = mData.highest_row();
	if (highCol < lowCol || highRow < lowRow) { return; }

	for (int32 row = lowRow; row <= highRow; ++row)
	{
		for (int32 col = lowCol; col <= highCol; ++col)
		{
			const xlnt::cell_reference ref((xlnt::column_t::index_t)col, (xlnt::row_t)row);
			if (!mData.has_cell(ref)) { continue; }
			xlnt::cell c = mData.cell(ref);
			if (c.is_date())
			{
				c.number_format(nf);
			}
		}
	}
}

void UExcelWorksheet::SetHeaderRowBold(int32 headerRow)
{
	if (mData == nullptr || headerRow < 1) { return; }

	const int32 lowCol = mData.lowest_column().index;
	const int32 highCol = mData.highest_column().index;
	if (highCol < lowCol) { return; }

	for (int32 col = lowCol; col <= highCol; ++col)
	{
		const xlnt::cell_reference ref((xlnt::column_t::index_t)col, (xlnt::row_t)headerRow);
		xlnt::cell c = mData.cell(ref);

		// ВАЖНО: не читать c.font() — если у ячейки ещё нет формата,
		// xlnt падает (format::font() разыменовывает null). Сеттер же
		// безопасен: он сам создаёт формат при необходимости.
		xlnt::font f;
		f.name("Calibri");
		f.size(11.0);
		f.bold(true);
		c.font(f);
	}
}

void UExcelWorksheet::FreezeHeader(int32 freezeBelowRow, int32 freezeRightOfColumn)
{
	if (mData == nullptr) { return; }
	const int32 r = FMath::Max(1, freezeBelowRow);
	const int32 c = FMath::Max(1, freezeRightOfColumn);
	mData.freeze_panes(xlnt::cell_reference((xlnt::column_t::index_t)c, (xlnt::row_t)r));
}

void UExcelWorksheet::EnableAutoFilter()
{
	if (mData == nullptr) { return; }
	mData.auto_filter(mData.calculate_dimension());
}

void UExcelWorksheet::BeautifyForExport(
	bool bShortDates, FString dateFormat,
	bool bBoldHeader, bool bFreezeHeader, bool bAutoFilter, bool bAutoFit,
	int32 headerRow, float MinWidth, float MaxWidth)
{
	if (mData == nullptr) { return; }

	if (bShortDates)
	{
		FormatAllDatesShort(dateFormat);
	}
	if (bBoldHeader)
	{
		SetHeaderRowBold(headerRow);
	}
	if (bAutoFilter)
	{
		EnableAutoFilter();
	}
	if (bFreezeHeader)
	{
		FreezeHeader(headerRow + 1, 1);
	}
	if (bAutoFit)
	{
		AutoFitColumns(1.15f, 2.0f, MinWidth, MaxWidth, true);
	}
}
