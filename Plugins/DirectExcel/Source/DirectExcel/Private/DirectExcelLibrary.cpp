// Copyright 2018 Jianzhao Fu. All Rights Reserved.
#include "DirectExcelLibrary.h"
#include "ExcelWorkbook.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "LogTypes.h"
#include "DataTableUtils.h"
#include "ExcelWorksheetDataTable.h"
#include "DataRegistrySubsystem.h"
#include "Engine/Engine.h"

/** Все пути плагина: только '/' (символ 0x2F). Без FPaths::Normalize. */
static FString DirectExcel_ToForwardSlashes(const FString& InPath)
{
	const TCHAR BackSlash = (TCHAR)92;   // '\'
	const TCHAR FwdSlash = (TCHAR)47;    // '/'

	FString Out;
	Out.Reserve(InPath.Len() + 4);
	for (int32 Index = 0; Index < InPath.Len(); ++Index)
	{
		const TCHAR Char = InPath[Index];
		Out.AppendChar(Char == BackSlash ? FwdSlash : Char);
	}
	return Out;
}

FString UDirectExcelLibrary::ToAbsolutePath(FString projectReleativePath, ExcelFileRelateiveDir relativeDir /*= ExcelFileRelateiveDir::Absolute*/)
{
	FString baseDir;
	switch (relativeDir)
	{
	case ExcelFileRelateiveDir::ProjectDir:
		baseDir = FPaths::ProjectDir();
		break;
	case ExcelFileRelateiveDir::ProjectContentDir:
		baseDir = FPaths::ProjectContentDir();
		break;
	case ExcelFileRelateiveDir::ProjectConfigDir:
		baseDir = FPaths::ProjectConfigDir();
		break;
	case ExcelFileRelateiveDir::ProjectPluginsDir:
		baseDir = FPaths::ProjectPluginsDir();
		break;
	case ExcelFileRelateiveDir::ProjectSavedDir:
		baseDir = FPaths::ProjectSavedDir();
		break;
	case ExcelFileRelateiveDir::ProjectModsDir:
		baseDir = FPaths::ProjectModsDir();
		break;
	default:
		break;
	}
	FString fullPath = FPaths::Combine(baseDir, projectReleativePath);
	fullPath = FPaths::ConvertRelativePathToFull(fullPath);
	return DirectExcel_ToForwardSlashes(fullPath);
}

bool UDirectExcelLibrary::DoesExcelFileExists(FString path, ExcelFileRelateiveDir relativeDir /*= ExcelFileRelateiveDir::Absolute*/)
{
	path = ToAbsolutePath(path, relativeDir);
	return FPaths::FileExists(path);
}

FString UDirectExcelLibrary::GetDesktopPath()
{
	// Результат ВСЕГДА вида: C:/Users/<name>/Desktop/
	FString profile = FPlatformMisc::GetEnvironmentVariable(TEXT("USERPROFILE"));
	if (profile.IsEmpty())
	{
		profile = FPlatformProcess::UserDir();
	}

	profile = DirectExcel_ToForwardSlashes(profile);
	while (profile.Len() > 0 && profile[profile.Len() - 1] == (TCHAR)47)
	{
		profile.LeftChopInline(1);
	}

	const FString result = profile + TEXT("/Desktop/");
	UE_LOG(LogDirectExcel, Warning, TEXT("GetDesktopPath => %s"), *result);
	return result;
}

static FString DirectExcel_NormalizeExcelFileName(const FString& DesiredName, const FString& FallbackFromSource)
{
	FString fileName = DesiredName;
	fileName.TrimStartAndEndInline();
	if (fileName.IsEmpty())
	{
		fileName = FPaths::GetCleanFilename(FallbackFromSource);
	}

	// Убрать path-сепараторы из имени
	fileName.ReplaceInline(TEXT("\\"), TEXT("_"));
	fileName.ReplaceInline(TEXT("/"), TEXT("_"));

	if (FPaths::GetExtension(fileName).IsEmpty())
	{
		fileName += TEXT(".xlsx");
	}
	return fileName;
}

static bool DirectExcel_ResolveCopyPaths(FString& OutSource, FString& OutDest, const FString& NewFileName)
{
	OutSource = DirectExcel_ToForwardSlashes(OutSource);
	OutDest = DirectExcel_ToForwardSlashes(OutDest);

	if (OutSource.IsEmpty() || OutDest.IsEmpty())
	{
		return false;
	}

	const FString resolvedName = DirectExcel_NormalizeExcelFileName(NewFileName, OutSource);

	FString trimmedNewName = NewFileName;
	trimmedNewName.TrimStartAndEndInline();

	// Если Dest — существующая папка или путь без расширения, считаем его директорией.
	const bool bDestExistsAsDir = FPaths::DirectoryExists(OutDest);
	const FString DestExt = FPaths::GetExtension(OutDest);
	if (bDestExistsAsDir || DestExt.IsEmpty())
	{
		OutDest = FPaths::Combine(OutDest, resolvedName);
	}
	else if (!trimmedNewName.IsEmpty())
	{
		// Dest был путём к файлу, но задано новое имя — переименовать в той же папке Dest.
		OutDest = FPaths::Combine(FPaths::GetPath(OutDest), resolvedName);
	}

	OutDest = DirectExcel_ToForwardSlashes(FPaths::ConvertRelativePathToFull(OutDest));
	OutSource = DirectExcel_ToForwardSlashes(FPaths::ConvertRelativePathToFull(OutSource));
	return true;
}

bool UDirectExcelLibrary::CopyExcelFile(
	FString SourcePath,
	FString DestPath,
	FString& OutCopiedPath,
	FString NewFileName,
	bool bOverwrite,
	ExcelFileRelateiveDir SourceRelativeDir,
	ExcelFileRelateiveDir DestRelativeDir)
{
	OutCopiedPath.Empty();

	FString sourceAbs = ToAbsolutePath(SourcePath, SourceRelativeDir);
	FString destAbs = ToAbsolutePath(DestPath, DestRelativeDir);

	if (!DirectExcel_ResolveCopyPaths(sourceAbs, destAbs, NewFileName))
	{
		UE_LOG(LogDirectExcel, Warning, TEXT("CopyExcelFile: empty source/dest path."));
		return false;
	}

	if (!FPaths::FileExists(sourceAbs))
	{
		UE_LOG(LogDirectExcel, Warning, TEXT("CopyExcelFile: source not found: %s"), *sourceAbs);
		return false;
	}

	if (!bOverwrite && FPaths::FileExists(destAbs))
	{
		UE_LOG(LogDirectExcel, Warning, TEXT("CopyExcelFile: destination already exists: %s"), *destAbs);
		return false;
	}

	const FString destDir = FPaths::GetPath(destAbs);
	if (!destDir.IsEmpty() && !FPaths::DirectoryExists(destDir))
	{
		if (!IFileManager::Get().MakeDirectory(*destDir, true))
		{
			UE_LOG(LogDirectExcel, Warning, TEXT("CopyExcelFile: failed to create directory: %s"), *destDir);
			return false;
		}
	}

	const uint32 CopyResult = IFileManager::Get().Copy(*destAbs, *sourceAbs, bOverwrite);
	if (CopyResult != COPY_OK)
	{
		UE_LOG(LogDirectExcel, Warning, TEXT("CopyExcelFile: copy failed (%u) from %s to %s"), CopyResult, *sourceAbs, *destAbs);
		return false;
	}

	OutCopiedPath = DirectExcel_ToForwardSlashes(destAbs);
	UE_LOG(LogDirectExcel, Warning, TEXT("CopyExcelFile OK: %s -> %s"), *sourceAbs, *OutCopiedPath);
	return true;
}

bool UDirectExcelLibrary::MoveExcelFile(
	FString SourcePath,
	FString DestPath,
	FString& OutMovedPath,
	FString NewFileName,
	bool bOverwrite,
	ExcelFileRelateiveDir SourceRelativeDir,
	ExcelFileRelateiveDir DestRelativeDir)
{
	OutMovedPath.Empty();

	FString sourceAbs = ToAbsolutePath(SourcePath, SourceRelativeDir);
	FString destAbs = ToAbsolutePath(DestPath, DestRelativeDir);

	if (!DirectExcel_ResolveCopyPaths(sourceAbs, destAbs, NewFileName))
	{
		UE_LOG(LogDirectExcel, Warning, TEXT("MoveExcelFile: empty source/dest path."));
		return false;
	}

	FString copiedPath;
	// destAbs уже финальный путь к файлу — NewFileName пустой, чтобы не трогать имя повторно.
	if (!CopyExcelFile(sourceAbs, destAbs, copiedPath, FString(), bOverwrite, ExcelFileRelateiveDir::Absolute, ExcelFileRelateiveDir::Absolute))
	{
		return false;
	}

	if (!IFileManager::Get().Delete(*sourceAbs, false, true))
	{
		UE_LOG(LogDirectExcel, Warning, TEXT("MoveExcelFile: copied, but failed to delete source: %s"), *sourceAbs);
		return false;
	}

	OutMovedPath = DirectExcel_ToForwardSlashes(copiedPath);
	UE_LOG(LogDirectExcel, Warning, TEXT("MoveExcelFile OK: %s -> %s"), *sourceAbs, *OutMovedPath);
	return true;
}

bool UDirectExcelLibrary::CopyExcelFileAs(
	FString SourcePath,
	FString DestFolder,
	FString NewFileName,
	FString& OutCopiedPath,
	bool bOverwrite,
	ExcelFileRelateiveDir SourceRelativeDir,
	ExcelFileRelateiveDir DestRelativeDir)
{
	FString trimmed = NewFileName;
	trimmed.TrimStartAndEndInline();
	if (trimmed.IsEmpty())
	{
		UE_LOG(LogDirectExcel, Warning, TEXT("CopyExcelFileAs: NewFileName is empty."));
		OutCopiedPath.Empty();
		return false;
	}

	return CopyExcelFile(SourcePath, DestFolder, OutCopiedPath, NewFileName, bOverwrite, SourceRelativeDir, DestRelativeDir);
}


TArray<FDataRegistryId> UDirectExcelLibrary::GetAllRegistryIds(FDataRegistryType registryType)
{
	TArray<FDataRegistryId> ids;

	UDataRegistrySubsystem* Subsystem = GEngine->GetEngineSubsystem<UDataRegistrySubsystem>();
	if (!Subsystem)
	{
		return ids;
	}

	UDataRegistry* Registry = Subsystem->GetRegistryForType(registryType);
	if (!Registry)
	{
		UE_LOG(LogDirectExcel, Error, TEXT("Cannot find DataRegistry:%s!"), *registryType.ToString());
		return ids;
	}
	Registry->GetPossibleRegistryIds(ids);
	return ids;
}

UExcelWorkbook* UDirectExcelLibrary::LoadExcel(FString path, ExcelFileRelateiveDir relativeDir /*= ExcelFileRelateiveDir::Absolute*/)
{
	UExcelWorkbook* wb = NewObject<UExcelWorkbook>();
	if (wb->Load(path, relativeDir))
	{
		return wb;
	}
	return nullptr;
}

bool UDirectExcelLibrary::SaveExcel(UExcelWorkbook* workbook, FString path, ExcelFileRelateiveDir relativeDir /*= ExcelFileRelateiveDir::ProjectSavedDir*/)
{
	if (workbook == nullptr)
	{
		UE_LOG(LogDirectExcel, Fatal, TEXT("SaveExcel: Parameter:workbook is null."));
		return false;
	}

	return workbook->SaveAs(path, relativeDir);
}

UExcelWorkbook* UDirectExcelLibrary::CreateExcel()
{
	return NewObject<UExcelWorkbook>();
}

//static bool ReadStructAtRowIndex(UExcelWorksheet* sheet, int32 row, FTableRowBase& OutItem) { return false; }

DEFINE_FUNCTION(UDirectExcelLibrary::execReadStructAtRowIndex)
{
	P_GET_OBJECT(UExcelWorksheet, sheet);
	P_GET_PROPERTY(FIntProperty, row);

	bool result;
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);

	void* OutItemDataPtr = Stack.MostRecentPropertyAddress;
	FStructProperty* OutItemProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	UScriptStruct* OutputStruct = OutItemProp->Struct;
	P_FINISH;
	P_NATIVE_BEGIN;

	result = ReadStructAtRowIndexRaw(sheet, row, OutputStruct, OutItemDataPtr);

	P_NATIVE_END;
	*(bool*)RESULT_PARAM = result;
}

bool UDirectExcelLibrary::ReadStructAtRowIndexRaw(const UExcelWorksheet* sheet, int32 row, const UScriptStruct* OutStruct, void* outStructData)
{
	if (sheet == nullptr)
	{
		UE_LOG(LogDirectExcel, Fatal, TEXT("ReadStructAtRowIndexRaw: Parameter:sheet is null."));
		return false;
	}

	OutStruct->InitializeStruct(outStructData);

	for (TFieldIterator<const FProperty> i(OutStruct, EFieldIteratorFlags::IncludeSuper); i; ++i)
	{
		const FProperty* property = *i;
		FName columnName = GetPropertyColumnName(*property);
		int32 column = sheet->FindColumnIndex(columnName);
		if (column <= 0)
		{
			UE_LOG(LogDirectExcel,Error, TEXT("Cannot find column: '%s' on row '%d'"), *columnName.ToString(), row);
			return false;
		}

		FString val = sheet->ToString(FExcelCellReference(column, row));

		FString ErrorStr = DataTableUtils::AssignStringToProperty(val, property, (uint8*)outStructData);

		// If we failed, output a problem string
		if (ErrorStr.Len() > 0)
		{
			UE_LOG(LogDirectExcel,Error, TEXT("Problem assigning string '%s' to property '%s' on row '%d' : %s"), *val, *columnName.ToString(), row, *ErrorStr);
			return false;
		}

		++column;
	}

	return true;
}

DEFINE_FUNCTION(UDirectExcelLibrary::execReadStructWithRowName)
{
	P_GET_OBJECT(UExcelWorksheet, sheet);
	P_GET_PROPERTY(FNameProperty, rowName);

	bool result;
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);

	void* OutItemDataPtr = Stack.MostRecentPropertyAddress;
	FStructProperty* OutItemProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	UScriptStruct* OutputStruct = OutItemProp->Struct;
	P_FINISH;
	P_NATIVE_BEGIN;

	result = ReadStructWithRowNameRaw(sheet, rowName, OutputStruct, OutItemDataPtr);

	P_NATIVE_END;
	*(bool*)RESULT_PARAM = result;
}

bool UDirectExcelLibrary::ReadStructWithRowNameRaw(const UExcelWorksheet* sheet, FName rowName, const UScriptStruct* OutStruct, void* outStructData)
{
	if (sheet == nullptr)
	{
		UE_LOG(LogDirectExcel, Fatal, TEXT("ReadStructWithRowNameRaw: Parameter:sheet is null."));
		return false;
	}

	if (sheet->GetRowStructInUsed()!=OutStruct)
	{
		UE_LOG(LogDirectExcel, Error, TEXT("Cannot read row by '%s', need to set table header at first"), *rowName.ToString());
		return false;
	}

	const UExcelWorksheetDataTable* dataTable = sheet->GetOrCreateDataTable();
	if (dataTable)
	{
		uint8* findData = dataTable->FindRowUnchecked(rowName);
		if (findData)
		{
			OutStruct->InitializeStruct(outStructData);
			OutStruct->CopyScriptStruct(outStructData, findData);
			return true;
		}
	}

	return false;
}

FName UDirectExcelLibrary::GetPropertyColumnName(const FProperty& property)
{
	//Name_2_C861169D466517CB284898BC1B15C476

	FString str= property.GetName();
	int32 outIndex=-1;
	if (str.FindChar('_', outIndex))
	{
		str=str.Left(outIndex);
	}

	return FName(*str);
}

DEFINE_FUNCTION(UDirectExcelLibrary::execReadItemAtCell)
{
	P_GET_OBJECT(UExcelWorksheet, sheet);
	P_GET_STRUCT(FExcelCellReference, cellReference);

	bool result = true;
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);

	void* OutItemDataPtr = Stack.MostRecentPropertyAddress;
	FString str = sheet->ToString(cellReference);
	FStringOutputDevice ImportError;

	Stack.MostRecentProperty->ImportText(*str, OutItemDataPtr, PPF_ExternalEditor, nullptr, &ImportError);
	if (ImportError.Len() > 0)
	{
		UE_LOG(LogDirectExcel, Error, TEXT("Problem read struct:%s at row:%d column:%d : %s"), *Stack.MostRecentProperty->GetName(), cellReference.Row, cellReference.Column, *ImportError);
		result = false;
	}

	P_FINISH;
	P_NATIVE_BEGIN;


	P_NATIVE_END;
	*(bool*)RESULT_PARAM = result;
}

bool UDirectExcelLibrary::TryPropertyToExcelVariant(const FProperty* Property, const void* StructData, FExcelVariant& OutVariant)
{
	if (Property == nullptr || StructData == nullptr)
	{
		return false;
	}

	// Пропускаем контейнеры — их пишут отдельными циклами в нужные колонки (30 / 50).
	if (Property->IsA<FArrayProperty>() || Property->IsA<FMapProperty>() || Property->IsA<FSetProperty>())
	{
		return false;
	}

	if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		OutVariant = FExcelVariant(BoolProp->GetPropertyValue_InContainer(StructData));
		return true;
	}
	if (const FIntProperty* IntProp = CastField<FIntProperty>(Property))
	{
		OutVariant = FExcelVariant(IntProp->GetPropertyValue_InContainer(StructData));
		return true;
	}
	if (const FInt64Property* Int64Prop = CastField<FInt64Property>(Property))
	{
		OutVariant = FExcelVariant((int32)Int64Prop->GetPropertyValue_InContainer(StructData));
		return true;
	}
	if (const FByteProperty* ByteProp = CastField<FByteProperty>(Property))
	{
		// Enum-as-byte или обычный byte
		OutVariant = FExcelVariant((int32)ByteProp->GetPropertyValue_InContainer(StructData));
		return true;
	}
	if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		const FNumericProperty* Underlying = EnumProp->GetUnderlyingProperty();
		const void* ValuePtr = EnumProp->ContainerPtrToValuePtr<void>(StructData);
		OutVariant = FExcelVariant((int32)Underlying->GetSignedIntPropertyValue(ValuePtr));
		return true;
	}
	if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
	{
		OutVariant = FExcelVariant(FloatProp->GetPropertyValue_InContainer(StructData));
		return true;
	}
	if (const FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
	{
		OutVariant = FExcelVariant((float)DoubleProp->GetPropertyValue_InContainer(StructData));
		return true;
	}
	if (const FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		OutVariant = FExcelVariant(StrProp->GetPropertyValue_InContainer(StructData));
		return true;
	}
	if (const FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		OutVariant = FExcelVariant(NameProp->GetPropertyValue_InContainer(StructData).ToString());
		return true;
	}
	if (const FTextProperty* TextProp = CastField<FTextProperty>(Property))
	{
		OutVariant = FExcelVariant(TextProp->GetPropertyValue_InContainer(StructData).ToString());
		return true;
	}
	if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		if (StructProp->Struct == TBaseStructure<FDateTime>::Get())
		{
			const FDateTime* DatePtr = StructProp->ContainerPtrToValuePtr<FDateTime>(StructData);
			OutVariant = FExcelVariant(DatePtr ? *DatePtr : FDateTime());
			return true;
		}
	}

	return false;
}

int32 UDirectExcelLibrary::AppendStructFieldsToVariantMapRaw(
	int32 StartIndex,
	int32 EndIndex,
	int32 StartColumn,
	int32 EndColumn,
	const UScriptStruct* StructType,
	const void* StructData,
	TMap<int32, FExcelVariant>& ColumnValues)
{
	if (StructType == nullptr || StructData == nullptr)
	{
		UE_LOG(LogDirectExcel, Warning, TEXT("AppendStructFieldsToVariantMap: struct is null."));
		return 0;
	}
	if (StartIndex < 0 || EndIndex < StartIndex)
	{
		UE_LOG(LogDirectExcel, Warning, TEXT("AppendStructFieldsToVariantMap: invalid field index range %d..%d (0-based, End>=Start)."), StartIndex, EndIndex);
		return 0;
	}
	if (StartColumn < 1 || EndColumn < StartColumn)
	{
		UE_LOG(LogDirectExcel, Warning, TEXT("AppendStructFieldsToVariantMap: invalid column range %d..%d (1-based, End>=Start)."), StartColumn, EndColumn);
		return 0;
	}

	const int32 indexSpan = EndIndex - StartIndex;
	const int32 columnSpan = EndColumn - StartColumn;
	if (indexSpan != columnSpan)
	{
		UE_LOG(LogDirectExcel, Warning,
			TEXT("AppendStructFieldsToVariantMap: index span (%d..%d) != column span (%d..%d); will stop at the shorter range."),
			StartIndex, EndIndex, StartColumn, EndColumn);
	}

	int32 fieldIndex = 0;
	int32 column = StartColumn;
	int32 written = 0;

	for (TFieldIterator<FProperty> It(StructType, EFieldIteratorFlags::IncludeSuper); It; ++It, ++fieldIndex)
	{
		if (fieldIndex < StartIndex)
		{
			continue;
		}
		if (fieldIndex > EndIndex || column > EndColumn)
		{
			break;
		}

		const FProperty* Property = *It;
		if (Property == nullptr)
		{
			continue;
		}

		// Массивы/Map в диапазоне индексов: пропускаем поле, колонку НЕ сдвигаем.
		if (Property->IsA<FArrayProperty>() || Property->IsA<FMapProperty>() || Property->IsA<FSetProperty>())
		{
			continue;
		}

		FExcelVariant variant;
		if (!TryPropertyToExcelVariant(Property, StructData, variant))
		{
			UE_LOG(LogDirectExcel, Verbose, TEXT("AppendStructFieldsToVariantMap: skip unsupported field [%d] '%s'"), fieldIndex, *Property->GetName());
			continue;
		}

		ColumnValues.Add(column, variant);
		++written;
		++column;
	}

	return written;
}

DEFINE_FUNCTION(UDirectExcelLibrary::execAppendStructFieldsToVariantMap)
{
	P_GET_PROPERTY(FIntProperty, StartIndex);
	P_GET_PROPERTY(FIntProperty, EndIndex);
	P_GET_PROPERTY(FIntProperty, StartColumn);
	P_GET_PROPERTY(FIntProperty, EndColumn);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	void* StructDataPtr = Stack.MostRecentPropertyAddress;
	FStructProperty* StructProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	UScriptStruct* StructType = StructProp ? StructProp->Struct : nullptr;

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FMapProperty>(nullptr);
	void* MapDataPtr = Stack.MostRecentPropertyAddress;

	P_FINISH;

	int32 written = 0;
	P_NATIVE_BEGIN;
	if (MapDataPtr != nullptr)
	{
		TMap<int32, FExcelVariant>* ColumnValues = (TMap<int32, FExcelVariant>*)MapDataPtr;
		written = AppendStructFieldsToVariantMapRaw(StartIndex, EndIndex, StartColumn, EndColumn, StructType, StructDataPtr, *ColumnValues);
	}
	else
	{
		UE_LOG(LogDirectExcel, Warning, TEXT("AppendStructFieldsToVariantMap: ColumnValues map is null."));
	}
	P_NATIVE_END;

	*(int32*)RESULT_PARAM = written;
}

bool UDirectExcelLibrary::AppendStructFieldToVariantMapRaw(
	int32 FieldIndex,
	int32 Column,
	const UScriptStruct* StructType,
	const void* StructData,
	TMap<int32, FExcelVariant>& ColumnValues)
{
	if (StructType == nullptr || StructData == nullptr)
	{
		UE_LOG(LogDirectExcel, Warning, TEXT("AppendStructFieldToVariantMap: struct is null."));
		return false;
	}
	if (FieldIndex < 0)
	{
		UE_LOG(LogDirectExcel, Warning, TEXT("AppendStructFieldToVariantMap: FieldIndex %d is invalid (need >= 0)."), FieldIndex);
		return false;
	}
	if (Column < 1)
	{
		UE_LOG(LogDirectExcel, Warning, TEXT("AppendStructFieldToVariantMap: Column %d is invalid (need >= 1)."), Column);
		return false;
	}

	int32 fieldIndex = 0;
	for (TFieldIterator<FProperty> It(StructType, EFieldIteratorFlags::IncludeSuper); It; ++It, ++fieldIndex)
	{
		if (fieldIndex != FieldIndex)
		{
			continue;
		}

		const FProperty* Property = *It;
		if (Property == nullptr)
		{
			return false;
		}

		if (Property->IsA<FArrayProperty>() || Property->IsA<FMapProperty>() || Property->IsA<FSetProperty>())
		{
			UE_LOG(LogDirectExcel, Warning, TEXT("AppendStructFieldToVariantMap: field [%d] '%s' is an array/map/set — skip."), FieldIndex, *Property->GetName());
			return false;
		}

		FExcelVariant variant;
		if (!TryPropertyToExcelVariant(Property, StructData, variant))
		{
			UE_LOG(LogDirectExcel, Warning, TEXT("AppendStructFieldToVariantMap: unsupported field [%d] '%s'."), FieldIndex, *Property->GetName());
			return false;
		}

		ColumnValues.Add(Column, variant);
		return true;
	}

	UE_LOG(LogDirectExcel, Warning, TEXT("AppendStructFieldToVariantMap: FieldIndex %d out of range for struct '%s'."), FieldIndex, *StructType->GetName());
	return false;
}

DEFINE_FUNCTION(UDirectExcelLibrary::execAppendStructFieldToVariantMap)
{
	P_GET_PROPERTY(FIntProperty, FieldIndex);
	P_GET_PROPERTY(FIntProperty, Column);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	void* StructDataPtr = Stack.MostRecentPropertyAddress;
	FStructProperty* StructProp = CastField<FStructProperty>(Stack.MostRecentProperty);
	UScriptStruct* StructType = StructProp ? StructProp->Struct : nullptr;

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FMapProperty>(nullptr);
	void* MapDataPtr = Stack.MostRecentPropertyAddress;

	P_FINISH;

	bool bOk = false;
	P_NATIVE_BEGIN;
	if (MapDataPtr != nullptr)
	{
		TMap<int32, FExcelVariant>* ColumnValues = (TMap<int32, FExcelVariant>*)MapDataPtr;
		bOk = AppendStructFieldToVariantMapRaw(FieldIndex, Column, StructType, StructDataPtr, *ColumnValues);
	}
	else
	{
		UE_LOG(LogDirectExcel, Warning, TEXT("AppendStructFieldToVariantMap: ColumnValues map is null."));
	}
	P_NATIVE_END;

	*(bool*)RESULT_PARAM = bOk;
}

