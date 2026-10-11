#include "engine/route_preview.h"
#include "engine/project.h"
#include "engine/squad_members.h"
#include "game/scenarios/route_path.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>
#include <array>
using namespace lordly;
struct RouteCase {
    QString name;
    QByteArray terrain = QByteArray(4096, 0), transitions = QByteArray(1024, 0), blocked;
    std::array<uint16_t,32> costs;
    int start = 65, goal = 650;
    RouteCase(QString name) : name(std::move(name)) {
        costs.fill(65535); costs[1] = 1; costs[2] = 2; costs[3] = 8;
        for (int y=1;y<=10;++y) for(int x=1;x<=10;++x) terrain[y*64+x]=1;
    }
    QJsonObject json() const {
        QJsonArray row; for(auto c:costs) row << int(c);
        return {{"name",name},{"terrain",QString::fromLatin1(terrain.toHex())},
                {"transitions",QString::fromLatin1(transitions.toHex())},
                {"costs",row},{"start",start},{"goal",goal},{"blocked",QString::fromLatin1(blocked.toHex())}};
    }
    QJsonObject run() const {
        uint16_t path[4096]; int n=0; uint32_t cost=0;
        const int result=lordly_route_path(reinterpret_cast<const uint8_t*>(terrain.constData()),costs.data(),
            reinterpret_cast<const uint8_t*>(transitions.constData()),blocked.isEmpty()?nullptr:
            reinterpret_cast<const uint8_t*>(blocked.constData()),start,goal,path,&n,&cost);
        QJsonArray steps; for(int i=0;i<n;++i) steps << int(path[i]);
        return {{"name",name},{"reached",result==1},{"path",steps},{"cost",int(cost)}};
    }
};
static QVector<RouteCase> cases() {
    QVector<RouteCase> out{RouteCase("flat"),RouteCase("same cell"),RouteCase("gap"),RouteCase("unreachable"),
        RouteCase("terrain cost"),RouteCase("directional transition"),RouteCase("blocked destination exception"),
        RouteCase("blocked start diagonal"),RouteCase("dynamic mask")};
    out[1].goal=out[1].start;
    for(int y=0;y<64;++y) out[2].terrain[y*64+5]=out[3].terrain[y*64+5]=0;
    out[2].terrain[6*64+5]=1;
    for(int y=2;y<10;++y) for(int x=2;x<10;++x) out[4].terrain[y*64+x]=3;
    for(int y=1;y<=10;++y) for(int x=6;x<=10;++x) out[5].terrain[y*64+x]=2;
    out[5].transitions[1*32+2]=1;
    out[6].terrain[out[6].goal]=0;
    out[7].terrain[out[7].start]=0;out[7].terrain[66]=0;out[7].terrain[129]=0;
    out[8].blocked=QByteArray(4096,0);
    for(int y=1;y<9;++y)out[8].blocked[y*64+5]=1;
    quint32 seed=19;
    for(int n=0;n<16;++n){RouteCase c(QString("mixed %1").arg(n));
        for(int y=1;y<=10;++y)for(int x=1;x<=10;++x){seed=seed*1664525U+1013904223U;c.terrain[y*64+x]=char((seed>>24)%4);}
        c.terrain[c.start]=c.terrain[c.goal]=1;out<<c;
    }
    return out;
}
class RoutePreviewTests:public QObject{
 Q_OBJECT
 private slots:
 void terrainAndLimits(){
    auto rows=cases();
    QVERIFY(rows[0].run()["reached"].toBool()); QCOMPARE(rows[0].run()["cost"].toInt(),126);
    QCOMPARE(rows[1].run()["path"].toArray(),QJsonArray{65});
    QVERIFY(rows[2].run()["reached"].toBool());QVERIFY(!rows[3].run()["reached"].toBool());
    QVERIFY(rows[6].run()["reached"].toBool());QVERIFY(!rows[7].run()["reached"].toBool());
    QVERIFY(rows[4].run()["cost"].toInt()>rows[0].run()["cost"].toInt());
    uint16_t path[4096];int count;uint32_t cost;
    auto c=rows[0];c.terrain[4]=32;
    QCOMPARE(lordly_route_path(reinterpret_cast<const uint8_t*>(c.terrain.constData()),c.costs.data(),
        reinterpret_cast<const uint8_t*>(c.transitions.constData()),nullptr,65,650,path,&count,&cost),-1);
 }
 void originalInstructions(){
    const auto rom=qEnvironmentVariable("LORDLY_TEST_ROM");if(rom.isEmpty())QSKIP("Private ROM required");
    QTemporaryDir work;QJsonArray inputs,expected;
    for(const auto &c:cases()){inputs<<c.json();expected<<c.run();}
    QFile file(work.filePath("inputs.json"));QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(QJsonObject{{"rom",rom},{"cases",inputs}}).toJson());file.close();
    QProcess probe;probe.start("node",{QFileInfo(QString::fromUtf8(__FILE__)).dir().filePath("route-path-mips.cjs"),file.fileName(),work.filePath("results.json")});
    QVERIFY(probe.waitForFinished(120000));QVERIFY2(probe.exitCode()==0,probe.readAllStandardError().constData());
    QFile result(work.filePath("results.json"));QVERIFY(result.open(QIODevice::ReadOnly));
    const auto actual=QJsonDocument::fromJson(result.readAll()).array();QCOMPARE(actual.size(),expected.size());
    for(int i=0;i<actual.size();++i)QVERIFY2(actual[i]==expected[i],qPrintable(QString::fromUtf8(QJsonDocument(QJsonObject{{"actual",actual[i]},{"expected",expected[i]}}).toJson())));
    qInfo().noquote()<<probe.readAllStandardOutput();
 }
 void editedTerrainAndExport(){
    const auto file=qEnvironmentVariable("LORDLY_TEST_ROM");if(file.isEmpty())QSKIP("Private ROM required");
    const auto rom=Rom::open(file);auto project=Project::fromRom(rom);project.loadScenarios();project.loadMapEdits();
    auto maps=*project.mapEdits();auto scenarios=*project.scenarios();auto classes=project.classData();
    const int key=5,row=1;
    // Controlled broad terrain makes an unambiguous crossing and detour.
    maps.setBoundary(key,QRectF(QPointF(8,8),QPointF(248,248)));
    maps.paint(key,{{40,100},{180,100}},3,25);
    maps.paint(key,{{110,72},{110,128}},8,5);
    auto d=scenarios.scenario(key).layout.deployments[row];put8(d,3,50);put8(d,4,100);
    scenarios.setDeployment(key,row,d,classes);scenarios.setRouteGroup(key,row,0,0,classes);
    scenarios.setSquadCommand(key,row,{SquadBehavior::Move,170,100},classes);
    auto members=squadMembers(scenarios.scenario(key).squads[row]);
    for(auto &m:members)m.classId=2;
    scenarios.setSquad(key,row,editSquadMembers(scenarios.scenario(key).squads[row],members),classes);
    for(const auto &m:members){auto c=classes.entries()[m.classId];put8(c.record,44,2);classes.set(m.classId,c);}
    const auto ground=RoutePreview::squad(rom,scenarios,classes,maps,key,row);
    QVERIFY2(ground.reachable,qPrintable(ground.message));QVERIFY(ground.cells.size()>2);
    for(int i=1;i+1<ground.cells.size();++i)QVERIFY(u8(maps.movementTerrain(key),ground.cells[i])!=8);
    for(const auto &m:members){auto c=classes.entries()[m.classId];put8(c.record,44,1);classes.set(m.classId,c);}
    const auto flying=RoutePreview::squad(rom,scenarios,classes,maps,key,row);
    QVERIFY2(flying.reachable,qPrintable(flying.message));QVERIFY(flying.cells!=ground.cells);
    // Export must use precisely the preview grid, including boundary edits.
    const auto grid=maps.movementTerrain(key);const auto plan=maps.plan();
    const auto part=std::find_if(plan.begin(),plan.end(),[&](const auto &p){return p.terrainIndex==maps.source(key).terrainIndex;});
    QVERIFY(part!=plan.end());QCOMPARE(fieldMembers(part->terrain)[1].data,grid);
    auto rebuilt=maps.rebuild(rom);Rom::updateChecksum(rebuilt);
    const auto checksum=rebuilt.mid(0x10,8);auto checked=rebuilt;Rom::updateChecksum(checked);
    QCOMPARE(checked.mid(0x10,8),checksum);
    const auto candidates=QFileInfo(file).dir().filePath("Test ROMs");QVERIFY(QDir().mkpath(candidates));
    QTemporaryDir exported(candidates+"/terrain-route-XXXXXX");QVERIFY(exported.isValid());
    QFile candidate(exported.filePath(QFileInfo(file).fileName()));
    QVERIFY(candidate.open(QIODevice::WriteOnly));QCOMPARE(candidate.write(rom.serialize(rebuilt)),rebuilt.size());candidate.close();
    const auto exportedRom=Rom::open(candidate.fileName());
    QCOMPARE(exportedRom.bytes(),rebuilt);QCOMPARE(exportedRom.byteOrder(),rom.byteOrder());
    const auto reopened=MapEdits::load(exportedRom);
    QCOMPARE(reopened.movementTerrain(key),grid);
    const auto replay=RoutePreview::squad(Rom::fromBytes(rebuilt),scenarios,classes,reopened,key,row);
    QCOMPARE(replay.cells,flying.cells);
    const auto original=maps.json();maps.restore(key);QVERIFY(maps.movementTerrain(key)!=grid);
    maps.apply(original);QCOMPARE(maps.movementTerrain(key),grid);
    maps.setBoundary(key,QRectF(QPointF(8,8),QPointF(150,248)));
    const auto outside=RoutePreview::squad(rom,scenarios,classes,maps,key,row);
    QVERIFY(!outside.reachable);QVERIFY(outside.message.contains("boundary"));maps.apply(original);
    // A selected follower previews the leader's path with the group's slower type.
    auto grouped=scenarios;
    auto second=members;for(auto &m:second)m.classId=3;
    grouped.setSquad(key,2,editSquadMembers(grouped.scenario(key).squads[2],second),classes);
    auto c=classes.entries()[3];put8(c.record,44,2);classes.set(3,c);
    grouped.setRouteGroup(key,1,240,1,classes);grouped.setRouteGroup(key,2,240,1,classes);
    const auto lead=RoutePreview::squad(rom,grouped,classes,maps,key,1);
    const auto follower=RoutePreview::squad(rom,grouped,classes,maps,key,2);
    QVERIFY2(follower.reachable,qPrintable(follower.message));QCOMPARE(follower.leaderRow,1);
    QCOMPARE(follower.movementType,2);QCOMPARE(follower.cells,lead.cells);
    QCOMPARE(follower.cells,ground.cells);
    auto changedCode=rom.bytes();put8(changedCode,0x107850,u8(changedCode,0x107850)^1);
    const auto unsupported=RoutePreview::squad(Rom::fromBytes(changedCode),scenarios,classes,maps,key,row);
    QVERIFY(!unsupported.reachable);QVERIFY(unsupported.message.contains("changed movement code"));
 }
};
QTEST_GUILESS_MAIN(RoutePreviewTests)
#include "route_preview_tests.moc"
