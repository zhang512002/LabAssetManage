// --- BorrowManager.cpp ---
#include "pch.h" 
#include "BorrowManager.h"
#include "enums.h"

#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

    constexpr const char* kConnectionName = "lab_asset_main";
    constexpr const char* kDateTimeFormat = "yyyy-MM-dd HH:mm:ss";

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

    bool parseRecordStatus(
        const QString& value,
        BorrowManager::RecordStatus& status)
    {
        if (value == QStringLiteral("borrowed")) {
            status = BorrowManager::RecordStatus::Borrowed;
            return true;
        }
        if (value == QStringLiteral("returned")) {
            status = BorrowManager::RecordStatus::Returned;
            return true;
        }
        return false;
    }

    QString recordStatusText(BorrowManager::RecordStatus status)
    {
        return status == BorrowManager::RecordStatus::Borrowed
            ? QStringLiteral("borrowed")
            : QStringLiteral("returned");
    }

    QString equipmentLabel(const QSqlDatabase& database, int equipmentId)
    {
        QSqlQuery query(database);
        query.prepare(
            "SELECT asset_no, name FROM equipment WHERE id = :id");
        query.bindValue(":id", equipmentId);
        if (query.exec() && query.next()) {
            return query.value(0).toString() + QStringLiteral(" - ")
                + query.value(1).toString();
        }
        return QStringLiteral("设备 ID %1").arg(equipmentId);
    }

} // namespace

QList<BorrowManager::AvailableEquipment>
BorrowManager::getAvailableEquipments(QString& errorText)
{
    errorText.clear();
    QList<AvailableEquipment> equipments;

    QSqlDatabase database;
    if (!openDatabase(database, errorText)) {
        return equipments;
    }

    QSqlQuery query(database);
    if (!query.prepare(
        "SELECT e.id, e.asset_no, e.name "
        "FROM equipment e "
        "WHERE e.status = :available_status "
        "AND NOT EXISTS ("
        "  SELECT 1 FROM borrow_record b "
        "  WHERE b.equipment_id = e.id "
        "  AND (b.status = :borrowed_status "
        "       OR b.actual_return_time IS NULL)"
        ") "
        "ORDER BY e.asset_no ASC")) {
        errorText = QStringLiteral("准备可借设备查询失败：%1")
            .arg(query.lastError().text());
        return equipments;
    }

    query.bindValue(
        ":available_status",
        LabAsset::statusToDb(LabAsset::EquipmentStatus::Available));
    query.bindValue(
        ":borrowed_status",
        LabAsset::statusToDb(LabAsset::EquipmentStatus::Borrowed));

    if (!query.exec()) {
        errorText = QStringLiteral("查询可借设备失败：%1")
            .arg(query.lastError().text());
        return equipments;
    }

    while (query.next()) {
        AvailableEquipment equipment;
        equipment.id = query.value(0).toInt();
        equipment.assetNo = query.value(1).toString();
        equipment.name = query.value(2).toString();
        equipments.append(equipment);
    }
    return equipments;
}

bool BorrowManager::processBorrow(
    int equipmentId,
    const QString& borrower,
    const QDateTime& borrowTime,
    const QDateTime& expectedReturnTime,
    const QString& remark,
    QString& errorText)
{
    return processBorrow(
        QList<int>{equipmentId},
        borrower,
        borrowTime,
        expectedReturnTime,
        remark,
        errorText);
}

bool BorrowManager::processBorrow(
    const QList<int>& equipmentIds,
    const QString& borrower,
    const QDateTime& borrowTime,
    const QDateTime& expectedReturnTime,
    const QString& remark,
    QString& errorText)
{
    errorText.clear();
    const QString normalizedBorrower = borrower.trimmed();

    if (equipmentIds.isEmpty()) {
        errorText = QStringLiteral("请勾选至少一台可借设备");
        return false;
    }
    QSet<int> selectedIds;
    for (int equipmentId : equipmentIds) {
        if (equipmentId <= 0 || selectedIds.contains(equipmentId)) {
            errorText = QStringLiteral("设备编号无效或重复");
            return false;
        }
        selectedIds.insert(equipmentId);
    }
    if (normalizedBorrower.isEmpty()) {
        errorText = QStringLiteral("借用人不能为空");
        return false;
    }
    if (!borrowTime.isValid() || !expectedReturnTime.isValid()) {
        errorText = QStringLiteral("借出时间或预计归还时间无效");
        return false;
    }
    if (expectedReturnTime < borrowTime) {
        errorText = QStringLiteral("预计归还时间不得早于借出时间");
        return false;
    }

    QSqlDatabase database;
    if (!openDatabase(database, errorText)) {
        return false;
    }
    if (!database.transaction()) {
        errorText = QStringLiteral("无法开启借出事务：%1")
            .arg(database.lastError().text());
        return false;
    }

    const QString borrowedStatus =
        LabAsset::statusToDb(LabAsset::EquipmentStatus::Borrowed);
    const QString availableStatus =
        LabAsset::statusToDb(LabAsset::EquipmentStatus::Available);

    QSqlQuery updateEquipment(database);
    if (!updateEquipment.prepare(
        "UPDATE equipment SET status = :borrowed_status "
        "WHERE id = :equipment_id "
        "AND status = :available_status "
        "AND NOT EXISTS ("
        "  SELECT 1 FROM borrow_record b "
        "  WHERE b.equipment_id = equipment.id "
        "  AND (b.status = :borrowed_status "
        "       OR b.actual_return_time IS NULL)"
        ")")) {
        errorText = QStringLiteral("准备更新设备状态失败：%1")
            .arg(updateEquipment.lastError().text());
        database.rollback();
        return false;
    }

    QSqlQuery insertRecord(database);
    if (!insertRecord.prepare(
        "INSERT INTO borrow_record("
        "equipment_id, borrower, borrow_time, expected_return_time, "
        "actual_return_time, status, remark"
        ") VALUES("
        ":equipment_id, :borrower, :borrow_time, "
        ":expected_return_time, NULL, :status, :remark"
        ")")) {
        errorText = QStringLiteral("准备新增借阅记录失败：%1")
            .arg(insertRecord.lastError().text());
        database.rollback();
        return false;
    }

    updateEquipment.bindValue(":borrowed_status", borrowedStatus);
    updateEquipment.bindValue(":available_status", availableStatus);
    insertRecord.bindValue(":borrower", normalizedBorrower);
    insertRecord.bindValue(
        ":borrow_time",
        borrowTime.toString(kDateTimeFormat));
    insertRecord.bindValue(
        ":expected_return_time",
        expectedReturnTime.toString(kDateTimeFormat));
    insertRecord.bindValue(":status", borrowedStatus);
    insertRecord.bindValue(":remark", remark.trimmed());

    for (int equipmentId : equipmentIds) {
        updateEquipment.bindValue(":equipment_id", equipmentId);
        if (!updateEquipment.exec()
            || updateEquipment.numRowsAffected() != 1) {
            const QString reason =
                updateEquipment.lastError().isValid()
                ? updateEquipment.lastError().text()
                : QStringLiteral("已不可借，请刷新后重试");
            updateEquipment.finish();
            errorText = equipmentLabel(database, equipmentId)
                + QStringLiteral("：") + reason;
            database.rollback();
            return false;
        }
        updateEquipment.finish();

        insertRecord.bindValue(":equipment_id", equipmentId);
        if (!insertRecord.exec()) {
            const QString reason = insertRecord.lastError().text();
            insertRecord.finish();
            errorText = equipmentLabel(database, equipmentId)
                + QStringLiteral("：新增借阅记录失败，")
                + reason;
            database.rollback();
            return false;
        }
        insertRecord.finish();
    }

    if (!database.commit()) {
        errorText = QStringLiteral("提交借出事务失败：%1")
            .arg(database.lastError().text());
        database.rollback();
        return false;
    }
    return true;
}

QList<BorrowManager::BorrowRecord> BorrowManager::getBorrowRecords(
    const QString& keyword,
    const std::optional<RecordStatus>& status,
    const QDate& fromDate,
    const QDate& toDate,
    QString& errorText)
{
    errorText.clear();
    QList<BorrowRecord> records;

    if (!fromDate.isValid() || !toDate.isValid()) {
        errorText = QStringLiteral("借阅记录的日期筛选条件无效");
        return records;
    }
    if (fromDate > toDate) {
        errorText = QStringLiteral("开始日期不得晚于结束日期");
        return records;
    }

    QSqlDatabase database;
    if (!openDatabase(database, errorText)) {
        return records;
    }

    QString sql =
        "SELECT b.id, b.equipment_id, e.asset_no, e.name, b.borrower, "
        "       b.borrow_time, b.expected_return_time, "
        "       b.actual_return_time, b.status, b.remark "
        "FROM borrow_record b "
        "JOIN equipment e ON b.equipment_id = e.id "
        "WHERE (b.borrower LIKE :borrower_keyword "
        "       OR e.name LIKE :equipment_keyword) "
        "AND b.borrow_time >= :from_date "
        "AND b.borrow_time < :to_date ";

    if (status.has_value()) {
        sql += "AND b.status = :record_status ";
    }
    sql += "ORDER BY b.borrow_time DESC, b.id DESC";

    QSqlQuery query(database);
    if (!query.prepare(sql)) {
        errorText = QStringLiteral("准备借阅记录查询失败：%1")
            .arg(query.lastError().text());
        return records;
    }

    const QString pattern =
        QStringLiteral("%") + keyword.trimmed() + QStringLiteral("%");
    query.bindValue(":borrower_keyword", pattern);
    query.bindValue(":equipment_keyword", pattern);
    query.bindValue(
        ":from_date",
        fromDate.toString(QStringLiteral("yyyy-MM-dd"))
        + QStringLiteral(" 00:00:00"));
    query.bindValue(
        ":to_date",
        toDate.addDays(1).toString(QStringLiteral("yyyy-MM-dd"))
        + QStringLiteral(" 00:00:00"));
    if (status.has_value()) {
        query.bindValue(":record_status", recordStatusText(*status));
    }

    if (!query.exec()) {
        errorText = QStringLiteral("查询借阅记录失败：%1")
            .arg(query.lastError().text());
        return records;
    }

    while (query.next()) {
        BorrowRecord record;
        record.id = query.value("id").toInt();
        record.equipmentId = query.value("equipment_id").toInt();
        record.assetNo = query.value("asset_no").toString();
        record.equipmentName = query.value("name").toString();
        record.borrower = query.value("borrower").toString();
        record.borrowTime = query.value("borrow_time").toString();
        record.expectedReturnTime =
            query.value("expected_return_time").toString();
        record.actualReturnTime =
            query.value("actual_return_time").toString();
        if (!parseRecordStatus(
            query.value("status").toString(),
            record.status)) {
            errorText = QStringLiteral("借阅记录 %1 的状态无效")
                .arg(record.id);
            return {};
        }
        record.remark = query.value("remark").toString();
        records.append(record);
    }
    return records;
}

bool BorrowManager::getReturnInfo(
    int recordId,
    ReturnInfo& returnInfo,
    QString& errorText)
{
    errorText.clear();
    returnInfo = ReturnInfo{};

    if (recordId <= 0) {
        errorText = QStringLiteral("借阅记录 ID 无效");
        return false;
    }

    QSqlDatabase database;
    if (!openDatabase(database, errorText)) {
        return false;
    }

    QSqlQuery query(database);
    if (!query.prepare(
        "SELECT b.equipment_id, b.borrow_time, "
        "       e.asset_no, e.name, b.borrower "
        "FROM borrow_record b "
        "JOIN equipment e ON b.equipment_id = e.id "
        "WHERE b.id = :record_id "
        "AND b.status = :borrowed_status "
        "AND b.actual_return_time IS NULL")) {
        errorText = QStringLiteral("准备归还信息查询失败：%1")
            .arg(query.lastError().text());
        return false;
    }

    query.bindValue(":record_id", recordId);
    query.bindValue(
        ":borrowed_status",
        recordStatusText(RecordStatus::Borrowed));

    if (!query.exec()) {
        errorText = QStringLiteral("查询归还信息失败：%1")
            .arg(query.lastError().text());
        return false;
    }
    if (!query.next()) {
        errorText = QStringLiteral(
            "记录不存在或已经归还，请刷新列表后重试");
        return false;
    }

    returnInfo.equipmentId = query.value(0).toInt();
    returnInfo.borrowTime =
        QDateTime::fromString(query.value(1).toString(), kDateTimeFormat);
    returnInfo.assetNo = query.value(2).toString();
    returnInfo.equipmentName = query.value(3).toString();
    returnInfo.borrower = query.value(4).toString();

    if (!returnInfo.borrowTime.isValid()) {
        errorText = QStringLiteral("借阅记录的借出时间格式无效");
        returnInfo = ReturnInfo{};
        return false;
    }
    return true;
}

bool BorrowManager::processReturn(
    int recordId,
    const QDateTime& actualReturnTime,
    bool damaged,
    int& returnedEquipmentId,
    QString& errorText)
{
    errorText.clear();
    returnedEquipmentId = -1;

    if (recordId <= 0) {
        errorText = QStringLiteral("借阅记录 ID 无效");
        return false;
    }
    if (!actualReturnTime.isValid()) {
        errorText = QStringLiteral("实际归还时间无效");
        return false;
    }

    QSqlDatabase database;
    if (!openDatabase(database, errorText)) {
        return false;
    }
    if (!database.transaction()) {
        errorText = QStringLiteral("无法开启归还事务：%1")
            .arg(database.lastError().text());
        return false;
    }

    QSqlQuery recordQuery(database);
    if (!recordQuery.prepare(
        "SELECT equipment_id, borrow_time, status, actual_return_time "
        "FROM borrow_record WHERE id = :record_id")) {
        errorText = QStringLiteral("准备读取归还记录失败：%1")
            .arg(recordQuery.lastError().text());
        database.rollback();
        return false;
    }
    recordQuery.bindValue(":record_id", recordId);
    if (!recordQuery.exec()) {
        errorText = QStringLiteral("读取归还记录失败：%1")
            .arg(recordQuery.lastError().text());
        database.rollback();
        return false;
    }
    if (!recordQuery.next()) {
        errorText = QStringLiteral("未找到对应的借阅记录");
        database.rollback();
        return false;
    }

    const int equipmentId = recordQuery.value(0).toInt();
    const QDateTime borrowTime =
        QDateTime::fromString(recordQuery.value(1).toString(), kDateTimeFormat);
    RecordStatus currentStatus = RecordStatus::Borrowed;
    const bool statusValid =
        parseRecordStatus(recordQuery.value(2).toString(), currentStatus);
    const bool alreadyReturned =
        !recordQuery.value(3).isNull()
        && !recordQuery.value(3).toString().isEmpty();

    if (!statusValid
        || currentStatus != RecordStatus::Borrowed
        || alreadyReturned) {
        errorText = QStringLiteral("该记录已经归还，请刷新列表后重试");
        database.rollback();
        return false;
    }
    if (!borrowTime.isValid()) {
        errorText = QStringLiteral("借阅记录的借出时间格式无效");
        database.rollback();
        return false;
    }
    if (actualReturnTime < borrowTime) {
        errorText = QStringLiteral("实际归还时间不得早于借出时间");
        database.rollback();
        return false;
    }

    const QString returnTimeText =
        actualReturnTime.toString(kDateTimeFormat);
    const QString borrowedStatus =
        recordStatusText(RecordStatus::Borrowed);

    QSqlQuery updateRecord(database);
    if (!updateRecord.prepare(
        "UPDATE borrow_record "
        "SET status = :returned_status, "
        "    actual_return_time = :actual_return_time "
        "WHERE id = :record_id "
        "AND equipment_id = :equipment_id "
        "AND status = :borrowed_status "
        "AND actual_return_time IS NULL "
        "AND borrow_time <= :actual_return_time")) {
        errorText = QStringLiteral("准备更新归还记录失败：%1")
            .arg(updateRecord.lastError().text());
        database.rollback();
        return false;
    }
    updateRecord.bindValue(
        ":returned_status",
        recordStatusText(RecordStatus::Returned));
    updateRecord.bindValue(":actual_return_time", returnTimeText);
    updateRecord.bindValue(":record_id", recordId);
    updateRecord.bindValue(":equipment_id", equipmentId);
    updateRecord.bindValue(":borrowed_status", borrowedStatus);

    if (!updateRecord.exec()) {
        errorText = QStringLiteral("更新归还记录失败：%1")
            .arg(updateRecord.lastError().text());
        database.rollback();
        return false;
    }
    if (updateRecord.numRowsAffected() != 1) {
        errorText = QStringLiteral(
            "记录已变化或已经归还，请刷新后重试");
        database.rollback();
        return false;
    }

    const LabAsset::EquipmentStatus resultStatus = damaged
        ? LabAsset::EquipmentStatus::PendingRepair
        : LabAsset::EquipmentStatus::Available;

    QSqlQuery updateEquipment(database);
    if (!updateEquipment.prepare(
        "UPDATE equipment SET status = :result_status "
        "WHERE id = :equipment_id "
        "AND status = :borrowed_status")) {
        errorText = QStringLiteral("准备更新归还后的设备状态失败：%1")
            .arg(updateEquipment.lastError().text());
        database.rollback();
        return false;
    }
    updateEquipment.bindValue(
        ":result_status",
        LabAsset::statusToDb(resultStatus));
    updateEquipment.bindValue(":equipment_id", equipmentId);
    updateEquipment.bindValue(":borrowed_status", borrowedStatus);

    if (!updateEquipment.exec()) {
        errorText = QStringLiteral("更新设备状态失败：%1")
            .arg(updateEquipment.lastError().text());
        database.rollback();
        return false;
    }
    if (updateEquipment.numRowsAffected() != 1) {
        errorText = QStringLiteral(
            "设备状态与借还记录不一致，请核对");
        database.rollback();
        return false;
    }

    if (!database.commit()) {
        errorText = QStringLiteral("提交归还事务失败：%1")
            .arg(database.lastError().text());
        database.rollback();
        return false;
    }

    returnedEquipmentId = equipmentId;
    return true;
}
