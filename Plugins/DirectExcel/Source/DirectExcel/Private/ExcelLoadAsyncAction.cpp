// DirectExcel — асинхронная загрузка (реализация).

#include "ExcelLoadAsyncAction.h"
#include "ExcelWorkbook.h"
#include "DirectExcelLibrary.h"
#include "LogTypes.h"
#include "Misc/FileHelper.h"
#include "Async/Async.h"

#include "xlnt/workbook/workbook.hpp"

UExcelLoadAsyncAction* UExcelLoadAsyncAction::LoadExcelAsync(
	FString Path,
	ExcelFileRelateiveDir RelativeDir /*= ExcelFileRelateiveDir::Absolute*/)
{
	UExcelLoadAsyncAction* Action = NewObject<UExcelLoadAsyncAction>();
	Action->PathArg = Path;
	Action->RelativeDirArg = RelativeDir;
	return Action;
}

void UExcelLoadAsyncAction::Activate()
{
	// Абсолютный путь считаем на игровом потоке (FPaths).
	const FString AbsPath = UDirectExcelLibrary::ToAbsolutePath(PathArg, RelativeDirArg);
	const double startSec = FPlatformTime::Seconds();

	// Держим экшен живым до завершения фоновой задачи.
	AddToRoot();
	TWeakObjectPtr<UExcelLoadAsyncAction> WeakThis(this);

	Async(EAsyncExecution::ThreadPool, [AbsPath, startSec, WeakThis]()
	{
		// Чтение файла и парсинг xlsx — в фоне (без UObject).
		xlnt::workbook* parsed = nullptr;
		TArray<uint8> fileData;
		if (FFileHelper::LoadFileToArray(fileData, *AbsPath, FILEREAD_AllowWrite))
		{
			std::vector<std::uint8_t> data;
			data.assign(fileData.GetData(), fileData.GetData() + fileData.Num());

			xlnt::workbook* wb = new xlnt::workbook();
			if (wb->load(data))
			{
				parsed = wb;
			}
			else
			{
				delete wb;
			}
		}

		const double ms = (FPlatformTime::Seconds() - startSec) * 1000.0;

		// Обёртка в UObject и делегаты — на игровом потоке.
		AsyncTask(ENamedThreads::GameThread, [WeakThis, parsed, ms, AbsPath]()
		{
			UExcelWorkbook* workbook = nullptr;
			if (parsed != nullptr)
			{
				workbook = NewObject<UExcelWorkbook>();
				workbook->AdoptLoadedWorkbook(parsed, AbsPath);
			}

			UE_LOG(LogDirectExcel, Log, TEXT("LoadExcelAsync: %.1f ms (ok=%d) -> %s"),
				ms, workbook ? 1 : 0, *AbsPath);

			if (WeakThis.IsValid())
			{
				if (workbook != nullptr)
				{
					WeakThis->OnLoaded.Broadcast(workbook, (float)ms);
				}
				else
				{
					WeakThis->OnFailed.Broadcast(nullptr, (float)ms);
				}
				WeakThis->RemoveFromRoot();
				WeakThis->SetReadyToDestroy();
			}
			else if (parsed != nullptr && workbook == nullptr)
			{
				delete parsed;
			}
		});
	});
}
