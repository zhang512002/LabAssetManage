#pragma once

#include <QString>

class QAbstractItemModel;

namespace UiHelpers {

bool exportModelToCsv(
    QAbstractItemModel* model,
    const QString& filePath,
    QString& errorText);

} // namespace UiHelpers
