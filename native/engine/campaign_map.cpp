#include "engine/campaign_map.h"
#include "engine/campaign_runtime.h"
#include "engine/scene_images.h"
#include <QJsonArray>
#include <QHash>
#include <QLineF>
#include <cmath>
namespace lordly {
namespace {
constexpr int nodeOffset = 0x864a0, roadOffset = 0x858e4;
void nodeId(int id) {
    if (id < 1 || id > CampaignMap::nodeCount)
        throw Error("Invalid map location");
}
void roadId(int id) {
    if (id < 1 || id > CampaignMap::roadCapacity)
        throw Error("Invalid world-map road");
}
} // namespace
CampaignMap::CampaignMap(const Rom &rom) {
    StoryRoutes routes(rom);
    nodes_ = original_ = rom.bytes().mid(nodeOffset, nodeCount * 8);
    roads_ = originalRoads_ = rom.bytes().mid(roadOffset, roadCount * 2);
    originalDots_ = rom.bytes().mid(0x86040, roadCount * 28);
    icons_=QByteArray(nodeCount,0);
    for(const auto value:rom.bytes().mid(0x871a8,10)) {
        const int id=quint8(value);
        if(id>=1 && id<=nodeCount)icons_[id-1]=1;
    }
    if (const auto placement = CampaignPlacement::read(rom.bytes())) {
        roads_ = originalRoads_ = placement->roads;
        originalDots_ = placement->dots;
        originalBendCapacity_ = placement->bendCapacity;
        originalDisplayDots_ = placement->displayDots;
        if(!placement->locationIcons.isEmpty())for(int id=1;id<=nodeCount;++id)
            icons_[id-1]=char((quint8(placement->locationIcons[(id-1)/8])>>((id-1)%8))&1);
        for(auto it=placement->locationNames.begin();it!=placement->locationNames.end();++it)
            ownedNamePointers_[u32(original_,(it.key()-1)*8+4)]=it.value();
    }
    auto resource = [&](int relative) {
        const int at = 0x594280 + relative;
        return decodeLzss(rom.bytes().mid(at + 8, u32(rom.bytes(), at)), u32(rom.bytes(), at + 4));
    };
    mapArtwork_ = resource(0x1d91cf6);
    markers_ = resource(0x1dd2486);
    initializeArtwork();
    if(const auto placement=CampaignPlacement::read(rom.bytes());placement && !placement->artworkIcons.isEmpty()) {
        loadIconCatalog(placement->artworkIcons);
        icons_=placement->artworkAssignments;
        mapArtwork_=placement->background;
        for(int id=1;id<=nodeCount;++id)iconName(icon(id));
    }
    originalMapArtwork_=mapArtwork_;originalArtworkIcons_=artworkIcons_;
    originalIcons_=icons_;
    worldEntry_ = routes.scripts[0];
    for (auto it = routes.nodes.cbegin(); it != routes.nodes.cend(); ++it) {
        labels_[it.key()] = it->name;
        pointers_[it.key()] = u32(rom.bytes(), nodeOffset + (it.key() - 1) * 8 + 4);
    }
}
QPoint CampaignMap::position(int id) const {
    nodeId(id);
    return {qint16(u16(nodes_, (id - 1) * 8)), qint16(u16(nodes_, (id - 1) * 8 + 2))};
}
int CampaignMap::label(int id) const {
    nodeId(id);
    const auto pointer = u32(nodes_, (id - 1) * 8 + 4);
    for (auto it = pointers_.cbegin(); it != pointers_.cend(); ++it)
        if (it.value() == pointer)
            return it.key();
    return -1;
}
QString CampaignMap::name(int id) const {
    return names_.value(id,labels_.value(label(id), QString("Unused map location %1").arg(id)));
}
void CampaignMap::setName(int id,const QString &text) {
    nodeId(id);const auto clean=text.trimmed();
    if(clean.isEmpty() || clean.size()>48)throw Error("Use 1–48 characters for a map location name");
    for(const auto ch:clean)if(ch.unicode()<32 || ch.unicode()>126)
        throw Error("Use printable game characters for a map location name");
    if(clean==labels_.value(label(id)))names_.remove(id);else names_[id]=clean;
}
QMap<int,QString> CampaignMap::relocatedNames() const {
    auto result=names_;
    for(int id=1;id<=nodeCount;++id) {
        const auto pointer=u32(nodes_,(id-1)*8+4);
        if(!result.contains(id) && ownedNamePointers_.contains(pointer))result[id]=ownedNamePointers_[pointer];
    }
    return result;
}
QPair<int, int> CampaignMap::road(int id) const {
    roadId(id);
    if (id > totalRoads())
        throw Error("Road does not exist");
    return {u8(roads_, (id - 1) * 2), u8(roads_, (id - 1) * 2 + 1)};
}
QVector<int> CampaignMap::availableNodes(bool clearedStory) const {
    QVector<int> result;
    for (int id = 1; id <= nodeCount; ++id) {
        if (position(id) != QPoint() || (!clearedStory && u16(worldEntry_, id * 2)))
            continue;
        bool connected = false;
        for (int r = 1; r <= totalRoads(); ++r)
            connected |= road(r).first == id || road(r).second == id;
        if (!connected)
            result << id;
    }
    return result;
}
void CampaignMap::setNode(int id, QPoint p, int labelId) {
    nodeId(id);
    if (p.x() < 1 || p.x() >= mapWidth || p.y() < 1 || p.y() >= mapHeight ||
        !pointers_.contains(labelId))
        throw Error("Choose a location name and a position inside the world map");
    put16(nodes_, (id - 1) * 8, quint16(p.x()));
    put16(nodes_, (id - 1) * 8 + 2, quint16(p.y()));
    put32(nodes_, (id - 1) * 8 + 4, pointers_[labelId]);
}
void CampaignMap::startEmpty(int startingLocation) {
    nodeId(startingLocation);
    for(int id=1;id<=nodeCount;++id){put16(nodes_,(id-1)*8,0);put16(nodes_,(id-1)*8+2,0);}
    roads_.fill(0);shapes_.clear();names_.clear();
    QImage blank(mapWidth,mapHeight,QImage::Format_RGBA8888);blank.fill(QColor(238,222,181));
    setBackground(prepareArtwork(blank,256,true));
    setNode(startingLocation,{mapWidth/2,mapHeight/2},label(startingLocation));
    setName(startingLocation,"Starting location");
}
void CampaignMap::setRoad(int id, int from, int to) {
    roadId(id);
    if (!from && !to) { removeRoad(id); return; }
    nodeId(from);
    nodeId(to);
    if (from == to || position(from).x() == 0 || position(to).x() == 0)
        throw Error("Choose two different map locations");
    if (id > totalRoads())
        throw Error("Road does not exist");
    for (int r = 1; r <= totalRoads(); ++r)
        if (r != id && (road(r) == qMakePair(from, to) || road(r) == qMakePair(to, from)))
            throw Error("These map locations are already connected");
    if (road(id) != qMakePair(from,to)) shapes_.remove(id);
    put8(roads_, (id - 1) * 2, quint8(from));
    put8(roads_, (id - 1) * 2 + 1, quint8(to));
}
void CampaignMap::removeRoad(int id) {
    road(id); roads_.replace((id-1)*2,2,QByteArray(2,0)); shapes_.remove(id);
}
void CampaignMap::setRoadDots(int id,const QVector<QPoint> &points) {
    const auto ends=road(id);
    if (!ends.first || !ends.second) throw Error("Choose an existing road");
    if(points.isEmpty() || points.size()>bendCapacity)throw Error("A road supports 1–14 bends across two sections");
    for (const auto &p:points) if (p.x()<1 || p.x()>=mapWidth || p.y()<1 || p.y()>=mapHeight)
        throw Error("Keep road dots inside the map");
    if (points==roadWaypoints(id)) return;
    shapes_[id]={position(ends.first),position(ends.second),points};
}
int CampaignMap::addRoad(int from, int to) {
    for (int id=roadCount+1;id<=totalRoads();++id) if (!road(id).first) {
        auto next=*this; next.setRoad(id,from,to); *this=std::move(next); return id;
    }
    if (totalRoads() == roadCapacity)
        throw Error("All 96 connections are in use");
    auto next = *this;
    next.roads_ += QByteArray(2, 0);
    const int id = next.totalRoads();
    next.setRoad(id, from, to);
    *this = std::move(next);
    return id;
}
QVector<QPoint> CampaignMap::roadWaypoints(int id) const {
    const auto endpoints = road(id);
    if (!endpoints.first || !endpoints.second) return {};
    const auto a = position(endpoints.first), b = position(endpoints.second);
    QVector<QPoint> points;
    if (shapes_.contains(id)) {
        const auto &shape=shapes_[id]; points=shape.points;
        const auto da=a-shape.from, db=b-shape.to;
        for (int n=0;n<points.size();++n) {
            const double t=double(n+1)/(points.size()+1);
            points[n]+=QPoint(qRound(da.x()*(1-t)+db.x()*t),qRound(da.y()*(1-t)+db.y()*t));
        }
    } else if (id * 2 <= originalRoads_.size() &&
        roads_.mid((id - 1) * 2, 2) == originalRoads_.mid((id - 1) * 2, 2)) {
        for (int n = 0; n < originalBendCapacity_; ++n) {
            QPoint dot(qint16(u16(originalDots_, (id - 1) * originalBendCapacity_ * 4 + n * 4)),
                       qint16(u16(originalDots_, (id - 1) * originalBendCapacity_ * 4 + n * 4 + 2)));
            if (dot.x())
                points << dot;
        }
        const auto oldPosition = [&](int node) {
            return QPoint(qint16(u16(original_, (node - 1) * 8)),
                          qint16(u16(original_, (node - 1) * 8 + 2)));
        };
        const auto da = a - oldPosition(endpoints.first), db = b - oldPosition(endpoints.second);
        for (int n = 0; n < points.size(); ++n) {
            const double t = double(n + 1) / (points.size() + 1);
            points[n] += QPoint(qRound(da.x() * (1 - t) + db.x() * t),
                                qRound(da.y() * (1 - t) + db.y() * t));
        }
    } else {
        const int count = qBound(1, int(std::round(QLineF(a, b).length() / 12)) - 1, 7);
        for (int n = 1; n <= count; ++n) {
            const double t = double(n) / (count + 1);
            points << QPoint(qRound(a.x() * (1 - t) + b.x() * t),
                             qRound(a.y() * (1 - t) + b.y() * t));
        }
    }
    for (auto &p : points)
        p = {qBound(1, p.x(), mapWidth - 1), qBound(1, p.y(), mapHeight - 1)};
    return points;
}
QVector<QPoint> CampaignMap::roadDots(int id) const {
    const auto ends=road(id);
    if (!ends.first || !ends.second) return {};
    const auto bends=roadWaypoints(id);
    const bool unchanged=!shapes_.contains(id) && roads_.mid((id-1)*2,2)==originalRoads_.mid((id-1)*2,2) &&
        nodes_.mid((ends.first-1)*8,4)==original_.mid((ends.first-1)*8,4) &&
        nodes_.mid((ends.second-1)*8,4)==original_.mid((ends.second-1)*8,4);
    if (unchanged) return id<=originalDisplayDots_.size()?originalDisplayDots_[id-1]:bends;
    QVector<QPoint> path{position(ends.first)};path+=bends;path<<position(ends.second);
    QVector<double> distance{0};
    for(int n=1;n<path.size();++n)distance<<distance.last()+QLineF(path[n-1],path[n]).length();
    const double length=distance.last();
    // Retail internal dot gaps have a median of11.18 map pixels.
    const int segments=qMax(2,qRound(length/11.0));
    QVector<QPoint> result;int segment=1;
    for(int n=1;n<segments;++n) {
        const double at=length*n/segments;
        while(segment<path.size()-1 && distance[segment]<at)++segment;
        const double span=distance[segment]-distance[segment-1];
        const double t=span>0?(at-distance[segment-1])/span:0;
        const auto point=QPointF(path[segment-1])+(QPointF(path[segment])-path[segment-1])*t;
        const auto rounded=point.toPoint();
        if(result.isEmpty() || result.last()!=rounded)result<<rounded;
    }
    return result;
}
QVector<QVector<QPoint>> CampaignMap::mergedRoadDots(const QVector<QVector<QPoint>> &roads) {
    auto result=roads;
    QHash<int,QVector<QPoint>> cells;
    for(auto &road:result)for(auto &point:road) {
        const int x=point.x()/6,y=point.y()/6;
        QPoint merged=point;int closest=17;bool found=false;
        for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)
            for(const auto candidate:cells.value((y+dy)*128+x+dx)) {
                const auto delta=candidate-point;
                const int square=delta.x()*delta.x()+delta.y()*delta.y();
                if(square<closest){closest=square;merged=candidate;found=true;}
            }
        if(!found)cells[y*128+x].append(point);
        point=merged;
    }
    return result;
}
QVector<QPoint> CampaignMap::pulledRoad(int id,int dot,QPoint delta) const {
    const auto visible=roadDots(id),bends=roadWaypoints(id);
    if(dot<0 || dot>=visible.size())throw Error("Choose an existing road dot");
    if(delta.isNull())return bends;
    const auto ends=road(id);QVector<QPoint> path{position(ends.first)};
    path+=bends;path<<position(ends.second);
    const auto grabbed=visible[dot];int grab=path.indexOf(grabbed);
    auto deviation=[](QPoint p,QPoint a,QPoint b) {
        const QPointF v=b-a,w=p-a;const double square=v.x()*v.x()+v.y()*v.y();
        const double t=square?qBound(0.0,(v.x()*w.x()+v.y()*w.y())/square,1.0):0;
        return QLineF(QPointF(p),QPointF(a)+v*t).length();
    };
    if(grab<1 || grab==path.size()-1) {
        double best=1e9;int segment=0;
        for(int n=0;n<path.size()-1;++n){const auto d=deviation(grabbed,path[n],path[n+1]);if(d<best){best=d;segment=n;}}
        grab=segment+1;path.insert(grab,grabbed);
    }
    QVector<double> distance{0};
    for(int n=1;n<path.size();++n)distance<<distance.last()+QLineF(path[n-1],path[n]).length();
    const double center=distance[grab];
    // The brush is local to three visible dot intervals, not a fraction of the
    // entire road. Fixed boundary bends keep its pull from propagating farther.
    double left=0,right=0;
    for(int n=dot;n>qMax(0,dot-3);--n)left+=QLineF(visible[n],visible[n-1]).length();
    for(int n=dot;n<qMin(int(visible.size())-1,dot+3);++n)right+=QLineF(visible[n],visible[n+1]).length();
    if(dot<3)left=center;
    if(dot+3>=visible.size())right=distance.last()-center;
    auto pointAt=[&](double at) {
        int n=1;while(n<path.size()-1 && distance[n]<at)++n;
        const double span=distance[n]-distance[n-1],t=span?(at-distance[n-1])/span:0;
        return (QPointF(path[n-1])+(QPointF(path[n])-path[n-1])*t).toPoint();
    };
    const double from=qMax(0.0,center-left),to=qMin(distance.last(),center+right);
    const auto leftAnchor=pointAt(from),rightAnchor=pointAt(to);
    QVector<QPoint> shaped;
    for(int n=0;n<path.size();++n) {
        if(distance[n]<from)shaped<<path[n];
    }
    if(shaped.isEmpty() || shaped.last()!=leftAnchor)shaped<<leftAnchor;
    const int firstEdited=shaped.size()-1;
    for(int n=1;n<path.size()-1;++n)if(distance[n]>from && distance[n]<to) {
        const double span=distance[n]<=center?left:right;
        const double t=span?qBound(0.0,1.0-std::abs(distance[n]-center)/span,1.0):1;
        const double weight=t*t; // gentler falloff; the grabbed dot still follows the pointer
        const auto p=path[n]+QPoint(qRound(delta.x()*weight),qRound(delta.y()*weight));
        shaped<<QPoint(qBound(1,p.x(),mapWidth-1),qBound(1,p.y(),mapHeight-1));
    }
    if(shaped.isEmpty() || shaped.last()!=rightAnchor)shaped<<rightAnchor;
    const int lastEdited=shaped.size()-1;
    for(int n=0;n<path.size();++n)if(distance[n]>to)shaped<<path[n];
    const auto moved=QPoint(qBound(1,grabbed.x()+delta.x(),mapWidth-1),qBound(1,grabbed.y()+delta.y(),mapHeight-1));
    // Refit every drag, not just once the table is full. Resampled visible dots
    // and rounded brush boundaries otherwise accumulate tiny extra corners on
    // each gesture. Within the edited span allow less than one map pixel of
    // error against the whole proposed path, retaining the grabbed point and
    // both fixed boundaries. Outside it, remove only exactly collinear points.
    // The shortest-path fit avoids cumulative error from greedy deletions.
    const int count=shaped.size();
    QVector<int> cost(count,count+1),previous(count,-1);cost[0]=0;
    for(int end=1;end<count;++end)for(int start=0;start<end;++start) {
        if(cost[start]+1>=cost[end])continue;
        const double tolerance=start>=firstEdited && end<=lastEdited?0.75:0.000001;
        const QPointF direction=shaped[end]-shaped[start];
        const double square=direction.x()*direction.x()+direction.y()*direction.y();
        double lastAlong=0;bool valid=true;
        for(int n=start+1;n<end;++n) {
            if(n==firstEdited || n==lastEdited || shaped[n]==moved ||
               deviation(shaped[n],shaped[start],shaped[end])>tolerance) {valid=false;break;}
            const QPointF offset=shaped[n]-shaped[start];
            const double along=square?(offset.x()*direction.x()+offset.y()*direction.y())/square:0;
            if(along<lastAlong-0.000001){valid=false;break;}
            lastAlong=along;
        }
        if(valid){cost[end]=cost[start]+1;previous[end]=start;}
    }
    QVector<QPoint> fitted;
    for(int at=count-1;at>=0;at=previous[at])fitted.prepend(shaped[at]);
    if(fitted.size()>bendCapacity+2)
        throw Error("This shape needs more than 14 bends. Undo a bend or straighten this part of the road first.");
    QVector<QPoint> result;
    for(int n=1;n<fitted.size()-1;++n)result<<fitted[n];
    return result;
}
QByteArray CampaignMap::dotBytes() const {
    auto data = QByteArray(totalRoads() * bendCapacity * 4, 0);
    for (int id = 1; id <= totalRoads(); ++id) {
        const auto dots = roadWaypoints(id);
        for (int n = 0; n < dots.size(); ++n) {
            put16(data, (id - 1) * bendCapacity * 4 + n * 4, quint16(dots[n].x()));
            put16(data, (id - 1) * bendCapacity * 4 + n * 4 + 2, quint16(dots[n].y()));
        }
    }
    return data;
}
RasterImage CampaignMap::background() const {
    return decodeRawImage(mapArtwork_, {{"width", mapWidth},
                                        {"height", mapHeight},
                                        {"pixelFormat", "ci8"},
                                        {"paletteCount", 256},
                                        {"paletteOffset", 0},
                                        {"dataOffset", 512}})
        .composite;
}
RasterImage CampaignMap::nodeArtwork(int node) const {
    return iconArtwork(icon(node));
}
int CampaignMap::icon(int node) const {
    nodeId(node);return quint8(icons_[node-1]);
}
void CampaignMap::setIcon(int node,int value) {
    nodeId(node);iconName(value);icons_[node-1]=char(value);
}
QByteArray CampaignMap::iconBytes()const {
    QByteArray bytes(5,0);for(int id=1;id<=nodeCount;++id)
        if(iconKind(icon(id)))bytes[(id-1)/8]|=char(1<<((id-1)%8));
    return bytes;
}
RasterImage CampaignMap::iconArtwork(int value) const {
    const auto image=iconFrame(value,1,0);
    return {image.width(),image.height(),QByteArray(reinterpret_cast<const char *>(image.constBits()),image.sizeInBytes())};
}
RasterImage CampaignMap::roadArtwork() const {
    return decodeRawImage(markers_, {{"width", 16},
                                     {"height", 6},
                                     {"pixelFormat", "ci4"},
                                     {"paletteCount", 16},
                                     {"paletteOffset", 0xa0},
                                     {"dataOffset", 0x4c0}})
        .composite;
}
QJsonObject CampaignMap::json() const {
    QJsonObject nodes, roads;
    for (int id = 1; id <= nodeCount; ++id)
        if (nodes_.mid((id - 1) * 8, 8) != original_.mid((id - 1) * 8, 8))
            nodes[QString::number(id)] = QJsonArray{position(id).x(), position(id).y(), label(id)};
    for (int id = 1; id <= totalRoads(); ++id)
        if (roads_.mid((id - 1) * 2, 2) != originalRoads_.mid((id - 1) * 2, 2))
            roads[QString::number(id)] = QJsonArray{road(id).first, road(id).second};
    QJsonObject result{{"version",1},{"nodes",nodes},{"roads",roads}}, shapes;
    for (auto it=shapes_.begin();it!=shapes_.end();++it) {
        QJsonArray points;
        for (const auto &p:it->points) points.append(QJsonArray{p.x(),p.y()});
        shapes[QString::number(it.key())]=QJsonObject{{"from",QJsonArray{it->from.x(),it->from.y()}},
            {"to",QJsonArray{it->to.x(),it->to.y()}},{"points",points}};
    }
    if (!shapes.isEmpty()) result["shapes"]=shapes;
    QJsonObject names;
    for(auto it=names_.begin();it!=names_.end();++it)names[QString::number(it.key())]=it.value();
    if(!names.isEmpty())result["names"]=names;
    QJsonObject icons;
    for(int id=1;id<=nodeCount;++id)
        if(icon(id)!=quint8(originalIcons_[id-1]))icons[QString::number(id)]=icon(id);
    if(!icons.isEmpty())result["icons"]=icons;
    const auto artwork=artworkEdits();if(!artwork.isEmpty())result["artwork"]=artwork;
    return result;
}
void CampaignMap::apply(const QJsonObject &value) {
    for(const auto &key:value.keys())if(key!="version" && key!="nodes" && key!="roads" && key!="shapes" && key!="names" && key!="icons" && key!="artwork")
        throw Error("Unknown campaign map field");
    if ((value.contains("icons") && !value["icons"].isObject()) || (value.contains("shapes") && !value["shapes"].isObject()) || (value.contains("names") && !value["names"].isObject()) || value["version"] != 1 || !value["nodes"].isObject() ||
        !value["roads"].isObject())
        throw Error("Invalid campaign map data");
    auto next = *this;
    next.shapes_.clear();
    next.names_.clear();
    next.icons_=originalIcons_;
    next.artworkIcons_=originalArtworkIcons_;next.mapArtwork_=originalMapArtwork_;
    if(value.contains("artwork")) {
        if(!value["artwork"].isObject())throw Error("Invalid campaign artwork");
        const auto art=value["artwork"].toObject();
        for(const auto &key:art.keys())if(key!="background" && key!="icons")throw Error("Unknown campaign artwork field");
        if(art.contains("background")) {
            const auto encoded=art["background"].toString().toLatin1(),data=QByteArray::fromBase64(encoded);
            if(data.toBase64()!=encoded)throw Error("Invalid map background encoding");
            next.setBackground(data);
        }
        if(art.contains("icons")) {
            if(!art["icons"].isObject())throw Error("Invalid icon catalog");
            next.loadIconCatalog(art["icons"].toObject());
        }
    }
    next.nodes_ = original_;
    next.roads_ = originalRoads_;
    int lastRoad = next.totalRoads();
    const auto roadEdits = value["roads"].toObject();
    for (auto it = roadEdits.begin(); it != roadEdits.end(); ++it) {
        bool ok;
        const int id = it.key().toInt(&ok);
        if (!ok || id < 1 || id > roadCapacity)
            throw Error("Invalid road ID");
        lastRoad = qMax(lastRoad, id);
    }
    for (int id = next.totalRoads() + 1; id <= lastRoad; ++id)
        if (!roadEdits.contains(QString::number(id)))
            throw Error("Missing added connection");
    next.roads_.resize(lastRoad * 2);
    for (const auto &kind : {QString("nodes"), QString("roads")}) {
        const auto entries = value[kind].toObject();
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            bool ok;
            const int id = it.key().toInt(&ok);
            const auto a = it.value().toArray();
            if (!ok || QString::number(id) != it.key() || !it.value().isArray() ||
                a.size() != (kind == "nodes" ? 3 : 2))
                throw Error("Invalid campaign map record");
            for (const auto v : a)
                if (!v.isDouble() || v.toDouble() != v.toInt())
                    throw Error("Campaign map fields must be integers");
            if (kind == "nodes") {
                if(a[0].toInt()==0&&a[1].toInt()==0) {
                    nodeId(id);if(!next.pointers_.contains(a[2].toInt()))throw Error("Invalid cleared map location label");
                    put16(next.nodes_,(id-1)*8,0);put16(next.nodes_,(id-1)*8+2,0);
                    put32(next.nodes_,(id-1)*8+4,next.pointers_[a[2].toInt()]);
                }else next.setNode(id, {a[0].toInt(), a[1].toInt()}, a[2].toInt());
            }
            else {
                roadId(id);
                if (a[0].toInt() || a[1].toInt()) { nodeId(a[0].toInt()); nodeId(a[1].toInt()); }
                put8(next.roads_, (id - 1) * 2, quint8(a[0].toInt()));
                put8(next.roads_, (id - 1) * 2 + 1, quint8(a[1].toInt()));
            }
        }
    }
    for (int id = 1; id <= next.totalRoads(); ++id)
        if (id > totalRoads() || next.road(id) != road(id))
            next.setRoad(id, next.road(id).first, next.road(id).second);
    for(int id=1;id<=next.totalRoads();++id) {
        const auto ends=next.road(id);
        if(ends.first&&(!next.position(ends.first).x()||!next.position(ends.second).x()))throw Error("A road connects to a cleared map location");
    }
    const auto shapeEdits=value["shapes"].toObject();
    auto point=[](const QJsonValue &v) {
        const auto a=v.toArray();
        if (a.size()!=2 || !a[0].isDouble() || !a[1].isDouble() || a[0].toDouble()!=a[0].toInt() || a[1].toDouble()!=a[1].toInt() ||
            a[0].toInt()<1 || a[0].toInt()>=mapWidth || a[1].toInt()<1 || a[1].toInt()>=mapHeight)
            throw Error("Invalid road dot position");
        return QPoint(a[0].toInt(),a[1].toInt());
    };
    for (auto it=shapeEdits.begin();it!=shapeEdits.end();++it) {
        bool ok; const int id=it.key().toInt(&ok);const auto o=it.value().toObject();
        if (!ok || QString::number(id)!=it.key() || o.size()!=3 || !next.road(id).first)
            throw Error("Invalid road shape");
        RoadShape shape{point(o["from"]),point(o["to"]),{}};
        const auto points=o["points"].toArray();
        if (points.isEmpty() || points.size()>bendCapacity) throw Error("A road supports 1–14 bends across two sections");
        for (const auto &v:points) shape.points.append(point(v));
        next.shapes_[id]=shape;
    }
    const auto names=value["names"].toObject();
    for(auto it=names.begin();it!=names.end();++it) {
        bool ok;const int id=it.key().toInt(&ok);
        if(!ok || QString::number(id)!=it.key() || !it.value().isString())throw Error("Invalid map location name");
        next.setName(id,it.value().toString());
    }
    const auto icons=value["icons"].toObject();
    for(auto it=icons.begin();it!=icons.end();++it) {
        bool ok;const int id=it.key().toInt(&ok);
        if(!ok || QString::number(id)!=it.key() || !it.value().isDouble() || it.value().toDouble()!=it.value().toInt())
            throw Error("Invalid map location icon");
        next.setIcon(id,it.value().toInt());
    }
    for(int id=1;id<=nodeCount;++id)next.iconName(next.icon(id));
    *this = std::move(next);
}
QByteArray CampaignMap::rebuild(const QByteArray &input) const {
    auto image = input;
    for (int id = 0; id < nodeCount; ++id)
        if (nodes_.mid(id * 8, 8) != original_.mid(id * 8, 8)) {
            if (image.mid(nodeOffset + id * 8, 8) != original_.mid(id * 8, 8))
                throw Error("World-map node was changed by another export feature");
            image.replace(nodeOffset + id * 8, 8, nodes_.mid(id * 8, 8));
        }
    for (int id = 0; id < roadCount; ++id)
        if (roads_.mid(id * 2, 2) != originalRoads_.mid(id * 2, 2)) {
            if (image.mid(roadOffset + id * 2, 2) != originalRoads_.mid(id * 2, 2))
                throw Error("World-map road was changed by another export feature");
            image.replace(roadOffset + id * 2, 2, roads_.mid(id * 2, 2));
        }
    const auto dots = dotBytes();
    // Keep the original table's first section mirrored for older readers. The
    // installed travel helper consumes all sections from its separate allocation.
    for (int id = 0; id < roadCount; ++id) {
        const auto before=originalDots_.mid(id*originalBendCapacity_*4,28);
        const auto after=dots.mid(id*bendCapacity*4,28);
        if(after!=before) {
            if(image.mid(0x86040+id*28,28)!=before)
                throw Error("World-map dots were changed by another export feature");
            image.replace(0x86040+id*28,28,after);
        }
    }
    return image;
}
int CampaignMap::changedNodes() const {
    const auto edits=json();
    return edits["nodes"].toObject().size() + edits["roads"].toObject().size() + edits["icons"].toObject().size() + shapes_.size() + names_.size() + edits["artwork"].toObject().size();
}
} // namespace lordly
