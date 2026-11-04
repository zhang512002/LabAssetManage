#include "stdafx.h"
#include "borrowdialog.h"
#include "ui_borrowdialog.h"

#include "BorrowManager.h"

#include <QListWidget>
#include <QMessageBox>

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
        ui->label_selected_count->setText(
            QStringLiteral("读取设备失败"));
        QMessageBox::warning(
            this,
            QStringLiteral("读取设备失败"),
            errorText);
        return;
    }

    for (const BorrowManager::AvailableEquipment& equipment : equipments) {
        auto* item = new QListWidgetItem(
            equipment.assetNo
                + QStringLiteral(" - ")
                + equipment.name,
            ui->list_equipment);
        item->setData(Qt::UserRole, equipment.id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }

    connect(
        ui->edit_equipment_search,
        &QLineEdit::textChanged,
        this,
        &BorrowDialog::filterEquipment);
    connect(
        ui->list_equipment,
        &QListWidget::itemChanged,
        this,
        [this](QListWidgetItem*) {
            filterEquipment(ui->edit_equipment_search->text());
        });
    filterEquipment(ui->edit_equipment_search->text());
}

BorrowDialog::~BorrowDialog()
{
    delete ui;
}

void BorrowDialog::filterEquipment(const QString& text)
{
    const QString keyword = text.trimmed();
    bool hasMatches = false;

    for (int row = 0; row < ui->list_equipment->count(); ++row) {
        QListWidgetItem* item = ui->list_equipment->item(row);
        const bool matches =
            item->text().contains(keyword, Qt::CaseInsensitive);
        item->setHidden(!matches);
        hasMatches = hasMatches || matches;
    }

    QString countText =
        QStringLiteral("已选 %1 台").arg(equipmentIds().size());
    if (ui->list_equipment->count() == 0) {
        countText += QStringLiteral("，暂无可借设备");
    }
    else if (!hasMatches) {
        countText += QStringLiteral("，没有匹配的设备");
    }
    ui->label_selected_count->setText(countText);
}

QList<int> BorrowDialog::equipmentIds() const
{
    QList<int> ids;
    for (int row = 0; row < ui->list_equipment->count(); ++row) {
        const QListWidgetItem* item =
            ui->list_equipment->item(row);
        if (item->checkState() == Qt::Checked) {
            ids.append(item->data(Qt::UserRole).toInt());
        }
    }
    return ids;
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
            equipmentIds(),
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
