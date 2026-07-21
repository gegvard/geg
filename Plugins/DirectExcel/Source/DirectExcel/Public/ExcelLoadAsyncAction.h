// DirectExcel — асинхронная загрузка (парсинг xlsx не блокирует игровой поток).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "ExcelTypes.h"
#include "ExcelLoadAsyncAction.generated.h"

class UExcelWorkbook;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FExcelLoadAsyncResult, UExcelWorkbook*, Workbook, float, MillisecondsTaken);

/**
 * Load Excel Async — читает файл и парсит xlsx в фоновом потоке.
 * Тяжёлый разбор (распаковка zip + XML) не вешает кадр.
 * On Loaded даёт готовый Workbook и время в мс; On Failed — при ошибке.
 */
UCLASS()
class DIRECTEXCEL_API UExcelLoadAsyncAction : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
		FExcelLoadAsyncResult OnLoaded;

	UPROPERTY(BlueprintAssignable)
		FExcelLoadAsyncResult OnFailed;

	UFUNCTION(BlueprintCallable, Category = "DirectExcel|Workbook",
		meta = (BlueprintInternalUseOnly = "true", DisplayName = "Load Excel Async",
			ToolTip = "Read + parse .xlsx on a background thread. Result via On Loaded (Workbook)."))
		static UExcelLoadAsyncAction* LoadExcelAsync(
			FString Path,
			ExcelFileRelateiveDir RelativeDir = ExcelFileRelateiveDir::Absolute);

	virtual void Activate() override;

private:
	FString PathArg;
	ExcelFileRelateiveDir RelativeDirArg = ExcelFileRelateiveDir::Absolute;
};
