#include "stdafx.h"
#include "statisticspage.h"
#include "ui_statisticspage.h"

#include "StatisticsManager.h"
#include "shared/UiHelpers.h"

#include <QDate>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTableWidgetItem>

StatisticsPage::StatisticsPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::StatisticsPage)
{
    ui->setupUi(this);

    ui->tableView->setColumnCount(3);
    ui->tableView->setHorizontalHeaderLabels({
        QStringLiteral("设备状态"),
        QStringLiteral("数量（台）"),
        QStringLiteral("占比")
    });
    ui->tableView->verticalHeader()->setVisible(false);
    ui->tableView->setEditTriggers(
        QAbstractItemView::NoEditTriggers);
    ui->tableView->horizontalHeader()
        ->setSectionResizeMode(QHeaderView::Stretch);

    connect(
        ui->btn_export_equipment,
        &QPushButton::clicked,
        this,
        &StatisticsPage::exportEquipment);
    connect(
        ui->btn_export_borrow,
        &QPushButton::clicked,
        this,
        &StatisticsPage::exportBorrow);
    connect(
        ui->btn_export_maintenance,
        &QPushButton::clicked,
        this,
        &StatisticsPage::exportMaintenance);

    refreshData();
}

StatisticsPage::~StatisticsPage()
{
    delete ui;
}

void StatisticsPage::refreshData()
{
    QMap<LabAsset::EquipmentStatus, int> counts;
    int total = 0;
    QString errorText;
    if (!StatisticsManager::getEquipmentStatusCounts(
            counts,
            total,
            errorText)) {
        QMessageBox::warning(
            this,
            QStringLiteral("统计失败"),
            errorText);
        return;
    }

    auto countFor = [&counts](
                        LabAsset::EquipmentStatus status) {
        return counts.value(status, 0);
    };
    auto percentFor = [total](int count) {
        if (total == 0) {
            return QStringLiteral("0.0%");
        }
        return QString::number(
                   static_cast<double>(count) / total * 100.0,
                   'f',
                   1)
            + QStringLiteral("%");
    };

    ui->label_available->setText(
        QStringLiteral("在用设备：%1 台")
            .arg(countFor(
                LabAsset::EquipmentStatus::Available)));
    ui->label_borrowed->setText(
        QStringLiteral("借出设备：%1 台")
            .arg(countFor(
                LabAsset::EquipmentStatus::Borrowed)));
    ui->label_pending_repair->setText(
        QStringLiteral("待维修：%1 台")
            .arg(countFor(
                LabAsset::EquipmentStatus::PendingRepair)));
    ui->label_repairing->setText(
        QStringLiteral("维修中：%1 台")
            .arg(countFor(
                LabAsset::EquipmentStatus::Repairing)));
    ui->label_maintaining->setText(
        QStringLiteral("保养中：%1 台")
            .arg(countFor(
                LabAsset::EquipmentStatus::Maintaining)));
    ui->label_scrapped->setText(
        QStringLiteral("已报废：%1 台")
            .arg(countFor(
                LabAsset::EquipmentStatus::Scrapped)));

    ui->tableView->setRowCount(
        static_cast<int>(
            LabAsset::allEquipmentStatuses().size()) + 1);
    int row = 0;
    for (LabAsset::EquipmentStatus status :
         LabAsset::allEquipmentStatuses()) {
        const int count = countFor(status);
        ui->tableView->setItem(
            row,
            0,
            new QTableWidgetItem(
                LabAsset::statusToDisplay(status)));
        ui->tableView->setItem(
            row,
            1,
            new QTableWidgetItem(QString::number(count)));
        ui->tableView->setItem(
            row,
            2,
            new QTableWidgetItem(percentFor(count)));
        ++row;
    }

    ui->tableView->setItem(
        row,
        0,
        new QTableWidgetItem(QStringLiteral("总计")));
    ui->tableView->setItem(
        row,
        1,
        new QTableWidgetItem(QString::number(total)));
    ui->tableView->setItem(
        row,
        2,
        new QTableWidgetItem(QStringLiteral("100%")));

    for (int index = 0; index < ui->tableView->rowCount(); ++index) {
        for (int column = 0; column < 3; ++column) {
            ui->tableView->item(index, column)
                ->setTextAlignment(Qt::AlignCenter);
        }
    }
}

void StatisticsPage::doExport(
    StatisticsManager::ExportKind kind,
    const QString& defaultFileName)
{
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("导出数据"),
        defaultFileName,
        QStringLiteral("CSV Files (*.csv)"));
    if (filePath.isEmpty()) {
        return;
    }

    StatisticsManager::TableData table;
    QString errorText;
    if (!StatisticsManager::getExportData(
            kind,
            table,
            errorText)) {
        QMessageBox::warning(
            this,
            QStringLiteral("导出失败"),
            errorText);
        return;
    }

    QStandardItemModel model;
    model.setHorizontalHeaderLabels(table.headers);
    for (const QStringList& record : table.rows) {
        QList<QStandardItem*> items;
        for (const QString& value : record) {
            items.append(new QStandardItem(value));
        }
        model.appendRow(items);
    }

    if (!UiHelpers::exportModelToCsv(
            &model,
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
        QStringLiteral("数据已导出"));
}

void StatisticsPage::exportEquipment()
{
    doExport(
        StatisticsManager::ExportKind::EquipmentOverview,
        QStringLiteral("设备总览_%1.csv")
            .arg(QDate::currentDate().toString(
                QStringLiteral("yyyyMMdd"))));
}

void StatisticsPage::exportBorrow()
{
    doExport(
        StatisticsManager::ExportKind::BorrowRecords,
        QStringLiteral("借还记录_%1.csv")
            .arg(QDate::currentDate().toString(
                QStringLiteral("yyyyMMdd"))));
}

void StatisticsPage::exportMaintenance()
{
    doExport(
        StatisticsManager::ExportKind::MaintenanceRecords,
        QStringLiteral("维保记录_%1.csv")
            .arg(QDate::currentDate().toString(
                QStringLiteral("yyyyMMdd"))));
}
