#include "stdafx.h"
#include "borrowdialog.h"
#include "ui_borrowdialog.h"

#include "BorrowManager.h"

#include <QMessageBox>
#include <QPushButton>

BorrowDialog::BorrowDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::BorrowDialog)
{
    ui->setupUi(this);

    const QDateTime now = QDateTime::currentDateTime();
    ui->dt_borrow_time->setDateTime(now);
    ui->dt_expected_return_time->setDateTime(now.addDays(7));

    connect(
        ui->buttonBox,
        &QDialogButtonBox::accepted,
        this,
        &BorrowDialog::accept);
    connect(
        ui->buttonBox,
        &QDialogButtonBox::rejected,
        this,
        &BorrowDialog::reject);

    QString errorText;
    const QList<BorrowManager::AvailableEquipment> equipments =
        BorrowManager::getAvailableEquipments(errorText);
    if (!errorText.isEmpty()) {
        ui->buttonBox->button(QDialogButtonBox::Ok)
            ->setEnabled(false);
        QMessageBox::warning(
            this,
            QStringLiteral("读取设备失败"),
            errorText);
        return;
    }

    for (const BorrowManager::AvailableEquipment& equipment :
         equipments) {
        ui->combo_equipment->addItem(
            equipment.assetNo
                + QStringLiteral(" - ")
                + equipment.name,
            equipment.id);
    }

    if (ui->combo_equipment->count() == 0) {
        ui->combo_equipment->addItem(
            QStringLiteral("暂无可借设备"));
        ui->buttonBox->button(QDialogButtonBox::Ok)
            ->setEnabled(false);
    }
}

BorrowDialog::~BorrowDialog()
{
    delete ui;
}

int BorrowDialog::equipmentId() const
{
    return ui->combo_equipment->currentData().toInt();
}

QString BorrowDialog::borrower() const
{
    return ui->edit_borrower->text();
}

QDateTime BorrowDialog::borrowTime() const
{
    return ui->dt_borrow_time->dateTime();
}

QDateTime BorrowDialog::expectedReturnTime() const
{
    return ui->dt_expected_return_time->dateTime();
}

QString BorrowDialog::remark() const
{
    return ui->edit_remark->toPlainText();
}

void BorrowDialog::accept()
{
    QString errorText;
    if (!BorrowManager::processBorrow(
            equipmentId(),
            borrower(),
            borrowTime(),
            expectedReturnTime(),
            remark(),
            errorText)) {
        QMessageBox::warning(
            this,
            QStringLiteral("借出失败"),
            errorText);
        return;
    }

    QDialog::accept();
}
