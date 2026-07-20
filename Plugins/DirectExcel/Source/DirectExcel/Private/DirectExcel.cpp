// Copyright 2018 Jianzhao Fu. All Rights Reserved.

#include "DirectExcel.h"
#include "Core.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "ExcelRowNameResolver.h"
#include "LogTypes.h"

#define LOCTEXT_NAMESPACE "FDirectExcelModule"

void FDirectExcelModule::StartupModule()
{
	mRowNameResolver = MakeShareable(new FExcelRowNameResolver());
	FDataRegistryResolverScope::RegisterGlobalResolver(mRowNameResolver);

	UE_LOG(LogDirectExcel, Warning, TEXT("DirectExcel loaded: 3.4.1-BATCH-20260720 (WriteVariantMap + Desktop fix + RowStruct init). If you do not see this line — old DLL."));
}

void FDirectExcelModule::ShutdownModule()
{
	if (mRowNameResolver.IsValid())
	{
		FDataRegistryResolverScope::UnregisterGlobalResolver(mRowNameResolver);
		mRowNameResolver = nullptr;
	}
}

TSharedPtr<FExcelRowNameResolver> FDirectExcelModule::mRowNameResolver;

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDirectExcelModule, DirectExcel)