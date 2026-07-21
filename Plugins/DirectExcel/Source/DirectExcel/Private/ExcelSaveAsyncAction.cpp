// DirectExcel — асинхронное сохранение (реализация).

#include "ExcelSaveAsyncAction.h"
#include "ExcelWorkbook.h"
#include "DirectExcelLibrary.h"
#include "LogTypes.h"
#include "Misc/FileHelper.h"
#include "Async/Async.h"

UExcelSaveAsyncAction* UExcelSaveAsyncAction::SaveExcelAsync(
	UExcelWorkbook* Workbook,
	FString Path,
	ExcelFileRelateiveDir RelativeDir /*= ExcelFileRelateiveDir::Absolute*/)
{
	UExcelSaveAsyncAction* Action = NewObject<UExcelSaveAsyncAction>();
	Action->WorkbookRef = Workbook;
	Action->PathArg = Path;
	Action->RelativeDirArg = RelativeDir;
	return Action;
}

void UExcelSaveAsyncAction::Activate()
{
	if (WorkbookRef == nullptr || WorkbookRef->Data() == nullptr)
	{
		UE_LOG(LogDirectExcel, Warning, TEXT("SaveExcelAsync: workbook is null."));
		OnFailed.Broadcast(0.0f);
		SetReadyToDestroy();
		return;
	}

	// Абсолютный путь считаем на игровом потоке (FPaths не потокобезопасен для ProjectDir и т.п.).
	const FString AbsPath = UDirectExcelLibrary::ToAbsolutePath(PathArg, RelativeDirArg);
	UExcelWorkbook* Wb = WorkbookRef; // жив за счёт UPROPERTY WorkbookRef
	const double startSec = FPlatformTime::Seconds();

	TWeakObjectPtr<UExcelSaveAsyncAction> WeakThis(this);

	Async(EAsyncExecution::ThreadPool, [Wb, AbsPath, startSec, WeakThis]()
	{
		bool bOk = false;
		int32 byteCount = 0;

		// Тяжёлая сериализация xlsx — в фоне.
		std::vector<std::uint8_t> outData;
		if (Wb->Save(outData))
		{
			byteCount = (int32)outData.size();
			TArrayView<uint8> view((uint8*)outData.data(), (int32)outData.size());
			bOk = FFileHelper::SaveArrayToFile(view, *AbsPath);
		}

		const double ms = (FPlatformTime::Seconds() - startSec) * 1000.0;

		// Возврат результата и делегаты — строго на игровом потоке.
		AsyncTask(ENamedThreads::GameThread, [WeakThis, bOk, ms, byteCount, AbsPath]()
		{
			UE_LOG(LogDirectExcel, Log, TEXT("SaveExcelAsync: %.1f ms (%d bytes, ok=%d) -> %s"),
				ms, byteCount, bOk ? 1 : 0, *AbsPath);

			if (WeakThis.IsValid())
			{
				if (bOk)
				{
					WeakThis->OnSaved.Broadcast((float)ms);
				}
				else
				{
					WeakThis->OnFailed.Broadcast((float)ms);
				}
				WeakThis->SetReadyToDestroy();
			}
		});
	});
}
