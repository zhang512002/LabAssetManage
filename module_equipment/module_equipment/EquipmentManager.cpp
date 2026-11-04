// module_equipment.cpp : 定义 DLL 的导出函数。
//

#include "pch.h"
// --- EquipmentManager.cpp ---
#include "EquipmentManager.h"

#include "equipmentimporter.h"

#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <algorithm>
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

    QString escapeLikePattern(QString text)
    {
        text.replace("\\", "\\\\");
        text.replace("%", "\\%");
        text.replace("_", "\\_");
        return text;
    }

    bool validateEquipment(Equipment& equipment, QString& errorText)
    {
        equipment.assetNo = equipment.assetNo.trimmed();
        equipment.name = equipment.name.trimmed();
        equipment.model = equipment.model.trimmed();
        equipment.brand = equipment.brand.trimmed();
        equipment.purchaseDate = equipment.purchaseDate.trimmed();
        equipment.location = equipment.location.trimmed();
        equipment.keeper = equipment.keeper.trimmed();
        equipment.remark = equipment.remark.trimmed();

        if (equipment.assetNo.isEmpty()) {
            errorText = QStringLiteral("资产编号不能为空");
            return false;
        }
        if (equipment.name.isEmpty()) {
            errorText = QStringLiteral("设备名称不能为空");
            return false;
        }
        if (!std::isfinite(equipment.price) || equipment.price < 0.0) {
            errorText = QStringLiteral("价格必须是合法的非负数字");
            return false;
        }

        const auto& statuses = LabAsset::allEquipmentStatuses();
        if (std::find(
            statuses.cbegin(),
            statuses.cend(),
            equipment.status) == statuses.cend()) {
            errorText = QStringLiteral("设备状态无效");
            return false;
        }
        return true;
    }

    bool equipmentFromQuery(
        const QSqlQuery& query,
        Equipment& equipment,
        QString& errorText)
    {
        equipment.id = query.value(0).toInt();
        equipment.assetNo = query.value(1).toString();
        equipment.name = query.value(2).toString();
        equipment.model = query.value(3).toString();
        equipment.brand = query.value(4).toString();
        equipment.purchaseDate = query.value(5).toString();
        equipment.price = query.value(6).toDouble();
        equipment.location = query.value(7).toString();

        const QString rawStatus = query.value(8).toString();
        const auto status = LabAsset::statusFromDb(rawStatus);
        if (!status.has_value()) {
            errorText = QStringLiteral("设备“%1”的状态无效：%2")
                .arg(equipment.assetNo, rawStatus);
            return false;
        }

        equipment.status = *status;
        equipment.keeper = query.value(9).toString();
        equipment.remark = query.value(10).toString();
        return true;
    }

    bool assetNoExistsInDatabase(
        QSqlDatabase& database,
        const QString& assetNo,
        int excludedId,
        bool& exists,
        QString& errorText)
    {
        exists = false;

        QSqlQuery query(database);
        if (!query.prepare(
            "SELECT 1 FROM equipment "
            "WHERE asset_no = :asset_no AND id <> :excluded_id "
            "LIMIT 1")) {
            errorText = QStringLiteral("准备资产编号校验失败：%1")
                .arg(query.lastError().text());
            return false;
        }

        query.bindValue(":asset_no", assetNo.trimmed());
        query.bindValue(":excluded_id", excludedId);

        if (!query.exec()) {
            errorText = QStringLiteral("校验资产编号失败：%1")
                .arg(query.lastError().text());
            return false;
        }

        exists = query.next();
        return true;
    }

    bool insertEquipment(
        QSqlDatabase& database,
        Equipment& equipment,
        QString& errorText)
    {
        QSqlQuery query(database);
        if (!query.prepare(
            "INSERT INTO equipment "
            "(asset_no, name, model, brand, purchase_date, price, "
            " location, status, keeper, remark) "
            "VALUES "
            "(:asset_no, :name, :model, :brand, :purchase_date, :price, "
            " :location, :status, :keeper, :remark)")) {
            errorText = QStringLiteral("准备新增设备失败：%1")
                .arg(query.lastError().text());
            return false;
        }

        query.bindValue(":asset_no", equipment.assetNo);
        query.bindValue(":name", equipment.name);
        query.bindValue(":model", equipment.model);
        query.bindValue(":brand", equipment.brand);
        query.bindValue(":purchase_date", equipment.purchaseDate);
        query.bindValue(":price", equipment.price);
        query.bindValue(":location", equipment.location);
        query.bindValue(
            ":status",
            LabAsset::statusToDb(LabAsset::EquipmentStatus::Available));
        query.bindValue(":keeper", equipment.keeper);
        query.bindValue(":remark", equipment.remark);

        if (!query.exec()) {
            const QString detail = query.lastError().text();
            if (detail.contains("UNIQUE", Qt::CaseInsensitive)) {
                errorText = QStringLiteral("资产编号已存在，请使用其他编号");
            }
            else {
                errorText = QStringLiteral("新增设备失败：%1").arg(detail);
            }
            return false;
        }

        equipment.id = query.lastInsertId().toInt();
        equipment.status = LabAsset::EquipmentStatus::Available;
        return true;
    }

    bool readEquipmentStatus(
        QSqlDatabase& database,
        int equipmentId,
        LabAsset::EquipmentStatus& status,
        QString& errorText)
    {
        QSqlQuery query(database);
        if (!query.prepare("SELECT status FROM equipment WHERE id = :id")) {
            errorText = QStringLiteral("准备读取设备状态失败：%1")
                .arg(query.lastError().text());
            return false;
        }

        query.bindValue(":id", equipmentId);
        if (!query.exec()) {
            errorText = QStringLiteral("读取设备状态失败：%1")
                .arg(query.lastError().text());
            return false;
        }
        if (!query.next()) {
            errorText = QStringLiteral("未找到对应设备，记录可能已被删除");
            return false;
        }

        const QString rawStatus = query.value(0).toString();
        const auto parsedStatus = LabAsset::statusFromDb(rawStatus);
        if (!parsedStatus.has_value()) {
            errorText = QStringLiteral("设备状态无效：%1").arg(rawStatus);
            return false;
        }

        status = *parsedStatus;
        return true;
    }

} // namespace

QList<Equipment> EquipmentManager::getEquipments(
    const QString& keyword,
    const std::optional<LabAsset::EquipmentStatus>& status,
    QString& errorText)
{
    errorText.clear();
    QList<Equipment> equipments;

    QSqlDatabase database;
    if (!openDatabase(database, errorText)) {
        return equipments;
    }

    QString sql =
        "SELECT id, asset_no, name, model, brand, purchase_date, "
        "       price, location, status, keeper, remark "
        "FROM equipment ";
    QStringList conditions;

    const QString trimmedKeyword = keyword.trimmed();
    if (!trimmedKeyword.isEmpty()) {
        conditions.append(
            "(asset_no LIKE :keyword ESCAPE '\\' "
            "OR name LIKE :keyword ESCAPE '\\')");
    }
    if (status.has_value()) {
        conditions.append("status = :status");
    }

    if (!conditions.isEmpty()) {
        sql += "WHERE " + conditions.join(" AND ") + " ";
    }
    sql += "ORDER BY asset_no ASC, id ASC";

    QSqlQuery query(database);
    if (!query.prepare(sql)) {
        errorText = QStringLiteral("准备设备查询失败：%1")
            .arg(query.lastError().text());
        return equipments;
    }

    if (!trimmedKeyword.isEmpty()) {
        const QString pattern =
            QStringLiteral("%") + escapeLikePattern(trimmedKeyword)
            + QStringLiteral("%");
        query.bindValue(":keyword", pattern);
    }
    if (status.has_value()) {
        query.bindValue(":status", LabAsset::statusToDb(*status));
    }

    if (!query.exec()) {
        errorText = QStringLiteral("查询设备失败：%1")
            .arg(query.lastError().text());
        return equipments;
    }

    while (query.next()) {
        Equipment equipment;
        if (!equipmentFromQuery(query, equipment, errorText)) {
            return {};
        }
        equipments.append(equipment);
    }
    return equipments;
}

bool EquipmentManager::checkEquipmentForSave(
    const Equipment& equipment,
    QString& errorText)
{
    errorText.clear();
    Equipment normalized = equipment;

    if (!validateEquipment(normalized, errorText)) {
        return false;
    }

    QSqlDatabase database;
    if (!openDatabase(database, errorText)) {
        return false;
    }

    bool exists = false;
    if (!assetNoExistsInDatabase(
        database,
        normalized.assetNo,
        normalized.id,
        exists,
        errorText)) {
        return false;
    }

    if (exists) {
        errorText = QStringLiteral("资产编号已存在，请使用其他编号");
        return false;
    }
    return true;
}

bool EquipmentManager::processAdd(
    Equipment& equipment,
    QString& errorText)
{
    errorText.clear();

    if (!validateEquipment(equipment, errorText)) {
        return false;
    }

    QSqlDatabase database;
    if (!openDatabase(database, errorText)) {
        return false;
    }

    bool exists = false;
    if (!assetNoExistsInDatabase(
        database,
        equipment.assetNo,
        -1,
        exists,
        errorText)) {
        return false;
    }
    if (exists) {
        errorText = QStringLiteral("资产编号已存在，请使用其他编号");
        return false;
    }

    return insertEquipment(database, equipment, errorText);
}

bool EquipmentManager::processUpdate(
    Equipment& equipment,
    QString& errorText)
{
    errorText.clear();

    if (equipment.id <= 0) {
        errorText = QStringLiteral("设备 ID 无效，无法更新");
        return false;
    }
    if (!validateEquipment(equipment, errorText)) {
        return false;
    }

    QSqlDatabase database;
    if (!openDatabase(database, errorText)) {
        return false;
    }

    bool exists = false;
    if (!assetNoExistsInDatabase(
        database,
        equipment.assetNo,
        equipment.id,
        exists,
        errorText)) {
        return false;
    }
    if (exists) {
        errorText = QStringLiteral("资产编号已存在，请使用其他编号");
        return false;
    }

    QSqlQuery query(database);
    if (!query.prepare(
        "UPDATE equipment SET "
        "asset_no = :asset_no, name = :name, model = :model, "
        "brand = :brand, purchase_date = :purchase_date, "
        "price = :price, location = :location, "
        "keeper = :keeper, remark = :remark "
        "WHERE id = :id")) {
        errorText = QStringLiteral("准备更新设备失败：%1")
            .arg(query.lastError().text());
        return false;
    }

    query.bindValue(":asset_no", equipment.assetNo);
    query.bindValue(":name", equipment.name);
    query.bindValue(":model", equipment.model);
    query.bindValue(":brand", equipment.brand);
    query.bindValue(":purchase_date", equipment.purchaseDate);
    query.bindValue(":price", equipment.price);
    query.bindValue(":location", equipment.location);
    query.bindValue(":keeper", equipment.keeper);
    query.bindValue(":remark", equipment.remark);
    query.bindValue(":id", equipment.id);

    if (!query.exec()) {
        const QString detail = query.lastError().text();
        if (detail.contains("UNIQUE", Qt::CaseInsensitive)) {
            errorText = QStringLiteral("资产编号已存在，请使用其他编号");
        }
        else {
            errorText = QStringLiteral("更新设备失败：%1").arg(detail);
        }
        return false;
    }

    if (query.numRowsAffected() != 1) {
        errorText = QStringLiteral("未找到对应设备，记录可能已被删除");
        return false;
    }
    return true;
}

bool EquipmentManager::processDelete(
    int equipmentId,
    QString& errorText)
{
    errorText.clear();

    if (equipmentId <= 0) {
        errorText = QStringLiteral("设备 ID 无效，无法删除");
        return false;
    }

    QSqlDatabase database;
    if (!openDatabase(database, errorText)) {
        return false;
    }

    LabAsset::EquipmentStatus status;
    if (!readEquipmentStatus(
        database,
        equipmentId,
        status,
        errorText)) {
        return false;
    }

    if (status != LabAsset::EquipmentStatus::Available
        && status != LabAsset::EquipmentStatus::Scrapped) {
        errorText =
            QStringLiteral("该设备当前为“%1”，存在关联流程，无法删除")
            .arg(LabAsset::statusToDisplay(status));
        return false;
    }

    if (!database.transaction()) {
        errorText = QStringLiteral("无法开启删除设备事务：%1")
            .arg(database.lastError().text());
        return false;
    }

    if (status == LabAsset::EquipmentStatus::Scrapped) {
        QSqlQuery deleteBorrowRecords(database);
        if (!deleteBorrowRecords.prepare(
            "DELETE FROM borrow_record "
            "WHERE equipment_id = :equipment_id")) {
            errorText = QStringLiteral(
                "准备清理设备借还历史失败：%1")
                .arg(deleteBorrowRecords.lastError().text());
            database.rollback();
            return false;
        }
        deleteBorrowRecords.bindValue(":equipment_id", equipmentId);
        if (!deleteBorrowRecords.exec()) {
            errorText = QStringLiteral(
                "清理设备借还历史失败：%1")
                .arg(deleteBorrowRecords.lastError().text());
            database.rollback();
            return false;
        }
    }

    QSqlQuery query(database);
    if (!query.prepare("DELETE FROM equipment WHERE id = :id")) {
        errorText = QStringLiteral("准备删除设备失败：%1")
            .arg(query.lastError().text());
        database.rollback();
        return false;
    }
    query.bindValue(":id", equipmentId);

    if (!query.exec()) {
        const QString detail = query.lastError().text();
        if (detail.contains("FOREIGN KEY", Qt::CaseInsensitive)) {
            errorText = QStringLiteral("该设备存在关联借还记录，无法删除");
        }
        else {
            errorText = QStringLiteral("删除设备失败：%1").arg(detail);
        }
        database.rollback();
        return false;
    }

    if (query.numRowsAffected() != 1) {
        errorText = QStringLiteral("未找到对应设备，记录可能已被删除");
        database.rollback();
        return false;
    }

    if (!database.commit()) {
        errorText = QStringLiteral("提交删除设备事务失败：%1")
            .arg(database.lastError().text());
        database.rollback();
        return false;
    }
    return true;
}

bool EquipmentManager::processImport(
    const QList<Equipment>& equipments,
    int& importedCount,
    QString& errorText)
{
    errorText.clear();
    importedCount = 0;

    if (equipments.isEmpty()) {
        errorText = QStringLiteral("没有可导入的设备记录");
        return false;
    }

    QList<Equipment> normalized = equipments;
    QSet<QString> assetNumbers;

    for (int index = 0; index < normalized.size(); ++index) {
        Equipment& equipment = normalized[index];
        QString itemError;

        if (!validateEquipment(equipment, itemError)) {
            errorText = QStringLiteral("第 %1 条记录：%2")
                .arg(index + 1)
                .arg(itemError);
            return false;
        }

        if (assetNumbers.contains(equipment.assetNo)) {
            errorText = QStringLiteral("第 %1 条记录：资产编号“%2”重复")
                .arg(index + 1)
                .arg(equipment.assetNo);
            return false;
        }
        assetNumbers.insert(equipment.assetNo);
    }

    QSqlDatabase database;
    if (!openDatabase(database, errorText)) {
        return false;
    }

    for (int index = 0; index < normalized.size(); ++index) {
        bool exists = false;
        if (!assetNoExistsInDatabase(
            database,
            normalized[index].assetNo,
            -1,
            exists,
            errorText)) {
            errorText = QStringLiteral("第 %1 条记录：%2")
                .arg(index + 1)
                .arg(errorText);
            return false;
        }
        if (exists) {
            errorText = QStringLiteral(
                "第 %1 条记录：资产编号“%2”已存在")
                .arg(index + 1)
                .arg(normalized[index].assetNo);
            return false;
        }
    }

    if (!database.transaction()) {
        errorText = QStringLiteral("启动批量导入事务失败：%1")
            .arg(database.lastError().text());
        return false;
    }

    for (int index = 0; index < normalized.size(); ++index) {
        QString itemError;
        if (!insertEquipment(database, normalized[index], itemError)) {
            errorText = QStringLiteral("第 %1 条记录导入失败：%2")
                .arg(index + 1)
                .arg(itemError);
            database.rollback();
            return false;
        }
    }

    if (!database.commit()) {
        errorText = QStringLiteral("提交批量导入事务失败：%1")
            .arg(database.lastError().text());
        database.rollback();
        return false;
    }

    importedCount = normalized.size();
    return true;
}

bool EquipmentManager::readImportFile(
    const QString& filePath,
    QList<Equipment>& equipments,
    QString& errorText)
{
    errorText.clear();
    equipments.clear();

    const EquipmentImportResult result = EquipmentImporter::read(filePath);
    if (!result.error.isEmpty()) {
        errorText = result.error;
        return false;
    }

    equipments = result.equipments;
    return true;
}

bool EquipmentManager::writeImportTemplate(
    const QString& filePath,
    QString& errorText)
{
    errorText.clear();
    return EquipmentImporter::writeTemplate(filePath, errorText);
}
