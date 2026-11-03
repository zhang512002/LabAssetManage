#pragma once

#include <QDialog>

#include "maintenancerecord.h"

class QComboBox;
class QDateEdit;
class QDialogButtonBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;

class MaintenanceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MaintenanceDialog(QWidget* parent = nullptr);
    MaintenanceDialog(
        const MaintenanceRecord& record,
        QWidget* parent = nullptr);

protected:
    void accept() override;

private:
    void setupUi();
    void loadEquipment();
    void updateStatusOptions();
    void selectRecord(const MaintenanceRecord& record);
    MaintenanceRecord record() const;

    bool m_editing = false;
    int m_recordId = -1;
    LabAsset::MaintenanceStatus m_recordStatus =
        LabAsset::MaintenanceStatus::Pending;

    QComboBox* m_equipmentCombo = nullptr;
    QDateEdit* m_dateEdit = nullptr;
    QComboBox* m_typeCombo = nullptr;
    QLabel* m_statusLabel = nullptr;
    QComboBox* m_statusCombo = nullptr;
    QDoubleSpinBox* m_costSpin = nullptr;
    QPlainTextEdit* m_descriptionEdit = nullptr;
    QLineEdit* m_operatorEdit = nullptr;
    QDialogButtonBox* m_buttonBox = nullptr;
};
