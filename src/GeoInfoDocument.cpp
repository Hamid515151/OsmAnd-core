#include "GeoInfoDocument.h"

OsmAnd::GeoInfoDocument::GeoInfoDocument()
{
}

OsmAnd::GeoInfoDocument::~GeoInfoDocument()
{
}

OsmAnd::GeoInfoDocument::ExtraData::ExtraData()
{
}

OsmAnd::GeoInfoDocument::ExtraData::~ExtraData()
{
}

OsmAnd::GeoInfoDocument::Link::Link()
{
}

OsmAnd::GeoInfoDocument::Link::~Link()
{
}

OsmAnd::GeoInfoDocument::Metadata::Metadata()
{
}

OsmAnd::GeoInfoDocument::Metadata::~Metadata()
{
}

OsmAnd::GeoInfoDocument::Author::Author()
{
}

OsmAnd::GeoInfoDocument::Author::~Author()
{
}

OsmAnd::GeoInfoDocument::Copyright::Copyright()
{
}

OsmAnd::GeoInfoDocument::Copyright::~Copyright()
{
}

OsmAnd::GeoInfoDocument::Bounds::Bounds()
    : minlat(std::numeric_limits<double>::quiet_NaN())
    , minlon(std::numeric_limits<double>::quiet_NaN())
    , maxlat(std::numeric_limits<double>::quiet_NaN())
    , maxlon(std::numeric_limits<double>::quiet_NaN())
{
}

OsmAnd::GeoInfoDocument::Bounds::~Bounds()
{
}

OsmAnd::GeoInfoDocument::LocationMark::LocationMark()
    : position(LatLon(std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()))
    , elevation(std::numeric_limits<double>::quiet_NaN())
    , timestamp(QDateTime())
{
}

OsmAnd::GeoInfoDocument::LocationMark::~LocationMark()
{
}

OsmAnd::GeoInfoDocument::TrackSegment::TrackSegment()
{
}

OsmAnd::GeoInfoDocument::TrackSegment::~TrackSegment()
{
}

OsmAnd::GeoInfoDocument::Track::Track()
{
}

OsmAnd::GeoInfoDocument::Track::~Track()
{
}

OsmAnd::GeoInfoDocument::Route::Route()
{
}

OsmAnd::GeoInfoDocument::Route::~Route()
{
}

bool OsmAnd::GeoInfoDocument::hasRtePt() const
{
    for (auto& r : routes)
        if (r->points.size() > 0)
            return true;

    return false;
}

bool OsmAnd::GeoInfoDocument::hasWptPt() const
{
    return locationMarks.size() > 0;
}

bool OsmAnd::GeoInfoDocument::hasTrkPt() const
{
    for (auto& t : tracks)
        for (auto& ts : t->segments)
            if (ts->points.size() > 0)
                return true;

    return false;
}
