#include "engine/new_campaign.h"
#include "engine/event_authoring.h"
#include <QJsonArray>
#include <QSet>
namespace lordly {
QVector<QPair<quint32,int>> NewCampaign::armySpans() {
    return {{0x80193bc0,100*56}, {0x801971f0,30*25}, {0x801977d8,30},
            {0x80197710,100}, {0x801969b8,10*11}, {0x80193ab0,6},
            {0x80190f80,0x16c}, {0x80196b00,278*4}, {0x80193ac0,40*4}};
}
QJsonObject NewCampaign::settings(const SceneEvents &events) {
    return events.flagEdits()["1"].toObject()["campaign"].toObject();
}
QByteArray NewCampaign::armyBytes(const QJsonObject &settings,int id) {
    for (const auto &value:settings["armies"].toArray()) {
        const auto row=value.toObject();
        if(row["id"].toInt(-1)==id) {
            const auto bytes=QByteArray::fromBase64(row["save"].toString().toLatin1());
            if(bytes.size()!=0x8000)throw Error("Invalid starting army data");
            return bytes;
        }
    }
    throw Error("Starting army no longer exists");
}
QByteArray NewCampaign::armyMemory(const Project &project,const QByteArray &bytes) {
    auto save=SaveDocument::open(project.runtimeRom(),bytes,project.records(Table::Classes).size());
    save.selectSlot(0);
    save.apply(save.planArmyRefresh(project),true);
    const auto &memory=save.memory();
    if(save.units().isEmpty())throw Error("A starting army needs at least one squad");
    const auto &classes=project.records(Table::Classes);
    const auto &items=project.records(Table::Items);
    for(int id=1;id<100;++id) {
        const int at=0x193bc0+id*56;
        if(!u8(memory,at+17))continue;
        if(u8(memory,at+17)>=classes.size()||u8(memory,at+18)>=classes.size()||
           project.contentDeleted("classes",u8(memory,at+17))||project.contentDeleted("classes",u8(memory,at+18)))
            throw Error("Starting army uses a class missing from this project");
        for(int slot=0;slot<4;++slot)if(u16(memory,at+42+slot*2)>=items.size()||project.contentDeleted("items",u16(memory,at+42+slot*2)))
            throw Error("Starting army uses equipment missing from this project");
    }
    QByteArray result;
    for(auto [address,size]:armySpans())result+=memory.mid(address&0x1fffffff,size);
    return result;
}
QJsonObject NewCampaign::army(const Project &project,int id,const QString &name,const QByteArray &bytes) {
    armyMemory(project,bytes);
    return {{"id",id},{"name",name.trimmed()},{"save",QString::fromLatin1(bytes.toBase64())}};
}
void NewCampaign::validate(const QJsonObject &settings) {
    if(settings.isEmpty())return;
    auto integer=[&](const QString &key,int low,int high) {
        const auto v=settings[key];const int n=v.toInt(low-1);
        if(!v.isDouble()||v.toDouble()!=n||n<low||n>high)throw Error("Invalid campaign setting: "+key);
        return n;
    };
    if(integer("version",1,1)!=1)throw Error("Unsupported campaign settings");
    for(const auto key:{"keepWorldMap","keepScenarioMaps"})if(settings.contains(key)&&!settings[key].isBool())throw Error("Invalid campaign preservation option");
    if(settings.contains("excludedBattlefields")) {
        if(!settings["excludedBattlefields"].isArray())throw Error("Invalid campaign map templates");
        QSet<int> used;
        for(const auto &v:settings["excludedBattlefields"].toArray()) {
            const int key=v.toInt(-1);if(key<1||key>=ScenarioResources::keyCount||v.toDouble()!=key||used.contains(key))throw Error("Invalid campaign map template");
            used.insert(key);
        }
    }
    const int location=integer("location",1,40),selected=integer("army",0,armyCapacity-1);
    integer("goth",0,99999999);integer("chaos",0,100);
    const auto armies=settings["armies"].toArray();QSet<int> ids;QSet<QString> names;
    if(armies.isEmpty()||armies.size()>armyCapacity)throw Error("Choose one to sixteen starting armies");
    for(const auto &v:armies) {
        const auto a=v.toObject();const int id=a["id"].toInt(-1);const auto name=a["name"].toString().trimmed();
        if(id<0||id>=armyCapacity||a["id"].toDouble()!=id||ids.contains(id)||name.isEmpty()||name.size()>100||names.contains(name.toCaseFolded()))
            throw Error("Starting armies need unique names and IDs");
        ids.insert(id);names.insert(name.toCaseFolded());armyBytes(settings,id);
    }
    if(!ids.contains(selected))throw Error("Choose an existing default starting army");
    QSet<int> visible;
    if(!settings["locations"].isArray())throw Error("Choose initially accessible map locations");
    for(const auto &v:settings["locations"].toArray()) {
        const int id=v.toInt(-1);
        if(id<1||id>40||v.toDouble()!=id||visible.contains(id))throw Error("Invalid starting map location");
        visible.insert(id);
    }
    if(!visible.contains(location))throw Error("The starting location must be accessible");
}
QJsonObject NewCampaign::defaults(const Project &project,int location) {
    const auto bytes=SaveDocument::newArmy(project).serialize();
    return {{"version",1},{"location",location},{"locations",QJsonArray{location}},
            {"keepWorldMap",true},{"keepScenarioMaps",true},
            {"goth",0},{"chaos",50},{"army",0},
            {"armies",QJsonArray{army(project,0,"Starting army",bytes)}}};
}
QJsonArray NewCampaign::opening(const QJsonObject &settings) {
    return {QJsonObject{{"id","campaign-opening"},{"name","Begin the campaign"},{"group",0},
        {"nodes",QJsonArray{QJsonObject{{"type","initializeCampaign"}},
                            QJsonObject{{"type","startingArmy"},{"army",settings["army"]}},
                            QJsonObject{{"type","beginCampaign"}}}}}};
}
void NewCampaign::configure(Project &project,const QJsonObject &settings) {
    validate(settings);
    project.loadCampaignMap();
    for(const auto &location:settings["locations"].toArray())
        if(!project.campaignMap()->position(location.toInt()).x())throw Error("Choose a map location that has been placed");
    for(const auto &v:settings["armies"].toArray())armyMemory(project,armyBytes(settings,v.toObject()["id"].toInt()));
    project.loadSceneEvents();auto json=project.sceneEvents()->json();auto flags=json["flags"].toObject();
    auto row=flags["1"].toObject();auto config=settings;
    if(!config.contains("templates")&&row["campaign"].toObject().contains("templates"))
        config["templates"]=row["campaign"].toObject()["templates"];
    auto trees=row["trees"].toArray();
    for(int t=0;t<trees.size();++t) {
        auto tree=trees[t].toObject();if(tree["id"]!="campaign-opening")continue;
        auto nodes=tree["nodes"].toArray();
        if(nodes.size()>1&&nodes[1].toObject()["type"]=="startingArmy") {
            auto node=nodes[1].toObject();node["army"]=config["army"];nodes[1]=node;
            tree["nodes"]=nodes;trees[t]=tree;
        }
    }
    row["trees"]=trees;
    row["campaign"]=config;flags["1"]=row;
    json["version"]=9;json["flags"]=flags;project.setSceneEvents(json);
}
void NewCampaign::initialize(Project &project,const QJsonObject &settings) {
    auto next=project;
    validate(settings);next.loadSceneEvents();
    next.loadScenarios();
    auto scenarios=next.scenarios()->json();auto rows=scenarios["scenarios"].toObject();
    auto config=settings;
    const auto existing=NewCampaign::settings(*next.sceneEvents());
    config["templates"]=existing.contains("templates")?existing["templates"]:scenarios;
    for(auto it=rows.begin();it!=rows.end();++it) {
        auto scenario=next.scenarios()->scenario(it.key().toInt());
        scenario.squads.clear();scenario.layout.deployments.clear();scenario.layout.conditions.clear();
        scenario.layout.conditionsPresent=false;scenario.layout.seed=QByteArray(2,0);
        QByteArray end(18,0);end[0]=4;scenario.layout.nodes={end};
        scenario.layout=Eset::decode(scenario.layout.encode());it.value()=scenario.json();
    }
    scenarios["scenarios"]=rows;next.setScenarios(scenarios);
    QJsonObject flags;
    // Leave the native training/menu/history services available. They are game
    // mechanisms, not an active story. All arrival and story handlers are replaced.
    for(int row=0;row<SceneEvents::retailRowCount;++row) {
        if(QSet<int>{64,65,66,67,68}.contains(row))continue;
        flags[QString::number(row)]=QJsonObject{{"replace",true}};
    }
    flags["1"]=QJsonObject{{"replace",true},{"campaign",config},{"trees",opening(config)}};
    next.setSceneEvents({{"version",9},{"bindings",QJsonObject{}},{"flags",flags}});
    configure(next,config);
    applyLayoutOptions(next);
    project=std::move(next);
}
void NewCampaign::applyLayoutOptions(Project &project) {
    auto next=project;next.loadSceneEvents();next.loadCampaignMap();next.loadScenarios();
    auto config=settings(*next.sceneEvents());validate(config);
    if(config.isEmpty())throw Error("Create a campaign first");
    if(!config["keepWorldMap"].toBool(true)) {
        auto map=*next.campaignMap();map.startEmpty(config["location"].toInt());next.setCampaignMap(map.json());
        config["locations"]=QJsonArray{config["location"]};
    }
    if(!config["keepScenarioMaps"].toBool(true)) {
        QJsonArray hidden;for(int key:next.scenarios()->entries().keys())hidden.append(key);
        config["excludedBattlefields"]=hidden;
    }else config.remove("excludedBattlefields");
    configure(next,config);project=std::move(next);
}
bool NewCampaign::includesBattlefield(const SceneEvents &events,int key) {
    return !settings(events)["excludedBattlefields"].toArray().contains(key);
}
}
