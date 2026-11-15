#include "app/cutscene_editor.h"
#include "app/scene_action_editor.h"
#include "app/scene_key_picker.h"
#include "app/scene_preview.h"
#include "app/scene_timeline.h"
#include "app/theme.h"
#include "engine/pose_preview.h"
#include "engine/project.h"
#include "engine/scene_sprites.h"
#include "engine/director_session.h"
#include "rebuild/scene_logic.h"
#include <QtTest>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QProcess>
#include <QFile>
#include <QJsonDocument>
#include <QFontDatabase>
#include <QGraphicsView>
#include <QDialog>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QLabel>
#include <QTimer>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QScrollArea>
#include <bit>
#include <QPainter>
#include <QVBoxLayout>
using namespace lordly;
class SceneWorkspaceTests : public QObject {
    Q_OBJECT
    Rom rom()const{return Rom::open(qEnvironmentVariable("LORDLY_TEST_ROM"));}
  private slots:
    void keyVisibility(){
        QWidget host;auto layout=new QVBoxLayout(&host);auto picker=new SceneKeyPicker(rom());
        layout->addWidget(picker,1);layout->addWidget(picker->auditionWidget());host.resize(650,850);host.show();
        auto banks=picker->findChild<QComboBox *>("sceneKeyBank"),kind=picker->findChild<QComboBox *>("sceneKeyKind"),variants=picker->findChild<QComboBox *>("sceneKeyAppearance");
        auto grid=picker->findChild<QListWidget *>("sceneKeyGrid");auto image=picker->auditionWidget()->findChild<QLabel *>("sceneKeyPreviewImage");
        for(int bank=1;bank<=ActorArt::bankCount(rom());++bank){banks->setCurrentIndex(banks->findData(bank));QVERIFY2(variants->count()>0,qPrintable(QString("Bank %1 appearances missing").arg(bank)));QCOMPARE(grid->count(),ActorArt::load(rom(),bank).selections().size());}
        for(int mode:{0,1}){
            kind->setCurrentIndex(mode);banks->setCurrentIndex(banks->findData(7));QCOMPARE(variants->currentData().toInt(),1);
            int checked=0;for(int row=0;row<grid->count();++row){auto item=grid->item(row);const int key=item->data(Qt::UserRole+1).toInt();if(key<401||key>408)continue;
                QVERIFY2(!item->icon().isNull(),qPrintable(item->text()));grid->setCurrentRow(row);QVERIFY(!image->pixmap().isNull());++checked;
            }QCOMPARE(checked,8);
            // Source pose 401 has four distinct frames. Real timer/VM playback must change visible pixels.
            for(int row=0;row<grid->count();++row)if(grid->item(row)->data(Qt::UserRole+1).toInt()==401)grid->setCurrentRow(row);
            const auto first=image->pixmap().toImage();QTRY_VERIFY_WITH_TIMEOUT(!image->pixmap().isNull()&&image->pixmap().toImage()!=first,2000);
            variants->setCurrentIndex(variants->findData(0));
            for(int row=0;row<grid->count();++row)if(grid->item(row)->data(Qt::UserRole+1).toInt()==401){QVERIFY(grid->item(row)->text().contains("Empty appearance"));QVERIFY(grid->item(row)->icon().isNull());}
        }
        // Original scene "Hugo assigns Magnus and Dio..." creates Dio in bank3 / appearance6.
        auto programs=ScenePrograms::load(rom());QVector<quint32> dio;
        for(auto id:programs.scenes())if(programs.displayName(id)=="Hugo assigns Magnus and Dio to clear Theodricus Mine")for(auto c:programs.program(id).commands)if(c.opcode==0x14&&c.operands[1]==3&&c.operands[8]==6){dio=c.operands;break;}
        QVERIFY(!dio.isEmpty());kind->setCurrentIndex(0);banks->setCurrentIndex(banks->findData(dio[1]));QVERIFY(variants->count()>6);
        variants->setCurrentIndex(variants->findData(dio[8]));
        int row=-1;for(int i=0;i<grid->count();++i)if(grid->item(i)->data(Qt::UserRole+1).toUInt()==dio[2]&&grid->item(i)->data(Qt::UserRole+2).toUInt()==dio[3])row=i;
        QVERIFY(row>=0);grid->setCurrentRow(row);QVERIFY(!image->pixmap().isNull());QCOMPARE(picker->selectedKey()[4],6);
        QVERIFY(host.grab().save("scene-key-dio.png"));
        banks->setCurrentIndex(banks->findData(7));for(int i=0;i<grid->count();++i)if(grid->item(i)->data(Qt::UserRole+1).toInt()==401){grid->setCurrentRow(i);grid->scrollToItem(grid->item(i),QAbstractItemView::PositionAtTop);break;}
        QTest::qWait(100);QVERIFY(host.grab().save("scene-key-bank7.png"));
    }
    void initTestCase(){QVERIFY2(!qEnvironmentVariable("LORDLY_TEST_ROM").isEmpty(),"Pinned private ROM required");QFontDatabase::addApplicationFont(QDir(qEnvironmentVariable("WINDIR")).filePath("Fonts/segoeui.ttf"));applyEditorTheme(*qApp);}
    void reportedPlaybackAndLayout(){
        auto project=Project::fromRom(rom());project.loadScenePrograms();project.loadSceneEvents();
        QUndoStack undo;CutsceneEditor editor(project.rom(),nullptr,&project,&undo);editor.resize(1280,850);editor.show();
        auto list=editor.findChild<QListWidget *>("cutsceneScenes");QTRY_VERIFY_WITH_TIMEOUT(list->count()>1000,10000);
        for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole).toUInt()==0x19aa5b6)list->setCurrentRow(i);
        auto preview=editor.findChild<ScenePreview *>();auto status=preview->findChild<QLabel *>("scenePlaybackStatus");
        auto play=preview->findChild<QPushButton *>("scenePlay");play->click();
        QTRY_VERIFY2_WITH_TIMEOUT(preview->property("sceneTick").toInt()>3,qPrintable(status->text()),5000);play->setChecked(false);
        preview->seekToUpdate(220);QTRY_VERIFY2_WITH_TIMEOUT(preview->property("sceneTick").toInt()>=220||preview->property("sceneStopped").toBool(),qPrintable(status->text()),20000);
        QVERIFY2(!preview->property("sceneStopped").toBool(),qPrintable(status->text()));
        auto canvas=preview->findChild<QGraphicsView *>("sceneCanvas");QVERIFY(canvas);
        for(const auto size:{QSize(1280,850),QSize(1600,1050)})for(bool advanced:{false,true}){
            editor.resize(size);editor.findChild<QCheckBox *>("cutsceneRawToggle")->setChecked(advanced);QTest::qWait(100);
            QVERIFY(canvas->viewport()->width()>=640);QVERIFY(canvas->viewport()->height()>=480);
            QVERIFY2(canvas->transform().m11()>=1.99,"The scene image must be at least 640x480");
            const auto bottom=canvas->mapTo(preview,canvas->rect().bottomLeft()).y();
            if(status->mapTo(preview,QPoint()).y()<=bottom){editor.grab().save("scene-workspace-layout-failure.png");qInfo()<<"geometry"<<size<<advanced<<preview->geometry()<<preview->minimumSizeHint()<<canvas->geometry()<<status->geometry()<<preview->parentWidget()->geometry()<<preview->parentWidget()->minimumSizeHint();}
            QVERIFY2(status->mapTo(preview,QPoint()).y()>bottom,"Status text overlaps the scene");
            auto live=preview->findChild<QComboBox *>("sceneLiveActors");QVERIFY(live->mapTo(preview,QPoint()).y()>bottom);
        }
        editor.findChild<QCheckBox *>("cutsceneRawToggle")->setChecked(false);editor.resize(1500,1000);QTest::qWait(100);
        QVERIFY(editor.grab().save("scene-workspace-fixed.png"));
        preview->seekToUpdate(6000);QTRY_VERIFY2_WITH_TIMEOUT(preview->property("sceneTick").toInt()>=6000||preview->property("sceneStopped").toBool(),qPrintable(status->text()),90000);
        QVERIFY2(status->text().contains("Scene released"),qPrintable(status->text()));
    }
    void keyFrameCounts(){
        SceneKeyPicker picker(rom());picker.show();const auto art=ActorArt::load(rom(),1);
        auto grid=picker.findChild<QListWidget *>("sceneKeyGrid");auto kind=picker.findChild<QComboBox *>("sceneKeyKind");
        for(int mode:{0,1}){kind->setCurrentIndex(mode);int checked=0;
            for(int i=0;i<grid->count();++i){auto item=grid->item(i);const int state=item->data(Qt::UserRole).toInt();QSet<int> expected;bool simple=true;
                for(int record=0;record<1024;++record){const auto r=art.poseRecord(state,record);if(r.opcode==0||r.opcode==4)break;if(r.opcode==5||r.opcode==22){simple=false;break;}if(r.opcode==1||r.opcode==21)expected<<u8(r.operands,0)+(r.opcode==21?256*u8(r.operands,1):0);}
                if(simple&&!expected.isEmpty()){QCOMPARE(item->data(Qt::UserRole+3).toInt(),expected.size());QVERIFY(item->text().contains(QString::number(expected.size())+" frame"));++checked;}
            }QVERIFY(checked>=8);
        }
    }
    void originalYawSetter(){
        SceneActors actors(rom(),0,0);int value=0;const QVector<int> actorSlots{0,1,27};QJsonArray initial,indices,cases;
        for(int slot:actorSlots){actors.service(0x14,{quint32(slot),30,0,0,0,0,0,0,0},value);initial<<QString::fromLatin1(actors.actor(slot)->record().toHex());indices<<slot;}
        for(quint32 selector:{0xffffffffU,1U,26U,0x10001U,0x7fffU})for(int angle:{-50000,200000,125})cases<<QJsonObject{{"selector",double(selector)},{"degrees",double(float(angle)/1000.f)},{"word",angle}};
        QTemporaryDir work;QFile input(work.filePath("input.json"));QVERIFY(input.open(QIODevice::WriteOnly));input.write(QJsonDocument(QJsonObject{{"slots",indices},{"initial",initial},{"cases",cases}}).toJson());input.close();
        QProcess process;process.start("node",{QFINDTESTDATA("actor-yaw-mips.cjs"),qEnvironmentVariable("LORDLY_TEST_ROM"),input.fileName(),work.filePath("result.json")});QVERIFY(process.waitForFinished(30000));QVERIFY2(process.exitCode()==0,process.readAllStandardError().constData());
        QFile output(work.filePath("result.json"));QVERIFY(output.open(QIODevice::ReadOnly));
        for(const auto &v:QJsonDocument::fromJson(output.readAll()).object()["cases"].toArray()){
            auto candidate=actors;const auto c=v.toObject();candidate.service(0x22,{quint32(c["selector"].toInteger()),0x7fffffffU,quint32(c["word"].toInt()),0x80000000U},value);
            for(int i=0;i<actorSlots.size();++i)QCOMPARE(candidate.actor(actorSlots[i])->record(),QByteArray::fromHex(c["records"].toArray()[i].toString().toLatin1()));
        }
    }
    void authoringAndUndo(){
        auto project=Project::fromRom(rom());project.loadScenePrograms();project.loadSceneEvents();
        auto programs=*project.scenePrograms();SceneId source;
        for(auto id:programs.scenes())if(programs.canCreateBlank(id)){source=id;break;}
        QVERIFY(source);const auto id=programs.createScene("Workspace test",source,true);
        project.setScenePrograms(programs.json());const auto baseline=programs.program(id).source;
        QUndoStack undo;CutsceneEditor editor(project.rom(),nullptr,&project,&undo);
        editor.resize(1500,1000);editor.show();
        auto scenes=editor.findChild<QListWidget *>("cutsceneScenes");QVERIFY(scenes);
        QTRY_VERIFY_WITH_TIMEOUT(scenes->count()>1000,10000);
        for(int i=0;i<scenes->count();++i)if(scenes->item(i)->data(Qt::UserRole+1).toString()==id.authoredId)scenes->setCurrentRow(i);
        auto pages=editor.findChild<QTabWidget *>("cutsceneDetailPages");QVERIFY(pages);
        QCOMPARE(pages->currentWidget()->objectName(),"cutsceneWorkspace");
        auto picker=editor.findChild<SceneKeyPicker *>();QVERIFY(picker);
        auto banks=picker->findChild<QComboBox *>("sceneKeyBank");auto grid=picker->findChild<QListWidget *>("sceneKeyGrid");
        banks->setCurrentIndex(banks->findData(3));QVERIFY(grid->count()>0);
        auto variants=picker->findChild<QComboBox *>("sceneKeyAppearance");variants->setCurrentIndex(variants->findData(6));
        QCOMPARE(grid->count(),ActorArt::load(rom(),3).selections().size());
        grid->setCurrentRow(0);QTest::qWait(100);
        QCOMPARE(project.scenePrograms()->program(id).source,baseline); // Audition never edits.
        auto actions=editor.findChild<SceneActionEditor *>();QVERIFY(actions);
        actions->addAtPlayhead(0xe);
        auto preview=editor.findChild<ScenePreview *>();QVERIFY(preview);preview->seekToUpdate(1);
        QTRY_COMPARE_WITH_TIMEOUT(preview->property("sceneTick").toInt(),1,5000);
        grid->setCurrentRow(1);grid->setCurrentRow(0);
        auto canvas=preview->findChild<QGraphicsView *>("sceneCanvas");QVERIFY(canvas);
        QSignalSpy placed(preview,&ScenePreview::assetPlaced);
        QTest::mouseClick(canvas->viewport(),Qt::LeftButton,Qt::NoModifier,canvas->viewport()->rect().center());
        QCOMPARE(placed.count(),1);
        QCOMPARE(placed.first().last().toInt(),6);
        bool dio=false;for(auto c:project.scenePrograms()->program(id).commands)if(c.opcode==0x14&&c.operands[1]==3){QCOMPARE(c.operands[8],6U);dio=true;}QVERIFY(dio);
        auto placedActions=SceneActions::inspect(project.scenePrograms()->program(id));
        QVERIFY(std::any_of(placedActions.begin(),placedActions.end(),[](const auto &a){return a.name=="Create Actor";}));
        QTRY_VERIFY_WITH_TIMEOUT(preview->property("sceneTick").toInt()>=2,5000);
        QPoint actorPoint(-1,-1);
        for(int y=2;y<238&&actorPoint.x()<0;y+=3)for(int x=2;x<318;x+=3)if(preview->actorAt({x,y})==0&&preview->actorAt({x+1,y+1})==0&&preview->actorAt({x-1,y-1})==0){actorPoint={x,y};break;}
        QVERIFY2(actorPoint.x()>=0,"Placed Actor must be visible and selectable");
        const auto beforeMove=project.scenePrograms()->program(id).source;
        actions->setPlacementMode(true,12,false);
        const auto from=canvas->mapFromScene(actorPoint),to=from+QPoint(30,15);
        QSignalSpy dragged(preview,&ScenePreview::actorPlacedAtPlayhead);
        QVERIFY(canvas->viewport()->rect().contains(from));
        QTest::mousePress(canvas->viewport(),Qt::LeftButton,Qt::NoModifier,from);
        QTest::mouseMove(canvas->viewport(),to,20);QTest::mouseRelease(canvas->viewport(),Qt::LeftButton,Qt::NoModifier,to);
        QCOMPARE(dragged.count(),1);
        QVERIFY2(project.scenePrograms()->program(id).source!=beforeMove,qPrintable(editor.findChild<QLabel *>("sceneActionStatus")->text()));
        bool moved=false;for(const auto &c:project.scenePrograms()->program(id).commands)if(c.opcode==7&&c.operands[0]==0&&c.operands[5]==12)moved=true;QVERIFY(moved);
        undo.undo();QCOMPARE(project.scenePrograms()->program(id).source,beforeMove);undo.redo();
        preview->seekToUpdate(0);QTRY_COMPARE_WITH_TIMEOUT(preview->property("sceneTick").toInt(),0,5000);
        actions->addAtPlayhead(SceneActions::SetStoryFlag);
        QVERIFY(needsSceneLogic(project));QVERIFY(project.scenePrograms()->program(id).source!=baseline);
        auto flags=actions->findChild<QComboBox *>("sceneStoryFlag");QVERIFY(flags);
        auto state=actions->findChild<QComboBox *>("sceneStoryFlagState");QVERIFY(state);
        state->setCurrentIndex(state->findData(0));
        const auto edited=project.scenePrograms()->program(id).source;
        auto actual=SceneActions::inspect(project.scenePrograms()->program(id));
        QVERIFY(std::any_of(actual.begin(),actual.end(),[](const auto &a){return a.name=="Set story flag";}));
        QTemporaryDir folder;project.saveFolder(folder.path());
        auto reopened=Project::fromRom(rom());reopened.applyManifest(Project::readProject(folder.path()));
        QCOMPARE(reopened.scenePrograms()->program(id).source,edited);
        while(undo.canUndo())undo.undo();QCOMPARE(project.scenePrograms()->program(id).source,baseline);
        while(undo.canRedo())undo.redo();QCOMPARE(project.scenePrograms()->program(id).source,edited);
        preview->seekToUpdate(3);QTRY_COMPARE_WITH_TIMEOUT(preview->property("sceneTick").toInt(),3,5000);
        QVERIFY(editor.grab().save("scene-workspace.png"));
    }
    void filterCreationAndTrigger(){
        auto project=Project::fromRom(rom());project.loadScenePrograms();project.loadSceneEvents();
        QUndoStack undo;CutsceneEditor editor(project.rom(),nullptr,&project,&undo);editor.resize(1280,850);editor.show();
        auto list=editor.findChild<QListWidget *>("cutsceneScenes");QTRY_VERIFY_WITH_TIMEOUT(list->count()>1000,10000);
        auto rows=editor.findChild<QComboBox *>("cutsceneEventRow");QVERIFY(rows);rows->setCurrentIndex(rows->findData(4));
        bool mulsuk=false;for(int i=0;i<list->count();++i){auto item=list->item(i);if(item->toolTip().contains("selectors 610")){QVERIFY(!item->isHidden());list->setCurrentRow(i);mulsuk=true;break;}}QVERIFY(mulsuk);
        auto search=editor.findChild<QLineEdit *>("cutsceneScenesSearch");search->setText("not-a-scene-unique-test");
        auto pages=editor.findChild<QTabWidget *>("cutsceneDetailPages");QVERIFY(pages->isHidden());QVERIFY(!list->currentItem());
        search->clear();QVERIFY(!pages->isHidden());
        QTimer::singleShot(0,&editor,[&]{auto d=editor.findChild<QDialog *>("sceneCreateDialog");QVERIFY(d);d->findChild<QLineEdit *>("newSceneName")->setText("Filtered new scene");d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();});
        editor.findChild<QPushButton *>("sceneCreate")->click();
        QCOMPARE(rows->currentData().toInt(),-1);QVERIFY(list->currentItem());QVERIFY(!list->currentItem()->isHidden());QCOMPARE(list->currentItem()->text(),"Filtered new scene");
        const auto id=SceneId::authored(list->currentItem()->data(Qt::UserRole+1).toString());QVERIFY(id);
        const int selector=project.scenePrograms()->resource(id).selectors.first();
        const auto eventsBefore=project.sceneEvents()->json();
        QTimer::singleShot(0,&editor,[&]{auto choose=qobject_cast<QInputDialog *>(QApplication::activeModalWidget());QVERIFY(choose);choose->setTextValue(rows->itemText(rows->findData(4)));choose->accept();
            QTimer::singleShot(0,&editor,[]{auto d=qobject_cast<QDialog *>(QApplication::activeModalWidget());QVERIFY(d);auto target=d->findChild<QComboBox *>("eventTreeTarget");QVERIFY(target);target->setCurrentIndex(target->findData(2));d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();});});
        editor.findChild<QPushButton *>("sceneCreateTrigger")->click();
        const auto trees=project.sceneEvents()->flagEdits()["4"].toObject()["trees"].toArray();QVERIFY(!trees.isEmpty());
        const auto trigger=trees.last().toObject();QCOMPARE(trigger["group"].toInt(),1);QCOMPARE(trigger["target"].toInt(),2);QCOMPARE(trigger["nodes"].toArray().first().toObject()["scene"].toInt(),selector);
        QCOMPARE(rows->currentData().toInt(),4);QVERIFY(!list->currentItem()->isHidden());QCOMPARE(list->currentItem()->text(),"Filtered new scene");
        undo.undo();QCOMPARE(project.sceneEvents()->json(),eventsBefore);undo.redo();QVERIFY(project.sceneEvents()->json()!=eventsBefore);
        QCOMPARE(editor.size(),QSize(1280,850));QVERIFY(editor.grab().save("scene-workspace-small.png"));
    }
    void spriteAuthoring(){
        auto project=Project::fromRom(rom());project.loadScenePrograms();project.loadSceneEvents();auto programs=*project.scenePrograms();SceneId source;
        for(auto id:programs.scenes())if(programs.resource(id).selectors.contains(610))source=id;
        const auto id=programs.createScene("Sprite placement",source,true);project.setScenePrograms(programs.json());
        QUndoStack undo;CutsceneEditor editor(project.rom(),nullptr,&project,&undo);editor.resize(1500,1000);editor.show();
        auto list=editor.findChild<QListWidget *>("cutsceneScenes");QTRY_VERIFY_WITH_TIMEOUT(list->count()>1000,10000);
        for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole+1).toString()==id.authoredId)list->setCurrentRow(i);
        auto actions=editor.findChild<SceneActionEditor *>();auto preview=editor.findChild<ScenePreview *>();actions->addAtPlayhead(0xe);preview->seekToUpdate(1);QTRY_COMPARE_WITH_TIMEOUT(preview->property("sceneTick").toInt(),1,5000);
        auto picker=editor.findChild<SceneKeyPicker *>();picker->findChild<QComboBox *>("sceneKeyKind")->setCurrentIndex(1);
        auto banks=picker->findChild<QComboBox *>("sceneKeyBank");banks->setCurrentIndex(banks->findData(7));auto grid=picker->findChild<QListWidget *>("sceneKeyGrid");grid->setCurrentRow(0);
        auto canvas=preview->findChild<QGraphicsView *>("sceneCanvas");QSignalSpy placed(preview,&ScenePreview::assetPlaced);
        QTest::mouseClick(canvas->viewport(),Qt::LeftButton,Qt::NoModifier,canvas->viewport()->rect().center());QCOMPARE(placed.count(),1);
        QCOMPARE(placed.first().last().toInt(),1);
        bool appearance=false;for(auto c:project.scenePrograms()->program(id).commands)if(c.opcode==0x46&&c.operands[1]==7){QCOMPARE(c.operands[8],1U);appearance=true;}QVERIFY(appearance);
        QTRY_VERIFY2_WITH_TIMEOUT(preview->property("sceneTick").toInt()==2,qPrintable(editor.findChild<QLabel *>("sceneActionStatus")->text()+" / "+preview->findChild<QLabel *>("scenePlaybackStatus")->text()),5000);
        QPoint point(-1,-1);for(int y=2;y<238&&point.x()<0;y+=3)for(int x=2;x<318;x+=3)if(preview->spriteAt({x,y})==0&&preview->spriteAt({x+1,y+1})==0&&preview->spriteAt({x-1,y-1})==0){point={x,y};break;}QVERIFY(point.x()>=0);
        const auto before=project.scenePrograms()->program(id).source;
        editor.findChild<QDoubleSpinBox *>("sceneMoveSeconds")->setValue(12);editor.findChild<QComboBox *>("scenePlacementTool")->setCurrentIndex(1);
        const auto from=canvas->mapFromScene(point),to=from+QPoint(30,15);QSignalSpy dragged(preview,&ScenePreview::spritePlacedAtPlayhead);
        QTest::mousePress(canvas->viewport(),Qt::LeftButton,Qt::NoModifier,from);QTest::mouseMove(canvas->viewport(),to,20);QTest::mouseRelease(canvas->viewport(),Qt::LeftButton,Qt::NoModifier,to);
        QCOMPARE(dragged.count(),1);QVERIFY(project.scenePrograms()->program(id).source!=before);
        bool moved=false;const auto p=project.scenePrograms()->program(id);for(const auto &a:SceneActions::inspect(p))if(a.name=="Move sprite"){QCOMPARE(p.commands[a.command+2].operands[1],12U);moved=true;}QVERIFY(moved);
        undo.undo();QCOMPARE(project.scenePrograms()->program(id).source,before);undo.redo();
    }
    void compiledFlagDispatcher(){
        const auto tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");QVERIFY(!tools.isEmpty());
        QTemporaryDir work;Compiler compiler(tools);
        const auto module=buildSceneLogicRuntime(compiler,work.path(),0x80360000,0x80000100,0x80000110);
        QFile binary(work.filePath("code.bin"));QVERIFY(binary.open(QIODevice::WriteOnly));binary.write(module.bytes);binary.close();
        QProcess process;process.start("node",{QFINDTESTDATA("scene-flags-mips.cjs"),qEnvironmentVariable("LORDLY_TEST_ROM"),binary.fileName(),QString::number(module.entries.value("func_00284288"))});
        QVERIFY(process.waitForFinished(60000));QVERIFY2(process.exitCode()==0,process.readAllStandardError().constData());
        qInfo().noquote()<<process.readAllStandardOutput();
    }
    void spriteMovement(){
        SceneSprites edited(rom()),control(rom());int value=0;
        const QVector<quint32> create{0,30,1,2,10,20,0,100,0};
        edited.service(0x46,create,value);control.service(0x46,create,value);
        edited.service(0x8e,{0x4c4d0000,50},value);edited.service(0x8e,{0x4c4e0000,60},value);edited.service(0x8e,{0x4c540000,4},value);
        for(int i=1;i<=4;++i){
            QVERIFY(edited.advancePoses());QVERIFY(control.advancePoses());auto expected=control.sprite(0)->record();
            put32(expected,0x24,u32(expected,0x24)+10*i);put32(expected,0x28,u32(expected,0x28)-10*i);
            QCOMPARE(edited.sprite(0)->record(),expected);
            edited.service(0x8f,{0,0,0x4c4d0000},value);QCOMPARE(value,i<4?1:0);
        }
        edited.service(0x8e,{0x4c500000,quint32(-7)},value);edited.service(0x8e,{0x4c510000,9},value);
        QCOMPARE(qint32(u32(edited.sprite(0)->record(),0x24)),-7);QCOMPARE(qint32(u32(edited.sprite(0)->record(),0x28)),-9);
        edited.service(0x8e,{0x4c4d0000,50},value);edited.service(0x8e,{0x4c4e0000,60},value);edited.service(0x8e,{0x4c540000,4},value);
        edited.service(0x66,{0},value);edited.service(0x46,create,value);control.service(0x46,create,value);
        edited.advancePoses();control.advancePoses();QCOMPARE(edited.sprite(0)->record(),control.sprite(0)->record());
    }
    void choiceBranchFlags(){
        auto project=Project::fromRom(rom());project.loadScenePrograms();project.loadSceneEvents();auto scenes=*project.scenePrograms();SceneId source;
        for(auto id:scenes.scenes())if(scenes.canCreateBlank(id)){source=id;break;}QVERIFY(source);
        const auto id=scenes.createScene("Choice branches",source,true);project.setScenePrograms(scenes.json());
        QUndoStack undo;SceneActionEditor editor(&project,&undo,[]{});editor.selectScene(id,-1);
        QTimer::singleShot(0,&editor,[&]{auto dialog=editor.findChild<QDialog *>("sceneChoiceDialog");QVERIFY(dialog);dialog->findChild<QLineEdit *>("sceneChoiceOption1")->setText("Help the town");dialog->findChild<QLineEdit *>("sceneChoiceOption2")->setText("Leave it");dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();});
        editor.addAtPlayhead(SceneActions::Choice);
        auto choices=SceneActions::choices(project.scenePrograms()->program(id));QCOMPARE(choices.size(),1);
        editor.selectCommand(choices.first().command);editor.addFromWorkspace(SceneActions::SetStoryFlag,3);
        auto create=editor.findChild<QPushButton *>("sceneCreateFlag");QVERIFY(create);
        QTimer::singleShot(0,&editor,[]{auto d=qobject_cast<QInputDialog *>(QApplication::activeModalWidget());QVERIFY(d);d->setTextValue("Town help accepted");d->accept();});
        create->click();const auto flags=project.sceneEvents()->customFlags();QCOMPARE(flags.size(),1);const int flag=flags.first().toObject()["id"].toInt();
        QCOMPARE(project.sceneEvents()->flagName(flag),QString("Flag %1 (Town help accepted)").arg(flag));
        choices=SceneActions::choices(project.scenePrograms()->program(id));editor.selectCommand(choices.first().command);editor.addFromWorkspace(SceneActions::SetStoryFlag,4);
        auto flagBox=editor.findChild<QComboBox *>("sceneStoryFlag");QVERIFY(flagBox);flagBox->setCurrentIndex(flagBox->findData(flag));
        auto state=editor.findChild<QComboBox *>("sceneStoryFlagState");QVERIFY(state);state->setCurrentIndex(state->findData(0));
        const auto program=project.scenePrograms()->program(id);choices=SceneActions::choices(program);QCOMPARE(choices.size(),1);
        for(int option:{1,255}){
            DirectorSession session(project.scenePrograms()->runtimeDirectors(),project.scenePrograms()->resource(id).selectors.first(),0);
            QMap<int,int> written;
            auto service=[&](quint32 opcode,const QVector<quint32> &args,int &value){
                if(opcode==0x25)value=option;
                if(opcode==0x10)value=1;
                if(opcode==0x8e&&(args[0]&0xffff0000U)==SceneActions::storyFlagTag)written[args[0]&0xffff]=args[1];
                return true;
            };
            for(int i=0;i<30;++i){const auto status=session.dispatch(service);if(status==LORDLY_DIRECTOR_END||status==LORDLY_DIRECTOR_RELEASE)break;QVERIFY2(status==LORDLY_DIRECTOR_YIELD,qPrintable(session.reason()));}
            QCOMPARE(written.size(),1);QCOMPARE(written.value(flag,-1),option==1?1:0);
        }
        QTemporaryDir folder;project.saveFolder(folder.path());auto opened=Project::fromRom(rom());opened.applyManifest(Project::readProject(folder.path()));QCOMPARE(opened.scenePrograms()->program(id).source,program.source);QCOMPARE(opened.sceneEvents()->flagName(flag),QString("Flag %1 (Town help accepted)").arg(flag));
    }
    void appearanceExport(){
        const auto tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN"),run=qEnvironmentVariable("LORDLY_SCENE_TEST_OUTPUT");QVERIFY(!tools.isEmpty()&&!run.isEmpty());QDir().mkpath(run);
        auto project=Project::fromRom(rom());QCOMPARE(rebuild(project,tools,QDir(run).filePath("unedited")).image,rom().bytes());
        project.loadScenePrograms();project.loadSceneEvents();auto programs=*project.scenePrograms();SceneId source;
        for(auto id:programs.scenes())if(programs.resource(id).selectors.contains(610)){source=id;break;}QVERIFY(source);
        const auto id=programs.createScene("Appearance placement test",source,true);project.setScenePrograms(programs.json());
        QUndoStack undo;SceneActionEditor actions(&project,&undo,{});actions.selectScene(id,SceneActions::positions(programs.program(id)).first().command);
        actions.addAsset(true,3,0,1,{-30,0,0},6);actions.addAsset(true,7,401,0,{40,0,0},1);
        auto p=project.scenePrograms()->program(id);int count=0;for(auto c:p.commands)if(c.opcode==0x46){QCOMPARE(c.operands[8],c.operands[1]==3?6U:1U);++count;}QCOMPARE(count,2);
        auto events=*project.sceneEvents();const int start=events.createFlag("Appearance test started"),done=events.createFlag("Appearance test finished");
        const int at=SceneActions::positions(p).first().command;p=SceneActions::insert(p,at,SceneActions::SetStoryFlag);p=SceneActions::edit(p,at,0,QString::number(start));
        p=SceneActions::insert(p,at+3,0xe,240);p=SceneActions::insert(p,at+9,SceneActions::SetStoryFlag);p=SceneActions::edit(p,at+9,0,QString::number(done));
        programs=*project.scenePrograms();programs.replace(id,p.source);project.setScenePrograms(programs.json());
        events.setTrees(4,QJsonArray{QJsonObject{{"id","appearance-test"},{"name","Appearance test"},{"group",1},{"target",2},{"nodes",QJsonArray{QJsonObject{{"type","scene"},{"scene",programs.resource(id).selectors.first()}}}}}},programs);project.setSceneEvents(events.json());
        project.saveFolder(QDir(run).filePath("project"));auto saved=Project::fromRom(rom());saved.applyManifest(Project::readProject(QDir(run).filePath("project")));QCOMPARE(saved.scenePrograms()->program(id).source,p.source);
        auto build=rebuild(project,tools,QDir(run).filePath("build"));const auto path=QDir(run).filePath("Ogre Battle 64 - Person of Lordly Caliber (USA).z64");exportRom(project,build,path);
        const auto output=Rom::open(path);auto reopened=ScenePrograms::load(output);bool found=false;for(auto candidate:reopened.scenes())if(reopened.program(candidate).source==p.source)found=true;QVERIFY(found);
        const auto placement=ClassPlacement::read(output);QVERIFY(placement);QFile meta(QDir(run).filePath("probe.json"));QVERIFY(meta.open(QIODevice::WriteOnly));
        meta.write(QJsonDocument(QJsonObject{{"flagsAddress",double(placement->layout.customFlags)},{"startFlag",start},{"doneFlag",done},{"scene",programs.resource(id).selectors.first()}}).toJson());
    }
    void exportRoundTrip(){
        try {
        const auto tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");QVERIFY(!tools.isEmpty());
        const auto run=qEnvironmentVariable("LORDLY_SCENE_TEST_OUTPUT");QVERIFY2(!run.isEmpty(),"Run-specific local export folder required");
        QDir().mkpath(run);auto project=Project::fromRom(rom());
        auto unedited=rebuild(project,tools,QDir(run).filePath("unedited-build"));QCOMPARE(unedited.image,rom().bytes());
        project.loadScenePrograms();project.loadSceneEvents();auto programs=*project.scenePrograms();SceneId source;
        for(auto id:programs.scenes())if(programs.resource(id).selectors.contains(610)){source=id;break;}QVERIFY(source);
        QVERIFY(programs.canCreateBlank(source));const auto id=programs.createScene("Scene authoring test",source,true);
        auto events=*project.sceneEvents();const int flag=events.createFlag("Scene choice accepted");project.setSceneEvents(events.json());
        const int done=events.createFlag("Scene movement finished");
        auto p=programs.program(id);int at=SceneActions::positions(p).first().command;
        p=SceneActions::insert(p,at,SceneActions::SetStoryFlag);p=SceneActions::edit(p,at,0,QString::number(flag));
        p=SceneActions::insert(p,at+1,0x46);p=SceneActions::insert(p,at+2,SceneActions::MoveSprite,30);
        p=SceneActions::edit(p,at+2,1,"40");p=SceneActions::edit(p,at+2,2,"30");
        p=SceneActions::insert(p,at+5,0xe,90);
        p=SceneActions::insert(p,at+11,SceneActions::SetStoryFlag);p=SceneActions::edit(p,at+11,0,QString::number(done));
        programs.replace(id,p.source);project.setScenePrograms(programs.json());
        const QJsonArray nodes{QJsonObject{{"type","scene"},{"scene",programs.resource(id).selectors.first()}}};
        const QJsonObject trigger{{"id","cutscene-workspace-test"},{"name","Cutscene workspace test"},{"group",1},{"target",2},{"nodes",nodes}};
        events.setTrees(4,QJsonArray{trigger},programs);
        project.setSceneEvents(events.json());project.saveFolder(QDir(run).filePath("project"));
        auto build=rebuild(project,tools,QDir(run).filePath("edited-build"));
        const auto filename=QDir(run).filePath("Ogre Battle 64 - Person of Lordly Caliber (USA).z64");exportRom(project,build,filename);
        const auto output=Rom::open(filename);QVERIFY(hasSceneLogicRuntime(output));
        const auto placement=ClassPlacement::read(output);QVERIFY(placement);
        QFile meta(QDir(run).filePath("probe.json"));QVERIFY(meta.open(QIODevice::WriteOnly));meta.write(QJsonDocument(QJsonObject{{"flagsAddress",double(placement->layout.customFlags)},{"startFlag",flag},{"doneFlag",done},{"scene",programs.resource(id).selectors.first()}}).toJson());
        auto reopened=Project::fromRom(output);reopened.loadScenePrograms();reopened.loadSceneEvents();
        bool found=false;for(auto candidate:reopened.scenePrograms()->scenes())if(reopened.scenePrograms()->program(candidate).source==p.source)found=true;QVERIFY(found);
        }catch(const std::exception &e){QFAIL(e.what());}
    }
};
QTEST_MAIN(SceneWorkspaceTests)
#include "scene_workspace_tests.moc"
