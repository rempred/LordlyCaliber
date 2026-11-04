#include "app/scenario_event_editor.h"
#include "app/theme.h"
#include "engine/event_script.h"
#include "engine/project.h"
#include "rebuild/compiler.h"
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QUndoStack>
#include <QtTest>
using namespace lordly;
class EventValueTests : public QObject {
    Q_OBJECT
  private slots:
    void initTestCase() {
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeuib.ttf");
        qApp->setFont(QFont("Segoe UI",10));applyEditorTheme(*qApp);
    }
    void operandsAndLayouts() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        try {
            const auto rom=Rom::open(path);int words=0;
            for(int row=0;row<115;++row) {
                if(row==2||row==3)continue;
                const auto bytes=SceneEvents::retailRow(rom,row);auto offsets=EventScript::instructionOffsets(row);
                words+=offsets.size();
                // Exercise each layout at its first prepared literal. The audited
                // inventory, not a guessed disassembly, excludes handler/table words.
                for(int at:offsets)if(u8(bytes,at)>=0x68&&u8(bytes,at)<=0x6f) {
                    const auto fields=EventScript::parameters(bytes,row,at);QVERIFY(!fields.isEmpty());
                    const auto &f=fields.first();
                    QCOMPARE(EventScript::applyOperands(bytes,row,EventScript::parameterEdit(bytes,row,at,f.key,f.value)),bytes);
                    break;
                }
                QVERIFY_THROWS_EXCEPTION(Error,EventScript::applyOperands(bytes,row,{{"0",1}}));
            }
            QCOMPARE(words,55471);
            auto bytes=SceneEvents::retailRow(rom,10);
            const auto f=EventScript::parameters(bytes,10,0xa4).first();
            QCOMPARE(f.value,527);QVERIFY(f.choices[526].contains("Asnabel"));QVERIFY(f.choices[527].contains("Katreda"));
            const auto patch=EventScript::parameterEdit(bytes,10,0xa4,"value",526);
            const auto changed=EventScript::applyOperands(bytes,10,patch);auto expected=bytes;expected[0xa5]=14;
            QCOMPARE(changed,expected);
            QVERIFY_THROWS_EXCEPTION(Error,EventScript::parameterEdit(bytes,10,0xa4,"value",65536));
            QVERIFY_THROWS_EXCEPTION(Error,EventScript::applyOperands(bytes,10,{{"164",1.5}}));
            // Native setters below service 1 hang; they must not become choices.
            QVERIFY_THROWS_EXCEPTION(Error,EventScript::applyOperands(bytes,10,{{"28",0}}));
            // Find actual short/wide branches and verify word displacements against
            // their destinations, including a call spanning more than 127 words.
            bool shortDone=false,wideDone=false;
            for(int row:{10,67}) {
                const auto b=SceneEvents::retailRow(rom,row);auto offsets=EventScript::instructionOffsets(row);
                for(int at:offsets) {
                    const int op=u8(b,at);if(op!=1&&op!=2&&op!=3)continue;
                    const auto fields=EventScript::parameters(b,row,at);
                    if(fields.first().key!="destination")continue;
                    const auto f=fields.first();int destination=-1;
                    for(auto p=f.choices.begin();p!=f.choices.end();++p)if(p.key()!=f.value){destination=p.key();break;}
                    if(destination<0)continue;
                    const auto edit=EventScript::parameterEdit(b,row,at,"destination",destination);
                    const auto result=EventScript::applyOperands(b,row,edit);
                    const int actual=op==1?at+2+qint8(u8(result,at+1))*2:
                        quint16((at+2)/2+(u8(result,at+1)<<8)+u8(result,at-1))*2;
                    QCOMPARE(actual,destination);if(op==1)shortDone=true;else wideDone=true;
                    if(shortDone&&wideDone)break;
                }
                if(shortDone&&wideDone)break;
            }
            QVERIFY(shortDone&&wideDone);
        }catch(const std::exception &e){QFAIL(e.what());}
    }
    void inlineControlsUndoAndSave() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        try {
            auto project=Project::fromRom(Rom::open(path));project.loadSceneEvents();const auto before=project.manifest();
            QUndoStack undo;ScenarioEventEditor editor(&project,&undo,[]{});editor.setRawInstructions(true);editor.resize(1450,900);editor.showAllRows();editor.show();
            editor.selectEventRow(10,0xa4);QApplication::processEvents();
            auto choice=editor.findChild<QComboBox *>("scenarioEventParameter_value");QVERIFY(choice);QVERIFY(choice->isVisible());
            QCOMPARE(choice->currentData().toInt(),527);
            const int index=choice->findData(526);QVERIFY(index>=0);choice->setCurrentIndex(index);
            QMetaObject::invokeMethod(choice,"activated",Q_ARG(int,index));
            QTRY_COMPARE(u8(project.sceneEvents()->program(10),0xa5),quint8(14));QCOMPARE(undo.count(),1);
            const auto after=project.manifest();undo.undo();QApplication::processEvents();QCOMPARE(project.manifest(),before);
            undo.redo();QApplication::processEvents();QCOMPARE(project.manifest(),after);
            auto tree=editor.findChild<QTreeWidget *>("scenarioEventSteps");QVERIFY(tree);
            editor.selectEventRow(10,0xa8);QApplication::processEvents();QVERIFY(tree->currentItem()->text(1).contains("Asnabel"));
            QTemporaryDir folder;project.saveFolder(folder.path());auto reopened=Project::fromRom(Rom::open(path));
            reopened.applyManifest(Project::readProject(folder.path()));QCOMPARE(reopened.sceneEvents()->program(10),project.sceneEvents()->program(10));
            // Type a numeric high byte, rather than clicking arrows. This tests
            // the finish-edit path and replacement of the inline widget itself.
            editor.selectEventRow(10,0xa6);QApplication::processEvents();
            auto number=editor.findChild<QSpinBox *>("scenarioEventParameter_value");QVERIFY(number);QVERIFY(number->isVisible());
            number->setFocus();QTest::keyClick(number,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(number,"1");QTest::keyClick(number,Qt::Key_Return);
            QTRY_COMPARE(u8(project.sceneEvents()->program(10),0xa7),quint8(1));
            undo.undo();QApplication::processEvents();QCOMPARE(project.manifest(),after);
            editor.selectEventRow(10,0xa4);QApplication::processEvents();
            QVERIFY(editor.grab().save("native/build/event-value-controls.png"));
        }catch(const std::exception &e){QFAIL(e.what());}
    }
    void comparisonRelationsAndReset() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        try {
            const auto rom=Rom::open(path);auto project=Project::fromRom(rom);project.loadSceneEvents();
            const auto original=SceneEvents::retailRow(rom,16);QCOMPARE(u8(original,382),quint8(0xc1));
            for(int relation=10;relation<=15;++relation) {
                const auto patch=EventScript::parameterEdit(original,16,382,"comparison",relation);
                auto expected=original;expected[382]=char((relation<<4)|1);
                const auto changed=EventScript::applyOperands(original,16,patch);QCOMPARE(changed,expected);
                // Both the register pair and signed word displacement are retained.
                QCOMPARE(u8(changed,383),u8(original,383));QCOMPARE(u8(changed,382)&15,1);
                const auto fields=EventScript::parameters(changed,16,382);bool found=false;
                for(const auto &f:fields)if(f.key=="comparison"){QCOMPARE(f.value,relation);QCOMPARE(f.choices.size(),6);found=true;}
                QVERIFY(found);EventScript::decode(changed,16);
            }
            for(const auto &bad:QVector<QJsonObject>{{{"comparison:382",9}},{{"comparison:382",16}},
                    {{"comparison:382",12.5}},{{"comparison:0382",12}},{{"comparison:330",12}},{{"comparison:383",12}}})
                QVERIFY_THROWS_EXCEPTION(Error,EventScript::applyOperands(original,16,bad));
            auto wrongLayout=original;wrongLayout[382]=char(0xc2);
            QVERIFY_THROWS_EXCEPTION(Error,EventScript::applyOperands(wrongLayout,16,{{"comparison:382",15}}));
            auto events=*project.sceneEvents();const auto baseline=events.json();
            events.editParameter(16,382,"comparison",15,*project.scenePrograms());
            QCOMPARE(u8(events.program(16),382),quint8(0xf1));project.setSceneEvents(events.json());
            QTemporaryDir folder;project.saveFolder(folder.path());auto saved=Project::fromRom(rom);
            saved.applyManifest(Project::readProject(folder.path()));QCOMPARE(saved.sceneEvents()->program(16),events.program(16));
            events.editParameter(16,382,"comparison",12,*project.scenePrograms());QCOMPARE(events.json(),baseline);
            // An explicit ON/OFF edit owns its comparison, rather than leaving
            // an older numeric relation to silently override the requested state.
            const auto bytes=events.program(10);const auto flags=EventScript::flagSteps(bytes);int check=-1;
            for(auto i=flags.begin();i!=flags.end();++i)if(i->check){check=i.key();break;}
            QVERIFY(check>=0);events.editParameter(10,check+4,"comparison",14,*project.scenePrograms());
            QVERIFY(!EventScript::flagSteps(events.program(10)).contains(check));
            events.editFlag(10,check,flags[check].flag,true);
            QCOMPARE(u8(events.program(10),check+4),quint8(0xbc));
            QVERIFY(!events.flagEdits()["10"].toObject()["operands"].toObject().contains(QString("comparison:%1").arg(check+4)));
        }catch(const std::exception &e){QFAIL(e.what());}
    }
    void exportReopenComposition() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM"),tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if(path.isEmpty()||tools.isEmpty())QSKIP("Private ROM and N64 tools required");
        try {
            const auto rom=Rom::open(path);auto project=Project::fromRom(rom);project.loadSceneEvents();auto events=*project.sceneEvents();
            events.editParameter(10,0xa4,"value",526,*project.scenePrograms());
            events.editParameter(10,106,"destination",136,*project.scenePrograms());
            events.editParameter(10,102,"argument",8,*project.scenePrograms());
            events.editParameter(16,382,"comparison",15,*project.scenePrograms());
            events.editFlag(10,0x424,250,false);
            // Shared/direct-layout world row, followed by a custom location rule.
            const auto world=SceneEvents::retailRow(rom,0);int literal=-1;
            for(int at:EventScript::instructionOffsets(0))if(u8(world,at)==0x69&&u8(world,at+2)==0x60&&u8(world,at+3)==0x61){literal=at;break;}
            QVERIFY(literal>=0);events.editParameter(0,literal,"value",250,*project.scenePrograms());
            events.setWorldRules(1,QJsonArray{QJsonObject{{"event",10},{"clearFrom",0},{"clearTo",0},{"conditions",QJsonArray{QJsonObject{{"flag",250},{"value",true}}}}}});
            QString launch;for(const auto &site:SceneEvents::sites())if(site.row==10&&site.shortForm){launch=site.id;break;}
            QVERIFY(!launch.isEmpty());const auto site=SceneEvents::site(launch);
            auto scenes=*project.scenePrograms();const auto added=scenes.createScene("Event value test",scenes.scenes().first(),false);
            project.setScenePrograms(scenes.json());const int selector=scenes.resource(added).selectors.first();QVERIFY(selector>255);
            events.editParameter(10,site.offset-2,"value",selector,scenes);project.setSceneEvents(events.json());
            QVERIFY(project.changeReport().text().contains("Instruction value"));
            QTemporaryDir work;project.saveFolder(work.filePath("project"));auto saved=Project::fromRom(rom);
            saved.applyManifest(Project::readProject(work.filePath("project")));QCOMPARE(saved.sceneEvents()->json(),events.json());
            const auto built=rebuild(saved,tools,work.path());auto crc=built.image;Rom::updateChecksum(crc);QCOMPARE(crc,built.image);
            const auto output=Rom::fromBytes(built.image);auto reopened=Project::fromRom(output);reopened.loadSceneEvents();
            QCOMPARE(reopened.sceneEvents()->json(),events.json());
            for(int row:{0,10,16}) {QCOMPARE(SceneEvents::readRow(output,row),events.program(row));QCOMPARE(SceneEvents::retailRow(output,row),SceneEvents::retailRow(rom,row));}
            QCOMPARE(rebuild(reopened,tools,work.path()).image,built.image);
            auto restored=*reopened.sceneEvents();restored.restoreFlags(10);QCOMPARE(restored.json(),reopened.sceneEvents()->json());
            QDir parent(QFileInfo(path).absolutePath());QVERIFY(parent.mkpath("Test ROMs"));
            QTemporaryDir destination(parent.filePath("Test ROMs/event-values-XXXXXX"));QVERIFY(destination.isValid());destination.setAutoRemove(false);
            exportRom(saved,built,destination.filePath(QFileInfo(path).fileName()));
            QFile bytes(destination.filePath("event-10.bin"));QVERIFY(bytes.open(QIODevice::WriteOnly));bytes.write(SceneEvents::readRow(output,10));bytes.close();
            qInfo().noquote()<<"Event values export:"<<destination.path();
        }catch(const std::exception &e){QFAIL(e.what());}
    }
};
QTEST_MAIN(EventValueTests)
#include "event_value_tests.moc"
