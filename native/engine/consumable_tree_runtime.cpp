#include "engine/consumable_tree_runtime.h"
#include "engine/class_data.h"
#include "engine/promotion.h"
#include "engine/capacity_placement.h"
#include <QJsonDocument>
namespace lordly {
QByteArray ConsumableTreePlacement::descriptor() const {
    QByteArray h(descriptorSize,0);h.replace(0,8,"LCITEMT1");put32(h,8,1);put32(h,12,offset);put32(h,16,address);put32(h,20,size);put32(h,24,crc32(data));put32(h,60,crc32(h.first(60)));return h;
}
std::optional<ConsumableTreePlacement> ConsumableTreePlacement::read(const Rom &rom){
    const auto h=rom.bytes().mid(descriptorOffset,descriptorSize);
    if(!h.startsWith("LCITEMT1"))return {};
    if(h.size()!=64||u32(h,8)!=1||u32(h,60)!=crc32(h.first(60)))throw Error("Damaged item-effect tree descriptor");
    ConsumableTreePlacement p;p.offset=u32(h,12);p.address=u32(h,16);p.size=u32(h,20);
    const auto classes=ClassPlacement::read(rom);
    if(!classes||p.offset<0x2800000||p.size<256||p.size>0x40000||p.address<classes->layout.indexBase||quint64(p.address)+p.size>classes->layout.end||p.offset!=classes->dataRomOffset+p.address-classes->layout.indexBase||quint64(p.offset)+p.size>quint64(rom.bytes().size()))throw Error("Item-effect trees are outside their allocated data");
    p.data=rom.bytes().mid(p.offset,p.size);
    if(crc32(p.data)!=u32(h,24)||!p.data.startsWith("LCITEMD1"))throw Error("Damaged item-effect tree runtime");
    const auto json=u32(p.data,8),length=u32(p.data,12);p.directory=u32(p.data,16);p.count=u32(p.data,20);
    if(json<256||quint64(json)+length>p.size||p.count!=6||p.directory<256||quint64(p.directory)+p.count*28>p.size)throw Error("Invalid item-effect tree directory");
    QJsonParseError error;p.model=QJsonDocument::fromJson(p.data.mid(json,length),&error).object();
    if(error.error!=QJsonParseError::NoError||p.model.isEmpty())throw Error("Invalid item-effect tree model");
    p.remove(rom);return p;
}
QByteArray ConsumableTreePlacement::remove(const Rom &rom) const{
    auto image=rom.bytes();
    const quint32 expected[]={0x41098,0xc4650,0xc9850,0x1ad8cc,0x1ad0e4,0x1a9290};
    for(quint32 i=0;i<count;i++){
        const auto row=directory+i*28,at=u32(data,row),extent=u32(data,row+4),target=u32(data,row+8);
        if(at!=expected[i]||extent<8||quint64(at)+extent>quint64(image.size())||target<address||target>=address+size||target%4||u32(image,at)!=(0x08000000U|((target>>2)&0x3ffffff))||u32(image,at+4))throw Error("Item-effect entry changed outside its runtime");
        image.replace(at,8,data.mid(row+12,8));
    }
    const auto restored=Rom::fromBytes(image);const auto capacity=CapacityPlacement::read(restored);
    const auto canonical=capacity?capacity->canonical(restored).bytes():image;
    for(quint32 i=0;i<count;i++){const auto row=directory+i*28;if(crc32(canonical.mid(u32(data,row),u32(data,row+4)))!=u32(data,row+20))throw Error("An item-effect owner changed outside its runtime");}
    image.replace(descriptorOffset,descriptorSize,QByteArray(descriptorSize,char(-1)));return image;
}
}
