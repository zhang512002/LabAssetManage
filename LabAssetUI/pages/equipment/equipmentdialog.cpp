#include "stdafx.h"
#include "equipmentdialog.h"

#include "EquipmentManager.h"

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

EquipmentDialog::EquipmentDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    setWindowTitle(QStringLiteral("新增设备"));
}

EquipmentDialog::EquipmentDialog(
    const Equipment& equipment,
    QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    setWindowTitle(QStringLiteral("编辑设备"));
    setEquipment(equipment);
}

void EquipmentDialog::setupUi()
{
    setMinimumWidth(520);

    auto* rootLayout = new QVBoxLayout(this);
    auto* formLayout = new QFormLayout();
    formLayout->setFieldGrowthPolicy(
        QFormLayout::AllNonFixedFieldsGrow);

    m_assetNoEdit = new QLineEdit(this);
    formLayout->addRow(
        QStringLiteral("资产编号 *："),
        m_assetNoEdit);

    m_nameEdit = new QLineEdit(this);
    formLayout->addRow(
        QStringLiteral("设备名称 *："),
        m_nameEdit);

    m_modelEdit = new QLineEdit(this);
    formLayout->addRow(QStringLiteral("型号："), m_modelEdit);

    m_brandEdit = new QLineEdit(this);
    formLayout->addRow(QStringLiteral("品牌："), m_brandEdit);

    m_purchaseDateEdit = new QDateEdit(
        QDate::currentDate(),
        this);
    m_purchaseDateEdit->setCalendarPopup(true);
    m_purchaseDateEdit->setDisplayFormat(
        QStringLiteral("yyyy-MM-dd"));
    formLayout->addRow(
        QStringLiteral("购置日期："),
        m_purchaseDateEdit);

    m_priceSpin = new QDoubleSpinBox(this);
    m_priceSpin->setRange(0.0, 1000000000000.0);
    m_priceSpin->setDecimals(2);
    m_priceSpin->setSuffix(QStringLiteral(" 元"));
    formLayout->addRow(QStringLiteral("价格："), m_priceSpin);

    m_locationEdit = new QLineEdit(this);
    formLayout->addRow(
        QStringLiteral("存放位置："),
        m_locationEdit);

    m_statusValueLabel = new QLabel(
        LabAsset::statusToDisplay(m_status),
        this);
    formLayout->addRow(
        QStringLiteral("设备状态："),
        m_statusValueLabel);

    m_keeperEdit = new QLineEdit(this);
    formLayout->addRow(
        QStringLiteral("责任人："),
        m_keeperEdit);

    m_remarkEdit = new QPlainTextEdit(this);
    m_remarkEdit->setMinimumHeight(80);
    formLayout->addRow(
        QStringLiteral("备注："),
        m_remarkEdit);

    rootLayout->addLayout(formLayout);
    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        this);
    m_buttonBox->button(QDialogButtonBox::Ok)
        ->setText(QStringLiteral("保存"));
    m_buttonBox->button(QDialogButtonBox::Cancel)
        ->setText(QStringLiteral("取消"));
    rootLayout->addWidget(m_buttonBox);

    connect(
        m_buttonBox,
        &QDialogButtonBox::accepted,
        this,
        &EquipmentDialog::accept);
    connect(
        m_buttonBox,
        &QDialogButtonBox::rejected,
        this,
        &EquipmentDialog::reject);
}

void EquipmentDialog::setEquipment(
    const Equipment& equipment)
{
    m_equipmentId = equipment.id;
    m_assetNoEdit->setText(equipment.assetNo);
    m_nameEdit->setText(equipment.name);
    m_modelEdit->setText(equipment.model);
    m_brandEdit->setText(equipment.brand);

    const QDate date = QDate::fromString(
        equipment.purchaseDate,
        QStringLiteral("yyyy-MM-dd"));
    if (date.isValid()) {
        m_purchaseDateEdit->setDate(date);
    }

    m_priceSpin->setValue(equipment.price);
    m_locationEdit->setText(equipment.location);
    m_status = equipment.status;
    m_statusValueLabel->setText(
        LabAsset::statusToDisplay(m_status));
    m_keeperEdit->setText(equipment.keeper);
    m_remarkEdit->setPlainText(equipment.remark);
}

Equipment EquipmentDialog::equipment() const
{
    Equipment result;
    result.id = m_equipmentId;
    result.assetNo = m_assetNoEdit->text().trimmed();
    result.name = m_nameEdit->text().trimmed();
    result.model = m_modelEdit->text().trimmed();
    result.brand = m_brandEdit->text().trimmed();
    result.purchaseDate =
        m_purchaseDateEdit->date().toString(
            QStringLiteral("yyyy-MM-dd"));
    result.price = m_priceSpin->value();
    result.location = m_locationEdit->text().trimmed();
    result.status = m_status;
    result.keeper = m_keeperEdit->text().trimmed();
    result.remark = m_remarkEdit->toPlainText().trimmed();
    return result;
}

void EquipmentDialog::accept()
{
    Equipment candidate = equipment();
    QString errorText;
    const bool succeeded = m_equipmentId > 0
        ? EquipmentManager::processUpdate(candidate, errorText)
        : EquipmentManager::processAdd(candidate, errorText);
    if (!succeeded) {
        QMessageBox::warning(
            this,
            QStringLiteral("保存失败"),
            errorText);
        return;
    }

    QDialog::accept();
}
