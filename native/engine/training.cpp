#include "engine/training.h"
#include "engine/story_routes.h"
#include "engine/squad_members.h"
#include <QJsonArray>
namespace lordly {
QJsonObject TrainingOpponent::json() const {
    return {{"node",node},{"slot",slot},{"price",price},{"squad",QString::fromLatin1(squad.toHex())}};
}
Training Training::load(const Rom &rom) {
    rom.requireSupported();
    if(rom.revision()) throw Error("Training editing requires US revision zero.");
    Training result;
    result.table_=rom.bytes().mid(tableOffset,tableSize);
    for(const auto &entry:ClassData::load(rom).entries()) result.classSizes_+=entry.record[ClassData::unitSizeOffset];
    const auto enemies=ScenarioResources::load(rom).enemyData();
    const auto nodes=StoryRoutes(rom).nodes;
    for(int node=1;node<=48;++node) for(int slot=0;slot<5;++slot) {
        const int at=(node-1)*24+slot*4,id=u16(result.table_,at);
        if(!id) continue; // Locations without training retain their original availability.
        TrainingOpponent row{node,slot,u8(result.table_,at+2),nodes.value(node).name,readSquadRecord(enemies,id)};
        if(row.name.isEmpty()) row.name=QString("Location %1").arg(node);
        result.rows_<<row;
    }
    result.originals_=result.rows_;return result;
}
int Training::fee(int level,int price) {
    if(level<0 || level>99 || price<0 || price>255) throw Error("Invalid training fee input");
    return (level<=20?level*2:level<=40?level+20:(level-20)*3)*price;
}
QJsonObject Training::json() const {
    QJsonArray entries;for(const auto &row:rows_) entries.append(row.json());
    return {{"version",1},{"opponents",entries}};
}
bool Training::modified() const {
    for(int i=0;i<rows_.size();++i) if(rows_[i].json()!=originals_[i].json()) return true;
    return false;
}
void Training::validate(const ClassData &classes,int itemCount) const {
    bool sizesChanged=classes.entries().size()<classSizes_.size();
    for(int id=0;id<std::min(classes.entries().size(),classSizes_.size());++id)
        sizesChanged|=classes.entries()[id].record[ClassData::unitSizeOffset]!=classSizes_[id];
    for(int i=0;i<rows_.size();++i) {
        const auto &row=rows_[i];
        if(!sizesChanged && row.json()==originals_[i].json()) continue;
        if(row.price<0 || row.price>255) throw Error("Training price factor must be 0–255.");
        const auto members=squadMembers(row.squad);
        validateSquadFormation(members,classes);
        for(const auto &member:members) for(int item:member.equipment)
            if(item>=itemCount) throw Error("Training opponent references an unavailable item.");
    }
}
void Training::apply(const QJsonObject &object,const ClassData &classes,int itemCount) {
    if(object.size()!=2 || object["version"]!=1 || !object["opponents"].isArray()) throw Error("Invalid training settings");
    const auto array=object["opponents"].toArray();
    if(array.size()!=rows_.size()) throw Error("Training location directory is incomplete");
    auto next=*this;
    for(int i=0;i<array.size();++i) {
        const auto value=array[i].toObject();auto &row=next.rows_[i];
        if(value.size()!=4 || value["node"]!=row.node || value["slot"]!=row.slot ||
           !value["price"].isDouble() || value["price"].toDouble()!=value["price"].toInt(-1))
            throw Error("Invalid training opponent");
        const auto hex=value["squad"].toString().toLatin1();
        row.squad=QByteArray::fromHex(hex);row.price=value["price"].toInt(-1);
        if(row.squad.toHex()!=hex.toLower()) throw Error("Invalid training squad data");
        squadMembers(row.squad);
    }
    next.validate(classes,itemCount);*this=std::move(next);
}
QByteArray Training::plan(QByteArray &enemies) const {
    auto table=table_;
    for(int i=0;i<rows_.size();++i) {
        const auto &row=rows_[i];const int at=(row.node-1)*24+row.slot*4;
        if(row.squad!=originals_[i].squad) {
            const int id=int(enemies.size()/35)+1;
            if(id+row.squad.size()/35>65536) throw Error("Training opponents exceed the squad library.");
            enemies+=row.squad;put16(table,at,quint16(id));
        }
        put8(table,at+2,quint8(row.price));
    }
    return table;
}
}
