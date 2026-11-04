#include "stdafx.h"
#include "UiHelpers.h"

#include <QAbstractItemModel>
#include <QFile>
#include <QTextStream>

namespace UiHelpers {

bool exportModelToCsv(
    QAbstractItemModel* model,
    const QString& filePath,
    QString& errorText)
{
    errorText.clear();

    if (!model) {
        errorText = QStringLiteral("没有可导出的表格数据");
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        errorText = QStringLiteral("无法创建导出文件：%1")
                        .arg(file.errorString());
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::System);

    // 写表头
    for (int column = 0; column < model->columnCount(); ++column) {
        out << model->headerData(
            column,
            Qt::Horizontal).toString()
            << ',';
    }
    out << "\n";

    // 写数据
    for (int row = 0; row < model->rowCount(); ++row) {
        for (int column = 0; column < model->columnCount(); ++column) {
            QString value = model->index(
                row,
                column).data().toString();
            value.replace("\"", "\"\"");
            out << '"' << value << "\",";
        }
        out << "\n";
    }

    file.close();
    return true;
}

} // namespace UiHelpers
