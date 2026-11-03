#include "stdafx.h"
#include "returndialog.h"
#include "ui_returndialog.h"

#include "BorrowManager.h"

#include <QMessageBox>
#include <QPushButton>

ReturnDialog::ReturnDialog(
    int recordId,
    QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::ReturnDialog)
    , m_recordId(recordId)
{
    ui->setupUi(this);
    ui->dt_actual_return_time->setDateTime(
        QDateTime::currentDateTime());

    connect(
        ui->buttonBox,
        &QDialogButtonBox::accepted,
        this,
        &ReturnDialog::accept);
    connect(
        ui->buttonBox,
        &QDialogButtonBox::rejected,
        this,
        &ReturnDialog::reject);

    BorrowManager::ReturnInfo info;
    QString errorText;
    if (!BorrowManager::getReturnInfo(
            m_recordId,
            info,
            errorText)) {
        ui->label_info->setText(errorText);
        ui->buttonBox->button(QDialogButtonBox::Ok)
            ->setEnabled(false);
        QMessageBox::warning(
            this,
            QStringLiteral("读取失败"),
            errorText);
        return;
    }

    m_equipmentId = info.equipmentId;
    m_borrowTime = info.borrowTime;
    ui->label_info->setText(
        QStringLiteral(
            "资产编号：%1\n设备：%2\n借用人：%3\n"
            "借出时间：%4")
            .arg(info.assetNo)
            .arg(info.equipmentName)
            .arg(info.borrower)
            .arg(info.borrowTime.toString(
                QStringLiteral("yyyy-MM-dd HH:mm:ss"))));

    ui->dt_actual_return_time->setMinimumDateTime(
        m_borrowTime);
}

ReturnDialog::~ReturnDialog()
{
    delete ui;
}

int ReturnDialog::returnedEquipmentId() const
{
    return m_equipmentId;
}

bool ReturnDialog::isDamaged() const
{
    return ui->check_damaged->isChecked();
}

void ReturnDialog::accept()
{
    int equipmentId = -1;
    QString errorText;
    if (!BorrowManager::processReturn(
            m_recordId,
            ui->dt_actual_return_time->dateTime(),
            isDamaged(),
            equipmentId,
            errorText)) {
        QMessageBox::warning(
            this,
            QStringLiteral("归还失败"),
            errorText);
        return;
    }

    m_equipmentId = equipmentId;
    QDialog::accept();
}
