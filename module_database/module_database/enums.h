#pragma once
#include <QString>
#include <array>
#include <optional>

namespace LabAsset {

    enum class EquipmentStatus {
        Available,   // 在用
        Borrowed,    // 借出
        PendingRepair, // 待维修
        Repairing,   // 维修中
        Maintaining, // 保养中
        Scrapped     // 已报废
    };

    enum class MaintenanceStatus {
        Pending,     // 待处理
        InProgress,  // 处理中
        Completed    // 已完成
    };

    inline const std::array<EquipmentStatus, 6>& allEquipmentStatuses() {
        static const std::array<EquipmentStatus, 6> statuses = {
            EquipmentStatus::Available,
            EquipmentStatus::Borrowed,
            EquipmentStatus::PendingRepair,
            EquipmentStatus::Repairing,
            EquipmentStatus::Maintaining,
            EquipmentStatus::Scrapped
        };
        return statuses;
    }

    // 存数据库时的英文字符串 (核心契约)
    inline QString statusToDb(EquipmentStatus s) {
        switch (s) {
        case EquipmentStatus::Available:     return "available";
        case EquipmentStatus::Borrowed:      return "borrowed";
        case EquipmentStatus::PendingRepair: return "pending_repair";
        case EquipmentStatus::Repairing:     return "repairing";
        case EquipmentStatus::Maintaining:   return "maintaining";
        case EquipmentStatus::Scrapped:      return "scrapped";
        }
        return {};
    }

    // 从数据库读出来的转换
    inline std::optional<EquipmentStatus> statusFromDb(const QString& s) {
        if (s == "available")      return EquipmentStatus::Available;
        if (s == "borrowed")       return EquipmentStatus::Borrowed;
        if (s == "pending_repair") return EquipmentStatus::PendingRepair;
        if (s == "repairing")      return EquipmentStatus::Repairing;
        if (s == "maintaining")    return EquipmentStatus::Maintaining;
        if (s == "scrapped")       return EquipmentStatus::Scrapped;
        return std::nullopt;
    }

    // 界面显示的中文
    inline QString statusToDisplay(EquipmentStatus s) {
        switch (s) {
        case EquipmentStatus::Available:     return QStringLiteral("在用");
        case EquipmentStatus::Borrowed:      return QStringLiteral("借出");
        case EquipmentStatus::PendingRepair: return QStringLiteral("待维修");
        case EquipmentStatus::Repairing:     return QStringLiteral("维修中");
        case EquipmentStatus::Maintaining:   return QStringLiteral("保养中");
        case EquipmentStatus::Scrapped:      return QStringLiteral("已报废");
        }
        return {};
    }

    inline QString maintenanceStatusToDb(MaintenanceStatus s) {
        switch (s) {
        case MaintenanceStatus::Pending:    return "pending";
        case MaintenanceStatus::InProgress: return "in_progress";
        case MaintenanceStatus::Completed:  return "completed";
        }
        return {};
    }

    inline std::optional<MaintenanceStatus> maintenanceStatusFromDb(
        const QString& s) {
        if (s == "pending")     return MaintenanceStatus::Pending;
        if (s == "in_progress") return MaintenanceStatus::InProgress;
        if (s == "completed")   return MaintenanceStatus::Completed;
        return std::nullopt;
    }

    inline QString maintenanceStatusToDisplay(MaintenanceStatus s) {
        switch (s) {
        case MaintenanceStatus::Pending:    return QStringLiteral("待处理");
        case MaintenanceStatus::InProgress: return QStringLiteral("处理中");
        case MaintenanceStatus::Completed:  return QStringLiteral("已完成");
        }
        return {};
    }

}
