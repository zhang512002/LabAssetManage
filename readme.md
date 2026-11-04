# LabAssetManage 实验室设备资产管理系统

LabAssetManage 是一个基于 C++17、Qt 6 Widgets 和 SQLite 的多模块桌面项目。

项目采用“表现层 + 业务层 + 数据访问层”的 DLL 分层架构：

- 最终应用只负责界面和用户交互。
- 每类业务分别封装在独立 DLL 中。
- 所有业务 DLL 共享同一个数据库连接，但业务模块之间不直接依赖。
- 公共头文件、导入库和运行 DLL 统一归档到 `commonlibdll`。

## 技术栈

| 项目 | 版本或配置 |
| --- | --- |
| IDE | Visual Studio 2019 |
| 平台工具集 | v142 |
| 语言标准 | C++17 |
| Qt | 6.5.3 MSVC2019 64-bit |
| Qt 模块 | Core、Gui、Widgets、Sql |
| 数据库 | SQLite |
| 目标平台 | x64 |

默认 Qt 路径：

```text
C:\Qt\6.5.3\msvc2019_64
```

## 系统功能

- 设备管理：查询、筛选、新增、编辑、删除、CSV/XLSX 批量导入和模板下载。
- 借阅管理：可借设备查询、借出登记、借还记录筛选、归还登记和 CSV 导出。
- 维保管理：维修和保养记录、任务状态流转、设备状态联动和 CSV 导出。
- 报废管理：维修失败后可将维修中的设备报废，报废设备可从设备页面永久删除。
- 统计导出：设备状态统计，以及设备、借还、维保数据导出。
- 跨模块联动：设备损坏归还后自动创建“待维修”任务。

## 总体架构

```text
┌──────────────────────────────────────────────┐
│ LabAssetUI                                   │
│ Qt Widgets Application                       │
│                                              │
│ MainWindow / pages / dialogs / table views   │
└───────────────┬──────────────────────────────┘
                │ 只调用 Manager 静态接口
                ▼
┌──────────────────────────────────────────────┐
│ 业务 DLL 层                                  │
│                                              │
│ module_borrow                                │
│ module_equipment                             │
│ module_maintenance                           │
│ module_statistics                            │
└───────────────┬──────────────────────────────┘
                │ 只依赖数据库 DLL
                ▼
┌──────────────────────────────────────────────┐
│ module_database                              │
│                                              │
│ DatabaseManager                              │
│ SQLite connection / table creation / migrate │
└───────────────┬──────────────────────────────┘
                ▼
           SQLite 数据库
```

工程链接和构建依赖关系（不是目录树）：

下表中的箭头 `使用方 -> 被依赖方` 表示使用方会链接对方的 `.lib`，
运行时需要对方的 `.dll`。这里的名称是 Visual Studio 工程名，不是目录名。

| 使用方工程 | 被依赖或被链接的工程 |
| --- | --- |
| `module_borrow` | `module_database` |
| `module_equipment` | `module_database` |
| `module_maintenance` | `module_database` |
| `module_statistics` | `module_database` |
| `LabAssetUI` | `module_database`、`module_borrow`、`module_equipment`、`module_maintenance`、`module_statistics` |

因此：

- 4 个业务 DLL 只依赖数据库 DLL，不互相信赖。
- `LabAssetUI` 是最终消费方，链接全部 5 个 DLL。
- 总解决方案会先构建 `module_database`，再构建 4 个业务 DLL，最后构建 `LabAssetUI`。
- 业务源码并不放在 `LabAssetUI/` 里面，它位于各自的 `module_*/module_*/` 工程目录。

### 分层职责

#### 表现层：LabAssetUI

负责：

- 显示页面和弹窗。
- 从控件收集用户输入。
- 调用业务 Manager。
- 显示错误信息。
- 页面切换和跨页面信号协调。
- 调用 SQLite 初始化数据库连接。

表现层不直接执行 `SELECT`、`INSERT`、`UPDATE` 或事务，也不持有业务表访问逻辑。

#### 业务层：module_borrow / module_equipment / module_maintenance / module_statistics

负责：

- 业务校验。
- SQL 查询。
- 插入、更新和删除。
- 数据库事务。
- 业务状态转换。
- 将错误信息通过 `QString& errorText` 返回给 UI。

业务层不包含 `QWidget`、`QDialog`、`QMessageBox` 等界面类型，也不会弹出窗口。

#### 数据访问层：module_database

负责：

- 注册命名数据库连接。
- 创建和迁移表结构。
- 向业务模块提供同一个 `QSqlDatabase`。
- 提供通用枚举、共享模型和数据库基础能力。

数据库连接名统一为：

```text
lab_asset_main
```

所有业务 DLL 都通过该连接名获取同一个 SQLite 连接。

## 仓库目录

```text
LabAssetManage/
├── .gitignore
├── LabAssetManage.sln
├── readme.md
│
├── commonlibdll/                         # 公共开发接口与模块产物
│   ├── module_database/
│   │   ├── include/
│   │   │   ├── common_global.h
│   │   │   ├── csvexporter.h
│   │   │   ├── databasemanager.h
│   │   │   ├── enums.h
│   │   │   └── maintenancerecord.h
│   │   ├── lib/                          # 构建后生成 .lib
│   │   └── bin/                          # 构建后生成 .dll
│   │
│   ├── module_borrow/
│   │   ├── include/BorrowManager.h
│   │   ├── lib/
│   │   └── bin/
│   │
│   ├── module_equipment/
│   │   ├── include/
│   │   │   ├── EquipmentManager.h
│   │   │   └── equipment.h
│   │   ├── lib/
│   │   └── bin/
│   │
│   ├── module_maintenance/
│   │   ├── include/MaintenanceManager.h
│   │   ├── lib/
│   │   └── bin/
│   │
│   └── module_statistics/
│       ├── include/StatisticsManager.h
│       ├── lib/
│       └── bin/
│
├── module_database/
│   └── module_database/
│       ├── module_database.sln
│       ├── module_database.vcxproj
│       ├── databasemanager.h / .cpp
│       ├── enums.h
│       ├── maintenancerecord.h
│       └── csvexporter.h / .cpp
│
├── module_borrow/
│   └── module_borrow/
│       ├── module_borrow.sln
│       ├── module_borrow.vcxproj
│       └── BorrowManager.h / .cpp
│
├── module_equipment/
│   └── module_equipment/
│       ├── module_equipment.sln
│       ├── module_equipment.vcxproj
│       ├── EquipmentManager.h / .cpp
│       ├── equipment.h
│       ├── equipmentimporter.h / .cpp
│       └── qzip.cpp / qzipreader_p.h
│
├── module_maintenance/
│   └── module_maintenance/
│       ├── module_maintenance.sln
│       ├── module_maintenance.vcxproj
│       └── MaintenanceManager.h / .cpp
│
├── module_statistics/
│   └── module_statistics/
│       ├── module_statistics.sln
│       ├── module_statistics.vcxproj
│       └── StatisticsManager.h / .cpp
│
└── LabAssetUI/
    ├── CommonSettings.props
    ├── LabAssetUI.vcxproj
    ├── main.cpp
    ├── MainWindow.h / .cpp / .ui
    ├── MainWindow.qrc
    │
    ├── pages/
    │   ├── borrow/
    │   │   ├── borrowpage.h / .cpp / .ui
    │   │   ├── borrowdialog.h / .cpp / .ui
    │   │   └── returndialog.h / .cpp / .ui
    │   ├── equipment/
    │   │   ├── equipmentpage.h / .cpp / .ui
    │   │   └── equipmentdialog.h / .cpp
    │   ├── maintenance/
    │   │   ├── maintenancepage.h / .cpp / .ui
    │   │   └── maintenancedialog.h / .cpp
    │   └── statistics/
    │       └── statisticspage.h / .cpp / .ui
    │
    ├── shared/
    │   └── UiHelpers.h / .cpp
    │
    └── x64/
        └── Debug|Release/
```

`x64`、`Debug`、`Release`、`qt`、`.pch`、`.obj`、`.tlog`、`.pdb` 等构建缓存不会提交到 Git。

## 业务接口

### DatabaseManager

```cpp
class DatabaseManager {
public:
    static DatabaseManager& instance();
    bool open(const QString& dbPath);
    void close();
    QSqlDatabase db() const;
};
```

数据库文件使用 `QStandardPaths::AppDataLocation` 定位，Windows 下通常位于：

```text
%APPDATA%\LabAssetManage\LabAssetManage\lab_asset.db
```

数据库初始化时会创建：

- `equipment`
- `borrow_record`
- `maintenance_record`

同时包含旧数据库字段和状态值迁移逻辑。

### BorrowManager

主要接口：

```cpp
getAvailableEquipments()
processBorrow()
getBorrowRecords()
getReturnInfo()
processReturn()
```

借出事务同时完成：

1. 检查设备仍为“在用”。
2. 将设备更新为“借出”。
3. 插入借阅记录。

归还事务同时完成：

1. 更新借阅记录和实际归还时间。
2. 正常归还时恢复“在用”。
3. 标记损坏时改为“待维修”。

### EquipmentManager

主要接口：

```cpp
getEquipments()
checkEquipmentForSave()
processAdd()
processUpdate()
processDelete()
processImport()
readImportFile()
writeImportTemplate()
```

设备导入支持 `.xlsx` 和 `.csv`。批量导入使用数据库事务，任意一条失败时整体回滚。

设备只有在“在用”状态且没有关联借还记录时才能删除。

### MaintenanceManager

主要接口：

```cpp
getRecords()
getLedger()
getEquipmentOptions()
getEquipmentStatusCounts()
processAdd()
processStart()
processComplete()
processUpdate()
processDelete()
createPendingRepairForDamagedReturn()
```

维修流程：

```text
待维修 -> 处理中 -> 已完成
```

保养登记会直接进入处理中：

```text
保养中 -> 已完成
```

任务完成时设备恢复为“在用”。

维修处理中还可以选择“维修失败并报废”：

```text
设备状态：维修中 -> 已报废
任务状态：处理中 -> 已完成
```

报废操作不增加新的数据库表或字段，而是复用现有状态和说明字段。报废后，
设备管理页面允许永久删除该设备；删除会同时清理关联借还记录，并依靠现有
外键级联删除维保记录。

### StatisticsManager

主要接口：

```cpp
getEquipmentStatusCounts()
getExportData()
```

支持三类数据导出：

- 设备总览
- 借还记录
- 维保记录

统计 Manager 返回二维表格数据，由 UI 负责选择文件并写入 CSV，避免 UI 直接执行统计 SQL。

## UI 架构

主窗体使用：

```text
QListWidget + QStackedWidget
```

导航索引与页面顺序：

| 索引 | 页面 |
| --- | --- |
| 0 | 设备管理 |
| 1 | 借阅管理 |
| 2 | 维保管理 |
| 3 | 统计导出 |

页面通过信号与主窗体协调：

```text
EquipmentPage::dataChanged
BorrowPage::dataChanged
MaintenancePage::dataChanged
        |
        v
MainWindow::refreshAllPages()
```

损坏归还联动：

```text
BorrowPage
  damagedEquipmentReturned(equipmentId)
        |
        v
MaintenancePage
  createPendingRepairForDamagedReturn(equipmentId)
```

## DLL 导出机制

业务层使用条件导出宏，例如：

```cpp
#if defined(BORROW_LIBRARY)
#define BORROW_MANAGER_EXPORT __declspec(dllexport)
#else
#define BORROW_MANAGER_EXPORT __declspec(dllimport)
#endif
```

含义：

- 编译业务 DLL 时定义对应的 `*_LIBRARY` 宏，生成导出符号。
- UI 编译时不定义该宏，因此按 `dllimport` 消费 DLL。
- UI 通过 `.lib` 导入库链接，运行时再加载对应 `.dll`。

各业务工程必须保持导出宏与公开头文件中的宏一致。

## 构建说明

### 前置条件

- Visual Studio 2019，安装“使用 C++ 的桌面开发”。
- Qt VS Tools。
- Qt 6.5.3 MSVC2019 64-bit。
- Windows 10 SDK。

### 构建步骤

1. 打开根目录的 `LabAssetManage.sln`。
2. 选择 `Debug | x64` 或 `Release | x64`。
3. 生成解决方案。

解决方案已经配置以下依赖：

```text
module_database
    |
    +-- module_borrow
    +-- module_equipment
    +-- module_maintenance
    +-- module_statistics
             |
             v
        LabAssetUI
```

UI 构建完成后，会自动把 5 个模块 DLL 复制到 UI 输出目录。

### Debug/Release 产物命名

Debug 产物追加 `d` 后缀，Release 保持无后缀：

| Debug | Release |
| --- | --- |
| `module_databased.dll/.lib` | `module_database.dll/.lib` |
| `module_borrowd.dll/.lib` | `module_borrow.dll/.lib` |
| `module_equipmentd.dll/.lib` | `module_equipment.dll/.lib` |
| `module_maintenanced.dll/.lib` | `module_maintenance.dll/.lib` |
| `module_statisticsd.dll/.lib` | `module_statistics.dll/.lib` |

因此 Debug 和 Release 产物可以同时保存在公共库中，不会互相覆盖。
UI 会按照当前配置链接并复制对应名称的 DLL。

## 输出位置

Debug UI：

```text
LabAssetUI\x64\Debug\LabAssetUI.exe
```

Release UI：

```text
LabAssetUI\x64\Release\LabAssetUI.exe
```

公共模块产物：

```text
commonlibdll\<module>\bin\*.dll
commonlibdll\<module>\lib\*.lib
commonlibdll\<module>\include\*.h
```

## 直接运行与 Qt 部署

Visual Studio 能直接运行，是因为 Qt VS Tools 会自动把 Qt 的 `bin` 和插件目录加入调试环境。双击 EXE 时不会获得这套环境。

Debug 部署：

```powershell
& "C:\Qt\6.5.3\msvc2019_64\bin\windeployqt.exe" `
  --debug `
  --no-translations `
  --no-compiler-runtime `
  ".\LabAssetUI\x64\Debug\LabAssetUI.exe"
```

Release 部署：

```powershell
& "C:\Qt\6.5.3\msvc2019_64\bin\windeployqt.exe" `
  --release `
  --no-translations `
  --no-compiler-runtime `
  ".\LabAssetUI\x64\Release\LabAssetUI.exe"
```

必须确保输出目录包含：

```text
Qt6Core*.dll
Qt6Gui*.dll
Qt6Widgets*.dll
Qt6Sql*.dll
platforms\qwindows*.dll
sqldrivers\qsqlite*.dll
module_*.dll
```

## 当前已知事项

- 5 个 DLL 工程存在 `OutDir` 与 `Link.OutputFile` 不一致的 MSB8012 警告。当前构建能够成功，但后续可进一步统一 `bin` 和 `lib` 的输出规则。
- 部分模块的 `OutDir` 没有以反斜杠结尾，会产生 MSB8004 警告，不影响当前构建。
- Git 默认不提交 Qt 运行库和 Visual Studio 构建缓存。
- 项目当前使用固定命名连接 `lab_asset_main`；所有模块必须继续使用该连接名。

## 扩展新模块

新增业务模块时建议遵循：

1. 新建独立 Qt DLL 工程。
2. 只依赖 `module_database`。
3. 暴露静态 Manager 接口，不暴露 UI 类型。
4. 错误通过 `QString& errorText` 返回。
5. 更新业务 DLL、LIB 和公共头文件到 `commonlibdll`。
6. 在根解决方案中添加 UI -> 新模块的依赖。
7. 在 UI 的 `CommonSettings.props` 中添加 include、lib 和依赖项。
8. 在 `LabAssetUI.vcxproj` 的构建后复制目标中添加新 DLL。

## 仓库

```text
https://github.com/zhang512002/LabAssetManage
```
