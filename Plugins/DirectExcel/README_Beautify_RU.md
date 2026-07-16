# DirectExcel — ноды улучшения вида Excel для экспорта студентов

UE 4.27 · плагин **DirectExcel** (обёртка над библиотекой **xlnt**)

---

## 0.1 Если плагин не компилируется

1. Путь должен быть: `YourProject/Plugins/DirectExcel/DirectExcel.uplugin`
2. Удали у плагина папки `Binaries` и `Intermediate` (если есть)
3. В `.uproject` должен быть включён плагин `DataRegistry` (DirectExcel от него зависит)
4. Правый клик по `.uproject` → **Generate Visual Studio project files** → Rebuild
5. Скачай свежий ZIP ветки (после фикса UHT для `TArray`):  
   https://github.com/gegvard/geg/archive/refs/heads/cursor/directexcel-batch-write-b5bb.zip

Если снова ошибка — пришли **текст красной ошибки** из Output Log / Visual Studio (не только «не пошла»).

---

## 0. Зависание / пропуски на 100+ студентах

**Причина:** в Blueprint цикл `ForEach` + десятки `SetStringAt`/`SetFloatAt` на каждого студента (16 колонок × 100 = 1600+ вызовов) блокирует Game Thread. UE 4.27 рвёт «бесконечный» цикл → часть строк пропускается. `Save` внутри цикла и тяжёлый `BeautifyForExport` (полный AutoFit) усугубляют.

**Как писать данные (рекомендуемый порядок):**

```
Create/Open Workbook → Get Sheet
→ Reserve Capacity (Students * Columns)     // один раз
→ Write String Matrix  ИЛИ  Write String Row в цикле
→ Beautify For Export
→ Save  (ТОЛЬКО один раз, в конце)
→ Close / перестать Watch
```

Новые ноды категории **DirectExcel | Worksheet | Batch**:

| Нода | Зачем |
|---|---|
| **Reserve Capacity** | `Students * Columns` до записи |
| **Write String Row** | одна строка студента = 1 вызов вместо 16 |
| **Write String Matrix** | весь список студентов = 1 вызов (плоский массив) |
| **Write Int/Float Row** | числовые строки |
| **Write Variant Row** | смешанные типы в строке |

Не вызывайте `Cell At` → `Set String` в цикле: каждый `Cell At` создаёт `UObject` и сильно тормозит.

Если AutoFit всё ещё тяжёлый на очень больших листах — в `Beautify For Export` можно выключить Auto Fit и задать ширины вручную (`Set Column Width`).

---

## 1. Что я изучил в плагине

- Лист `UExcelWorksheet` хранит внутри настоящий `xlnt::worksheet mData` (приватное поле). Значит, все доп. возможности оформления нужно добавлять **методами самого класса** — у него есть прямой доступ к `mData`.
- В Blueprint у листа **нет** нод ширины столбцов, закрепления областей, автофильтра и пакетного формата дат. Есть только работа со значениями/ячейками и формат на уровне одной ячейки (`UExcelFormat`).
- При этом `xlnt` всё нужное умеет:
  - ширина столбца — `column_properties{ width, custom_width, best_fit }` + `add_column_properties()`;
  - короткие даты — `cell.number_format(number_format("dd.mm.yyyy"))`, есть `cell.is_date()` для автоопределения;
  - `freeze_panes()`, `auto_filter()`, `font().bold()`.

Вывод: добавляем в `UExcelWorksheet` блок новых `BlueprintCallable`-нод категории **DirectExcel|Beautify**.

## 2. Какие ноды добавлены

| Нода | Назначение |
|---|---|
| **BeautifyForExport** | «Одной кнопкой»: короткие даты → жирный + закреплённый заголовок → автофильтр → автоширина. Главная нода. |
| **AutoFitColumns** | Автоподбор ширины всех столбцов по самой длинной записи (мин/макс/паддинг настраиваются). |
| **SetColumnWidth / SetColumnWidthByString** | Ручная ширина столбца (по индексу или по букве «A», «C»…). |
| **FormatAllDatesShort** | Пройти лист и ужать **все** ячейки-даты до `dd.mm.yyyy`. |
| **FormatColumnAsShortDate** | Короткая дата для конкретного столбца. |
| **SetColumnNumberFormat** | Любой числовой формат для столбца (`0.00`, `# ##0` и т.п.). |
| **SetHeaderRowBold** | Жирная строка-заголовок. |
| **FreezeHeader** | Закрепить заголовок (и при желании первые столбцы). |
| **EnableAutoFilter** | Включить выпадающие фильтры по используемому диапазону. |

Важная деталь логики: **даты ужимаются ПЕРЕД автошириной** — иначе колонка считалась бы по длинному виду (`2007-09-01 00:00:00`), а не по короткому (`01.09.2007`). В `BeautifyForExport` этот порядок уже соблюдён.

## 3. Подключение (2 файла)

1. Открой `Source/DirectExcel/Public/ExcelWorksheet.h`, вставь содержимое **`ExcelWorksheet_Beautify.additions.h`** в любую `public:`-секцию класса `UExcelWorksheet` (например прямо перед финальным `private:` с полями `mWorkbook/mData`).
2. Содержимое **`ExcelWorksheet_Beautify.additions.cpp`** добавь в конец `Source/DirectExcel/Private/ExcelWorksheet.cpp`.
   - Если положишь его отдельным `.cpp` в ту же папку `Private/` — тоже соберётся; повторные `#include` не помешают.
3. Удали папки `Binaries/` и `Intermediate/` плагина, в проекте — `Build / Rebuild` модуля (или правый клик по `.uproject` → *Generate Visual Studio project files* → пересборка). Ноды появятся в палитре под категорией **DirectExcel | Beautify**.

> Единицы ширины — «символьные», как в самом Excel (ширина ≈ число символов шрифтом по умолчанию). Поэтому `MinWidth/MaxWidth` задаются в символах: 5…60 — разумный диапазон.

## 4. Как применить в экспорте студентов (Blueprint)

После того как твой код заполнил лист данными студентов и **до** сохранения книги:

```
... (Set... значения студентов записаны) ...
        │
   [Worksheet]
        │
        ▼
  Beautify For Export
     • Short Dates      = true
     • Date Format      = dd.mm.yyyy
     • Bold Header      = true
     • Freeze Header    = true
     • Auto Filter      = true
     • Auto Fit         = true
     • Header Row       = 1
     • Min Width        = 5
     • Max Width        = 60
        │
        ▼
   Workbook → Save (как и раньше)
```

Тонкая настройка вместо «всё сразу», если нужно:

```
Worksheet ─► Format All Dates Short (dd.mm.yyyy)
          ─► Format Column As Short Date (columnIndex = 4)   // напр. «Дата рождения»
          ─► Set Column Width (columnIndex = 1, width = 6)   // «№»
          ─► Set Header Row Bold (1)
          ─► Freeze Header (freezeBelowRow = 2)
          ─► Enable Auto Filter
          ─► Auto Fit Columns (MaxWidth = 45)
```

## 5. На что обратить внимание

- **Кириллица в ширине** считается правильно: длина берётся через `FString.Len()` (символы), а не байты UTF-8.
- **Многострочные ячейки**: ширина считается по самой длинной строке внутри ячейки.
- **MaxWidth** защищает от «растягивания» из-за длинных адресов/примечаний — длинные столбцы упрутся в потолок, а не разъедутся на весь экран.
- Формат даты — стандартные коды Excel (строчные): `dd.mm.yyyy`, `dd.mm.yy`, `dd MMM yyyy` и т.д. Можно передать свой.
- Если у вас есть «настоящие» даты как текст (строки), `is_date()` их не распознает — тогда пишите их в ячейку через `Set Date Time`, а не `Set String`, либо примените `FormatColumnAsShortDate` к конкретному столбцу принудительно.
