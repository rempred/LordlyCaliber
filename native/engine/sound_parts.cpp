#include "engine/sound_parts.h"
#include "engine/sound_preview.h"
#include <QSet>
namespace lordly {
namespace {
constexpr quint32 nativeFx = 0x8010a760, nativeSamples = 0x80104710;
void checkState(const QByteArray &s) {
    if (s.isEmpty()) return;
    if (s.size() != soundPartStateSize || u32(s, 0x13c) >= 0xde70 ||
        !u32(s, 0x140) || u32(s, 0x140) > 4096 || u32(s, 0x144) > 900)
        throw Error("Invalid isolated sound part");
    for (int at : {4,8,0x34,0x38,0x74,0x78,0x7c,0x80,0x84,0x88,0x8c,
                   0xe0,0xe4,0xe8,0xec,0xf0,0xf4,0xf8,0xfc,0x100,0x104,0x108,0x10c}) {
        const auto p = u32(s,at);
        if (p && !(p >= nativeFx && p < nativeFx + 0xde70) &&
            !(p >= nativeSamples && p < nativeSamples + 0x5ce0))
            throw Error("Native sound part contains a foreign resource reference");
    }
    if (u32(s,0x74) || u32(s,0x84) || u8(s,0xdb) > 4)
        throw Error("This part is not an effect-bank voice");
}
void align(QByteArray &b) { b += QByteArray((16 - b.size() % 16) % 16, 0); }
}
QByteArray soundPartState(const QJsonObject &o, const char *field) {
    if (!o.contains(field)) return {};
    if (!o[field].isString()) throw Error("Invalid sound part reference");
    const auto b = QByteArray::fromBase64Encoding(o[field].toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (!b) throw Error("Invalid sound part reference encoding");
    checkState(b.decoded); return b.decoded;
}
void validateSoundParts(const AttackSound &s) {
    static thread_local int depth = 0;
    if (depth >= 8) throw Error("Sound replacement nesting exceeds eight levels");
    ++depth; struct Guard { int &n; ~Guard() { --n; } } guard{depth};
    if (s.enemyRequest < -1 || s.enemyRequest >= 740 || s.parts.size() > 64)
        throw Error("Invalid sound part list");
    checkState(s.state); checkState(s.enemyState);
    if (!s.sequence.isEmpty() && (s.sequence.size() != 0xde70 || u32(s.sequence,0) != 1798 ||
        u32(s.sequence,4) != 740 || u32(s.sequence,8) != 258 || u32(s.sequence,20) != 0xdc6a))
        throw Error("Invalid original effect bank for private sound parts");
    if (s.hasParts() && s.pcm.isEmpty() && s.sequence.isEmpty())
        throw Error("Native sound parts require their original effect bank");
    QSet<int> indices;
    for (const auto &v : s.parts) {
        if (!v.isObject()) throw Error("Invalid sound part edit");
        const auto p = v.toObject(); const int index = p["index"].toInt(-1), tick = p["tick"].toInt(-2);
        if (index < 0 || index > 4095 || indices.contains(index) || tick < -1 || tick > 1800 ||
            p["index"].toDouble() != index || p["tick"].toDouble() != tick)
            throw Error("Invalid sound part or timing");
        indices.insert(index);
        const auto a = soundPartState(p,"original"), b = soundPartState(p,"enemyOriginal");
        if (s.state.isEmpty() && s.pcm.isEmpty() && a.isEmpty())
            throw Error("Sound part edits require their native reference");
        if (!p["muted"].isBool() || !p["source"].isObject()) throw Error("Invalid replacement sound");
        AttackSound::fromJson(p["source"].toObject());
    }
}
int soundImportBytes(const AttackSound &s) {
    int result = s.pcm.size();
    for (const auto &v : s.parts) result += soundImportBytes(AttackSound::fromJson(v.toObject()["source"].toObject()));
    return result;
}
QByteArray buildSoundSource(const AttackSound &s, quint32 romOffset) {
    s.validate();
    // LCSND001: header64; private FX at64 for carrier/isolated kinds.
    // Kind0 native root, 1 isolated note, 2 PCM, 3 edited native root,
    // 4 edited single source (no carrier). Child entries are 32 bytes.
    const int kind = !s.parts.isEmpty() ? (s.state.isEmpty() && s.pcm.isEmpty() ? 3 : 4)
                     : !s.state.isEmpty() ? 1 : !s.pcm.isEmpty() ? 2 : 0;
    QByteArray b(64,0); b.replace(0,8,"LCSND001");
    put32(b,12,romOffset); put32(b,16,kind);
    put32(b,20,s.request); put32(b,24,s.enemyRequest >= 0 ? s.enemyRequest : s.request);
    if (kind == 3 || kind == 0) {
        if(s.sequence.isEmpty())throw Error("Copying a native sound requires its source bank");
        b += s.sequence;
    }
    else if (kind == 1) {
        QByteArray fx(0x260,0); put32(fx,0,1);put32(fx,4,1);put32(fx,8,258);
        put32(fx,20,0x40);put32(fx,24,0x30);put32(fx,28,100);put8(fx,0x30,0x80);
        fx.replace(0x40,516,s.sequence.mid(0xdc6a,516)); b += fx;
        put32(b,28,b.size()); b += s.state;
        put32(b,32,b.size()); b += s.enemyState.isEmpty() ? s.state : s.enemyState;
    } else if (kind == 2) {
        auto plain = s; plain.parts = {}; plain.state.clear(); plain.enemyState.clear();
        b += plain.bank(romOffset+64);
    }
    align(b);
    if (kind == 1 || kind == 3) {
        // A private silent raw16 wave suppresses only a replaced voice start.
        put32(b,44,b.size());
        const int wave = b.size(); b += QByteArray(80,0);
        put32(b,wave,romOffset+wave+32);put32(b,wave+4,32);put8(b,wave+8,1);
        put32(b,wave+64,0);put32(b,wave+68,16);put32(b,wave+72,0xffffffff);
    }
    put32(b,36,s.parts.size()); put32(b,40,b.size());
    const int entries = b.size(); b += QByteArray(s.parts.size()*32,0);
    for (int i=0;i<s.parts.size();++i) {
        const auto p=s.parts[i].toObject();const int row=entries+i*32;
        const auto a=soundPartState(p,"original"), e=soundPartState(p,"enemyOriginal");
        const int tick=p["tick"].toInt();put32(b,row,p["index"].toInt());
        put32(b,row+4,tick>=0?tick:a.isEmpty()?0:u32(a,0x144));
        put32(b,row+8,kind==3 && e.isEmpty()?0xffffffff:tick>=0?tick:e.isEmpty()?0:u32(e,0x144));
        put32(b,row+12,a.isEmpty()?0xffffffff:u32(a,0x13c));put32(b,row+16,a.isEmpty()?0:u32(a,0x140));
        put32(b,row+20,e.isEmpty()?0xffffffff:u32(e,0x13c));put32(b,row+24,e.isEmpty()?0:u32(e,0x140));
        if (!p["muted"].toBool()) {
            put32(b,row+28,b.size());b += buildSoundSource(AttackSound::fromJson(p["source"].toObject()),romOffset+b.size());
        }
    }
    align(b);put32(b,8,b.size());return b;
}
namespace {
AttackSound prepared(const Rom &rom,AttackSound s) {
    s.anchor=-1;s.delay=0;
    if(s.sequence.isEmpty() && s.pcm.isEmpty())s.sequence=rom.bytes().mid(0x4e3140,0xde70);
    if(s.cue>=0){s.request=u16(rom.bytes(),0x2123a0+s.cue*4+2);s.enemyRequest=u16(rom.bytes(),0x2123a0+s.cue*4);}
    return s;
}
QJsonObject editRecord(const Rom &rom,AttackSound &sound,int index) {
    for(const auto &v:sound.parts)if(v.toObject()["index"].toInt()==index)return v.toObject();
    auto base=sound;base.parts={};
    const auto source=isolateSoundPart(rom,base,index);
    QJsonObject p{{"index",index},{"tick",-1},{"muted",false},{"source",source.json()}};
    if(base.state.isEmpty() && base.pcm.isEmpty()) {
        const auto a=renderAuthoredSound(rom,base,1).parts,e=renderAuthoredSound(rom,base,0).parts;
        p["original"]=QString::fromLatin1(a[index].state.toBase64());
        if(index<e.size())p["enemyOriginal"]=QString::fromLatin1(e[index].state.toBase64());
    }
    if(sound.sequence.isEmpty() && sound.pcm.isEmpty())sound.sequence=rom.bytes().mid(0x4e3140,0xde70);
    if(sound.cue>=0){sound.request=u16(rom.bytes(),0x2123a0+sound.cue*4+2);sound.enemyRequest=u16(rom.bytes(),0x2123a0+sound.cue*4);}
    return p;
}
void saveEdit(AttackSound &sound,const QJsonObject &p) {
    for(int i=0;i<sound.parts.size();++i)if(sound.parts[i].toObject()["index"]==p["index"]){sound.parts[i]=p;sound.validate();return;}
    sound.parts.append(p);sound.validate();
}
}
AttackSound isolateSoundPart(const Rom &rom,const AttackSound &sound,int index) {
    for(const auto &v:sound.parts)if(v.toObject()["index"].toInt()==index)
        return prepared(rom,AttackSound::fromJson(v.toObject()["source"].toObject()));
    auto base=sound;base.parts={};
    if(!base.state.isEmpty() || !base.pcm.isEmpty()) {
        if(index!=0)throw Error("This sound contains one part");return prepared(rom,base);
    }
    const auto a=renderAuthoredSound(rom,base,1).parts,e=renderAuthoredSound(rom,base,0).parts;
    if(index<0 || index>=a.size() || a[index].state.isEmpty())throw Error("Cannot find this native sound part");
    auto result=prepared(rom,base);result.state=a[index].state;
    result.enemyState=index<e.size()?e[index].state:a[index].state;
    result.name=QString("%1 · part %2").arg(sound.name).arg(index+1);result.validate();return result;
}
void replaceSoundPart(const Rom &rom,AttackSound &s,int index,const AttackSound &source) {
    auto next=s;auto p=editRecord(rom,next,index);p["source"]=prepared(rom,source).json();p["muted"]=false;saveEdit(next,p);s=std::move(next);
}
void setSoundPartTime(const Rom &rom,AttackSound &s,int index,int tick) {
    auto next=s;auto p=editRecord(rom,next,index);p["tick"]=tick;saveEdit(next,p);s=std::move(next);
}
void muteSoundPart(const Rom &rom,AttackSound &s,int index) {
    auto next=s;auto p=editRecord(rom,next,index);p["muted"]=true;saveEdit(next,p);s=std::move(next);
}
}
