// --- BorrowManager.h ---
#pragma once

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QString>

#include <optional>

#if defined(BORROW_LIBRARY)
#  define BORROW_MANAGER_EXPORT __declspec(dllexport)
#else
#  define BORROW_MANAGER_EXPORT __declspec(dllimport)
#endif

class BORROW_MANAGER_EXPORT BorrowManager final
{
public:
    enum class RecordStatus {
        Borrowed,
        Returned
    };

    struct AvailableEquipment {
        int id = -1;
        QString assetNo;
        QString name;
    };

    struct BorrowRecord {
        int id = -1;
        int equipmentId = -1;
        QString assetNo;
        QString equipmentName;
        QString borrower;
        QString borrowTime;
        QString expectedReturnTime;
        QString actualReturnTime;
        RecordStatus status = RecordStatus::Borrowed;
        QString remark;
    };

    struct ReturnInfo {
        int equipmentId = -1;
        QString assetNo;
        QString equipmentName;
        QString borrower;
        QDateTime borrowTime;
    };

    static QList<AvailableEquipment> getAvailableEquipments(
        QString& errorText);

    static bool processBorrow(
        int equipmentId,
        const QString& borrower,
        const QDateTime& borrowTime,
        const QDateTime& expectedReturnTime,
        const QString& remark,
        QString& errorText);

    static bool processBorrow(
        const QList<int>& equipmentIds,
        const QString& borrower,
        const QDateTime& borrowTime,
        const QDateTime& expectedReturnTime,
        const QString& remark,
        QString& errorText);

    static QList<BorrowRecord> getBorrowRecords(
        const QString& keyword,
        const std::optional<RecordStatus>& status,
        const QDate& fromDate,
        const QDate& toDate,
        QString& errorText);

    static bool getReturnInfo(
        int recordId,
        ReturnInfo& returnInfo,
        QString& errorText);

    static bool processReturn(
        int recordId,
        const QDateTime& actualReturnTime,
        bool damaged,
        int& returnedEquipmentId,
        QString& errorText);

private:
    BorrowManager() = delete;
};
