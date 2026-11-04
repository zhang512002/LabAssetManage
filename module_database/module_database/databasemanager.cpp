#include "pch.h"
#include "databasemanager.h"
#include "enums.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QDate>

const QString DatabaseManager::kConnectionName = "lab_asset_main";

DatabaseManager& DatabaseManager::instance()
{
    static DatabaseManager inst;
    return inst;
}

DatabaseManager::~DatabaseManager()
{
    close();
}

bool DatabaseManager::open(const QString& dbPath)
{
    if (m_db.isOpen())
        return true;

    m_db = QSqlDatabase::addDatabase("QSQLITE", kConnectionName);
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qCritical() << "打开数据库失败:" << m_db.lastError().text();
        return false;
    }

    // SQLite 每个连接都要手动打开外键约束
    QSqlQuery pragma(m_db);
    pragma.exec("PRAGMA foreign_keys = ON");

    return createTables();
}

void DatabaseManager::close()
{
    if (m_db.isOpen())
        m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(kConnectionName);
}

bool DatabaseManager::createTables()
{
    const QStringList stmts = {
        R"(CREATE TABLE IF NOT EXISTS equipment (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            asset_no      TEXT NOT NULL UNIQUE,
            name          TEXT NOT NULL,
            model         TEXT,
            brand         TEXT,
            purchase_date TEXT,
            price         REAL DEFAULT 0,
            location      TEXT,
            status        TEXT NOT NULL DEFAULT 'available',
            keeper        TEXT,
            remark        TEXT
        ))",
        R"(CREATE TABLE IF NOT EXISTS borrow_record (
            id                   INTEGER PRIMARY KEY AUTOINCREMENT,
            equipment_id         INTEGER NOT NULL,
            borrower             TEXT NOT NULL,
            borrow_time          TEXT NOT NULL,
            expected_return_time TEXT,
            actual_return_time   TEXT,
            status               TEXT NOT NULL DEFAULT 'borrowed',
            remark               TEXT,
            FOREIGN KEY (equipment_id) REFERENCES equipment(id) ON DELETE RESTRICT
        ))",
        R"(CREATE TABLE IF NOT EXISTS maintenance_record (
            id               INTEGER PRIMARY KEY AUTOINCREMENT,
            equipment_id     INTEGER NOT NULL,
            maintenance_date TEXT NOT NULL,
            type             TEXT NOT NULL,
            status           TEXT NOT NULL DEFAULT 'pending',
            description      TEXT,
            cost             REAL DEFAULT 0,
            operator         TEXT,
            FOREIGN KEY (equipment_id) REFERENCES equipment(id) ON DELETE CASCADE
        ))",
        "CREATE INDEX IF NOT EXISTS idx_equipment_asset_no ON equipment(asset_no)",
        "CREATE INDEX IF NOT EXISTS idx_borrow_equipment ON borrow_record(equipment_id)",
        "CREATE INDEX IF NOT EXISTS idx_maintenance_equipment ON maintenance_record(equipment_id)"
    };

    QSqlQuery q(m_db);
    for (const QString& sql : stmts) {
        if (!q.exec(sql)) {
            qCritical() << "建表失败:" << sql << q.lastError().text();
            return false;
        }
    }

    // 旧数据库没有任务状态列,已有记录按历史完成记录迁移。
    bool hasMaintenanceStatus = false;
    QSqlQuery tableInfo(m_db);
    if (!tableInfo.exec("PRAGMA table_info(maintenance_record)")) {
        qCritical() << "读取维保表结构失败:" << tableInfo.lastError().text();
        return false;
    }
    while (tableInfo.next()) {
        if (tableInfo.value(1).toString() == "status") {
            hasMaintenanceStatus = true;
            break;
        }
    }

    if (!hasMaintenanceStatus) {
        QSqlQuery alterTable(m_db);
        if (!alterTable.exec(
                "ALTER TABLE maintenance_record "
                "ADD COLUMN status TEXT NOT NULL DEFAULT 'completed'")) {
            qCritical()
                << "增加维保任务状态列失败:"
                << alterTable.lastError().text();
            return false;
        }
    }

    // 兼容旧数据库中的状态值
    QSqlQuery migration(m_db);
    if (!migration.prepare(
            "UPDATE equipment SET status = :new_status "
            "WHERE status = :old_status")) {
        qCritical() << "准备状态迁移失败:" << migration.lastError().text();
        return false;
    }

    const struct {
        const char* oldStatus;
        LabAsset::EquipmentStatus newStatus;
    } statusMigrations[] = {
        {"under_maintenance", LabAsset::EquipmentStatus::Maintaining},
        {"damaged", LabAsset::EquipmentStatus::PendingRepair}
    };

    if (!m_db.transaction()) {
        qCritical() << "无法开始状态迁移事务:" << m_db.lastError().text();
        return false;
    }

    for (const auto& item : statusMigrations) {
        migration.bindValue(":old_status", item.oldStatus);
        migration.bindValue(
            ":new_status",
            LabAsset::statusToDb(item.newStatus));
        if (!migration.exec()) {
            qCritical()
                << "迁移设备状态失败:"
                << item.oldStatus
                << migration.lastError().text();
            m_db.rollback();
            return false;
        }
    }

    // 为旧数据库中处于维修流程但没有对应任务的设备补齐当前任务
    QSqlQuery activeTask(m_db);
    if (!activeTask.prepare(
            "INSERT INTO maintenance_record "
            "(equipment_id, maintenance_date, type, description, cost, operator, status) "
            "SELECT e.id, :date, :type, :description, 0, :operator, :maintenance_status "
            "FROM equipment e "
            "WHERE e.status = :equipment_status "
            "AND NOT EXISTS ("
            "SELECT 1 FROM maintenance_record m "
            "WHERE m.equipment_id = e.id "
            "AND m.status IN ('pending', 'in_progress')"
            ")")) {
        qCritical() << "准备补齐维保任务失败:" << activeTask.lastError().text();
        m_db.rollback();
        return false;
    }

    const struct {
        LabAsset::EquipmentStatus equipmentStatus;
        LabAsset::MaintenanceStatus maintenanceStatus;
        const char* type;
        const char* description;
    } activeTaskMigrations[] = {
        {LabAsset::EquipmentStatus::PendingRepair,
         LabAsset::MaintenanceStatus::Pending,
         "repair",
         "系统迁移生成待维修任务"},
        {LabAsset::EquipmentStatus::Repairing,
         LabAsset::MaintenanceStatus::InProgress,
         "repair",
         "系统迁移生成维修任务"},
        {LabAsset::EquipmentStatus::Maintaining,
         LabAsset::MaintenanceStatus::InProgress,
         "maintain",
         "系统迁移生成保养任务"}
    };

    const QString migrationDate =
        QDate::currentDate().toString("yyyy-MM-dd");
    for (const auto& item : activeTaskMigrations) {
        activeTask.bindValue(":date", migrationDate);
        activeTask.bindValue(":type", item.type);
        activeTask.bindValue(":description", item.description);
        activeTask.bindValue(":operator", "系统");
        activeTask.bindValue(
            ":maintenance_status",
            LabAsset::maintenanceStatusToDb(item.maintenanceStatus));
        activeTask.bindValue(
            ":equipment_status",
            LabAsset::statusToDb(item.equipmentStatus));
        if (!activeTask.exec()) {
            qCritical()
                << "补齐维保任务失败:"
                << activeTask.lastError().text();
            m_db.rollback();
            return false;
        }
    }

    if (!m_db.commit()) {
        qCritical() << "提交状态迁移失败:" << m_db.lastError().text();
        m_db.rollback();
        return false;
    }

    return true;
}
