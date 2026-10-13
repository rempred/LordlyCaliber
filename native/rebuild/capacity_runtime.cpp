#include "rebuild/capacity_runtime.h"
#include "engine/capacity_placement.h"
#include "engine/scenario_member_runtime.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

namespace lordly {
namespace {
QByteArray source(const QString &name) {
    QFile f(":/game/" + name);
    if (!f.open(QIODevice::ReadOnly)) throw Error("Missing capacity source: " + name);
    return f.readAll();
}
QJsonObject catalog(const QString &name) {
    const auto doc = QJsonDocument::fromJson(source("scenarios/" + name + ".json"));
    if (!doc.isObject()) throw Error("Invalid capacity catalog: " + name);
    return doc.object();
}
struct Allocation { const char *group; const char *symbol; quint32 old, size, offset; };
constexpr Allocation arrays[] = {
    {"characters", "lc_enemy_characters", 0x80195560, 0x1450, 0},
    {"soldier_hp", "lc_enemy_soldier_hp", 0x8019532c, 1, 0x3500},
    {"soldier_state", "lc_enemy_soldier_state", 0x80195480, 1, 0x3700},
    {"sources", "lc_capacity_sources", 0x801971f0, 1250, 0x3800},
    {"active_sources", "lc_capacity_active_sources", 0x801977d8, 50, 0x3e00},
    {"objects", "lc_capacity_objects", 0x801f1080, 50*192, 0x3e40},
    {"pointers", "lc_capacity_pointers", 0x801f0cb0, 50*4, 0x6c00},
    {"availability", "lc_capacity_availability", 0x801f0bd8, 200, 0x6d00},
    {"drops", "lc_capacity_drops", 0x801936b8, 40, 0x6e80},
    {"backups", "lc_capacity_backups", 0x801910f0, 50*192, 0x6f00},
};
quint32 groupBase(const CapacityRuntimeBuild &b, const QString &name) {
    if (name == "drops_source_index") return b.base + 0x6e80 - 60;
    for (const auto &a : arrays) if (name == a.group) return b.base + a.offset;
    throw Error("Unknown capacity allocation " + name);
}
quint32 number(QJsonValue value) { return quint32(value.toInteger()); }
void word(CapacityRuntimeBuild &b, quint32 site, quint32 before, quint32 after, bool replace=false) {
    if (b.expected.contains(site) && b.expected[site] != before)
        throw Error("Conflicting capacity guards at z64 " + QString::number(site,16));
    if (!replace && b.words.contains(site) && b.words[site] != after)
        throw Error("Conflicting capacity patches at z64 " + QString::number(site,16));
    b.expected[site] = before; b.words[site] = after;
}
void pair(CapacityRuntimeBuild &b, quint32 hi, quint32 lo, quint32 hw, quint32 lw, quint32 address) {
    const auto upper = (address + ((lw >> 26) == 13 ? 0U : 0x8000U)) >> 16;
    word(b,hi,hw,(hw & 0xffff0000U) | upper);
    word(b,lo,lw,(lw & 0xffff0000U) | (address & 65535U));
}
bool rewritten(const CapacityRuntimeBuild &b, quint32 site) {
    for (auto it=b.rewrittenOwners.cbegin();it!=b.rewrittenOwners.cend();++it)
        if (site >= it.key() && site < it.key()+it.value()) return true;
    return false;
}
}
quint32 CapacityRuntimeBuild::relocated(quint32 a, const ClassDataLayout &classes) const {
    for (const auto &row : arrays)
        if (a >= row.old && a < row.old + row.size) return base + row.offset + a-row.old;
    if (a >= 0x80187bcc && a < 0x8018aa70) return classes.indexBase+a-0x80187bcc;
    return a;
}
CapacityRuntimeBuild prepareCapacityRuntime(quint32 base, const Rom &rom) {
    if (base < 0x80250000 || (base & 0xffff) || base+0xad00 >= 0x80400000)
        throw Error("Invalid permanent capacity storage allocation");
    CapacityRuntimeBuild b; b.base=base; b.storage=QByteArray(0xad00,0);
    for (const auto &a : arrays) b.symbols[a.symbol]=base+a.offset;
    b.symbols["lc_enemy_types"]=base+0x3400;
    b.symbols["lc_capacity_name_work"]=base+0x9cc0;
    b.symbols["lc_capacity_quick_descriptor"]=base+0xacc0;
    b.symbols["lc_enemy_soldier_max_hp"]=0x801953f2;
    auto descriptor=rom.bytes().mid(0x5d518,64);
    if (descriptor.size()!=64 || u32(descriptor,0)!=0x80195560 ||
        u32(descriptor,4)!=52 || u32(descriptor,12)!=100 ||
        u32(descriptor,16)!=0x801974de || u32(descriptor,20)!=25 ||
        u32(descriptor,28)!=20 || u32(descriptor,48)!=0)
        throw Error("Unrecognized enemy save descriptor");
    put32(descriptor,0,base); put32(descriptor,12,256);
    put32(descriptor,16,b.symbols["lc_capacity_sources"]+30*25); put32(descriptor,28,31);
    b.storage.replace(0xacc0,64,descriptor);
    auto guardEntry=[&](quint32 at) { b.entryGuards[at]=u32(rom.bytes(),at);b.entryGuards[at+4]=u32(rom.bytes(),at+4); };
    const auto classes=ClassPlacement::read(rom);
    auto ownerBytes=[&](quint32 at,quint32 size) {
        auto bytes=rom.bytes().mid(at,size);
        return classes?classes->canonicalReferences(bytes,at):bytes;
    };
    for(const auto v:catalog("capacity/owners")["owners"].toArray()) {
        const auto o=v.toObject();const auto at=number(o["start"]);
        const auto bytes=ownerBytes(at,number(o["size"]));
        if(sha256(bytes)!=o["retailSha256"].toString().toLatin1())
            throw Error("Expanded armies cannot replace unrecognized code at z64 "+QString::number(at,16));
        b.ownerGuards[at]=bytes;guardEntry(at);
        for(const auto e:o["entryPoints"].toArray())guardEntry(number(e.toObject()["start"]));
    }
    guardEntry(0x23a5ec);
    guardEntry(0x19554c);
    b.ownerGuards[0x23a5ec]=ownerBytes(0x23a5ec,0xa5c);
    const auto member=ownerBytes(0x19554c,0x284);
    if(sha256(member)!=QByteArray("5985a5dfc866d4effb58c0e412aa76a8e0ae8da0ef19bb8e44a6bf278c2a5e2b") &&
       !scenarioMemberRuntimeInstalled(rom))
        throw Error("Expanded armies cannot replace an unrecognized scenario member constructor");
    b.ownerGuards[0x19554c]=member;
    return b;
}
void compileCapacityRuntime(CapacityRuntimeBuild &b, quint32 codeAddress,
                            const ClassDataLayout &classes, const Compiler &compiler,
                            const QString &directory) {
    if (!b.code.isEmpty() || codeAddress%16 || codeAddress < b.base+b.storage.size())
        throw Error("Invalid capacity code allocation");
    b.codeAddress=codeAddress;
    QMap<QString,QByteArray> headers{{"enemy-capacity.h",source("scenarios/enemy-capacity.h")},
        {"capacity-save.h",source("scenarios/capacity-save.h")},
        {"custom.h",source("encounters/custom.h")}};
    auto bindings=b.symbols;
    bindings.insert({{"lc_capacity_clear",0x80093380},{"lc_capacity_character",0x802150bc},
        {"lc_capacity_finalize",0x801d4928},{"lc_capacity_player_construct",0x80215340},
        {"lc_capacity_create_record",0x801d0840},{"lc_capacity_original_persuasion",0x801d168c},
        {"lc_capacity_original_talk_target",0x801c940c},{"lc_capacity_pack",0x80074494},
        {"lc_capacity_unpack",0x800742f4},{"lc_capacity_name_encode",0x8017aa18},
        {"lc_capacity_name_decode",0x8017aaf4}});
    bindings.insert(b.custom);
    auto append=[&](QByteArray bytes) {
        b.code+=bytes;
        while(b.code.size()%16)b.code.append(char(0));
        if (quint64(b.codeAddress)+b.code.size()>0x80400000)
            throw Error("Capacity runtime exceeds 4 MB RAM");
    };
    auto module=[&](const QString &name,const QByteArray &text,QMap<QString,quint32> symbols) {
        symbols.insert(bindings); symbols.insert(b.entries);
        const auto compiled=compiler.compileModule(text,directory,"capacity_"+name,
            b.codeAddress+quint32(b.code.size()),headers,symbols,true);
        b.entries.insert(compiled.entries); append(compiled.bytes); return compiled;
    };
    auto core=source("scenarios/enemy-capacity.c")+"\n"+source("scenarios/capacity-tags.c")+
        "\n"+source("scenarios/capacity-quick-save.c")+"\n"+source("scenarios/capacity-neutral.c");
    if(!b.custom.isEmpty())core="#define LC_CAPACITY_CUSTOM 1\n#define LC_ENEMY_CAPACITY 1\n"+core;
    module("core",core,{});
    {
        const auto owner=scenarioMemberOwner();QMap<QString,quint32> symbols;
        const auto original=owner["symbols"].toObject();
        for(auto it=original.begin();it!=original.end();++it)symbols[it.key()]=b.relocated(number(it.value()),classes);
        module("scenario_member",source("scenarios/squad-member.c"),symbols);
        b.rewrittenOwners[0x19554c]=0x284;
    }
    const auto owners=catalog("capacity/owners")["owners"].toArray();
    for(const auto v:owners) {
        const auto o=v.toObject(); const auto start=number(o["start"]);
        b.rewrittenOwners[start]=number(o["size"]);
        auto text=source(o["adaptedSource"].toString().mid(5));
        if(sha256(text)!=o["adaptedSha256"].toString().toLatin1())
            throw Error("Capacity C and owner catalog disagree");
        QMap<QString,quint32> symbols;
        const auto original=o["symbols"].toObject();
        for(auto it=original.begin();it!=original.end();++it)
            symbols[it.key()]=b.relocated(number(it.value()),classes);
        if(start==0x1960a8) symbols["D_8019367C"]=groupBase(b,"drops_source_index");
        module(QFileInfo(o["adaptedSource"].toString()).baseName(),text,symbols);
    }
    QMap<QString,quint32> training{{"load_resource",0x8009dbb8},{"resource_size",0x8009daf4},
        {"free_resource",0x800712c4},{"load_archive",0x8007acb0},{"copy_bytes",0x80080998},
        {"clear_bytes",0x80093380},{"construct_member",0x801d0840},{"prepare_battle",0x80172394},
        {"set_mode",0x8017223c},{"release_battle",0x801707f4}};
    b.trainingVariant=compiler.compileModule(source("tools/training-constructor.c"),directory,
        "capacity_training_legacy",0x801d0a8c,{},training).fitOwner(0xa5c);
    if(b.ownerGuards[0x23a5ec]!=b.trainingVariant &&
       sha256(b.ownerGuards[0x23a5ec])!=QByteArray("0c893885b08c8c8a42810d57327bd7c86fbaca2dbdd286c1599868903563721c"))
        throw Error("Expanded armies cannot replace an unrecognized training constructor");
    module("training","#define LC_ENEMY_CAPACITY 1\n"+source("tools/training-constructor.c"),training);
    b.rewrittenOwners[0x23a5ec]=0xa5c;
    // Relocate existing instructions before building thunks, so any displaced
    // load in a thunk retains its final class/item/capacity address operand.
    for(const auto v:catalog("capacity-references")["owners"].toArray()) {
        const auto o=v.toObject(); if(rewritten(b,number(o["start"])))continue;
        for(const auto p:o["references"].toArray()) {
            const auto ref=p.toObject();
            for(const auto q:ref["uses"].toArray()) {
                const auto use=q.toObject();
                pair(b,number(ref["high"]),number(use["low"]),number(ref["highWord"]),
                    number(use["lowWord"]),groupBase(b,use["group"].toString())+number(use["addend"]));
            }
        }
    }
    for(const auto v:catalog("capacity-bounds")["owners"].toArray())
        for(const auto p:v.toObject()["patches"].toArray()) {
            const auto patch=p.toObject();const auto at=number(patch["site"]);
            if(!rewritten(b,at))word(b,at,number(patch["expected"]),number(patch["value"]));
        }
    for(const auto v:catalog("capacity-save-owners")["owners"].toArray()) {
        const auto o=v.toObject();
        for(const auto p:o["patches"].toArray()) {
            const auto patch=p.toObject();const auto kind=patch["kind"].toString();
            const quint32 value=kind=="enemyCount"?31:kind=="mapCount"?61:
                (kind=="snapshotOffset"?64:10312)+number(patch["addend"]);
            word(b,number(patch["site"]),number(patch["expected"]),
                (number(patch["expected"]) & 0xffff0000U)|value);
        }
        for(const auto p:o["references"].toArray()) {
            const auto r=p.toObject();pair(b,number(r["high"]),number(r["low"]),number(r["highWord"]),
                number(r["lowWord"]),groupBase(b,r["group"].toString())+number(r["addend"]));
        }
    }
    auto thunks=catalog("capacity-predicates")["patches"].toArray();
    for(const auto v:catalog("capacity-extra")["patches"].toArray())thunks.append(v);
    struct Thunk { QJsonObject row; quint32 address; QByteArray bytes; };
    QVector<Thunk> pending;
    auto linkThunk=[&](const QJsonObject &row,quint32 address,bool provisional) {
        auto symbols=bindings;symbols.insert(b.entries);
        const auto defs=row["bindings"].toObject();
        for(auto it=defs.begin();it!=defs.end();++it) {
            if(it.value().isObject()) {
                const auto ref=it.value().toObject();const auto name=ref["entry"].toString();
                if(!provisional && !b.entries.contains(name))throw Error("Missing capacity join "+name);
                symbols[it.key()]=b.entries.value(name,address)+number(ref["offset"]);
            } else symbols[it.key()]=number(it.value());
        }
        for(const auto v:row["displaced"].toArray()) {
            const auto d=v.toObject();const auto at=number(d["site"]);
            if(b.words.contains(at))symbols[d["symbol"].toString()]=b.words[at];
        }
        return compiler.assembleOwner(row["source"].toString().toUtf8(),directory,
            row["name"].toString()+(provisional?"_size":""),address,symbols);
    };
    for(const auto v:thunks) {
        const auto row=v.toObject();const auto address=b.codeAddress+quint32(b.code.size());
        const auto bytes=linkThunk(row,address,true);
        b.entries[row["name"].toString()]=address;pending.append({row,address,bytes});append(bytes);
    }
    for(const auto &thunk:pending) {
        const auto &row=thunk.row;const auto bytes=linkThunk(row,thunk.address,false);
        if(bytes.size()!=thunk.bytes.size())throw Error("Capacity thunk relinking changed size");
        b.code.replace(thunk.address-b.codeAddress,bytes.size(),bytes);
        const auto at=number(row.contains("hook")?row["hook"]:row["site"]);
        const auto expected=row["expected"].toArray();
        for(int i=0;i<expected.size();++i)
            word(b,at+i*4,number(expected[i]),i?0:0x08000000U|((thunk.address>>2)&0x3ffffffU),true);
    }
    auto redirect=[&](quint32 at,const QString &name) {
        if(!b.entries.contains(name)||!b.entryGuards.contains(at))throw Error("Missing capacity C entry: "+name);
        word(b,at,b.entryGuards[at],0x08000000U|((b.entries[name]>>2)&0x3ffffffU));
        word(b,at+4,b.entryGuards[at+4],0);
    };
    for(const auto v:owners) {
        const auto o=v.toObject();const auto entries=o["entryPoints"].toArray();
        if(entries.isEmpty())redirect(number(o["start"]),QFileInfo(o["adaptedSource"].toString()).baseName());
        else for(const auto e:entries)redirect(number(e.toObject()["start"]),e.toObject()["name"].toString());
    }
    redirect(0x23a5ec,"training_constructor");
    redirect(0x19554c,"lc_scenario_member");
    // Normal saves retain their original counts and player roster. Their bases
    // follow the relocated source arrays and the legacy Soldier index interval.
    word(b,0x5d354,0x801971f0,b.symbols["lc_capacity_sources"]);
    word(b,0x5d3c4,0x801977d8,b.symbols["lc_capacity_active_sources"]);
    word(b,0x5d424,0x801953f4,b.symbols["lc_enemy_soldier_hp"]+200);
    word(b,0x5d434,0x801954e4,b.symbols["lc_enemy_soldier_state"]+100);
    word(b,0x3864c+28+16,0x8016d068,b.entries["lc_capacity_quick_load"]);
    word(b,0x3864c+28+20,0x8016d100,b.entries["lc_capacity_quick_save"]);
    word(b,0x3864c+3*28+24,0xecc,9964);
}
void applyCapacityRuntime(QByteArray &rom,const CapacityRuntimeBuild &b) {
    // A partial install is never published: authenticate all operands first.
    // Independent scenario templates may have installed their maintained C
    // helper earlier in this same export. Both whole owners are authenticated;
    // capacity replaces its entry with the same helper linked to the new pools.
    const auto input=Rom::fromBytes(rom);
    const auto classes=ClassPlacement::read(input);
    bool memberVariant=false,trainingVariant=false;
    for(auto it=b.ownerGuards.cbegin();it!=b.ownerGuards.cend();++it) {
        auto bytes=rom.mid(it.key(),it.value().size());
        if(classes)bytes=classes->canonicalReferences(bytes,it.key());
        if(bytes==it.value())continue;
        if(it.key()==0x19554c && scenarioMemberRuntimeInstalled(input)){memberVariant=true;continue;}
        if(it.key()==0x23a5ec && bytes==b.trainingVariant){trainingVariant=true;continue;}
        throw Error("Expanded-army owner changed during export at z64 "+QString::number(it.key(),16));
    }
    if(u32(rom,0x19554c)!=b.entryGuards.value(0x19554c) || u32(rom,0x195550)!=b.entryGuards.value(0x195550))
        memberVariant=scenarioMemberRuntimeInstalled(Rom::fromBytes(rom));
    for(auto it=b.expected.cbegin();it!=b.expected.cend();++it)
        if(u32(rom,it.key())!=it.value() && !(memberVariant && (it.key()==0x19554c || it.key()==0x195550)) &&
           !(trainingVariant && (it.key()==0x23a5ec || it.key()==0x23a5f0)))
            throw Error("Unrecognized capacity operand at z64 "+QString::number(it.key(),16));
    for(auto it=b.words.cbegin();it!=b.words.cend();++it)put32(rom,it.key(),it.value());
}
QByteArray installCapacityRuntime(const Rom &rom,const CapacityRuntimeBuild &b) {
    auto classes=ClassPlacement::read(rom);
    if(!classes || !classes->saves.installed() || b.base<classes->layout.indexBase ||
        quint64(b.codeAddress)+b.code.size()>classes->layout.end)
        throw Error("Capacity code is outside the installed permanent allocation");
    const auto data=rom.bytes().mid(classes->dataRomOffset,classes->layout.end-classes->layout.indexBase);
    if(data.mid(b.base-classes->layout.indexBase,b.storage.size())!=b.storage ||
        data.mid(b.codeAddress-classes->layout.indexBase,b.code.size())!=b.code)
        throw Error("Installed capacity data differs from its linked source");
    auto before=rom.bytes();Rom::updateChecksum(before);
    auto image=before;applyCapacityRuntime(image,b);
    classes->saves.ownerChecksum=saveRuntimeOwnerChecksum(image,classes->saves.version);
    image.replace(ClassPlacement::descriptorRomOffset,ClassPlacement::descriptorSize,
        classes->descriptor(data,image.mid(ClassPlacement::codeRomOffset,classes->codeBytes)));
    Rom::updateChecksum(image);
    image=CapacityPlacement::record(before,image,b.base,b.codeAddress);
    ClassPlacement::read(Rom::fromBytes(image));
    return image;
}
} // namespace lordly
