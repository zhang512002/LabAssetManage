#include "stdafx.h"
#include "maintenancedialog.h"

#include "MaintenanceManager.h"

#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

MaintenanceDialog::MaintenanceDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    loadEquipment();
    updateStatusOptions();
}

MaintenanceDialog::MaintenanceDialog(
    const MaintenanceRecord& record,
    QWidget* parent)
    : QDialog(parent)
    , m_editing(true)
{
    setupUi();
    loadEquipment();
    selectRecord(record);
    setWindowTitle(QStringLiteral("编辑维保记录"));
}

void MaintenanceDialog::setupUi()
{
    setWindowTitle(QStringLiteral("新增维保记录"));
    setMinimumWidth(500);

    auto* rootLayout = new QVBoxLayout(this);
    auto* formLayout = new QFormLayout();

    m_equipmentCombo = new QComboBox(this);
    formLayout->addRow(
        QStringLiteral("设备："),
        m_equipmentCombo);

    m_dateEdit = new QDateEdit(QDate::currentDate(), this);
    m_dateEdit->setCalendarPopup(true);
    m_dateEdit->setDisplayFormat(
        QStringLiteral("yyyy-MM-dd"));
    formLayout->addRow(QStringLiteral("日期："), m_dateEdit);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem(
        QStringLiteral("维修"),
        QStringLiteral("repair"));
    m_typeCombo->addItem(
        QStringLiteral("保养"),
        QStringLiteral("maintain"));
    formLayout->addRow(QStringLiteral("类型："), m_typeCombo);

    m_statusLabel = new QLabel(
        QStringLiteral("任务状态："),
        this);
    m_statusCombo = new QComboBox(this);
    formLayout->addRow(m_statusLabel, m_statusCombo);

    m_costSpin = new QDoubleSpinBox(this);
    m_costSpin->setRange(0.0, 99999999.0);
    m_costSpin->setDecimals(2);
    m_costSpin->setSuffix(QStringLiteral(" 元"));
    formLayout->addRow(QStringLiteral("费用："), m_costSpin);

    m_descriptionEdit = new QPlainTextEdit(this);
    m_descriptionEdit->setMinimumHeight(90);
    formLayout->addRow(
        QStringLiteral("说明："),
        m_descriptionEdit);

    m_operatorEdit = new QLineEdit(this);
    formLayout->addRow(
        QStringLiteral("经办人："),
        m_operatorEdit);

    rootLayout->addLayout(formLayout);
    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        this);
    rootLayout->addWidget(m_buttonBox);

    connect(
        m_typeCombo,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this,
        [this](int) {
            updateStatusOptions();
        });
    connect(
        m_buttonBox,
        &QDialogButtonBox::accepted,
        this,
        &MaintenanceDialog::accept);
    connect(
        m_buttonBox,
        &QDialogButtonBox::rejected,
        this,
        &MaintenanceDialog::reject);
}

void MaintenanceDialog::loadEquipment()
{
    m_equipmentCombo->clear();

    QString errorText;
    const QList<MaintenanceEquipmentOption> options =
        MaintenanceManager::getEquipmentOptions(
            !m_editing,
            errorText);
    if (!errorText.isEmpty()) {
        m_buttonBox->button(QDialogButtonBox::Ok)
            ->setEnabled(false);
        QMessageBox::warning(
            this,
            QStringLiteral("读取设备失败"),
            errorText);
        return;
    }

    for (const MaintenanceEquipmentOption& option : options) {
        m_equipmentCombo->addItem(
            option.assetNo + QStringLiteral(" - ") + option.name,
            option.id);
    }
    if (m_equipmentCombo->count() == 0) {
        m_equipmentCombo->addItem(
            QStringLiteral("暂无可登记设备"),
            -1);
        m_buttonBox->button(QDialogButtonBox::Ok)
            ->setEnabled(false);
    }
}

void MaintenanceDialog::updateStatusOptions()
{
    m_statusCombo->clear();
    if (m_typeCombo->currentData().toString()
        == QStringLiteral("repair")) {
        m_statusCombo->addItem(
            LabAsset::maintenanceStatusToDisplay(
                LabAsset::MaintenanceStatus::Pending),
            static_cast<int>(
                LabAsset::MaintenanceStatus::Pending));
        m_statusCombo->addItem(
            LabAsset::maintenanceStatusToDisplay(
                LabAsset::MaintenanceStatus::InProgress),
            static_cast<int>(
                LabAsset::MaintenanceStatus::InProgress));
    }
    else {
        m_statusCombo->addItem(
            LabAsset::maintenanceStatusToDisplay(
                LabAsset::MaintenanceStatus::InProgress),
            static_cast<int>(
                LabAsset::MaintenanceStatus::InProgress));
    }

    m_statusLabel->setVisible(!m_editing);
    m_statusCombo->setVisible(!m_editing);
}

void MaintenanceDialog::selectRecord(
    const MaintenanceRecord& record)
{
    m_recordId = record.id;
    const int equipmentIndex =
        m_equipmentCombo->findData(record.equipmentId);
    if (equipmentIndex >= 0) {
        m_equipmentCombo->setCurrentIndex(equipmentIndex);
    }

    const QDate date = QDate::fromString(
        record.maintenanceDate,
        QStringLiteral("yyyy-MM-dd"));
    if (date.isValid()) {
        m_dateEdit->setDate(date);
    }

    const int typeIndex =
        m_typeCombo->findData(record.type);
    if (typeIndex >= 0) {
        m_typeCombo->setCurrentIndex(typeIndex);
    }

    m_costSpin->setValue(record.cost);
    m_descriptionEdit->setPlainText(record.description);
    m_operatorEdit->setText(record.operatorName);
    m_recordStatus = record.status;

    const bool completed =
        record.status
        == LabAsset::MaintenanceStatus::Completed;
    m_equipmentCombo->setEnabled(completed);
    m_typeCombo->setEnabled(completed);
    updateStatusOptions();
}

MaintenanceRecord MaintenanceDialog::record() const
{
    MaintenanceRecord result;
    result.id = m_recordId;
    result.equipmentId =
        m_equipmentCombo->currentData().toInt();
    result.maintenanceDate =
        m_dateEdit->date().toString(
            QStringLiteral("yyyy-MM-dd"));
    result.type =
        m_typeCombo->currentData().toString();
    result.description =
        m_descriptionEdit->toPlainText().trimmed();
    result.cost = m_costSpin->value();
    result.operatorName =
        m_operatorEdit->text().trimmed();
    result.status = m_editing
        ? m_recordStatus
        : static_cast<LabAsset::MaintenanceStatus>(
            m_statusCombo->currentData().toInt());
    return result;
}

void MaintenanceDialog::accept()
{
    MaintenanceRecord candidate = record();
    QString errorText;
    const bool succeeded = m_editing
        ? MaintenanceManager::processUpdate(
            candidate,
            errorText)
        : MaintenanceManager::processAdd(
            candidate,
            errorText);
    if (!succeeded) {
        QMessageBox::warning(
            this,
            QStringLiteral("保存失败"),
            errorText);
        return;
    }

    QDialog::accept();
}
