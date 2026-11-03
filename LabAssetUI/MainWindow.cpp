#include "stdafx.h"
#include "MainWindow.h"

#include "pages/borrow/borrowpage.h"
#include "pages/equipment/equipmentpage.h"
#include "pages/maintenance/maintenancepage.h"
#include "pages/statistics/statisticspage.h"

#include <QListWidget>
#include <QStackedWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    while (ui.stackedWidget->count() > 0) {
        QWidget* placeholder = ui.stackedWidget->widget(0);
        ui.stackedWidget->removeWidget(placeholder);
        delete placeholder;
    }

    m_equipmentPage = new EquipmentPage(this);
    m_borrowPage = new BorrowPage(this);
    m_maintenancePage = new MaintenancePage(this);
    m_statisticsPage = new StatisticsPage(this);

    ui.stackedWidget->addWidget(m_equipmentPage);
    ui.stackedWidget->addWidget(m_borrowPage);
    ui.stackedWidget->addWidget(m_maintenancePage);
    ui.stackedWidget->addWidget(m_statisticsPage);

    connect(
        ui.listWidget,
        &QListWidget::currentRowChanged,
        this,
        &MainWindow::showPage);

    connect(
        m_equipmentPage,
        &EquipmentPage::dataChanged,
        this,
        &MainWindow::refreshAllPages);
    connect(
        m_borrowPage,
        &BorrowPage::dataChanged,
        this,
        &MainWindow::refreshAllPages);
    connect(
        m_maintenancePage,
        &MaintenancePage::dataChanged,
        this,
        &MainWindow::refreshAllPages);
    connect(
        m_borrowPage,
        &BorrowPage::damagedEquipmentReturned,
        m_maintenancePage,
        &MaintenancePage::createPendingRepairForDamagedReturn);

    setWindowTitle(QStringLiteral("实验室设备资产管理系统"));
    resize(1000, 700);
    ui.listWidget->setCurrentRow(0);
    showPage(0);
}

MainWindow::~MainWindow()
{
}

void MainWindow::showPage(int index)
{
    if (index < 0 || index >= ui.stackedWidget->count()) {
        return;
    }

    ui.stackedWidget->setCurrentIndex(index);
    switch (index) {
    case 0:
        m_equipmentPage->refreshData();
        break;
    case 1:
        m_borrowPage->refreshData();
        break;
    case 2:
        m_maintenancePage->refreshData();
        break;
    case 3:
        m_statisticsPage->refreshData();
        break;
    default:
        break;
    }
}

void MainWindow::refreshAllPages()
{
    m_equipmentPage->refreshData();
    m_borrowPage->refreshData();
    m_maintenancePage->refreshData();
    m_statisticsPage->refreshData();
}

