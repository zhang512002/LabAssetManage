#include "stdafx.h"
#include "borrowpage.h"
#include "ui_borrowpage.h"

#include "BorrowManager.h"
#include "borrowdialog.h"
#include "returndialog.h"
#include "shared/UiHelpers.h"

#include <QDate>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QTableWidgetItem>

#include <algorithm>
#include <optional>

BorrowPage::BorrowPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::BorrowPage)
{
    ui->setupUi(this);

    ui->combo_status->addItem(QStringLiteral("全部状态"), -1);
    ui->combo_status->addItem(QStringLiteral("未归还"), 0);
    ui->combo_status->addItem(QStringLiteral("已归还"), 1);

    ui->date_from->setDate(QDate::currentDate().addMonths(-1));
    ui->date_to->setDate(QDate::currentDate());

    ui->tableView->setColumnCount(9);
    ui->tableView->setHorizontalHeaderLabels({
        QStringLiteral("记录ID"),
        QStringLiteral("资产编号"),
        QStringLiteral("设备名称"),
        QStringLiteral("借用人"),
        QStringLiteral("借出时间"),
        QStringLiteral("预计归还时间"),
        QStringLiteral("实际归还时间"),
        QStringLiteral("状态"),
        QStringLiteral("备注")
    });
    ui->tableView->hideColumn(0);
    ui->tableView->verticalHeader()->setVisible(false);
    ui->tableView->setSelectionBehavior(
        QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(
        QAbstractItemView::SingleSelection);
    ui->tableView->setEditTriggers(
        QAbstractItemView::NoEditTriggers);
    ui->tableView->horizontalHeader()
        ->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableView->horizontalHeader()
        ->setStretchLastSection(true);

    connect(
        ui->btn_search,
        &QPushButton::clicked,
        this,
        &BorrowPage::refreshData);
    connect(
        ui->btn_borrow,
        &QPushButton::clicked,
        this,
        &BorrowPage::borrowEquipment);
    connect(
        ui->btn_return,
        &QPushButton::clicked,
        this,
        &BorrowPage::returnEquipment);
    connect(
        ui->btn_export_borrow,
        &QPushButton::clicked,
        this,
        &BorrowPage::exportData);
    connect(
        ui->edit_keyword,
        &QLineEdit::returnPressed,
        this,
        &BorrowPage::refreshData);
    connect(
        ui->combo_status,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this,
        [this](int) {
            refreshData();
        });

    refreshData();
}

BorrowPage::~BorrowPage()
{
    delete ui;
}

void BorrowPage::refreshData()
{
    std::optional<BorrowManager::RecordStatus> status;
    const int statusIndex =
        ui->combo_status->currentIndex();
    if (statusIndex == 1) {
        status = BorrowManager::RecordStatus::Borrowed;
    }
    else if (statusIndex == 2) {
        status = BorrowManager::RecordStatus::Returned;
    }

    QString errorText;
    const QList<BorrowManager::BorrowRecord> records =
        BorrowManager::getBorrowRecords(
            ui->edit_keyword->text(),
            status,
            ui->date_from->date(),
            ui->date_to->date(),
            errorText);

    if (!errorText.isEmpty()) {
        ui->tableView->setRowCount(0);
        ui->label_count->setText(QStringLiteral("查询失败"));
        QMessageBox::warning(
            this,
            QStringLiteral("查询失败"),
            errorText);
        return;
    }

    ui->tableView->setRowCount(records.size());
    for (int row = 0; row < records.size(); ++row) {
        const BorrowManager::BorrowRecord& record =
            records.at(row);
        const QStringList values = {
            QString::number(record.id),
            record.assetNo,
            record.equipmentName,
            record.borrower,
            record.borrowTime,
            record.expectedReturnTime,
            record.actualReturnTime,
            record.status == BorrowManager::RecordStatus::Borrowed
                ? QStringLiteral("未归还")
                : QStringLiteral("已归还"),
            record.remark
        };

        for (int column = 0; column < values.size(); ++column) {
            auto* item =
                new QTableWidgetItem(values.at(column));
            if (column != 2 && column != 8) {
                item->setTextAlignment(Qt::AlignCenter);
            }
            ui->tableView->setItem(row, column, item);
        }

        ui->tableView->item(row, 0)->setData(
            Qt::UserRole,
            record.id);
        ui->tableView->item(row, 7)->setData(
            Qt::UserRole,
            static_cast<int>(record.status));
    }

    ui->label_count->setText(
        QStringLiteral("共 %1 条记录").arg(records.size()));
}

void BorrowPage::borrowEquipment()
{
    BorrowDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    refreshData();
    emit dataChanged();
}

void BorrowPage::returnEquipment()
{
    const int row = ui->tableView->currentRow();
    if (row < 0) {
        QMessageBox::information(
            this,
            QStringLiteral("提示"),
            QStringLiteral("请先选择一条未归还记录"));
        return;
    }

    const QTableWidgetItem* idItem =
        ui->tableView->item(row, 0);
    const QTableWidgetItem* statusItem =
        ui->tableView->item(row, 7);
    if (!idItem || !statusItem) {
        return;
    }

    if (statusItem->data(Qt::UserRole).toInt()
        != static_cast<int>(
            BorrowManager::RecordStatus::Borrowed)) {
        QMessageBox::information(
            this,
            QStringLiteral("提示"),
            QStringLiteral("这条记录已经归还"));
        return;
    }

    const int recordId = idItem->data(Qt::UserRole).toInt();
    ReturnDialog dialog(recordId, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const int equipmentId = dialog.returnedEquipmentId();
    const bool damaged = dialog.isDamaged();
    refreshData();
    emit dataChanged();

    if (damaged) {
        emit damagedEquipmentReturned(equipmentId);
    }
}

void BorrowPage::exportData()
{
    if (ui->tableView->rowCount() == 0) {
        QMessageBox::information(
            this,
            QStringLiteral("提示"),
            QStringLiteral("当前没有可导出的数据"));
        return;
    }

    const QString defaultFileName =
        QStringLiteral("借还记录_%1.csv")
            .arg(QDate::currentDate().toString(
                QStringLiteral("yyyyMMdd")));
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("导出借还记录"),
        defaultFileName,
        QStringLiteral("CSV Files (*.csv)"));
    if (filePath.isEmpty()) {
        return;
    }

    QString errorText;
    if (!UiHelpers::exportModelToCsv(
            ui->tableView->model(),
            filePath,
            errorText)) {
        QMessageBox::warning(
            this,
            QStringLiteral("导出失败"),
            errorText);
        return;
    }

    QMessageBox::information(
        this,
        QStringLiteral("成功"),
        QStringLiteral("借还记录已导出"));
}
