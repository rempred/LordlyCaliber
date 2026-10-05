#include "app/actor_pose_editor.h"
#include "app/scene_preview.h"
#include "app/tools_editor.h"
#include "app/consumable_editor.h"
#include "app/cutscene_editor.h"
#include "app/dialogue_inspector.h"
#include "app/dialogue_preview.h"
#include "app/encounter_editor.h"
#include "app/main_window.h"
#include "app/save_editor.h"
#include "app/sprite_editor.h"
#include "app/art_editor.h"
#include "app/portrait_editor.h"
#include "app/army_editor.h"
#include "app/combat_editor.h"
#include <QDialogButtonBox>
#include "game/classes/save_format.h"
#include <QUndoGroup>
#include "app/promotion_editor.h"
#include "app/record_model.h"
#include "app/scenario_editor.h"
#include "app/scene_command_editor.h"
#include "app/scene_event_editor.h"
#include "app/shop_editor.h"
#include "app/text_workspace.h"
#include "app/transform_preview.h"
#include "app/value_editor.h"
#include "engine/scene_import.h"
#include "rebuild/compiler.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFontDatabase>
#include <QGraphicsItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableView>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QPainter>
#include <QtTest>
#include <bit>
using namespace lordly;
class UiTests : public QObject {
    Q_OBJECT
  private slots:
    void editedCombatPreviewAndUndo() {
        try {
            const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
            const auto rom=Rom::open(path);auto project=Project::fromRom(rom);project.loadCombatArtwork();QUndoStack undo;
            CutsceneEditor editor(rom,nullptr,&project,&undo,[]{});editor.resize(1300,900);editor.show();
            auto inspector=editor.findChild<ActorPoseEditor *>();QVERIFY(inspector);
            auto kind=inspector->findChild<QComboBox *>("actorSourceKind"),bank=inspector->findChild<QComboBox *>("actorBank");
            kind->setCurrentIndex(1);bank->setCurrentIndex(bank->findData(1));
            auto position=inspector->findChild<QSpinBox *>("actorPosition");position->setValue(1);
            auto projected=inspector->findChild<QCheckBox *>("actorProjected");auto view=inspector->findChild<QGraphicsView *>("actorCanvas");
            auto flat=[&]{QImage image(320,240,QImage::Format_RGBA8888);image.fill(Qt::transparent);QPainter painter(&image);view->scene()->render(&painter);return image;};
            const auto original=flat();const auto cursor=inspector->property("poseCursor");
            projected->setChecked(true);const auto projectedOriginal=inspector->property("placementFrame").value<QImage>();QVERIFY(!projectedOriginal.isNull());projected->setChecked(false);
            const auto body=ActorArt::loadBody(rom,1,1,0,0);const auto layer=body.bodyLayers(rom,inspector->property("frameToken").toInt(),body.defaultAppearance(rom)).first();
            const auto id=CombatArtwork::identity(body.descriptorKey(),layer.metadata.art);const auto sprite=project.combatArtwork()->original(id);const int child=layer.child<sprite.children().size()?layer.child:0;
            CombatEditor artEditor(&project,&undo,[]{});artEditor.selectBinding(id,child);auto blank=project.combatArtwork()->image(id,child);blank.fill(Qt::transparent);artEditor.assignImage(blank);
            const auto edited=flat();QVERIFY(edited!=original);QCOMPARE(position->value(),1);QCOMPARE(inspector->property("poseCursor"),cursor);
            projected->setChecked(true);const auto projectedEdited=inspector->property("placementFrame").value<QImage>();QVERIFY(projectedEdited!=projectedOriginal);
            position->setValue(18);position->setValue(1);QCOMPARE(inspector->property("placementFrame").value<QImage>(),projectedEdited);
            undo.undo();QCOMPARE(inspector->property("placementFrame").value<QImage>(),projectedOriginal);undo.redo();QCOMPARE(inspector->property("placementFrame").value<QImage>(),projectedEdited);
            projected->setChecked(false);QCOMPARE(flat(),edited);undo.undo();QCOMPARE(flat(),original);
            // The dynamic scene rebuilds captured/checkpoint frames at the same tick.
            auto scenes=ScenePrograms::load(rom);const quint32 key=0x1fa8bf4-0x594284;auto program=scenes.program(key);
            for(auto &command:program.commands)if(command.opcode==0x1b){command.operands[4]=0;command.operands[5]=0;break;}
            scenes.replace(key,program.encode());ScenePreview preview(rom);preview.resize(1200,850);preview.show();preview.selectScene(scenes,key,2);
            auto scenePosition=preview.findChild<QSpinBox *>("scenePosition");scenePosition->setValue(3);QTRY_COMPARE(preview.property("sceneTick").toInt(),3);const auto sceneOriginal=preview.renderedFrame();
            auto staged=CombatArtwork::load(rom);QSet<quint32> descriptors;
            for(int flags=0;flags<4;++flags)descriptors.insert(ActorArt::loadBody(rom,0x58,0x58,flags/2,flags%2).descriptorKey());
            for(const auto &binding:staged.bindings())if(descriptors.contains(binding.descriptorKey)){
                const auto object=staged.original(binding.id);
                for(int variant=0;variant<object.children().size();++variant)if(object.children()[variant].colorOffset>=0&&object.children()[variant].alphaOffset>=0){auto pixels=object.pixels(variant);pixels.alpha.fill(0);staged.setPixels(binding.id,variant,pixels);}
            }
            QVERIFY(staged.modified());preview.setArtwork(staged.previewEdits());QTRY_COMPARE(preview.property("sceneTick").toInt(),3);const auto sceneEdited=preview.renderedFrame();QVERIFY(sceneEdited!=sceneOriginal);
            scenePosition->setValue(1);QTRY_COMPARE(preview.property("sceneTick").toInt(),1);scenePosition->setValue(3);QTRY_COMPARE(preview.property("sceneTick").toInt(),3);QCOMPARE(preview.renderedFrame(),sceneEdited);
            preview.setArtwork({});QTRY_COMPARE(preview.property("sceneTick").toInt(),3);QCOMPARE(preview.renderedFrame(),sceneOriginal);
        }catch(const Error &e){QFAIL(e.what());}
    }
    void combatArtworkEditing() {
        try {
            const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
            auto project=Project::fromRom(Rom::open(path));QUndoStack undo;CombatEditor editor(&project,&undo,[]{});editor.resize(1340,850);editor.show();
            const auto id=CombatArtwork::identity(0x3bac1c,0);editor.selectBinding(id,0);
            auto descriptor=editor.findChild<QComboBox *>("combatDescriptor");QVERIFY(descriptor);QCOMPARE(descriptor->count(),166);QVERIFY(descriptor->currentText().contains("Soldier"));
            auto canvas=editor.findChild<SpriteCanvas *>("combatCanvas");QVERIFY(canvas);QVERIFY2(canvas->isEnabled(),qPrintable(editor.findChild<QLabel *>("combatNotice")->text()));
            const auto original=project.combatArtwork()->pixels(id,0);const int width=project.combatArtwork()->original(id).width(),pixel=width+1;
            const int index=(u8(original.color,pixel)+7)&255;editor.findChild<QSpinBox *>("combatInk")->setValue(index);
            QTest::mouseClick(canvas,Qt::LeftButton,Qt::NoModifier,QPoint(1,1)*canvas->zoom()+QPoint(2,2));
            QCOMPARE(u8(project.combatArtwork()->pixels(id,0).color,pixel),quint8(index));QCOMPARE(project.combatArtwork()->pixels(id,0).alpha,original.alpha);QCOMPARE(undo.count(),1);
            auto channel=editor.findChild<QComboBox *>("combatChannel");channel->setCurrentIndex(1);const int alpha=(u8(original.alpha,pixel)+3)&15;editor.findChild<QSpinBox *>("combatInk")->setValue(alpha);
            QTest::mouseClick(canvas,Qt::LeftButton,Qt::NoModifier,QPoint(1,1)*canvas->zoom()+QPoint(2,2));
            QCOMPARE(u8(project.combatArtwork()->pixels(id,0).alpha,pixel),quint8(alpha));const auto painted=project.combatArtwork()->pixels(id,0);
            undo.undo();QCOMPARE(project.combatArtwork()->pixels(id,0).alpha,original.alpha);undo.redo();QCOMPARE(project.combatArtwork()->pixels(id,0),painted);
            const auto image=project.combatArtwork()->image(id,0);
            const auto copied=editor.copyToLibrary();QCOMPARE(project.sprites().asset(copied).frames[0].layers[0].image,image);
            editor.findChild<QPushButton *>("combatRestore")->click();QCOMPARE(project.combatArtwork()->pixels(id,0),original);
            auto library=editor.findChild<QComboBox *>("combatLibrary");library->setCurrentIndex(library->findData(copied));
            QTimer::singleShot(0,&editor,[&]{auto dialog=editor.findChild<QDialog *>("combatImagePreparation");QVERIFY(dialog);dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();});editor.findChild<QPushButton *>("combatAssignLibrary")->click();
            QCOMPARE(project.combatArtwork()->image(id,0),image);
            QTemporaryDir work;editor.exportImage(work.filePath("combat.png"));QCOMPARE(QImage(work.filePath("combat.png")).convertToFormat(QImage::Format_RGBA8888),image);
            undo.undo();editor.importImage(work.filePath("combat.png"));QCOMPARE(project.combatArtwork()->image(id,0),image);QVERIFY_THROWS_EXCEPTION(Error,editor.exportImage(path));
            project.saveFolder(work.filePath("folder"));auto loaded=Project::fromRom(project.rom());loaded.applyManifest(Project::readProject(work.filePath("folder")));QCOMPARE(loaded.combatArtwork()->json(),project.combatArtwork()->json());
            editor.selectBinding(id,1);QVERIFY(canvas->isEnabled());
            QCOMPARE(project.combatArtwork()->pixels(id,1),project.combatArtwork()->original(id).pixels(1));
            descriptor->setCurrentIndex(descriptor->findData(0x100ad4cU));QVERIFY(!canvas->isEnabled());
            editor.selectBinding(id,0);channel->setCurrentIndex(2);QVERIFY(canvas->isEnabled());
            QVERIFY(editor.grab().save("combat-artwork-workspace.png"));
        }catch(const Error &e){QFAIL(e.what());}
    }
    void armyArtworkEditing() {
        try {
            const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
            auto project=Project::fromRom(Rom::open(path));QUndoStack undo;ArmyEditor editor(&project,&undo,[]{});editor.resize(1300,820);editor.show();
            auto models=editor.findChild<QListWidget *>("armyModels");QVERIFY(models);QCOMPARE(models->count(),64);
            editor.selectModel(3,24);QCOMPARE(models->count(),64);auto canvas=editor.findChild<SpriteCanvas *>("armyCanvas");QVERIFY(canvas);QVERIFY(!canvas->isEnabled());
            auto create=editor.findChild<QPushButton *>("armyCreate");QVERIFY(create->isEnabled());create->click();QVERIFY(canvas->isEnabled());QVERIFY(project.armyArtwork()->present(3,24));QCOMPARE(project.armyArtwork()->indices(3,24),QByteArray(384,char(-1)));
            editor.findChild<QSpinBox *>("armyInk")->setValue(1);QTest::mouseClick(canvas,Qt::LeftButton,Qt::NoModifier,QPoint(2,3)*canvas->zoom()+QPoint(2,2));
            QCOMPARE(u8(project.armyArtwork()->indices(3,24),3*16+2),quint8(1));QCOMPARE(undo.count(),2);
            const auto painted=project.armyArtwork()->indices(3,24);undo.undo();QCOMPARE(project.armyArtwork()->indices(3,24),QByteArray(384,char(-1)));undo.redo();
            editor.findChild<QComboBox *>("armyPalette")->setCurrentIndex(1);QCOMPARE(project.armyArtwork()->indices(3,24),painted);
            const auto image=project.armyArtwork()->image(3,24,1);const auto pixel=image.pixelColor(2,3);const auto copied=editor.copyToLibrary();QCOMPARE(project.sprites().asset(copied).frames[0].layers[0].image,image);
            editor.selectModel(3,25,1);auto library=editor.findChild<QComboBox *>("armyLibrary");library->setCurrentIndex(library->findData(copied));
            QTimer::singleShot(0,&editor,[&]{auto dialog=editor.findChild<QDialog *>("armyImagePreparation");QVERIFY(dialog);dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();});editor.findChild<QPushButton *>("armyAssignLibrary")->click();
            QVERIFY(project.armyArtwork()->present(3,25));QCOMPARE(project.armyArtwork()->image(3,25,1).pixelColor(2,3),pixel);QCOMPARE(project.armyArtwork()->image(3,25,1).pixelColor(0,0).alpha(),0);
            QTemporaryDir work;editor.exportImage(work.filePath("army.png"));QCOMPARE(QImage(work.filePath("army.png")).convertToFormat(QImage::Format_RGBA8888),project.armyArtwork()->image(3,25,1));
            editor.selectModel(3,26,1);editor.importImage(work.filePath("army.png"));QCOMPARE(project.armyArtwork()->image(3,26,1).pixelColor(2,3),pixel);
            const auto saved=project.armyArtwork()->json();project.saveFolder(work.filePath("project"));auto reopened=Project::fromRom(project.rom());reopened.applyManifest(Project::readProject(work.filePath("project")));QCOMPARE(reopened.armyArtwork()->json(),saved);
            editor.findChild<QPushButton *>("armyRestore")->click();QVERIFY(!project.armyArtwork()->present(3,26));undo.undo();QCOMPARE(project.armyArtwork()->json(),saved);
            QVERIFY_THROWS_EXCEPTION(Error,editor.exportImage(path));QVERIFY(editor.grab().save("army-artwork-workspace.png"));
            QCOMPARE(project.appendClass(1,"New Army class"),165);const auto added=project.classData().entries()[165];editor.selectModel(ArmyArtwork::classLane(added)*2,ArmyArtwork::classModel(added));QVERIFY(editor.findChild<QLabel *>("armyConsumers")->text().contains("New Army class (165)"));
            editor.selectModel(0,56);editor.findChild<QPushButton *>("armyCreate")->click();editor.selectModel(1,56);editor.findChild<QPushButton *>("armyCreate")->click();
            auto targetClass=editor.findChild<QComboBox *>("armyRouteClass");targetClass->setCurrentIndex(targetClass->findData(165));editor.findChild<QPushButton *>("armyAssignRoute")->click();QCOMPARE(ArmyArtwork::classModel(project.classData().entries()[165]),56);QVERIFY(project.armyArtwork()->routes().contains(165));
            undo.undo();QVERIFY(!project.armyArtwork()->routes().contains(165));undo.redo();QCOMPARE(ArmyArtwork::classModel(project.classData().entries()[165]),56);
            editor.findChild<QPushButton *>("armyRestoreRoute")->click();QVERIFY(!project.armyArtwork()->routes().contains(165));undo.undo();QCOMPARE(ArmyArtwork::classModel(project.classData().entries()[165]),56);
            editor.selectModel(4,16,0);QVERIFY(!editor.findChild<QPushButton *>("armyAssignRoute")->isEnabled());QVERIFY(editor.grab().save("army-artwork-loaded-workspace.png"));
        }catch(const Error &e){QFAIL(e.what());}
    }
    void indexedPixelTools() {
        SpriteCanvas canvas;canvas.setZoom(8);canvas.setGrid(false);canvas.show();
        QVector<QColor> palette(256,QColor(Qt::black));palette[0]=Qt::transparent;palette[1]=palette[2]=Qt::red;
        QByteArray pixels(12,char(1));pixels[0]=2;canvas.setIndexedAsset("paired",{4,3},pixels,palette,0);
        connect(&canvas,&SpriteCanvas::indicesCommitted,&canvas,[&](const QByteArray &next,QPoint at){QCOMPARE(at,QPoint{});pixels=next;canvas.setIndexedAsset("paired",{4,3},pixels,palette,0);});
        const auto original=canvas.grab().toImage();canvas.setIndexColor(2);
        QTest::mouseClick(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(12,12));
        QCOMPARE(quint8(pixels[5]),quint8(2));QCOMPARE(canvas.grab().toImage(),original);
        palette[2]=Qt::blue;canvas.setIndexedAsset("paired",{4,3},pixels,palette,0);
        QCOMPARE(canvas.grab().toImage().pixelColor(12,12),QColor(Qt::blue));
        canvas.setTool(SpriteCanvas::Tool::Select);QTest::mouseClick(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(12,12));
        canvas.setTool(SpriteCanvas::Tool::Move);QTest::mousePress(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(12,12));QTest::mouseMove(&canvas,QPoint(20,12));QTest::mouseRelease(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(20,12));
        QCOMPARE(quint8(pixels[5]),quint8(0));QCOMPARE(quint8(pixels[6]),quint8(2));
        canvas.clearSelection();canvas.setTool(SpriteCanvas::Tool::Eraser);QTest::mouseClick(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(20,12));QCOMPARE(quint8(pixels[6]),quint8(0));
        QSignalSpy picked(&canvas,&SpriteCanvas::indexPicked);canvas.setTool(SpriteCanvas::Tool::Pick);QTest::mouseClick(&canvas,Qt::LeftButton,Qt::NoModifier,QPoint(2,2));QCOMPARE(picked.count(),1);QCOMPARE(picked.first().first().toInt(),2);
    }
    void portraitArtworkEditing() {
        try {
            const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
            auto project=Project::fromRom(Rom::open(path));QUndoStack undo;
            PortraitEditor editor(&project,&undo,[]{});editor.resize(1250,790);editor.show();
            auto list=editor.findChild<QListWidget *>("portraitClasses");QVERIFY(list);QCOMPARE(list->count(),165);
            const auto classes=project.classData();const auto before=project.avatarArtwork()->words(classes,1,0);
            const auto variant=project.avatarArtwork()->words(classes,1,1);
            auto palette=editor.findChild<QTableWidget *>("portraitPalette");QVERIFY(palette);const auto cell=palette->item(0,0);QVERIFY(cell);
            const auto color=cell->data(Qt::UserRole).value<QColor>();
            QTest::mouseClick(palette->viewport(),Qt::LeftButton,Qt::NoModifier,palette->visualItemRect(cell).center());
            const auto image=project.avatarArtwork()->image(classes,1,0);QPoint target;bool found=false;
            for(int y=0;y<48 && !found;++y)for(int x=0;x<40;++x)if(image.pixelColor(x,y)!=color){target={x,y};found=true;break;}QVERIFY(found);
            auto canvas=editor.findChild<SpriteCanvas *>("portraitCanvas");QVERIFY(canvas);
            QTest::mouseClick(canvas,Qt::LeftButton,Qt::NoModifier,target*canvas->zoom()+QPoint(2,2));
            QVERIFY(project.avatarArtwork()->modified(1,0));QCOMPARE(project.avatarArtwork()->image(classes,1,0).pixelColor(target),color);
            QCOMPARE(project.avatarArtwork()->words(classes,1,1),variant);QCOMPARE(undo.count(),1);
            undo.undo();QCOMPARE(project.avatarArtwork()->words(classes,1,0),before);undo.redo();
            const auto copied=editor.copyToLibrary();QCOMPARE(project.sprites().asset(copied).composite(0),project.avatarArtwork()->image(classes,1,0));
            const auto edited=project.avatarArtwork()->words(classes,1,0);editor.selectPortrait(2,1);
            editor.findChild<QCheckBox *>("portraitBoth")->setChecked(true);
            auto library=editor.findChild<QComboBox *>("portraitLibrary");library->setCurrentIndex(library->findData(copied));
            QTimer::singleShot(0,&editor,[&]{auto dialog=editor.findChild<QDialog *>("portraitImagePreparation");QVERIFY(dialog);dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();});
            editor.findChild<QPushButton *>("portraitAssignLibrary")->click();
            for(int selector=0;selector<2;++selector)QCOMPARE(project.avatarArtwork()->words(classes,2,selector),edited);
            QTemporaryDir work;editor.exportImage(work.filePath("portrait.png"));
            QCOMPARE(QImage(work.filePath("portrait.png")).convertToFormat(QImage::Format_RGBA8888),AvatarArtwork::render(edited));
            editor.selectPortrait(3,0);editor.findChild<QCheckBox *>("portraitBoth")->setChecked(false);editor.importImage(work.filePath("portrait.png"));
            QCOMPARE(project.avatarArtwork()->words(classes,3,0),edited);
            const auto saved=project.avatarArtwork()->json();project.saveFolder(work.filePath("project"));auto loaded=Project::fromRom(project.rom());loaded.applyManifest(Project::readProject(work.filePath("project")));QCOMPARE(loaded.avatarArtwork()->json(),saved);
            editor.findChild<QPushButton *>("portraitRestore")->click();QVERIFY(!project.avatarArtwork()->modified(3,0));undo.undo();QCOMPARE(project.avatarArtwork()->json(),saved);
            QVERIFY_THROWS_EXCEPTION(Error,editor.exportImage(path));QVERIFY(editor.grab().save("portrait-artwork-workspace.png"));
        }catch(const Error &e){QFAIL(e.what());}
    }
    void iconArtworkEditing() {
        try {
            const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
            auto project=Project::fromRom(Rom::open(path));QUndoStack undo;ArtEditor editor(&project,&undo,[]{});editor.resize(1250,790);editor.show();
            auto list=editor.findChild<QListWidget *>("artIcons");QVERIFY(list);QCOMPARE(list->count(),277);
            const auto original=project.iconArtwork()->words(1);auto canvas=editor.findChild<SpriteCanvas *>("artIconCanvas");QVERIFY(canvas);auto tools=editor.findChild<QComboBox *>("artPixelTool");
            tools->setCurrentIndex(int(SpriteCanvas::Tool::Eraser));
            // Pick an opaque source pixel so erasing necessarily changes the icon.
            const auto source=project.iconArtwork()->image(1);QPoint target;bool found=false;
            for(int y=0;y<16 && !found;++y)for(int x=0;x<16;++x)if(source.pixelColor(x,y).alpha()){target={x,y};found=true;break;}QVERIFY(found);
            QTest::mouseClick(canvas,Qt::LeftButton,Qt::NoModifier,target*canvas->zoom()+QPoint(2,2));QVERIFY(project.iconArtwork()->modified(1));QCOMPARE(project.iconArtwork()->image(1).pixelColor(target).alpha(),0);QCOMPARE(undo.count(),1);
            undo.undo();QCOMPARE(project.iconArtwork()->words(1),original);undo.redo();
            const auto libraryId=editor.copyToLibrary();QCOMPARE(project.sprites().asset(libraryId).frames[0].layers[0].image,project.iconArtwork()->image(1));QCOMPARE(project.sprites().assets().size(),1);
            editor.selectIcon(2);auto library=editor.findChild<QComboBox *>("artLibraryAsset");library->setCurrentIndex(library->findData(libraryId));
            QTimer::singleShot(0,&editor,[&]{auto dialog=editor.findChild<QDialog *>("artImagePreparation");QVERIFY(dialog);dialog->findChild<QComboBox *>("artImportFit")->setCurrentIndex(3);dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();});
            editor.findChild<QPushButton *>("artAssignLibrary")->click();QCOMPARE(project.iconArtwork()->words(2),project.iconArtwork()->words(1));
            const auto edited=project.iconArtwork()->json();QTemporaryDir work;editor.exportImage(work.filePath("icon.png"));QCOMPARE(QImage(work.filePath("icon.png")).convertToFormat(QImage::Format_RGBA8888),project.iconArtwork()->image(2));
            editor.selectIcon(0x201);QCOMPARE(list->count(),44);const auto consumable=project.iconArtwork()->words(0x201);editor.importImage(work.filePath("icon.png"));QVERIFY(project.iconArtwork()->words(0x201)!=consumable);undo.undo();QCOMPARE(project.iconArtwork()->json(),edited);
            project.saveFolder(work.filePath("project"));auto restored=Project::fromRom(project.rom());restored.applyManifest(Project::readProject(work.filePath("project")));QCOMPARE(restored.iconArtwork()->json(),edited);QCOMPARE(restored.sprites().json(),project.sprites().json());
            editor.selectIcon(2);editor.findChild<QPushButton *>("artRestore")->click();QVERIFY(!project.iconArtwork()->modified(2));undo.undo();QCOMPARE(project.iconArtwork()->json(),edited);
            QVERIFY_THROWS_EXCEPTION(Error,editor.exportImage(path));QVERIFY(editor.grab().save("icon-artwork-workspace.png"));
        }catch(const Error &e){QFAIL(e.what());}
    }
    void spriteWorkspaceEditing() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            auto project = Project::fromRom(Rom::open(path));
            QUndoStack undo;
            SpriteEditor editor(&project, &undo, [] {});
            editor.resize(1400, 820);
            editor.show();
            auto asset = SpriteAsset::blank("Layered test", {16, 12});
            editor.addAsset(asset);
            const auto id = asset.id;
            auto canvas = editor.findChild<SpriteCanvas *>();
            auto tool = editor.findChild<QComboBox *>("spriteTool");
            auto frameList = editor.findChild<QListWidget *>("spriteFrames");
            auto layerList = editor.findChild<QListWidget *>("spriteLayers");
            auto click = [&](const char *name) {
                auto button = editor.findChild<QPushButton *>(name);
                QVERIFY(button);
                button->click();
            };
            auto paint = [&](QPoint from, QPoint to) {
                QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier,
                                  from * canvas->zoom() + QPoint(2, 2));
                QTest::mouseMove(canvas, to * canvas->zoom() + QPoint(2, 2));
                QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier,
                                    to * canvas->zoom() + QPoint(2, 2));
            };
            QApplication::processEvents();
            canvas->setColor(QColor(200, 40, 20, 128));
            paint({1, 1}, {5, 1});
            QCOMPARE(undo.count(), 2);
            QCOMPARE(project.sprites().asset(id).frames[0].layers[0].image.pixelColor(3, 1),
                     QColor(200, 40, 20, 128));
            undo.undo();
            QCOMPARE(project.sprites().asset(id).frames[0].layers[0].image.pixelColor(3, 1),
                     QColor(Qt::transparent));
            undo.redo();
            click("spriteAddLayer");
            QCOMPARE(layerList->currentRow(), 0);
            QCOMPARE(project.sprites().asset(id).frames[0].layers.size(), 2);
            canvas->setColor(Qt::blue);
            paint({2, 2}, {4, 2});
            QCOMPARE(project.sprites().asset(id).frames[0].layers[1].image.pixelColor(3, 2),
                     QColor(Qt::blue));
            QCOMPARE(project.sprites().asset(id).frames[0].layers[0].image.pixelColor(3, 2),
                     QColor(Qt::transparent));
            editor.findChild<QSpinBox *>("spriteLayerX")->setValue(-1);
            QCOMPARE(project.sprites().asset(id).frames[0].layers[1].position, QPoint(-1, 0));
            auto visible = editor.findChild<QCheckBox *>("spriteLayerVisible");
            visible->setChecked(false);
            QCOMPARE(project.sprites().asset(id).composite(0).pixelColor(2, 2),
                     QColor(Qt::transparent));
            undo.undo();
            click("spriteLayerDown");
            QCOMPARE(layerList->currentRow(), 1);
            QCOMPARE(project.sprites().asset(id).frames[0].layers[0].position, QPoint(-1, 0));
            undo.undo();
            layerList->setCurrentRow(0);
            tool->setCurrentIndex(int(SpriteCanvas::Tool::Select));
            paint({1, 2}, {2, 2});
            QCOMPARE(canvas->selection(), QRect(1, 2, 2, 1));
            tool->setCurrentIndex(int(SpriteCanvas::Tool::Eraser));
            paint({0, 2}, {4, 2});
            auto erased = project.sprites().asset(id).frames[0].layers[1].image;
            QCOMPARE(erased.pixelColor(2, 2), QColor(Qt::transparent));
            QCOMPARE(erased.pixelColor(3, 2), QColor(Qt::transparent));
            QCOMPARE(erased.pixelColor(4, 2), QColor(Qt::blue));
            undo.undo();
            tool->setCurrentIndex(int(SpriteCanvas::Tool::Pencil));
            const int count = undo.count();
            QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier,
                              QPoint(9, 9) * canvas->zoom());
            QTest::keyClick(canvas, Qt::Key_Escape);
            QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier,
                                QPoint(10, 9) * canvas->zoom());
            QCOMPARE(undo.count(), count);
            click("spriteDuplicateFrame");
            QCOMPARE(frameList->currentRow(), 1);
            QCOMPARE(project.sprites().asset(id).frames.size(), 2);
            editor.findChild<QSpinBox *>("spriteFrameTicks")->setValue(3);
            QCOMPARE(project.sprites().asset(id).frames[1].ticks, 3);
            click("spriteFrameLeft");
            QCOMPARE(frameList->currentRow(), 0);
            QCOMPARE(project.sprites().asset(id).frames[0].ticks, 3);
            QTemporaryDir work;
            editor.exportAsset(work.filePath("sprite.json"), true);
            editor.exportAsset(work.filePath("sprite.lcsprite"), false);
            editor.exportImage(work.filePath("frame.png"), "PNG");
            QCOMPARE(QImage(work.filePath("frame.png")).convertToFormat(QImage::Format_RGBA8888),
                     project.sprites().asset(id).composite(0));
            editor.exportImage(work.filePath("frame.jpg"), "JPEG");
            QVERIFY(!QImage(work.filePath("frame.jpg")).isNull());
            const auto before = project.sprites().asset(id);
            editor.importFile(work.filePath("sprite.json"));
            QCOMPARE(project.sprites().assets().size(), 2);
            QVERIFY(editor.selectedAsset() != id);
            auto imported = project.sprites().asset(editor.selectedAsset());
            imported.id = id;
            QCOMPARE(imported.json(true), before.json(true));
            undo.undo();
            QCOMPARE(project.sprites().assets().size(), 1);
            editor.importFile(work.filePath("frame.png"));
            QCOMPARE(project.sprites().assets().size(), 2);
            QCOMPARE(project.sprites().asset(editor.selectedAsset()).composite(0),
                     before.composite(0));
            undo.undo();
            editor.selectAsset(id);
            click("spriteDelete");
            QVERIFY(project.sprites().assets().isEmpty());
            undo.undo();
            QCOMPARE(project.sprites().asset(id).json(), before.json());
            editor.selectAsset(id);
            QVERIFY(editor.grab().save("sprite-workspace.png"));
            QVERIFY_THROWS_EXCEPTION(Error, editor.exportImage(path, "PNG"));
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void initTestCase() {
        // The offscreen Qt platform does not enumerate Windows system fonts.
        QFontDatabase::addApplicationFont(
            QDir(qEnvironmentVariable("WINDIR")).filePath("Fonts/segoeui.ttf"));
        QApplication::setFont(QFont("Segoe UI", 10));
    }
    void experienceToolsEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto project = Project::fromRom(rom);
        QUndoStack undo;
        ToolsEditor editor(&project, &undo, [] {});
        editor.resize(850, 420);
        editor.show();
        auto percent = editor.findChild<QSpinBox *>("experienceScale");
        auto weights = editor.findChild<QLabel *>("experienceWeights");
        auto restore = editor.findChild<QPushButton *>("experienceRestore");
        QVERIFY(percent->isEnabled());
        QCOMPARE(percent->value(), 100);
        percent->setValue(137);
        QCOMPARE(project.tools()->experiencePercent(), 137);
        QVERIFY(weights->text().contains("109.6%"));
        QVERIFY(restore->isEnabled());
        undo.undo();
        QCOMPARE(percent->value(), 100);
        undo.redo();
        QCOMPARE(percent->value(), 137);
        QVERIFY(editor.grab().save("experience-tools.png"));
        restore->click();
        QCOMPARE(percent->value(), 100);
        undo.undo();
        QCOMPARE(percent->value(), 137);
        editor.findChild<QPushButton *>("experienceRetail")->click();
        QCOMPARE(project.tools()->experiencePercent(), 100);
        auto bytes = rom.bytes();
        bytes[0x21928c] ^= 1;
        auto foreign = Project::fromRom(Rom::fromBytes(bytes));
        QUndoStack foreignUndo;
        ToolsEditor disabled(&foreign, &foreignUndo, [] {});
        QVERIFY(!disabled.findChild<QSpinBox *>("experienceScale")->isEnabled());
        QVERIFY(disabled.findChild<QLabel *>("toolsNotice")->text().contains("unrecognized"));
    }
    void chaosToolsEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto project = Project::fromRom(rom);
        QUndoStack undo;
        ToolsEditor editor(&project, &undo, [] {});
        editor.resize(850, 440);
        editor.show();
        auto box = editor.findChild<QCheckBox *>("chaosCounter");
        auto restore = editor.findChild<QPushButton *>("chaosRestore");
        QVERIFY(box->isEnabled());
        QVERIFY(!box->isChecked());
        QVERIFY(editor.findChild<QLabel *>("chaosNotice")->text().contains("8 MiB"));
        box->click();
        QVERIFY(project.tools()->chaosCounter().enabled());
        QVERIFY(restore->isEnabled());
        QVERIFY(!editor.findChild<QPushButton *>("experienceRestore")->isEnabled());
        editor.findChild<QSpinBox *>("experienceScale")->setValue(137);
        undo.undo();
        QVERIFY(box->isChecked());
        QCOMPARE(project.tools()->experiencePercent(), 100);
        undo.undo();
        QVERIFY(!box->isChecked());
        undo.redo();
        QVERIFY(box->isChecked());
        restore->click();
        QVERIFY(!box->isChecked());
        undo.undo();
        QVERIFY(box->isChecked());
        QVERIFY(editor.grab().save("chaos-tools.png"));
        auto bytes = rom.bytes();
        bytes[0x3078c] ^= 1;
        auto foreign = Project::fromRom(Rom::fromBytes(bytes));
        QUndoStack foreignUndo;
        ToolsEditor disabled(&foreign, &foreignUndo, [] {});
        QVERIFY(!disabled.findChild<QCheckBox *>("chaosCounter")->isEnabled());
        QVERIFY(disabled.findChild<QSpinBox *>("experienceScale")->isEnabled());
    }
    void displayToolsEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto project = Project::fromRom(rom);
        QUndoStack undo;
        ToolsEditor editor(&project, &undo, [] {});
        editor.resize(850, 700);
        editor.show();
        auto luck = editor.findChild<QCheckBox *>("luckDisplay");
        auto alignment = editor.findChild<QCheckBox *>("alignmentDisplay");
        QVERIFY(luck->isEnabled());
        QVERIFY(alignment->isEnabled());
        luck->click();
        alignment->click();
        QVERIFY(project.tools()->displayTool(ToolLabel::Luck).enabled());
        QVERIFY(project.tools()->displayTool(ToolLabel::Alignment).enabled());
        undo.undo();
        QVERIFY(luck->isChecked());
        QVERIFY(!alignment->isChecked());
        undo.redo();
        QVERIFY(alignment->isChecked());
        editor.findChild<QPushButton *>("luckRestore")->click();
        QVERIFY(!luck->isChecked());
        QVERIFY(alignment->isChecked());
        undo.undo();
        QVERIFY(luck->isChecked());
        QVERIFY(editor.grab().save("display-tools.png"));
        auto bytes = rom.bytes();
        bytes[0x351c0] ^= 1;
        auto foreign = Project::fromRom(Rom::fromBytes(bytes));
        QUndoStack foreignUndo;
        ToolsEditor held(&foreign, &foreignUndo, [] {});
        QVERIFY(!held.findChild<QCheckBox *>("alignmentDisplay")->isEnabled());
        QVERIFY(held.findChild<QCheckBox *>("luckDisplay")->isEnabled());
    }
    void highAttackToolsEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto project = Project::fromRom(rom);
        QUndoStack undo;
        ToolsEditor editor(&project, &undo, [] {});
        editor.resize(900, 900);
        editor.show();
        auto box = editor.findChild<QCheckBox *>("highAttack");
        auto restore = editor.findChild<QPushButton *>("highAttackRestore");
        QVERIFY(box->isEnabled());
        QVERIFY(!box->isChecked());
        const auto notice = editor.findChild<QLabel *>("highAttackNotice")->text();
        QVERIFY(notice.contains("8 MiB"));
        QVERIFY(notice.contains("Interpreter"));
        QVERIFY(notice.contains("pending"));
        box->click();
        QVERIFY(project.tools()->highAttack().enabled());
        editor.findChild<QCheckBox *>("luckDisplay")->click();
        undo.undo();
        QVERIFY(box->isChecked());
        undo.undo();
        QVERIFY(!box->isChecked());
        undo.redo();
        QVERIFY(box->isChecked());
        restore->click();
        QVERIFY(!box->isChecked());
        undo.undo();
        QVERIFY(box->isChecked());
        QVERIFY(editor.grab().save("high-attack-tools.png"));
        auto bytes = rom.bytes();
        bytes[0x34f00] ^= 1;
        auto foreign = Project::fromRom(Rom::fromBytes(bytes));
        QUndoStack foreignUndo;
        ToolsEditor held(&foreign, &foreignUndo, [] {});
        QVERIFY(!held.findChild<QCheckBox *>("highAttack")->isEnabled());
        QVERIFY(held.findChild<QCheckBox *>("luckDisplay")->isEnabled());
    }
    void saveWorkspaceEditing() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            QFile file(QFileInfo(path).dir().filePath("tools/pj64_golden.sra"));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto original = file.readAll();
            auto project = Project::fromRom(rom);
            SaveEditor editor(&project);
            editor.resize(1250, 700);
            editor.show();
            editor.loadBytes(original, file.fileName());
            auto grid = editor.findChild<QTableView *>("saveCharacters");
            auto model = grid->model();
            QVERIFY(model->rowCount() > 0);
            int hpColumn = -1;
            for (int i = 0; i < model->columnCount(); ++i)
                if (model->headerData(i, Qt::Horizontal).toString() == "Current HP")
                    hpColumn = i;
            QVERIFY(hpColumn > 0);
            const auto hpIndex = model->index(0, hpColumn);
            grid->setCurrentIndex(hpIndex);
            grid->scrollTo(hpIndex);
            grid->edit(hpIndex);
            QTRY_VERIFY(grid->findChild<QSpinBox *>() != nullptr);
            auto hpEditor = grid->findChild<QSpinBox *>();
            hpEditor->setValue(0);
            QTest::keyClick(hpEditor, Qt::Key_Return);
            QTRY_VERIFY(editor.modified());
            QCOMPARE(editor.document()->value(SaveTable::Characters, 1, "hpCur").toUInt(), 0U);
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), original);
            QVERIFY(!editor.modified());
            editor.undoStack()->redo();
            QVERIFY(!editor.edit(SaveTable::Characters, 1, "element", 5));
            QVERIFY(!editor.findChild<QLabel *>("saveError")->text().isEmpty());
            QTemporaryDir work;
            const auto output = work.filePath("edited.sra");
            editor.exportFile(output);
            QVERIFY(!editor.modified());
            QFile saved(output);
            QVERIFY(saved.open(QIODevice::ReadOnly));
            QCOMPARE(SaveDocument::open(rom, saved.readAll())
                         .value(SaveTable::Characters, 1, "hpCur")
                         .toUInt(),
                     0U);
            QVERIFY_THROWS_EXCEPTION(Error, editor.exportFile(file.fileName()));
            editor.findChild<QPushButton *>("restoreSave")->click();
            QCOMPARE(editor.document()->serialize(), original);
            QVERIFY(editor.modified()); // The last export contains the zero-HP edit.
            QVERIFY(editor.grab().save("save-workspace.png"));
            auto twoSlots = original;
            auto swap = [](QByteArray &bytes) {
                for (int i = 0; i < bytes.size(); i += 4) {
                    std::swap(bytes[i], bytes[i + 3]);
                    std::swap(bytes[i + 1], bytes[i + 2]);
                }
            };
            swap(twoSlots);
            twoSlots.replace(0x1860, 0x1850, twoSlots.mid(0x10, 0x1850));
            QVERIFY(lc_save_record_seal(reinterpret_cast<unsigned char *>(twoSlots.data() + 0x1860),
                                        0x1850, 1, 0));
            swap(twoSlots);
            editor.loadBytes(twoSlots);
            auto slotPicker = editor.findChild<QComboBox *>("saveSlot");
            slotPicker->setCurrentIndex(slotPicker->findData(1));
            QCOMPARE(editor.document()->selectedSlot(), 1);
            QVERIFY(editor.edit(SaveTable::Game, 0, "goth", 12345));
            slotPicker->setCurrentIndex(slotPicker->findData(0));
            QVERIFY(editor.edit(SaveTable::Game, 0, "goth", 23456));
            editor.undoStack()->undo();
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->selectedSlot(), 1);
            QCOMPARE(editor.document()->serialize(), twoSlots);
            const auto beforeClass = project;
            QCOMPARE(project.appendClass(1,"Save Knight"),165);
            QVERIFY(editor.edit(SaveTable::Characters,1,"class",165));
            editor.exportFile(work.filePath("new-class.sra"));
            project = beforeClass;
            QVERIFY_THROWS_EXCEPTION(Error,editor.exportFile(work.filePath("missing-class.sra")));
            editor.undoStack()->undo();
            editor.exportFile(work.filePath("restored-class.sra"));
            // Opening and confirming an unknown encoded name must not truncate it.
            auto raw = SaveDocument::open(rom,original).memory();
            raw.replace(0x193bf8,17,QByteArray(17,char(1)));
            editor.loadBytes(raw);
            grid->setCurrentIndex(model->index(0,1)); grid->scrollTo(model->index(0,1)); grid->edit(model->index(0,1));
            QTRY_VERIFY(grid->findChild<QLineEdit *>() != nullptr);
            auto unknownName = grid->findChild<QLineEdit *>();
            QCOMPARE(unknownName->text(),QString("{01}").repeated(17));
            QTest::keyClick(unknownName,Qt::Key_Return);
            QCoreApplication::processEvents();
            QCOMPARE(editor.document()->serialize(),raw);
            MainWindow window;
            window.show();
            window.loadRom(path);
            QTRY_VERIFY_WITH_TIMEOUT(window.findChild<SaveEditor *>() != nullptr, 20000);
            auto integrated = window.findChild<SaveEditor *>();
            integrated->loadBytes(original);
            auto group = window.findChild<QUndoGroup *>();
            QVERIFY(group);
            auto tabs = window.findChild<QTabWidget *>();
            tabs->setCurrentWidget(integrated);
            QCOMPARE(group->activeStack(), integrated->undoStack());
            QVERIFY(integrated->edit(SaveTable::Game, 0, "goth", 12345));
            QVERIFY(window.isWindowModified());
            group->undo();
            QVERIFY(!integrated->modified());
            tabs->setCurrentIndex(0);
            QVERIFY(group->activeStack() != integrated->undoStack());
        } catch (const Error &error) {
            QFAIL(error.what());
        }
    }
    void editUndoRedoPreservesRecord() {
        QByteArray data(0x2800000, char(0));
        put32(data, 0, 0x80371240);
        data.replace(0x20, 12, "OgreBattle64");
        data.replace(0x3b, 2, "NO");
        data[0x3e] = 'E';
        auto project = Project::fromRom(Rom::fromBytes(data));
        QUndoStack undo;
        RecordModel model(&project, Table::Classes, &undo);
        auto original = project.record(Table::Classes, 5).bytes;
        QVERIFY(model.setData(model.index(5, 2), 2, Qt::EditRole));
        auto expected = original;
        expected[4] = 2;
        QCOMPARE(project.record(Table::Classes, 5).bytes, expected);
        undo.undo();
        QCOMPARE(project.record(Table::Classes, 5).bytes, original);
        undo.redo();
        QCOMPARE(project.record(Table::Classes, 5).bytes, expected);
        QVERIFY(!model.setData(model.index(5, 2), 256, Qt::EditRole));
        QCOMPARE(project.record(Table::Classes, 5).bytes, expected);
    }
    void transformSequencePreview() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto bytes = Rom::open(path).bytes();
        QByteArray transform(8 + 960 + 16, 0);
        put32(transform, 0, 2);
        put32(transform, 4, 1);
        put32(transform, 8 + 0xa0, 2000);
        put32(transform, 488 + 0xa0, 8000);
        for (int channel = 0; channel < 20; ++channel) {
            put32(transform, 8 + 0x190 + channel * 4, 1000);
            put32(transform, 488 + 0x190 + channel * 4, 1000);
        }
        put32(transform, 976, 1);
        put32(transform, 980, 0xffffffffU);
        const auto packed = encodeLzss(transform);
        put32(bytes, 0x2700000, quint32(packed.size() + 4));
        put32(bytes, 0x2700004, quint32(transform.size()));
        bytes.replace(0x2700008, packed.size(), packed);
        put32(bytes, 0x594280 + 0x19a63ec + 4, 0x2700000 - 0x594280);
        const auto rom = Rom::fromBytes(bytes);
        TransformPreview preview(rom);
        preview.resize(1100, 800);
        preview.show();
        DecodedImage image;
        image.origin = {-10, -10};
        image.composite = {20, 20, QByteArray(20 * 20 * 4, 0)};
        for (int i = 0; i < 400; ++i) {
            image.composite.rgba[i * 4] = char(-1);
            image.composite.rgba[i * 4 + 3] = char(-1);
        }
        preview.setImage(image);
        const QVector<quint32> words{0x36,  0, 0, 0, 0, 0,           120000,
                                     90000, 8, 0, 0, 3, 0x80000001U, 0xff000005U};
        QByteArray script(words.size() * 4, 0);
        for (int i = 0; i < words.size(); ++i)
            put32(script, i * 4, words[i]);
        const auto program = DirectorProgram::decode(script);
        preview.selectCommand(program, 1);
        auto tick = preview.findChild<QSpinBox *>("transformPosition");
        auto channel = preview.findChild<QSpinBox *>("transformChannel");
        auto play = preview.findChild<QPushButton *>("transformPlay");
        auto step = preview.findChild<QPushButton *>("transformStep");
        auto status = preview.findChild<QLabel *>("transformStatus");
        QVERIFY(tick && channel && play && step && status);
        QVERIFY2(!preview.renderedFrame().isNull(), qPrintable(status->text()));
        const auto first = preview.renderedFrame();
        QCOMPARE(first.pixelColor(153, 120), QColor(Qt::red));
        QVERIFY(first.pixelColor(176, 120) != QColor(Qt::red));
        step->click();
        QCOMPARE(tick->value(), 1);
        QCOMPARE(std::bit_cast<float>(u32(preview.property("channelRecord").toByteArray(), 0x40)),
                 4.f);
        tick->setValue(3);
        QVERIFY(preview.renderedFrame().pixelColor(153, 120) != QColor(Qt::red));
        QCOMPARE(preview.renderedFrame().pixelColor(176, 120), QColor(Qt::red));
        QVERIFY(!play->isEnabled());
        QVERIFY(status->text().contains("Sequence complete"));
        channel->setValue(1);
        QCOMPARE(std::bit_cast<float>(u32(preview.property("channelRecord").toByteArray(), 0x40)),
                 0.f);
        channel->setValue(0);
        tick->setValue(0);
        QCOMPARE(preview.renderedFrame(), first);
        play->click();
        QTRY_COMPARE_WITH_TIMEOUT(tick->value(), 3, 2000);
        QCOMPARE(play->text(), QString("Play"));
        QVERIFY(!play->isEnabled());
        preview.selectSequence(0, 99, 3, SceneCamera(rom.bytes().mid(0x2866f0, 56), 1));
        QVERIFY(preview.renderedFrame().isNull());
        QVERIFY(!step->isEnabled());
        QVERIFY(status->text().contains("group"));
        preview.findChild<QPushButton *>("transformRestart")->click();
        QCOMPARE(preview.renderedFrame(), first);
        QVERIFY(preview.grab().save("transform-preview-ui.png"));
    }
    void imagePresentationPlayback_data() {
        QTest::addColumn<int>("transition");
        QTest::newRow("scripted-pan")<<-1;
        QTest::newRow("echo-forward")<<0;
        QTest::newRow("echo-reverse")<<1;
    }
    void imageSkipInputReplay() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        const auto rom=Rom::open(path);auto scenes=ScenePrograms::load(rom);quint32 source=0;
        for(const auto &entry:scenes.original().entries())if(entry.program.resourceClass()==4){source=entry.key;break;}
        QVERIFY(source);const auto id=scenes.createScene("Start-skip playback",source,false);
        QVector<quint32> words{0x80000007U,47,0x3a,1,0,0,0,70,70,130,8,0xb2};
        for(int i=0;i<140;++i)words<<0<<0x80000000U;
        words<<0xb3<<0x80000005U<<12345<<0x50<<0<<1<<0;
        for(int i=0;i<150;++i)words<<0<<0x80000000U;
        words<<0x80000001U<<0xff000004U;
        QByteArray bytes(words.size()*4,0);for(int i=0;i<words.size();++i)put32(bytes,i*4,words[i]);scenes.replace(id,bytes);
        ScenePreview preview(rom);preview.resize(1200,800);preview.show();preview.selectScene(scenes,id,0);
        auto position=preview.findChild<QSpinBox *>("scenePosition");auto status=preview.findChild<QLabel *>("scenePlaybackStatus");
        auto skip=preview.findChild<QPushButton *>("sceneSkip");auto view=preview.findChild<QGraphicsView *>("sceneCanvas");
        QVERIFY(!skip->isVisible());position->setValue(10);QTRY_COMPARE(preview.property("sceneTick").toInt(),10);
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(status->text()));QVERIFY(skip->isVisible());
        const auto before=preview.renderedFrame();QTest::keyClick(view,Qt::Key_Return);
        QCOMPARE(preview.property("sceneTick").toInt(),11);QVERIFY(!skip->isVisible());
        position->setValue(14);QTRY_COMPARE(preview.property("sceneTick").toInt(),14);
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(status->text()));const auto skipped=preview.renderedFrame();QVERIFY(skipped!=before);
        position->setValue(124);QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(),124,20000);
        position->setValue(14);QTRY_COMPARE(preview.property("sceneTick").toInt(),14);QCOMPARE(preview.renderedFrame(),skipped);
        preview.findChild<QPushButton *>("sceneRestart")->click();position->setValue(14);
        QTRY_COMPARE(preview.property("sceneTick").toInt(),14);QCOMPARE(preview.renderedFrame(),skipped);
        preview.selectScene(scenes,id,0);position->setValue(10);QTRY_COMPARE(preview.property("sceneTick").toInt(),10);
        skip->click();position->setValue(14);QTRY_COMPARE(preview.property("sceneTick").toInt(),14);QCOMPARE(preview.renderedFrame(),skipped);
    }
    void pathRibbonPreviewSeek() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        const auto rom=Rom::open(path);auto scenes=ScenePrograms::load(rom);quint32 source=0;
        for(const auto &entry:scenes.original().entries())if(entry.program.resourceClass()==4){source=entry.key;break;}
        QVERIFY(source);const auto id=scenes.createScene("Path ribbon playback",source,false);
        QVector<quint32> words{0x80000007U,47,0x3a,1,0,0,0,70,70,130,8,0,0x80000000U,
                               0x49,0,3,0,90,0xffffffffU,3,0,0,1};
        for(int i=0;i<140;++i)words<<0<<0x80000000U;
        words<<0x4b<<0<<1;
        for(int i=0;i<35;++i)words<<0<<0x80000000U;
        words<<0x80000001U<<0xff000004U;
        QByteArray bytes(words.size()*4,0);for(int i=0;i<words.size();++i)put32(bytes,i*4,words[i]);scenes.replace(id,bytes);
        ScenePreview preview(rom);preview.resize(1200,800);preview.show();preview.selectScene(scenes,id,0);
        auto position=preview.findChild<QSpinBox *>("scenePosition");auto status=preview.findChild<QLabel *>("scenePlaybackStatus");
        preview.findChild<QPushButton *>("sceneStep")->click();
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(status->text()));const auto background=preview.renderedFrame();
        position->setValue(35);QTRY_COMPARE(preview.property("sceneTick").toInt(),35);
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(status->text()));const auto revealed=preview.renderedFrame();
        int changed=0;for(int y=0;y<240;++y)for(int x=0;x<320;++x)changed+=revealed.pixel(x,y)!=background.pixel(x,y);
        QVERIFY(changed>50);QVERIFY(status->text().contains("approximate strip"));
        position->setValue(124);QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(),124,20000);
        const auto complete=preview.renderedFrame();QVERIFY(complete!=revealed);
        QVERIFY(preview.grab().save("scene-path-ribbon.png"));
        position->setValue(35);QTRY_COMPARE(preview.property("sceneTick").toInt(),35);QCOMPARE(preview.renderedFrame(),revealed);
        position->setValue(150);QTRY_COMPARE(preview.property("sceneTick").toInt(),150);QVERIFY(preview.renderedFrame()!=complete);
        position->setValue(174);QTRY_COMPARE(preview.property("sceneTick").toInt(),174);
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(status->text()));QCOMPARE(preview.renderedFrame(),background);
        position->setValue(124);QTRY_COMPARE(preview.property("sceneTick").toInt(),124);QCOMPARE(preview.renderedFrame(),complete);
        preview.findChild<QPushButton *>("sceneRestart")->click();position->setValue(35);
        QTRY_COMPARE(preview.property("sceneTick").toInt(),35);QCOMPARE(preview.renderedFrame(),revealed);
    }
    void storyOpeningDialogue() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        const auto scenes = ScenePrograms::load(rom);
        ScenePreview preview(rom);
        preview.resize(1250, 950);
        preview.show();
        preview.selectScene(scenes, 0x1fa8bf4 - 0x594284, 2);
        auto step = preview.findChild<QPushButton *>("sceneStep");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        for (int i = 0; i < 200 && preview.property("sceneDialogueState").toInt() != 4; ++i) {
            step->click();
            QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        }
        QCOMPARE(preview.property("sceneDialogueState").toInt(), 4);
        const int tick = preview.property("sceneTick").toInt();
        const auto frame = preview.renderedFrame();
        const auto pixels = frame.convertToFormat(QImage::Format_RGB32);
        int lit = 0;
        for (int y = 0; y < pixels.height(); ++y)
            for (int x = 0; x < pixels.width(); ++x)
                lit += (pixels.pixel(x, y) & 0x00ffffffU) != 0;
        QVERIFY(lit > 100);
        QVERIFY(lit < 320 * 240 / 2); // Authored black fade remains behind the dialogue.
        QVERIFY(preview.grab().save("scene-story-opening.png"));
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        position->setValue(1);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 1);
        position->setValue(tick);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), tick);
        QCOMPARE(preview.renderedFrame(), frame);
        position->setValue(250);
        QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), 250, 20000);
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        QCOMPARE(preview.property("sceneAudioRequests").toULongLong(), qulonglong(2));
        const auto room = preview.renderedFrame();
        QVERIFY(room != frame);
        QVERIFY(preview.grab().save("scene-story-revealed.png"));
        position->setValue(tick);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), tick);
        position->setValue(250);
        QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), 250, 20000);
        QCOMPARE(preview.renderedFrame(), room);
        QCOMPARE(preview.property("sceneAudioRequests").toULongLong(), qulonglong(2));
    }
    void storyCompleteReplay() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        const auto scenes = ScenePrograms::load(rom);
        const auto savedScenes = scenes.json();
        ScenePreview preview(rom);
        preview.resize(1250, 950);
        preview.show();
        preview.selectScene(scenes, 0x1fa8bf4 - 0x594284, 2);
        auto step = preview.findChild<QPushButton *>("sceneStep");
        auto confirm = preview.findChild<QPushButton *>("panelConfirm");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        QMap<int, QImage> frames;
        for (int i = 0; i < 5000 && !preview.property("sceneStopped").toBool(); ++i) {
            if (i % 45 == 0 && confirm->isVisible())
                confirm->click();
            else
                step->click();
            if (i % 500 == 499)
                frames.insert(preview.property("sceneTick").toInt(), preview.renderedFrame());
        }
        QVERIFY2(status->text().contains("Scene released."), qPrintable(status->text()));
        const int end = preview.property("sceneTick").toInt();
        QVERIFY(end > 250);
        const auto finalFrame = preview.renderedFrame();
        const auto audioCount = preview.property("sceneAudioRequests").toULongLong();
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        for (auto it = frames.cbegin(); it != frames.cend(); ++it) {
            position->setValue(it.key());
            QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), it.key(), 20000);
            QCOMPARE(preview.renderedFrame(), it.value());
        }
        position->setValue(end);
        QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), end, 20000);
        QCOMPARE(preview.renderedFrame(), finalFrame);
        QCOMPARE(preview.property("sceneAudioRequests").toULongLong(), audioCount);
        QCOMPARE(scenes.json(), savedScenes);
        qInfo() << "Full story scene released at step" << end << "with" << audioCount
                << "audio requests;" << frames.size() << "replay samples";
    }
    void sceneDialogueReplay() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        auto dialogue = Dialogues::load(rom);
        dialogue.setEntry(Dialogues::Table::Primary, 341, 23,
                          "@v020@y003Edited @%0@aAfter confirm@s");
        const auto savedText = dialogue.json();
        const auto id = scenes.createScene("Dialogue replay", 0x1fa8bf4 - 0x594284, false);
        QVector<quint32> words{0x80000006U, 34,  0xbf, 0, 341, 23, 0, 160,
                               180,         192, 0,    0, 0,   0,  0, 1};
        for (int i = 0; i < 250; ++i)
            words << 0 << 0x80000000U;
        words << 6 << 0;
        for (int i = 0; i < 80; ++i)
            words << 0 << 0x80000000U;
        words << 0x80000001U << 0xff000001U;
        QByteArray bytes(words.size() * 4, 0);
        for (int i = 0; i < words.size(); ++i)
            put32(bytes, i * 4, words[i]);
        scenes.replace(id, bytes);
        const auto savedScenes = scenes.json();
        ScenePreview preview(rom);
        preview.resize(1250, 900);
        preview.show();
        preview.selectScene(scenes, id, 2, dialogue);
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        const auto seek = [&](int tick) {
            position->setValue(tick);
            QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), tick, 20000);
            QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        };
        seek(60);
        QCOMPARE(preview.property("sceneDialogueState").toInt(), 3);
        const auto waiting = preview.renderedFrame();
        QVERIFY(preview.findChild<QPushButton *>("panelConfirm")->isVisible());
        QVERIFY(preview.grab().save("scene-dialogue.png"));
        preview.findChild<QPushButton *>("panelConfirm")->click();
        seek(125);
        QCOMPARE(preview.property("sceneDialogueState").toInt(), 4);
        const auto resumed = preview.renderedFrame();
        QVERIFY(resumed != waiting);
        seek(275);
        QCOMPARE(preview.property("sceneDialogueState").toInt(), -1);
        QVERIFY(!preview.findChild<QPushButton *>("panelConfirm")->isVisible());
        seek(60);
        QCOMPARE(preview.renderedFrame(), waiting);
        seek(125);
        QCOMPARE(preview.renderedFrame(), resumed);
        preview.findChild<QPushButton *>("sceneRestart")->click();
        seek(125);
        QCOMPARE(preview.renderedFrame(), resumed);
        auto name = preview.findChild<QLineEdit *>("sceneDialogueName0");
        name->setText("Joe");
        QMetaObject::invokeMethod(name, "editingFinished");
        seek(60);
        QCOMPARE(preview.property("sceneDialogueState").toInt(), 3);
        QVERIFY(preview.renderedFrame() != waiting);
        QCOMPARE(dialogue.json(), savedText);
        QCOMPARE(scenes.json(), savedScenes);
        preview.setDialogues(dialogue);
        QCOMPARE(preview.property("sceneTick").toInt(), 60); // No reset without a text change.
        dialogue.setEntry(Dialogues::Table::Primary, 341, 23, "@v020@y003Current edit@aEnd@s");
        preview.setDialogues(dialogue);
        QCOMPARE(preview.property("sceneTick").toInt(), 0);
        seek(60);
        QCOMPARE(preview.property("sceneDialogueState").toInt(), 3);
        QVERIFY(preview.renderedFrame() != waiting);
        name->setFocus(); // Focus loss during widget destruction must not restart dead state.
    }
    void storyCastPreview() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        const quint32 key = 0x1fa8bf4 - 0x594284;
        auto program = scenes.program(key);
        for (auto &command : program.commands)
            if (command.opcode == 0x1b) {
                command.operands[4] = 0;
                command.operands[5] = 0;
                break;
            }
        scenes.replace(key, program.encode());
        const auto before = scenes.json();
        ScenePreview preview(rom);
        preview.resize(1200, 900);
        preview.show();
        preview.selectScene(scenes, key, 2);
        auto first = preview.findChild<QComboBox *>("sceneCast0");
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        QVERIFY(first->isVisible());
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        position->setValue(3);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 3);
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        const auto original = preview.renderedFrame();
        first->setCurrentIndex(first->findData(0x54));
        QCOMPARE(preview.property("sceneTick").toInt(), 0);
        position->setValue(3);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 3);
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        const auto changed = preview.renderedFrame();
        QVERIFY(changed != original);
        QVERIFY(preview.grab().save("scene-story-cast.png"));
        position->setValue(1);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 1);
        position->setValue(3);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 3);
        QCOMPARE(preview.renderedFrame(), changed);
        QCOMPARE(scenes.json(), before);
        preview.selectScene(scenes, scenes.original().entries().first().key, 2);
        QVERIFY(!first->isVisible());
    }
    void panelPreviewSeek() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        quint32 source = 0;
        for (const auto &entry : scenes.original().entries())
            if (entry.program.resourceClass() == 4) {
                source = entry.key;
                break;
            }
        QVERIFY(source);
        for (quint32 preset : {33U, 3U, 1U, 34U}) {
            const auto id =
                scenes.createScene(QString("Panel %1 replay").arg(preset), source, false);
            QVector<quint32> words{0x80000007U, 47, 0x80000006U, 0,  0x3a, 2, 0,
                                   0,           0,  70,          70, 130,  8};
            for (int i = 0; i < 4; ++i)
                words << 0 << 0x80000000U;
            // Open only when the native owner query reports this slot absent.
            words << 0x56 << 0x6a << 0 << 0xfffffffbU << 4 << 0x62 << 4 << preset << 0x80000002U;
            for (int i = 0; i < 150; ++i)
                words << 0 << 0x80000000U;
            words << 0x6b << 4;
            for (int i = 0; i < 30; ++i)
                words << 0 << 0x80000000U;
            words << 0x80000001U << 0xff000004U;
            QByteArray bytes(words.size() * 4, 0);
            for (int i = 0; i < words.size(); ++i)
                put32(bytes, i * 4, words[i]);
            scenes.replace(id, bytes);
            const auto savedScenes = scenes.json();
            ScenePreview preview(rom);
            preview.resize(1200, 800);
            preview.show();
            preview.selectScene(scenes, id, 0);
            auto position = preview.findChild<QSpinBox *>("scenePosition");
            auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
            auto reward = preview.findChild<QComboBox *>("sceneReward");
            QCOMPARE(reward->isVisible(), preset == 34);
            const auto seek = [&](int tick) {
                position->setValue(tick);
                QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), tick, 20000);
                QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
            };
            seek(4);
            const auto before = preview.renderedFrame();
            seek(12);
            const auto opening = preview.renderedFrame();
            QVERIFY(opening != before);
            seek(20);
            const auto ready = preview.renderedFrame();
            QVERIFY(ready != opening);
            QVERIFY(status->text().contains("approximate frame"));
            if (preset == 3 || preset == 1) {
                preview.findChild<QPushButton *>("panelDown")->click();
                QCOMPARE(preview.property("sceneTick").toInt(), 21);
                QVERIFY(preview.renderedFrame() != ready);
            }
            preview.findChild<QPushButton *>("panelConfirm")->click();
            const int acceptedTick = preview.property("sceneTick").toInt();
            const auto accepted = preview.renderedFrame();
            if (preset == 3)
                QVERIFY(preview.grab().save("scene-briefing-panel.png"));
            seek(40);
            QCOMPARE(preview.renderedFrame(), before);
            QVERIFY(!preview.findChild<QPushButton *>("panelConfirm")->isVisible());
            seek(124);
            QCOMPARE(preview.renderedFrame(), before);
            seek(12);
            QCOMPARE(preview.renderedFrame(), opening);
            seek(acceptedTick);
            QCOMPARE(preview.renderedFrame(), accepted);
            seek(160);
            QCOMPARE(preview.renderedFrame(), before);
            preview.findChild<QPushButton *>("sceneRestart")->click();
            seek(acceptedTick);
            QCOMPARE(preview.renderedFrame(), accepted);
            // A new input after rewinding replaces the recorded future.
            seek(20);
            preview.findChild<QPushButton *>("panelCancel")->click();
            seek(40);
            QCOMPARE(preview.renderedFrame(), before);
            if (preset == 34) {
                reward->setCurrentIndex(reward->findData(0x201));
                QCOMPARE(preview.property("sceneTick").toInt(), 0);
                seek(20);
                const auto changed = preview.renderedFrame();
                QVERIFY(changed != ready);
                QVERIFY(preview.grab().save("scene-reward-panel.png"));
                seek(124);
                QCOMPARE(preview.renderedFrame(), changed); // Changing reward clears prior input.
                seek(20);
                QCOMPARE(preview.renderedFrame(), changed);
                preview.findChild<QPushButton *>("panelConfirm")->click();
                seek(40);
                QCOMPARE(preview.renderedFrame(), before);
                preview.findChild<QPushButton *>("sceneRestart")->click();
                seek(40);
                QCOMPARE(preview.renderedFrame(), before);
            }
            QCOMPARE(scenes.json(), savedScenes);
        }
    }
    void irisPreviewSeek() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        quint32 source = 0;
        for (const auto &entry : scenes.original().entries())
            if (entry.program.resourceClass() == 4) {
                source = entry.key;
                break;
            }
        QVERIFY(source);
        const auto id = scenes.createScene("Iris close and open", source, false);
        QVector<quint32> words{0x80000007U, 47, 0x80000006U, 0, 0x3a, 2, 0, 0, 0, 70, 70, 130, 8};
        for (int i = 0; i < 4; ++i)
            words << 0 << 0x80000000U;
        words << 0x4d << 320 << 240 << 0 << 0 << 0 << 255 << 12 << 0 << 0;
        for (int i = 0; i < 140; ++i)
            words << 0 << 0x80000000U;
        words << 0x4d << 320 << 240 << 0 << 0 << 0 << 255 << 12 << 1 << 0;
        for (int i = 0; i < 30; ++i)
            words << 0 << 0x80000000U;
        words << 0x80000001U << 0xff000004U;
        QByteArray bytes(words.size() * 4, 0);
        for (int i = 0; i < words.size(); ++i)
            put32(bytes, i * 4, words[i]);
        scenes.replace(id, bytes);
        ScenePreview preview(rom);
        preview.resize(1200, 800);
        preview.show();
        preview.selectScene(scenes, id, 0);
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        const auto seek = [&](int tick) {
            position->setValue(tick);
            QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), tick, 20000);
            QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        };
        seek(4);
        const auto before = preview.renderedFrame();
        seek(11);
        const auto middle = preview.renderedFrame();
        QVERIFY(middle != before);
        QVERIFY(status->text().contains("Iris: native layers"));
        QVERIFY(preview.grab().save("scene-iris.png"));
        seek(25);
        const auto closed = preview.renderedFrame();
        QVERIFY(closed != middle);
        seek(124);
        QCOMPARE(preview.renderedFrame(), closed);
        seek(151);
        const auto opening = preview.renderedFrame();
        QVERIFY(opening != closed);
        seek(160);
        QCOMPARE(preview.renderedFrame(), before);
        seek(11);
        QCOMPARE(preview.renderedFrame(), middle);
        seek(151);
        QCOMPARE(preview.renderedFrame(), opening);
        preview.findChild<QPushButton *>("sceneRestart")->click();
        seek(25);
        QCOMPARE(preview.renderedFrame(), closed);
    }
    void screenCropPreviewSeek() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        const auto source = scenes.original().entries().first();
        const auto id = scenes.createScene("Screen crop playback", source.key, false);
        QVector<quint32> words{0x80000006U, 0};
        for (int i = 0; i < 3; ++i)
            words << 0 << 0x80000000U;
        words << 0x3b << 0xffffffffU << 0xffffffffU << 80 << 70 << 4 << 1;
        for (int i = 0; i < 140; ++i)
            words << 0 << 0x80000000U;
        words << 0x3b << 80 << 70 << 24 << 24 << 4 << 0;
        for (int i = 0; i < 20; ++i)
            words << 0 << 0x80000000U;
        words << 0x80000001U << (0xff000000U | quint32(source.program.resourceClass()));
        QByteArray bytes(words.size() * 4, 0);
        for (int i = 0; i < words.size(); ++i)
            put32(bytes, i * 4, words[i]);
        scenes.replace(id, bytes);
        ScenePreview preview(rom);
        preview.resize(1100, 800);
        preview.show();
        preview.selectScene(scenes, id, 0);
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        const auto seek = [&](int tick) {
            position->setValue(tick);
            QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), tick, 20000);
            QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        };
        seek(3);
        const auto before = preview.renderedFrame();
        QCOMPARE(before.pixelColor(160, 10), QColor(Qt::black));
        seek(5);
        const auto halfway = preview.renderedFrame();
        QVERIFY(halfway != before);
        seek(8);
        const auto closed = preview.renderedFrame();
        QVERIFY(closed != halfway);
        QCOMPARE(closed.pixelColor(160, 79), QColor(Qt::black));
        QCOMPARE(closed.pixelColor(160, 170), QColor(Qt::black));
        QVERIFY(closed.pixelColor(160, 110) != QColor(Qt::black));
        preview.repaint();
        QCOMPARE(preview.renderedFrame(), closed);
        seek(124);
        QCOMPARE(preview.renderedFrame(), closed);
        seek(5);
        QCOMPARE(preview.renderedFrame(), halfway);
        seek(151);
        QCOMPARE(preview.renderedFrame(), before);
        preview.findChild<QPushButton *>("sceneRestart")->click();
        seek(8);
        QCOMPARE(preview.renderedFrame(), closed);
    }
    void launchImageSelection() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        quint32 source = 0;
        for (const auto &entry : scenes.original().entries())
            if (entry.program.resourceClass() == 4) {
                source = entry.key;
                break;
            }
        QVERIFY(source);
        const auto missing = scenes.createScene("Event-selected image", source, false);
        const auto explicitImage = scenes.createScene("Scene-selected image", source, false);
        QVector<quint32> words{0x3a, 1, 0, 0, 0, 70, 70, 130, 8,
                               0, 0x80000000U, 0, 0x80000000U, 0x80000001U, 0xff000004U};
        QByteArray bytes(words.size() * 4, 0);
        for (int i = 0; i < words.size(); ++i)
            put32(bytes, i * 4, words[i]);
        scenes.replace(missing, bytes);
        QByteArray selected(8, 0);
        put32(selected, 0, 0x80000007U);
        put32(selected, 4, 47);
        selected += bytes;
        scenes.replace(explicitImage, selected);
        const auto before = scenes.json();
        ScenePreview preview(rom);
        preview.resize(1100, 800);
        preview.show();
        preview.selectScene(scenes, missing, 0);
        auto choice = preview.findChild<QComboBox *>("sceneLaunchImage");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        auto step = preview.findChild<QPushButton *>("sceneStep");
        QVERIFY(choice->isVisible());
        QVERIFY(!step->isEnabled());
        QVERIFY(status->text().contains("Choose the event's launch image"));
        QVERIFY(choice->findData(47) > 0);
        QCOMPARE(choice->findData(10), -1); // This retail row has no image selection.
        choice->setCurrentIndex(choice->findData(47));
        QVERIFY2(step->isEnabled(), qPrintable(status->text()));
        step->click();
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        const auto first = preview.renderedFrame();
        QVERIFY(!first.isNull());
        int visible = 0;
        for (int y = 0; y < first.height(); ++y)
            for (int x = 0; x < first.width(); ++x)
                visible += first.pixelColor(x, y).alpha() > 16;
        QVERIFY(visible > 1000);
        choice->setCurrentIndex(choice->findData(1));
        QCOMPARE(preview.property("sceneTick").toInt(), 0);
        step->click();
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        QVERIFY(preview.renderedFrame() != first);
        preview.selectScene(scenes, explicitImage, 0);
        QCOMPARE(choice->currentData().toInt(), -1);
        step->click();
        QCOMPARE(preview.renderedFrame(), first);
        preview.selectScene(scenes, missing, 0);
        QVERIFY(!step->isEnabled());
        QCOMPARE(choice->currentData().toInt(), -1);
        QCOMPARE(scenes.json(), before);
    }
    void manualImageInputReplay() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        const auto rom=Rom::open(path);auto scenes=ScenePrograms::load(rom);quint32 source=0;
        for(const auto &entry:scenes.original().entries())if(entry.program.resourceClass()==4){source=entry.key;break;}
        QVERIFY(source);const auto id=scenes.createScene("Manual image input",source,false);
        // Retail row47 is288x276, exceeding both manual movement thresholds.
        QVector<quint32> words{0x80000007U,47,0x3a,1,0,0,0,70,70,130,8,0x50,0,1,0,0x6c,56};
        for(int i=0;i<170;++i)words<<0x55<<0<<0x80000000U;
        words<<0x6d<<0<<0x80000000U<<0<<0x80000000U<<0x80000001U<<0xff000004U;
        QByteArray bytes(words.size()*4,0);for(int i=0;i<words.size();++i)put32(bytes,i*4,words[i]);scenes.replace(id,bytes);
        ScenePreview preview(rom);preview.resize(1200,800);preview.show();preview.selectScene(scenes,id,0);
        auto position=preview.findChild<QSpinBox *>("scenePosition");auto status=preview.findChild<QLabel *>("scenePlaybackStatus");
        auto controls=preview.findChild<QWidget *>("sceneImageControls");auto view=preview.findChild<QGraphicsView *>("sceneCanvas");
        QVERIFY(!controls->isVisible());position->setValue(4);QTRY_COMPARE(preview.property("sceneTick").toInt(),4);
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(status->text()));QVERIFY(controls->isVisible());
        const auto original=preview.renderedFrame();
        QTest::keyPress(view,Qt::Key_Right);QCOMPARE(preview.property("sceneTick").toInt(),5);
        position->setValue(35);QTRY_COMPARE(preview.property("sceneTick").toInt(),35);QTest::keyRelease(view,Qt::Key_Right);
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(status->text()));const auto moved=preview.renderedFrame();QVERIFY(moved!=original);
        position->setValue(124);QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(),124,20000);
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(status->text()));
        position->setValue(35);QTRY_COMPARE(preview.property("sceneTick").toInt(),35);QCOMPARE(preview.renderedFrame(),moved);
        preview.findChild<QPushButton *>("sceneRestart")->click();position->setValue(35);
        QTRY_COMPARE(preview.property("sceneTick").toInt(),35);QCOMPARE(preview.renderedFrame(),moved);
        position->setValue(10);QTRY_COMPARE(preview.property("sceneTick").toInt(),10);
        QTest::mouseClick(preview.findChild<QPushButton *>("imagePanLeft"),Qt::LeftButton);
        position->setValue(35);QTRY_COMPARE(preview.property("sceneTick").toInt(),35);QVERIFY(preview.renderedFrame()!=moved);
        const auto beforeZoom=preview.renderedFrame();
        QTest::mouseClick(preview.findChild<QPushButton *>("imageZoomOut"),Qt::LeftButton);
        position->setValue(75);QTRY_COMPARE(preview.property("sceneTick").toInt(),75);
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(status->text()));QVERIFY(preview.renderedFrame()!=beforeZoom);
        const auto zoomed=preview.renderedFrame();position->setValue(124);QTRY_COMPARE(preview.property("sceneTick").toInt(),124);
        position->setValue(75);QTRY_COMPARE(preview.property("sceneTick").toInt(),75);QCOMPARE(preview.renderedFrame(),zoomed);
        QVERIFY(preview.grab().save("manual-image-controls.png"));
        position->setValue(171);QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(),171,20000);
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(status->text()));QVERIFY(!controls->isVisible());
    }
    void imagePresentationPlayback() {
        QFETCH(int,transition);
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        const auto rom=Rom::open(path);auto scenes=ScenePrograms::load(rom);quint32 source=0;
        for(const auto &entry:scenes.original().entries())if(entry.program.resourceClass()==4){source=entry.key;break;}
        QVERIFY(source);const auto id=scenes.createScene("Image presentation playback",source,false);
        QVector<quint32> words{0x80000007U,1,0x3a,1,0,0,0,70,70,130,8};
        if(transition<0)words<<0x76<<quint32(-1)<<quint32(-1)<<0<<50<<0<<0<<20;
        else words<<0<<0x80000000U<<0x50<<quint32(transition)<<20<<4;
        for(int i=0;i<140;++i)words<<0<<0x80000000U;
        words<<0x80000001U<<0xff000004U;
        QByteArray bytes(words.size()*4,0);for(int i=0;i<words.size();++i)put32(bytes,i*4,words[i]);
        scenes.replace(id,bytes);ScenePreview preview(rom);preview.resize(1000,750);preview.show();preview.selectScene(scenes,id,0);
        auto position=preview.findChild<QSpinBox *>("scenePosition");auto status=preview.findChild<QLabel *>("scenePlaybackStatus");
        preview.findChild<QPushButton *>("sceneStep")->click();
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(status->text()));
        const auto first=preview.renderedFrame();int visible=0;
        for(int y=0;y<first.height();++y)for(int x=0;x<first.width();++x)if(first.pixelColor(x,y).alpha()>16)++visible;
        QVERIFY(visible>1000);
        position->setValue(12);QTRY_COMPARE(preview.property("sceneTick").toInt(),12);
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(status->text()));QVERIFY(preview.renderedFrame()!=first);
        const auto moved=preview.renderedFrame();
        QVERIFY(preview.grab().save(QString("scene-image-transition-%1.png").arg(transition)));
        position->setValue(124);QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(),124,20000);
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(status->text()));
        position->setValue(12);QTRY_COMPARE(preview.property("sceneTick").toInt(),12);QCOMPARE(preview.renderedFrame(),moved);
        position->setValue(1);QTRY_COMPARE(preview.property("sceneTick").toInt(),1);QCOMPARE(preview.renderedFrame(),first);
        preview.update();QCoreApplication::processEvents();QCOMPARE(preview.renderedFrame(),first);
        QVERIFY(preview.grab().save(QString("scene-image-presentation-%1.png").arg(transition)));
        preview.findChild<QPushButton *>("sceneRestart")->click();preview.findChild<QPushButton *>("sceneStep")->click();QCOMPARE(preview.renderedFrame(),first);
    }
    void scatteredPropsSeek_data() {
        QTest::addColumn<int>("environmentSelector");
        QTest::newRow("snow") << 35;
        QTest::newRow("scrolling-texture") << 31;
        QTest::newRow("dynamic-hall-props") << 34;
    }
    void scatteredPropsSeek() {
        QFETCH(int, environmentSelector);
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        const auto source = scenes.original().entries().first();
        const auto id = scenes.createScene("Scattered props playback", source.key, false);
        QVector<quint32> words{0x80000006U, quint32(environmentSelector)};
        for (int i = 0; i < 140; ++i) words << 0 << 0x80000000U;
        words << 0x80000001U << (0xff000000U | quint32(source.program.resourceClass()));
        QByteArray bytes(words.size() * 4, 0);
        for (int i = 0; i < words.size(); ++i) put32(bytes, i * 4, words[i]);
        scenes.replace(id, bytes);
        ScenePreview preview(rom); preview.resize(1100, 800); preview.show();
        preview.selectScene(scenes, id, 2);
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        preview.findChild<QPushButton *>("sceneStep")->click();
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        const auto first = preview.renderedFrame(); QVERIFY(!first.isNull());
        position->setValue(124); QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), 124, 20000);
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        QVERIFY(preview.renderedFrame() != first);
        position->setValue(1); QTRY_COMPARE(preview.property("sceneTick").toInt(), 1);
        QCOMPARE(preview.renderedFrame(), first);
        preview.update(); QCoreApplication::processEvents(); QCOMPARE(preview.renderedFrame(), first);
        preview.findChild<QPushButton *>("sceneRestart")->click();
        preview.findChild<QPushButton *>("sceneStep")->click(); QCOMPARE(preview.renderedFrame(), first);
        QVERIFY(preview.grab().save(QString("scattered-props-preview-%1.png").arg(environmentSelector)));
    }
    void worldScenePanAndSeek() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        const auto source = scenes.original().entries().first();
        const auto id = scenes.createScene("World projection pan", source.key, false);
        QVector<quint32> words{0x46, 0, 30, 1, 2, 40, 20, 0, 100, 0,
                              0, 0x80000000U, 0x2c, 25000, quint32(-10000), 1000, 1000, 2};
        for (int i = 0; i < 6; ++i) words << 0 << 0x80000000U;
        words << 0x80000001U << (0xff000000U | quint32(source.program.resourceClass()));
        auto save = [&] {
            QByteArray bytes(words.size() * 4, 0);
            for (int i = 0; i < words.size(); ++i) put32(bytes, i * 4, words[i]);
            scenes.replace(id, bytes);
        };
        save();
        ScenePreview preview(rom);
        preview.resize(1100, 700); preview.show(); preview.selectScene(scenes, id, 2);
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        preview.findChild<QPushButton *>("sceneStep")->click();
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(preview.findChild<QLabel *>("scenePlaybackStatus")->text()));
        const auto first = preview.renderedFrame();
        auto center = [](const QImage &image) {
            QPointF sum; double weight = 0;
            // Measure the sprite inside the native scissor, excluding its black border.
            for (int y = 24; y < 215; ++y) for (int x = 0; x < 319; ++x) {
                const auto a = image.pixelColor(x,y).alpha(); sum += QPointF(x,y)*a; weight += a;
            }
            return weight ? sum / weight : QPointF(-10000,-10000);
        };
        QVERIFY(center(first).x() > 180);
        position->setValue(4);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 4);
        const auto delta = center(preview.renderedFrame()) - center(first);
        QVERIFY(qAbs(delta.x()-25) < 1.1); QVERIFY(qAbs(delta.y()-10) < 1.1);
        position->setValue(1); QTRY_COMPARE(preview.property("sceneTick").toInt(), 1);
        QCOMPARE(preview.renderedFrame(), first);
        words.prepend(57); words.prepend(0x80000006U); save();
        preview.selectScene(scenes, id, 2); preview.findChild<QPushButton *>("sceneStep")->click();
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(preview.findChild<QLabel *>("scenePlaybackStatus")->text()));
        const auto hall = preview.renderedFrame();
        QVERIFY(hall != first);
        position->setValue(4); QTRY_COMPARE(preview.property("sceneTick").toInt(), 4);
        QVERIFY(preview.renderedFrame() != hall);
        position->setValue(1); QTRY_COMPARE(preview.property("sceneTick").toInt(), 1);
        QCOMPARE(preview.renderedFrame(), hall);
        QVERIFY(preview.grab().save("world-scene-pan.png"));
    }
    void actorColorFocusPlayback() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM"); if(path.isEmpty()) QSKIP("Private ROM required");
        const auto rom=Rom::open(path); auto scenes=ScenePrograms::load(rom);
        const auto source=scenes.original().entries().first(); const auto id=scenes.createScene("Actor color focus",source.key,false);
        QVector<quint32> words{0x80000006U,2,0x73,0x14,0,30,1,2,quint32(-40000),0,0,0,0,0x1c,0,6,0x14,1,31,1,2,40000,0,0,0,0,0x1c,1,6,0,0x80000000U,0x80,0,0,0x80000000U,0x80,1};
        for(int i=0;i<130;++i) words<<0<<0x80000000U;
        words<<0x80000001U<<(0xff000000U|quint32(source.program.resourceClass()));
        QByteArray bytes(words.size()*4,0); for(int i=0;i<words.size();++i) put32(bytes,i*4,words[i]); scenes.replace(id,bytes);
        ScenePreview preview(rom); preview.resize(1100,800); preview.show(); preview.selectScene(scenes,id,0);
        auto next=preview.findChild<QPushButton*>("sceneStep"); auto position=preview.findChild<QSpinBox*>("scenePosition");
        next->click(); const auto sepia=preview.renderedFrame(); next->click(); const auto first=preview.renderedFrame(); QVERIFY(first!=sepia);
        next->click(); const auto second=preview.renderedFrame(); QVERIFY(second!=first);
        position->setValue(124); QTRY_COMPARE(preview.property("sceneTick").toInt(),124);
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(preview.findChild<QLabel*>("scenePlaybackStatus")->text()));
        position->setValue(1); QTRY_COMPARE(preview.property("sceneTick").toInt(),1); QCOMPARE(preview.renderedFrame(),sepia);
        position->setValue(2); QTRY_COMPARE(preview.property("sceneTick").toInt(),2); QCOMPARE(preview.renderedFrame(),first);
        position->setValue(3); QTRY_COMPARE(preview.property("sceneTick").toInt(),3); QCOMPARE(preview.renderedFrame(),second);
        QVERIFY(preview.grab().save("actor-color-focus.png"));
    }
    void specialBlackPlayback() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM"); if(path.isEmpty()) QSKIP("Private ROM required");
        const auto rom=Rom::open(path); auto scenes=ScenePrograms::load(rom);
        const auto source=scenes.original().entries().first(); const auto id=scenes.createScene("Special black fade",source.key,false);
        QVector<quint32> words{0x80000006U,34,0x14,0,30,1,2,0,0,0,0,0,0xa9,0x80000008U,4,4};
        for(int i=0;i<130;++i) words<<0<<0x80000000U;
        words<<0x80000001U<<(0xff000000U|quint32(source.program.resourceClass()));
        QByteArray bytes(words.size()*4,0); for(int i=0;i<words.size();++i) put32(bytes,i*4,words[i]); scenes.replace(id,bytes);
        ScenePreview preview(rom); preview.selectScene(scenes,id,2); preview.findChild<QPushButton*>("sceneStep")->click();
        const auto first=preview.renderedFrame(); auto position=preview.findChild<QSpinBox*>("scenePosition");
        position->setValue(31); QTRY_COMPARE(preview.property("sceneTick").toInt(),31); const auto half=preview.renderedFrame(); QVERIFY(half!=first);
        position->setValue(61); QTRY_COMPARE(preview.property("sceneTick").toInt(),61);
        const auto black=preview.renderedFrame();
        for(int y=0;y<240;++y) for(int x=0;x<320;++x) QCOMPARE(black.pixelColor(x,y),QColor(Qt::black));
        position->setValue(1); QTRY_COMPARE(preview.property("sceneTick").toInt(),1); QCOMPARE(preview.renderedFrame(),first);
        position->setValue(31); QTRY_COMPARE(preview.property("sceneTick").toInt(),31); QCOMPARE(preview.renderedFrame(),half);
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(preview.findChild<QLabel*>("scenePlaybackStatus")->text()));
    }
    void capturedScenePlayback_data() {
        QTest::addColumn<int>("mode");
        QTest::newRow("world") << 2; QTest::newRow("layered") << 0;
    }
    void capturedScenePlayback() {
        QFETCH(int,mode);
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM"); if(path.isEmpty()) QSKIP("Private ROM required");
        const auto rom=Rom::open(path); auto scenes=ScenePrograms::load(rom);
        const auto source=scenes.original().entries().first(); const auto id=scenes.createScene("Captured transitions",source.key,false);
        ScenePreview preview(rom); preview.resize(1100,800); preview.show();
        auto position=preview.findChild<QSpinBox*>("scenePosition"); auto next=preview.findChild<QPushButton*>("sceneStep");
        QVector<quint32> words{0x80000006U, mode ? 34U : 2U, 0,0x80000000U,0x80000008U,2,4};
        for(int i=0;i<5;++i) words << 0 << 0x80000000U;
        words << 0x80000008U << 3 << 4;
        for(int i=0;i<130;++i) words << 0 << 0x80000000U;
        words << 0x80000001U << (0xff000000U|quint32(source.program.resourceClass()));
        QByteArray bytes(words.size()*4,0); for(int i=0;i<words.size();++i) put32(bytes,i*4,words[i]);
        scenes.replace(id,bytes); preview.selectScene(scenes,id,mode); next->click();
        const auto normal=preview.renderedFrame(); next->click(); const auto first=preview.renderedFrame();
        QVERIFY(first!=normal);
        position->setValue(5); QTRY_COMPARE(preview.property("sceneTick").toInt(),5);
        const auto sepia=preview.renderedFrame(); QVERIFY(sepia!=first); QVERIFY(sepia!=normal);
        position->setValue(7); QTRY_COMPARE(preview.property("sceneTick").toInt(),7);
        const auto restoring=preview.renderedFrame(); QVERIFY(restoring!=sepia);
        position->setValue(10); QTRY_COMPARE(preview.property("sceneTick").toInt(),10);
        const auto restored=preview.renderedFrame(); QVERIFY(restored!=restoring);
        if(!mode) QCOMPARE(restored,normal); // World props have their own draw-driven poses.
        position->setValue(124); QTRY_COMPARE(preview.property("sceneTick").toInt(),124);
        QVERIFY2(!preview.property("sceneStopped").toBool(),qPrintable(preview.findChild<QLabel*>("scenePlaybackStatus")->text()));
        position->setValue(2); QTRY_COMPARE(preview.property("sceneTick").toInt(),2); QCOMPARE(preview.renderedFrame(),first);
        position->setValue(5); QTRY_COMPARE(preview.property("sceneTick").toInt(),5); QCOMPARE(preview.renderedFrame(),sepia);
        QVERIFY(preview.grab().save(QString("captured-scene-%1.png").arg(mode)));
        position->setValue(10); QTRY_COMPARE(preview.property("sceneTick").toInt(),10); QCOMPARE(preview.renderedFrame(),restored);
    }
    void sceneColorPlayback_data() {
        QTest::addColumn<int>("kind");
        QTest::newRow("world-background-and-props") << 0;
        QTest::newRow("layered-background") << 1;
        QTest::newRow("sprite-channel-color") << 2;
        QTest::newRow("actor-material-independent") << 3;
    }
    void sceneColorPlayback() {
        QFETCH(int, kind);
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        const auto source = scenes.original().entries().first();
        const auto id = scenes.createScene("Scene color playback", source.key, false);
        ScenePreview preview(rom); preview.resize(1100, 800); preview.show();
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        auto next = preview.findChild<QPushButton *>("sceneStep");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        const QVector<quint32> setup = kind < 2 ? QVector<quint32>{0x80000006U, kind ? 2U : 34U}
            : kind == 2 ? QVector<quint32>{0x46, 0, 30, 1, 2, 6, 0, 0, 100, 0}
            : QVector<quint32>{0x14, 0, 30, 1, 2, 0, 0, 0, 0, 0};
        auto select = [&](bool tint) {
            auto words = setup;
            if (tint) words << 0x33 << 255 << 255 << 255 << 0 << 255 << 0 << 4 << 0xffffffffU;
            for (int i = 0; i < 8; ++i) words << 0 << 0x80000000U;
            words << 0x80000001U << (0xff000000U | quint32(source.program.resourceClass()));
            QByteArray bytes(words.size() * 4, 0);
            for (int i = 0; i < words.size(); ++i) put32(bytes, i * 4, words[i]);
            scenes.replace(id, bytes); preview.selectScene(scenes, id, kind == 0 ? 2 : 0);
            next->click();
        };
        select(false); position->setValue(5); QTRY_COMPARE(preview.property("sceneTick").toInt(), 5);
        const auto untinted = preview.renderedFrame();
        select(true); const auto first = preview.renderedFrame();
        position->setValue(5); QTRY_COMPARE(preview.property("sceneTick").toInt(), 5);
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        const auto last = preview.renderedFrame();
        if (kind == 3) QCOMPARE(last, untinted);
        else {
            QVERIFY(last != untinted);
            int visible = 0;
            for (int y = 0; y < 240; ++y) for (int x = 0; x < 320; ++x) {
                const auto c = last.pixelColor(x, y);
                if (!c.alpha()) continue;
                ++visible; QCOMPARE(c.red(), 0); QCOMPARE(c.blue(), 0);
                if (kind == 2) QCOMPARE(c.green(), 0);
            }
            QVERIFY(visible > 30);
        }
        position->setValue(1); QTRY_COMPARE(preview.property("sceneTick").toInt(), 1);
        QCOMPARE(preview.renderedFrame(), first);
        position->setValue(5); QTRY_COMPARE(preview.property("sceneTick").toInt(), 5);
        QCOMPARE(preview.renderedFrame(), last);
        if (kind == 0) QVERIFY(preview.grab().save("scene-color-preview.png"));
    }
    void sceneRoomComposition() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        const auto source = scenes.original().entries().first();
        const auto id = scenes.createScene("Room composition", source.key, false);
        ScenePreview preview(rom);
        preview.resize(1100, 700);
        preview.show();
        auto run = [&](QVector<quint32> words) {
            words << 0 << 0x80000000U << 0 << 0x80000000U << 0x80000001U
                  << (0xff000000U | quint32(source.program.resourceClass()));
            QByteArray bytes(words.size() * 4, 0);
            for (int i = 0; i < words.size(); ++i) put32(bytes, i * 4, words[i]);
            scenes.replace(id, bytes);
            preview.selectScene(scenes, id, 0);
            preview.findChild<QPushButton *>("sceneStep")->click();
            return preview.renderedFrame();
        };
        const QVector<quint32> room{0x80000006U, 2};
        const QVector<quint32> actor{0x14, 0, 30, 1, 2, 0, quint32(-40000), 0, 0, 0};
        const auto background = run(room);
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(preview.findChild<QLabel *>("scenePlaybackStatus")->text()));
        const auto actorFrame = run(actor);
        auto expected = background.copy();
        { QPainter painter(&expected); painter.drawImage(0, 0, actorFrame); }
        const auto front = run(room + actor + QVector<quint32>{0x1c, 0, 6});
        QCOMPARE(front, expected);
        const auto behind = run(room + actor);
        QVERIFY(behind != front);
        QVERIFY(behind != background);
        QVERIFY(preview.grab().save("scene-room-composition.png"));
        const auto empty = run({0x80000006U, 23});
        QVERIFY(!preview.property("sceneStopped").toBool());
        QVERIFY(empty != background);
        for (int y = 0; y < empty.height(); ++y) for (int x = 0; x < empty.width(); ++x)
            if (x >= 319 || y < 24 || y >= 215)
                QCOMPARE(empty.pixelColor(x, y), QColor(Qt::black));
            else
                QCOMPARE(empty.pixelColor(x, y).alpha(), 0);
    }
    void sceneSpriteComposition() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        const auto source = scenes.original().entries().first();
        const auto id = scenes.createScene("Sprite composition", source.key, false);
        ScenePreview preview(rom);
        preview.show();
        auto next = preview.findChild<QPushButton *>("sceneStep");
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        QVERIFY(next && position && status);
        auto run = [&](QVector<quint32> words) {
            for (int i = 0; i < 8; ++i) words << 0 << 0x80000000U;
            words << 0x80000001U << (0xff000000U | quint32(source.program.resourceClass()));
            QByteArray bytes(words.size() * 4, 0);
            for (int i = 0; i < words.size(); ++i) put32(bytes, i * 4, words[i]);
            scenes.replace(id, bytes);
            preview.selectScene(scenes, id, 0);
            next->click();
            return preview.renderedFrame();
        };
        const QVector<quint32> actor{0x14, 0, 30, 1, 2, 0, 0, 0, 0, 0};
        QVector<quint32> sprite{0x46, 0, 30, 1, 2, 6, 0, 0, 100, 0};
        const auto actorFrame = run(actor), spriteFrame = run(sprite);
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        int overlap = 0;
        for (int y = 0; y < 240; ++y) for (int x = 0; x < 320; ++x)
            overlap += actorFrame.pixelColor(x, y).alpha() > 0 && spriteFrame.pixelColor(x, y).alpha() > 0;
        QVERIFY(overlap > 10);
        // Drawing a sprite also sets environment alpha to one. An offscreen
        // sprite establishes that same native state for the isolated Actor frame.
        auto offscreenSprite = sprite;
        offscreenSprite[5] = 1000;
        const auto actorAfterSprite = run(actor + offscreenSprite);
        auto expected = spriteFrame.copy();
        { QPainter painter(&expected); painter.drawImage(0, 0, actorAfterSprite); }
        const auto together = run(actor + sprite);
        QCOMPARE(together, expected); // Sprite pass is before Actor pass within a channel.
        position->setValue(4);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 4);
        position->setValue(1);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 1);
        QCOMPARE(preview.renderedFrame(), together);
        sprite[7] = 1; // A later sprite channel must instead appear over channel-zero Actors.
        const auto laterSprite = run(sprite);
        expected = actorFrame.copy();
        { QPainter painter(&expected); painter.drawImage(0, 0, laterSprite); }
        QCOMPARE(run(actor + sprite), expected);
        QVERIFY(preview.grab().save("scene-sprite-composition.png"));
    }
    void scenePlaybackPreview() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        const auto source = scenes.original().entries().first();
        const auto id = scenes.createScene("Preview test", source.key, false);
        QVector<quint32> words{0x14, 0, 30, 1, 2, 0, 0, 0, 0, 0,
                              0x1e, 0, 255, 255, 255, 255, 0, 0, 4, 1};
        for (int i = 0; i < 8; ++i) words << 0 << 0x80000000U;
        words << 0x80000006U << 0xffffffffU << 0x80000001U
              << (0xff000000U | quint32(source.program.resourceClass()));
        auto encode = [&] {
            QByteArray bytes(words.size() * 4, 0);
            for (int i = 0; i < words.size(); ++i) put32(bytes, i * 4, words[i]);
            return bytes;
        };
        scenes.replace(id, encode());
        ScenePreview preview(rom);
        preview.resize(1100, 800);
        preview.show();
        preview.selectScene(scenes, id, 0);
        auto next = preview.findChild<QPushButton *>("sceneStep");
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        QVERIFY(next && position && status);
        next->click();
        QCOMPARE(preview.property("sceneTick").toInt(), 1);
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        const auto first = preview.renderedFrame();
        QVERIFY(!first.isNull());
        int visible = 0;
        for (int y = 0; y < first.height(); ++y)
            for (int x = 0; x < first.width(); ++x)
                visible += first.pixelColor(x, y).alpha() > 0;
        QVERIFY(visible > 10);
        position->setValue(4);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 4);
        QVERIFY(preview.renderedFrame() != first);
        position->setValue(1);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 1);
        QCOMPARE(preview.renderedFrame(), first);
        preview.repaint();
        QCoreApplication::processEvents();
        QCOMPARE(preview.renderedFrame(), first);
        QVERIFY(preview.grab().save("scene-playback-preview.png"));
        position->setValue(10);
        QTRY_VERIFY(preview.property("sceneStopped").toBool());
        QVERIFY(status->text().contains("gameplay launch context"));
        QVERIFY(!next->isEnabled());
        position->setValue(1);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 1);
        QCOMPARE(preview.renderedFrame(), first);
        QVERIFY(next->isEnabled());
        position->setValue(10000);
        preview.findChild<QPushButton *>("sceneRestart")->click();
        QCoreApplication::processEvents();
        QCOMPARE(preview.property("sceneTick").toInt(), 0);
        words[5] = 30000; // Reloaded project edits change the command's Actor X.
        scenes.replace(id, encode());
        preview.selectScene(scenes, id, 0);
        next->click();
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        QVERIFY(preview.renderedFrame() != first);
    }
    void projectedActorPreview() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        ActorPoseEditor editor(Rom::open(path));
        editor.resize(1250, 950);
        editor.show();
        auto banks = editor.findChild<QComboBox *>("actorBank"),
             poses = editor.findChild<QComboBox *>("actorPose");
        auto projected = editor.findChild<QCheckBox *>("actorProjected"),
             shadow = editor.findChild<QCheckBox *>("actorPlacementShadow");
        auto x = editor.findChild<QDoubleSpinBox *>("actorPlacement0"),
             yaw = editor.findChild<QDoubleSpinBox *>("actorCameraYaw");
        auto position = editor.findChild<QSpinBox *>("actorPosition");
        auto layers = editor.findChild<QListWidget *>("actorLayers");
        QVERIFY(banks && poses && projected && shadow && x && yaw && position && layers);
        banks->setCurrentIndex(banks->findData(30));
        poses->setCurrentIndex(poses->findData(12));
        position->setValue(1);
        projected->setChecked(true);
        auto image = [&] { return editor.property("placementFrame").value<QImage>(); };
        auto center = [](const QImage &frame) {
            double sum = 0, weight = 0;
            for (int y = 0; y < frame.height(); ++y)
                for (int x = 0; x < frame.width(); ++x) {
                    const auto a = frame.pixelColor(x, y).alpha();
                    sum += x * a;
                    weight += a;
                }
            return weight > 0 ? sum / weight : -1.;
        };
        QVERIFY(!image().isNull());
        QVERIFY(center(image()) >= 0);
        QVERIFY(layers->count() > 1);
        const auto first = image();
        auto material = editor.findChild<QComboBox *>("actorMaterialMode");
        auto red = editor.findChild<QSpinBox *>("actorColor0");
        auto environment = editor.findChild<QSpinBox *>("actorEnvironment2");
        QVERIFY(material && red && environment);
        red->setValue(40);
        QVERIFY(image() != first);
        const auto multiplied = image();
        material->setCurrentIndex(1);
        QVERIFY(image() != multiplied);
        material->setCurrentIndex(2);
        const auto darkMaterial = image();
        environment->setValue(255);
        QVERIFY(image() != darkMaterial);
        material->setCurrentIndex(3);
        const auto coloredMaterial = image();
        position->setValue(8);
        position->setValue(1);
        QCOMPARE(image(), coloredMaterial);
        QVERIFY(editor.grab().save("actor-material-preview.png"));
        material->setCurrentIndex(0);
        red->setValue(255);
        environment->setValue(0);
        QCOMPARE(image(), first);
        const auto initialCenter = center(first);
        x->setValue(30);
        QVERIFY(center(image()) > initialCenter + 10);
        x->setValue(0);
        QCOMPARE(image(), first);
        yaw->setValue(35);
        QVERIFY(image() != first);
        yaw->setValue(0);
        QCOMPARE(image(), first);
        position->setValue(8);
        position->setValue(1);
        QCOMPARE(image(), first);
        const int withShadow = layers->count();
        shadow->setChecked(false);
        QCOMPARE(layers->count() * 2, withShadow);
        QVERIFY(image() != first);
        shadow->setChecked(true);
        QCOMPARE(image(), first);
        layers->setCurrentRow(0);
        auto view = editor.findChild<QGraphicsView *>("actorCanvas");
        QCOMPARE(view->scene()->selectedItems().size(), 1);
        auto selectedLayer = view->scene()->selectedItems().first();
        layers->item(0)->setCheckState(Qt::Unchecked);
        QVERIFY(!selectedLayer->isVisible());
        layers->item(0)->setCheckState(Qt::Checked);
        QTest::qWait(25);
        QVERIFY(editor.grab().save("projected-actor-preview.png"));
        auto placementMode = editor.findChild<QComboBox *>("actorPlacementMode");
        QVERIFY(placementMode);
        placementMode->setCurrentIndex(1);
        const auto world = image();
        QVERIFY(!world.isNull());
        QVERIFY(center(world) >= 0);
        QVERIFY(world != first);
        QCOMPARE(layers->count(), withShadow * 3 / 2);
        x->setValue(30);
        QVERIFY(center(image()) > center(world) + 10);
        x->setValue(0);
        QCOMPARE(image(), world);
        position->setValue(8);
        position->setValue(1);
        QCOMPARE(image(), world);
        QVERIFY(editor.grab().save("world-actor-preview.png"));
        placementMode->setCurrentIndex(0);
        QCOMPARE(image(), first);
        projected->setChecked(false);
        QVERIFY(layers->count() > 0);
    }
    void equipmentBodyPreview() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        ActorPoseEditor editor(Rom::open(path));
        editor.resize(1250,950); editor.show();
        auto kind = editor.findChild<QComboBox *>("actorSourceKind");
        auto bank = editor.findChild<QComboBox *>("actorBank");
        auto equipment = editor.findChild<QComboBox *>("actorEquipment");
        auto projected = editor.findChild<QCheckBox *>("actorProjected");
        auto view = editor.findChild<QGraphicsView *>("actorCanvas");
        auto position = editor.findChild<QSpinBox *>("actorPosition");
        QVERIFY(kind && bank && equipment && projected && view && position);
        QVERIFY(!equipment->isVisible());
        kind->setCurrentIndex(1); bank->setCurrentIndex(bank->findData(2));
        QVERIFY(equipment->isVisible());
        QCOMPARE(equipment->currentData().toInt(),-1);
        QVERIFY(!editor.findChild<QSpinBox *>("actorVariant")->isEnabled());
        auto flat = [&] {
            QImage image(320,240,QImage::Format_RGBA8888); image.fill(Qt::transparent);
            QPainter painter(&image); view->scene()->render(&painter); return image;
        };
        position->setValue(1);
        const auto defaultImage = flat();
        equipment->setCurrentIndex(equipment->findData(1));
        const auto first = flat(); const auto cursor = editor.property("poseCursor");
        int changedItem = -1;
        for (int item : {2,6,11,20,50,100}) {
            equipment->setCurrentIndex(equipment->findData(item));
            if (flat() != first) { changedItem = item; break; }
        }
        QVERIFY(changedItem >= 0);
        QCOMPARE(position->value(),1); QCOMPARE(editor.property("poseCursor"),cursor);
        equipment->setCurrentIndex(equipment->findData(1)); QCOMPARE(flat(),first);
        equipment->setCurrentIndex(0); QCOMPARE(flat(),defaultImage);
        equipment->setCurrentIndex(equipment->findData(1));
        projected->setChecked(true);
        const auto projectedFirst = editor.property("placementFrame").value<QImage>();
        QVERIFY(!projectedFirst.isNull());
        equipment->setCurrentIndex(equipment->findData(changedItem));
        QVERIFY(editor.property("placementFrame").value<QImage>() != projectedFirst);
        equipment->setCurrentIndex(equipment->findData(1));
        QCOMPARE(editor.property("placementFrame").value<QImage>(),projectedFirst);
        position->setValue(18); position->setValue(1);
        QCOMPARE(editor.property("placementFrame").value<QImage>(),projectedFirst);
        QTest::qWait(25);
        QVERIFY(editor.grab().save("body-equipment-preview.png"));
        kind->setCurrentIndex(0); QVERIFY(!equipment->isVisible());
        QVERIFY(editor.findChild<QSpinBox *>("actorVariant")->isEnabled());
    }
    void actorPosePreview() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        ActorPoseEditor editor(rom);
        editor.resize(1250, 900);
        editor.show();
        auto banks = editor.findChild<QComboBox *>("actorBank"),
             poses = editor.findChild<QComboBox *>("actorPose");
        auto position = editor.findChild<QSpinBox *>("actorPosition"),
             variant = editor.findChild<QSpinBox *>("actorVariant");
        auto next = editor.findChild<QPushButton *>("actorStep"),
             reset = editor.findChild<QPushButton *>("actorRestart");
        auto layers = editor.findChild<QListWidget *>("actorLayers");
        auto view = editor.findChild<QGraphicsView *>("actorCanvas");
        QVERIFY(banks && poses && position && variant && next && reset && layers && view);
        QCOMPARE(banks->count(), 68);
        banks->setCurrentIndex(banks->findData(30));
        poses->setCurrentIndex(poses->findData(12));
        ActorPosePlayer reference(rom, ActorArt::load(rom, 30), 12);
        for (int i = 1; i <= 24; ++i) {
            QCOMPARE(reference.advance(), LORDLY_POSE_OK);
            next->click();
            QCOMPARE(position->value(), i);
            QCOMPARE(editor.property("frameToken").toInt(), reference.actor().frame);
            QCOMPARE(editor.property("poseCursor").toInt(), reference.actor().cursor);
            QCOMPARE(editor.property("poseDelay").toInt(), reference.actor().delay);
        }
        QVERIFY(layers->count() > 0);
        const auto finalFrame = editor.property("frameToken");
        reset->click();
        QCOMPARE(position->value(), 0);
        position->setValue(24);
        QCOMPARE(editor.property("frameToken"), finalFrame);
        position->setValue(1);
        QCOMPARE(position->value(), 1);
        variant->setValue(255);
        QCOMPARE(position->value(), 0);
        next->click();
        QVERIFY(layers->item(0)->toolTip().contains("fallback"));
        layers->setCurrentRow(0);
        QCOMPARE(view->scene()->selectedItems().size(), 1);
        auto selected = view->scene()->selectedItems().first();
        layers->item(0)->setCheckState(Qt::Unchecked);
        QVERIFY(!selected->isVisible());
        layers->item(0)->setCheckState(Qt::Checked);
        QVERIFY(selected->isVisible());
        variant->setValue(0);
        position->setValue(3);
        QTest::qWait(25);
        QVERIFY(editor.grab().save("native-actor-preview.png"));
        auto kind = editor.findChild<QComboBox *>("actorSourceKind"),
             owner = editor.findChild<QComboBox *>("actorOwner"),
             flags = editor.findChild<QComboBox *>("actorBodyFlags");
        QVERIFY(kind && owner && flags);
        kind->setCurrentIndex(1);
        banks->setCurrentIndex(banks->findData(83));
        flags->setCurrentIndex(0);
        poses->setCurrentIndex(poses->findData(0));
        const auto body = ActorArt::loadBody(rom, 83, 83, 0, 0);
        QCOMPARE(variant->value(), body.bodyVariant());
        ActorPosePlayer bodyReference(rom, body, 0);
        bodyReference.setLinkedObjectKnownAbsent(true);
        for (int i = 0; i < 8; ++i) {
            QCOMPARE(bodyReference.advance(), LORDLY_POSE_OK);
            next->click();
            QCOMPARE(editor.property("frameToken").toInt(), bodyReference.actor().frame);
            QCOMPARE(editor.property("poseCursor").toInt(), bodyReference.actor().cursor);
        }
        QVERIFY(layers->count() > 0);
        QTest::qWait(25);
        QVERIFY(editor.grab().save("native-body-preview.png"));
        layers->setCurrentRow(0);
        banks->setCurrentIndex(banks->findData(0));
        QCOMPARE(layers->count(), 0);
        QCOMPARE(view->scene()->items().size(), 0);
        QVERIFY(editor.findChild<QLabel *>("actorStatus")->text().contains("absent descriptor"));
        banks->setCurrentIndex(banks->findData(83));
        owner->setCurrentIndex(owner->findData(88));
        flags->setCurrentIndex(1);
        const auto alternate = ActorArt::loadBody(rom, 83, 88, 0, 1);
        QCOMPARE(variant->value(), alternate.bodyVariant());
        QCOMPARE(poses->count(), alternate.poses().size());
        kind->setCurrentIndex(0);
        QCOMPARE(banks->count(), 68);
    }
    void spritePosePreview() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        ActorPoseEditor editor(rom);
        editor.resize(1250, 900);
        editor.show();
        auto kind = editor.findChild<QComboBox *>("actorSourceKind"),
             banks = editor.findChild<QComboBox *>("actorBank"),
             poses = editor.findChild<QComboBox *>("actorPose");
        auto position = editor.findChild<QSpinBox *>("actorPosition");
        auto next = editor.findChild<QPushButton *>("actorStep");
        auto sepia = editor.findChild<QCheckBox *>("actorSepia");
        auto view = editor.findChild<QGraphicsView *>("actorCanvas");
        QVERIFY(kind && banks && poses && position && next && sepia && view);
        // This retail program contains opcode 12. It moves a 3D actor but is
        // deliberately a no-op in the 2D sprite sequencer.
        kind->setCurrentIndex(2);
        banks->setCurrentIndex(banks->findData(3));
        poses->setCurrentIndex(poses->findData(59));
        const auto art = ActorArt::load(rom, 3);
        SpritePosePlayer reference(rom, art, 59);
        for (int i = 1; i <= 16; ++i) {
            QCOMPARE(reference.advance(), LORDLY_POSE_OK);
            next->click();
            QCOMPARE(position->value(), i);
            QCOMPARE(editor.property("frameToken").toInt(), reference.sprite().frame);
            QCOMPARE(editor.property("poseCursor").toInt(), reference.sprite().cursor);
            QCOMPARE(editor.property("poseX").toDouble(), reference.sprite().x);
            QCOMPARE(editor.property("poseY").toDouble(), reference.sprite().y);
        }
        const auto cursor = editor.property("poseCursor");
        position->setValue(0);
        position->setValue(16);
        QCOMPARE(editor.property("poseCursor"), cursor);
        sepia->setChecked(true);
        const auto frame = art.frame(rom, reference.sprite().frame, 0, true);
        QCOMPARE(view->scene()->items().size(), frame.size());
        for (auto raw : view->scene()->items()) {
            auto item = qgraphicsitem_cast<QGraphicsPixmapItem *>(raw);
            QVERIFY(item);
            const int layer = int(item->zValue());
            const auto &expected = frame[layer];
            const auto &r = expected.pixels.image;
            const QImage expectedImage(reinterpret_cast<const uchar *>(r.rgba.constData()), r.width,
                                       r.height, r.width * 4, QImage::Format_RGBA8888);
            // QPixmap can premultiply alpha; compare the same Qt display format.
            QCOMPARE(item->pixmap().toImage(), QPixmap::fromImage(expectedImage).toImage());
            QCOMPARE(item->opacity(), reference.sprite().color[layer] / 255.);
        }
        banks->setCurrentIndex(banks->findData(30));
        poses->setCurrentIndex(poses->findData(12));
        position->setValue(3);
        QTest::qWait(25);
        QVERIFY(editor.grab().save("native-scene-sprite-preview.png"));
        kind->setCurrentIndex(0);
        QCOMPARE(position->value(), 0);
        sepia->setChecked(false);
        next->click();
        QCOMPARE(position->value(), 1);
    }
    void sceneEventBindingControls() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadSceneEvents();
        auto scenes = *project.scenePrograms();
        const auto added = scenes.createScene("A new journey", scenes.scenes().first(), false);
        scenes.renameScene(added, "Journey renamed");
        project.setScenePrograms(scenes.json());
        QUndoStack undo;
        int changed = 0;
        SceneEventEditor editor(&project, &undo, [&] { ++changed; });
        editor.resize(1100, 750);
        editor.show();
        auto table = editor.findChild<QTableWidget *>("sceneEventSites");
        auto target = editor.findChild<QComboBox *>("sceneEventTarget");
        auto assign = editor.findChild<QPushButton *>("sceneEventAssign");
        auto restore = editor.findChild<QPushButton *>("sceneEventRestore");
        auto search = editor.findChild<QLineEdit *>("sceneEventSearch");
        QVERIFY(table && target && assign && restore && search);
        QCOMPARE(table->rowCount(), 1998);
        const QString site = "event-director:1:b0032";
        int row = -1;
        for (int i = 0; i < table->rowCount(); ++i)
            if (table->item(i, 0)->text() == site)
                row = i;
        QVERIFY(row >= 0);
        table->setCurrentCell(row, 0);
        QVERIFY(editor.findChild<QLabel *>("sceneEventStatus")->text().contains("Shared"));
        const auto selector = scenes.resource(added).selectors.first();
        const auto index = target->findData(selector);
        QVERIFY(index >= 0);
        QVERIFY(target->itemText(index).contains("Journey renamed"));
        target->setCurrentIndex(index);
        assign->click();
        QCOMPARE(changed, 1);
        QCOMPARE(project.sceneEvents()->target(site), selector);
        QVERIFY(table->item(row, 2)->text().contains("Journey renamed"));
        QVERIFY(restore->isEnabled());
        undo.undo();
        QCOMPARE(project.sceneEvents()->target(site), 1);
        undo.redo();
        QCOMPARE(project.sceneEvents()->target(site), selector);
        auto removed = *project.scenePrograms();
        removed.removeScene(added);
        QVERIFY_THROWS_EXCEPTION(Error, project.setScenePrograms(removed.json()));
        const auto historyCount = undo.count();
        QVERIFY_THROWS_EXCEPTION(
            Error, pushSceneProgramChange(&project, &undo, "Remove bound scene", removed));
        QCOMPARE(undo.count(), historyCount);
        target->setEditText("not a scene");
        assign->click();
        QCOMPARE(project.sceneEvents()->target(site), selector);
        QCOMPARE(changed, 1);
        search->setText(site);
        QVERIFY(!table->isRowHidden(row));
        int visible = 0;
        for (int i = 0; i < table->rowCount(); ++i)
            visible += !table->isRowHidden(i);
        QCOMPARE(visible, 1);
        restore->click();
        QCOMPARE(project.sceneEvents()->target(site), 1);
        QVERIFY(!restore->isEnabled());
        undo.undo();
        QCOMPARE(project.sceneEvents()->target(site), selector);
        QVERIFY(!table->isRowHidden(row));
        QTest::qWait(30);
        QVERIFY(editor.grab().save("native-scene-events.png"));
    }
    void sceneDraftSurvivesEventUndo() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        QUndoStack undo;
        CutsceneEditor editor(project.rom(), nullptr, &project, &undo, [] {});
        auto list = editor.findChild<QListWidget *>("cutsceneScenes");
        QTRY_COMPARE_WITH_TIMEOUT(list->count(), 1498, 10000);
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->data(Qt::UserRole).toUInt() == 0x019aa5b6) {
                list->setCurrentRow(i);
                break;
            }
        editor.findChild<QTableWidget *>("cutsceneCommands")->setCurrentCell(0, 0);
        auto form = editor.findChild<SceneCommandEditor *>();
        auto field = form->findChild<QLineEdit *>("sceneOperand0");
        QVERIFY(field);
        field->setText("unfinished");
        QVERIFY(form->property("invalidEdits").toBool());
        auto target = editor.findChild<QComboBox *>("sceneEventTarget");
        target->setCurrentIndex(target->currentIndex() == 0 ? 1 : 0);
        editor.findChild<QPushButton *>("sceneEventAssign")->click();
        QCOMPARE(field->text(), QString("unfinished"));
        QVERIFY(form->property("invalidEdits").toBool());
        QVERIFY(undo.canUndo());
        undo.undo();
        QCOMPARE(field->text(), QString("unfinished"));
        QVERIFY(form->property("invalidEdits").toBool());
        QVERIFY(!editor.findChild<QPushButton *>("sceneWipe")->isEnabled());
        form->findChild<QPushButton *>("sceneCommandDiscard")->click();
        QVERIFY(!form->property("invalidEdits").toBool());
    }
    void wipeSceneControls() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        QUndoStack undo;
        CutsceneEditor editor(project.rom(), nullptr, &project, &undo, [] {});
        editor.resize(1300, 950);
        editor.show();
        auto list = editor.findChild<QListWidget *>("cutsceneScenes");
        auto wipe = editor.findChild<QPushButton *>("sceneWipe");
        QTRY_COMPARE_WITH_TIMEOUT(list->count(), 1498, 10000);
        QVERIFY(wipe);
        const quint32 key = 0x019aa5b6;
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->data(Qt::UserRole).toUInt() == key) {
                list->setCurrentRow(i);
                break;
            }
        QCOMPARE(list->currentItem()->data(Qt::UserRole).toUInt(), key);
        const auto before = project.scenePrograms()->program(key);
        const auto expected = wipeSceneProgram(before).source;
        QVERIFY(wipe->isEnabled());
        wipe->click();
        QCOMPARE(project.scenePrograms()->program(key).source, expected);
        QVERIFY(!wipe->isEnabled());
        undo.undo();
        QCOMPARE(project.scenePrograms()->program(key).source, before.source);
        QVERIFY(wipe->isEnabled());
        auto commands = editor.findChild<QTableWidget *>("cutsceneCommands");
        commands->setCurrentCell(0, 0);
        auto field =
            editor.findChild<SceneCommandEditor *>()->findChild<QLineEdit *>("sceneOperand0");
        QVERIFY(field);
        field->setText("invalid");
        QVERIFY(!wipe->isEnabled());
        field->setText(QString::number(before.commands[0].operands[0]));
        QVERIFY(wipe->isEnabled());
        undo.redo();
        QCOMPARE(project.scenePrograms()->program(key).source, expected);
    }
    void exportedSceneRemovalControls() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto programs = ScenePrograms::load(rom);
        const auto authored =
            programs.createScene("Remove after reopening", programs.scenes().first(), false);
        const auto image = SceneProgramResources::rebuild(rom, {}, programs.authoredPlan());
        auto project = Project::fromRom(Rom::fromBytes(image));
        QUndoStack undo;
        CutsceneEditor editor(project.rom(), nullptr, &project, &undo, [] {});
        editor.resize(1300, 950);
        editor.show();
        auto list = editor.findChild<QListWidget *>("cutsceneScenes");
        auto remove = editor.findChild<QPushButton *>("sceneRemove");
        QTRY_COMPARE_WITH_TIMEOUT(list->count(), 1499, 10000);
        int selected = -1;
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->data(Qt::UserRole + 1).toString() == authored.authoredId)
                selected = i;
        QVERIFY(selected >= 0);
        list->setCurrentRow(selected);
        QVERIFY(remove->isEnabled());
        remove->click();
        QCOMPARE(list->count(), 1498);
        QVERIFY(project.scenePrograms()->authoredScenes().isEmpty());
        QCOMPARE(project.scenePrograms()->changedPrograms(), 1);
        undo.undo();
        QCOMPARE(list->count(), 1499);
        QCOMPARE(project.scenePrograms()->program(authored).source,
                 programs.program(authored).source);
        QVERIFY(!project.scenePrograms()->modified());
        undo.redo();
        QCOMPARE(list->count(), 1498);
        QVERIFY(project.scenePrograms()->modified());
    }
    void authoredSceneCreationControls() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        QUndoStack undo;
        CutsceneEditor editor(project.rom(), nullptr, &project, &undo, [] {});
        editor.resize(1300, 950);
        editor.show();
        auto scenes = editor.findChild<QListWidget *>("cutsceneScenes");
        QTRY_COMPARE_WITH_TIMEOUT(scenes->count(), 1498, 10000);
        auto create = editor.findChild<QPushButton *>("sceneCreate");
        auto duplicate = editor.findChild<QPushButton *>("sceneDuplicate");
        auto rename = editor.findChild<QPushButton *>("sceneRename");
        auto remove = editor.findChild<QPushButton *>("sceneRemove");
        QVERIFY(create && duplicate && rename && remove);
        QVERIFY(create->isEnabled());
        bool sawDialog = false;
        QTimer::singleShot(0, &editor, [&] {
            if (auto dialog = editor.findChild<QDialog *>("sceneCreateDialog")) {
                auto name = dialog->findChild<QLineEdit *>("newSceneName");
                auto choices = dialog->findChild<QComboBox *>("newSceneTemplate");
                sawDialog = name && choices && choices->count() > 0;
                if (name)
                    name->setText("A new adventure");
                dialog->accept();
            }
        });
        create->click();
        QVERIFY(sawDialog);
        QCOMPARE(project.scenePrograms()->authoredScenes().size(), 1);
        QCOMPARE(scenes->count(), 1499);
        const auto first = project.scenePrograms()->authoredScenes().first();
        QCOMPARE(scenes->currentItem()->data(Qt::UserRole + 1).toString(), first.id);
        QVERIFY(scenes->currentItem()->text().contains("A new adventure"));
        auto commands = editor.findChild<QTableWidget *>("cutsceneCommands");
        commands->setCurrentCell(2, 0);
        auto form = editor.findChild<SceneCommandEditor *>();
        auto field = form->findChild<QLineEdit *>("sceneOperand0");
        QVERIFY(field);
        field->setText("2222");
        QCOMPARE(
            project.scenePrograms()->program(SceneId::authored(first.id)).commands[2].operands[0],
            quint32(2222));
        undo.undo();
        QCOMPARE(project.scenePrograms()->program(SceneId::authored(first.id)).source,
                 first.current.source);
        undo.redo();
        QTimer::singleShot(0, &editor, [&] {
            if (auto dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget())) {
                dialog->setTextValue("Adventure renamed");
                dialog->accept();
            }
        });
        rename->click();
        QCOMPARE(project.scenePrograms()->authoredScenes()[first.id].name,
                 QString("Adventure renamed"));
        undo.undo();
        QCOMPARE(project.scenePrograms()->authoredScenes()[first.id].name, first.name);
        undo.redo();
        sawDialog = false;
        QTimer::singleShot(0, &editor, [&] {
            if (auto dialog = editor.findChild<QDialog *>("sceneCreateDialog")) {
                auto name = dialog->findChild<QLineEdit *>("newSceneName");
                sawDialog = name != nullptr;
                if (name)
                    name->setText("Adventure copy");
                dialog->accept();
            }
        });
        duplicate->click();
        QVERIFY(sawDialog);
        QCOMPARE(project.scenePrograms()->authoredScenes().size(), 2);
        const auto secondId = scenes->currentItem()->data(Qt::UserRole + 1).toString();
        QVERIFY(secondId != first.id);
        QCOMPARE(project.scenePrograms()->program(SceneId::authored(secondId)).source,
                 project.scenePrograms()->program(SceneId::authored(first.id)).source);
        remove->click();
        QCOMPARE(project.scenePrograms()->authoredScenes().size(), 1);
        QCOMPARE(scenes->count(), 1499);
        undo.undo();
        QCOMPARE(project.scenePrograms()->authoredScenes().size(), 2);
        undo.redo();
        QCOMPARE(project.scenePrograms()->authoredScenes().size(), 1);
        for (int i = 0; i < scenes->count(); ++i)
            if (scenes->item(i)->data(Qt::UserRole + 1).toString() == first.id) {
                scenes->setCurrentRow(i);
                break;
            }
        editor.findChild<QTabWidget *>("cutsceneDetailPages")->setCurrentIndex(1);
        QVERIFY(editor.grab().save("native-new-scene.png"));
    }
    void sceneCommandEditingAndPreview() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        QUndoStack undo;
        CutsceneEditor editor(project.rom(), nullptr, &project, &undo, [] {});
        editor.resize(1300, 950);
        editor.show();
        auto scenes = editor.findChild<QListWidget *>("cutsceneScenes");
        auto commands = editor.findChild<QTableWidget *>("cutsceneCommands");
        auto form = editor.findChild<SceneCommandEditor *>();
        QVERIFY(scenes && commands && form);
        QTRY_VERIFY_WITH_TIMEOUT(scenes->count() > 0, 10000);
        quint32 key = 0;
        int camera = -1, group = -1;
        for (const auto &scene : project.scenePrograms()->original().entries()) {
            if (scene.program.resourceClass() != 5)
                continue;
            for (int i = 0; i < scene.program.commands.size(); ++i) {
                if (scene.program.commands[i].opcode == 0x35)
                    camera = i;
                if (scene.program.commands[i].opcode == 0x80000006)
                    group = i;
            }
            if (camera >= 0 && group >= 0) {
                key = scene.key;
                break;
            }
            camera = group = -1;
        }
        QVERIFY(key != 0);
        for (int i = 0; i < scenes->count(); ++i)
            if (scenes->item(i)->data(Qt::UserRole).toUInt() == key) {
                scenes->setCurrentRow(i);
                break;
            }
        const auto original = project.scenePrograms()->program(key);
        commands->setCurrentCell(camera, 0);
        auto field = form->findChild<QLineEdit *>("sceneOperand0");
        QVERIFY(field);
        QCOMPARE(field->accessibleName(), QString("camera_target_x_milliunits"));
        field->setText("-1200");
        QCOMPARE(project.scenePrograms()->program(key).commands[camera].operands[0],
                 quint32(qint32(-1200)));
        QVERIFY(commands->item(camera, 2)->text().contains("FFFFFB50"));
        undo.undo();
        QCOMPARE(project.scenePrograms()->program(key).source, original.source);
        undo.redo();
        field = form->findChild<QLineEdit *>("sceneOperand0");
        QCOMPARE(field->text(), QString("-1200"));
        field->setText("0x1234");
        QCOMPARE(project.scenePrograms()->program(key).commands[camera].operands[0],
                 quint32(0x1234));
        field->setText("0x");
        auto second = form->findChild<QLineEdit *>("sceneOperand1");
        second->setText("4321");
        QVERIFY(form->property("invalidEdits").toBool());
        field->setText("0x1234");
        QCOMPARE(project.scenePrograms()->program(key).commands[camera].operands[1], quint32(4321));
        field->setText("0x");
        QVERIFY(form->property("invalidEdits").toBool());
        QVERIFY(!commands->isEnabled() && !scenes->isEnabled());
        auto catalogTabs = editor.findChild<QTabWidget *>("cutsceneCatalogTabs");
        catalogTabs->setCurrentIndex(1);
        catalogTabs->setCurrentIndex(0);
        QVERIFY(!editor.findChild<QPushButton *>("sceneCreate")->isEnabled());
        QVERIFY(!editor.findChild<QPushButton *>("sceneDuplicate")->isEnabled());
        form->findChild<QPushButton *>("sceneCommandDiscard")->click();
        QVERIFY(!form->property("invalidEdits").toBool());
        QVERIFY(commands->isEnabled() && scenes->isEnabled());
        const auto before = project.scenePrograms()->program(key).source;
        auto kind = form->findChild<QComboBox *>("sceneCommandKind");
        kind->setCurrentIndex(kind->findData(quint32(0x35)));
        form->findChild<QPushButton *>("sceneCommandInsert")->click();
        QCOMPARE(commands->rowCount(), original.commands.size() + 1);
        form->findChild<QPushButton *>("sceneCommandRemove")->click();
        QCOMPARE(project.scenePrograms()->program(key).source, before);
        commands->setCurrentCell(group, 0);
        field = form->findChild<QLineEdit *>("sceneOperand0");
        field->setText("22");
        auto editedScene = project.scenePrograms()->resource(key);
        editedScene.program = project.scenePrograms()->program(key);
        const auto expected = SceneImages::backgrounds(project.rom(), editedScene);
        auto requests = editor.findChild<QComboBox *>("cutsceneRequests");
        QVERIFY(requests);
        QCOMPARE(requests->count(), expected.requests.size());
        QVERIFY(requests->count() > 0);
        QVERIFY(requests->itemText(0).contains("selector 22"));
        QVERIFY(!expected.requests[0].images.isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(editor.property("displayedImage").toString(),
                                  expected.requests[0].images[0], 10000);
        form->findChild<QPushButton *>("sceneCommandRestore")->click();
        QVERIFY(!project.scenePrograms()->modified());
        undo.undo();
        QCOMPARE(project.scenePrograms()->program(key).commands[group].operands[0], quint32(22));
        // Navigation cannot discard an invalid operand.
        commands->setCurrentCell(camera, 0);
        field = form->findChild<QLineEdit *>("sceneOperand0");
        field->setText("4294967296");
        QVERIFY(form->property("invalidEdits").toBool());
        form->findChild<QPushButton *>("sceneCommandDiscard")->click();
        editor.findChild<QTabWidget *>("cutsceneDetailPages")->setCurrentIndex(1);
        QVERIFY(editor.grab().save("native-scene-command-editing.png"));
    }
    void dialogueEditingUndoAndNewEntry() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        QUndoStack undo;
        int notifications = 0;
        DialogueInspector editor(&project, &undo, [&] { ++notifications; });
        editor.resize(1200, 850);
        editor.show();
        auto source = editor.findChild<QPlainTextEdit *>("dialogueSource");
        auto entries = editor.findChild<QListWidget *>("dialogueEntries");
        auto archives = editor.findChild<QComboBox *>("dialogueArchives");
        auto add = editor.findChild<QPushButton *>("dialogueAdd");
        auto restore = editor.findChild<QPushButton *>("dialogueRestore");
        auto discard = editor.findChild<QPushButton *>("dialogueDiscard");
        auto preview = editor.findChild<DialoguePreview *>();
        QVERIFY(source && entries && archives && add && restore && discard && preview);
        QVERIFY(!source->isReadOnly());
        const auto original = project.dialogues()->entry(Dialogues::Table::Primary, 0, 0);
        const QByteArray text = "@-@x@l3@r@S0@A@n@n@=Edited dialogue@s@c";
        source->setPlainText(QString::fromLatin1(text));
        QCOMPARE(project.dialogues()->entry(Dialogues::Table::Primary, 0, 0), text);
        QVERIFY(notifications > 0);
        undo.undo();
        QCOMPARE(source->toPlainText().toLatin1(), original);
        undo.redo();
        QCOMPARE(source->toPlainText().toLatin1(), text);
        source->setPlainText(QString::fromLatin1(text) + QString::fromUtf8("\xe2\x98\x83"));
        QVERIFY(editor.property("invalidEdits").toBool());
        QVERIFY(!entries->isEnabled() && !archives->isEnabled() && !add->isEnabled());
        QCOMPARE(project.dialogues()->entry(Dialogues::Table::Primary, 0, 0), text);
        editor.selectEntry(1, 0);
        QCOMPARE(editor.property("archiveSelector").toInt(), 0);
        discard->click();
        QVERIFY(!editor.property("invalidEdits").toBool());
        QCOMPARE(source->toPlainText().toLatin1(), text);
        source->setPlainText("Bytes " + QString(QChar(92)) + "x81" + QString(2, QChar(92)));
        QCOMPARE(project.dialogues()->entry(Dialogues::Table::Primary, 0, 0),
                 QByteArray("Bytes ") + char(-127) + char(92));
        editor.selectEntry(0, 1);
        editor.selectEntry(0, 0);
        QCOMPARE(source->toPlainText(),
                 "Bytes " + QString(QChar(92)) + "x81" + QString(2, QChar(92)));
        restore->click();
        QCOMPARE(project.dialogues()->entry(Dialogues::Table::Primary, 0, 0), original);
        const auto originalCount = entries->count();
        add->click();
        QCOMPARE(entries->count(), originalCount + 1);
        QCOMPARE(entries->currentRow(), originalCount);
        QVERIFY(!restore->isEnabled());
        source->setPlainText(QString::fromLatin1(text));
        QTRY_COMPARE_WITH_TIMEOUT(preview->property("updateCount").toULongLong(), qulonglong(1),
                                  1000);
        auto step = preview->findChild<QPushButton *>("dialogueStep");
        auto status = preview->findChild<QLabel *>("dialoguePlaybackStatus");
        for (int i = 0; i < 200 && preview->property("dialogueState").toInt() != 4; ++i) {
            step->click();
            QVERIFY2(!status->text().contains("unavailable"), qPrintable(status->text()));
        }
        QCOMPARE(preview->property("dialogueState").toInt(), 4);
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        project.saveFolder(folder.path());
        auto reopened = Project::fromRom(project.rom());
        reopened.applyManifest(Project::readProject(folder.path()));
        QCOMPARE(reopened.dialogues()->json(), project.dialogues()->json());
        undo.undo(); // Added text.
        undo.undo(); // Added entry.
        QCOMPARE(entries->count(), originalCount);
        undo.redo();
        undo.redo();
        editor.selectEntry(0, originalCount);
        QCOMPARE(source->toPlainText().toLatin1(), text);
        editor.findChild<QTabWidget *>("dialogueDetailPages")->setCurrentWidget(preview);
        QVERIFY(editor.grab().save("native-dialogue-editing.png"));
    }
    void dialogueInvalidInputBlocksSave() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        MainWindow window;
        window.show();
        window.loadRom(path);
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<DialogueInspector *>(), 10000);
        auto editor = window.findChild<DialogueInspector *>();
        auto source = editor->findChild<QPlainTextEdit *>("dialogueSource");
        QVERIFY(source && !source->isReadOnly());
        source->setPlainText(QString(QChar(92)) + "x00");
        QVERIFY(editor->property("invalidEdits").toBool());
        QVERIFY(window.isWindowModified());
        int checked = 0;
        for (auto action : window.findChildren<QAction *>()) {
            if (!action->text().startsWith("Save project") &&
                !action->text().startsWith("Save sharing") &&
                !action->text().startsWith("Build and"))
                continue;
            QString message;
            QTimer::singleShot(0, &window, [&] {
                if (auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                    message = box->text();
                    box->accept();
                }
            });
            action->trigger();
            QVERIFY2(message.startsWith("Correct invalid text input"), qPrintable(message));
            ++checked;
        }
        QCOMPARE(checked, 3);
        editor->findChild<QPushButton *>("dialogueDiscard")->click();
        QVERIFY(!window.isWindowModified());
    }
    void dialoguePlaybackControls() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        DialogueInspector inspector(Rom::open(path));
        inspector.resize(1200, 850);
        inspector.show();
        auto pages = inspector.findChild<QTabWidget *>("dialogueDetailPages");
        auto preview = inspector.findChild<DialoguePreview *>();
        QVERIFY(pages && preview);
        pages->setCurrentWidget(preview);
        auto step = preview->findChild<QPushButton *>("dialogueStep");
        auto play = preview->findChild<QPushButton *>("dialoguePlay");
        auto restart = preview->findChild<QPushButton *>("dialogueRestart");
        auto resume = preview->findChild<QPushButton *>("dialogueResume");
        auto canvas = preview->findChild<QLabel *>("dialogueCanvas");
        auto status = preview->findChild<QLabel *>("dialoguePlaybackStatus");
        QVERIFY(step && play && restart && resume && canvas && status);
        QVERIFY2(!status->text().contains("unavailable"), qPrintable(status->text()));
        QCOMPARE(preview->property("updateCount").toULongLong(), qulonglong(1));
        int updates = 0;
        for (; updates < 200 && preview->property("dialogueState").toInt() != 4; ++updates) {
            step->click();
            QVERIFY2(!status->text().contains("unavailable"), qPrintable(status->text()));
        }
        QVERIFY(updates < 200);
        QVERIFY(!canvas->pixmap().isNull());
        QVERIFY(inspector.grab().save("native-dialogue-playback.png"));
        resume->click();
        for (updates = 0; updates < 100 && preview->property("dialogueState").toInt() != -1;
             ++updates) {
            step->click();
            QVERIFY2(!status->text().contains("unavailable"), qPrintable(status->text()));
        }
        QVERIFY(updates < 100);
        QVERIFY(status->text().startsWith("Closed"));
        restart->click();
        QCOMPARE(preview->property("updateCount").toULongLong(), qulonglong(1));
        play->click();
        QTRY_VERIFY_WITH_TIMEOUT(preview->property("updateCount").toULongLong() > 1, 1000);
        play->click();
        QVERIFY(!play->isChecked());
        const auto paused = preview->property("updateCount").toULongLong();
        QTest::qWait(80);
        QCOMPARE(preview->property("updateCount").toULongLong(), paused);
        inspector.selectEntry(9, 0);
        QCOMPARE(preview->property("updateCount").toULongLong(), qulonglong(0));
        QVERIFY(canvas->pixmap().isNull());
        auto name = preview->findChild<QLineEdit *>("dialogueName0");
        name->setFocus();
        name->setText("Preview teardown");
    }
    void dialogueSourceSelection() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        DialogueInspector inspector(rom);
        inspector.resize(1100, 650);
        inspector.show();
        const auto archives = inspector.findChild<QComboBox *>("dialogueArchives");
        const auto entries = inspector.findChild<QListWidget *>("dialogueEntries");
        const auto source = inspector.findChild<QPlainTextEdit *>("dialogueSource");
        const auto search = inspector.findChild<QLineEdit *>("dialogueSearch");
        const auto status = inspector.findChild<QLabel *>("dialogueStatus");
        QVERIFY(archives && entries && source && search && status);
        QCOMPARE(archives->count(), 349);
        QCOMPARE(entries->count(), 7);
        QCOMPARE(inspector.property("archiveSelector").toInt(), 0);
        QCOMPARE(source->toPlainText(),
                 QString("@-@x@l3@r@S0@A@n@n@={T05}You're leaving tomorrow?@s@c"));
        QVERIFY(source->isReadOnly());
        search->setText("You're leaving tomorrow");
        QVERIFY(!entries->item(0)->isHidden());
        QVERIFY(entries->item(1)->isHidden());
        inspector.selectEntry(1, 0);
        QCOMPARE(inspector.property("archiveSelector").toInt(), 1);
        QCOMPARE(inspector.property("entryIndex").toInt(), 0);
        QVERIFY(search->text().isEmpty());
        QVERIFY(source->toPlainText().startsWith("Archbishop Odiron@n"));
        QVERIFY(inspector.grab().save("native-dialogue-source.png"));
        inspector.selectEntry(9, 0);
        QCOMPARE(inspector.property("archiveSelector").toInt(), -1);
        QVERIFY(source->toPlainText().isEmpty());
        QVERIFY(status->text().contains("not present"));
        inspector.selectEntry(0, 99);
        QCOMPARE(inspector.property("entryIndex").toInt(), -1);
        QVERIFY(source->toPlainText().isEmpty());
        inspector.selectEntry(0, 1);
        QCOMPARE(inspector.property("entryIndex").toInt(), 1);
        QCOMPARE(source->toPlainText().toLatin1(), DialogueArchive::load(rom, 0).entry(1));
        inspector.selectEntry(0, 0x8000);
        QCOMPARE(inspector.property("archiveTable").toInt(), 1);
        QCOMPARE(inspector.property("archiveSelector").toInt(), 0);
        QCOMPARE(inspector.property("entryIndex").toInt(), 0);
        QCOMPARE(source->toPlainText().toLatin1(),
                 DialogueArchive::load(rom, 0, DialogueArchive::Table::Alternate).entry(0));
        inspector.selectEntry(0, 0x4000);
        QVERIFY(source->toPlainText().isEmpty());
        QCOMPARE(inspector.property("archiveTable").toInt(), -1);
        QVERIFY(status->text().contains("unresolved"));
        inspector.selectEntry(0, 1);
        QCOMPARE(inspector.property("archiveTable").toInt(), 0);
        QCOMPARE(source->toPlainText().toLatin1(), DialogueArchive::load(rom, 0).entry(1));
    }
    void cutsceneCatalogSelection() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        const auto before = sha256(rom.bytes());
        CutsceneEditor editor(rom);
        editor.resize(1450, 950);
        editor.show();
        auto scenes = editor.findChild<QListWidget *>("cutsceneScenes");
        auto assets = editor.findChild<QListWidget *>("cutsceneAssets");
        auto tabs = editor.findChild<QTabWidget *>("cutsceneCatalogTabs");
        auto commands = editor.findChild<QTableWidget *>("cutsceneCommands");
        auto requests = editor.findChild<QComboBox *>("cutsceneRequests");
        auto frames = editor.findChild<QComboBox *>("cutsceneFrames");
        auto layers = editor.findChild<QListWidget *>("cutsceneLayers");
        auto view = editor.findChild<QGraphicsView *>("cutsceneCanvas");
        QVERIFY(scenes && assets && tabs && commands && requests && frames && layers && view);
        QTRY_COMPARE_WITH_TIMEOUT(scenes->count(), 1498, 10000);
        QCOMPARE(assets->count(), 366);
        auto selectScene = [&](quint32 key) {
            for (int i = 0; i < scenes->count(); ++i)
                if (scenes->item(i)->data(Qt::UserRole).toUInt() == key) {
                    scenes->setCurrentRow(i);
                    return true;
                }
            return false;
        };
        auto selectAsset = [&](const QString &id) {
            for (int i = 0; i < assets->count(); ++i)
                if (assets->item(i)->data(Qt::UserRole).toString() == id) {
                    assets->setCurrentRow(i);
                    return true;
                }
            return false;
        };
        QVERIFY(selectScene(0x1a14970));
        QCOMPARE(commands->rowCount(), 282);
        QVERIFY(requests->currentText().contains("selector 34"));
        QTRY_COMPARE_WITH_TIMEOUT(editor.property("displayedImage").toString(),
                                  "mode2-environment:000D62F6", 10000);
        QVERIFY(selectScene(0x19aa852)); // Selector 2, Graduation Ceremony.
        const auto dialogue = editor.findChild<DialogueInspector *>();
        const auto pages = editor.findChild<QTabWidget *>("cutsceneDetailPages");
        QVERIFY(dialogue && pages);
        const auto catalog = Directors::load(rom);
        const auto program = catalog.entries()[0x19aa852].program;
        bool foundDialogue = false;
        for (int i = 0; i < program.commands.size(); ++i) {
            const auto &cmd = program.commands[i];
            if (cmd.opcode != 0xbf)
                continue;
            commands->cellDoubleClicked(i, 1);
            QCOMPARE(pages->currentWidget(), dialogue);
            QCOMPARE(dialogue->property("archiveSelector").toInt(), int(quint16(cmd.operands[1])));
            QCOMPARE(dialogue->property("entryIndex").toInt(), int(quint16(cmd.operands[2])));
            foundDialogue = true;
            break;
        }
        QVERIFY(foundDialogue);
        bool foundTransform = false;
        const auto transform = editor.findChild<TransformPreview *>();
        QVERIFY(transform);
        for (const auto &resource : catalog.entries()) {
            if (foundTransform)
                break;
            if (resource.program.resourceClass() != 4 && resource.program.resourceClass() != 5)
                continue;
            for (int i = 0; i < resource.program.commands.size(); ++i) {
                if (resource.program.commands[i].opcode != 8)
                    continue;
                QVERIFY(selectScene(resource.key));
                commands->cellDoubleClicked(i, 1);
                QCOMPARE(pages->currentWidget(), transform);
                QVERIFY2(!transform->property("channelRecord").toByteArray().isEmpty(),
                         qPrintable(transform->findChild<QLabel *>("transformStatus")->text()));
                foundTransform = true;
                break;
            }
        }
        QVERIFY(foundTransform);
        bool foundActor = false;
        const auto actorInspector = editor.findChild<ActorPoseEditor *>();
        QVERIFY(actorInspector);
        for (const auto &resource : catalog.entries()) {
            if (foundActor)
                break;
            if (resource.program.resourceClass() != 4 && resource.program.resourceClass() != 5)
                continue;
            for (int i = 0; i < resource.program.commands.size(); ++i) {
                const auto &command = resource.program.commands[i];
                if (command.opcode != 0x14 || command.operands.size() != 9)
                    continue;
                const auto &args = command.operands;
                if (args[1] < 1 || args[1] > 68 || qint32(args[2]) < 0 || args[3] > 9)
                    continue;
                bool inRange = true;
                for (int axis = 0; axis < 3; ++axis)
                    inRange &= qAbs(double(qint32(args[4 + axis])) / 1000.) <= 10000;
                if (!inRange)
                    continue;
                QVERIFY(selectScene(resource.key));
                commands->cellDoubleClicked(i, 1);
                QCOMPARE(pages->currentWidget(), actorInspector);
                QVERIFY2(actorInspector->property("inspectedActorCommand").toInt() == i,
                         qPrintable(actorInspector->findChild<QLabel *>("actorStatus")->text()));
                QCOMPARE(actorInspector->findChild<QComboBox *>("actorBank")->currentData().toInt(),
                         qint32(args[1]));
                for (int axis = 0; axis < 3; ++axis)
                    QCOMPARE(
                        actorInspector
                            ->findChild<QDoubleSpinBox *>(QString("actorPlacement%1").arg(axis))
                            ->value(),
                        double(qint32(args[4 + axis])) / 1000.);
                QCOMPARE(actorInspector->findChild<QSpinBox *>("actorPlacementFacing")->value(),
                         qint32(args[3]));
                QVERIFY(actorInspector->findChild<QCheckBox *>("actorProjected")->isChecked());
                foundActor = true;
                break;
            }
        }
        QVERIFY(foundActor);
        QVERIFY(selectScene(0x19aa852));
        pages->setCurrentIndex(0);
        QTRY_COMPARE_WITH_TIMEOUT(editor.property("displayedImage").toString(),
                                  "mode2-environment:00236B58", 10000);
        QCOMPARE(view->scene()->sceneRect().size(), QSizeF(495, 383));
        QVERIFY(editor.grab().save("cutscene-graduation.png"));
        tabs->setCurrentIndex(1);
        QVERIFY(selectAsset("archive:102"));
        QVERIFY(selectAsset("archive:636"));
        QTRY_COMPARE_WITH_TIMEOUT(editor.property("displayedImage").toString(), "archive:636",
                                  10000);
        QCOMPARE(frames->count(), 12);
        frames->setCurrentIndex(2);
        QCOMPARE(view->scene()->sceneRect().size(), QSizeF(256, 48));
        // Allow the earlier, more expensive compound decode to finish. It must not replace this
        // selection.
        QTest::qWait(200);
        QCOMPARE(editor.property("displayedImage").toString(), "archive:636");
        QVERIFY(selectAsset("archive:102"));
        QTRY_COMPARE_WITH_TIMEOUT(editor.property("displayedImage").toString(), "archive:102",
                                  10000);
        QCOMPARE(layers->count(), 6);
        QCOMPARE(view->scene()->sceneRect(), QRectF(-360, -320, 607, 511));
        layers->setCurrentRow(2);
        QCOMPARE(view->scene()->selectedItems().size(), 1);
        auto chosen = view->scene()->selectedItems().first();
        layers->item(2)->setCheckState(Qt::Unchecked);
        QVERIFY(!chosen->isVisible());
        layers->item(2)->setCheckState(Qt::Checked);
        QVERIFY(chosen->isVisible());
        QVERIFY(editor.grab().save("cutscene-compound.png"));
        auto search = editor.findChild<QLineEdit *>("cutsceneAssetsSearch");
        search->setText("congra");
        int visible = 0;
        for (int i = 0; i < assets->count(); ++i)
            visible += !assets->item(i)->isHidden();
        QCOMPARE(visible, 1);
        QCOMPARE(sha256(rom.bytes()), before);
    }
    void scenarioMapEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            project.loadScenarios();
            // Add a real waypoint so all three editable map object kinds are exercised.
            auto scenarios = *project.scenarios();
            const int waypoint = scenarios.addNode(1, 1, project.classData());
            project.setScenarios(scenarios.json());
            QUndoStack undo;
            ScenarioEditor editor(&project, &undo, [] {});
            editor.resize(1450, 950);
            editor.show();
            auto pages = editor.findChild<QTabWidget *>("scenarioPages");
            pages->setCurrentIndex(3);
            QTest::qWait(20);
            auto objects = editor.findChild<QListWidget *>("mapObjects");
            auto apply = editor.findChild<QPushButton *>("mapApply");
            auto fields = editor.findChild<QPushButton *>("mapEditFields");
            auto x = editor.findChild<QSpinBox *>("mapX"), z = editor.findChild<QSpinBox *>("mapZ");
            auto canvas = editor.findChild<QWidget *>("scenarioMapCanvas");
            QVERIFY(objects && apply && fields && x && z && canvas);
            QVERIFY(objects->count() > project.scenarios()->scenario(1).squads.size());
            const auto original = project.scenarios()->json(),
                       originalWorld = project.world()->json();
            objects->setCurrentRow(0);
            fields->click();
            QCOMPARE(pages->currentIndex(), 0);
            QCOMPARE(editor.findChild<QListWidget *>("scenarioSquads")->currentRow(), 0);
            pages->setCurrentIndex(3);
            objects->setCurrentRow(0);
            x->setValue(128);
            z->setValue(128);
            apply->click();
            const auto centered = project.scenarios()->json();
            QCOMPARE(u8(project.scenarios()->scenario(1).layout.deployments[0], 3), quint8(128));
            QCOMPARE(u8(project.scenarios()->scenario(1).layout.deployments[0], 4), quint8(128));
            undo.undo();
            QCOMPARE(project.scenarios()->json(), original);
            undo.redo();
            QCOMPARE(project.scenarios()->json(), centered);
            const QPoint center(canvas->width() / 2, canvas->height() / 2);
            const int shift = (std::min(canvas->width(), canvas->height()) - 72) / 4;
            QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, center);
            QTest::mouseMove(canvas, center + QPoint(shift, -shift), 20);
            QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier,
                                center + QPoint(shift, -shift));
            const auto moved = project.scenarios()->scenario(1).layout.deployments[0];
            QVERIFY(std::abs(int(u8(moved, 3)) - 192) <= 1);
            QVERIFY(std::abs(int(u8(moved, 4)) - 64) <= 1);
            undo.undo();
            QCOMPARE(project.scenarios()->json(), centered);
            undo.redo();
            int waypointItem = -1, treasureItem = -1, siteItem = -1;
            const int nodeId = u8(project.scenarios()->scenario(1).layout.nodes[waypoint], 0);
            const auto townName =
                project.world()->townName(u16(project.world()->scenario(1).sites, 1));
            for (int i = 0; i < objects->count(); ++i) {
                const auto label = objects->item(i)->text();
                if (label.startsWith(QString("Waypoint %1").arg(nodeId)))
                    waypointItem = i;
                if (label.startsWith("Treasure 1 Ã‚Â·"))
                    treasureItem = i;
                if (label == townName)
                    siteItem = i;
            }
            QVERIFY(waypointItem >= 0 && treasureItem >= 0 && siteItem >= 0);
            objects->setCurrentRow(waypointItem);
            x->setValue(55);
            z->setValue(66);
            apply->click();
            fields->click();
            QCOMPARE(pages->currentIndex(), 1);
            QCOMPARE(editor.findChild<QListWidget *>("scenarioNodes")->currentRow(), waypoint);
            pages->setCurrentIndex(3);
            objects->setCurrentRow(treasureItem);
            x->setValue(255);
            z->setValue(0);
            apply->click();
            auto treasure =
                ScenarioWorld::treasureRecord(project.world()->scenario(1).treasures, 0);
            QCOMPARE(u8(treasure, 1), quint8(255));
            QCOMPARE(u8(treasure, 2), quint8(0));
            undo.undo();
            QCOMPARE(project.world()->json(), originalWorld);
            undo.redo();
            fields->click();
            QCOMPARE(pages->currentIndex(), 2);
            QCOMPARE(editor.findChild<QTabWidget *>("worldPages")->currentIndex(), 1);
            QCOMPARE(editor.findChild<QListWidget *>("worldTreasures")->currentRow(), 0);
            pages->setCurrentIndex(3);
            objects->setCurrentRow(siteItem);
            QVERIFY(!apply->isEnabled());
            fields->click();
            QCOMPARE(editor.findChild<QTabWidget *>("worldPages")->currentIndex(), 0);
            pages->setCurrentIndex(3);
            objects->setCurrentRow(0);
            QVERIFY(editor.grab().save("scenario-map.png"));
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(rom);
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.scenarios()->json(), project.scenarios()->json());
            QCOMPARE(restored.world()->json(), project.world()->json());
            const auto result = rebuild(restored, tools, work.path());
            const auto root = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
            QVERIFY(QDir().mkpath(root));
            QTemporaryDir output(QDir(root).filePath("native-map-edit-XXXXXX"));
            QVERIFY(output.isValid());
            const auto candidate = output.filePath(QFileInfo(path).fileName());
            exportRom(restored, result, candidate);
            auto reopened = Project::fromRom(Rom::open(candidate));
            reopened.loadScenarios();
            reopened.loadWorld();
            QCOMPARE(reopened.scenarios()->scenario(1).layout.deployments[0].mid(3, 2),
                     moved.mid(3, 2));
            QCOMPARE(reopened.scenarios()->scenario(1).layout.nodes[waypoint].mid(4, 2),
                     QByteArray::fromHex("3742"));
            QCOMPARE(ScenarioWorld::treasureRecord(reopened.world()->scenario(1).treasures, 0),
                     treasure);
            auto checksum = result.image;
            Rom::updateChecksum(checksum);
            QCOMPARE(checksum, result.image);
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
            output.setAutoRemove(false);
            qInfo().noquote() << "Map edit export:" << candidate;
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void scenarioWorldEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadScenarios();
        QUndoStack undo;
        ScenarioEditor editor(&project, &undo, [] {});
        editor.resize(1450, 950);
        editor.show();
        editor.findChild<QTabWidget *>("scenarioPages")->setCurrentIndex(2);
        auto button = [&](const char *name) { return editor.findChild<QPushButton *>(name); };
        auto spin = [&](const char *name) { return editor.findChild<QSpinBox *>(name); };
        auto missions = editor.findChild<QListWidget *>("scenarioList");
        auto sites = editor.findChild<QListWidget *>("worldSites");
        auto treasures = editor.findChild<QListWidget *>("worldTreasures");
        const auto base = project.world()->json();
        for (int i = 0; i < missions->count(); ++i) {
            missions->setCurrentRow(i);
            for (int row = 0; row < sites->count(); ++row) {
                sites->setCurrentRow(row);
                button("worldSiteApply")->click();
                QCOMPARE(project.world()->json(), base);
            }
            for (int row = 0; row < treasures->count(); ++row) {
                treasures->setCurrentRow(row);
                button("worldTreasureApply")->click();
                QCOMPARE(project.world()->json(), base);
            }
        }
        QCOMPARE(undo.count(), 0);
        for (int i = 0; i < missions->count(); ++i)
            if (missions->item(i)->data(Qt::UserRole).toInt() == 52)
                missions->setCurrentRow(i);
        sites->setCurrentRow(0);
        const int id = sites->currentItem()->data(Qt::UserRole).toInt();
        spin("worldPopulation")->setValue(4567);
        auto allegiance = editor.findChild<QComboBox *>("worldAllegiance");
        allegiance->setCurrentIndex(allegiance->findData(2));
        button("worldSiteApply")->click();
        QCOMPARE(u16(project.world()->town(id), 22), quint16(4567));
        QCOMPARE(u16(project.world()->scenario(52).sites, 3), quint16(4));
        undo.undo();
        QCOMPARE(project.world()->json(), base);
        undo.redo();
        auto objective = editor.findChild<QCheckBox *>("worldObjective");
        objective->setChecked(true);
        button("worldSiteApply")->click();
        QCOMPARE(u8(project.world()->town(id), 24), quint8(255));
        QVERIFY(!spin("worldMorale")->isEnabled());
        editor.grab().save("scenario-sites.png");
        editor.findChild<QTabWidget *>("worldPages")->setCurrentIndex(1);
        const int count = treasures->count();
        button("worldTreasureAdd")->click();
        QCOMPARE(treasures->count(), count + 1);
        const int row = treasures->currentRow();
        spin("worldTreasureX")->setValue(255);
        spin("worldTreasureZ")->setValue(0);
        auto table = editor.findChild<QComboBox *>("worldTreasureTable");
        table->setCurrentIndex(table->findData(1));
        auto item = editor.findChild<QComboBox *>("worldTreasureItem");
        item->setCurrentIndex(item->findData(277));
        button("worldTreasureApply")->click();
        const auto record =
            ScenarioWorld::treasureRecord(project.world()->scenario(52).treasures, row);
        QCOMPARE(u16(record, 4), quint16(277));
        QCOMPARE(u8(record, 1), quint8(255));
        editor.grab().save("scenario-treasure.png");
        button("worldTreasureRemove")->click();
        QCOMPARE(treasures->count(), count);
        undo.undo();
        QCOMPARE(treasures->count(), count + 1);
        while (undo.canUndo())
            undo.undo();
        QCOMPARE(project.world()->json(), base);
    }
    void scenarioRouteEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadScenarios();
        QUndoStack undo;
        ScenarioEditor editor(&project, &undo, [] {});
        editor.resize(1500, 950);
        editor.show();
        editor.findChild<QTabWidget *>("scenarioPages")->setCurrentIndex(1);
        auto button = [&](const char *name) { return editor.findChild<QPushButton *>(name); };
        auto spin = [&](const char *name) { return editor.findChild<QSpinBox *>(name); };
        auto select = [&](const char *name, int id) {
            auto box = editor.findChild<QComboBox *>(name);
            QVERIFY(box);
            const int at = box->findData(id);
            QVERIFY(at >= 0);
            box->setCurrentIndex(at);
        };
        auto missions = editor.findChild<QListWidget *>("scenarioList");
        auto nodes = editor.findChild<QListWidget *>("scenarioNodes");
        auto conditions = editor.findChild<QListWidget *>("scenarioConditions");
        auto tabs = editor.findChild<QTabWidget *>("scenarioRouteTabs");
        const auto original = project.scenarios()->json();
        for (int mission = 0; mission < missions->count(); ++mission) {
            missions->setCurrentRow(mission);
            for (int row = 0; row < nodes->count(); ++row) {
                nodes->setCurrentRow(row);
                button("routeNodeApply")->click();
                QCOMPARE(project.scenarios()->json(), original);
            }
            for (int row = 0; row < conditions->count(); ++row) {
                conditions->setCurrentRow(row);
                button("routeConditionApply")->click();
                QCOMPARE(project.scenarios()->json(), original);
            }
        }
        for (int i = 0; i < missions->count(); ++i)
            if (missions->item(i)->data(Qt::UserRole).toInt() == 10)
                missions->setCurrentRow(i);
        button("routeConditionAdd")->click();
        QCOMPARE(project.scenarios()->scenario(10).layout.conditions.size(), 1);
        tabs->setCurrentIndex(1);
        spin("routePayload_6")->setValue(4);
        button("routeConditionApply")->click();
        QCOMPARE(u8(project.scenarios()->scenario(10).layout.conditions[0], 6), 4);
        tabs->setCurrentIndex(0);
        button("routeNodeAdd")->click();
        QCOMPARE(project.scenarios()->scenario(10).layout.nodes.size(), 2);
        select("routePlacement", 1);
        spin("routeX")->setValue(80);
        spin("routeZ")->setValue(160);
        select("routeConditionA", 1);
        select("routeNext", 1);
        button("routeNodeApply")->click();
        const auto saved = project.scenarios()->json();
        QCOMPARE(u8(project.scenarios()->scenario(10).layout.nodes[1], 5), 160);
        QCOMPARE(u8(project.scenarios()->scenario(10).layout.nodes[1], 10), 1);
        undo.undo();
        QVERIFY(project.scenarios()->json() != saved);
        undo.redo();
        QCOMPARE(project.scenarios()->json(), saved);
        tabs->setCurrentIndex(1);
        button("routeConditionRemove")->click();
        QCOMPARE(project.scenarios()->json(), saved);
        QVERIFY(editor.findChild<QLabel *>("routeNotice")->text().contains("gates"));
        spin("routePayload_6")->setValue(31);
        button("routeConditionApply")->click();
        QCOMPARE(project.scenarios()->json(), saved);
        QVERIFY(editor.findChild<QLabel *>("routeNotice")->text().contains("30"));
        tabs->setCurrentIndex(2);
        select("routeGroupSquad", 1);
        spin("routeGroup")->setValue(7);
        select("routePattern", 5);
        button("routeGroupApply")->click();
        QCOMPARE(project.scenarios()->scenario(10).layout.deployments[0].right(2),
                 QByteArray::fromHex("0705"));
        tabs->setCurrentIndex(0);
        QTest::qWait(50);
        QVERIFY(
            editor.grab().save(QCoreApplication::applicationDirPath() + "/scenario-routes.png"));
    }
    void scenarioSquadEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadScenarios();
        QCOMPARE(project.appendClass(5, "Scenario fighter"), 165);
        QUndoStack undo;
        ScenarioEditor editor(&project, &undo, [] {});
        editor.resize(1500, 950);
        editor.show();
        auto button = [&](const char *name) { return editor.findChild<QPushButton *>(name); };
        auto combo = [&](const char *name) { return editor.findChild<QComboBox *>(name); };
        auto spin = [&](const char *name) { return editor.findChild<QSpinBox *>(name); };
        auto select = [&](const QString &name, int id) {
            auto box = editor.findChild<QComboBox *>(name);
            QVERIFY(box);
            const int at = box->findData(id);
            QVERIFY(at >= 0);
            box->setCurrentIndex(at);
        };
        auto list = editor.findChild<QListWidget *>("scenarioList");
        QCOMPARE(list->count(), 63);
        const auto untouched = project.scenarios()->json();
        for (int i = 0; i < list->count(); ++i) {
            list->setCurrentRow(i);
            auto squads = editor.findChild<QListWidget *>("scenarioSquads");
            for (int row = 0; row < squads->count(); ++row) {
                squads->setCurrentRow(row);
                button("scenarioSquadApply")->click();
                QCOMPARE(project.scenarios()->json(), untouched);
            }
        }
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->data(Qt::UserRole).toInt() == 10)
                list->setCurrentRow(i);
        const auto before = project.scenarios()->json();
        // Applying untouched controls must preserve raw drop encoding and all unknown bytes.
        button("scenarioSquadApply")->click();
        QCOMPARE(project.scenarios()->json(), before);
        select("scenarioClass_0", 165);
        for (int c = 1; c < 3; ++c)
            for (int p = 0; p < 3; ++p)
                select(QString("scenarioPosition_%1_%2").arg(c).arg(p), 0);
        select("scenarioGear_0_0", 277);
        for (const auto field : {"scenarioHealing", "scenarioFruit", "scenarioRevival"})
            select(field, 0);
        select("scenarioDrop", 0x8115);
        combo("scenarioPlacement")->setCurrentIndex(1);
        spin("scenarioX")->setValue(120);
        spin("scenarioZ")->setValue(160);
        button("scenarioSquadApply")->click();
        const auto after = project.scenarios()->json();
        QVERIFY(after != before);
        QCOMPARE(u8(project.scenarios()->scenario(10).squads[0], 0), 165);
        QCOMPARE(u16(project.scenarios()->scenario(10).squads[0], 2), 277);
        QCOMPARE(u8(project.scenarios()->scenario(10).layout.deployments[0], 4), 160);
        QCOMPARE(project.scenarios()->scenario(22).json(),
                 project.scenarios()->original(22).json());
        undo.undo();
        QCOMPARE(project.scenarios()->json(), before);
        undo.redo();
        QCOMPARE(project.scenarios()->json(), after);
        spin("scenarioLevel_0")->setValue(127);
        button("scenarioSquadApply")->click();
        QCOMPARE(project.scenarios()->json(), after);
        QVERIFY(editor.findChild<QLabel *>("scenarioEditNotice")->text().contains("1Ã¢â‚¬â€œ99"));
        spin("scenarioLevel_0")->setValue(0);
        button("scenarioAddSquad")->click();
        QCOMPARE(project.scenarios()->scenario(10).squads.size(),
                 project.scenarios()->original(10).squads.size() + 1);
        QVERIFY(button("scenarioRemoveSquad")->isEnabled());
        button("scenarioRemoveSquad")->click();
        QCOMPARE(project.scenarios()->json(), after);
        undo.undo();
        auto squads = editor.findChild<QListWidget *>("scenarioSquads");
        squads->setCurrentRow(squads->count() - 1);
        QVERIFY(button("scenarioRemoveSquad")->isEnabled());
        QTest::qWait(50);
        QVERIFY(
            editor.grab().save(QCoreApplication::applicationDirPath() + "/scenario-squads.png"));
        auto search = editor.findChild<QLineEdit *>("scenarioSearch");
        search->setText("no matching mission");
        QCOMPARE(list->currentRow(), -1);
        search->clear();
        QVERIFY(list->currentRow() >= 0);
    }
    void customSquadEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadEncounters();
        QCOMPARE(project.appendClass(5, "New squad class"), 165);
        QUndoStack undo;
        EncounterEditor editor(&project, &undo, [] {});
        editor.resize(1400, 850);
        editor.show();
        editor.findChild<QTabWidget *>()->setCurrentWidget(
            editor.findChild<QWidget *>("customSquadsPage"));
        auto button = [&](const char *name) { return editor.findChild<QPushButton *>(name); };
        auto combo = [&](const char *name) { return editor.findChild<QComboBox *>(name); };
        auto spin = [&](const char *name) { return editor.findChild<QSpinBox *>(name); };
        auto select = [&](const char *name, int id) {
            auto box = combo(name);
            QVERIFY(box);
            const auto at = box->findData(id);
            QVERIFY(at >= 0);
            box->setCurrentIndex(at);
        };
        button("customNew")->click();
        select("customScenario", 33);
        select("customTerrain", 9);
        editor.findChild<QLineEdit *>("customLabel")->setText("Raiding party");
        spin("customMemberCount")->setValue(3);
        select("customClass_0", 165);
        select("customClass_1", 71);
        spin("customLevel_1")->setValue(-10);
        select("customEquipment_0_0", 277);
        select("customEquipment_0_1", 1);
        editor.findChild<QTableWidget *>("customMembers")->setCurrentCell(2, 0);
        button("customCell_0")->click();
        QCOMPARE(combo("customPosition_2")->currentData().toInt(), 0);
        spin("customChance")->setValue(70);
        spin("customRetreat")->setValue(12);
        select("customBonusClass_0", 165);
        spin("customBonus_0")->setValue(25);
        select("customReward_0", 44);
        select("customReward_1", 0x8115);
        spin("customWeight_0")->setValue(3);
        spin("customWeight_1")->setValue(1);
        spin("customWeight_2")->setValue(2);
        button("customApply")->click();
        QCOMPARE(project.encounters()->customSquads().profiles().size(), 1);
        const auto saved = project.encounters()->customSquads().profiles()[339];
        QCOMPARE(saved.members.size(), 3);
        QCOMPARE(saved.members[0].classId, 165);
        QCOMPARE(saved.members[0].equipment[0], 277);
        QCOMPARE(saved.members[1].levelOffset, -10);
        QCOMPARE(saved.members[2].cell, 0);
        QCOMPARE(saved.chance, 70);
        QCOMPARE(saved.retreatHp, 12);
        QCOMPARE(saved.bonuses[0].second, 25);
        QCOMPARE(saved.rewards.outcomes[0], 44);
        const auto before = project.manifest();
        select("customPosition_2", 6);
        button("customApply")->click();
        QCOMPARE(project.manifest(), before);
        QVERIFY(editor.findChild<QLabel *>("customNotice")->text().contains("share"));
        undo.undo();
        QVERIFY(!project.encounters()->customSquads().hasProfiles());
        undo.redo();
        QCOMPARE(project.manifest(), before);
        combo("customClass_0")->setEditText("No matching class");
        button("customApply")->click();
        QCOMPARE(project.manifest(), before);
        select("customClass_0", 5);
        select("customClass_0", 165);
        QApplication::processEvents();
        QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath() +
                                   "/custom-squad-members.png"));
        editor.findChild<QTabWidget *>("customDetailsTabs")->setCurrentIndex(1);
        QApplication::processEvents();
        QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath() +
                                   "/custom-squad-battle.png"));
        button("customCopy")->click();
        QVERIFY(combo("customScenario")->currentData().toInt() != 33 ||
                combo("customTerrain")->currentData().toInt() != 9);
        button("customApply")->click();
        QCOMPARE(project.encounters()->customSquads().profiles().size(), 2);
        button("customRemove")->click();
        QCOMPARE(project.encounters()->customSquads().profiles().size(), 1);
        undo.undo();
        QCOMPARE(project.encounters()->customSquads().profiles().size(), 2);
        button("customRestore")->click();
        QCOMPARE(project.encounters()->customSquads().profiles().size(), 1);
        undo.undo();
        QCOMPARE(project.encounters()->customSquads().profiles().size(), 2);
        select("customPersuasion", 0);
        QVERIFY(!spin("customChance")->isEnabled());
        QVERIFY(!combo("customBonusClass_0")->isEnabled());
        auto search = editor.findChild<QLineEdit *>("customSearch");
        search->setText("No matching squad");
        QVERIFY(!button("customApply")->isEnabled());
        QVERIFY(!button("customCopy")->isEnabled());
        search->clear();
        QVERIFY(button("customApply")->isEnabled());
    }
    void scenarioRateEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadEncounters();
        QUndoStack undo;
        EncounterEditor editor(&project, &undo, [] {});
        editor.resize(1350, 760);
        editor.show();
        auto keys = editor.findChild<QListWidget *>("scenarioRateKeys");
        keys->setCurrentRow(65);
        auto normal = editor.findChild<QComboBox *>("scenarioMode_1");
        auto alternate = editor.findChild<QComboBox *>("scenarioMode_0");
        auto divisor = editor.findChild<QDoubleSpinBox *>("scenarioDivisor_1");
        auto pass = editor.findChild<QDoubleSpinBox *>("scenarioPass_1");
        auto apply = editor.findChild<QPushButton *>("scenarioApply");
        const auto original = project.encounters()->json();
        normal->setCurrentIndex(1);
        divisor->setValue(4294967295.0);
        pass->setValue(2147483648.0);
        alternate->setCurrentIndex(2);
        apply->click();
        QCOMPARE(project.encounters()->scenarioRates().branch(65, true)->passCount,
                 quint32(0x80000000));
        QCOMPARE(project.encounters()->scenarioRates().branch(65, false)->passCount, quint32(0));
        undo.undo();
        QCOMPARE(project.encounters()->json(), original);
        undo.redo();
        const auto before = project.manifest();
        divisor->setValue(1);
        apply->click();
        QCOMPARE(project.manifest(), before);
        QVERIFY(!editor.findChild<QLabel *>("scenarioNotice")->text().isEmpty());
        editor.findChild<QPushButton *>("scenarioRestore")->click();
        QCOMPARE(project.encounters()->json(), original);
        auto globalNormal = editor.findChild<QDoubleSpinBox *>("encounterNormalPass");
        globalNormal->setValue(777);
        editor.findChild<QPushButton *>("encounterApplyGlobal")->click();
        QCOMPARE(pass->value(), 777.0);
        QVERIFY(!pass->isEnabled());
        normal->setCurrentIndex(1);
        pass->setValue(100);
        apply->click();
        editor.findChild<QPushButton *>("scenarioInherit")->click();
        QVERIFY(!project.encounters()->scenarioRates().branch(65, true));
        undo.undo();
        QCOMPARE(project.encounters()->scenarioRates().branch(65, true)->passCount, quint32(100));
        editor.findChild<QTabWidget *>()->setCurrentWidget(
            editor.findChild<QWidget *>("scenarioRatesPage"));
        QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath() + "/scenario-rates.png"));
        editor.findChild<QLineEdit *>("scenarioRateSearch")->setText("No matching scenario");
        QVERIFY(!keys->currentItem());
        QVERIFY(!apply->isEnabled());
    }
    void creatureRewardEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadEncounters();
        project.appendClass(1, "New reward class");
        QUndoStack undo;
        EncounterEditor editor(&project, &undo, [] {});
        editor.resize(1250, 760);
        editor.show();
        auto classes = editor.findChild<QListWidget *>("rewardClasses");
        QCOMPARE(classes->count(), 165);
        editor.findChild<QLineEdit *>("rewardSearch")->setText("New reward class");
        QCOMPARE(classes->currentItem()->data(Qt::UserRole).toInt(), 165);
        auto item = editor.findChild<QComboBox *>("rewardItem_0");
        auto first = editor.findChild<QSpinBox *>("rewardWeight_0");
        auto second = editor.findChild<QSpinBox *>("rewardWeight_1");
        auto third = editor.findChild<QSpinBox *>("rewardWeight_2");
        auto apply = editor.findChild<QPushButton *>("rewardApply");
        const auto original = project.encounters()->json();
        item->setCurrentIndex(item->findData(0x8115));
        first->setValue(65535);
        second->setValue(0);
        third->setValue(1);
        apply->click();
        QCOMPARE(project.encounters()->rewards().row(165).outcomes[0], 0x8115);
        QCOMPARE(project.encounters()->rewards().row(165).weights[0], 65535);
        undo.undo();
        QCOMPARE(project.encounters()->json(), original);
        undo.redo();
        QCOMPARE(first->value(), 65535);
        const auto before = project.manifest();
        first->setValue(0);
        third->setValue(0);
        apply->click();
        QCOMPARE(project.manifest(), before);
        QVERIFY(!editor.findChild<QLabel *>("rewardNotice")->text().isEmpty());
        editor.findChild<QPushButton *>("rewardRestore")->click();
        QCOMPARE(project.encounters()->json(), original);
        auto copy = editor.findChild<QComboBox *>("rewardCopySource");
        copy->setCurrentIndex(copy->findData(0x30));
        editor.findChild<QPushButton *>("rewardCopy")->click();
        QCOMPARE(project.encounters()->rewards().row(165).json(),
                 project.encounters()->rewards().row(0x30).json());
        editor.findChild<QPushButton *>("rewardClear")->click();
        QCOMPARE(project.encounters()->rewards().row(165).json(), CreatureReward{}.json());
        undo.undo();
        editor.findChild<QTabWidget *>()->setCurrentWidget(
            editor.findChild<QWidget *>("creatureRewardsPage"));
        QVERIFY(
            editor.grab().save(QCoreApplication::applicationDirPath() + "/creature-rewards.png"));
        editor.findChild<QLineEdit *>("rewardSearch")->setText("No class with this name");
        QVERIFY(!classes->currentItem());
        QVERIFY(!apply->isEnabled());
        editor.findChild<QLineEdit *>("rewardSearch")->clear();
        QVERIFY(classes->currentItem());
        QVERIFY(apply->isEnabled());
    }
    void encounterGlobalRateEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadEncounters();
        QUndoStack undo;
        EncounterEditor editor(&project, &undo, [] {});
        editor.resize(1200, 750);
        editor.show();
        auto divisor = editor.findChild<QDoubleSpinBox *>("encounterDivisor"),
             normal = editor.findChild<QDoubleSpinBox *>("encounterNormalPass"),
             alternate = editor.findChild<QDoubleSpinBox *>("encounterAlternatePass");
        auto apply = editor.findChild<QPushButton *>("encounterApplyGlobal");
        QCOMPARE(divisor->value(), 72000.0);
        QCOMPARE(normal->value(), 51.0);
        QCOMPARE(alternate->value(), 11.0);
        const auto original = project.encounters()->json();
        divisor->setValue(1000000);
        normal->setValue(0);
        alternate->setValue(1000000);
        apply->click();
        QCOMPARE(project.encounters()->globalRate().normalPassCount(), quint32(0));
        QCOMPARE(project.encounters()->globalRate().alternatePassCount(), quint32(1000000));
        undo.undo();
        QCOMPARE(project.encounters()->json(), original);
        QCOMPARE(normal->value(), 51.0);
        undo.redo();
        auto linked = editor.findChild<QCheckBox *>("encounterLinkRates");
        linked->setChecked(true);
        normal->setValue(700);
        QCOMPARE(alternate->value(), 700.0);
        QVERIFY(!alternate->isEnabled());
        apply->click();
        QCOMPARE(project.encounters()->globalRate().alternatePassCount(), quint32(700));
        const auto before = project.manifest();
        divisor->setValue(1);
        apply->click();
        QCOMPARE(project.manifest(), before);
        editor.findChild<QPushButton *>("encounterRestoreGlobal")->click();
        QCOMPARE(project.encounters()->json(), original);
        QVERIFY(!linked->isChecked());
        undo.undo();
        QCOMPARE(project.encounters()->globalRate().normalPassCount(), quint32(700));
        auto tabs = editor.findChild<QTabWidget *>();
        tabs->setCurrentIndex(2);
        QApplication::processEvents();
        QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath() + "/global-rates.png"));
    }
    void encounterEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadEncounters();
        project.appendClass(1, "Encounter tester");
        QUndoStack undo;
        EncounterEditor editor(&project, &undo, [] {});
        editor.resize(1350, 750);
        editor.show();
        auto list = editor.findChild<QListWidget *>("encounterPools");
        QCOMPARE(list->count(), 45);
        list->setCurrentRow(44);
        const auto original = project.encounters()->json();
        auto first = editor.findChild<QComboBox *>("encounterClass_9_0");
        first->setCurrentIndex(first->findData(165));
        QCOMPARE(project.encounters()->classAt(45, 9, 0), 165);
        undo.undo();
        QCOMPARE(project.encounters()->json(), original);
        undo.redo();
        QCOMPARE(first->currentData().toInt(), 165);
        editor.findChild<QComboBox *>("encounterCopySource")->setCurrentIndex(0);
        editor.findChild<QPushButton *>("encounterCopy")->click();
        for (int s = 0; s < 10; ++s)
            for (int side = 0; side < 2; ++side)
                QCOMPARE(project.encounters()->classAt(45, s, side),
                         project.encounters()->classAt(1, s, side));
        undo.undo();
        QCOMPARE(project.encounters()->classAt(45, 9, 0), 165);
        editor.findChild<QPushButton *>("encounterRestore")->click();
        QCOMPARE(project.encounters()->json(), original);
        undo.undo();
        QCOMPARE(project.encounters()->classAt(45, 9, 0), 165);
        auto slot = editor.findChild<QComboBox *>("encounterSlot_31");
        auto rate = editor.findChild<QSpinBox *>("encounterRate_31");
        QVERIFY(!rate->isEnabled());
        slot->setCurrentIndex(10);
        QVERIFY(rate->isEnabled());
        rate->setValue(100);
        QCOMPARE(project.encounters()->rate(31), 100);
        slot->setCurrentIndex(0);
        QCOMPARE(project.encounters()->rate(31), 0);
        QVERIFY(!rate->isEnabled());
        undo.undo();
        QCOMPARE(rate->value(), 100);
        QCOMPARE(slot->currentData().toInt(), 10);
        editor.findChild<QLineEdit *>("encounterSearch")->setText("Encounter tester");
        QVERIFY(!list->item(44)->isHidden());
        QApplication::processEvents();
        editor.grab().save(QCoreApplication::applicationDirPath() + "/encounters.png");
    }
    void consumableEffectEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        auto project = Project::fromRom(Rom::open(path));
        project.loadTexts();
        project.loadConsumableEffects();
        QUndoStack undo;
        ConsumableEditor editor(&project, &undo, [] {});
        editor.resize(410, 550);
        editor.show();
        editor.selectConsumable(3);
        const auto before = project.manifest();
        auto minimum = editor.findChild<QSpinBox *>("effectMinimum");
        auto maximum = editor.findChild<QSpinBox *>("effectMaximum");
        auto apply = editor.findChild<QPushButton *>("effectApply");
        minimum->setValue(333);
        apply->click();
        QCOMPARE(project.consumableEffects()->models()["healPack"].toObject()["magnitude"].toInt(),
                 333);
        QVERIFY(project.textBanks()["consumables"].text(3).contains("333HP"));
        undo.undo();
        QCOMPARE(project.manifest(), before);
        QCOMPARE(minimum->value(), 150);
        undo.redo();
        QCOMPARE(minimum->value(), 333);
        editor.selectConsumable(11);
        minimum->setValue(7);
        maximum->setValue(9);
        apply->click();
        editor.selectConsumable(16);
        QCOMPARE(minimum->value(), 7);
        QCOMPARE(maximum->value(), 9);
        const auto count = undo.count();
        minimum->setValue(12);
        maximum->setValue(10);
        apply->click();
        QCOMPARE(undo.count(), count);
        QCOMPARE(project.consumableEffects()
                     ->models()["sharedStatBoosters11To16"]
                     .toObject()["minimum"]
                     .toInt(),
                 7);
        editor.selectConsumable(44);
        QVERIFY(!apply->isEnabled());
        editor.selectConsumable(3);
        editor.findChild<QPushButton *>("effectRestore")->click();
        QCOMPARE(minimum->value(), 150);
        undo.undo();
        QCOMPARE(minimum->value(), 333);
        editor.grab().save(QCoreApplication::applicationDirPath() + "/consumable-effects.png");
    }
    void shopEditingAndUndo() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        QFile catalog(":/catalog.json");
        QVERIFY(catalog.open(QIODevice::ReadOnly));
        auto project =
            Project::fromRom(Rom::open(path), QJsonDocument::fromJson(catalog.readAll()).object());
        project.loadShops();
        const auto original = project.shops()->json();
        QUndoStack undo;
        ShopEditor editor(&project, &undo, [] {});
        editor.resize(1250, 740);
        editor.show();
        auto shops = editor.findChild<QListWidget *>("shopList");
        QCOMPARE(shops->count(), 35);
        shops->setCurrentRow(1);
        auto choice = editor.findChild<QComboBox *>("shopEquipmentChoice");
        QVERIFY(choice->findData(277) >= 0);
        choice->setCurrentIndex(choice->findData(277));
        editor.findChild<QPushButton *>("shopEquipmentAdd")->click();
        QCOMPARE(project.shops()->stock(2).equipment.last(), quint16(277));
        QCOMPARE(choice->findData(277), -1);
        auto inventory = editor.findChild<QListWidget *>("shopEquipmentList");
        QCOMPARE(inventory->currentItem()->data(Qt::UserRole).toInt(), 277);
        const auto added = project.shops()->json();
        editor.findChild<QPushButton *>("shopEquipmentUp")->click();
        QCOMPARE(
            project.shops()->stock(2).equipment[project.shops()->stock(2).equipment.size() - 2],
            quint16(277));
        undo.undo();
        QCOMPARE(project.shops()->json(), added);
        editor.findChild<QPushButton *>("shopEquipmentRemove")->click();
        QCOMPARE(project.shops()->json(), original);
        undo.undo();
        QCOMPARE(project.shops()->json(), added);
        auto consumable = editor.findChild<QComboBox *>("shopConsumablesChoice");
        consumable->setCurrentIndex(consumable->findData(44));
        QVERIFY(!consumable->currentText().startsWith("Consumable "));
        editor.findChild<QPushButton *>("shopConsumablesAdd")->click();
        QCOMPARE(project.shops()->stock(2).consumables.last(), quint16(44));
        auto search = editor.findChild<QLineEdit *>("shopSearch");
        search->setText("Jadd");
        QVERIFY(!shops->item(1)->isHidden());
        QVERIFY(shops->item(34)->isHidden());
        search->clear();
        auto from = editor.findChild<QComboBox *>("shopCopySource");
        from->setCurrentIndex(from->findData(3));
        editor.findChild<QPushButton *>("shopCopy")->click();
        QCOMPARE(project.shops()->stock(2), project.shops()->stock(3));
        undo.undo();
        QCOMPARE(project.shops()->stock(2).consumables.last(), quint16(44));
        QVERIFY(editor.findChild<QPushButton *>("shopEquipmentDown")->isEnabled());
        editor.findChild<QPushButton *>("shopRestore")->click();
        QCOMPARE(project.shops()->json(), original);
        undo.undo();
        QCOMPARE(project.shops()->stock(2).consumables.last(), quint16(44));
        QTest::qWait(50);
        if (!qEnvironmentVariable("LORDLY_SHOP_SCREENSHOT").isEmpty())
            QVERIFY(editor.grab().save(qEnvironmentVariable("LORDLY_SHOP_SCREENSHOT")));
    }
    void consumableFieldsUndo() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        auto project = Project::fromRom(Rom::open(path));
        const auto original = project.record(Table::Consumables, 1).bytes;
        QUndoStack undo;
        RecordModel model(&project, Table::Consumables, &undo);
        QCOMPARE(model.rowCount(), 45);
        QVERIFY(model.setData(model.index(1, 2), 65535, Qt::EditRole));
        auto expected = original;
        put16(expected, 6, 65535);
        QCOMPARE(project.record(Table::Consumables, 1).bytes, expected);
        undo.undo();
        QCOMPARE(project.record(Table::Consumables, 1).bytes, original);
        undo.redo();
        QCOMPARE(project.record(Table::Consumables, 1).bytes, expected);
        QVERIFY(!model.setData(model.index(1, 2), 65536, Qt::EditRole));
        QCOMPARE(project.record(Table::Consumables, 1).bytes, expected);
        QVERIFY(!(model.flags(model.index(1, 1)) & Qt::ItemIsEditable));
    }
    void classNamesAndUndo() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        auto project = Project::fromRom(Rom::open(path));
        QUndoStack undo;
        RecordModel model(&project, Table::Classes, &undo);
        const auto original = project.classNameBytes(164);
        const Field choiceField{"class", "Class", "", 0, 1, false, "CLASS_NAMES"};
        ValueEditor choice(choiceField, project.catalog(), [](int) {});
        connect(&model, &RecordModel::edited, &choice,
                [&] { choice.setChoices(project.catalog()); });
        QTableView view;
        view.setModel(&model);
        view.setItemDelegateForColumn(1, new ClassNameDelegate(&view));
        view.resize(600, 400);
        view.show();
        const auto index = model.index(164, 1);
        view.scrollTo(index);
        view.edit(index);
        QTRY_VERIFY(view.findChild<QPlainTextEdit *>());
        auto editor = view.findChild<QPlainTextEdit *>();
        QCOMPARE(editor->toPlainText(), QString("Death\nBahamut"));
        editor->setPlainText("Ancient\nBahamut");
        QTest::keyClick(editor, Qt::Key_Return, Qt::ControlModifier);
        QTRY_COMPARE(project.record(Table::Classes, 164).name, QString("Ancient\nBahamut"));
        auto combo = choice.findChild<QComboBox *>();
        QCOMPARE(combo->itemText(combo->findData(164)), QString("Ancient\nBahamut"));
        undo.undo();
        QCOMPARE(project.classNameBytes(164), original);
        QCOMPARE(combo->itemText(combo->findData(164)), QString("Death\nBahamut"));
        undo.redo();
        QCOMPARE(project.classNameBytes(164), encodeClassName("Ancient\nBahamut"));
        QSignalSpy rejected(&model, &RecordModel::editRejected);
        QVERIFY(!model.setData(index, "", Qt::EditRole));
        QCOMPARE(rejected.size(), 1);
        QCOMPARE(project.classNameBytes(164), encodeClassName("Ancient\nBahamut"));
    }
    void addClassUndoRedo() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        auto project = Project::fromRom(Rom::open(path));
        project.loadTexts();
        project.loadPromotion();
        QUndoStack undo;
        RecordModel model(&project, Table::Classes, &undo);
        const auto before = project.manifest();
        auto descriptions = std::unique_ptr<QWidget>(createTextWorkspace(&project, &undo, [] {}));
        descriptions->findChild<QComboBox *>()->setCurrentIndex(1); // classes follows actions
        QCOMPARE(model.appendClass(5, "New fighter"), 165);
        QCOMPARE(model.rowCount(), 166);
        const auto added = project.manifest();
        QVERIFY(model.setData(model.index(165, 1), "Renamed fighter", Qt::EditRole));
        undo.undo();
        QCOMPARE(project.manifest(), added);
        undo.undo();
        QCOMPARE(model.rowCount(), 165);
        QCOMPARE(project.manifest(), before);
        QCOMPARE(project.textBanks()["classes"].entries.size(), 225);
        QCOMPARE(project.promotion()->classCount(), 81);
        undo.redo();
        QCOMPARE(model.rowCount(), 166);
        QCOMPARE(project.manifest(), added);
        QCOMPARE(project.textBanks()["classes"].entries.size(), 226);
        undo.redo();
        QCOMPARE(project.record(Table::Classes, 165).name, QString("Renamed fighter"));
        QSignalSpy rejected(&model, &RecordModel::editRejected);
        const auto current = project.manifest();
        QCOMPARE(model.appendClass(0, "Invalid"), -1);
        QCOMPARE(rejected.size(), 1);
        QCOMPARE(project.manifest(), current);
    }
    void loadDesktopWorkspace() {
        auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        MainWindow window;
        window.show();
        window.loadRom(path);
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QListWidget *>("encounterPools"), 10000);
        auto classes = window.findChild<QTableView *>("classesTable");
        auto items = window.findChild<QTableView *>("itemsTable");
        auto consumables = window.findChild<QTableView *>("consumablesTable");
        QVERIFY(classes);
        QVERIFY(items);
        QVERIFY(consumables);
        QCOMPARE(classes->model()->rowCount(), 165);
        QCOMPARE(items->model()->rowCount(), 278);
        QCOMPARE(consumables->model()->rowCount(), 45);
        QCOMPARE(window.findChild<QListWidget *>("encounterPools")->count(), 45);
        QCOMPARE(window.findChild<QListWidget *>("scenarioList")->count(), 63);
        auto screenshot = qEnvironmentVariable("LORDLY_TEST_SCREENSHOT");
        if (!screenshot.isEmpty()) {
            QTest::qWait(100);
            QVERIFY(window.grab().save(screenshot));
        }
    }
    void promotionControlsAndUndo() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        auto project = Project::fromRom(Rom::open(path));
        project.loadPromotion();
        const auto original = project.promotion()->json();
        QUndoStack undo;
        PromotionEditor editor(&project, &undo, [] {});
        editor.selectClass(5);
        auto equipment = editor.findChild<QCheckBox *>("standardEquipmentList");
        QVERIFY(equipment);
        const auto originalFlags = u8(project.promotion()->flags, 5);
        equipment->setChecked(!(originalFlags & 0x80));
        QCOMPARE(u8(project.promotion()->flags, 5), quint8(originalFlags ^ 0x80));
        undo.undo();
        QCOMPARE(project.promotion()->json(), original);
        QCOMPARE(equipment->isChecked(), bool(originalFlags & 0x80));
        undo.redo();
        QCOMPARE(u8(project.promotion()->flags, 5), quint8(originalFlags ^ 0x80));
        undo.undo();
        auto gate = editor.findChild<QSpinBox *>("promotionGate0");
        gate->setValue(90);
        QCOMPARE(u8(project.promotion()->gates, 40), quint8(90));
        undo.undo();
        QCOMPARE(project.promotion()->json(), original);
        undo.redo();
        QCOMPARE(gate->value(), 90);
        editor.findChild<QLineEdit *>("newFamilyName")->setText("Order of Dawn");
        editor.findChild<QComboBox *>("familyTemplate")->setCurrentIndex(0);
        editor.findChild<QPushButton *>("createFamily")->click();
        QCOMPARE(project.promotion()->families["assignments"].toArray()[5].toInt(), 16);
        undo.undo();
        QCOMPARE(project.promotion()->families, project.promotion()->originalFamilies);
        undo.redo();
        QCOMPARE(editor.findChild<QComboBox *>("classFamily")->currentData().toInt(), 16);
        editor.selectClass(100);
        QVERIFY(!gate->isEnabled());
        QVERIFY(!equipment->isEnabled());
        RecordModel model(&project, Table::Classes, &undo);
        connect(&model, &RecordModel::edited, &editor,
                [&] { editor.selectClass(model.rowCount() > 165 ? 165 : 100); });
        QCOMPARE(model.appendClass(5, "New promotion class"), 165);
        QVERIFY(gate->isEnabled());
        QVERIFY(equipment->isEnabled());
        QCOMPARE(gate->value(), 90);
        gate->setValue(120);
        QCOMPARE(u8(project.promotion()->gates, 165 * 8), quint8(120));
        undo.undo();
        QCOMPARE(gate->value(), 90);
        undo.undo();
        QVERIFY(!gate->isEnabled());
        QCOMPARE(project.promotion()->classCount(), 81);
        undo.redo();
        QVERIFY(gate->isEnabled());
        QCOMPARE(project.promotion()->classCount(), 166);
        const auto previousFamily = project.promotion()->families["assignments"].toArray()[165];
        editor.findChild<QLineEdit *>("newFamilyName")->setText("Added class family");
        editor.findChild<QPushButton *>("createFamily")->click();
        QCOMPARE(project.promotion()->families["assignments"].toArray()[165].toInt(), 17);
        undo.undo();
        QCOMPARE(project.promotion()->families["assignments"].toArray()[165], previousFamily);
        undo.redo();
        QCOMPARE(editor.findChild<QComboBox *>("classFamily")->currentData().toInt(), 17);
    }
};
QTEST_MAIN(UiTests)
#include "ui_tests.moc"
