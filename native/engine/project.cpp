#include "engine/project.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <cmath>

namespace lordly {
QString tableKey(Table t) { return t==Table::Classes?"classes":"items"; }
const QVector<Field>& fields(Table table) {
    static const auto classes=[] {
        QVector<Field> f;
        auto add=[&](QString key,QString label,QString group,int offset,int width=1,QString choices={}) {
            f.push_back({key,label,group,offset,width,false,choices});
        };
        add("unitSize","Size","Identity",4,1,"UNIT_SIZES");
        add("sexOrVoice","Body / voice","Identity",5,1,"CLASS_SEX_VOICE");
        add("leadership","Leadership","Identity",6,1,"CLASS_LEADERSHIP");
        add("baseHp","Base HP","Stats",8,2); add("hpGrowth","HP growth","Growth",10);
        const QStringList stats={"STR","VIT","INT","MEN","AGI","DEX"};
        for(int i=0;i<stats.size();++i) {
            add(stats[i].toLower()+"Base",stats[i]+" base","Stats",12+i*4,2);
            add(stats[i].toLower()+"Growth",stats[i]+" growth","Growth",14+i*4);
        }
        add("luck","Base LCK","Stats",35); add("alignment","Alignment","Stats",36);
        const QStringList res={"Physical","Wind","Fire","Earth","Water","Virtue","Bane"};
        for(int i=0;i<res.size();++i) add("res"+res[i],res[i],"Resistance",37+i);
        add("moveType","Movement","Identity",44,1,"MOVEMENT_TYPES");
        const QStringList equip={"Weapon","Body","Off-hand","Head"};
        for(int i=0;i<equip.size();++i) add("equipment"+QString::number(i),equip[i],"Equipment",46+i*2,2,"ITEM_NAMES");
        add("fixedEquipment","Fixed equipment mask","Equipment",54);
        const QStringList rows={"Front","Middle","Rear"};
        for(int i=0;i<rows.size();++i) {
            add("attack"+QString::number(i),rows[i]+" action","Attacks",55+i*2,1,"ACTION_TEMPLATE_LABELS");
            add("attackCount"+QString::number(i),rows[i]+" count","Attacks",56+i*2);
        }
        add("physAtk","Physical attack","Combat",61); add("magAtk","Magic attack","Combat",62);
        add("physDef","Physical defense","Combat",63); add("magDef","Magic defense","Combat",64);
        add("baseClass","Base growth class","Growth",65,1,"CLASS_NAMES");
        add("baseTransitionLevel","First transition level","Growth",66);
        add("intermediateClass","Intermediate growth class","Growth",67,1,"CLASS_NAMES");
        add("finalTransitionLevel","Second transition level","Growth",68);
        add("classCopyMatch","Class-copy match","Growth",69,1,"CLASS_NAMES");
        add("element","Default element","Combat",70,1,"DEFAULT_ELEMENTS");
        add("itemCapacity","Carried-item contribution","Equipment",71);
        return f;
    }();
    static const auto items=[] {
        QVector<Field> f={{"equipType","Equipment type","Identity",4,1,false,"EQUIP_TYPES"},
            {"element","Element","Identity",5,1,false,"ELEMENT_NAMES"},
            {"grade","Grade","Identity",6}, {"price","Price","Identity",8,2}};
        const QStringList stats={"STR","INT","AGI","DEX","VIT","MEN"};
        for(int i=0;i<stats.size();++i) f.push_back({stats[i].toLower(),stats[i],"Stats",10+i,1,true});
        const QStringList res={"Physical","Wind","Fire","Earth","Water","Virtue","Bane"};
        for(int i=0;i<res.size();++i) f.push_back({"res"+res[i],res[i],"Resistance",17+i,1,true});
        f.push_back({"growth20","Growth lanes B20","Growth",24});
        f.push_back({"growth21","Growth lanes B21","Growth",25});
        return f;
    }();
    return table==Table::Classes?classes:items;
}
Project Project::fromRom(Rom rom,const QJsonObject& catalog) {
    rom.requireSupported(); Project p; p.rom_=std::move(rom); p.catalog_=catalog;
    for(auto table:{Table::Classes,Table::Items}) {
        auto& records=p.tables_[table]; bool cls=table==Table::Classes;
        int count=cls?165:278, stride=cls?72:32;
        qsizetype base=cls?0x5dad8+72-12:0x62310-4;
        if(p.rom_.revision()==1) base+=0x20;
        auto names=catalog.value(cls?"CLASS_NAMES":"ITEM_NAMES").toObject();
        for(int id=0;id<count;++id) {
            Record r; r.id=id; r.romOffset=base+id*stride;
            r.name=names.value(QString::number(id)).toString(QString("%1 %2").arg(cls?"Class":"Item").arg(id));
            r.original=p.rom_.bytes().mid(r.romOffset,stride); r.bytes=r.original;
            if(r.bytes.size()!=stride) throw Error("Truncated record table");
            records.push_back(std::move(r));
        }
    }
    return p;
}
const Record& Project::record(Table t,int row) const {
    const auto& records=tables_.at(t);
    if(row<0 || row>=records.size()) throw Error("Record index outside table");
    return records[row];
}
int Project::value(Table t,int row,int column) const {
    if(column<0 || column>=fields(t).size()) throw Error("Field index outside table");
    const auto& f=fields(t)[column]; const auto& bytes=record(t,row).bytes;
    auto value=f.width==2 ? int(u16(bytes,f.offset)) : int(u8(bytes,f.offset));
    return f.signedValue && value>=128?value-256:value;
}
void Project::setValue(Table t,int row,int column,int value) {
    if(column<0 || column>=fields(t).size()) throw Error("Field index outside table");
    const auto& f=fields(t)[column];
    if(value<f.minimum() || value>f.maximum()) throw Error("Field value outside its stored range");
    auto bytes=record(t,row).bytes;
    if(f.width==2) put16(bytes,f.offset,quint16(value)); else put8(bytes,f.offset,quint8(value));
    replaceBytes(t,row,bytes);
}
void Project::replaceBytes(Table t,int row,const QByteArray& bytes) {
    if(bytes.size()!=record(t,row).bytes.size()) throw Error("Record size changed unexpectedly");
    tables_.at(t)[row].bytes=bytes;
}
int Project::changedRecords() const {
    int count=0; for(const auto& [table,records]:tables_) for(const auto& r:records) if(r.original!=r.bytes) ++count;
    return count;
}
QJsonObject Project::manifest() const {
    auto result=retained_;
    result["format"]="LordlyCaliberProject"; result["version"]=1;
    result["sourceSha256"]=QString::fromLatin1(sha256(rom_.bytes()));
    result["sourcePath"]=rom_.sourcePath();
    QJsonObject edits;
    for(const auto& [table,records]:tables_) {
        QJsonObject rows;
        for(const auto& r:records) if(r.original!=r.bytes) rows[QString::number(r.id)]=QString::fromLatin1(r.bytes.toHex());
        edits[tableKey(table)]=rows;
    }
    result["records"]=edits;
    if(!legacy_.isEmpty()) result["legacyImport"]=legacy_;
    return result;
}
void Project::applyManifest(const QJsonObject& object) {
    if(object.value("format")!="LordlyCaliberProject" || object.value("version").toInt()!=1) throw Error("Unsupported native project format");
    if(object.value("sourceSha256").toString().toLatin1()!=sha256(rom_.bytes())) throw Error("Project requires a different source ROM");
    auto candidate=*this;
    auto edits=object.value("records").toObject();
    for(auto it=edits.begin();it!=edits.end();++it) {
        Table table;
        if(it.key()=="classes") table=Table::Classes;
        else if(it.key()=="items") table=Table::Items;
        else throw Error("Unsupported project record type: "+it.key());
        if(!it.value().isObject()) throw Error("Project record table must be an object");
        const auto rows=it.value().toObject();
        for(auto row=rows.begin();row!=rows.end();++row) {
            bool ok=false; int id=row.key().toInt(&ok); if(!ok) throw Error("Invalid project record ID");
            auto text=row.value().toString().toLatin1();
            auto bytes=QByteArray::fromHex(text);
            if(text.toLower()!=bytes.toHex()) throw Error("Invalid project record bytes");
            candidate.replaceBytes(table,id,bytes);
        }
    }
    candidate.retained_=object; candidate.legacy_=object.value("legacyImport").toObject();
    *this=std::move(candidate);
}
void Project::importLegacy(const QJsonObject& object) {
    if(object.value("format").toString()!="ob64-patch") throw Error("Unsupported browser project format");
    int version=object.value("version").toInt();
    if(version<9 || version>36) throw Error("Browser project version requires an explicit migration before import");
    auto candidate=*this; auto patch=object.value("patches").toObject();
    for(auto table:{Table::Classes,Table::Items}) {
        auto rows=patch.value(table==Table::Classes?"classDefs":"items").toObject();
        for(auto it=rows.begin();it!=rows.end();++it) {
            bool ok=false; auto id=it.key().toInt(&ok); if(!ok) throw Error("Invalid imported record ID");
            auto bytes=candidate.record(table,id).bytes;
            auto entry=it.value().toObject();
            if(table==Table::Classes && entry.contains("record_index") && entry["record_index"].toInt()!=id+1)
                throw Error("Imported class record index disagrees with class ID");
            auto changes=entry.value("bytes").toObject();
            for(auto v=changes.begin();v!=changes.end();++v) {
                int offset=v.key().toInt(&ok); double value=v.value().toDouble(-1);
                if(!ok || offset<0 || offset>=bytes.size() || value<0 || value>255 || std::floor(value)!=value)
                    throw Error("Invalid imported record byte");
                int header=table==Table::Classes?12:4, body=int(bytes.size())-header;
                put8(bytes,offset<body?offset+header:offset-body,quint8(value));
            }
            candidate.replaceBytes(table,id,bytes);
        }
    }
    candidate.legacy_=object;
    candidate.importWarnings_={"The original browser project is retained. Only supported native sections are applied; other sections remain pending migration."};
    *this=std::move(candidate);
}
void atomicWrite(const QString& path,const QByteArray& data) {
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly) || file.write(data)!=data.size() || !file.commit()) throw Error("Cannot save "+path+": "+file.errorString());
}
void Project::saveFolder(const QString& folder) const {
    if(!QDir().mkpath(folder)) throw Error("Cannot create project folder");
    atomicWrite(QDir(folder).filePath("project.json"),QJsonDocument(manifest()).toJson());
}
QJsonObject Project::readProject(const QString& path) {
    QFile file(QFileInfo(path).isDir()?QDir(path).filePath("project.json"):path);
    if(!file.open(QIODevice::ReadOnly) || file.size()>64*1024*1024) throw Error("Cannot read project or project is too large");
    auto bytes=file.readAll();
    if(bytes.startsWith("LC05PACK\n")) {
        bytes=bytes.mid(9);
        if(bytes.size()<4 || u32(bytes,0)>64*1024*1024) throw Error("Project archive exceeds decoded size limit");
        bytes=qUncompress(bytes); if(bytes.isEmpty()) throw Error("Damaged project archive");
    }
    QJsonParseError error; auto doc=QJsonDocument::fromJson(bytes,&error);
    if(error.error!=QJsonParseError::NoError || !doc.isObject()) throw Error("Invalid project JSON: "+error.errorString());
    return doc.object();
}
void Project::saveArchive(const QString& path) const {
    auto object=manifest(); object.remove("sourcePath");
    atomicWrite(path,"LC05PACK\n"+qCompress(QJsonDocument(object).toJson(QJsonDocument::Compact),9));
}
QByteArray Project::rebuildRecords() const {
    auto out=rom_.bytes();
    for(const auto& [table,records]:tables_) for(const auto& r:records) {
        if(r.romOffset<0 || r.romOffset+r.bytes.size()>out.size()) throw Error("Record allocation exceeds ROM");
        out.replace(r.romOffset,r.bytes.size(),r.bytes);
    }
    return out;
}
}
