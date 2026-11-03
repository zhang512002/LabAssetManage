// csvexporter.h
#pragma once
#include <QString>
#include "common_global.h"
class QAbstractItemModel;

class COMMON_EXPORT CsvExporter {
public:
    // 接收任何数据模型，导出到指定路径
    static bool exportToCsv(QAbstractItemModel* model, const QString& filePath);
};
