#pragma once

#include <QDateTime>
#include <QDialog>

namespace Ui {
class ReturnDialog;
}

class ReturnDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReturnDialog(
        int recordId,
        QWidget* parent = nullptr);
    ~ReturnDialog() override;

    int returnedEquipmentId() const;
    bool isDamaged() const;

protected:
    void accept() override;

private:
    Ui::ReturnDialog* ui;
    int m_recordId = -1;
    int m_equipmentId = -1;
    QDateTime m_borrowTime;
};
