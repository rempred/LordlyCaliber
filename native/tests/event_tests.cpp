#include "engine/project.h"
#include "engine/promotion.h"
#include "engine/event_script.h"
#include "engine/campaign_runtime.h"
#include "rebuild/compiler.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class EventTests : public QObject {
    Q_OBJECT
  private slots:
    void fullRetailDescriptions() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            int programs = 0, words = 0;
            for (int row = 0; row < 115; ++row) {
                if (row == 2 || row == 3) continue;
                const auto bytes = SceneEvents::retailRow(rom, row);
                const auto before = bytes;
                const auto handlers = EventScript::display(bytes, row);
                QVERIFY2(!handlers.isEmpty(), qPrintable(QString("Missing event row %1").arg(row)));
                int previous = -1;
                for (const auto &handler : handlers) {
                    QVERIFY(!handler.title.isEmpty());
                    for (const auto &step : handler.steps) {
                        QVERIFY(step.researched);
                        QVERIFY(step.offset > previous); previous = step.offset;
                        QVERIFY(!step.text.isEmpty());
                        QVERIFY(!step.text.startsWith("Game command "));
                        QVERIFY(step.offset + step.length <= bytes.size());
                        words += step.length / 2;
                    }
                }
                QCOMPARE(bytes, before);
                QCOMPARE(EventScript::apply(bytes, {}), before);
                ++programs;
            }
            // Counts come from the independently audited native VM corpus, not
            // the older viewer's CFG (which missed shared and direct programs).
            QCOMPARE(programs, 113); QCOMPARE(words, 55471);
            QMap<int, QString> rebel;
            for (const auto &handler : EventScript::display(SceneEvents::retailRow(rom, 28), 28))
                for (const auto &step : handler.steps) rebel[step.offset] = step.text;
            QVERIFY(rebel[318].contains("R1 (flag to check) = 201"));
            QVERIFY(rebel[320].contains("R0 (flag value: ON/OFF)"));
            QVERIFY(rebel[388].contains("R1 (hour)"));
            QVERIFY(!rebel[388].contains("flag to check"));
            QVERIFY(EventScript::flagName(169).contains("Ankiseth"));
            QVERIFY(EventScript::flagName(202, 28).contains("Biske"));
            const auto original = SceneEvents::retailRow(rom, 10);
            auto changed = original; changed[0xa5] = char(14); // Katreda -> Asnabel template.
            bool sawRecruitment = false;
            for (const auto &handler : EventScript::display(changed, 10))
                for (const auto &step : handler.steps) {
                    QVERIFY(!step.researched);
                    if (step.offset == 0xa8) {
                        QVERIFY(step.text.contains("Asnabel"));
                        QVERIFY(!step.text.contains("Katreda"));
                        sawRecruitment = true;
                    }
                }
            QVERIFY(sawRecruitment);
            // A direct-routing program cannot borrow original story labels
            // after an operand changes, even though its physical layout matches.
            const auto world = SceneEvents::retailRow(rom, 0);
            auto modified = world;
            const int first = EventScript::display(world, 0).first().steps.first().offset;
            modified[first+1] = char(u8(world, first+1) ^ 1);
            for (const auto &handler : EventScript::display(modified, 0))
                for (const auto &step : handler.steps) QVERIFY(!step.researched);
            // Names are read-only annotations. Reinstating the original bytes
            // reinstates the correct contextual description.
            QVERIFY(EventScript::display(original, 10).first().steps.first().researched);
            for (const auto &handler : EventScript::display(original, 10, false))
                for (const auto &step : handler.steps) QVERIFY(!step.researched);
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void semanticEventInstructions() {
        const auto program = [](const char *code) {
            return QByteArray::fromHex("000c000c000c000c000c000c0010ffff") + QByteArray::fromHex(code);
        };
        const auto lines = [](const QByteArray &bytes, int row = 10) {
            QStringList result;
            const auto handlers = EventScript::decode(bytes, row);
            for (const auto &step : handlers.first().steps) result << step.text;
            return result;
        };
        QCOMPARE(EventScript::flagName(202, 10), QString("Flag 202 (Katreda encountered this mission)"));
        QCOMPARE(EventScript::flagName(202, 77), QString("Flag 202"));
        QCOMPARE(EventScript::flagName(202), QString("Flag 202"));
        QCOMPARE(EventScript::flagName(250, 10), QString("Flag 250"));
        auto text = lines(program("680f8802160113162f00"));
        QVERIFY(text[0].contains("R0 = 15"));
        QVERIFY(text[0].contains("low byte of recruitment template 527 (Katreda)"));
        QVERIFY(text[1].contains("R0 = 527"));
        QCOMPARE(text[2], QString("Queue individual recruitment from recruitment template 527 (Katreda)"));
        QVERIFY(text[3].contains("Process queued army operation"));
        QVERIFY(lines(program("68d5880116002f00"))[2].contains("squad recruitment"));
        QVERIFY(lines(program("680e880216012f00"))[2].contains("526 (Asnabel)"));
        QVERIFY(!lines(program("6810880216012f00")).join('\n').contains("Katreda"));
        // Different incoming templates must not borrow either branch's name.
        QVERIFY(!lines(program("680f8802ac02680e880216012f00")).last().contains("Katreda"));
        text = lines(program("680f8802ac02680e880216012f00"));
        QCOMPARE(text[text.size()-2], QString("Queue individual recruitment from R0 (recruitment template)"));
        // A call / unrecognized command can change registers. Do not infer past it.
        text = lines(program("680f88023f0016012f00"));
        QCOMPARE(text[3], QString("Queue individual recruitment from R0 (recruitment template)"));
        QVERIFY(!text.join('\n').contains("Katreda"));
        // R1 is an index, not a flag operation; branching into a read keeps it separate.
        text = lines(program("69ca01006061ac002f00"));
        QVERIFY(text[0].contains("R1 = 202 (Flag 202 (Katreda encountered this mission))"));
        QVERIFY(text[2].startsWith("READ Flag 202"));
        QVERIFY(text[3].startsWith("CHECK Flag 202 (Katreda encountered this mission) = OFF"));
        text = lines(program("6862690171602f00"));
        QCOMPARE(text[0], QString("SET Flag 98 (Asnabel recruitment recorded) = ON"));
        text = lines(program("6862690071602f00"));
        QCOMPARE(text[0], QString("CLEAR Flag 98 (Asnabel recruitment recorded) = OFF"));
        text = lines(program("69ca6061ac002f00"));
        QVERIFY(text[0].startsWith("CHECK Flag 202 (Katreda encountered this mission) = OFF"));
        text = lines(program("69036aff6391ab002f00"));
        QCOMPARE(text[0], QString("Prepare R1 = 3 (dialogue answer control)"));
        QVERIFY(text[1].contains("comparison value for dialogue answer"));
        QCOMPARE(text[2], QString("READ dialogue answer, control 3 → R3"));
        QVERIFY(text[3].startsWith("CHECK R2 (255) = R3 (dialogue answer, control 3)"));
        text = lines(program("68021100"));
        QVERIFY(text.last().startsWith("Request transition 2"));
        QVERIFY(!text.last().contains("Switch"));
        text = lines(program("68c888ff120a"));
        QCOMPARE(text.last(), QString("Switch to event row 10; CLEAR Flags 200–254"));
        const auto intro = QByteArray::fromHex("00020006ffff6862690171602f00");
        const auto introHandlers = EventScript::decode(intro, 113);
        QCOMPARE(introHandlers.size(), 1);
        QCOMPARE(introHandlers.first().steps.first().offset, 6);
        QCOMPARE(introHandlers.first().steps.first().text, QString("SET Flag 98 (Asnabel recruitment recorded) = ON"));
        QVERIFY_THROWS_EXCEPTION(Error, EventScript::decode(QByteArray::fromHex("00020002ffff2f00")));
    }
    void retailEventSemanticLabels() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            const auto stepMap = [](const QByteArray &bytes, int row) {
                QMap<int, EventStep> result;
                for (const auto &handler : EventScript::decode(bytes, row))
                    for (const auto &step : handler.steps) result[step.offset] = step;
                return result;
            };
            const auto gunther = SceneEvents::retailRow(rom, 10);
            const auto steps = stepMap(gunther, 10);
            QVERIFY(steps[0xa4].text.contains("527 (Katreda)"));
            QVERIFY(steps[0xa6].text.contains("R0 = 527"));
            QVERIFY(steps[0xa8].text.contains("individual recruitment"));
            QVERIFY(steps[0xa8].text.contains("Katreda"));
            QVERIFY(steps[0x420].text.contains("526 (Asnabel)"));
            QVERIFY(steps[0x424].text.startsWith("SET Flag 98 (Asnabel recruitment recorded) = ON"));
            bool encountered = false;
            for (const auto &step : steps) if (step.flag == 202) {
                encountered = true; QVERIFY(step.text.contains("Katreda encountered this mission"));
            }
            QVERIFY(encountered);
            const auto edited = EventScript::apply(gunther, {{"1060", QJsonObject{{"flag", 250}, {"value", false}}}});
            QCOMPARE(stepMap(edited, 10)[0x424].text, QString("CLEAR Flag 250 = OFF"));
            auto expected = gunther; put8(expected, 0x425, 250); expected[0x427] = 0;
            QCOMPARE(edited, expected);
            const auto ankiseth = stepMap(SceneEvents::retailRow(rom, 113), 113);
            bool foundSquad = false, foundPrerequisite = false;
            for (const auto &step : ankiseth) {
                foundSquad |= step.text.contains("Queue squad recruitment from recruitment template 469 (Ankiseth and four companions)");
                foundPrerequisite |= step.flag == 17 && step.text.contains("Magnus refused to execute Frederick");
            }
            QVERIFY(foundSquad); QVERIFY(foundPrerequisite);
            const auto wentinus = stepMap(SceneEvents::retailRow(rom, 22), 22);
            bool foundPeace = false;
            for (const auto &step : wentinus)
                foundPeace |= step.text.contains("scenario 21 (Wentinus: fight Baldwin)");
            QVERIFY(foundPeace);
            // Reading annotations is not an edit, and a no-edit byte writer is exact.
            QCOMPARE(EventScript::apply(gunther, {}), gunther);
            QCOMPARE(SceneEvents::retailRow(rom, 10), gunther);
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void interludeFlagsProjectAndExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"), tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty()) QSKIP("Private ROM and N64 tools required");
        try {
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom); project.loadSceneEvents();
            const auto original = project.sceneEvents()->program(113);
            const auto controls = EventScript::flagSteps(original);
            int at = -1;
            for (const auto &step : controls) if (step.flag == 17 && step.check) { at = step.offset; break; }
            QVERIFY(at >= 0);
            auto events = *project.sceneEvents();
            events.editFlag(113, at, 17, !controls[at].value);
            project.setSceneEvents(events.json());
            auto expected = original; put8(expected, at + 4, controls[at].value ? 0xac : 0xbc);
            QCOMPARE(project.sceneEvents()->program(113), expected);
            QTemporaryDir folder; project.saveFolder(folder.filePath("project"));
            auto restored = Project::fromRom(rom);
            restored.applyManifest(Project::readProject(folder.filePath("project")));
            QCOMPARE(restored.sceneEvents()->program(113), expected);
            const auto built = rebuild(restored, tools, folder.path());
            auto checked = built.image; Rom::updateChecksum(checked); QCOMPARE(checked, built.image);
            const auto exported = Rom::fromBytes(built.image);
            QCOMPARE(SceneEvents::readRow(exported, 113), expected);
            QCOMPARE(SceneEvents::retailRow(exported, 113), original);
            const auto reopened = SceneEvents::load(exported);
            QCOMPARE(reopened.json(), events.json());
            QDir parent(QFileInfo(path).absolutePath()); QVERIFY(parent.mkpath("Test ROMs"));
            QTemporaryDir output(parent.filePath("Test ROMs/event-row-labels-XXXXXX"));
            QVERIFY(output.isValid()); output.setAutoRemove(false);
            const auto candidate = output.filePath(QFileInfo(path).fileName());
            exportRom(restored, built, candidate);
            QCOMPARE(SceneEvents::load(Rom::open(candidate)).program(113), expected);
            qInfo().noquote() << "Chapter-introduction event export:" << candidate;
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void campaignMapBranchesAndExport() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM"),tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if(path.isEmpty()||tools.isEmpty())QSKIP("Private ROM and N64 tools required");
        try {
            const auto rom=Rom::open(path);auto project=Project::fromRom(rom);project.loadCampaignMap();project.loadSceneEvents();
            auto map=*project.campaignMap();QCOMPARE(map.rebuild(rom.bytes()),rom.bytes());
            QTemporaryDir untouched;QCOMPARE(rebuild(project,tools,untouched.path()).image,rom.bytes());
            const QSet<int> castles{7,11,17,18,20,22,27,31,36,39};
            for(int node=1;node<=40;++node)QCOMPARE(map.icon(node),castles.contains(node)?1:0);
            QVERIFY_THROWS_EXCEPTION(Error,map.setIcon(0,0));QVERIFY_THROWS_EXCEPTION(Error,map.setIcon(41,0));
            QVERIFY_THROWS_EXCEPTION(Error,map.setIcon(1,2));
            for(int node=1;node<=40;++node)map.setIcon(node,1);
            auto invalid=map.json();invalid["icons"]=QJsonObject{{"6",2}};
            const auto beforeInvalid=map.json();QVERIFY_THROWS_EXCEPTION(Error,map.apply(invalid));QCOMPARE(map.json(),beforeInvalid);
            const auto spare=map.availableNodes();QVERIFY(!spare.isEmpty());const int id=spare.first();
            QVERIFY(id<=40);QVERIFY(!map.availableNodes().contains(49));
            const auto oldDots=map.roadDots(5);const auto bg=map.background();QCOMPARE(bg.width,512);QCOMPARE(bg.height,320);
            map.setNode(6,{280,252},map.label(6));QVERIFY(map.roadDots(5)!=oldDots);
            map.setNode(id,{230,250},4);map.setName(id,"New Reach");QCOMPARE(map.addRoad(6,id),41);map.addRoad(id,8);
            QVERIFY_THROWS_EXCEPTION(Error,map.addRoad(id,6));
            project.setCampaignMap(map.json());
            QVERIFY(!project.campaignMap()->availableNodes().contains(id));
            QCOMPARE(project.campaignMap()->position(id),QPoint(230,250));
            auto events=*project.sceneEvents();
            const QJsonObject rule{{"event",5},{"conditions",QJsonArray{}},{"clearFrom",200},{"clearTo",255}};
            events.setWorldRules(id,QJsonArray{rule});events.unlockNodeAfter(4,id,true);
            auto conditional=rule;conditional["conditions"]=QJsonArray{QJsonObject{{"flag",250},{"value",true}}};
            events.setWorldRules(6,QJsonArray{conditional});
            StoryRoutes original(rom),edited(rom);edited.scripts[0]=events.program(0);
            for(int chaos:{0,25,50,75,100}) {
                QCOMPARE(edited.routes(6,chaos,{{250,0}}),original.routes(6,chaos,{{250,0}}));
                const auto matches=edited.routes(6,chaos,{{250,1}});QCOMPARE(matches.size(),1);QCOMPARE(matches.first().event,5);
            }
            edited.nodes[id]={"New location",{230,250}};QCOMPARE(edited.routes(id).first().event,5);
            const auto before=SceneEvents::retailRow(rom,4),after=events.program(4);
            QCOMPARE(after.first(10),before.first(10));QCOMPARE(after.mid(12,before.size()-12),before.mid(12));
            bool unlock=false;for(const auto &handler:EventScript::decode(after))if(handler.group==5&&handler.entry>=before.size())unlock=true;QVERIFY(unlock);
            const auto controls=EventScript::branchSteps(before);QVERIFY(!controls.isEmpty());
            const auto branch=controls.first();events.editBranch(4,branch.offset,branch.offset+branch.length);
            QCOMPARE(EventScript::branchSteps(events.program(4))[branch.offset].target,branch.offset+branch.length);
            project.setSceneEvents(events.json());QVERIFY(project.changedRecords()>0);
            QVERIFY(project.changeReport().text().contains("World Map"));QVERIFY(project.changeReport().text().contains("Storyline"));
            QTemporaryDir work;project.saveFolder(work.filePath("campaign"));auto saved=Project::fromRom(rom);saved.applyManifest(Project::readProject(work.filePath("campaign")));
            QCOMPARE(saved.campaignMap()->position(id),QPoint(230,250));QCOMPARE(saved.sceneEvents()->json(),events.json());
            QCOMPARE(saved.campaignMap()->iconBytes(),QByteArray(5,char(255)));
            const auto built=rebuild(saved,tools,work.path());auto crc=built.image;Rom::updateChecksum(crc);QCOMPARE(crc,built.image);
            auto reopened=Project::fromRom(Rom::fromBytes(built.image));reopened.loadCampaignMap();reopened.loadSceneEvents();
            QCOMPARE(reopened.campaignMap()->position(id),QPoint(230,250));QCOMPARE(reopened.campaignMap()->name(id),map.name(id));
            QCOMPARE(reopened.campaignMap()->totalRoads(),42);QCOMPARE(reopened.campaignMap()->road(41),qMakePair(6,id));QCOMPARE(reopened.campaignMap()->dotBytes(),map.dotBytes());
            QCOMPARE(reopened.campaignMap()->iconBytes(),map.iconBytes());
            QCOMPARE(built.image.mid(0x81cf4,0x718),rom.bytes().mid(0x81cf4,0x718));
            QCOMPARE(built.image.mid(0x871a8,10),rom.bytes().mid(0x871a8,10));
            QCOMPARE(reopened.sceneEvents()->json(),events.json());QCOMPARE(SceneEvents::readRow(reopened.rom(),0),events.program(0));QCOMPARE(SceneEvents::readRow(reopened.rom(),4),events.program(4));
            QCOMPARE(rebuild(reopened,tools,work.path()).image,built.image);
            auto continued=reopened;auto continuedMap=*continued.campaignMap();continuedMap.setNode(id,{240,260},continuedMap.label(id));continuedMap.setRoad(42,id,3);continuedMap.setIcon(6,0);continued.setCampaignMap(continuedMap.json());
            const auto rebuilt=rebuild(continued,tools,work.path());QCOMPARE(rebuilt.image.size(),built.image.size());
            auto continuedRead=Project::fromRom(Rom::fromBytes(rebuilt.image));continuedRead.loadCampaignMap();QCOMPARE(continuedRead.campaignMap()->dotBytes(),continuedMap.dotBytes());QCOMPARE(continuedRead.campaignMap()->road(42),qMakePair(id,3));
            QCOMPARE(continuedRead.campaignMap()->icon(6),0);QCOMPARE(continuedRead.campaignMap()->icon(7),1);
            // Older layouts have seven bends, not just a different version word.
            // These reader fixtures model v1/v2/v3 data; their synthetic code is
            // not executed. Native travel is checked on the real exported v4 ROM.
            const auto placement=*CampaignPlacement::read(built.image);
            for (int version : {1,2,3}) {
                auto old=built.image;
                const auto offset=[&](quint32 address){return placement.overlayOffset+address-CampaignPlacement::ramStart;};
                const auto directory=placement.dotAddress+96*28,points=directory+400;
                const auto pointCount=u32(old,placement.descriptor+72);
                const auto display=old.mid(offset(placement.displayDirectory),400);
                const auto pointBytes=old.mid(offset(placement.displayPoints),pointCount*4);
                old.replace(offset(placement.dotAddress),96*28,QByteArray(96*28,0));
                for(int road=0;road<42;++road)
                    old.replace(offset(placement.dotAddress)+road*28,28,placement.dots.mid(road*56,28));
                old.replace(offset(directory),400,display);
                old.replace(offset(points),pointBytes.size(),pointBytes);
                put32(old,placement.descriptor+8,version);
                put32(old,placement.descriptor+64,version>=2?directory:0);
                put32(old,placement.descriptor+68,version>=2?points:0);
                if(version<3) {
                    old.replace(placement.overlayOffset+0x81d98-CampaignPlacement::sourceStart,40,rom.bytes().mid(0x81d98,40));
                    old.replace(offset(CampaignPlacement::codeAddress+8192+4),5,QByteArray(5,0));
                }
                if(version==1) {
                    put32(old,placement.descriptor+72,0);put32(old,placement.descriptor+76,0);
                    // Version 1 predates authored location names and display dots.
                    old.replace(placement.overlayOffset+0x86040-CampaignPlacement::sourceStart,1120,old.mid(0x86040,1120));
                    for(int node=1;node<=40;++node) {
                        const int name=0x864a0+(node-1)*8+4;
                        put32(old,name,u32(rom.bytes(),name));
                        put32(old,placement.overlayOffset+name-CampaignPlacement::sourceStart,u32(rom.bytes(),name));
                    }
                }
                old.replace(placement.descriptor+32,32,QByteArray::fromHex(sha256(old.mid(placement.overlayOffset,placement.overlaySize))));
                auto legacy=Project::fromRom(Rom::fromBytes(old));legacy.loadCampaignMap();
                for(int node=1;node<=40;++node)QCOMPARE(legacy.campaignMap()->icon(node),version>=3?1:(castles.contains(node)?1:0));
                QCOMPARE(legacy.campaignMap()->road(41),qMakePair(6,id));
                for(int road=1;road<=42;++road)
                    QCOMPARE(legacy.campaignMap()->roadWaypoints(road),map.roadWaypoints(road));
            }

            QDir parent(QFileInfo(path).absolutePath());QVERIFY(parent.mkpath("Test ROMs"));QTemporaryDir output(parent.filePath("Test ROMs/campaign-workspace-XXXXXX"));QVERIFY(output.isValid());output.setAutoRemove(false);
            const auto candidate=output.filePath(QFileInfo(path).fileName());exportRom(saved,built,candidate);qInfo().noquote()<<"Campaign candidate:"<<candidate;
        } catch(const std::exception &e){QFAIL(e.what());}
    }
    void flagInstructionControls() {
        // Six handler groups share this small script. The check branches to the
        // second return. Its destination byte must never change during flag edits.
        const auto bytes = QByteArray::fromHex("000c000c000c000c000c000c0010ffff68666901716069666061ac012f002f00");
        const auto controls = EventScript::flagSteps(bytes);
        QCOMPARE(controls.size(), 2);
        QCOMPARE(controls[16].flag, 102); QVERIFY(!controls[16].check); QVERIFY(controls[16].value);
        QVERIFY(controls[22].check); QCOMPARE(controls[22].target, 30);
        auto edited = EventScript::apply(bytes, {{"16", QJsonObject{{"flag", 250}, {"value", false}}},
                                                 {"22", QJsonObject{{"flag", 250}, {"value", true}}}});
        auto expected = bytes; expected[17] = char(250); expected[19] = 0; expected[23] = char(250); expected[26] = char(0xbc);
        QCOMPARE(edited, expected);
        QVERIFY_THROWS_EXCEPTION(Error, EventScript::apply(bytes, {{"18", QJsonObject{{"flag", 1}, {"value", true}}}}));
        QVERIFY_THROWS_EXCEPTION(Error, EventScript::apply(bytes, {{"16", QJsonObject{{"flag", 256}, {"value", true}}}}));
        QVERIFY_THROWS_EXCEPTION(Error, EventScript::apply(bytes, {{"16", QJsonObject{{"flag", 1}, {"value", 1}}}}));
        // An alternate handler can enter the middle of a would-be compound.
        // Such words must not be offered as one editable operation.
        auto shared = QByteArray::fromHex("000c000c000c000c000c000c00120014ffff6866690171602f00");
        QVERIFY(EventScript::flagSteps(shared).isEmpty());
    }
    void scenarioFlagsProjectAndExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"), tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty()) QSKIP("Private ROM and N64 tools required");
        try {
            const auto rom = Rom::open(path);
            int decoded = 0;
            for (int row = 4; row <= 109; ++row) {
                if (row >= 65 && row < 71) continue;
                QVERIFY(!EventScript::decode(SceneEvents::retailRow(rom, row)).isEmpty()); ++decoded;
            }
            QCOMPARE(decoded, 100);
            auto project = Project::fromRom(rom); project.loadSceneEvents();
            auto events = *project.sceneEvents();
            QCOMPARE(events.scenarioRows(1), QVector<int>({4, 71}));
            const auto initial = events.json();
            QVERIFY_THROWS_EXCEPTION(Error, events.editFlag(4, 0x2f0, 256, true));
            QCOMPARE(events.json(), initial);
            const auto original = SceneEvents::retailRow(rom, 4);
            const auto controls = EventScript::flagSteps(original);
            QCOMPARE(controls[0x2f0].flag, 102); QVERIFY(controls[0x2f0].value);
            events.editFlag(4, 0x2f0, 250, false);
            auto expected = original; expected[0x2f1] = char(250); expected[0x2f3] = 0;
            QCOMPARE(events.program(4), expected);
            auto restored = events; restored.restoreFlags(4); restored.validate(*project.scenePrograms());
            QCOMPARE(restored.json(), project.sceneEvents()->json());
            QVERIFY(!restored.modified()); QCOMPARE(restored.changedSites(), 0);
            // Compose a short-form launch requiring an appended stub with a flag edit.
            QString launch;
            for (const auto &site : SceneEvents::sites()) if (site.row == 4 && site.shortForm) { launch = site.id; break; }
            QVERIFY(!launch.isEmpty());
            auto scenes = *project.scenePrograms();
            const auto added = scenes.createScene("Flag integration scene", scenes.scenes().first(), false);
            const int selector = scenes.resource(added).selectors.first(); QVERIFY(selector > 255);
            project.setScenePrograms(scenes.json()); events.bind(launch, selector, scenes);
            project.setSceneEvents(events.json());
            QVERIFY(project.changeReport().text().contains("Tenne Plains: The Southern Reaches completed"));
            QVERIFY(project.changeReport().text().contains("Flag 250"));
            QTemporaryDir work;
            project.saveFolder(work.filePath("flags-project"));
            auto saved = Project::fromRom(rom); saved.applyManifest(Project::readProject(work.filePath("flags-project")));
            QCOMPARE(saved.sceneEvents()->json(), events.json());
            auto forged = events.json(); auto flags = forged["flags"].toObject();
            flags["4"] = QJsonObject{{"0", QJsonObject{{"flag", 1}, {"value", true}}}}; forged["flags"] = flags;
            QVERIFY_THROWS_EXCEPTION(Error, saved.setSceneEvents(forged));
            QCOMPARE(saved.sceneEvents()->json(), events.json());
            const auto built = rebuild(saved, tools, work.path());
            auto crc = built.image; Rom::updateChecksum(crc); QCOMPARE(crc, built.image);
            const auto exported = Rom::fromBytes(built.image);
            const auto placement = *SceneProgramResources::placement(exported);
            QCOMPARE(u32(built.image, SceneProgramResources::descriptorOffset + 8), quint32(5));
            QCOMPARE(placement.eventRows, QSet<int>{4});
            QCOMPARE(SceneEvents::readRow(exported, 4), events.program(4));
            QCOMPARE(SceneEvents::load(exported).json(), events.json());
            QCOMPARE(SceneEvents::retailRow(exported, 4), original);
            auto reopened = Project::fromRom(exported); reopened.loadSceneEvents();
            QCOMPARE(rebuild(reopened, tools, work.path()).image, built.image);
            auto restoredExport = *reopened.sceneEvents(); restoredExport.editFlag(4, 0x2f0, 102, true);
            restoredExport.restoreFlags(4); restoredExport.validate(*reopened.scenePrograms());
            QCOMPARE(restoredExport.json(), reopened.sceneEvents()->json());
            // Remove only the flag edit: preserve the other scene work and downgrade metadata.
            auto cleared = *reopened.sceneEvents(); cleared.editFlag(4, 0x2f0, 102, true);
            reopened.setSceneEvents(cleared.json());
            const auto cleanFlags = rebuild(reopened, tools, work.path());
            QCOMPARE(u32(cleanFlags.image, SceneProgramResources::descriptorOffset + 8), quint32(4));
            QCOMPARE(SceneEvents::load(Rom::fromBytes(cleanFlags.image)).target(launch), selector);
            QVERIFY(SceneEvents::load(Rom::fromBytes(cleanFlags.image)).flagEdits().isEmpty());
            QDir parent(QFileInfo(path).absolutePath()); QVERIFY(parent.mkpath("Test ROMs"));
            QTemporaryDir output(parent.filePath("Test ROMs/scenario-event-flags-XXXXXX"));
            QVERIFY(output.isValid()); output.setAutoRemove(false);
            const auto candidate = output.filePath(QFileInfo(path).fileName());
            exportRom(saved, built, candidate);
            QCOMPARE(SceneEvents::load(Rom::open(candidate)).json(), events.json());
            qInfo().noquote() << "Scenario event export:" << candidate;
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void launchRewriteCompatibility() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            QProcess generator;
            generator.start("node", {QFINDTESTDATA("browser-event-rows.cjs"), path,
                                     work.filePath("rows.json")});
            QVERIFY(generator.waitForFinished(30000));
            QVERIFY2(generator.exitCode() == 0, generator.readAllStandardError().constData());
            QFile input(work.filePath("rows.json"));
            QVERIFY(input.open(QIODevice::ReadOnly));
            const auto rows = QJsonDocument::fromJson(input.readAll()).object()["rows"].toArray();
            QCOMPARE(SceneEvents::sites().size(), 1998);
            int shortCount = 0, checked = 0;
            for (const auto value : rows) {
                const auto row = value.toObject();
                const auto index = row["row"].toInt();
                const auto retail = SceneEvents::retailRow(rom, index);
                QMap<QString, int> high, low;
                for (const auto &site : SceneEvents::sites())
                    if (site.row == index) {
                        high[site.id] = 319;
                        low[site.id] = 2;
                        shortCount += site.shortForm;
                        ++checked;
                    }
                QCOMPARE(SceneEvents::bindRow(retail, index, high),
                         QByteArray::fromHex(row["high"].toString().toLatin1()));
                const auto direct = SceneEvents::bindRow(retail, index, low);
                QCOMPARE(direct.size(), retail.size());
                auto changedLow = low;
                for (auto it = changedLow.begin(); it != changedLow.end();) {
                    if (SceneEvents::site(it.key()).selector == 2)
                        it = changedLow.erase(it);
                    else
                        ++it;
                }
                QCOMPARE(SceneEvents::discoverRow(retail, direct, index), changedLow);
                QCOMPARE(SceneEvents::discoverRow(
                             retail, QByteArray::fromHex(row["low"].toString().toLatin1()), index),
                         changedLow);
                auto damaged = direct;
                damaged[0] ^= 1;
                QVERIFY_THROWS_EXCEPTION(Error, SceneEvents::discoverRow(retail, damaged, index));
            }
            QCOMPARE(checked, 1998);
            QCOMPARE(shortCount, 402);
            QVERIFY(!SceneEvents::load(rom).modified());
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void exportedSceneRemoval() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and N64 compiler required");
        try {
            const auto rom = Rom::open(path);
            auto scenes = ScenePrograms::load(rom);
            const auto source = scenes.scenes().first();
            const auto caller = scenes.createScene("Caller", source, false);
            const auto target = scenes.createScene("Target", source, false);
            const auto targetSelector = scenes.resource(target).selectors.first();
            const auto callerSelector = scenes.resource(caller).selectors.first();
            QByteArray transfer(16, 0);
            put32(transfer, 0, 0x80000003);
            put32(transfer, 4, targetSelector);
            put32(transfer, 8, 0x80000001);
            put32(transfer, 12,
                  u32(scenes.program(caller).source, scenes.program(caller).source.size() - 4));
            scenes.replace(caller, transfer);
            const QString launch = "event-director:1:b0032";
            auto project = Project::fromRom(rom);
            project.setScenePrograms(scenes.json());
            project.loadSceneEvents();
            auto events = *project.sceneEvents();
            events.bind(launch, targetSelector, scenes);
            project.setSceneEvents(events.json());
            QTemporaryDir work;
            const auto built = rebuild(project, tools, work.path());
            const QByteArray foreign(64, 'Y');
            const auto loadedRom = Rom::fromBytes(built.image + foreign);
            auto loaded = Project::fromRom(loadedRom);
            loaded.loadSceneEvents();
            auto next = *loaded.scenePrograms();
            const auto before = next.json();
            QVERIFY_THROWS_EXCEPTION(Error, next.removeScene(target));
            QCOMPARE(next.json(), before);
            auto unsafe = next.authoredPlan();
            unsafe.erase(
                std::remove_if(unsafe.begin(), unsafe.end(),
                               [&](const auto &row) { return row.id == target.authoredId; }),
                unsafe.end());
            QVERIFY_THROWS_EXCEPTION(Error, SceneProgramResources::rebuild(loadedRom, {}, unsafe));
            auto forged = before;
            auto rows = forged["authored"].toArray();
            for (int i = rows.size() - 1; i >= 0; --i)
                if (rows[i].toObject()["id"] == target.authoredId)
                    rows.removeAt(i);
            forged["authored"] = rows;
            QVERIFY_THROWS_EXCEPTION(Error, next.apply(forged));
            next.removeCommand(caller, 0);
            loaded.setScenePrograms(next.json());
            next.removeScene(target);
            QVERIFY_THROWS_EXCEPTION(Error, loaded.setScenePrograms(next.json()));
            auto restoredEvents = *loaded.sceneEvents();
            restoredEvents.restore(launch);
            loaded.setSceneEvents(restoredEvents.json());
            loaded.setScenePrograms(next.json());
            QVERIFY(!loaded.scenePrograms()->scenes().contains(target));
            QVERIFY_THROWS_EXCEPTION(Error, loaded.scenePrograms()->program(target));
            loaded.saveFolder(work.filePath("deleted-project"));
            auto saved = Project::fromRom(loadedRom);
            saved.loadSceneEvents();
            saved.applyManifest(Project::readProject(work.filePath("deleted-project")));
            QCOMPARE(saved.scenePrograms()->json(), loaded.scenePrograms()->json());
            QCOMPARE(saved.sceneEvents()->json(), loaded.sceneEvents()->json());
            const auto removed = rebuild(saved, tools, work.path());
            QCOMPARE(removed.image.mid(built.image.size(), foreign.size()), foreign);
            QCOMPARE(u32(removed.image, 0x1f3ca88 + targetSelector * 4), quint32(0));
            const auto reopened = ScenePrograms::load(Rom::fromBytes(removed.image));
            QVERIFY(reopened.availableSelectors().contains(targetSelector));
            QVERIFY(!reopened.authoredScenes().contains(target.authoredId));
            QCOMPARE(reopened.program(caller).source, next.program(caller).source);
            next.removeScene(caller);
            saved.setScenePrograms(next.json());
            const auto empty = rebuild(saved, tools, work.path());
            QVERIFY(!SceneProgramResources::placement(Rom::fromBytes(empty.image)));
            QCOMPARE(u32(empty.image, 0x1f3ca88 + callerSelector * 4), quint32(0));
            QCOMPARE(empty.image.last(foreign.size()), foreign);
            auto tail = Project::fromRom(Rom::fromBytes(built.image));
            tail.loadSceneEvents();
            auto tailEvents = *tail.sceneEvents();
            tailEvents.restore(launch);
            tail.setSceneEvents(tailEvents.json());
            auto tailScenes = *tail.scenePrograms();
            tailScenes.removeScene(caller);
            tailScenes.removeScene(target);
            tail.setScenePrograms(tailScenes.json());
            const auto pristine = rebuild(tail, tools, work.path());
            QCOMPARE(pristine.image, rom.bytes());
            QDir parent(QFileInfo(path).absolutePath());
            QVERIFY(parent.mkpath("Test ROMs"));
            QTemporaryDir output(parent.filePath("Test ROMs/scene-delete-native-XXXXXX"));
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto candidate = output.filePath(QFileInfo(path).fileName());
            exportRom(saved, empty, candidate);
            QVERIFY(ScenePrograms::load(Rom::open(candidate)).authoredScenes().isEmpty());
            qInfo().noquote() << "Scene deletion export candidate:" << candidate;
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void bindingProjectAndExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and N64 compiler required");
        try {
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            project.loadSceneEvents();
            auto scenes = *project.scenePrograms();
            const auto source = scenes.original().entries().first();
            const auto created = scenes.createScene("Event-bound scene", source.key, false);
            project.setScenePrograms(scenes.json());
            auto events = *project.sceneEvents();
            const auto selector = scenes.resource(created).selectors.first();
            const QString shortId = "event-director:1:b0032", longId = "event-director:1:b0862";
            events.bind(shortId, selector, scenes);
            events.bind(longId, selector, scenes);
            QVERIFY_THROWS_EXCEPTION(Error, events.bind(shortId, 65535, scenes));
            QCOMPARE(events.changedSites(), 2);
            project.setSceneEvents(events.json());
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(rom);
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.sceneEvents()->json(), events.json());
            const auto built = rebuild(project, tools, work.path());
            auto checkedImage = built.image;
            Rom::updateChecksum(checkedImage);
            QCOMPARE(checkedImage, built.image);
            auto exportedRom = Rom::fromBytes(built.image);
            const auto allocation = *SceneProgramResources::placement(exportedRom);
            QCOMPARE(allocation.eventRows, QSet<int>{1});
            QCOMPARE(allocation.authored.size(), 1);
            QCOMPARE(SceneEvents::load(exportedRom).json(), events.json());
            QCOMPARE(SceneEvents::readRow(exportedRom, 1), events.plan().rows[1]);
            QCOMPARE(ScenePrograms::load(exportedRom).program(created).source,
                     scenes.program(created).source);
            QDir parent(QFileInfo(path).absolutePath());
            QVERIFY(parent.mkpath("Test ROMs"));
            QTemporaryDir output(parent.filePath("Test ROMs/event-native-XXXXXX"));
            QVERIFY(output.isValid());
            output.setAutoRemove(false);
            const auto candidate = output.filePath(QFileInfo(path).fileName());
            exportRom(project, built, candidate);
            QCOMPARE(SceneEvents::load(Rom::open(candidate)).json(), events.json());
            qInfo().noquote() << "Event export candidate:" << candidate;
            const QByteArray foreign(64, 'X');
            auto withForeign = built.image + foreign;
            auto again = Project::fromRom(Rom::fromBytes(withForeign));
            again.loadSceneEvents();
            auto unbound = *again.sceneEvents();
            unbound.restore(shortId);
            unbound.restore(longId);
            again.setSceneEvents(unbound.json());
            const auto rebuilt = rebuild(again, tools, work.path());
            QCOMPARE(rebuilt.image.mid(built.image.size(), foreign.size()), foreign);
            QCOMPARE(u32(rebuilt.image, SceneEvents::directory + 8), SceneEvents::retailKey(1));
            const auto clean = Rom::fromBytes(rebuilt.image);
            QVERIFY(SceneEvents::load(clean).bindings().isEmpty());
            QCOMPARE(ScenePrograms::load(clean).program(created).source,
                     scenes.program(created).source);
            // A binding-only allocation can be removed entirely and safely truncate its own tail.
            auto only = SceneEvents::load(rom);
            only.bind(shortId, 2, *project.scenePrograms());
            auto plan = only.plan();
            const auto bound = Rom::fromBytes(SceneProgramResources::rebuild(rom, {}, {}, &plan));
            SceneEventBuild empty;
            const auto original = SceneProgramResources::rebuild(bound, {}, {}, &empty);
            QCOMPARE(original, rom.bytes());
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
};
QTEST_GUILESS_MAIN(EventTests)
#include "event_tests.moc"
