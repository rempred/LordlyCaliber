#include "engine/attack_data.h"
#include "engine/effect_player.h"
#include "engine/effect_resources.h"
#include "engine/promotion.h"
#include "engine/resource.h"
#include "engine/text.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QSet>
#include <cmath>
#include <cctype>
#include <QImage>

namespace lordly {
namespace {
QByteArray attackName(const QString &text) {
    if (text.trimmed().isEmpty() || text.contains('\n') || text.contains('\r'))
        throw Error("Enter a one-line attack name");
    auto bytes = encodeText(text.trimmed(), false);
    if (bytes.isEmpty() || bytes.size() > 128 || bytes.contains(char(0)))
        throw Error("Attack names may contain up to 128 encoded bytes");
    return bytes;
}
int integer(const QJsonObject &o, const char *key, int fallback) {
    if (!o.contains(key))
        return fallback;
    const auto v = o[key];
    if (!v.isDouble() || !std::isfinite(v.toDouble()) || v.toDouble() != v.toInt())
        throw Error(QString("Invalid attack setting: %1").arg(key));
    return v.toInt();
}
void validateSpellLinks(const QVector<Attack> &entries) {
    for (const auto &a : entries)
        for (auto it = a.spellLinks.cbegin(); it != a.spellLinks.cend(); ++it)
            if (it.value() < 1 || it.value() >= entries.size() ||
                AttackData::isTemplate(entries[it.value()].behavior) || u8(entries[it.value()].data, 0) > 5)
                throw Error("A spell array must select an existing attack, not another spell array");
}
} // namespace
std::optional<AttackPlacement> AttackPlacement::read(const Rom &rom) {
    const auto h = rom.bytes().mid(descriptorOffset, descriptorSize);
    if (!h.startsWith("LCATK001"))
        return {};
    if (h.size() != descriptorSize || u32(h, 8) != 1 || u32(h, 92) != crc32(h.first(92)))
        throw Error("Damaged attack allocation descriptor");
    AttackPlacement p;
    p.romOffset = u32(h, 12);
    p.address = u32(h, 16);
    p.size = u32(h, 20);
    p.records = u32(h, 28);
    p.settings = u32(h, 32);
    p.count = int(u32(h, 36));
    p.code = u32(h, 40);
    p.codeSize = u32(h, 44);
    p.spellIndexEntry = u32(h, 48);
    p.amountEntry = u32(h, 52);
    p.amountOriginal = u32(h, 56);
    p.hooks = u32(h, 60);
    p.hookCount = u32(h, 64);
    if (p.romOffset < 0x2800000 || (p.romOffset & 15) || (p.address & 15) ||
        p.address < 0x80243db0 || quint64(p.address) + p.size > 0x80400000 ||
        quint64(p.romOffset) + p.size > quint64(rom.bytes().size()) ||
        p.records != p.address + AttackData::recordOffset ||
        p.settings != p.address + AttackData::settingsOffset || p.count < AttackData::retailCount ||
        p.count > AttackData::maximumCount ||
        (p.codeSize &&
         (p.code < p.address || quint64(p.code) + p.codeSize > quint64(p.address) + p.size)) ||
        (!p.codeSize && p.code) || p.hookCount > 64 ||
        (p.hookCount &&
         (p.hooks < p.code || quint64(p.hooks) + p.hookCount * 24 > quint64(p.code) + p.codeSize)))
        throw Error("Attack allocation is outside its declared bounds");
    if (u32(h, 24) != crc32(rom.bytes().mid(p.romOffset, p.size)))
        throw Error("Attack allocation checksum mismatch");
    for (const auto entry : {p.spellIndexEntry, p.amountEntry, p.amountOriginal})
        if (p.codeSize && (entry < p.code || entry >= p.code + p.codeSize || entry % 4))
            throw Error("Attack entry lies outside its code allocation");
    return p;
}
QByteArray AttackPlacement::canonicalOwner(const Rom &rom, const QByteArray &owner, qsizetype start) const {
    QFile file(":/game/attacks/hooks.json");
    if (!file.open(QIODevice::ReadOnly)) throw Error("Missing attack hook catalog");
    const auto catalog = QJsonDocument::fromJson(file.readAll()).object()["hooks"].toArray();
    if (hookCount > quint32(catalog.size())) throw Error("Unknown attack hook directory");
    auto result = owner;
    const auto directory = romOffset + hooks - address;
    for (quint32 i=0;i<hookCount;++i) {
        const auto row = directory+i*24;
        const auto spec = catalog[int(i)].toObject();
        const auto at = quint32(spec["rom"].toInteger());
        if (at < start || at+8 > start+owner.size()) continue;
        const auto words = spec["words"].toArray();
        const auto target = u32(rom.bytes(),row+12);
        if (u32(rom.bytes(),row)!=at || target<code || target>=code+codeSize || (target&3) ||
            u32(rom.bytes(),row+4)!=quint32(words[0].toInteger()) ||
            u32(rom.bytes(),row+8)!=quint32(words[1].toInteger()))
            throw Error("Invalid shared attack owner entry");
        // Export removes hooks before rebuilding other owners; both the
        // installed and already-restored forms must retain their exact words.
        const auto first=u32(result,at-start), second=u32(result,at-start+4);
        if (first==quint32(words[0].toInteger()) && second==quint32(words[1].toInteger())) continue;
        if (first!=(0x08000000U|((target>>2)&0x3ffffff)) || second)
            throw Error("Shared attack owner contains unrecognized code");
        put32(result,at-start,quint32(words[0].toInteger()));
        put32(result,at-start+4,quint32(words[1].toInteger()));
    }
    return result;
}
QByteArray AttackPlacement::descriptor(const QByteArray &data) const {
    if (data.size() != size)
        throw Error("Attack allocation size mismatch");
    QByteArray h(descriptorSize, 0);
    h.replace(0, 8, "LCATK001");
    put32(h, 8, 1);
    put32(h, 12, romOffset);
    put32(h, 16, address);
    put32(h, 20, size);
    put32(h, 24, crc32(data));
    put32(h, 28, records);
    put32(h, 32, settings);
    put32(h, 36, count);
    put32(h, 40, code);
    put32(h, 44, codeSize);
    put32(h, 48, spellIndexEntry);
    put32(h, 52, amountEntry);
    put32(h, 56, amountOriginal);
    put32(h, 60, hooks);
    put32(h, 64, hookCount);
    put32(h, 92, crc32(h.first(92)));
    return h;
}
AttackData AttackData::load(const Rom &rom) {
    auto result = loadRetail(rom);
    const auto p = AttackPlacement::read(rom);
    if (!p)
        return result;
    const auto bytes = rom.bytes().mid(p->romOffset, p->size);
    if (!bytes.startsWith("LCATDATA") || bytes.size() < 64 ||
        (u32(bytes, 8) < 2 || u32(bytes, 8) > 7) || u32(bytes, 12) != quint32(p->count))
        throw Error("Unrecognized attack data");
    const auto size = u32(bytes, 16);
    if (size < namesOffset || size > quint32(bytes.size()))
        throw Error("Attack data lies outside its allocation");
    result.entries_.clear();
    for (int id = 0; id < p->count; ++id) {
        Attack a;
        const int at = settingsOffset + id * 32;
        a.data = bytes.mid(recordOffset + id * 16 + 4, 12);
        a.behavior = u8(bytes, at);
        a.animation = u8(bytes, at + 1);
        a.targetPattern = u8(bytes, at + 2);
        a.targetSide = u8(bytes, at + 3);
        a.targetMask = u16(bytes, at + 4);
        a.strikeCount = u32(bytes, 8) >= 6 && u8(bytes, at + 22) ? u8(bytes, at + 22) : 6;
        a.powerPercent = u16(bytes, at + 6);
        a.status = u8(bytes, at + 8) == 255 ? -1 : u8(bytes, at + 8);
        a.condition = u8(bytes, at + 9);
        a.chance = u8(bytes, at + 10);
        a.afterEffect = u8(bytes, at + 11) == 255 ? -1 : u8(bytes, at + 11);
        a.afterBasis = u8(bytes, at + 12);
        a.playback = u32(bytes, 8) >= 5 ? u8(bytes, at + 13) : 0;
        a.afterPercent = u16(bytes, at + 14);
        a.nameGroup = int(u32(bytes, at + 16));
        if (u8(bytes, at + 20) > 1)
            throw Error("Invalid dynamic attack-name flag");
        a.dynamicName = u8(bytes, at + 20) != 0;
        if (u32(bytes, 8) >= 4 && u32(bytes, at + 28)) {
            const auto pointer = u32(bytes, at + 28);
            if (pointer < p->address + namesOffset || quint64(pointer - p->address) + 16 > size)
                throw Error("Spell array lies outside its allocation");
            const auto offset = pointer - p->address;
            const int count = u16(bytes, offset + 8);
            if (bytes.mid(offset, 8) != "LCATMAP1" || count < 1 || count > 10 ||
                quint64(offset) + 16 + count * 4 > size)
                throw Error("Invalid spell array metadata");
            for (int n = 0; n < count; ++n) {
                const int from = u16(bytes, offset + 16 + n * 4), to = u16(bytes, offset + 18 + n * 4);
                if (a.spellLinks.contains(from)) throw Error("Duplicate spell array entry");
                a.spellLinks[from] = to;
            }
        }
        if (u32(bytes, 8) >= 3 && u8(bytes, at + 21)) {
            const auto pointer = u32(bytes, at + 24);
            if (pointer < p->address + namesOffset || quint64(pointer - p->address) + 32 > size)
                throw Error("Attack effect lies outside its allocation");
            const auto offset = pointer - p->address, length = u32(bytes, offset + 8);
            if (length < 32 || quint64(offset) + length > size)
                throw Error("Attack effect has an invalid extent");
            a.visual = AttackVisual::read(bytes.mid(offset, length));
            if(bytes.mid(offset,8)=="LCFX0003") {
                const auto audioOffset=u32(bytes,offset+16);
                if(u32(bytes,offset+audioOffset+12)!=p->romOffset+offset+audioOffset)
                    throw Error("Imported sound ROM address does not match its allocation");
            }
            validateEffectArtwork(rom, a.visual);
        }
        if (u32(bytes, 8) >= 7) {
            const auto table = u32(bytes, 20);
            if (table < p->address + namesOffset || quint64(table-p->address)+1024 > size)
                throw Error("Battle caption table lies outside its allocation");
            const auto image = u32(bytes, table-p->address+id*4);
            if (image) {
                if (image < table+1024 || quint64(image-p->address)+728 > size ||
                    bytes.mid(image-p->address,8) != QByteArray::fromHex("36340200005a000f"))
                    throw Error("Invalid battle caption image");
                a.caption = QByteArray(1350,0);
                for(int y=0;y<15;++y) for(int x=0;x<90;++x) {
                    const auto pixel=(u8(bytes,image-p->address+8+y*48+x/2)>>(x&1?0:4))&15;
                    if(pixel>3) throw Error("Invalid battle caption color");
                    a.caption[y*90+x]=char(pixel);
                }
            }
        }
        const auto pointer = u32(bytes, recordOffset + id * 16);
        if (a.dynamicName) {
            if (pointer != 0x8018feb8)
                throw Error("Invalid dynamic attack-name pointer");
            a.name = "[Elemental Magic]";
        } else {
            if (pointer < p->address + namesOffset || pointer >= p->address + size)
                throw Error("Attack name lies outside its allocation");
            const auto begin = pointer - p->address;
            const auto end = bytes.indexOf(char(0), begin);
            if (end < begin || end - begin > 128 || quint64(end) >= size)
                throw Error("Unterminated relocated attack name");
            a.name = bytes.mid(begin, end - begin);
        }
        if (id)
            validate(a);
        result.entries_ << a;
    }
    result.apply(result.json()); // Validate name sharing as well as individual rows.
    const auto expected = result.build(p->address,p->romOffset);
    if (bytes.first(expected.size()) != expected)
        throw Error("Attack records disagree with their metadata");
    result.original_ = result.entries_;
    return result;
}
QByteArray AttackData::build(quint32 address, quint32 romOffset) const {
    validateSpellLinks(entries_);
    QByteArray bytes(namesOffset, 0);
    bytes.replace(0, 8, "LCATDATA");
    put32(bytes, 8, 2);
    put32(bytes, 12, quint32(entries_.size()));
    // Every byte-sized index has a bounded record, including unused/sentinel IDs.
    for (int id = 0; id < 256; ++id)
        bytes.replace(recordOffset + id * 16 + 4, 12, entries_.at(0).data);
    QMap<int, quint32> names;
    for (int id = 0; id < entries_.size(); ++id) {
        const auto &a = entries_[id];
        if (id)
            validate(a);
        quint32 pointer = 0x8018feb8;
        if (!a.dynamicName) {
            if (!names.contains(a.nameGroup)) {
                names[a.nameGroup] = address + quint32(bytes.size());
                bytes += a.name + char(0);
            }
            pointer = names[a.nameGroup];
        }
        put32(bytes, recordOffset + id * 16, pointer);
        bytes.replace(recordOffset + id * 16 + 4, 12, a.data);
        const int at = settingsOffset + id * 32;
        put8(bytes, at, a.behavior);
        put8(bytes, at + 1, a.animation);
        put8(bytes, at + 2, a.targetPattern);
        put8(bytes, at + 3, a.targetSide);
        put16(bytes, at + 4, a.targetMask);
        put16(bytes, at + 6, a.powerPercent);
        put8(bytes, at + 8, quint8(a.status));
        put8(bytes, at + 9, a.condition);
        put8(bytes, at + 10, a.chance);
        put8(bytes, at + 11, quint8(a.afterEffect));
        put8(bytes, at + 12, a.afterBasis);
        put8(bytes, at + 13, a.playback);
        put16(bytes, at + 14, a.afterPercent);
        put32(bytes, at + 16, a.nameGroup);
        put8(bytes, at + 20, a.dynamicName ? 1 : 0);
        if (a.targetPattern == 11 || a.strikeCount != 6) {
            put8(bytes, at + 22, a.strikeCount);
            put32(bytes, 8, 6);
        }
    }
    for (int id = int(entries_.size()); id < 256; ++id)
        put32(bytes, recordOffset + id * 16, u32(bytes, recordOffset));
    for (int id = 1; id < entries_.size(); ++id) {
        const auto &links = entries_[id].spellLinks;
        if (links.isEmpty()) continue;
        bytes += QByteArray((16 - bytes.size() % 16) % 16, 0);
        put32(bytes, settingsOffset + id * 32 + 28, address + quint32(bytes.size()));
        QByteArray map(16 + links.size() * 4, 0);
        map.replace(0, 8, "LCATMAP1");put16(map, 8, links.size());
        int n = 16;
        for (auto it = links.cbegin(); it != links.cend(); ++it, n += 4) {
            put16(map, n, it.key());put16(map, n + 2, it.value());
        }
        bytes += map;put32(bytes, 8, qMax(4u, u32(bytes, 8)));
    }
    int visualSlot = 136;
    for (int id = 1; id < entries_.size(); ++id) {
        const auto &a = entries_[id];
        if (!a.visual.modified())
            continue;
        if (visualSlot > 254)
            throw Error("The battle event format supports 119 independently edited attack effects");
        bytes += QByteArray((16 - bytes.size() % 16) % 16, 0);
        const auto at = settingsOffset + id * 32;
        put8(bytes, at + 21, quint8(visualSlot++));
        put32(bytes, at + 24, address + quint32(bytes.size()));
        bytes += a.visual.build(EffectPlayer::retailEffect(a.animation), romOffset+bytes.size());
        put32(bytes, 8, qMax(3u, u32(bytes, 8)));
    }
    for (const auto &attack : entries_)
        if (attack.playback) put32(bytes, 8, qMax(5u, u32(bytes, 8)));
    bool captions=false;
    for(const auto &a:entries_) captions |= !a.caption.isEmpty();
    if(captions) {
        bytes += QByteArray((16-bytes.size()%16)%16,0);
        const auto table=bytes.size();
        put32(bytes,20,address+quint32(table));
        bytes += QByteArray(1024,0);
        for(int id=1;id<entries_.size();++id) if(!entries_[id].caption.isEmpty()) {
            put32(bytes,table+id*4,address+quint32(bytes.size()));
            QByteArray image(728,0);image.replace(0,8,QByteArray::fromHex("36340200005a000f"));
            for(int y=0;y<15;++y) for(int x=0;x<90;++x) {
                const int at=8+y*48+x/2;
                put8(image,at,u8(image,at)|(u8(entries_[id].caption,y*90+x)<<(x&1?0:4)));
            }
            bytes += image;
        }
        put32(bytes,8,7);
    }
    bytes += QByteArray((16 - bytes.size() % 16) % 16, 0);
    put32(bytes, 16, quint32(bytes.size()));
    if (address % 16 || address < 0x80243db0 || quint64(address) + bytes.size() > 0x80400000)
        throw Error("Attack data exceeds the permanent RAM allocation");
    return bytes;
}
AttackData AttackData::loadRetail(const Rom &rom) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Attack editing requires the US revision 0 ROM");
    AttackData result;
    QMap<quint32, int> groups;
    for (int id = 0; id < retailCount; ++id) {
        const auto at = retailOffset + id * 16;
        Attack a;
        a.data = rom.bytes().mid(at + 4, 12);
        a.behavior = a.animation = id;
        const auto pointer = u32(rom.bytes(), at);
        if (!groups.contains(pointer))
            groups[pointer] = int(groups.size()) + 1;
        a.nameGroup = groups[pointer];
        a.dynamicName = pointer == 0x8018feb8;
        if (a.dynamicName)
            a.name = "[Elemental Magic]";
        else {
            if (pointer < 0x8016af80 || pointer >= 0x80190f10)
                throw Error(QString("Attack %1 has an unrecognized name pointer").arg(id));
            const auto begin = pointer - 0x8012a100;
            const auto end = rom.bytes().indexOf(char(0), begin);
            if (end < begin || end - begin > 128)
                throw Error("Unterminated attack name");
            a.name = rom.bytes().mid(begin, end - begin);
        }
        if (id)
            validate(a);
        result.entries_ << a;
    }
    // Native battle captions: packed 2-bit cells, 24-byte rows, 90x15 each.
    // Preserve source artwork; only authored captions get their own allocation.
    const auto key=(u32(rom.bytes(),0x1f0cb0)&65535)*65536u+(u32(rom.bytes(),0x1f0cb8)&65535);
    const auto at=quint32(0x594280)+key;
    const auto sheet=Resource::load(rom,at,u32(rom.bytes(),at)+4).decoded;
    if(sheet.size()<8 || sheet.first(6)!=QByteArray::fromHex("36340204005a") ||
       sheet.size()<8+int(u16(sheet,6))*24)
        throw Error("Unsupported battle caption sheet");
    for(int id=0;id<retailCount;++id) {
        const int slot=u8(rom.bytes(),0x2122a4+id), start=8+slot*15*24;
        if(start+360>sheet.size()) throw Error("Battle caption slot exceeds the sheet");
        QByteArray pixels(1350,0);
        for(int y=0;y<15;++y) for(int x=0;x<90;++x)
            pixels[y*90+x]=char((u8(sheet,start+y*24+x/4)>>(6-2*(x%4)))&3);
        result.nativeCaptions_ << pixels;result.nativeNames_ << result.entries_[id].name;
    }
    auto raw=[&](quint32 key) {const auto at=0x594280+key;return rom.bytes().mid(at+4,u32(rom.bytes(),at));};
    result.captionFont_=raw(0x218c4fa);result.captionWidths_=raw(0x218c450);
    if(result.captionFont_.size()<166*39 || result.captionWidths_.size()<166)
        throw Error("Unsupported ROM lettering");
    // Measured glyph rectangles in the original battle-caption sheet. Read the
    // opened ROM's pixels, not a bundled font or the unrelated dialogue font.
    struct Glyph { char letter; int slot, x, width; };
    const Glyph glyphs[] = {
        {'A',32,16,7},{'B',33,9,6},{'C',4,28,7},{'D',41,19,7},{'E',42,22,6},
        {'F',15,23,6},{'G',56,28,7},{'H',61,27,7},{'I',84,28,1},{'J',27,32,6},
        {'K',22,57,7},{'L',30,4,6},{'M',69,12,9},{'N',44,23,7},{'O',15,43,7},
        {'P',1,33,6},{'Q',64,43,8},{'R',5,33,6},{'S',25,48,6},{'T',0,29,6},
        {'V',53,43,7},{'W',17,20,9},
        {'a',4,44,5},{'b',29,37,5},{'c',1,46,5},{'d',5,52,5},{'e',1,40,5},
        {'f',84,36,3},{'g',30,13,5},{'h',25,55,5},{'i',15,32,1},{'k',1,52,5},
        {'l',15,30,1},{'m',25,17,7},{'n',25,39,5},{'o',25,61,5},{'p',15,34,5},
        {'q',39,44,5},{'r',15,63,4},{'s',9,48,5},{'t',13,56,3},{'u',25,11,5},
        {'v',15,51,5},{'w',25,67,9},{'y',36,57,5},{'z',20,62,4}
    };
    for (const auto &glyph : glyphs) {
        QByteArray pixels(glyph.width * 15, 0);
        for (int x=0; x<glyph.width; ++x) for (int y=0; y<15; ++y) {
            const int column=glyph.x+x, start=8+glyph.slot*360+y*24;
            const int value=(u8(sheet,start+column/4)>>(6-2*(column%4)))&3;
            if (value>1) pixels[x*15+y]=char(value);
        }
        result.battleGlyphs_[glyph.letter]=pixels;
    }
    result.original_ = result.entries_;
    return result;
}
QByteArray AttackData::captionPixels(int id) const {
    const auto &a=entry(id);
    return a.caption.isEmpty()?nativeCaptions_.at(id<retailCount?id:a.animation):a.caption;
}
QByteArray AttackData::generateCaption(const QByteArray &encoded) const {
    const auto text=decodeText(encoded).replace('\n',' ').toLatin1();
    QVector<QByteArray> glyphs;
    int width=0;
    for (char c : text) {
        const int index=quint8(c)+16;
        if(index<48||index>=144) throw Error("Use printable text to generate a caption");
        QByteArray glyph;
        if(c==' ') glyph=QByteArray(3*15,0);
        else if(battleGlyphs_.contains(c)) glyph=battleGlyphs_[c];
        else {
            // Symbols absent from retail captions use the ROM glyph's foreground,
            // fitted to the battle alphabet's cap height and common outline below.
            QImage fallback(11,14,QImage::Format_Grayscale8); fallback.fill(0);
            int left=11,right=-1,top=14,bottom=-1;
            for(int y=0;y<14;++y)for(int x=0;x<11;++x) {
                const int bit=(y*11+x)*2,value=(u8(captionFont_,index*39+bit/8)>>(6-bit%8))&3;
                if(value>1){fallback.scanLine(y)[x]=uchar(value);left=qMin(left,x);right=qMax(right,x);top=qMin(top,y);bottom=qMax(bottom,y);}
            }
            if(right>=left) {
                const int height=std::isalnum(static_cast<unsigned char>(c))?11:bottom-top+1;
                fallback=fallback.copy(left,top,right-left+1,bottom-top+1)
                    .scaled(qMin(8,right-left+1),height,Qt::IgnoreAspectRatio,Qt::FastTransformation);
                glyph=QByteArray(fallback.width()*15,0);
                const int y0=std::isalnum(static_cast<unsigned char>(c))?1:top;
                for(int x=0;x<fallback.width();++x)for(int y=0;y<fallback.height() && y+y0<14;++y)
                    glyph[x*15+y+y0]=char(fallback.constScanLine(y)[x]);
            } else glyph=QByteArray(3*15,0);
        }
        width+=glyph.size()/15+1; glyphs.append(glyph);
    }
    QImage foreground(qMax(1,width-1),15,QImage::Format_Grayscale8); foreground.fill(0);
    int left=0;
    for(const auto &glyph:glyphs) {
        for(int x=0;x<glyph.size()/15;++x)for(int y=0;y<15;++y)
            foreground.scanLine(y)[left+x]=u8(glyph,x*15+y);
        left+=glyph.size()/15+1;
    }
    if(foreground.width()>87)foreground=foreground.scaled(87,15,Qt::IgnoreAspectRatio,Qt::FastTransformation);
    QByteArray pixels(1350,0);const int origin=(90-foreground.width()-3)/2+1;
    auto plot=[&](int x,int y,int value){if(x>=0&&x<90&&y>=0&&y<15)pixels[y*90+x]=char(value);};
    for(int y=0;y<15;++y)for(int x=0;x<foreground.width();++x)if(foreground.constScanLine(y)[x])
        for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
            plot(origin+x+dx,y+dy,1);plot(origin+x+dx+1,y+dy+1,1);
        }
    for(int y=0;y<15;++y)for(int x=0;x<foreground.width();++x)
        if(foreground.constScanLine(y)[x])plot(origin+x,y,foreground.constScanLine(y)[x]);
    return pixels;
}
void AttackData::restoreCaption(int id) {entry(id);entries_[id].caption=id<original_.size()?original_[id].caption:QByteArray{};}
const Attack &AttackData::entry(int id) const {
    if (id < 1 || id >= entries_.size())
        throw Error("Choose an existing attack");
    return entries_[id];
}
void AttackData::validate(const Attack &a) {
    a.visual.validate();
    if (!a.caption.isEmpty() && (a.caption.size()!=1350 || std::any_of(a.caption.begin(),a.caption.end(),[](char p){return quint8(p)>3;})))
        throw Error("A battle caption requires 90x15 pixels using the four native colors");
    if (a.data.size() != 12 || a.name.isEmpty() || a.name.size() > 128 ||
        a.name.contains(char(0)) || a.nameGroup < 1 || a.nameGroup > 65535 || a.behavior < 1 ||
        a.behavior >= retailCount || a.animation < 1 || a.animation >= retailCount)
        throw Error("Invalid attack record or source behavior");
    if (a.targetPattern < 0 || a.targetPattern > 12 ||
        a.targetSide < 0 || a.targetSide > 3 || a.targetMask < 0 ||
        a.targetMask > 511 || (a.targetPattern == 12 && !a.targetMask))
        throw Error("Choose a valid target pattern with at least one position");
    if (a.strikeCount < 1 || a.strikeCount > maximumStrikes)
        throw Error(QString("Choose between 1 and %1 strikes").arg(maximumStrikes));
    if (a.powerPercent < 0 || a.powerPercent > 1000 ||
        (a.status != -1 && !statuses().contains(a.status)) || a.condition < 0 || a.condition > 2 ||
        a.chance < 0 || a.chance > 100 || a.afterEffect < -1 || a.afterEffect > 2 ||
        a.afterBasis < 0 || a.afterBasis > 3 || a.afterPercent < 0 || a.afterPercent > 1000)
        throw Error("Attack effect setting is outside its supported range");
    if (a.status == 7 && a.condition != 2)
        throw Error("Undead slaying requires a successful killing blow");
    if (a.playback < 0 || a.playback > 1)
        throw Error("Choose a supported attack playback mode");
    const auto nativeSpells = templateTargets(a.behavior).values();
    for (auto it = a.spellLinks.cbegin(); it != a.spellLinks.cend(); ++it)
        if (!nativeSpells.contains(it.key()) || it.value() < 1 || it.value() >= maximumCount)
            throw Error("Invalid spell array entry");
}
void AttackData::set(int id, const Attack &a) {
    entry(id);
    validate(a);
    int visualCount = a.visual.modified();
    for (int i = 1; i < entries_.size(); ++i)
        if (i != id)
            visualCount += entries_[i].visual.modified();
    if (visualCount > 119)
        throw Error("All 119 independent attack effect slots are in use");
    for (int i = 1; i < entries_.size(); ++i)
        if (i != id && entries_[i].nameGroup == a.nameGroup &&
            (entries_[i].name != a.name || entries_[i].dynamicName != a.dynamicName))
            throw Error("Shared attack names must be changed together or made independent");
    auto next = entries_;next[id] = a;
    validateSpellLinks(next);
    entries_ = std::move(next);
}
int AttackData::nextNameGroup() const {
    int next = 1;
    for (const auto &a : entries_)
        next = qMax(next, a.nameGroup + 1);
    if (next > 65535)
        throw Error("Too many independent name revisions in this project");
    return next;
}
QVector<int> AttackData::sharingName(int id) const {
    const auto group = entry(id).nameGroup;
    QVector<int> result;
    for (int i = 1; i < entries_.size(); ++i)
        if (entries_[i].nameGroup == group)
            result << i;
    return result;
}
void AttackData::rename(int id, const QString &name, bool shared) {
    auto a = entry(id);
    a.name = attackName(name);
    a.dynamicName = false;
    if (!shared)
        a.nameGroup = nextNameGroup();
    validate(a);
    const auto users = shared ? sharingName(id) : QVector<int>{id};
    for (int user : users) {
        entries_[user].name = a.name;
        entries_[user].nameGroup = a.nameGroup;
        entries_[user].dynamicName = false;
        entries_[user].caption=generateCaption(a.name);
    }
}
int AttackData::duplicate(int source, const QString &name) {
    auto a = entry(source);
    if (entries_.size() >= maximumCount)
        throw Error("All 254 attack IDs are in use");
    if (a.visual.modified()) {
        int edited = 0;
        for (const auto &entry : entries_)
            edited += entry.visual.modified();
        if (edited >= 119)
            throw Error("All 119 independent attack effect slots are in use");
    }
    a.name = attackName(name);
    a.nameGroup = nextNameGroup();
    a.dynamicName = false;
    a.caption=generateCaption(a.name);
    validate(a);
    entries_ << a;
    return int(entries_.size()) - 1;
}
int AttackData::create(const QString &name) {
    // Cleave is a valid ordinary attack with complete native artwork and execution.
    return duplicate(5, name);
}
QJsonObject AttackData::json() const {
    QJsonArray rows;
    for (int i = 1; i < entries_.size(); ++i) {
        const auto &a = entries_[i];
        QJsonObject links;
        for (auto it = a.spellLinks.cbegin(); it != a.spellLinks.cend(); ++it)
            links[QString::number(it.key())] = it.value();
        rows << QJsonObject{{"data", QString::fromLatin1(a.data.toHex())},
                            {"name", QString::fromLatin1(a.name.toHex())},
                            {"caption",QString::fromLatin1(a.caption.toHex())},
                            {"nameGroup", a.nameGroup},
                            {"dynamicName", a.dynamicName},
                            {"behavior", a.behavior},
                            {"animation", a.animation},
                            {"targetPattern", a.targetPattern},
                            {"targetSide", a.targetSide},
                            {"targetMask", a.targetMask},
                            {"strikeCount", a.strikeCount},
                            {"playback", a.playback},
                            {"powerPercent", a.powerPercent},
                            {"status", a.status},
                            {"condition", a.condition},
                            {"chance", a.chance},
                            {"afterEffect", a.afterEffect},
                            {"afterBasis", a.afterBasis},
                            {"afterPercent", a.afterPercent},
                            {"visual", a.visual.json()}, {"spellLinks", links}};
    }
    return {{"version", 1}, {"entries", rows}};
}
void AttackData::apply(const QJsonObject &object) {
    if (integer(object, "version", 0) != 1 || !object["entries"].isArray())
        throw Error("Unsupported attack data version");
    const auto rows = object["entries"].toArray();
    if (rows.size() < retailCount - 1 || rows.size() >= maximumCount)
        throw Error("Attack table must contain 158–254 records");
    QVector<Attack> next{entries_.at(0)};
    QMap<int, QPair<QByteArray, bool>> groups;
    for (const auto &value : rows) {
        if (!value.isObject())
            throw Error("Invalid attack entry");
        const auto row = value.toObject();
        Attack a;
        auto decode = [&](const char *key) {
            const auto text = row[key].toString().toLatin1();
            const auto bytes = QByteArray::fromHex(text);
            if (bytes.toHex() != text.toLower())
                throw Error("Invalid encoded attack data");
            return bytes;
        };
        a.data = decode("data");
        a.name = decode("name");
        if(row.contains("caption")) a.caption=decode("caption");
        a.nameGroup = integer(row, "nameGroup", 0);
        if (!row["dynamicName"].isBool())
            throw Error("Invalid dynamic attack name");
        a.dynamicName = row["dynamicName"].toBool();
        a.behavior = integer(row, "behavior", 0);
        a.animation = integer(row, "animation", 0);
        a.targetPattern = integer(row, "targetPattern", 0);
        a.targetSide = integer(row, "targetSide", 0);
        a.targetMask = integer(row, "targetMask", 0);
        a.strikeCount = integer(row, "strikeCount", 6);
        a.playback = integer(row, "playback", 0);
        a.powerPercent = integer(row, "powerPercent", 100);
        a.status = integer(row, "status", -1);
        a.condition = integer(row, "condition", 1);
        a.chance = integer(row, "chance", 100);
        a.afterEffect = integer(row, "afterEffect", -1);
        a.afterBasis = integer(row, "afterBasis", 0);
        a.afterPercent = integer(row, "afterPercent", 100);
        if (row.contains("spellLinks")) {
            if (!row["spellLinks"].isObject()) throw Error("Invalid spell array");
            const auto links = row["spellLinks"].toObject();
            for (auto it = links.begin(); it != links.end(); ++it) {
                bool ok = false;const int from = it.key().toInt(&ok);
                if (!ok || QString::number(from) != it.key()) throw Error("Invalid spell array slot");
                a.spellLinks[from] = integer(links, it.key().toLatin1().constData(), 0);
            }
        }
        if (row.contains("visual")) {
            if (!row["visual"].isObject())
                throw Error("Invalid attack visual settings");
            a.visual = AttackVisual::fromJson(row["visual"].toObject());
        }
        // Older projects stored only names. Repair their missing caption on import.
        if(!row.contains("caption") && !a.dynamicName &&
           (next.size()>=nativeNames_.size() || a.name!=nativeNames_[next.size()]))
            a.caption=generateCaption(a.name);
        validate(a);
        const auto name = qMakePair(a.name, a.dynamicName);
        if (groups.contains(a.nameGroup) && groups[a.nameGroup] != name)
            throw Error("Inconsistent shared attack names");
        groups[a.nameGroup] = name;
        next << a;
    }
    int edited = 0;
    for (const auto &a : next)
        edited += a.visual.modified();
    if (edited > 119)
        throw Error("The battle event format supports 119 independently edited attack effects");
    validateSpellLinks(next);
    entries_ = std::move(next);
}
QMap<int, int> AttackData::resolvedTargets(int id) const {
    const auto &a = entry(id);
    auto result = templateTargets(a.behavior);
    for (auto it = result.begin(); it != result.end(); ++it)
        it.value() = a.spellLinks.value(it.value(), it.value());
    return result;
}
bool AttackData::isTemplate(int id) {
    return (id >= 45 && id <= 54) || id == 145;
}
QMap<int, int> AttackData::templateTargets(int id) {
    // Accepted func_0004440c, resident element maps at z64 0x6136c.
    if (id >= 45 && id <= 48) {
        const int targets[4][6] = {{55, 63, 71, 79, 87, 91},
                                   {57, 65, 73, 81, 89, 93},
                                   {59, 67, 75, 83, 87, 95},
                                   {60, 68, 76, 84, 154, 96}};
        QMap<int, int> result;
        for (int i = 0; i < 6; ++i)
            result[i + 1] = targets[id - 45][i];
        for (int item = 249; item <= 252; ++item)
            result[-item] = 99 + item - 249;
        return result;
    }
    if (id == 145)
        return {{0, 146}, {1, 147}, {2, 148}, {3, 149}, {4, 150}, {5, 151}, {6, 152}};
    if (id >= 51 && id <= 54)
        return {{-1, QVector<int>{55, 87, 89, 95}[id - 51]},
                {-249, 99},
                {-250, 100},
                {-251, 101},
                {-252, 102}};
    if (id == 49 || id == 50)
        return {{1, 57}, {2, 65}, {3, 73}, {4, 81}, {6, 93}};
    return {};
}
QString AttackData::displayName(const Attack &a) {
    return decodeText(a.name).simplified();
}
QMap<int, QString> AttackData::elements() {
    return {{0, "Physical"},
            {1, "Wind"},
            {2, "Flame"},
            {3, "Earth"},
            {4, "Water"},
            {5, "Virtue"},
            {6, "Bane"},
            {7, "No elemental resistance (ID 7)"},
            {8, "Resolved by template"},
            {9, "Wind + Water"},
            {10, "Wind + Flame"},
            {11, "Flame + Earth"},
            {12, "Earth + Water"},
            {13, "Wind + Bane"},
            {14, "Flame + Bane"},
            {15, "From equipment"},
            {255, "From equipment (alternate ID)"}};
}
QMap<int, QString> AttackData::effects() {
    return {{0, "No main effect"},
            {1, "Damage"},
            {2, "Healing"},
            {3, "Petrification"},
            {4, "Sleep"},
            {5, "Power Up"},
            {6, "Power Down"},
            {8, "Paralysis"},
            {10, "Poison"},
            {11, "Remove ailments"},
            {16, "Death (HP to zero)"}};
}
QMap<int, QString> AttackData::calculations() {
    return {{0, "Ordinary calculation (ID 0)"},
            {1, "Ordinary calculation"},
            {2, "Half calculated amount"},
            {3, "Attacker current HP / 20"},
            {4, "Target current HP / 2"},
            {5, "Incoming power / 6"},
            {6, "Amount supplied by the preceding effect"},
            {7, "Enhanced incoming power (×1.5)"},
            {8, "Enhanced incoming power (×1.6)"},
            {9, "Attacker current HP / 2"},
            {10, "80% of calculated amount"}};
}
QMap<int, QString> AttackData::statuses() {
    return {{0, "None"},     {3, "Petrification"},    {4, "Sleep"},
            {5, "Power Up"}, {6, "Power Down"},       {7, "Undead slaying (killing blow)"}, {8, "Paralysis"},
            {10, "Poison"},  {11, "Remove ailments"}, {16, "Death (HP to zero)"}};
}
} // namespace lordly
