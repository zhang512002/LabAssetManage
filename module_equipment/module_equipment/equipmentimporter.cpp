#include "pch.h"
#include "equipmentimporter.h"

#include "qzipreader_p.h"

#include <QDate>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QSaveFile>
#include <QSet>
#include <QStringList>
#include <QXmlStreamReader>

#include <algorithm>
#include <cmath>

namespace {

enum class Column {
    AssetNo,
    Name,
    Model,
    Brand,
    PurchaseDate,
    Price,
    Location,
    Keeper,
    Remark
};

const QStringList kTemplateHeaders = {
    "资产编号*",
    "设备名称*",
    "型号",
    "品牌",
    "购置日期",
    "价格（元）",
    "存放位置",
    "责任人",
    "备注"
};

QString normalizedHeader(QString text)
{
    text = text.trimmed().toLower();
    text.remove(' ');
    text.remove('\t');
    text.remove('*');
    text.remove(':');
    text.remove(QChar(0xFF1A));
    text.replace(QChar(0xFF08), '(');
    text.replace(QChar(0xFF09), ')');
    return text;
}

int columnFromHeader(const QString& header)
{
    const QString text = normalizedHeader(header);
    if (text == "资产编号" || text == "设备编号"
        || text == "asset_no" || text == "assetno") {
        return static_cast<int>(Column::AssetNo);
    }
    if (text == "设备名称" || text == "名称" || text == "name") {
        return static_cast<int>(Column::Name);
    }
    if (text == "型号" || text == "model") {
        return static_cast<int>(Column::Model);
    }
    if (text == "品牌" || text == "brand") {
        return static_cast<int>(Column::Brand);
    }
    if (text == "购置日期" || text == "购买日期"
        || text == "purchase_date") {
        return static_cast<int>(Column::PurchaseDate);
    }
    if (text == "价格" || text == "价格(元)" || text == "单价"
        || text == "price") {
        return static_cast<int>(Column::Price);
    }
    if (text == "存放位置" || text == "存放地点"
        || text == "位置" || text == "location") {
        return static_cast<int>(Column::Location);
    }
    if (text == "责任人" || text == "保管人"
        || text == "keeper") {
        return static_cast<int>(Column::Keeper);
    }
    if (text == "备注" || text == "remark") {
        return static_cast<int>(Column::Remark);
    }
    return -1;
}

int columnIndexFromReference(const QString& reference)
{
    int result = 0;
    for (const QChar ch : reference) {
        if (ch == '$') {
            continue;
        }
        if (ch >= 'A' && ch <= 'Z') {
            result = result * 26 + (ch.unicode() - 'A' + 1);
            continue;
        }
        if (ch >= 'a' && ch <= 'z') {
            result = result * 26 + (ch.unicode() - 'a' + 1);
            continue;
        }
        break;
    }
    return result - 1;
}

QString decodeTextFile(const QByteArray& data)
{
    QString text = QString::fromUtf8(data);
    if (text.contains(QChar::ReplacementCharacter)) {
        text = QString::fromLocal8Bit(data);
    }
    if (!text.isEmpty() && text.at(0) == QChar(0xFEFF)) {
        text.remove(0, 1);
    }
    return text;
}

QChar detectDelimiter(const QString& text)
{
    QMap<QChar, int> counts;
    counts.insert(',', 0);
    counts.insert('\t', 0);
    counts.insert(';', 0);

    bool quoted = false;
    for (const QChar ch : text) {
        if (ch == '"') {
            quoted = !quoted;
        }
        else if (!quoted && (ch == '\n' || ch == '\r')) {
            break;
        }
        else if (!quoted && counts.contains(ch)) {
            counts[ch] += 1;
        }
    }

    QChar result = ',';
    int best = -1;
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
        if (it.value() > best) {
            result = it.key();
            best = it.value();
        }
    }
    return result;
}

QList<QStringList> parseDelimitedText(
    const QString& text,
    QChar delimiter,
    QString& error)
{
    QList<QStringList> rows;
    QStringList row;
    QString field;
    bool quoted = false;

    for (int index = 0; index < text.size(); ++index) {
        const QChar ch = text.at(index);
        if (ch == '"') {
            if (quoted && index + 1 < text.size()
                && text.at(index + 1) == '"') {
                field.append('"');
                ++index;
            }
            else {
                quoted = !quoted;
            }
        }
        else if (quoted) {
            field.append(ch);
        }
        else if (ch == delimiter) {
            row.append(field);
            field.clear();
        }
        else if (ch == '\r' || ch == '\n') {
            row.append(field);
            rows.append(row);
            row.clear();
            field.clear();
            if (ch == '\r' && index + 1 < text.size()
                && text.at(index + 1) == '\n') {
                ++index;
            }
        }
        else {
            field.append(ch);
        }
    }

    if (quoted) {
        error = "CSV 文件中存在未闭合的双引号。";
        return {};
    }
    if (!field.isEmpty() || !row.isEmpty()) {
        row.append(field);
        rows.append(row);
    }
    return rows;
}

QByteArray firstWorksheetData(QZipReader& zip)
{
    QString preferred = "xl/worksheets/sheet1.xml";
    QStringList worksheets;
    for (const QZipReader::FileInfo& info : zip.fileInfoList()) {
        if (info.filePath.startsWith("xl/worksheets/sheet")
            && info.filePath.endsWith(".xml")) {
            worksheets.append(info.filePath);
        }
    }
    std::sort(worksheets.begin(), worksheets.end());
    if (worksheets.contains(preferred)) {
        return zip.fileData(preferred);
    }
    if (worksheets.isEmpty()) {
        return {};
    }
    return zip.fileData(worksheets.first());
}

QStringList readSharedStrings(QZipReader& zip)
{
    QStringList values;
    const QByteArray xml = zip.fileData("xl/sharedStrings.xml");
    if (xml.isEmpty()) {
        return values;
    }

    QXmlStreamReader reader(xml);
    QString current;
    bool insideString = false;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()
            && reader.name().toString() == QStringLiteral("si")) {
            current.clear();
            insideString = true;
        }
        else if (insideString && reader.isStartElement()
                 && reader.name().toString() == QStringLiteral("t")) {
            current += reader.readElementText(
                QXmlStreamReader::IncludeChildElements);
        }
        else if (insideString && reader.isEndElement()
                 && reader.name().toString() == QStringLiteral("si")) {
            values.append(current);
            current.clear();
            insideString = false;
        }
    }
    return values;
}

QList<QStringList> readWorksheet(
    const QByteArray& xml,
    const QStringList& sharedStrings)
{
    QList<QStringList> rows;
    if (xml.isEmpty()) {
        return rows;
    }

    QXmlStreamReader reader(xml);
    QStringList row;
    int currentColumn = -1;
    QString cellType;
    QString cellValue;
    QString inlineValue;
    bool insideCell = false;

    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()
            && reader.name().toString() == QStringLiteral("row")) {
            row.clear();
        }
        else if (reader.isStartElement()
                 && reader.name().toString() == QStringLiteral("c")) {
            const QXmlStreamAttributes attrs = reader.attributes();
            currentColumn = columnIndexFromReference(
                attrs.value("r").toString());
            cellType = attrs.value("t").toString();
            cellValue.clear();
            inlineValue.clear();
            insideCell = true;
        }
        else if (insideCell && reader.isStartElement()
                 && reader.name().toString() == QStringLiteral("v")) {
            cellValue = reader.readElementText(
                QXmlStreamReader::IncludeChildElements);
        }
        else if (insideCell && reader.isStartElement()
                 && reader.name().toString() == QStringLiteral("t")) {
            inlineValue += reader.readElementText(
                QXmlStreamReader::IncludeChildElements);
        }
        else if (insideCell && reader.isEndElement()
                 && reader.name().toString() == QStringLiteral("c")) {
            QString value = cellValue;
            if (cellType == "s") {
                bool ok = false;
                const int sharedIndex = cellValue.toInt(&ok);
                value = ok && sharedIndex >= 0
                        && sharedIndex < sharedStrings.size()
                    ? sharedStrings.at(sharedIndex)
                    : QString();
            }
            else if (cellType == "inlineStr") {
                value = inlineValue;
            }
            else if (cellType == "b") {
                value = cellValue == "1" ? "是" : "否";
            }
            else if (cellType == "e") {
                value.clear();
            }

            if (currentColumn >= 0) {
                while (row.size() <= currentColumn) {
                    row.append(QString());
                }
                row[currentColumn] = value;
            }
            insideCell = false;
        }
        else if (reader.isEndElement()
                 && reader.name().toString() == QStringLiteral("row")) {
            rows.append(row);
            row.clear();
        }
    }
    return rows;
}

QString cellValue(const QStringList& row, int column)
{
    return column >= 0 && column < row.size()
        ? row.at(column).trimmed()
        : QString();
}

bool parsePurchaseDate(
    const QString& text,
    QString& result,
    QString& error)
{
    if (text.isEmpty()) {
        result.clear();
        return true;
    }

    const QStringList formats = {
        "yyyy-MM-dd",
        "yyyy-M-d",
        "yyyy/MM/dd",
        "yyyy/M/d",
        "yyyy.MM.dd",
        "yyyy.M.d",
        "yyyy年MM月dd日",
        "yyyy年M月d日"
    };
    for (const QString& format : formats) {
        const QDate date = QDate::fromString(text, format);
        if (date.isValid()) {
            result = date.toString("yyyy-MM-dd");
            return true;
        }
    }

    bool ok = false;
    const double serial = text.toDouble(&ok);
    if (ok && std::isfinite(serial) && serial > 0.0) {
        const int days = static_cast<int>(std::floor(serial));
        const QDate date = QDate(1899, 12, 31).addDays(
            days - (days >= 60 ? 1 : 0));
        if (date.isValid()) {
            result = date.toString("yyyy-MM-dd");
            return true;
        }
    }

    error = QString("购置日期“%1”无效，请使用 yyyy-MM-dd 格式。")
                .arg(text);
    return false;
}

bool parsePrice(const QString& text, double& result, QString& error)
{
    if (text.isEmpty()) {
        result = 0.0;
        return true;
    }

    QString normalized = text;
    normalized.remove(QChar(0xFFE5));
    normalized.remove(QChar(0x00A5));
    normalized.remove("元");
    normalized.remove(',');
    normalized.remove(QChar(0xFF0C));
    normalized = normalized.trimmed();

    bool ok = false;
    const double value = normalized.toDouble(&ok);
    if (!ok || !std::isfinite(value) || value < 0.0) {
        error = QString("价格“%1”无效，请填写非负数字。").arg(text);
        return false;
    }
    result = value;
    return true;
}

EquipmentImportResult convertRows(const QList<QStringList>& rows)
{
    EquipmentImportResult result;
    QHash<int, int> columns;
    int headerRowIndex = -1;

    for (int rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        const QStringList& row = rows.at(rowIndex);
        bool hasText = false;
        for (const QString& value : row) {
            if (!value.trimmed().isEmpty()) {
                hasText = true;
                break;
            }
        }
        if (!hasText) {
            continue;
        }

        headerRowIndex = rowIndex;
        for (int index = 0; index < row.size(); ++index) {
            const int column = columnFromHeader(row.at(index));
            if (column >= 0) {
                columns.insert(column, index);
            }
        }
        break;
    }

    if (headerRowIndex < 0) {
        result.error = "表格中没有可读取的数据。";
        return result;
    }
    if (!columns.contains(static_cast<int>(Column::AssetNo))
        || !columns.contains(static_cast<int>(Column::Name))) {
        result.error =
            "表头缺少“资产编号”或“设备名称”列，请使用下载的导入模板。";
        return result;
    }

    QSet<QString> assetNumbers;
    QStringList issues;
    for (int rowIndex = headerRowIndex + 1;
         rowIndex < rows.size();
         ++rowIndex) {
        const QStringList& row = rows.at(rowIndex);
        const int sourceRow = rowIndex + 1;

        auto valueOf = [&row, &columns](Column column) {
            return cellValue(
                row,
                columns.value(static_cast<int>(column), -1));
        };

        Equipment equipment;
        equipment.assetNo = valueOf(Column::AssetNo);
        equipment.name = valueOf(Column::Name);
        equipment.model = valueOf(Column::Model);
        equipment.brand = valueOf(Column::Brand);
        equipment.location = valueOf(Column::Location);
        equipment.keeper = valueOf(Column::Keeper);
        equipment.remark = valueOf(Column::Remark);

        if (equipment.assetNo.isEmpty()
            && equipment.name.isEmpty()
            && equipment.model.isEmpty()
            && equipment.brand.isEmpty()
            && equipment.location.isEmpty()
            && equipment.keeper.isEmpty()
            && equipment.remark.isEmpty()
            && valueOf(Column::PurchaseDate).isEmpty()
            && valueOf(Column::Price).isEmpty()) {
            continue;
        }

        const int issueCountBefore = issues.size();
        QString rowError;
        if (equipment.assetNo.isEmpty()) {
            issues.append(QString("第 %1 行：资产编号不能为空。")
                              .arg(sourceRow));
        }
        else if (assetNumbers.contains(equipment.assetNo)) {
            issues.append(QString("第 %1 行：资产编号“%2”在文件中重复。")
                              .arg(sourceRow)
                              .arg(equipment.assetNo));
        }
        else {
            assetNumbers.insert(equipment.assetNo);
        }

        if (equipment.name.isEmpty()) {
            issues.append(QString("第 %1 行：设备名称不能为空。")
                              .arg(sourceRow));
        }

        if (!parsePurchaseDate(
                valueOf(Column::PurchaseDate),
                equipment.purchaseDate,
                rowError)) {
            issues.append(
                QString("第 %1 行：%2").arg(sourceRow).arg(rowError));
        }
        if (!parsePrice(
                valueOf(Column::Price),
                equipment.price,
                rowError)) {
            issues.append(
                QString("第 %1 行：%2").arg(sourceRow).arg(rowError));
        }

        if (issues.size() == issueCountBefore) {
            result.equipments.append(equipment);
        }
    }

    if (result.equipments.isEmpty() && issues.isEmpty()) {
        result.error = "表格中没有设备记录。";
    }
    else if (!issues.isEmpty()) {
        constexpr int visibleIssueCount = 12;
        const int visible = std::min(
            visibleIssueCount,
            static_cast<int>(issues.size()));
        QString message = "表格校验未通过：\n";
        for (int index = 0; index < visible; ++index) {
            message += issues.at(index) + "\n";
        }
        if (issues.size() > visible) {
            message += QString("另有 %1 个问题未显示。\n")
                           .arg(issues.size() - visible);
        }
        message += "请修正后重新导入，本次不会写入任何设备。";
        result.error = message;
    }
    return result;
}

bool readXlsx(const QString& filePath, EquipmentImportResult& result)
{
    QZipReader zip(filePath);
    if (!zip.isReadable()) {
        result.error = "无法读取 Excel 文件，文件可能已损坏或不是 .xlsx 格式。";
        return false;
    }

    const QByteArray sheet = firstWorksheetData(zip);
    if (sheet.isEmpty()) {
        result.error = "Excel 文件中没有可读取的工作表。";
        return false;
    }

    const QStringList sharedStrings = readSharedStrings(zip);
    result = convertRows(readWorksheet(sheet, sharedStrings));
    return result.error.isEmpty();
}

bool readCsv(const QString& filePath, EquipmentImportResult& result)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = "无法打开 CSV 文件：" + file.errorString();
        return false;
    }

    const QString text = decodeTextFile(file.readAll());
    QString parseError;
    const QList<QStringList> rows = parseDelimitedText(
        text,
        detectDelimiter(text),
        parseError);
    if (!parseError.isEmpty()) {
        result.error = parseError;
        return false;
    }

    result = convertRows(rows);
    return result.error.isEmpty();
}

} // namespace

EquipmentImportResult EquipmentImporter::read(const QString& filePath)
{
    EquipmentImportResult result;
    const QString suffix =
        QFileInfo(filePath).suffix().toLower();
    if (suffix == "xlsx") {
        readXlsx(filePath, result);
    }
    else if (suffix == "csv") {
        readCsv(filePath, result);
    }
    else {
        result.error =
            "不支持该文件格式，请选择 .xlsx 或 .csv 文件。"
            "旧版 .xls 请先在 Excel 中另存为 .xlsx。";
    }
    return result;
}

bool EquipmentImporter::writeTemplate(
    const QString& filePath,
    QString& error)
{
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        error = "无法创建模板文件：" + file.errorString();
        return false;
    }

    QByteArray content("\xEF\xBB\xBF");
    content += kTemplateHeaders.join(',').toUtf8();
    content += "\r\n";
    if (file.write(content) != content.size()) {
        error = "写入模板文件失败：" + file.errorString();
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        error = "保存模板文件失败：" + file.errorString();
        return false;
    }
    error.clear();
    return true;
}
