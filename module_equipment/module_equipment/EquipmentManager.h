// --- EquipmentManager.h ---
#pragma once

#include <QList>
#include <QString>

#include <optional>

#include "equipment.h"

#if defined(EQUIPMENT_LIBRARY)
#  define EQUIPMENT_MANAGER_EXPORT __declspec(dllexport)
#else
#  define EQUIPMENT_MANAGER_EXPORT __declspec(dllimport)
#endif

class EQUIPMENT_MANAGER_EXPORT EquipmentManager final
{
public:
    static QList<Equipment> getEquipments(
        const QString& keyword,
        const std::optional<LabAsset::EquipmentStatus>& status,
        QString& errorText);

    static bool checkEquipmentForSave(
        const Equipment& equipment,
        QString& errorText);

    static bool processAdd(
        Equipment& equipment,
        QString& errorText);

    static bool processUpdate(
        Equipment& equipment,
        QString& errorText);

    static bool processDelete(
        int equipmentId,
        QString& errorText);

    static bool processImport(
        const QList<Equipment>& equipments,
        int& importedCount,
        QString& errorText);

    static bool readImportFile(
        const QString& filePath,
        QList<Equipment>& equipments,
        QString& errorText);

    static bool writeImportTemplate(
        const QString& filePath,
        QString& errorText);

private:
    EquipmentManager() = delete;
};