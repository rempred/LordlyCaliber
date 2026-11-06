#include "engine/battlefield_clone.h"
#include "engine/field_resources.h"
#include "engine/map_edits.h"
#include "engine/mission_battlefield.h"
#include "engine/scenario_battlefields.h"
#include "engine/promotion.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QProcess>
#include <QTest>

using namespace lordly;
namespace {
Rom input(){const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())throw Error("Private ROM required");return Rom::open(path);}
QByteArray prepare(const Rom &rom,int target,int source){auto bytes=rom.bytes();
    bytes.replace(0x654a0+target*9,9,bytes.mid(0x654a0+source*9,9));
    bytes.replace(0x13c874+target*4,4,bytes.mid(0x13c874+source*4,4));return bytes;}
void write(const QString &path,const QByteArray &bytes){QFile f(path);if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size())throw Error("Could not write private battlefield fixture");}
}
class BattlefieldCloneTests final:public QObject {
    Q_OBJECT
  private slots:
    void privateMapsAndNativeLoaders(){
        if(qEnvironmentVariableIsEmpty("LORDLY_TEST_ROM"))QSKIP("Private ROM required");
        try{
            const auto rom=input();const auto source=BattlefieldClone::capture(rom,1,false);
            auto bytes=BattlefieldClone::apply(Rom::fromBytes(prepare(rom,10,1)),10,source);
            auto copied=Rom::fromBytes(bytes);BattlefieldClone::validate(copied,10,source);
            QCOMPARE(BattlefieldClone::modelCount(copied),46);QCOMPARE(u8(bytes,0x654a0+10*9+3),45);
            // Scene media row67 and its original model4/geometry0 remain present.
            QCOMPARE(bytes.mid(0x654a0+67*9,9),rom.bytes().mid(0x654a0+67*9,9));
            QCOMPARE(BattlefieldClone::modelRow(copied,4),BattlefieldClone::modelRow(rom,4));
            for(quint32 root:{0x1e39dceU,0x20712eaU,0x18bd022U}){
                const auto old=fieldResource(rom,root);const auto now=fieldResource(copied,root);
                QCOMPARE(now.first(old.size()),old);
            }
            QCOMPARE(BattlefieldClone::capture(copied,1,false),source);
            auto maps=MapEdits::load(copied);QCOMPARE(maps.source(10).field.triangleCount(),maps.source(1).field.triangleCount());
            QCOMPARE(maps.sharedScenarios(10),QVector<int>{10});
            const auto originalFine=maps.fineTerrain(1);maps.paint(10,{{128,128}},1,4);
            QCOMPARE(maps.fineTerrain(1),originalFine);QVERIFY(maps.fineTerrain(10)!=maps.source(10).terrain[0].data);
            auto painted=Rom::fromBytes(maps.rebuild(copied));QCOMPARE(BattlefieldClone::capture(painted,1,false),source);
            QCOMPARE(MapEdits::load(painted).source(10).terrain[0].data,maps.fineTerrain(10));
            auto points=fieldMembers(fieldResource(copied,0x2092506));const int index=u8(bytes,0x654a0+10*9+7);
            auto moved=points[index].data;put32(moved,28,u32(moved,28)+1);
            replaceFieldMemberInPlace(bytes,0x2092506,index,moved);
            QCOMPARE(fieldMembers(fieldResource(Rom::fromBytes(bytes),0x2092506))[index].data,moved);
            QCOMPARE(BattlefieldClone::capture(Rom::fromBytes(bytes),1,false),source);
            auto second=BattlefieldClone::capture(rom,2,false);
            bytes=BattlefieldClone::apply(Rom::fromBytes(prepare(Rom::fromBytes(bytes),22,2)),22,second);
            QCOMPARE(BattlefieldClone::modelCount(Rom::fromBytes(bytes)),47);
            BattlefieldClone::linkRuntime(bytes,0x80188000);
            auto restored=BattlefieldClone::apply(Rom::fromBytes(prepare(Rom::fromBytes(bytes),10,1)),10,source);
            QCOMPARE(BattlefieldClone::modelCount(Rom::fromBytes(restored)),47);
            BattlefieldClone::linkRuntime(restored,0x80188100);
            BattlefieldClone::validate(Rom::fromBytes(restored),10,source);
            BattlefieldClone::validate(Rom::fromBytes(restored),22,second);
            const auto unchanged=restored;
            BattlefieldClone::linkRuntime(restored,0x80188100);QCOMPARE(restored,unchanged);
            restored=BattlefieldClone::apply(Rom::fromBytes(prepare(Rom::fromBytes(restored),10,1)),10,source);
            QCOMPARE(restored,unchanged);
            Rom::updateChecksum(restored);
            const auto directory=qEnvironmentVariable("LORDLY_BATTLEFIELD_FIXTURE_DIR");
            if(!directory.isEmpty()){
                QDir().mkpath(directory);const auto path=QDir(directory).filePath(QFileInfo(rom.sourcePath()).fileName());write(path,restored);
                QJsonArray resources;
                for(const auto &pair:{qMakePair(10,source),qMakePair(22,second)}){
                    const auto r=Rom::fromBytes(restored);const int at=0x654a0+pair.first*9,model=u8(restored,at+3);
                    resources.append(QJsonObject{{"root",double(BattlefieldClone::resourceKey(r,0x1e39dce))},{"index",int(u32(BattlefieldClone::modelRow(r,model),0))},{"member",0},{"data",QString::fromLatin1(fieldMembers(QByteArray::fromBase64(pair.second["geometry"].toString().toLatin1())).first().data.toHex())}});
                    resources.append(QJsonObject{{"root",double(BattlefieldClone::resourceKey(r,0x1e39dce))},{"index",int(u32(BattlefieldClone::modelRow(r,model),0))},{"member",1},{"data",QString::fromLatin1(fieldMembers(QByteArray::fromBase64(pair.second["geometry"].toString().toLatin1()))[1].data.toHex())}});
                    resources.append(QJsonObject{{"root",double(BattlefieldClone::resourceKey(r,0x20712ea))},{"index",int(u8(restored,at+5))},{"member",0},{"data",QString::fromLatin1(fieldMembers(QByteArray::fromBase64(pair.second["terrain"].toString().toLatin1())).first().data.toHex())}});
                    resources.append(QJsonObject{{"root",double(BattlefieldClone::resourceKey(r,0x2092506))},{"index",int(u8(restored,at+7))},{"member",-1},{"data",QString::fromLatin1(QByteArray::fromBase64(pair.second["points"].toString().toLatin1()).toHex())}});
                    resources.append(QJsonObject{{"root",double(BattlefieldClone::resourceKey(r,0x2091e8c))},{"index",int(u8(restored,at+4))},{"member",-1},{"data",QString::fromLatin1(QByteArray::fromBase64(pair.second["bounds"].toString().toLatin1()).toHex())}});
                }
                write(QDir(directory).filePath("battlefield.json"),QJsonDocument(QJsonObject{{"rom",path},{"resources",resources}}).toJson());
            }
        }catch(const std::exception &e){QFAIL(e.what());}
    }
    void allAdditionalBattlefields() {
        if (qEnvironmentVariableIsEmpty("LORDLY_TEST_ROM")) QSKIP("Private ROM required");
        try {
            const auto retail = input();
            const auto scenarios = Scenarios::load(retail);
            const auto worlds = ScenarioWorld::load(retail);
            const auto captured = MissionBattlefield::capture(retail, scenarios, worlds, 1, "blank");
            const auto art = BattlefieldClone::capture(retail, 1, true, {0, 6});
            auto image = retail.bytes();
            QJsonArray snapshots;
            QSet<int> models;
            QVector<int> keys{10, 22, 35, 62};
            for (int key = MissionTables::firstKey; key < MissionTables::keyCount; ++key)
                keys << key;
            QCOMPARE(keys.size(), 68);
            for (const int key : keys) {
                auto units = captured;
                auto scenario = units["scenario"].toObject();
                auto layout = Eset::decode(QByteArray::fromHex(scenario["layout"].toString().toLatin1()));
                layout.setStartTime(key % 24, key % 60);
                layout.setBaseLevel(1 + key % 99);
                scenario["layout"] = QString::fromLatin1(layout.encode().toHex());
                units["scenario"] = scenario;
                image = MissionBattlefield::prepare(Rom::fromBytes(image), key, units);
                image = BattlefieldClone::apply(Rom::fromBytes(image), key, art);
                const auto descriptor = ScenarioResources::row(Rom::fromBytes(image), key);
                models.insert(u8(descriptor, 3));
                snapshots.append(QJsonObject{{"key", key}, {"source", 1}, {"mode", "blank"},
                    {"name", QString("Capacity fixture %1").arg(key)},
                    {"snapshot", QJsonObject{{"units", units}, {"art", art}}}});
            }
            image = writeScenarioBattlefields(Rom::fromBytes(image), snapshots);
            const auto reopened = Rom::fromBytes(image);
            const auto resources = ScenarioResources::load(reopened);
            const auto world = WorldResources::load(reopened);
            QCOMPARE(readScenarioBattlefields(reopened), snapshots);
            QCOMPARE(models.size(), 68);
            QCOMPARE(BattlefieldClone::modelCount(reopened), 113);
            QCOMPARE(ScenarioResources::row(reopened, 67), ScenarioResources::row(retail, 67));
            QCOMPARE(BattlefieldClone::modelRow(reopened, 4), BattlefieldClone::modelRow(retail, 4));
            for (const int key : keys) {
                const auto layout = Eset::decode(resources.scenario(reopened, key));
                QCOMPARE(layout.startMinute(), key % 60);
                QCOMPARE(layout.baseLevel(), 1 + key % 99);
                QCOMPARE(ScenarioWorld::declaredSiteCount(world.sites(reopened, key)), 2);
                QCOMPARE(world.treasure(reopened, key), QByteArray(1, 0));
            }
            const int highest = MissionTables::keyCount - 1;
            QVERIFY(resources.selector(highest) >= 128);
            QCOMPARE(MissionTables::row(reopened, MissionTables::Camera, highest),
                     MissionTables::row(retail, MissionTables::Camera, 1));
            BattlefieldClone::validate(reopened, highest, art);
            QCOMPARE(MapEdits::load(reopened).source(highest).map.points.size(), 2);
            QCOMPARE(BattlefieldClone::apply(reopened, highest, art), image);
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void blankHasOnlyFlatGround(){
        if(qEnvironmentVariableIsEmpty("LORDLY_TEST_ROM"))QSKIP("Private ROM required");
        try{const auto rom=input();auto blank=BattlefieldClone::capture(rom,1,true,{0,6});
            const auto bytes=BattlefieldClone::apply(Rom::fromBytes(prepare(rom,10,1)),10,blank);
            auto maps=MapEdits::load(Rom::fromBytes(bytes));const auto &s=maps.source(10);
            QCOMPARE(s.field.buildingCount(),2);QVERIFY(s.field.triangleCount()>2);QCOMPARE(s.map.points.size(),2);
            QCOMPARE(s.terrain[0].data,QByteArray(65536,char(3)));QCOMPARE(s.terrain[1].data,QByteArray(4096,char(3)));
            // The native grass preset draws its own flat two-triangle surface
            // (ROM F89B0); the editable ground quad is flat as well.
            int ground=0;for(const auto &triangle:s.field.triangles())if(triangle.group<0){++ground;for(const auto &point:triangle.points)QCOMPARE(point.y(),0.f);}QVERIFY(ground>=2);
            QCOMPARE(s.field.siteGroups().size(),2);
            maps.paint(10,{{120,120},{135,135}},5,4);QVERIFY(maps.model(10)!=s.geometry[0].data);
            const auto exported=Rom::fromBytes(maps.rebuild(Rom::fromBytes(bytes)));
            QVERIFY(MapEdits::load(exported).source(10).field.triangleCount()>2);
            QCOMPARE(BattlefieldClone::capture(exported,1,false),BattlefieldClone::capture(rom,1,false));
        }catch(const std::exception &e){QFAIL(e.what());}
    }
};
QTEST_MAIN(BattlefieldCloneTests)
#include "battlefield_clone_tests.moc"
