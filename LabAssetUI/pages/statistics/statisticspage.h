#pragma once

#include <QWidget>

#include "StatisticsManager.h"

namespace Ui {
class StatisticsPage;
}

class StatisticsPage : public QWidget
{
    Q_OBJECT

public:
    explicit StatisticsPage(QWidget* parent = nullptr);
    ~StatisticsPage() override;

public slots:
    void refreshData();

private slots:
    void exportEquipment();
    void exportBorrow();
    void exportMaintenance();

private:
    void doExport(
        StatisticsManager::ExportKind kind,
        const QString& defaultFileName);

    Ui::StatisticsPage* ui;
};
