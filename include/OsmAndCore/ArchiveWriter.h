#ifndef _OSMAND_CORE_ARCHIVE_READER_H_
#define _OSMAND_CORE_ARCHIVE_READER_H_

#include <OsmAndCore/stdlib_common.h>

#include <OsmAndCore/QtExtensions.h>

#include <QString>
#include <QList>
#include <QDateTime>

#include <OsmAndCore.h>
#include <OsmAndCore/PrivateImplementation.h>

namespace OsmAnd
{
    class ArchiveWriter_P;
    class OSMAND_CORE_API ArchiveWriter
    {
        Q_DISABLE_COPY_AND_MOVE(ArchiveWriter);
    public:
    private:
        PrivateImplementation<ArchiveWriter_P> _p;
    protected:
    public:
        ArchiveWriter();
        virtual ~ArchiveWriter();

        void createArchive(bool* const ok_, const QString& filePath, const QList<QString>& filesToArcive, const QString& basePath);
    };
}

#endif // !defined(_OSMAND_CORE_ARCHIVE_READER_H_)
