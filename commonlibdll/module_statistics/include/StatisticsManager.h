// --- StatisticsManager.h ---
#pragma once

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

#include "enums.h"

#if defined(STATISTICS_LIBRARY) || defined(EXPORT_LIBRARY)
#  define STATISTICS_MANAGER_EXPORT __declspec(dllexport)
#else
#  define STATISTICS_MANAGER_EXPORT __declspec(dllimport)
#endif

class STATISTICS_MANAGER_EXPORT StatisticsManager final
{
public:
    enum class ExportKind {
        EquipmentOverview,
        BorrowRecords,
        MaintenanceRecords
    };

    struct TableData {
        QStringList headers;
        QList<QStringList> rows;
    };

    static bool getEquipmentStatusCounts(
        QMap<LabAsset::EquipmentStatus, int>& counts,
        int& total,
        QString& errorText);

    static bool getExportData(
        ExportKind kind,
        TableData& table,
        QString& errorText);

private:
    StatisticsManager() = delete;
};
