#pragma once

#include <QList>
#include <QWidget>

#include "MaintenanceManager.h"

namespace Ui {
class MaintenancePage;
}

class MaintenancePage : public QWidget
{
    Q_OBJECT

public:
    explicit MaintenancePage(QWidget* parent = nullptr);
    ~MaintenancePage() override;

public slots:
    void refreshData();
    bool createPendingRepairForDamagedReturn(int equipmentId);

signals:
    void dataChanged();

private slots:
    void addRecord();
    void editRecord();
    void startRecord();
    void completeRecord();
    void scrapRecord();
    void exportData();

private:
    bool refreshEquipmentFilter();
    void updateActionButtons();

    Ui::MaintenancePage* ui;
    QList<MaintenanceLedgerItem> m_records;
};
