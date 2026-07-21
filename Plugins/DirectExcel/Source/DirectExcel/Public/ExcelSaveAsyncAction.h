// DirectExcel — асинхронное сохранение (не блокирует игровой поток).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "ExcelTypes.h"
#include "ExcelSaveAsyncAction.generated.h"

class UExcelWorkbook;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FExcelSaveAsyncResult, float, MillisecondsTaken);

/**
 * Save Excel Async — сериализует и пишет файл в фоновом потоке.
 * Игровой поток не встаёт колом на время Save (самая частая причина «фриза»).
 * ВАЖНО: пока идёт сохранение, НЕ меняй этот Workbook (дождись On Saved / On Failed).
 */
UCLASS()
class DIRECTEXCEL_API UExcelSaveAsyncAction : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	/** Успех. Отдаёт время сохранения в миллисекундах. */
	UPROPERTY(BlueprintAssignable)
		FExcelSaveAsyncResult OnSaved;

	/** Ошибка сохранения. */
	UPROPERTY(BlueprintAssignable)
		FExcelSaveAsyncResult OnFailed;

	/** Прогресс сериализации: 0..1 (для ProgressBar). Может не вызываться на очень маленьких файлах. */
	UPROPERTY(BlueprintAssignable)
		FExcelSaveAsyncResult OnProgress;

	UFUNCTION(BlueprintCallable, Category = "DirectExcel|Workbook",
		meta = (BlueprintInternalUseOnly = "true", DisplayName = "Save Excel Async",
			ToolTip = "Serialize + write .xlsx on a background thread. Do not modify the workbook until On Saved/On Failed."))
		static UExcelSaveAsyncAction* SaveExcelAsync(
			UExcelWorkbook* Workbook,
			FString Path,
			ExcelFileRelateiveDir RelativeDir = ExcelFileRelateiveDir::Absolute);

	virtual void Activate() override;

private:
	UPROPERTY()
		UExcelWorkbook* WorkbookRef = nullptr;

	FString PathArg;
	ExcelFileRelateiveDir RelativeDirArg = ExcelFileRelateiveDir::Absolute;
};
