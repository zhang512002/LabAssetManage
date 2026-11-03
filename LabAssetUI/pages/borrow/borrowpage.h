#pragma once

#include <QWidget>

namespace Ui {
class BorrowPage;
}

class BorrowPage : public QWidget
{
    Q_OBJECT

public:
    explicit BorrowPage(QWidget* parent = nullptr);
    ~BorrowPage() override;

public slots:
    void refreshData();

signals:
    void dataChanged();
    void damagedEquipmentReturned(int equipmentId);

private slots:
    void borrowEquipment();
    void returnEquipment();
    void exportData();

private:
    Ui::BorrowPage* ui;
};
