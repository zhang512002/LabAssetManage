// --- MaintenanceManager.cpp ---
#include "pch.h"
#include "MaintenanceManager.h"

#include <QDate>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <cmath>

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

    bool taskTargetStatus(
        const QString& type,
        LabAsset::MaintenanceStatus status,
        LabAsset::EquipmentStatus& target)
    {
        if (type == QStringLiteral("repair")
            && status == LabAsset::MaintenanceStatus::Pending) {
            target = LabAsset::EquipmentStatus::PendingRepair;
            return true;
        }
        if (type == QStringLiteral("repair")
            && status == LabAsset::MaintenanceStatus::InProgress) {
            target = LabAsset::EquipmentStatus::Repairing;
            return true;
        }
        if (type == QStringLiteral("maintain")
            && status == LabAsset::MaintenanceStatus::InProgress) {
            target = LabAsset::EquipmentStatus::Maintaining;
            return true;
        }
        return false;
    }

    LabAsset::EquipmentStatus activeEquipmentStatus(
        const QString& type)
    {
        return type == QStringLiteral("maintain")
            ? LabAsset::EquipmentStatus::Maintaining
            : LabAsset::EquipmentStatus::Repairing;
    }

    bool validateRecord(
        const MaintenanceRecord& record,
        QString& errorText)
    {
        if (record.equipmentId <= 0) {
            errorText = QStringLiteral("设备 ID 无效，无法登记维保");
            return false;
        }

        const QDate date = QDate::fromString(
            record.maintenanceDate.trimmed(),
            QStringLiteral("yyyy-MM-dd"));
        if (!date.isValid()) {
            errorText = QStringLiteral("维保日期无效");
            return false;
        }

        if (record.type != QStringLiteral("repair")
            && record.type != QStringLiteral("maintain")) {
            errorText = QStringLiteral(
                "维保类型无效，必须为维修或保养");
            return false;
        }

        if (record.description.trimmed().isEmpty()) {
            errorText = QStringLiteral(
                "请填写故障情况或保养内容");
            return false;
        }

        if (!std::isfinite(record.cost) || record.cost < 0.0) {
            errorText = QStringLiteral(
                "维保费用必须是合法的非负数字");
            return false;
        }

        switch (record.status) {
        case LabAsset::MaintenanceStatus::Pending:
        case LabAsset::MaintenanceStatus::InProgress:
        case LabAsset::MaintenanceStatus::Completed:
            return true;
        }
        errorText = QStringLiteral("维保任务状态无效");
        return false;
    }

    bool parseStatus(
        const QString& value,
        LabAsset::MaintenanceStatus& status)
    {
        const auto parsed =
            LabAsset::maintenanceStatusFromDb(value);
        if (!parsed.has_value()) {
            return false;
        }
        status = *parsed;
        return true;
    }

    void readRecordFromQuery(
        const QSqlQuery& query,
        MaintenanceRecord& record)
    {
        record.id = query.value(QStringLiteral("id")).toInt();
        record.equipmentId =
            query.value(QStringLiteral("equipment_id")).toInt();
        record.maintenanceDate =
            query.value(QStringLiteral("maintenance_date")).toString();
        record.type = query.value(QStringLiteral("type")).toString();
        record.status = *LabAsset::maintenanceStatusFromDb(
            query.value(QStringLiteral("status")).toString());
        record.description =
            query.value(QStringLiteral("description")).toString();
        record.cost =
            query.value(QStringLiteral("cost")).toDouble();
        record.operatorName =
            query.value(QStringLiteral("operator")).toString();
    }

} // namespace

QList<MaintenanceRecord> MaintenanceManager::getRecords(
    int equipmentId,
    QString& errorText)
{
    errorText.clear();
    QList<MaintenanceRecord> records;

    QSqlDatabase database;
    if (!openDatabase(database, errorText)) {
        return records;
    }

    QString sql =
        "SELECT id, equipment_id, maintenance_date, type, "
        "       status, description, cost, operator "
        "FROM maintenance_record ";
    if (equipmentId > 0) {
        sql += "WHERE equipment_id = :equipment_id ";
    }
    sql += "ORDER BY maintenance_date DESC, id DESC";

    QSqlQuery query(database);
    if (!query.prepare(sql)) {
        errorText = QStringLiteral("准备查询维保记录失败：%1")
            .arg(query.lastError().text());
        return records;
    }
    if (equipmentId > 0) {
        query.bindValue(":equipment_id", equipmentId);
    }
    if (!query.exec()) {
        errorText = QStringLiteral("查询维保记录失败：%1")
            .arg(query.lastError().text());
        return records;
    }

    while (query.next()) {
        const QString rawStatus =
            query.value(QStringLiteral("status")).toString();
        const auto status =
            LabAsset::maintenanceStatusFromDb(rawStatus);
        if (!status.has_value()) {
            errorText = QStringLiteral("维保记录状态无效：%1")
                .arg(rawStatus);
            return {};
        }

        MaintenanceRecord record;
        readRecordFromQuery(query, record);
        records.append(record);
    }
    return records;
}

QList<MaintenanceLedgerItem> MaintenanceManager::getLedger(
    int equipmentId,
    QString& errorText)
{
    errorText.clear();
    QList<MaintenanceLedgerItem> records;

    QSqlDatabase database;
    if (!openDatabase(database, errorText)) {
        return records;
    }

    QString sql =
        "SELECT m.id, m.equipment_id, m.maintenance_date, m.type, "
        "       m.status, m.description, m.cost, m.operator, "
        "       e.asset_no, e.name "
        "FROM maintenance_record m "
        "INNER JOIN equipment e ON e.id = m.equipment_id ";
    if (equipmentId > 0) {
        sql += "WHERE m.equipment_id = :equipment_id ";
    }
    sql += "ORDER BY m.maintenance_date DESC, m.id DESC";

    QSqlQuery query(database);
    if (!query.prepare(sql)) {
        errorText = QStringLiteral("准备查询维保台账失败：%1")
            .arg(query.lastError().text());
        return records;
    }
    if (equipmentId > 0) {
        query.bindValue(":equipment_id", equipmentId);
    }
    if (!query.exec()) {
        errorText = QStringLiteral("查询维保台账失败：%1")
            .arg(query.lastError().text());
        return records;
    }

    while (query.next()) {
        const QString rawStatus =
            query.value(QStringLiteral("status")).toString();
        const auto status =
            LabAsset::maintenanceStatusFromDb(rawStatus);
        if (!status.has_value()) {
            errorText = QStringLiteral(
                "维保台账中存在无效任务状态：%1")
                .arg(rawStatus);
            return {};
        }

        MaintenanceLedgerItem item;
        readRecordFromQuery(query, item.record);
        item.assetNo =
            query.value(QStringLiteral("asset_no")).toString();
        item.equipmentName =
            query.value(QStringLiteral("name")).toString();
        records.append(item);
    }
    return records;
}

QList<MaintenanceEquipmentOption>
MaintenanceManager::getEquipmentOptions(
    bool availableOnly,
    QString& errorText)
{
    errorText.clear();
    QList<MaintenanceEquipmentOption> options;

    QSqlDatabase database;
    if (!openDatabase(database, errorText)) {
        return options;
    }

    QString sql =
        "SELECT id, asset_no, name FROM equipment ";
    if (availableOnly) {
        sql += "WHERE status = :available_status ";
    }
    sql += "ORDER BY asset_no ASC";

    QSqlQuery query(database);
    if (!query.prepare(sql)) {
        errorText = QStringLiteral("准备查询设备列表失败：%1")
            .arg(query.lastError().text());
        return options;
    }
    if (availableOnly) {
        query.bindValue(
            ":available_status",
            LabAsset::statusToDb(
                LabAsset::EquipmentStatus::Available));
    }
    if (!query.exec()) {
        errorText = QStringLiteral("查询设备列表失败：%1")
            .arg(query.lastError().text());
        return options;
    }

    while (query.next()) {
        MaintenanceEquipmentOption option;
        option.id = query.value(QStringLiteral("id")).toInt();
        option.assetNo =
            query.value(QStringLiteral("asset_no")).toString();
        option.name =
            query.value(QStringLiteral("name")).toString();
        options.append(option);
    }
    return options;
}

QMap<QString, int>
MaintenanceManager::getEquipmentStatusCounts(
    QString& errorText)
{
    errorText.clear();
    QMap<QString, int> counts;

    QSqlDatabase database;
    if (!openDatabase(database, errorText)) {
        return counts;
    }

    QSqlQuery query(database);
    if (!query.prepare(
        "SELECT status, COUNT(*) AS total "
        "FROM equipment "
        "GROUP BY status")) {
        errorText = QStringLiteral("准备统计设备状态失败：%1")
            .arg(query.lastError().text());
        return counts;
    }
    if (!query.exec()) {
        errorText = QStringLiteral("统计设备状态失败：%1")
            .arg(query.lastError().text());
        return counts;
    }

    while (query.next()) {
        counts.insert(
            query.value(QStringLiteral("status")).toString(),
            query.value(QStringLiteral("total")).toInt());
    }
    return counts;
}

bool MaintenanceManager::processAdd(
    MaintenanceRecord& record,
    QString& errorText)
{
    errorText.clear();

    if (!validateRecord(record, errorText)) {
        return false;
    }

    LabAsset::EquipmentStatus targetStatus;
    if (!taskTargetStatus(
        record.type,
        record.status,
        targetStatus)) {
        errorText = QStringLiteral(
            "维保类型、任务状态或设备目标状态不匹配");
        return false;
    }

    QSqlDatabase database;
    if (!openDatabase(database, errorText)) {
        return false;
    }
    if (!database.transaction()) {
        errorText = QStringLiteral("无法开启维保事务：%1")
            .arg(database.lastError().text());
        return false;
    }

    QSqlQuery updateEquipment(database);
    if (!updateEquipment.prepare(
        "UPDATE equipment "
        "SET status = :target_status "
        "WHERE id = :equipment_id "
        "AND status = :available_status "
        "AND NOT EXISTS ("
        "  SELECT 1 FROM maintenance_record m "
        "  WHERE m.equipment_id = equipment.id "
        "  AND m.status IN ('pending', 'in_progress')"
        ")")) {
        errorText = QStringLiteral("准备更新设备状态失败：%1")
            .arg(updateEquipment.lastError().text());
        database.rollback();
        return false;
    }

    updateEquipment.bindValue(
        ":target_status",
        LabAsset::statusToDb(targetStatus));
    updateEquipment.bindValue(
        ":equipment_id",
        record.equipmentId);
    updateEquipment.bindValue(
        ":available_status",
        LabAsset::statusToDb(
            LabAsset::EquipmentStatus::Available));

    if (!updateEquipment.exec()) {
        errorText = QStringLiteral("更新设备状态失败：%1")
            .arg(updateEquipment.lastError().text());
        database.rollback();
        return false;
    }
    if (updateEquipment.numRowsAffected() != 1) {
        errorText = QStringLiteral(
            "设备当前不可用，或已有未完成的维修保养任务");
        database.rollback();
        return false;
    }

    QSqlQuery insertRecord(database);
    if (!insertRecord.prepare(
        "INSERT INTO maintenance_record "
        "(equipment_id, maintenance_date, type, status, "
        " description, cost, operator) "
        "VALUES "
        "(:equipment_id, :maintenance_date, :type, :status, "
        " :description, :cost, :operator)")) {
        errorText = QStringLiteral("准备新增维保记录失败：%1")
            .arg(insertRecord.lastError().text());
        database.rollback();
        return false;
    }

    insertRecord.bindValue(":equipment_id", record.equipmentId);
    insertRecord.bindValue(
        ":maintenance_date",
        record.maintenanceDate);
    insertRecord.bindValue(":type", record.type);
    insertRecord.bindValue(
        ":status",
        LabAsset::maintenanceStatusToDb(record.status));
    insertRecord.bindValue(":description", record.description);
    insertRecord.bindValue(":cost", record.cost);
    insertRecord.bindValue(":operator", record.operatorName);

    if (!insertRecord.exec()) {
        errorText = QStringLiteral("新增维保记录失败：%1")
            .arg(insertRecord.lastError().text());
        database.rollback();
        return false;
    }

    const int insertedId =
        insertRecord.lastInsertId().toInt();
    if (!database.commit()) {
        errorText = QStringLiteral("提交维保事务失败：%1")
            .arg(database.lastError().text());
        database.rollback();
        return false;
    }

    record.id = insertedId;
    return true;
}

bool MaintenanceManager::processStart(
    int recordId,
    QString& errorText)
{
    errorText.clear();

    if (recordId <= 0) {
        errorText = QStringLiteral(
            "维保记录 ID 无效，无法开始处理");
        return false;
    }

    QSqlDatabase database;
    if (!openDatabase(database, errorText)) {
        return false;
    }
    if (!database.transaction()) {
        errorText = QStringLiteral("无法开启开始处理事务：%1")
            .arg(database.lastError().text());
        return false;
    }

    QSqlQuery recordQuery(database);
    if (!recordQuery.prepare(
        "SELECT equipment_id, type, status "
        "FROM maintenance_record WHERE id = :id")) {
        errorText = QStringLiteral("准备读取维保记录失败：%1")
            .arg(recordQuery.lastError().text());
        database.rollback();
        return false;
    }
    recordQuery.bindValue(":id", recordId);
    if (!recordQuery.exec()) {
        errorText = QStringLiteral("读取维保记录失败：%1")
            .arg(recordQuery.lastError().text());
        database.rollback();
        return false;
    }
    if (!recordQuery.next()) {
        errorText = QStringLiteral("未找到对应的维保记录");
        database.rollback();
        return false;
    }

    const int equipmentId =
        recordQuery.value(QStringLiteral("equipment_id")).toInt();
    const QString type =
        recordQuery.value(QStringLiteral("type")).toString();
    LabAsset::MaintenanceStatus status;
    if (!parseStatus(
        recordQuery.value(QStringLiteral("status")).toString(),
        status)) {
        errorText = QStringLiteral("维保任务状态无效");
        database.rollback();
        return false;
    }
    if (status != LabAsset::MaintenanceStatus::Pending) {
        errorText = QStringLiteral("只有待处理任务可以开始处理");
        database.rollback();
        return false;
    }
    if (type != QStringLiteral("repair")) {
        errorText = QStringLiteral(
            "保养任务创建后直接进入处理中，无法再次开始");
        database.rollback();
        return false;
    }

    QSqlQuery equipmentQuery(database);
    if (!equipmentQuery.prepare(
        "UPDATE equipment SET status = :new_status "
        "WHERE id = :equipment_id "
        "AND status = :expected_status")) {
        errorText = QStringLiteral("准备更新设备状态失败：%1")
            .arg(equipmentQuery.lastError().text());
        database.rollback();
        return false;
    }
    equipmentQuery.bindValue(
        ":new_status",
        LabAsset::statusToDb(
            LabAsset::EquipmentStatus::Repairing));
    equipmentQuery.bindValue(":equipment_id", equipmentId);
    equipmentQuery.bindValue(
        ":expected_status",
        LabAsset::statusToDb(
            LabAsset::EquipmentStatus::PendingRepair));

    if (!equipmentQuery.exec()) {
        errorText = QStringLiteral("更新设备状态失败：%1")
            .arg(equipmentQuery.lastError().text());
        database.rollback();
        return false;
    }
    if (equipmentQuery.numRowsAffected() != 1) {
        errorText = QStringLiteral(
            "设备当前不是待维修状态，无法开始维修");
        database.rollback();
        return false;
    }

    QSqlQuery updateTask(database);
    if (!updateTask.prepare(
        "UPDATE maintenance_record SET status = :new_status "
        "WHERE id = :id AND status = :expected_status")) {
        errorText = QStringLiteral("准备更新任务状态失败：%1")
            .arg(updateTask.lastError().text());
        database.rollback();
        return false;
    }
    updateTask.bindValue(
        ":new_status",
        LabAsset::maintenanceStatusToDb(
            LabAsset::MaintenanceStatus::InProgress));
    updateTask.bindValue(":id", recordId);
    updateTask.bindValue(
        ":expected_status",
        LabAsset::maintenanceStatusToDb(
            LabAsset::MaintenanceStatus::Pending));

    if (!updateTask.exec()) {
        errorText = QStringLiteral("更新任务状态失败：%1")
            .arg(updateTask.lastError().text());
        database.rollback();
        return false;
    }
    if (updateTask.numRowsAffected() != 1) {
        errorText = QStringLiteral(
            "任务状态已变化，请刷新后重试");
        database.rollback();
        return false;
    }

    if (!database.commit()) {
        errorText = QStringLiteral("提交开始处理事务失败：%1")
            .arg(database.lastError().text());
        database.rollback();
        return false;
    }
    return true;
}

bool MaintenanceManager::processComplete(
    int recordId,
    QString& errorText)
{
    errorText.clear();

    if (recordId <= 0) {
        errorText = QStringLiteral(
            "维保记录 ID 无效，无法完成处理");
        return false;
    }

    QSqlDatabase database;
    if (!openDatabase(database, errorText)) {
        return false;
    }
    if (!database.transaction()) {
        errorText = QStringLiteral("无法开启完成处理事务：%1")
            .arg(database.lastError().text());
        return false;
    }

    QSqlQuery recordQuery(database);
    if (!recordQuery.prepare(
        "SELECT equipment_id, type, status "
        "FROM maintenance_record WHERE id = :id")) {
        errorText = QStringLiteral("准备读取维保记录失败：%1")
            .arg(recordQuery.lastError().text());
        database.rollback();
        return false;
    }
    recordQuery.bindValue(":id", recordId);
    if (!recordQuery.exec()) {
        errorText = QStringLiteral("读取维保记录失败：%1")
            .arg(recordQuery.lastError().text());
        database.rollback();
        return false;
    }
    if (!recordQuery.next()) {
        errorText = QStringLiteral("未找到对应的维保记录");
        database.rollback();
        return false;
    }

    const int equipmentId =
        recordQuery.value(QStringLiteral("equipment_id")).toInt();
    const QString type =
        recordQuery.value(QStringLiteral("type")).toString();
    LabAsset::MaintenanceStatus status;
    if (!parseStatus(
        recordQuery.value(QStringLiteral("status")).toString(),
        status)) {
        errorText = QStringLiteral("维保任务状态无效");
        database.rollback();
        return false;
    }
    if (status != LabAsset::MaintenanceStatus::InProgress) {
        errorText = QStringLiteral("只有处理中的任务可以完成");
        database.rollback();
        return false;
    }

    QSqlQuery equipmentQuery(database);
    if (!equipmentQuery.prepare(
        "UPDATE equipment SET status = :new_status "
        "WHERE id = :equipment_id "
        "AND status = :expected_status")) {
        errorText = QStringLiteral("准备更新设备状态失败：%1")
            .arg(equipmentQuery.lastError().text());
        database.rollback();
        return false;
    }
    equipmentQuery.bindValue(
        ":new_status",
        LabAsset::statusToDb(
            LabAsset::EquipmentStatus::Available));
    equipmentQuery.bindValue(":equipment_id", equipmentId);
    equipmentQuery.bindValue(
        ":expected_status",
        LabAsset::statusToDb(activeEquipmentStatus(type)));

    if (!equipmentQuery.exec()) {
        errorText = QStringLiteral("更新设备状态失败：%1")
            .arg(equipmentQuery.lastError().text());
        database.rollback();
        return false;
    }
    if (equipmentQuery.numRowsAffected() != 1) {
        errorText = QStringLiteral(
            "设备当前状态与任务不一致，无法完成处理");
        database.rollback();
        return false;
    }

    QSqlQuery updateTask(database);
    if (!updateTask.prepare(
        "UPDATE maintenance_record SET status = :new_status "
        "WHERE id = :id AND status = :expected_status")) {
        errorText = QStringLiteral("准备更新任务状态失败：%1")
            .arg(updateTask.lastError().text());
        database.rollback();
        return false;
    }
    updateTask.bindValue(
        ":new_status",
        LabAsset::maintenanceStatusToDb(
            LabAsset::MaintenanceStatus::Completed));
    updateTask.bindValue(":id", recordId);
    updateTask.bindValue(
        ":expected_status",
        LabAsset::maintenanceStatusToDb(
            LabAsset::MaintenanceStatus::InProgress));

    if (!updateTask.exec()) {
        errorText = QStringLiteral("更新任务状态失败：%1")
            .arg(updateTask.lastError().text());
        database.rollback();
        return false;
    }
    if (updateTask.numRowsAffected() != 1) {
        errorText = QStringLiteral(
            "任务状态已变化，请刷新后重试");
        database.rollback();
        return false;
    }

    if (!database.commit()) {
        errorText = QStringLiteral("提交完成处理事务失败：%1")
            .arg(database.lastError().text());
        database.rollback();
        return false;
    }
    return true;
}

bool MaintenanceManager::createPendingRepairForDamagedReturn(
    int equipmentId,
    QString& errorText)
{
    errorText.clear();

    if (equipmentId <= 0) {
        errorText = QStringLiteral(
            "设备 ID 无效，无法创建待维修任务");
        return false;
    }

    QSqlDatabase database;
    if (!openDatabase(database, errorText)) {
        return false;
    }
    if (!database.transaction()) {
        errorText = QStringLiteral(
            "无法开启创建待维修任务事务：%1")
            .arg(database.lastError().text());
        return false;
    }

    QSqlQuery equipmentQuery(database);
    if (!equipmentQuery.prepare(
        "SELECT status FROM equipment WHERE id = :id")) {
        errorText = QStringLiteral("准备读取设备状态失败：%1")
            .arg(equipmentQuery.lastError().text());
        database.rollback();
        return false;
    }
    equipmentQuery.bindValue(":id", equipmentId);
    if (!equipmentQuery.exec()) {
        errorText = QStringLiteral("读取设备状态失败：%1")
            .arg(equipmentQuery.lastError().text());
        database.rollback();
        return false;
    }
    if (!equipmentQuery.next()
        || equipmentQuery.value(0).toString()
        != LabAsset::statusToDb(
            LabAsset::EquipmentStatus::PendingRepair)) {
        errorText = QStringLiteral(
            "设备不是待维修状态，无法创建损坏归还任务");
        database.rollback();
        return false;
    }

    QSqlQuery activeQuery(database);
    if (!activeQuery.prepare(
        "SELECT 1 FROM maintenance_record "
        "WHERE equipment_id = :equipment_id "
        "AND status IN ('pending', 'in_progress') LIMIT 1")) {
        errorText = QStringLiteral("准备检查未完成维保任务失败：%1")
            .arg(activeQuery.lastError().text());
        database.rollback();
        return false;
    }
    activeQuery.bindValue(":equipment_id", equipmentId);
    if (!activeQuery.exec()) {
        errorText = QStringLiteral("检查未完成维保任务失败：%1")
            .arg(activeQuery.lastError().text());
        database.rollback();
        return false;
    }
    if (activeQuery.next()) {
        errorText = QStringLiteral(
            "设备已有未完成的维修保养任务");
        database.rollback();
        return false;
    }

    QSqlQuery insertRecord(database);
    if (!insertRecord.prepare(
        "INSERT INTO maintenance_record "
        "(equipment_id, maintenance_date, type, status, "
        " description, cost, operator) "
        "VALUES "
        "(:equipment_id, :maintenance_date, 'repair', :status, "
        " :description, 0, :operator)")) {
        errorText = QStringLiteral("准备创建待维修任务失败：%1")
            .arg(insertRecord.lastError().text());
        database.rollback();
        return false;
    }

    insertRecord.bindValue(":equipment_id", equipmentId);
    insertRecord.bindValue(
        ":maintenance_date",
        QDate::currentDate().toString(
            QStringLiteral("yyyy-MM-dd")));
    insertRecord.bindValue(
        ":status",
        LabAsset::maintenanceStatusToDb(
            LabAsset::MaintenanceStatus::Pending));
    insertRecord.bindValue(
        ":description",
        QStringLiteral("设备归还时标记损坏"));
    insertRecord.bindValue(
        ":operator",
        QStringLiteral("系统"));

    if (!insertRecord.exec()) {
        errorText = QStringLiteral("创建待维修任务失败：%1")
            .arg(insertRecord.lastError().text());
        database.rollback();
        return false;
    }

    if (!database.commit()) {
        errorText = QStringLiteral("提交待维修任务事务失败：%1")
            .arg(database.lastError().text());
        database.rollback();
        return false;
    }
    return true;
}

bool MaintenanceManager::processUpdate(
    const MaintenanceRecord& record,
    QString& errorText)
{
    errorText.clear();

    if (record.id <= 0 || record.equipmentId <= 0) {
        errorText = QStringLiteral(
            "维保记录或设备 ID 无效，无法更新");
        return false;
    }
    if (!validateRecord(record, errorText)) {
        return false;
    }

    QSqlDatabase database;
    if (!openDatabase(database, errorText)) {
        return false;
    }
    if (!database.transaction()) {
        errorText = QStringLiteral("无法开启更新维保事务：%1")
            .arg(database.lastError().text());
        return false;
    }

    QSqlQuery currentQuery(database);
    if (!currentQuery.prepare(
        "SELECT equipment_id, type, status "
        "FROM maintenance_record WHERE id = :id")) {
        errorText = QStringLiteral("准备读取维保记录失败：%1")
            .arg(currentQuery.lastError().text());
        database.rollback();
        return false;
    }
    currentQuery.bindValue(":id", record.id);
    if (!currentQuery.exec()) {
        errorText = QStringLiteral("读取维保记录失败：%1")
            .arg(currentQuery.lastError().text());
        database.rollback();
        return false;
    }
    if (!currentQuery.next()) {
        errorText = QStringLiteral("未找到对应的维保记录");
        database.rollback();
        return false;
    }

    const int currentEquipmentId =
        currentQuery.value(QStringLiteral("equipment_id")).toInt();
    const QString currentType =
        currentQuery.value(QStringLiteral("type")).toString();
    const QString currentRawStatus =
        currentQuery.value(QStringLiteral("status")).toString();

    LabAsset::MaintenanceStatus currentStatus;
    if (!parseStatus(currentRawStatus, currentStatus)) {
        errorText = QStringLiteral("维保任务状态无效");
        database.rollback();
        return false;
    }

    if (currentStatus != LabAsset::MaintenanceStatus::Completed
        && (record.equipmentId != currentEquipmentId
            || record.type != currentType)) {
        errorText = QStringLiteral(
            "未完成任务只能修改日期、说明、费用和经办人");
        database.rollback();
        return false;
    }

    QSqlQuery updateRecord(database);
    if (!updateRecord.prepare(
        "UPDATE maintenance_record "
        "SET equipment_id = :equipment_id, "
        "    maintenance_date = :maintenance_date, "
        "    type = :type, "
        "    description = :description, "
        "    cost = :cost, "
        "    operator = :operator "
        "WHERE id = :id AND status = :current_status")) {
        errorText = QStringLiteral("准备更新维保记录失败：%1")
            .arg(updateRecord.lastError().text());
        database.rollback();
        return false;
    }

    updateRecord.bindValue(":equipment_id", record.equipmentId);
    updateRecord.bindValue(
        ":maintenance_date",
        record.maintenanceDate);
    updateRecord.bindValue(":type", record.type);
    updateRecord.bindValue(":description", record.description);
    updateRecord.bindValue(":cost", record.cost);
    updateRecord.bindValue(":operator", record.operatorName);
    updateRecord.bindValue(":id", record.id);
    updateRecord.bindValue(":current_status", currentRawStatus);

    if (!updateRecord.exec()) {
        errorText = QStringLiteral("更新维保记录失败：%1")
            .arg(updateRecord.lastError().text());
        database.rollback();
        return false;
    }
    if (updateRecord.numRowsAffected() != 1) {
        errorText = QStringLiteral(
            "更新维保记录失败或记录状态已变化");
        database.rollback();
        return false;
    }

    if (!database.commit()) {
        errorText = QStringLiteral("提交更新维保事务失败：%1")
            .arg(database.lastError().text());
        database.rollback();
        return false;
    }
    return true;
}

bool MaintenanceManager::processDelete(
    int recordId,
    QString& errorText)
{
    errorText.clear();

    if (recordId <= 0) {
        errorText = QStringLiteral("维保记录 ID 无效，无法删除");
        return false;
    }

    QSqlDatabase database;
    if (!openDatabase(database, errorText)) {
        return false;
    }

    QSqlQuery query(database);
    if (!query.prepare(
        "DELETE FROM maintenance_record "
        "WHERE id = :id AND status = :completed_status")) {
        errorText = QStringLiteral("准备删除维保记录失败：%1")
            .arg(query.lastError().text());
        return false;
    }

    query.bindValue(":id", recordId);
    query.bindValue(
        ":completed_status",
        LabAsset::maintenanceStatusToDb(
            LabAsset::MaintenanceStatus::Completed));

    if (!query.exec()) {
        errorText = QStringLiteral("删除维保记录失败：%1")
            .arg(query.lastError().text());
        return false;
    }
    if (query.numRowsAffected() != 1) {
        errorText = QStringLiteral(
            "只能删除已完成记录，或记录已不存在");
        return false;
    }
    return true;
}