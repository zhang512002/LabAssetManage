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
    if (model->rowCount() == 0) {
        errorText = QStringLiteral("当前没有可导出的数据");
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

    for (int column = 0; column < model->columnCount(); ++column) {
        if (column > 0) {
            out << ',';
        }
        out << model->headerData(
            column,
            Qt::Horizontal).toString();
    }
    out << '\n';

    for (int row = 0; row < model->rowCount(); ++row) {
        for (int column = 0; column < model->columnCount(); ++column) {
            if (column > 0) {
                out << ',';
            }

            QString value = model->index(
                row,
                column).data().toString();
            value.replace("\"", "\"\"");
            out << '"' << value << '"';
        }
        out << '\n';
    }

    if (out.status() != QTextStream::Ok) {
        errorText = QStringLiteral("写入导出文件失败");
        file.close();
        return false;
    }

    if (!file.flush()) {
        errorText = QStringLiteral("保存导出文件失败：%1")
                        .arg(file.errorString());
        file.close();
        return false;
    }

    file.close();
    return true;
}

} // namespace UiHelpers
