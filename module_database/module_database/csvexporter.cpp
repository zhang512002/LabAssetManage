// csvexporter.cpp
#include "pch.h"
#include "csvexporter.h"
#include <QFile>
#include <QTextStream>
#include <QAbstractItemModel>

bool CsvExporter::exportToCsv(QAbstractItemModel* model, const QString& filePath) {
    if (!model) return false;
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QTextStream out(&file);
    // 写 BOM 头，防止 Excel 打开中文乱码
    out.setEncoding(QStringConverter::System);

    // 1. 写表头
    for (int col = 0; col < model->columnCount(); ++col) {
        out << model->headerData(col, Qt::Horizontal).toString() << ",";
    }
    out << "\n";

    // 2. 写数据
    for (int row = 0; row < model->rowCount(); ++row) {
        for (int col = 0; col < model->columnCount(); ++col) {
            QString text = model->index(row, col).data().toString();
            text.replace("\"", "\"\""); // 处理文本中本身的引号
            out << "\"" << text << "\",";
        }
        out << "\n";
    }
    file.close();
    return true;
}