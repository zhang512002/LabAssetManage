#pragma once

#include <QList>
#include <QWidget>

#include "equipment.h"

namespace Ui {
class EquipmentPage;
}

class EquipmentPage : public QWidget
{
    Q_OBJECT

public:
    explicit EquipmentPage(QWidget* parent = nullptr);
    ~EquipmentPage() override;

public slots:
    void refreshData();

signals:
    void dataChanged();

private slots:
    void addEquipment();
    void editEquipment();
    void deleteEquipment();
    void importEquipment();
    void downloadImportTemplate();
    void exportData();

private:
    int selectedEquipmentId() const;
    const Equipment* findEquipment(int id) const;

    Ui::EquipmentPage* ui;
    QList<Equipment> m_equipment;
};
