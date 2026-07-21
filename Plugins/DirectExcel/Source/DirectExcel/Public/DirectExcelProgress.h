// DirectExcel — лёгкий колбэк прогресса для сохранения (Save).
// Только std-типы: этот заголовок включается из кода xlnt, UE-типы сюда тащить нельзя.
// Колбэк хранится в thread_local, поэтому безопасен при работе в фоновом потоке.

#pragma once

#include <functional>
#include <cstdint>

namespace DirectExcelProgress
{
	void BeginSave(std::function<void(double)> callback);
	void ReportSaveRow(uint64_t currentRow, uint64_t firstRow, uint64_t lastRow);
	void EndSave();
}
