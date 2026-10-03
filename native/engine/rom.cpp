#include "engine/rom.h"
#include <QCryptographicHash>
#include <QFile>
#include <algorithm>
#include <bit>
#include <limits>

namespace lordly {
static void bounds(const QByteArray& data, qsizetype offset, qsizetype count) {
    if (offset < 0 || count < 0 || offset > data.size() || count > data.size() - offset)
        throw Error(QString("Data access outside resource at 0x%1 (%2 bytes)").arg(offset, 0, 16).arg(count));
}
quint8 u8(const QByteArray& d, qsizetype p) { bounds(d,p,1); return static_cast<quint8>(d[p]); }
quint16 u16(const QByteArray& d, qsizetype p) { return (quint16(u8(d,p)) << 8) | u8(d,p+1); }
quint32 u32(const QByteArray& d, qsizetype p) { return (quint32(u16(d,p)) << 16) | u16(d,p+2); }
void put8(QByteArray& d, qsizetype p, quint8 v) { bounds(d,p,1); d[p]=char(v); }
void put16(QByteArray& d, qsizetype p, quint16 v) { bounds(d,p,2); put8(d,p,quint8(v>>8)); put8(d,p+1,quint8(v)); }
void put32(QByteArray& d, qsizetype p, quint32 v) { bounds(d,p,4); put16(d,p,quint16(v>>16)); put16(d,p+2,quint16(v)); }
QByteArray sha256(const QByteArray& d) { return QCryptographicHash::hash(d,QCryptographicHash::Sha256).toHex(); }

QByteArray decodeLzss(const QByteArray& data, qsizetype size, qsizetype* consumed) {
    if (size < 0 || size > 64*1024*1024) throw Error("Invalid decoded resource size");
    QByteArray output; output.reserve(size);
    qsizetype pos=0;
    auto next=[&]() { return u8(data,pos++); };
    auto repeat=[&](quint8 value, qsizetype count) {
        count=std::min(count,size-output.size()); output.append(count,char(value));
    };
    auto copy=[&](qsizetype distance, qsizetype count) {
        count=std::min(count,size-output.size());
        for(qsizetype i=0;i<count;++i) {
            auto from=output.size()-distance;
            output.append(from<0 ? char(0) : output[from]);
        }
    };
    while(output.size()<size) {
        auto b=next();
        if(b>=0x80) { auto b2=next(); copy(((b&7)<<8 | b2)+1,((b>>3)&15)+3); }
        else if(b>=0x40) {
            qsizetype count=(b&63)+1; bounds(data,pos,count);
            output.append(data.constData()+pos,std::min(count,size-output.size())); pos+=count;
        } else if(b>=0x20) repeat(0,(b&31)+2);
        else if(b>=0x10) { auto b2=next(); auto b3=next(); copy(((b2&63)<<8 | b3)+1,((b&15)|((b2&192)>>2))+4); }
        else if(b==0) { auto n=next(); auto hi=next(); auto lo=next(); copy((hi<<8 | lo)+1,n+5); }
        else if(b==1 || b==2) { auto n=next(); repeat(b==1?255:0,n+3); }
        // 0x03..0x0f are native no-ops. The input bound still guarantees termination.
    }
    if(consumed) *consumed=pos;
    return output;
}
QByteArray encodeLzss(const QByteArray& data) {
    // Deterministic valid streams. Runs are compressed; literal spans never exceed 64 bytes.
    QByteArray out;
    for(qsizetype i=0;i<data.size();) {
        quint8 value=u8(data,i); qsizetype run=1;
        while(i+run<data.size() && run<258 && u8(data,i+run)==value) ++run;
        if((value==0 || value==255) && run>=3) {
            out.append(char(value==0?2:1)); out.append(char(run-3)); i+=run;
        } else {
            qsizetype start=i++;
            while(i<data.size() && i-start<64) {
                auto c=u8(data,i);
                if((c==0 || c==255) && i+2<data.size() && u8(data,i+1)==c && u8(data,i+2)==c) break;
                ++i;
            }
            out.append(char(0x40+(i-start)-1)); out.append(data.constData()+start,i-start);
        }
    }
    return out;
}
Rom Rom::fromBytes(QByteArray bytes,const QString& sourcePath) {
    if(bytes.size()<64 || bytes.size()%4) throw Error("ROM must contain a complete N64 header and whole 32-bit words");
    Rom rom; rom.path_=sourcePath;
    switch(u32(bytes,0)) {
        case 0x80371240: rom.order_=ByteOrder::BigEndian; break;
        case 0x37804012: rom.order_=ByteOrder::ByteSwapped; for(qsizetype i=0;i<bytes.size();i+=2) std::swap(bytes[i],bytes[i+1]); break;
        case 0x40123780: rom.order_=ByteOrder::LittleEndian; for(qsizetype i=0;i<bytes.size();i+=4) { std::swap(bytes[i],bytes[i+3]); std::swap(bytes[i+1],bytes[i+2]); } break;
        default: throw Error("Unrecognized N64 ROM byte order");
    }
    rom.bytes_=std::move(bytes); return rom;
}
Rom Rom::open(const QString& path) {
    QFile file(path); if(!file.open(QIODevice::ReadOnly)) throw Error(file.errorString());
    if(file.size()>128*1024*1024) throw Error("ROM exceeds the supported 128 MiB file limit");
    auto bytes=file.readAll(); if(bytes.size()!=file.size()) throw Error("Incomplete ROM read");
    return fromBytes(std::move(bytes),file.fileName());
}
QString Rom::extension() const { return order_==ByteOrder::ByteSwapped?"v64":order_==ByteOrder::LittleEndian?"n64":"z64"; }
QString Rom::title() const { return QString::fromLatin1(bytes_.mid(0x20,20)).trimmed(); }
int Rom::revision() const { return u8(bytes_,0x3f); }
void Rom::requireSupported() const {
    if(bytes_.size()<0x2800000 || bytes_.mid(0x3c,3)!="NOE" || revision()>1)
        throw Error("This editor currently supports US Ogre Battle 64 revisions 0 and 1");
}
QByteArray Rom::serialize(const QByteArray& normalized) const {
    if(normalized.size()%4) throw Error("Export is not word aligned");
    QByteArray out=normalized;
    if(order_==ByteOrder::ByteSwapped) for(qsizetype i=0;i<out.size();i+=2) std::swap(out[i],out[i+1]);
    if(order_==ByteOrder::LittleEndian) for(qsizetype i=0;i<out.size();i+=4) { std::swap(out[i],out[i+3]); std::swap(out[i+1],out[i+2]); }
    return out;
}
void Rom::updateChecksum(QByteArray& data) {
    bounds(data,0x1000,0x100000);
    quint32 t1=0xf8ca4ddc,t2=t1,t3=t1,t4=t1,t5=t1,t6=t1;
    for(qsizetype i=0x1000;i<0x101000;i+=4) {
        auto d=u32(data,i); auto sum=t6+d; if(sum<t6) ++t4; t6=sum; t3^=d;
        auto r=std::rotl(d,int(d&31)); t5+=r; t2^=t2>d?r:t6^d; t1+=t5^d;
    }
    put32(data,0x10,t6^t4^t3); put32(data,0x14,t5^t2^t1);
}
}
