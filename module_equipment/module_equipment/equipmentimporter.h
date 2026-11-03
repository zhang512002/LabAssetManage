#pragma once

#include <QList>
#include <QString>

#include "equipment.h"

struct EquipmentImportResult
{
    QList<Equipment> equipments;
    QString error;
};

class EquipmentImporter
{
public:
    static EquipmentImportResult read(const QString& filePath);
    static bool writeTemplate(const QString& filePath, QString& error);
};
