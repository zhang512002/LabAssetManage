#pragma once

#include <QDateTime>
#include <QDialog>
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

    int equipmentId() const;
    QString borrower() const;
    QDateTime borrowTime() const;
    QDateTime expectedReturnTime() const;
    QString remark() const;

protected:
    void accept() override;

private:
    Ui::BorrowDialog* ui;
};
