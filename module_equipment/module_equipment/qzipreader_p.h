#ifndef QZIPREADER_H
#define QZIPREADER_H

#include <QtCore/qglobal.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qfile.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

class QZipReaderPrivate;

class QZipReader
{
public:
    explicit QZipReader(const QString& fileName, QIODevice::OpenMode mode = QIODevice::ReadOnly);
    explicit QZipReader(QIODevice* device);
    ~QZipReader();

    QIODevice* device() const;
    bool isReadable() const;
    bool exists() const;

    struct FileInfo {
        FileInfo() : isDir(false), isFile(false), isSymLink(false), size(0), crc(0) {}
        bool isDir;
        bool isFile;
        bool isSymLink;
        qint64 size;
        uint crc;
        QString filePath;
        QDateTime lastModified;
        Qt::CaseSensitivity caseSensitivity;
    };

    QList<FileInfo> fileInfoList() const;
    int count() const;

    FileInfo entryInfoAt(int index) const;
    QByteArray fileData(const QString& fileName) const;
    bool extractAll(const QString& destinationDir) const;

    enum Status {
        NoError,
        FileReadError,
        FileOpenError,
        FilePermissionsError,
        FileError
    };

    Status status() const;

private:
    Q_DISABLE_COPY(QZipReader)
        Q_DECLARE_PRIVATE(QZipReader)
        QZipReaderPrivate* d_ptr;
};

#endif // QZIPREADER_H