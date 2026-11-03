#include "stdafx.h"
#include "MainWindow.h"

#include "databasemanager.h"

#include <QDir>
#include <QtWidgets/QApplication>
#include <QMessageBox>
#include <QStandardPaths>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QApplication::setOrganizationName(
        QStringLiteral("LabAssetManage"));
    QApplication::setApplicationName(
        QStringLiteral("LabAssetManage"));

    const QString dataDir =
        QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation);
    if (!QDir().mkpath(dataDir)) {
        QMessageBox::critical(
            nullptr,
            QStringLiteral("数据库错误"),
            QStringLiteral("无法创建应用数据目录"));
        return 1;
    }

    const QString dbPath =
        QDir(dataDir).filePath(QStringLiteral("lab_asset.db"));
    if (!DatabaseManager::instance().open(dbPath)) {
        QMessageBox::critical(
            nullptr,
            QStringLiteral("数据库错误"),
            QStringLiteral(
                "数据库初始化失败，请检查 SQLite 驱动是否正确部署。"));
        return 1;
    }

    MainWindow window;
    window.show();
    return app.exec();
}
