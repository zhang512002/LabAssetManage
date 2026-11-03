#include "stdafx.h"
#include "maintenancepage.h"
#include "ui_maintenancepage.h"

#include "MaintenanceManager.h"
#include "maintenancedialog.h"
#include "shared/UiHelpers.h"

#include <QComboBox>
#include <QDate>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QTableWidgetItem>

MaintenancePage::MaintenancePage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::MaintenancePage)
{
    ui->setupUi(this);

    ui->tableView->setColumnCount(8);
    ui->tableView->setHorizontalHeaderLabels({
        QStringLiteral("资产编号"),
        QStringLiteral("设备名称"),
        QStringLiteral("日期"),
        QStringLiteral("类型"),
        QStringLiteral("任务状态"),
        QStringLiteral("说明"),
        QStringLiteral("费用"),
        QStringLiteral("经办人")
    });
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
        ->setSectionResizeMode(5, QHeaderView::Stretch);

    connect(
        ui->combo_equipment,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this,
        [this](int) {
            refreshData();
        });
    connect(
        ui->btn_add,
        &QPushButton::clicked,
        this,
        &MaintenancePage::addRecord);
    connect(
        ui->btn_edit,
        &QPushButton::clicked,
        this,
        &MaintenancePage::editRecord);
    connect(
        ui->btn_start,
        &QPushButton::clicked,
        this,
        &MaintenancePage::startRecord);
    connect(
        ui->btn_complete,
        &QPushButton::clicked,
        this,
        &MaintenancePage::completeRecord);
    connect(
        ui->btn_refresh,
        &QPushButton::clicked,
        this,
        &MaintenancePage::refreshData);
    connect(
        ui->btn_export,
        &QPushButton::clicked,
        this,
        &MaintenancePage::exportData);
    connect(
        ui->tableView,
        &QTableWidget::itemSelectionChanged,
        this,
        &MaintenancePage::updateActionButtons);
    connect(
        ui->tableView,
        &QTableWidget::cellDoubleClicked,
        this,
        [this](int, int) {
            editRecord();
        });

    refreshData();
}

MaintenancePage::~MaintenancePage()
{
    delete ui;
}

bool MaintenancePage::refreshEquipmentFilter()
{
    const int previousId =
        ui->combo_equipment->currentData().toInt();
    QString errorText;
    const QList<MaintenanceEquipmentOption> options =
        MaintenanceManager::getEquipmentOptions(
            false,
            errorText);
    if (!errorText.isEmpty()) {
        QMessageBox::warning(
            this,
            QStringLiteral("查询失败"),
            errorText);
        return false;
    }

    QSignalBlocker blocker(ui->combo_equipment);
    ui->combo_equipment->clear();
    ui->combo_equipment->addItem(
        QStringLiteral("全部设备"),
        -1);
    for (const MaintenanceEquipmentOption& option : options) {
        ui->combo_equipment->addItem(
            option.assetNo + QStringLiteral(" - ") + option.name,
            option.id);
    }

    const int previousIndex =
        ui->combo_equipment->findData(previousId);
    ui->combo_equipment->setCurrentIndex(
        previousIndex >= 0 ? previousIndex : 0);
    return true;
}

void MaintenancePage::refreshData()
{
    if (!refreshEquipmentFilter()) {
        return;
    }

    QString errorText;
    m_records = MaintenanceManager::getLedger(
        ui->combo_equipment->currentData().toInt(),
        errorText);
    if (!errorText.isEmpty()) {
        m_records.clear();
        ui->tableView->setRowCount(0);
        ui->label_status->setText(
            QStringLiteral("查询失败：") + errorText);
        QMessageBox::warning(
            this,
            QStringLiteral("查询失败"),
            errorText);
        return;
    }

    ui->tableView->setRowCount(m_records.size());
    for (int row = 0; row < m_records.size(); ++row) {
        const MaintenanceLedgerItem& item = m_records.at(row);
        const QStringList values = {
            item.assetNo,
            item.equipmentName,
            item.record.maintenanceDate,
            item.record.type == QStringLiteral("repair")
                ? QStringLiteral("维修")
                : QStringLiteral("保养"),
            LabAsset::maintenanceStatusToDisplay(
                item.record.status),
            item.record.description,
            QString::number(item.record.cost, 'f', 2),
            item.record.operatorName
        };

        for (int column = 0; column < values.size(); ++column) {
            auto* cell =
                new QTableWidgetItem(values.at(column));
            if (column == 0 || column == 2 || column == 3
                || column == 4 || column == 6) {
                cell->setTextAlignment(Qt::AlignCenter);
            }
            ui->tableView->setItem(row, column, cell);
        }
    }

    ui->label_status->setText(
        QStringLiteral("共 %1 条记录").arg(m_records.size()));
    updateActionButtons();
}

void MaintenancePage::updateActionButtons()
{
    const int row = ui->tableView->currentRow();
    const bool selected =
        row >= 0 && row < m_records.size();

    ui->btn_edit->setEnabled(selected);
    ui->btn_start->setEnabled(
        selected
        && m_records.at(row).record.status
            == LabAsset::MaintenanceStatus::Pending);
    ui->btn_complete->setEnabled(
        selected
        && m_records.at(row).record.status
            == LabAsset::MaintenanceStatus::InProgress);
}

void MaintenancePage::addRecord()
{
    MaintenanceDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    refreshData();
    emit dataChanged();
}

void MaintenancePage::editRecord()
{
    const int row = ui->tableView->currentRow();
    if (row < 0 || row >= m_records.size()) {
        QMessageBox::information(
            this,
            QStringLiteral("提示"),
            QStringLiteral("请先选择一条记录"));
        return;
    }

    MaintenanceDialog dialog(m_records.at(row).record, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    refreshData();
    emit dataChanged();
}

void MaintenancePage::startRecord()
{
    const int row = ui->tableView->currentRow();
    if (row < 0 || row >= m_records.size()) {
        return;
    }

    QString errorText;
    if (!MaintenanceManager::processStart(
            m_records.at(row).record.id,
            errorText)) {
        QMessageBox::warning(
            this,
            QStringLiteral("开始处理失败"),
            errorText);
        return;
    }

    refreshData();
    emit dataChanged();
}

void MaintenancePage::completeRecord()
{
    const int row = ui->tableView->currentRow();
    if (row < 0 || row >= m_records.size()) {
        return;
    }

    QString errorText;
    if (!MaintenanceManager::processComplete(
            m_records.at(row).record.id,
            errorText)) {
        QMessageBox::warning(
            this,
            QStringLiteral("完成处理失败"),
            errorText);
        return;
    }

    refreshData();
    emit dataChanged();
}

bool MaintenancePage::createPendingRepairForDamagedReturn(
    int equipmentId)
{
    QString errorText;
    if (!MaintenanceManager::
            createPendingRepairForDamagedReturn(
                equipmentId,
                errorText)) {
        QMessageBox::warning(
            this,
            QStringLiteral("创建维修任务失败"),
            errorText);
        return false;
    }

    refreshData();
    emit dataChanged();
    return true;
}

void MaintenancePage::exportData()
{
    if (ui->tableView->rowCount() == 0) {
        QMessageBox::information(
            this,
            QStringLiteral("提示"),
            QStringLiteral("当前没有可导出的数据"));
        return;
    }

    const QString defaultFileName =
        QStringLiteral("维保记录_%1.csv")
            .arg(QDate::currentDate().toString(
                QStringLiteral("yyyyMMdd")));
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("导出维保记录"),
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
        QStringLiteral("维保记录已导出"));
}
