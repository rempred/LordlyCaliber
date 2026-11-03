#include "engine/project.h"
#include "engine/deleted_content.h"
#include "engine/project_references.h"
#include "engine/scene_actions.h"
#include <QJsonArray>
#include <algorithm>
namespace lordly {
bool Project::contentDeleted(const QString &kind,int id) const {
    return deletedContent_[kind].toArray().contains(id);
}
int Project::nextContentId(const QString &kind,int count) const {
    int next=count;
    for (const auto &v:deletedContent_[kind].toArray()) next=qMin(next,v.toInt());
    return next;
}
void Project::setDeletedContent(const QJsonObject &value) {
    validateDeletedContent(value);
    for (auto it=value.begin();it!=value.end();++it) {
        int count=0;
        if(it.key()=="classes")count=records(Table::Classes).size();
        else if(it.key()=="items")count=records(Table::Items).size();
        else if(it.key()=="attacks"){loadAttacks();count=attacks_->entries().size();}
        else if(it.key()=="shops"){loadShops();count=shops_->size();}
        else if(it.key()=="pools"){loadEncounters();count=encounters_->slotsPerPool();}
        for(const auto &v:it.value().toArray())if(v.toInt()>=count)throw Error("Deleted entry is outside its table");
    }
    deletedContent_=value;
    for (const auto table:{Table::Classes,Table::Items}) {
        auto labels=catalog_[table==Table::Classes?"CLASS_NAMES":"ITEM_NAMES"].toObject();
        for(const auto &row:records(table)) {
            if(contentDeleted(tableKey(table),row.id))labels.remove(QString::number(row.id));
            else labels[QString::number(row.id)]=row.name;
        }
        catalog_[table==Table::Classes?"CLASS_NAMES":"ITEM_NAMES"]=labels;
    }
    if(attacks_)setAttacks(attacks_->json());
}
void Project::markContentDeleted(const QString &kind,int id,bool deleted) {
    auto next=deletedContent_;auto list=next[kind].toArray();
    if(deleted && !list.contains(id))list.append(id);
    if(!deleted)for(int i=list.size()-1;i>=0;--i)if(list[i].toInt()==id)list.removeAt(i);
    if(list.isEmpty())next.remove(kind);else next[kind]=list;
    setDeletedContent(next);
}
QMap<int,QString> Project::customContent(const QString &kind) {
    QMap<int,QString> result;
    auto add=[&](int id,const QString &name){if(!contentDeleted(kind,id))result[id]=name;};
    if(kind=="classes" || kind=="items") {
        const auto table=kind=="classes"?Table::Classes:Table::Items;
        for(const auto &r:records(table))if(r.id>=(table==Table::Classes?165:278))add(r.id,r.name.simplified());
    } else if(kind=="attacks") {
        loadAttacks();for(int id=159;id<attacks_->entries().size();++id)add(id,AttackData::displayName(attacks_->entry(id)));
    } else if(kind=="shops") {
        loadShops();for(int id=36;id<shops_->size();++id)add(id,QString("Shop %1").arg(id));
    } else if(kind=="pools") {
        loadEncounters();for(int id=10;id<encounters_->slotsPerPool();++id)add(id,QString("Pool %1").arg(id+1));
    } else if(kind=="weaponTypes") {
        const auto names=weaponTypes_.names();for(auto it=names.begin();it!=names.end();++it)add(it.key().toInt(),it.value().toString());
    } else if(kind=="families") {
        loadPromotion();for(const auto &v:promotion_->families["families"].toArray()) {
            const auto f=v.toObject();if(f["id"].toInt()>=16)add(f["id"].toInt(),Promotion::familyName(f));
        }
    }
    return result;
}
QStringList Project::deletionUses(const QString &kind,int id) {
    if(!customContent(kind).contains(id))throw Error("Select an added entry to delete");
    QStringList uses;
    if(kind=="classes" || kind=="items" || kind=="shops") {
        for(const auto &ref:projectReferences(*this)) {
            if(ref.kind!=kind || ref.id!=id || contentDeleted(ref.location.editor,ref.location.id))continue;
            if(kind=="classes" && ref.location.editor=="classes" && ref.location.id==id)continue;
            uses<<ref.description;
        }
        // Authored cutscene actors can refer to custom identities directly.
        if(kind=="classes")loadScenePrograms();
        if(kind=="classes" && scenePrograms_)for(const auto &scene:scenePrograms_->scenes()) {
            const auto program=scenePrograms_->program(scene);
            for(const auto &action:SceneActions::inspect(program))for(const auto &field:action.fields)
                if(field.name.startsWith("Primary class") || field.name.startsWith("Current class")) {
                    const auto &command=program.commands[action.command+field.commandOffset];
                    if(field.operand>=0 && field.operand<command.operands.size() && field.canonical(command.operands[field.operand]) &&
                        qint64(qint32(command.operands[field.operand]))-field.bias==id)
                        uses<<QString("Cutscene · %1").arg(action.name);
                }
        }
    } else if(kind=="attacks") {
        for(const auto &r:records(Table::Classes))if(!contentDeleted("classes",r.id))
            for(const auto &f:fields(Table::Classes))if(f.choices=="ACTION_NAMES" && u8(r.bytes,f.offset)==id)
                uses<<r.name.simplified()+" · "+f.label;
        loadAttacks();
        for(int attack=1;attack<attacks_->entries().size();++attack)if(attack!=id && !contentDeleted("attacks",attack))
            for(const auto target:attacks_->entry(attack).spellLinks)if(target==id)
                uses<<AttackData::displayName(attacks_->entry(attack))+" · spell array";
    } else if(kind=="weaponTypes") {
        for(const auto &r:records(Table::Items))if(!contentDeleted("items",r.id) && u8(r.bytes,4)==id)uses<<r.name.simplified();
    } else if(kind=="families") {
        loadPromotion();const auto assignments=promotion_->families["assignments"].toArray();
        for(int cls=1;cls<assignments.size();++cls)if(assignments[cls].toInt()==id && !contentDeleted("classes",cls))
            uses<<record(Table::Classes,cls).name.simplified();
        for(int linked:promotion_->linkedFamilies(id))uses<<QString("Linked family %1").arg(linked);
    } else if(kind=="pools") {
        loadEncounters();for(int terrain=0;terrain<32;++terrain)if(encounters_->lookup(terrain)==id+1)
            uses<<Encounters::mapTerrainName(terrain);
    }
    uses.removeDuplicates();return uses;
}
void Project::deleteCustomContent(const QString &kind,int id) {
    auto next=*this;const auto uses=next.deletionUses(kind,id);
    if(!uses.isEmpty())throw Error("This entry is still used by:\n"+uses.join('\n'));
    if(kind=="weaponTypes") {
        auto names=next.weaponTypes_.names();names.remove(QString::number(id));next.setWeaponTypes(names);
    } else if(kind=="families") {
        auto families=next.promotion_->families;auto list=families["families"].toArray();
        for(int i=list.size()-1;i>=0;--i)if(list[i].toObject()["id"].toInt()==id)list.removeAt(i);
        families["families"]=list;next.promotion_->families=Promotion::validateFamilies(families);
    } else {
        if(kind=="classes") {
            next.loadPromotion();next.promotion_->assign(id,0);
            auto names=next.personalNames();names.assign(id,-1);next.setPersonalNames(names.json());
        } else if(kind=="items") {
            auto bytes=next.record(Table::Items,id).bytes;put8(bytes,6,0);next.replaceBytes(Table::Items,id,bytes);
        } else if(kind=="attacks") {
            next.attacks_->rename(id,"Unused",false);auto cleared=next.attacks_->entry(5);
            cleared.nameGroup=next.attacks_->entry(id).nameGroup;cleared.name="Unused";cleared.dynamicName=false;
            next.attacks_->set(id,cleared);
        } else if(kind=="shops") next.shops_->setStock(id,{});
        else if(kind=="pools") {
            for(int pool=1;pool<=Encounters::poolCount;++pool)next.encounters_->setPair(pool,id,0,0,next.records(Table::Classes).size());
            const auto squads=next.encounters_->customSquads().profiles();
            for(const auto &s:squads)if(s.terrainSlot==id)next.encounters_->removeCustomSquad(s.scenarioKey,id);
        }
        next.markContentDeleted(kind,id,true);
    }
    *this=std::move(next);
}
int Project::createAttack(const QString &name,int source) {
    auto next=*this;next.loadAttacks();const int id=next.nextContentId("attacks",next.attacks_->entries().size());
    if(id==next.attacks_->entries().size())next.attacks_->duplicate(source,name);
    else {
        next.attacks_->rename(id,name,false);auto value=next.attacks_->entry(source);
        value.name=next.attacks_->entry(id).name;value.nameGroup=next.attacks_->entry(id).nameGroup;value.dynamicName=false;
        next.attacks_->set(id,value);next.markContentDeleted("attacks",id,false);
    }
    next.setAttacks(next.attacks_->json());*this=std::move(next);return id;
}
int Project::createShop() {
    loadShops();const int id=nextContentId("shops",shops_->size());
    if(id==shops_->size())shops_->addShop();else {shops_->setStock(id,{});markContentDeleted("shops",id,false);}return id;
}
int Project::createEncounterPool() {
    loadEncounters();const int id=nextContentId("pools",encounters_->slotsPerPool());
    if(id==encounters_->slotsPerPool())encounters_->addPool();else markContentDeleted("pools",id,false);return id;
}
}
