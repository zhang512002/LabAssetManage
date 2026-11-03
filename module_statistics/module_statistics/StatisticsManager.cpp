// --- StatisticsManager.cpp ---
#include "pch.h"
#include "StatisticsManager.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

    constexpr const char* kConnectionName = "lab_asset_main";

    bool openDatabase(QSqlDatabase& database, QString& errorText)
    {
        database = QSqlDatabase::database(kConnectionName);
        if (!database.isValid() || !database.isOpen()) {
            errorText = QStringLiteral(
                "数据库未打开，请先初始化数据库连接“lab_asset_main”");
            return false;
        }
        return true;
    }

} // namespace

bool StatisticsManager::getEquipmentStatusCounts(
    QMap<LabAsset::EquipmentStatus, int>& counts,
    int& total,
    QString& errorText)
{
    errorText.clear();
    counts.clear();
    total = 0;

    for (LabAsset::EquipmentStatus status :
    LabAsset::allEquipmentStatuses()) {
        counts.insert(status, 0);
    }

    QSqlDatabase database;
    if (!openDatabase(database, errorText)) {
        return false;
    }

    QSqlQuery query(database);
    if (!query.prepare(
        "SELECT status, COUNT(*) "
        "FROM equipment "
        "GROUP BY status")) {
        errorText = QStringLiteral("准备设备状态统计失败：%1")
            .arg(query.lastError().text());
        return false;
    }

    if (!query.exec()) {
        errorText = QStringLiteral("统计设备状态失败：%1")
            .arg(query.lastError().text());
        return false;
    }

    while (query.next()) {
        const QString rawStatus = query.value(0).toString();
        const auto status = LabAsset::statusFromDb(rawStatus);
        if (!status.has_value()) {
            errorText = QStringLiteral("设备状态无效：%1")
                .arg(rawStatus);
            counts.clear();
            total = 0;
            return false;
        }

        const int count = query.value(1).toInt();
        counts[*status] = count;
        total += count;
    }

    return true;
}

bool StatisticsManager::getExportData(
    ExportKind kind,
    TableData& table,
    QString& errorText)
{
    errorText.clear();
    table.headers.clear();
    table.rows.clear();

    QString sql;
    switch (kind) {
    case ExportKind::EquipmentOverview:
        table.headers = {
            QStringLiteral("资产编号"),
            QStringLiteral("设备名称"),
            QStringLiteral("型号"),
            QStringLiteral("状态"),
            QStringLiteral("位置")
        };
        sql =
            "SELECT asset_no, name, model, status, location "
            "FROM equipment "
            "ORDER BY asset_no ASC, id ASC";
        break;

    case ExportKind::BorrowRecords:
        table.headers = {
            QStringLiteral("借用人"),
            QStringLiteral("设备名称"),
            QStringLiteral("借出时间"),
            QStringLiteral("归还时间"),
            QStringLiteral("状态")
        };
        sql =
            "SELECT b.borrower, e.name, b.borrow_time, "
            "       b.actual_return_time, b.status "
            "FROM borrow_record b "
            "LEFT JOIN equipment e ON b.equipment_id = e.id "
            "ORDER BY b.borrow_time DESC, b.id DESC";
        break;

    case ExportKind::MaintenanceRecords:
        table.headers = {
            QStringLiteral("日期"),
            QStringLiteral("设备名称"),
            QStringLiteral("类型"),
            QStringLiteral("任务状态"),
            QStringLiteral("费用"),
            QStringLiteral("描述")
        };
        sql =
            "SELECT m.maintenance_date, e.name, m.type, "
            "       CASE m.status "
            "           WHEN 'pending' THEN '待维修' "
            "           WHEN 'in_progress' THEN '处理中' "
            "           WHEN 'completed' THEN '已完成' "
            "           ELSE m.status "
            "       END, "
            "       m.cost, m.description "
            "FROM maintenance_record m "
            "LEFT JOIN equipment e ON m.equipment_id = e.id "
            "ORDER BY m.maintenance_date DESC, m.id DESC";
        break;
    }

    QSqlDatabase database;
    if (!openDatabase(database, errorText)) {
        return false;
    }

    QSqlQuery query(database);
    if (!query.prepare(sql)) {
        errorText = QStringLiteral("准备统计数据导出失败：%1")
            .arg(query.lastError().text());
        return false;
    }

    if (!query.exec()) {
        errorText = QStringLiteral("查询统计数据失败：%1")
            .arg(query.lastError().text());
        return false;
    }

    while (query.next()) {
        QStringList row;
        row.reserve(table.headers.size());
        for (int column = 0;
            column < table.headers.size();
            ++column) {
            row.append(query.value(column).toString());
        }
        table.rows.append(row);
    }

    return true;
}
