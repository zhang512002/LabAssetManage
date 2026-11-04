#pragma once

#include <QDateTime>
#include <QDialog>
#include <QList>
#include <QString>

namespace Ui {
class BorrowDialog;
}

class BorrowDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BorrowDialog(QWidget* parent = nullptr);
    ~BorrowDialog() override;

    QList<int> equipmentIds() const;
    QString borrower() const;
    QDateTime borrowTime() const;
    QDateTime expectedReturnTime() const;
    QString remark() const;

protected:
    void accept() override;

private:
    void filterEquipment(const QString& text);

    Ui::BorrowDialog* ui;
};
