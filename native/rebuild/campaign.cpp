#include "rebuild/campaign.h"
#include "engine/new_campaign.h"
#include "engine/campaign_runtime.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
namespace lordly {
namespace {
QByteArray resource(const QString &name) {
    QFile f(":/game/campaign/" + name);
    if (!f.open(QIODevice::ReadOnly))
        throw Error("Missing campaign source");
    return f.readAll();
}
void address(QByteArray &b, int high, int low, quint32 value) {
    put32(b, high, (u32(b, high) & 0xffff0000) | ((value + 0x8000) >> 16));
    put32(b, low, (u32(b, low) & 0xffff0000) | (value & 65535));
}
void loader(QByteArray &b, quint32 offset, quint32 size, bool stock = false) {
    const auto start = CampaignPlacement::ramStart, end = start + size;
    address(b, 0x514fc, 0x51500, stock ? 0x801a6890 : end);
    address(b, 0x5150c, 0x51510, stock ? 0x801a6890 : start);
    address(b, 0x51514, 0x51518, end);
    address(b, 0x51524, 0x51528, offset);
    address(b, 0x51534, 0x51538, offset + size);
}
} // namespace
QByteArray rebuildCampaignRuntime(const Project &p, const QByteArray &input,
                                  const Compiler &compiler, const QString &directory) {
    const auto prior = CampaignPlacement::read(p.rom().bytes());
    const bool newCampaign=p.sceneEvents()&&!NewCampaign::settings(*p.sceneEvents()).isEmpty();
    if (!p.campaignMap() && !prior && !newCampaign)
        return input;
    const auto loaded=p.campaignMap()?std::optional<CampaignMap>{}:std::optional<CampaignMap>{p.rom()};
    const auto &map = p.campaignMap()?*p.campaignMap():*loaded;
    if (!newCampaign && !map.changedNodes() && (!prior || prior->version==6))
        return input;
    const bool artwork=!map.artworkEdits().isEmpty() || (prior && !prior->artworkIcons.isEmpty());
    auto output=input;
    quint32 backgroundOffset=prior && prior->backgroundOffset?prior->backgroundOffset:0x2325f76;
    quint32 backgroundAllocation=prior && prior->backgroundAllocation?prior->backgroundAllocation:u32(input,backgroundOffset)+4;
    if(map.artworkEdits().contains("background")) {
        const auto encoded=encodeLzss(map.backgroundBytes());
        const quint32 required=quint32((encoded.size()+8+15)&~qsizetype(15));
        const bool reuse=prior && prior->backgroundOffset>=0x2800000 && required<=prior->backgroundAllocation;
        const quint32 allocation=reuse?prior->backgroundAllocation:required;
        backgroundOffset=reuse
            ?prior->backgroundOffset:quint32((output.size()+15)&~qsizetype(15));
        backgroundAllocation=allocation;
        if(quint64(backgroundOffset)+allocation>0x4000000)throw Error("Map artwork exceeds the supported ROM size");
        if(backgroundOffset+allocation>output.size())output.resize(backgroundOffset+allocation,char(0xff));
        QByteArray container(allocation,3);put32(container,0,allocation-4);put32(container,4,map.backgroundBytes().size());
        container.replace(8,encoded.size(),encoded);output.replace(backgroundOffset,allocation,container);
        if(decodeLzss(encoded,map.backgroundBytes().size())!=map.backgroundBytes())throw Error("Map artwork compression readback failed");
    }
    auto checked = input;
    if (prior) {
        auto expected = checked;
        loader(expected, prior->overlayOffset, prior->overlaySize);
        if (expected.mid(0x514ec, 0xc4) != checked.mid(0x514ec, 0xc4))
            throw Error("Campaign loader was changed by another feature");
        loader(checked, CampaignPlacement::sourceStart,
               CampaignPlacement::sourceEnd - CampaignPlacement::sourceStart, true);
    }
    // Event commands also run while the world overlay is absent. Bound these
    // resident owners in place; adapt the accepted getter's C.
    QMap<int,QByteArray> resident;
    const auto flagOwners=QJsonDocument::fromJson(resource("flag-owners.json")).object();
    compiler.compileQualifiedOwner(flagOwners["originalGetter"].toObject(),directory,"campaign_event_road_get_original");
    for(const auto *key:{"set","get"}) {
        const auto owner=flagOwners[key].toObject();
        resident[owner["start"].toInt()]=compiler.compileQualifiedOwner(owner,directory,QString("campaign_event_road_%1").arg(key)).code;
    }
    for (const auto &v :
         QJsonDocument::fromJson(resource("owners.json")).object()["owners"].toArray()) {
        const auto owner = v.toObject();
        const int at = owner["start"].toInt(), size = owner["end"].toInt() - at;
        if (sha256(checked.mid(at, size)).toUpper() != owner["sha256"].toString().toLatin1() &&
            (!resident.contains(at) || checked.mid(at,size)!=resident.value(at)))
            throw Error(
                QString("World-map code at ROM %1 has unrecognized changes").arg(at, 0, 16));
    }
    for(auto it=resident.begin();it!=resident.end();++it)output.replace(it.key(),it.value().size(),it.value());
    constexpr quint32 countAddress = CampaignPlacement::codeAddress + 8192,
                      roadsAddress = countAddress + 16, dotsAddress = roadsAddress + 192,
                      displayDirectory = dotsAddress + 96 * CampaignMap::bendCapacity * 4,
                      displayPoints = displayDirectory + 400;
    const auto code = compiler.compileModule(resource("roads.c"), directory, "campaign_roads",
                                             CampaignPlacement::codeAddress, {},
                                             {{"campaign_road_count", countAddress},
                                              {"campaign_road_flags",countAddress-100},
                                              {"campaign_travel_next", countAddress+12},
                                              {"campaign_waypoints", dotsAddress},
                                              {"campaign_location_icons",countAddress+4},
                                              {"campaign_artwork",countAddress-4},
                                              {"campaign_roads", roadsAddress},
                                              {"campaign_display_offsets", displayDirectory},
                                              {"campaign_display_points", displayPoints}});
    if (code.bytes.size() > 8092)
        throw Error("Campaign code exceeds its allocation");
    auto overlay = input.mid(CampaignPlacement::sourceStart,
                             CampaignPlacement::sourceEnd - CampaignPlacement::sourceStart);
    // Keep the native renderer's setup, but submit the resampled dots below.
    // Travel pages the separate, relocated waypoint table in seven-bend sections.
    overlay.replace(0x86040-CampaignPlacement::sourceStart,1120,QByteArray(1120,0));
    overlay += code.bytes;
    overlay += QByteArray(countAddress - CampaignPlacement::ramStart - overlay.size(), 0);
    const auto originalRoads=QByteArray::fromHex(QJsonDocument::fromJson(resource("owners.json")).object()["originalRoads"].toString().toLatin1());
    if(originalRoads.size()!=80)throw Error("Missing original road flag identities");
    for(int id=1;id<=map.totalRoads();++id) {
        int flag=id<=40?id:0;const auto ends=map.road(id);
        if(id>40 && ends.first && ends.second)for(int n=0;n<40;++n) {
            const auto a=u8(originalRoads,n*2),b=u8(originalRoads,n*2+1);
            if((ends.first==a && ends.second==b)||(ends.first==b && ends.second==a)){flag=n+1;break;}
        }
        put8(overlay,countAddress-100-CampaignPlacement::ramStart+id-1,quint8(newCampaign?0:flag));
    }
    auto data = QByteArray(16 + 192 + 96 * CampaignMap::bendCapacity * 4 + 400, 0);
    put32(data, 0, quint32(map.totalRoads()));
    data.replace(4,5,map.iconBytes());
    data.replace(16, map.roadBytes().size(), map.roadBytes());
    data.replace(16 + 192, map.dotBytes().size(), map.dotBytes());
    quint32 displayCount=0;
    for(int road=1;road<=map.totalRoads();++road) {
        const auto dots=map.roadDots(road);
        if(dots.size()>512)throw Error("World-map road has too many visible dots");
        for(const auto &point:dots) {
            QByteArray pair(4,0);put16(pair,0,quint16(point.x()));put16(pair,2,quint16(point.y()));data+=pair;
        }
        displayCount+=dots.size();put32(data,16+192+96*CampaignMap::bendCapacity*4+road*4,displayCount);
    }
    // Retain the original full-capacity dot budget, growing for longer paths.
    data+=QByteArray((qMax(displayCount,quint32(96*7))-displayCount)*4,0);
    overlay += data;
    quint32 nameDirectory=0;QMap<int,quint32> namePointers;
    const auto names=map.relocatedNames();
    if(!names.isEmpty()) {
        while(overlay.size()%16)overlay.append(char(0));
        const auto offset=overlay.size();nameDirectory=CampaignPlacement::ramStart+quint32(offset);
        overlay+=QByteArray(160,0);
        for(auto it=names.begin();it!=names.end();++it) {
            const auto pointer=CampaignPlacement::ramStart+quint32(overlay.size());namePointers[it.key()]=pointer;
            put32(overlay,offset+(it.key()-1)*4,pointer);
            overlay+=QByteArray(1,char(14))+it.value().toLatin1()+char(15)+char(0);
            put32(overlay,0x864a0-CampaignPlacement::sourceStart+(it.key()-1)*8+4,pointer);
        }
    }
    // Location names have variable byte lengths. Keep the overlay DMA and the
    // cartridge image aligned even when no artwork follows the last name.
    while(overlay.size()%16)overlay.append(char(0));
    if(artwork) {
        while(overlay.size()%16)overlay.append(char(0));
        const int artOffset=overlay.size();const quint32 artAddress=CampaignPlacement::ramStart+artOffset;
        const auto catalog=map.iconCatalog();const auto metadata=QJsonDocument(catalog).toJson(QJsonDocument::Compact);
        QByteArray art(96+metadata.size(),0);art.replace(0,8,"LCMAPA01");
        put32(art,8,backgroundOffset);put32(art,12,backgroundAllocation);
        art.replace(16,32,QByteArray::fromHex(sha256(output.mid(backgroundOffset,backgroundAllocation))));
        art.replace(52,40,map.iconAssignments());put32(art,92,metadata.size());art.replace(96,metadata.size(),metadata);
        while(art.size()%16)art.append(char(0));
        const int records=art.size();put32(art,48,artAddress+records);art+=QByteArray(CampaignMap::iconCapacity*12,0);
        for(int id:map.iconIds()) {
            put32(art,records+id*12,map.iconKind(id));
            for(int state=1;state<=2;++state) {
                put32(art,records+id*12+state*4,artAddress+art.size());
                for(int frame=0;frame<3;++frame)art+=map.iconFrameBytes(id,state,frame);
            }
        }
        overlay+=art;
        put32(overlay,countAddress-4-CampaignPlacement::ramStart,artAddress);
        // Resource keys use LUI/ORI, not the sign-adjusted LUI/ADDIU loader pair.
        const quint32 key=backgroundOffset-0x594280;
        put32(overlay,0x797b0-CampaignPlacement::sourceStart,0x3c040000|(key>>16));
        put32(overlay,0x797b4-CampaignPlacement::sourceStart,0x34840000|(key&65535));
    }
    auto hook = [&](int rom, const QString &symbol) {
        if (!code.entries.contains(symbol))
            throw Error("Missing compiled campaign entry");
        put32(overlay, rom - CampaignPlacement::sourceStart,
              0x08000000 | ((code.entries[symbol] >> 2) & 0x3ffffff));
        put32(overlay, rom - CampaignPlacement::sourceStart + 4, 0);
    };
    hook(0x7d450, "campaign_find_path");
    hook(0x7dc08, "campaign_find_road");
    hook(0x7d960, "campaign_road_open");
    hook(0x7d90c, "campaign_set_road");
    // The native renderer only has a ten-location castle list. Replace its
    // membership loop with a bounded C lookup, retaining its state palettes,
    // flag animation and all rendering commands. No accepted C owner exists
    // for this renderer; owners.json authenticates its entire original body.
    const auto iconEntry=code.entries.value("campaign_location_icon");
    if(!iconEntry)throw Error("Missing world-map icon selector");
    const int iconSite=0x81d98-CampaignPlacement::sourceStart;
    overlay.replace(iconSite,40,QByteArray(40,0));
    put32(overlay,iconSite,0x26440001); // a0 = location index + 1
    put32(overlay,iconSite+4,0x0c000000|((iconEntry>>2)&0x3ffffff));
    put32(overlay,iconSite+12,0x0040a021); // s4 = selected icon
    if(artwork) {
        const auto entry=code.entries.value("campaign_icon_texture");
        if(!entry)throw Error("Missing compiled location artwork selector");
        for(const int site:{0x81f08,0x82094}) {
            const int offset=site-CampaignPlacement::sourceStart;
            overlay.replace(offset,24,QByteArray(24,0));
            put32(overlay,offset,0x02402021); // a0 = location index
            put32(overlay,offset+4,0x03c02821); // a1 = original location state
            put32(overlay,offset+8,0x0c000000|((entry>>2)&0x3ffffff));
        }
    }
    // Keep each native travel page bounded. The second section continues at
    // the shared bend without creating a location or changing road/story flags.
    const auto prepare=code.entries.value("campaign_prepare_road"),more=code.entries.value("campaign_more_road");
    if(!prepare || !more)throw Error("Missing compiled road-section support");
    const int prepareSite=0x7b230-CampaignPlacement::sourceStart;
    overlay.replace(prepareSite,0x194,QByteArray(0x194,0));
    put32(overlay,prepareSite,0x0c000000|((prepare>>2)&0x3ffffff));
    put32(overlay,prepareSite+8,0x3c048019); // restore the context expected by the native walker
    put32(overlay,prepareSite+12,0x8c847af8);
    put32(overlay,prepareSite+16,0x08000000|((0x8019c434u>>2)&0x3ffffff));
    const int moreSite=0x7b5e0-CampaignPlacement::sourceStart;
    overlay.replace(moreSite,24,QByteArray(24,0));
    put32(overlay,moreSite,0x0c000000|((more>>2)&0x3ffffff));
    put32(overlay,moreSite+8,0x3c038019); // v1 is still used by the unchanged end-of-road path
    put32(overlay,moreSite+12,0x8c637af8);
    // The original branch at 0x7b5f8 now consumes the helper's boolean result.
    int calls = 0;
    for (int at = 0; at < 0x85820 - CampaignPlacement::sourceStart; at += 4)
        if (u32(overlay, at) == (0x0c000000 | ((0x801a347c >> 2) & 0x3ffffff))) {
            put32(overlay, at,
                  0x0c000000 | ((code.entries["campaign_draw_roads"] >> 2) & 0x3ffffff));
            ++calls;
        }
    if (calls != 1)
        throw Error("World-map road drawing caller is unrecognized");
    if(quint64(CampaignPlacement::ramStart)+overlay.size()>0x801f1510)
        throw Error("World-map artwork and connections exceed the map's memory allocation");
    quint32 descriptor = prior && overlay.size() <= prior->overlaySize
                             ? prior->descriptor
                             : quint32((output.size() + 15) & ~qsizetype(15));
    if (quint64(descriptor) + 80 + overlay.size() > 0x4000000)
        throw Error("Campaign data would exceed the supported ROM size");
    if (descriptor + 80 + overlay.size() > output.size())
        output += QByteArray(descriptor + 80 + overlay.size() - output.size(), char(0xff));
    auto header = QByteArray(80, 0);
    header.replace(0, 8, "LCCAMP01");
    put32(header, 8, newCampaign?7:6);
    put32(header, 12, descriptor + 80);
    put32(header, 16, quint32(overlay.size()));
    put32(header, 20, quint32(map.totalRoads()));
    put32(header, 24, roadsAddress);
    put32(header, 28, dotsAddress);
    header.replace(32, 32, QByteArray::fromHex(sha256(overlay)));
    put32(header,64,displayDirectory);put32(header,68,displayPoints);put32(header,72,displayCount);put32(header,76,nameDirectory);
    output.replace(descriptor, 80, header);
    output.replace(descriptor + 80, overlay.size(), overlay);
    for(auto it=namePointers.begin();it!=namePointers.end();++it)put32(output,0x864a0+(it.key()-1)*8+4,it.value());
    loader(output, descriptor + 80, quint32(overlay.size()));
    CampaignPlacement::read(output);
    return output;
}
} // namespace lordly
