#pragma once

#include <QString>

#include "enums.h"

struct Equipment
{
    int id = -1;
    QString assetNo;
    QString name;
    QString model;
    QString brand;
    QString purchaseDate;
    double price = 0.0;
    QString location;
    LabAsset::EquipmentStatus status = LabAsset::EquipmentStatus::Available;
    QString keeper;
    QString remark;
};

