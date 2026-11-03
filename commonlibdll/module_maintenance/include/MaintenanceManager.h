// --- MaintenanceManager.h ---
#pragma once

#include <QList>
#include <QMap>
#include <QString>

#include "enums.h"
#include "maintenancerecord.h"

#if defined(MAINTENANCE_LIBRARY)
#  define MAINTENANCE_MANAGER_EXPORT __declspec(dllexport)
#else
#  define MAINTENANCE_MANAGER_EXPORT __declspec(dllimport)
#endif

struct MaintenanceEquipmentOption
{
    int id = -1;
    QString assetNo;
    QString name;
};

struct MaintenanceLedgerItem
{
    MaintenanceRecord record;
    QString assetNo;
    QString equipmentName;
};

class MAINTENANCE_MANAGER_EXPORT MaintenanceManager final
{
public:
    static QList<MaintenanceRecord> getRecords(
        int equipmentId,
        QString& errorText);

    static QList<MaintenanceLedgerItem> getLedger(
        int equipmentId,
        QString& errorText);

    static QList<MaintenanceEquipmentOption> getEquipmentOptions(
        bool availableOnly,
        QString& errorText);

    static QMap<QString, int> getEquipmentStatusCounts(
        QString& errorText);

    static bool processAdd(
        MaintenanceRecord& record,
        QString& errorText);

    static bool processStart(
        int recordId,
        QString& errorText);

    static bool processComplete(
        int recordId,
        QString& errorText);

    static bool createPendingRepairForDamagedReturn(
        int equipmentId,
        QString& errorText);

    static bool processUpdate(
        const MaintenanceRecord& record,
        QString& errorText);

    static bool processDelete(
        int recordId,
        QString& errorText);

private:
    MaintenanceManager() = delete;
};
