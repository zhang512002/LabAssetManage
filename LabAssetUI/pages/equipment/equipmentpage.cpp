#include "stdafx.h"
#include "equipmentpage.h"
#include "ui_equipmentpage.h"

#include "EquipmentManager.h"
#include "equipmentdialog.h"
#include "shared/UiHelpers.h"

#include <QComboBox>
#include <QDate>
#include <QFileDialog>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QTableWidgetItem>

#include <algorithm>
#include <optional>

EquipmentPage::EquipmentPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::EquipmentPage)
{
    ui->setupUi(this);

    ui->combo_status->addItem(QStringLiteral("全部状态"), -1);
    for (LabAsset::EquipmentStatus status :
         LabAsset::allEquipmentStatuses()) {
        ui->combo_status->addItem(
            LabAsset::statusToDisplay(status),
            static_cast<int>(status));
    }

    ui->tableView->setColumnCount(10);
    ui->tableView->setHorizontalHeaderLabels({
        QStringLiteral("资产编号"),
        QStringLiteral("设备名称"),
        QStringLiteral("型号"),
        QStringLiteral("品牌"),
        QStringLiteral("购置日期"),
        QStringLiteral("价格（元）"),
        QStringLiteral("存放位置"),
        QStringLiteral("状态"),
        QStringLiteral("责任人"),
        QStringLiteral("备注")
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
        ->setStretchLastSection(true);

    connect(
        ui->btn_search,
        &QPushButton::clicked,
        this,
        &EquipmentPage::refreshData);
    connect(
        ui->edit_keyword,
        &QLineEdit::returnPressed,
        this,
        &EquipmentPage::refreshData);
    connect(
        ui->combo_status,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this,
        [this](int) {
            refreshData();
        });
    connect(
        ui->btn_add,
        &QPushButton::clicked,
        this,
        &EquipmentPage::addEquipment);
    connect(
        ui->btn_edit,
        &QPushButton::clicked,
        this,
        &EquipmentPage::editEquipment);
    connect(
        ui->btn_delete,
        &QPushButton::clicked,
        this,
        &EquipmentPage::deleteEquipment);
    connect(
        ui->btn_refresh,
        &QPushButton::clicked,
        this,
        &EquipmentPage::refreshData);
    connect(
        ui->btn_import_excel,
        &QPushButton::clicked,
        this,
        &EquipmentPage::importEquipment);
    connect(
        ui->btn_download_template,
        &QPushButton::clicked,
        this,
        &EquipmentPage::downloadImportTemplate);
    connect(
        ui->btn_export_equipment,
        &QPushButton::clicked,
        this,
        &EquipmentPage::exportData);
    connect(
        ui->tableView,
        &QTableWidget::cellDoubleClicked,
        this,
        [this](int, int) {
            editEquipment();
        });

    refreshData();
}

EquipmentPage::~EquipmentPage()
{
    delete ui;
}

void EquipmentPage::refreshData()
{
    std::optional<LabAsset::EquipmentStatus> status;
    const int statusValue =
        ui->combo_status->currentData().toInt();
    if (statusValue >= 0) {
        status = static_cast<LabAsset::EquipmentStatus>(
            statusValue);
    }

    QString errorText;
    m_equipment = EquipmentManager::getEquipments(
        ui->edit_keyword->text(),
        status,
        errorText);
    if (!errorText.isEmpty()) {
        m_equipment.clear();
        ui->tableView->setRowCount(0);
        ui->label_status->setText(
            QStringLiteral("查询失败：") + errorText);
        QMessageBox::warning(
            this,
            QStringLiteral("查询失败"),
            errorText);
        return;
    }

    ui->tableView->setRowCount(m_equipment.size());
    for (int row = 0; row < m_equipment.size(); ++row) {
        const Equipment& equipment = m_equipment.at(row);
        const QStringList values = {
            equipment.assetNo,
            equipment.name,
            equipment.model,
            equipment.brand,
            equipment.purchaseDate,
            QString::number(equipment.price, 'f', 2),
            equipment.location,
            LabAsset::statusToDisplay(equipment.status),
            equipment.keeper,
            equipment.remark
        };

        for (int column = 0; column < values.size(); ++column) {
            auto* item =
                new QTableWidgetItem(values.at(column));
            if (column == 0 || column == 4 || column == 5
                || column == 7) {
                item->setTextAlignment(Qt::AlignCenter);
            }
            ui->tableView->setItem(row, column, item);
        }
        ui->tableView->item(row, 0)->setData(
            Qt::UserRole,
            equipment.id);
    }

    ui->label_status->setText(
        QStringLiteral("共找到 %1 台设备")
            .arg(m_equipment.size()));
}

int EquipmentPage::selectedEquipmentId() const
{
    const int row = ui->tableView->currentRow();
    if (row < 0) {
        return -1;
    }

    const QTableWidgetItem* item =
        ui->tableView->item(row, 0);
    return item ? item->data(Qt::UserRole).toInt() : -1;
}

const Equipment* EquipmentPage::findEquipment(int id) const
{
    const auto it = std::find_if(
        m_equipment.cbegin(),
        m_equipment.cend(),
        [id](const Equipment& equipment) {
            return equipment.id == id;
        });
    return it == m_equipment.cend() ? nullptr : &(*it);
}

void EquipmentPage::addEquipment()
{
    EquipmentDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    refreshData();
    emit dataChanged();
}

void EquipmentPage::editEquipment()
{
    const Equipment* equipment =
        findEquipment(selectedEquipmentId());
    if (!equipment) {
        QMessageBox::information(
            this,
            QStringLiteral("提示"),
            QStringLiteral("请先选择一台设备"));
        return;
    }

    EquipmentDialog dialog(*equipment, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    refreshData();
    emit dataChanged();
}

void EquipmentPage::deleteEquipment()
{
    const Equipment* equipment =
        findEquipment(selectedEquipmentId());
    if (!equipment) {
        QMessageBox::information(
            this,
            QStringLiteral("提示"),
            QStringLiteral("请先选择一台设备"));
        return;
    }

    const int id = equipment->id;
    const QString description =
        equipment->assetNo + QStringLiteral(" - ")
        + equipment->name;
    const auto answer = QMessageBox::question(
        this,
        QStringLiteral("确认删除"),
        QStringLiteral("确定永久删除设备“%1”吗？")
            .arg(description),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    QString errorText;
    if (!EquipmentManager::processDelete(id, errorText)) {
        QMessageBox::warning(
            this,
            QStringLiteral("无法删除"),
            errorText);
        return;
    }

    refreshData();
    emit dataChanged();
}

void EquipmentPage::importEquipment()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("导入设备"),
        QString(),
        QStringLiteral(
            "Excel 工作簿 (*.xlsx);;CSV 文件 (*.csv);;"
            "所有文件 (*.*)"));
    if (filePath.isEmpty()) {
        return;
    }

    QList<Equipment> equipments;
    QString errorText;
    if (!EquipmentManager::readImportFile(
            filePath,
            equipments,
            errorText)) {
        QMessageBox::warning(
            this,
            QStringLiteral("导入失败"),
            errorText);
        return;
    }
    if (equipments.isEmpty()) {
        QMessageBox::warning(
            this,
            QStringLiteral("导入失败"),
            QStringLiteral("文件中没有可导入的设备"));
        return;
    }

    const auto answer = QMessageBox::question(
        this,
        QStringLiteral("确认导入"),
        QStringLiteral("已读取 %1 条设备记录，是否全部导入？")
            .arg(equipments.size()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    if (answer != QMessageBox::Yes) {
        return;
    }

    int importedCount = 0;
    if (!EquipmentManager::processImport(
            equipments,
            importedCount,
            errorText)) {
        QMessageBox::warning(
            this,
            QStringLiteral("导入失败"),
            errorText
                + QStringLiteral(
                    "\n\n本次没有写入任何设备。"));
        return;
    }

    refreshData();
    emit dataChanged();
    QMessageBox::information(
        this,
        QStringLiteral("导入成功"),
        QStringLiteral("已导入 %1 台设备")
            .arg(importedCount));
}

void EquipmentPage::downloadImportTemplate()
{
    QString filePath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("保存导入模板"),
        QStringLiteral("设备导入模板.csv"),
        QStringLiteral("CSV 文件 (*.csv)"));
    if (filePath.isEmpty()) {
        return;
    }
    if (!filePath.endsWith(
            QStringLiteral(".csv"),
            Qt::CaseInsensitive)) {
        filePath += QStringLiteral(".csv");
    }

    QString errorText;
    if (!EquipmentManager::writeImportTemplate(
            filePath,
            errorText)) {
        QMessageBox::warning(
            this,
            QStringLiteral("保存失败"),
            errorText);
        return;
    }

    QMessageBox::information(
        this,
        QStringLiteral("成功"),
        QStringLiteral("设备导入模板已保存"));
}

void EquipmentPage::exportData()
{
    if (ui->tableView->rowCount() == 0) {
        QMessageBox::information(
            this,
            QStringLiteral("提示"),
            QStringLiteral("当前没有可导出的数据"));
        return;
    }

    const QString defaultFileName =
        QStringLiteral("设备列表_%1.csv")
            .arg(QDate::currentDate().toString(
                QStringLiteral("yyyyMMdd")));
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("导出设备"),
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
        QStringLiteral("设备列表已导出"));
}
