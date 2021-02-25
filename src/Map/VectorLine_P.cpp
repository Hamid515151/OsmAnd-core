#include "VectorLine_P.h"
#include "VectorLine.h"
#include "Utilities.h"

#include "ignore_warnings_on_external_includes.h"
#include "restore_internal_warnings.h"

#include <SkBitmap.h>
#include <SkPathMeasure.h>

#include "MapSymbol.h"
#include "MapSymbolsGroup.h"
#include "VectorMapSymbol.h"
#include "OnSurfaceVectorMapSymbol.h"
#include "OnSurfaceRasterMapSymbol.h"
#include "QKeyValueIterator.h"
#include "Logging.h"
#include "IAtlasMapRenderer.h"

OsmAnd::VectorLine_P::VectorLine_P(VectorLine* const owner_)
: _hasUnappliedChanges(false), _hasUnappliedPrimitiveChanges(false), _isHidden(false),
  _metersPerPixel(1.0), _mapZoomLevel(InvalidZoomLevel), _mapVisualZoom(0.f), _mapVisualZoomShift(0.f),
  owner(owner_)
{
}

OsmAnd::VectorLine_P::~VectorLine_P()
{
}

bool OsmAnd::VectorLine_P::isHidden() const
{
    QReadLocker scopedLocker(&_lock);
    
    return _isHidden;
}

void OsmAnd::VectorLine_P::setIsHidden(const bool hidden)
{
    QWriteLocker scopedLocker(&_lock);
    
    _isHidden = hidden;
    _hasUnappliedChanges = true;
}

QVector<OsmAnd::PointI> OsmAnd::VectorLine_P::getPoints() const
{
    QReadLocker scopedLocker(&_lock);
    
    return _points;
}

void OsmAnd::VectorLine_P::setPoints(const QVector<PointI>& points)
{
    QWriteLocker scopedLocker(&_lock);
    
    _points = points;
    
    _hasUnappliedPrimitiveChanges = true;
    _hasUnappliedChanges = true;
}

OsmAnd::FColorARGB OsmAnd::VectorLine_P::getFillColor() const
{
    QReadLocker scopedLocker(&_lock);
    
    return _fillColor;
}

void OsmAnd::VectorLine_P::setFillColor(const FColorARGB color)
{
    QWriteLocker scopedLocker(&_lock);
    
    _fillColor = color;
    
    _hasUnappliedPrimitiveChanges = true;
    _hasUnappliedChanges = true;
}

bool OsmAnd::VectorLine_P::hasUnappliedChanges() const
{
    QReadLocker scopedLocker(&_lock);
    
    return _hasUnappliedChanges;
}

bool OsmAnd::VectorLine_P::hasUnappliedPrimitiveChanges() const
{
    QReadLocker scopedLocker(&_lock);
    
    return _hasUnappliedPrimitiveChanges;
}

bool OsmAnd::VectorLine_P::isMapStateChanged(const MapState& mapState) const
{
    bool changed = qAbs(_mapZoomLevel + _mapVisualZoom - mapState.zoomLevel - mapState.visualZoom) > 0.5;
    //_mapZoomLevel != mapState.zoomLevel ||
    //_mapVisualZoom != mapState.visualZoom ||
    //_mapVisualZoomShift != mapState.visualZoomShift;
    return changed;
}

void OsmAnd::VectorLine_P::applyMapState(const MapState& mapState)
{
    _metersPerPixel = mapState.metersPerPixel;
    _mapZoomLevel = mapState.zoomLevel;
    _mapVisualZoom = mapState.visualZoom;
    _mapVisualZoomShift = mapState.visualZoomShift;
}

bool OsmAnd::VectorLine_P::update(const MapState& mapState)
{
    QWriteLocker scopedLocker(&_lock);

    bool mapStateChanged = isMapStateChanged(mapState);
    if (mapStateChanged)
        applyMapState(mapState);

    _hasUnappliedPrimitiveChanges |= mapStateChanged;
    return mapStateChanged;
}

bool OsmAnd::VectorLine_P::applyChanges()
{
    QReadLocker scopedLocker1(&_lock);
    
    if (!_hasUnappliedChanges && !_hasUnappliedPrimitiveChanges)
        return false;
    
    QReadLocker scopedLocker2(&_symbolsGroupsRegistryLock);
    for (const auto& symbolGroup_ : constOf(_symbolsGroupsRegistry))
    {
        const auto symbolGroup = symbolGroup_.lock();
        if (!symbolGroup)
            continue;
        
        const auto& vectorLineSymbolGroup = std::dynamic_pointer_cast<VectorLine::SymbolsGroup>(symbolGroup);
        bool needUpdatePrimitive = _hasUnappliedPrimitiveChanges && _points.size() > 1;
        for (const auto& symbol_ : constOf(symbolGroup->symbols))
        {
            symbol_->isHidden = _isHidden;
            
            if (const auto symbol = std::dynamic_pointer_cast<OnSurfaceVectorMapSymbol>(symbol_))
            {
                if (needUpdatePrimitive)
                    generatePrimitive(symbol);
            }
        }
    }

    _hasUnappliedChanges = false;
    _hasUnappliedPrimitiveChanges = false;
    
    return true;
}

std::shared_ptr<OsmAnd::VectorLine::SymbolsGroup> OsmAnd::VectorLine_P::inflateSymbolsGroup() const
{
    QReadLocker scopedLocker(&_lock);
    
    // Construct new map symbols group for this marker
    const std::shared_ptr<VectorLine::SymbolsGroup> symbolsGroup(new VectorLine::SymbolsGroup(std::const_pointer_cast<VectorLine_P>(shared_from_this())));
    symbolsGroup->presentationMode |= MapSymbolsGroup::PresentationModeFlag::ShowAllOrNothing;
    
    if (_points.size() > 1)
    {
        const auto& vectorLine = std::make_shared<OnSurfaceVectorMapSymbol>(symbolsGroup);
        generatePrimitive(vectorLine);
        vectorLine->allowFastCheckByFrustum = false;
        symbolsGroup->symbols.push_back(vectorLine);
        
        // TODO: needs to be updatable
        //generateArrowsOnPath(symbolsGroup);
    }
    
    return symbolsGroup;
}

std::shared_ptr<OsmAnd::VectorLine::SymbolsGroup> OsmAnd::VectorLine_P::createSymbolsGroup(const MapState& mapState)
{
    applyMapState(mapState);
    
    const auto inflatedSymbolsGroup = inflateSymbolsGroup();
    registerSymbolsGroup(inflatedSymbolsGroup);
    return inflatedSymbolsGroup;
}

void OsmAnd::VectorLine_P::registerSymbolsGroup(const std::shared_ptr<MapSymbolsGroup>& symbolsGroup) const
{
    QWriteLocker scopedLocker(&_symbolsGroupsRegistryLock);
    
    _symbolsGroupsRegistry.insert(symbolsGroup.get(), symbolsGroup);
}

void OsmAnd::VectorLine_P::unregisterSymbolsGroup(MapSymbolsGroup* const symbolsGroup) const
{
    QWriteLocker scopedLocker(&_symbolsGroupsRegistryLock);
    
    _symbolsGroupsRegistry.remove(symbolsGroup);
}

OsmAnd::PointD OsmAnd::VectorLine_P::findLineIntersection(PointD p1, OsmAnd::PointD p2, OsmAnd::PointD p3, OsmAnd::PointD p4) const
{
    double d = (p1.x - p2.x) * (p3.y - p4.y) - (p1.y - p2.y) * (p3.x - p4.x);
    //double atn1 = atan2(p1.x - p2.x, p1.y - p2.y);
    //double atn2 = atan2(p3.x - p4.x, p3.y - p4.y);
    //double df = qAbs(atn1 - atn2);
    // printf("\n %f %f d=%f df=%f df-PI=%f df-2PI=%f", atn1, atn2, d, df, df - M_PI, df - 2 * M_PI);
    //double THRESHOLD = M_PI / 6;
    if(d == 0 // || df < THRESHOLD  || qAbs(df - M_PI) < THRESHOLD || qAbs(df - 2 * M_PI) < THRESHOLD
       ) {
        // in case of lines connecting p2 == p3
        return p2;
    }
    OsmAnd::PointD r;
    r.x = ((p1.x* p2.y-p1.y*p2.x)*(p3.x - p4.x) - (p3.x* p4.y-p3.y*p4.x)*(p1.x - p2.x)) / d;
    r.y = ((p1.x* p2.y-p1.y*p2.x)*(p3.y - p4.y) - (p3.x* p4.y-p3.y*p4.x)*(p1.y - p2.y)) / d;
    
    
    return r;
}

OsmAnd::PointD OsmAnd::VectorLine_P::getProjection(PointD point, PointD from, PointD to ) const
{

    double mDist = (from.x - to.x) * (from.x - to.x) + (from.y - to.y) * (from.y - to.y);
    double projection = scalarMultiplication(from.x, from.y, to.x, to.y, point.x, point.y);
    if (projection < 0)
    {
        return from;
    }
    else if (projection >= mDist)
    {
        return to;
    }
    return PointD(from.x + (to.x - from.x) * (projection / mDist),
                  from.y + (to.y - from.y) * (projection / mDist));
}

double OsmAnd::VectorLine_P::scalarMultiplication(double xA, double yA, double xB, double yB, double xC, double yC) const
{
    // Scalar multiplication between (AB, AC)
    return (xB - xA) * (xC - xA) + (yB - yA) * (yC - yA);
}

int OsmAnd::VectorLine_P::simplifyDouglasPeucker(std::vector<PointD>& points, uint start, uint end, double epsilon, std::vector<bool>& include) const
{
    double dmax = -1;
    int index = -1;
    for (int i = start + 1; i <= end - 1; i++) {
        PointD proj = getProjection(points[i],points[start], points[end]);
        double d = qSqrt((points[i].x-proj.x)*(points[i].x-proj.x)+
                         (points[i].y-proj.y)*(points[i].y-proj.y));
        // calculate distance from line
        if (d > dmax) {
            dmax = d;
            index = i;
        }
    }
    if (dmax >= epsilon) {
        int enabled1 = simplifyDouglasPeucker(points, start, index, epsilon, include);
        int enabled2 = simplifyDouglasPeucker(points, index, end, epsilon, include);
        return enabled1 + enabled2 ;
    } else {
        include[end] = true;
        return 1;
    }
}

float OsmAnd::VectorLine_P::zoom() const
{
    return _mapZoomLevel + (_mapVisualZoom >= 1.0f ? _mapVisualZoom - 1.0f : (_mapVisualZoom - 1.0f) * 2.0f);
}

QVector<OsmAnd::PointD> OsmAnd::VectorLine_P::splitLine(std::vector<PointD>& points, std::vector<bool>& include) const
{
    
    uint intervalLength = 500;
    QVector<PointD> output;
    QVector<PointD> simplifiedPoints;
    for (auto pointIdx = 0u; pointIdx < points.size(); pointIdx++)
    {
        if(!include[pointIdx])
        {
            continue;
        }
        simplifiedPoints.push_back(points[pointIdx]);
    }
    
    PointD end = simplifiedPoints[simplifiedPoints.count() - 1];
    PointD start = simplifiedPoints[0];
    
    
    PointD prevPnt = start;
    uint residualSegLength = 0;
    for (uint i = 1; i < simplifiedPoints.count(); i++)
    {
        PointD pnt = simplifiedPoints[i];
        // for each line of the dataframe calculate segment length
        double segLength = sqrt(pow((prevPnt.x - pnt.x), 2) + pow((prevPnt.y - pnt.y), 2));
        
        // create a vector of direction for the segment
        QPair<double, double> v(pnt.x - prevPnt.x, pnt.y - prevPnt.y);
        
        // unit length
        QPair<double, double> u(v.first / sqrt(pow(v.first, 2) + pow(v.second, 2)), v.second / sqrt(pow(v.first, 2) + pow(v.second, 2)));
        
        // calculate number of segment the segment is split into
        uint numSeg = floor((segLength - residualSegLength)  /  intervalLength);
        
        // skip if next vertex is before interval_length
        if(numSeg >= 0)
        {
            // add interpolated segments
            for (uint times = 0; times < numSeg; times++)
            {
                output.append(PointD(prevPnt.x + u.first * residualSegLength + u.first * intervalLength * times,
                                     prevPnt.y + u.second * residualSegLength + u.second * intervalLength * times));
            }
            output.append(pnt);
        }
        else
        {
            output.append(pnt);
            prevPnt = pnt;
            residualSegLength -= segLength;
            continue;
            
        }

        prevPnt = pnt;
        // calculate residual segment length
        residualSegLength = intervalLength - ((segLength - residualSegLength) - (numSeg  *  intervalLength));
    }
    output.append(end);
    return output;
}

//resample_polyline  = function(polyline, interval_length = 20, add_original_points = TRUE, add_final_point = FALSE) {
//
//  # The function splits a polyline into segments of a given length.
//  # polyline: a spatial polyline data frame
//  # interval_length: the length of the segments to split the lines into, in units of the polyline coordinates
//  # add_original_points: whether or not the original points of the polyline should be added to the resulting line
//  #                      if set FALSE, the resulting line will be shorter
//  # add_final_point: whether or not the final point of the polyline should be added to the resulting line
//
//  # transform input polyline
//  linedf = data.frame(
//    x  = polyline$x[1:nrow(polyline)-1],
//    y  = polyline$y[1:nrow(polyline)-1],
//    x2 = polyline$x[2:nrow(polyline)],
//    y2 = polyline$y[2:nrow(polyline)]
//  )
//
//  # prepare output
//  df = data.frame(
//    x  = numeric(),
//    y  = numeric()
//  )
//
//  residual_seg_length = 0
//  for (i in 1:nrow(linedf)) {
//
//    # for each line of the dataframe calculate segment length
//    v_seg      = linedf[i, ]
//    seg_length = sqrt((v_seg$x - v_seg$x2) ^ 2 + (v_seg$y - v_seg$y2) ^ 2)
//
//    # create a vector of direction for the segment
//    v = c(v_seg$x2 - v_seg$x, v_seg$y2 - v_seg$y)
//
//    # unit length
//    u = c(v[1]  /  sqrt(v[1]  ^  2 + v[2]  ^  2), v[2]  /  sqrt(v[1]  ^  2 + v[2]  ^ 2))
//
//    # calculate number of segment the segment is split into
//    num_seg = floor((seg_length - residual_seg_length)  /  interval_length)
//
//    # skip if next vertex is before interval_length
//    if(num_seg >= 0) {
//
//      # add interpolated segments
//      for (i in 0:(num_seg)) {
//        df[nrow(df) + 1,] = c(
//          v_seg$x  +  u[1] * residual_seg_length  +  u[1]  *  interval_length  *  i ,
//          v_seg$y  +  u[2] * residual_seg_length  +  u[2]  *  interval_length  *  i
//        )
//      }
//
//      # add original point (optional)
//      if(add_original_points){
//        df[nrow(df) + 1,] = c(
//          v_seg$x2,
//          v_seg$y2
//        )
//      }
//
//    } else {
//
//      # add original point (optional)
//      if(add_original_points){
//        df[nrow(df) + 1,] = c(
//          v_seg$x2,
//          v_seg$y2
//        )
//      }
//
//      residual_seg_length = residual_seg_length - seg_length
//      next()
//
//    }
//
//    # calculate residual segment length
//    residual_seg_length = interval_length - ((seg_length - residual_seg_length) - (num_seg  *  interval_length))
//
//  }
//
//  # add final point (optional)
//  if(add_final_point){
//    df = rbind(df, data.frame(
//      x = tail(polyline$x, n=1),
//      y = tail(polyline$y, n=1)
//    ))
//  }
//
//  return(df)
//
//}

std::shared_ptr<OsmAnd::OnSurfaceVectorMapSymbol> OsmAnd::VectorLine_P::generatePrimitive(const std::shared_ptr<OnSurfaceVectorMapSymbol> vectorLine) const
{
    int order = owner->baseOrder;
    int pointsCount = _points.size();
    
    vectorLine->order = order++;
    vectorLine->position31 = _points[0];
    vectorLine->primitiveType = VectorMapSymbol::PrimitiveType::TriangleStrip;

    const auto verticesAndIndexes = std::make_shared<VectorMapSymbol::VerticesAndIndexes>();
    // Line has no reusable vertices - TODO clarify
    verticesAndIndexes->indices = nullptr;
    verticesAndIndexes->indicesCount = 0;
    
    vectorLine->scaleType = VectorMapSymbol::ScaleType::In31;
    vectorLine->scale = 1.0;
    vectorLine->direction = 0.f;
    
    float zoom = this->zoom();
    double radius = owner->lineWidth * Utilities::getPowZoom( 31 - _mapZoomLevel) * qSqrt(zoom) /
                        (IAtlasMapRenderer::TileSize3D * IAtlasMapRenderer::TileSize3D);
    // generate array of points
    std::vector<OsmAnd::PointD> pointsToPlot(pointsCount);
    for (auto pointIdx = 0u; pointIdx < pointsCount; pointIdx++)
    {
        pointsToPlot[pointIdx] = PointD((_points[pointIdx].x-_points[0].x), (_points[pointIdx].y-_points[0].y));
    }
    
    std::vector<bool> include(pointsCount, false);
    include[0] = true;
    int pointsSimpleCount = simplifyDouglasPeucker(pointsToPlot, 0, (uint) pointsToPlot.size() - 1, radius / 3, include) + 1;
    pointsSimpleCount *= 2;
    
    const auto segments = splitLine(pointsToPlot, include);
    uint splitPointsCount = segments.count();
    
    // generate base points for connecting lines with triangles
    std::vector<OsmAnd::PointD> b1(splitPointsCount), b2(splitPointsCount), e1(splitPointsCount), e2(splitPointsCount), original(splitPointsCount);
    double ntan = 0, nx1 = 0, ny1 = 0;
    uint prevPointIdx = 0;
    uint insertIdx = 0;
    for (auto pointIdx = 0u; pointIdx < splitPointsCount; pointIdx++)
    {
//        if(!include[pointIdx])
//        {
//            continue;
//        }
        PointD pnt = segments[pointIdx];
        PointD prevPnt = segments[prevPointIdx];
        original[insertIdx]=pnt;
        if(pointIdx > 0)
        {
            ntan = atan2(segments[pointIdx].x - segments[prevPointIdx].x,
                            segments[pointIdx].y - segments[prevPointIdx].y);
            nx1 = radius * sin(M_PI_2 - ntan) ;
            ny1 = radius * cos(M_PI_2 - ntan) ;
            e1[insertIdx] = b1[insertIdx] = OsmAnd::PointD(pnt.x - nx1, pnt.y + ny1);
            e2[insertIdx] = b2[insertIdx] = OsmAnd::PointD(pnt.x + nx1, pnt.y - ny1);
            e1[insertIdx-1] = OsmAnd::PointD(prevPnt.x - nx1, prevPnt.y + ny1);
            e2[insertIdx-1] = OsmAnd::PointD(prevPnt.x + nx1, prevPnt.y - ny1);
        } else {
            b2[insertIdx] = b1[insertIdx] = pnt;
        }
        prevPointIdx = pointIdx;
        insertIdx++;
    }
    
    verticesAndIndexes->position31 = new PointI(vectorLine->position31.x, vectorLine->position31.y);
    
    std::vector<VectorMapSymbol::Vertex> vertices;
    VectorMapSymbol::Vertex vertex;
    VectorMapSymbol::Vertex* pVertex = &vertex;
    
    //OsmAnd::LogPrintf(OsmAnd::LogSeverityLevel::Info, "=== pointsCount=%d zoom=%d visualZoom=%f metersPerPixel=%f radius=%f simpleCount=%d cnt=%d", verticesAndIndexes->verticesCount,
    //  _mapZoomLevel, _mapVisualZoom, _metersPerPixel, radius, pointsSimpleCount,pointsCount);
    bool direction = true;
    
    // generate triangles
    for (auto pointIdx = 0u; pointIdx < pointsSimpleCount; pointIdx++)
    {
        FColorARGB fillColor = _fillColor;
//        fillColor.a = pointIdx % 1 == 0 ? 1.0 : 0.0;
        if (pointIdx == 0)
        {
            pVertex->positionXY[0] = e2[pointIdx].x;
            pVertex->positionXY[1] = e2[pointIdx].y;
            pVertex->color = fillColor;
            vertices.push_back(vertex);
            
            pVertex->positionXY[0] = e1[pointIdx].x;
            pVertex->positionXY[1] = e1[pointIdx].y;
            pVertex->color = fillColor;
            vertices.push_back(vertex);
            
            direction = true;
        }
        else if (pointIdx == pointsSimpleCount - 1)
        {
            if(!direction) {
                pVertex->positionXY[0] = b1[pointIdx].x;
                pVertex->positionXY[1] = b1[pointIdx].y;
                pVertex->color = fillColor;
                vertices.push_back(vertex);
            }
            
            pVertex->positionXY[0] = b2[pointIdx].x;
            pVertex->positionXY[1] = b2[pointIdx].y;
            pVertex->color = fillColor;
            vertices.push_back(vertex);
            if(direction) {
                pVertex->positionXY[0] = b1[pointIdx].x;
                pVertex->positionXY[1] = b1[pointIdx].y;
                pVertex->color = fillColor;
                vertices.push_back(vertex);
            }
        }
        else
        {
            int smoothLevel = 1; // could be constant & could be dynamic depends on the angle
            PointD l1 = findLineIntersection(e1[pointIdx-1], b1[pointIdx], e1[pointIdx], b1[pointIdx+1]);
            PointD l2 = findLineIntersection(e2[pointIdx-1], b2[pointIdx], e2[pointIdx], b2[pointIdx+1]);
            bool l1Intersects = (l1.x >= qMin(e1[pointIdx-1].x,b1[pointIdx].x)
                                  && l1.x <= qMax(e1[pointIdx-1].x,b1[pointIdx].x)) ||
                                (l1.x >= qMin(e1[pointIdx].x,b1[pointIdx+1].x)
                                  && l1.x <= qMax(e1[pointIdx].x,b1[pointIdx+1].x));
            bool l2Intersects = (l2.x >= qMin(e2[pointIdx-1].x,b2[pointIdx].x)
                                 && l2.x <= qMax(e2[pointIdx-1].x,b2[pointIdx].x)) ||
                                (l2.x >= qMin(e2[pointIdx].x,b2[pointIdx+1].x)
                                 && l2.x <= qMax(e2[pointIdx].x,b2[pointIdx+1].x));
            
            // bewel - connecting only 3 points (one excluded depends on angle)
            // miter - connecting 4 points
            // round - generating between 2-3 point triangles (in place of triangle different between bewel/miter)
            
            if(!l2Intersects && !l1Intersects) {
                // skip point
            } else {
                bool startDirection = l2Intersects;
                const PointD& lp = startDirection ? l2 : l1;
                const std::vector<OsmAnd::PointD>& bp = startDirection ? b1 : b2;
                const std::vector<OsmAnd::PointD>& ep = startDirection ? e1 : e2;
                int phase = direction == startDirection ? 0 : 2;
                if(phase % 3 == 0) {
                    pVertex->positionXY[0] = lp.x;
                    pVertex->positionXY[1] = lp.y;
                    pVertex->color = fillColor;
                    vertices.push_back(vertex);
                    phase++;
                }
                
                pVertex->positionXY[0] = bp[pointIdx].x;
                pVertex->positionXY[1] = bp[pointIdx].y;
                pVertex->color = fillColor;
                vertices.push_back(vertex);
                phase++;
                if(phase % 3 == 0) {
                    pVertex->positionXY[0] = lp.x;
                    pVertex->positionXY[1] = lp.y;
                    pVertex->color = fillColor;
                    vertices.push_back(vertex);
                    phase++;
                }
                
                
                if(smoothLevel > 0) {
                    double dv = 1.0 / (1 << smoothLevel);
                    double nt = dv;
                    while(nt < 1) {
                        double rx = bp[pointIdx].x*nt + ep[pointIdx].x*(1-nt);
                        double ry = bp[pointIdx].y*nt + ep[pointIdx].y*(1-nt);
                        double ld = (rx-original[pointIdx].x)*(rx-original[pointIdx].x) +
                                    (ry-original[pointIdx].y)*(ry-original[pointIdx].y);
                        pVertex->positionXY[0] = original[pointIdx].x + radius/sqrt(ld)*(rx-original[pointIdx].x);
                        pVertex->positionXY[1] = original[pointIdx].y + radius/sqrt(ld)*(ry-original[pointIdx].y);
                        pVertex->color = fillColor;
                        vertices.push_back(vertex);
                        phase++;
                        if(phase % 3 == 0) {
                            // avoid overlap
                            vertices.push_back(vertex);
                            
                            pVertex->positionXY[0] = lp.x;
                            pVertex->positionXY[1] = lp.y;
                            pVertex->color = fillColor;
                            vertices.push_back(vertex);
                            phase++;
                        }
                        nt += dv;
                    }
                }
                
                pVertex->positionXY[0] = ep[pointIdx].x;
                pVertex->positionXY[1] = ep[pointIdx].y;
                pVertex->color = fillColor;
                vertices.push_back(vertex);
                phase++;
                if(phase % 3 == 0) {
                    // avoid overlap
                    vertices.push_back(vertex);
                    
                    pVertex->positionXY[0] = lp.x;
                    pVertex->positionXY[1] = lp.y;
                    pVertex->color = fillColor;
                    vertices.push_back(vertex);
                    phase++;
                }
                if(smoothLevel > 0) {
                    //direction = direction;
                } else {
                    direction = !direction;
                }
            }
        }
    }
    
    //verticesAndIndexes->verticesCount = (pointsSimpleCount - 2) * 2 + 2 * 2;
    verticesAndIndexes->verticesCount = vertices.size();
    verticesAndIndexes->vertices = new VectorMapSymbol::Vertex[vertices.size()];
    std::copy(vertices.begin(), vertices.end(), verticesAndIndexes->vertices);

    vectorLine->isHidden = _isHidden;
    
    vectorLine->setVerticesAndIndexes(verticesAndIndexes);
    
    return vectorLine;
}

void OsmAnd::VectorLine_P::generateArrowsOnPath(const std::shared_ptr<VectorLine::SymbolsGroup> symbolsGroup) const
{
    if (owner->pathIconStep > 0 && owner->pathIcon)
    {
        SkPath path;
        const auto& start = _points[0];
        path.moveTo(start.x, start.y);
        for (int i = 1; i < _points.size(); i++)
        {
            const auto& p = _points[i];
            path.lineTo(p.x, p.y);
        }
        SkPathMeasure pathMeasure(path, false);
        
        bool ok = false;
        const auto length = pathMeasure.getLength();
        
        float step = Utilities::metersToX31(owner->pathIconStep * _metersPerPixel);
        auto iconOffset = 0.5f * step;
        const auto iconInstancesCount = static_cast<int>((length - iconOffset) / step) + 1;
        if (iconInstancesCount > 0)
        {
            int surfOrder = owner->baseOrder - 1;
            // Set of OnSurfaceMapSymbol from onMapSurfaceIcons
            const auto& onMapSurfaceIcon = owner->pathIcon;
            
            std::shared_ptr<SkBitmap> iconClone(new SkBitmap());
            ok = onMapSurfaceIcon->deepCopyTo(iconClone.get());
            assert(ok);
            
            for (auto iconInstanceIdx = 0; iconInstanceIdx < iconInstancesCount; iconInstanceIdx++, iconOffset += step)
            {
                SkPoint p;
                SkVector t;
                ok = pathMeasure.getPosTan(iconOffset, &p, &t);
                if (!ok)
                    break;
                
                // Get direction
                float direction = Utilities::normalizedAngleDegrees(qRadiansToDegrees(atan2(-t.x(), t.y())));
                const auto arrowSymbol = std::make_shared<OnSurfaceRasterMapSymbol>(symbolsGroup);
                
                arrowSymbol->order = surfOrder;
                
                arrowSymbol->bitmap = iconClone;
                arrowSymbol->size = PointI(iconClone->width(), iconClone->height());
                arrowSymbol->content = QString().sprintf(
                                                         "markerGroup(%p:%p)->onMapSurfaceIconBitmap:%p",
                                                         this,
                                                         symbolsGroup.get(),
                                                         iconClone->getPixels());
                arrowSymbol->languageId = LanguageId::Invariant;
                arrowSymbol->position31 = PointI(p.x(), p.y());
                arrowSymbol->direction = direction;
                arrowSymbol->isHidden = _isHidden;
                symbolsGroup->symbols.push_back(arrowSymbol);
            }
        }
    }
}

