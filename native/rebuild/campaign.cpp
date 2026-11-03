#include "rebuild/campaign.h"
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
    if (!p.campaignMap())
        return input;
    const auto &map = *p.campaignMap();
    const auto prior = CampaignPlacement::read(p.rom().bytes());
    if (!map.changedNodes())
        return input;
    auto checked = input;
    if (prior) {
        auto expected = checked;
        loader(expected, prior->overlayOffset, prior->overlaySize);
        if (expected.mid(0x514ec, 0xc4) != checked.mid(0x514ec, 0xc4))
            throw Error("Campaign loader was changed by another feature");
        loader(checked, CampaignPlacement::sourceStart,
               CampaignPlacement::sourceEnd - CampaignPlacement::sourceStart, true);
    }
    for (const auto &v :
         QJsonDocument::fromJson(resource("owners.json")).object()["owners"].toArray()) {
        const auto owner = v.toObject();
        const int at = owner["start"].toInt(), size = owner["end"].toInt() - at;
        if (sha256(checked.mid(at, size)).toUpper() != owner["sha256"].toString().toLatin1())
            throw Error(
                QString("World-map code at ROM %1 has unrecognized changes").arg(at, 0, 16));
    }
    constexpr quint32 countAddress = CampaignPlacement::codeAddress + 8192,
                      roadsAddress = countAddress + 16, dotsAddress = roadsAddress + 192,
                      displayDirectory = dotsAddress + 96 * 28,
                      displayPoints = displayDirectory + 400;
    const auto code = compiler.compileModule(resource("roads.c"), directory, "campaign_roads",
                                             CampaignPlacement::codeAddress, {},
                                             {{"campaign_road_count", countAddress},
                                              {"campaign_location_icons",countAddress+4},
                                              {"campaign_roads", roadsAddress},
                                              {"campaign_display_offsets", displayDirectory},
                                              {"campaign_display_points", displayPoints}});
    if (code.bytes.size() > 8192)
        throw Error("Campaign code exceeds its allocation");
    auto overlay = input.mid(CampaignPlacement::sourceStart,
                             CampaignPlacement::sourceEnd - CampaignPlacement::sourceStart);
    // Keep the native renderer's setup, but submit the resampled dots below.
    // Travel reads its separate, relocated waypoint table and is unchanged.
    overlay.replace(0x86040-CampaignPlacement::sourceStart,1120,QByteArray(1120,0));
    overlay += code.bytes;
    overlay += QByteArray(countAddress - CampaignPlacement::ramStart - overlay.size(), 0);
    auto data = QByteArray(16 + 192 + 96 * 28 + 400, 0);
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
        displayCount+=dots.size();put32(data,16+192+96*28+road*4,displayCount);
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
    for (const int low : {0x7b288, 0x7b2f0})
        address(overlay, low - 4 - CampaignPlacement::sourceStart,
                low - CampaignPlacement::sourceStart, dotsAddress);
    int calls = 0;
    for (int at = 0; at < 0x85820 - CampaignPlacement::sourceStart; at += 4)
        if (u32(overlay, at) == (0x0c000000 | ((0x801a347c >> 2) & 0x3ffffff))) {
            put32(overlay, at,
                  0x0c000000 | ((code.entries["campaign_draw_roads"] >> 2) & 0x3ffffff));
            ++calls;
        }
    if (calls != 1)
        throw Error("World-map road drawing caller is unrecognized");
    auto output = input;
    quint32 descriptor = prior && overlay.size() <= prior->overlaySize
                             ? prior->descriptor
                             : quint32((output.size() + 15) & ~qsizetype(15));
    if (quint64(descriptor) + 80 + overlay.size() > 0x4000000)
        throw Error("Campaign data would exceed the supported ROM size");
    if (descriptor + 80 + overlay.size() > output.size())
        output += QByteArray(descriptor + 80 + overlay.size() - output.size(), char(0xff));
    auto header = QByteArray(80, 0);
    header.replace(0, 8, "LCCAMP01");
    put32(header, 8, 3);
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
