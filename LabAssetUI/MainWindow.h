#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_MainWindow.h"

class BorrowPage;
class EquipmentPage;
class MaintenancePage;
class StatisticsPage;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void showPage(int index);
    void refreshAllPages();

private:
    BorrowPage* m_borrowPage = nullptr;
    EquipmentPage* m_equipmentPage = nullptr;
    MaintenancePage* m_maintenancePage = nullptr;
    StatisticsPage* m_statisticsPage = nullptr;
    Ui::MainWindowClass ui;
};

