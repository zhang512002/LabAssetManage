#pragma once

#include <QString>
#include "enums.h"

struct MaintenanceRecord
{
    int id = -1;
    int equipmentId = -1;
    QString maintenanceDate;
    QString type; // repair: 维修，maintain: 保养
    QString description;
    double cost = 0.0;
    QString operatorName;
    LabAsset::MaintenanceStatus status = LabAsset::MaintenanceStatus::Pending;
};
