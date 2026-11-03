#include "pch.h" 
#include "qzipreader_p.h"

#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qdir.h>
#include <QtCore/qendian.h>
#include <QtCore/qdebug.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qvector.h>
#include <QtCore/qbuffer.h>

#ifndef QT_NO_TEXTODFWRITER



class QZipReaderPrivate
{
public:
    QZipReaderPrivate(QIODevice* device, bool ownDev)
        : device(device), ownDevice(ownDev), status(QZipReader::NoError), init(false) {}

    ~QZipReaderPrivate() {
        if (ownDevice)
            delete device;
    }

    void scanFiles();

    struct FileInfo {
        QString filePath;
        quint32 crc;
        quint32 size;
        quint16 lastModifiedDate;
        quint16 lastModifiedTime;
        quint32 offset;
        quint32 compressionMethod;
        bool isDir;
        bool isFile;
        bool isSymLink;
        QByteArray symLinkTarget;
    };

    QIODevice* device;
    bool ownDevice;
    QZipReader::Status status;
    bool init;
    QList<FileInfo> fileList;
    quint32 start_of_directory;
};

static inline uint readUInt(const uchar* data)
{
    return (data[0]) + (data[1] << 8) + (data[2] << 16) + (data[3] << 24);
}

static inline ushort readUShort(const uchar* data)
{
    return (data[0]) + (data[1] << 8);
}

static inline void copyUInt(uchar* dest, uint i)
{
    dest[0] = i & 0xff;
    dest[1] = (i >> 8) & 0xff;
    dest[2] = (i >> 16) & 0xff;
    dest[3] = (i >> 24) & 0xff;
}

static inline void copyUShort(uchar* dest, ushort i)
{
    dest[0] = i & 0xff;
    dest[1] = (i >> 8) & 0xff;
}

static void readMSDosDate(uchar* dest, QDateTime& dt)
{
    quint16 time = readUShort(dest);
    quint16 date = readUShort(dest + 2);
    dt = QDateTime(QDate(((date >> 9) & 0x7f) + 1980, (date >> 5) & 0x0f, date & 0x1f),
        QTime((time >> 11) & 0x1f, (time >> 5) & 0x3f, (time & 0x1f) << 1));
}

void QZipReaderPrivate::scanFiles()
{
    if (init)
        return;

    init = true;

    if (!device->isOpen())
        device->open(QIODevice::ReadOnly);

    if (!device->isReadable() || !device->size()) {
        status = QZipReader::FileOpenError;
        return;
    }

    // find start of central directory
    device->seek(device->size() - 22);
    uchar tmp[4];
    if (device->read((char*)tmp, 4) != 4 || readUInt(tmp) != 0x06054b50) {
        status = QZipReader::FileOpenError;
        return;
    }

    uchar tmp2[20];
    if (device->read((char*)tmp2, 20) != 20) {
        status = QZipReader::FileOpenError;
        return;
    }

    quint16 numEntries = readUShort(tmp2 + 8);
    quint32 cdOffset = readUInt(tmp2 + 14);

    device->seek(cdOffset);

    for (quint16 i = 0; i < numEntries; ++i) {
        uchar hdr[46];
        if (device->read((char*)hdr, 46) != 46 || readUInt(hdr) != 0x02014b50) {
            status = QZipReader::FileError;
            return;
        }

        FileInfo info;
        info.compressionMethod = readUShort(hdr + 10);
        info.lastModifiedTime = readUShort(hdr + 12);
        info.lastModifiedDate = readUShort(hdr + 14);
        info.crc = readUInt(hdr + 16);
        info.size = readUInt(hdr + 24);
        quint16 fileNameLength = readUShort(hdr + 28);
        quint16 extraFieldLength = readUShort(hdr + 30);
        quint16 commentLength = readUShort(hdr + 32);
        info.offset = readUInt(hdr + 42);

        QByteArray fileName(fileNameLength, Qt::Uninitialized);
        if (device->read(fileName.data(), fileNameLength) != fileNameLength) {
            status = QZipReader::FileError;
            return;
        }
        info.filePath = QString::fromUtf8(fileName.constData(), fileNameLength);

        device->seek(device->pos() + extraFieldLength + commentLength);

        info.isDir = info.filePath.endsWith(QLatin1Char('/'));
        info.isFile = !info.isDir;

        // detect symlink via external attributes
        quint32 externalAttr = readUInt(hdr + 38);
        quint32 unixMode = (externalAttr >> 16) & 0xFFFF;
        if ((unixMode & 0xF000) == 0xA000) {
            info.isSymLink = true;
            info.isFile = false;
        }

        fileList.append(info);
    }
}

QZipReader::QZipReader(const QString& fileName, QIODevice::OpenMode mode)
    : d_ptr(new QZipReaderPrivate(new QFile(fileName), true))
{
    Q_UNUSED(mode)
}

QZipReader::QZipReader(QIODevice* device)
    : d_ptr(new QZipReaderPrivate(device, false))
{
}

QZipReader::~QZipReader()
{
    delete d_ptr;
}

QIODevice* QZipReader::device() const
{
    return d_ptr->device;
}

bool QZipReader::isReadable() const
{
    return d_ptr->device->isReadable();
}

bool QZipReader::exists() const
{
    QFile* f = qobject_cast<QFile*> (d_ptr->device);
    if (f)
        return f->exists();
    return true;
}

QList<QZipReader::FileInfo> QZipReader::fileInfoList() const
{
    d_ptr->scanFiles();
    QList<QZipReader::FileInfo> files;
    for (const auto& info : d_ptr->fileList) {
        FileInfo fi;
        fi.isDir = info.isDir;
        fi.isFile = info.isFile;
        fi.isSymLink = info.isSymLink;
        fi.size = info.size;
        fi.crc = info.crc;
        fi.filePath = info.filePath;
        QDateTime dt;
        uchar dateTime[4];
        copyUShort(dateTime, info.lastModifiedTime);
        copyUShort(dateTime + 2, info.lastModifiedDate);
        readMSDosDate(dateTime, dt);
        fi.lastModified = dt;
        files.append(fi);
    }
    return files;
}

int QZipReader::count() const
{
    d_ptr->scanFiles();
    return d_ptr->fileList.count();
}

QZipReader::FileInfo QZipReader::entryInfoAt(int index) const
{
    d_ptr->scanFiles();
    if (index < 0 || index >= d_ptr->fileList.size())
        return FileInfo();
    const auto& info = d_ptr->fileList.at(index);
    FileInfo fi;
    fi.isDir = info.isDir;
    fi.isFile = info.isFile;
    fi.isSymLink = info.isSymLink;
    fi.size = info.size;
    fi.crc = info.crc;
    fi.filePath = info.filePath;
    QDateTime dt;
    uchar dateTime[4];
    copyUShort(dateTime, info.lastModifiedTime);
    copyUShort(dateTime + 2, info.lastModifiedDate);
    readMSDosDate(dateTime, dt);
    fi.lastModified = dt;
    return fi;
}

QByteArray QZipReader::fileData(const QString& fileName) const
{
    d_ptr->scanFiles();
    for (const auto& info : d_ptr->fileList) {
        if (info.filePath == fileName) {
            d_ptr->device->seek(info.offset);
            uchar hdr[30];
            if (d_ptr->device->read((char*)hdr, 30) != 30)
                return QByteArray();
            quint16 fileNameLength = readUShort(hdr + 26);
            quint16 extraFieldLength = readUShort(hdr + 28);
            d_ptr->device->seek(d_ptr->device->pos() + fileNameLength + extraFieldLength);
            QByteArray data(info.size, Qt::Uninitialized);
            if (d_ptr->device->read(data.data(), info.size) != info.size)
                return QByteArray();
            if (info.compressionMethod == 0) {
                return data;
            }
            // For compressed data, you would need zlib here.
            // This stripped version assumes stored (uncompressed) entries only.
            return QByteArray();
        }
    }
    return QByteArray();
}

bool QZipReader::extractAll(const QString& destinationDir) const
{
    QDir baseDir(destinationDir);
    if (!baseDir.exists() && !baseDir.mkpath(QLatin1String(".")))
        return false;

    d_ptr->scanFiles();
    for (const auto& info : d_ptr->fileList) {
        QString path = info.filePath;
        // security check
        if (path.contains(QLatin1String("..")))
            return false;

        if (info.isDir) {
            if (!baseDir.mkpath(path))
                return false;
            continue;
        }

        QFile f(baseDir.filePath(path));
        if (!f.open(QIODevice::WriteOnly))
            return false;
        QByteArray data = fileData(path);
        if (f.write(data) != data.size())
            return false;
    }
    return true;
}

QZipReader::Status QZipReader::status() const
{
    d_ptr->scanFiles();
    return d_ptr->status;
}



#endif // QT_NO_TEXTODFWRITER