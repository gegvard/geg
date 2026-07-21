// DirectExcel — реализация колбэка прогресса сохранения (см. DirectExcelProgress.h).

#include "DirectExcelProgress.h"

namespace DirectExcelProgress
{
	static thread_local std::function<void(double)> GSaveCb;
	static thread_local double GSaveLast = -1.0;

	void BeginSave(std::function<void(double)> callback)
	{
		GSaveCb = std::move(callback);
		GSaveLast = -1.0;
	}

	void ReportSaveRow(uint64_t currentRow, uint64_t firstRow, uint64_t lastRow)
	{
		if (!GSaveCb)
		{
			return;
		}
		double frac = (lastRow > firstRow)
			? double(currentRow - firstRow) / double(lastRow - firstRow)
			: 1.0;
		if (frac < 0.0) { frac = 0.0; }
		if (frac > 1.0) { frac = 1.0; }
		// Троттлинг: не чаще, чем каждый +1% (и обязательно 100%).
		if (GSaveLast < 0.0 || (frac - GSaveLast) >= 0.01 || frac >= 1.0)
		{
			GSaveLast = frac;
			GSaveCb(frac);
		}
	}

	void EndSave()
	{
		if (GSaveCb)
		{
			GSaveCb(1.0);
		}
		GSaveCb = nullptr;
		GSaveLast = -1.0;
	}
}
