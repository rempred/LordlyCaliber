#include "app/actor_pose_editor.h"
#include "app/animation_editor.h"
#include "app/animation_override_editor.h"
#include "app/army_editor.h"
#include "app/art_editor.h"
#include "app/attack_editor.h"
#include "app/attack_effect_editor.h"
#include "app/category_icon_editor.h"
#include "app/campaign_editor.h"
#include "app/change_report_dialog.h"
#include "app/class_setup.h"
#include "app/combat_editor.h"
#include "app/consumable_editor.h"
#include "app/cutscene_editor.h"
#include "app/dialogue_inspector.h"
#include "app/dialogue_preview.h"
#include "app/effect_composer.h"
#include "app/effect_library_editor.h"
#include "app/encounter_editor.h"
#include "app/formation_board.h"
#include "app/leader_affinity_editor.h"
#include "app/main_window.h"
#include "app/rom_build_dialog.h"
#include "app/custom_content.h"
#include "app/personal_name_picker.h"
#include "app/content_refresh.h"
#include "app/pending_edits.h"
#include "app/persuasion_editor.h"
#include "app/portrait_editor.h"
#include "app/project_review_dialog.h"
#include "app/promotion_editor.h"
#include "app/record_model.h"
#include "app/recovery_draft.h"
#include "app/rom_art_import.h"
#include "app/save_army_drag.h"
#include "app/save_editor.h"
#include "app/save_unit_editor.h"
#include "app/save_character_card.h"
#include "app/names_text_editor.h"
#include "app/item_picker.h"
#include "app/scenario_editor.h"
#include "app/scenario_event_editor.h"
#include "app/scenario_map_editor.h"
#include "app/scenario_route_editor.h"
#include "app/scene_action_editor.h"
#include "app/scene_command_editor.h"
#include "app/scene_event_editor.h"
#include "app/scene_preview.h"
#include "app/shop_editor.h"
#include "app/sprite_editor.h"
#include "app/sprite_library_command.h"
#include "app/squad_route_steps.h"
#include "app/table_transfer.h"
#include "app/terrain_assignments.h"
#include "app/testing_editor.h"
#include "app/text_workspace.h"
#include "app/theme.h"
#include "app/tools_editor.h"
#include "app/transform_preview.h"
#include "app/value_editor.h"
#include "engine/animation_file_import.h"
#include "engine/effect_resources.h"
#include "engine/event_script.h"
#include "engine/project64_session.h"
#include "engine/project_references.h"
#include "engine/recovery_store.h"
#include "engine/scene_import.h"
#include "engine/scene_timed_authoring.h"
#include "engine/sprite_sources.h"
#include "engine/squad_members.h"
#include "engine/weapon_status.h"
#include "game/classes/save_format.h"
#include "rebuild/compiler.h"
#include <QAbstractProxyModel>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
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
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QScopeGuard>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTabBar>
#include <QTabWidget>
#include <QTableView>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTimeEdit>
#include <QTimer>
#include <QTreeWidget>
#include <QUndoGroup>
#include <QWheelEvent>
#include <QtTest>
#include <QtConcurrent>
#include <bit>
#include <cmath>
using namespace lordly;
static bool selectMapObject(QWidget &editor, MapObject kind, int row) {
    auto objects = editor.findChild<QListWidget *>("mapObjects");
    for (int i = 0; objects && i < objects->count(); ++i) {
        auto item = objects->item(i);
        if (item->data(Qt::UserRole).toInt() == int(kind) &&
            item->data(Qt::UserRole + 1).toInt() == row) {
            objects->setCurrentItem(item);
            return true;
        }
    }
    return false;
}
static void editScenarioMembers(QWidget &editor,const QString &prefix,const std::function<void(QDialog *)> &edit) {
    auto button=editor.findChild<QPushButton *>(prefix+"EditMembers");QVERIFY(button);
    QTimer::singleShot(0,&editor,[&]{
        auto dialog=editor.findChild<QDialog *>(prefix+"MemberDialog");QVERIFY(dialog);
        const auto close=qScopeGuard([&]{dialog->reject();});edit(dialog);
    });
    button->click();
}
class UiTests : public QObject {
    Q_OBJECT
  private slots:
    void romBuildProgress() {
        RomBuildDialog dialog("C:/exports/Test ROM.v64");
        dialog.show();
        auto overall = dialog.findChild<QProgressBar *>("romBuildOverall");
        auto current = dialog.findChild<QProgressBar *>("romBuildCurrent");
        auto log = dialog.findChild<QPlainTextEdit *>("romBuildLog");
        auto close = dialog.findChild<QPushButton *>("romBuildClose");
        QVERIFY(overall && current && log && close);
        QVERIFY(!close->isEnabled());
        dialog.reject(); QVERIFY(dialog.isVisible());
        dialog.close(); QVERIFY(dialog.isVisible());
        auto callback = dialog.progressCallback();
        auto worker = QtConcurrent::run([callback] {
            callback({2, "Building scenarios and story", "Building squads", {"Squads compiled"}});
            callback({6, "Verifying ROM", "Checking records", {"Reopened ROM"}, 12, 20});
        });
        worker.waitForFinished();
        QTRY_VERIFY(log->toPlainText().contains("Reopened ROM"));
        QCOMPARE(overall->value(), 6);
        QCOMPARE(current->value(), 12); QCOMPARE(current->maximum(), 20);
        dialog.finish("Example <compiler error>", false, true);
        QVERIFY(close->isEnabled());
        QVERIFY(overall->value() < overall->maximum());
        QCOMPARE(current->value(), 0); QCOMPARE(current->maximum(), 1);
        QVERIFY(log->toPlainText().contains("Example <compiler error>"));
        dialog.findChild<QPushButton *>("romBuildCopy")->click();
        QVERIFY(QApplication::clipboard()->text().contains("Squads compiled"));
        QVERIFY(QApplication::clipboard()->text().contains("Example <compiler error>"));
        // A late queued progress event must not overwrite a failure.
        callback({8, "ROM exported", {}, {"late message"}, 1, 1});
        QCoreApplication::processEvents();
        QCOMPARE(overall->value(), 6);
        QVERIFY(!log->toPlainText().contains("late message"));
        QSignalSpy review(&dialog, &RomBuildDialog::reviewRequested);
        dialog.findChild<QPushButton *>("romBuildReview")->click();
        QCOMPARE(review.size(), 1); QVERIFY(!dialog.isVisible());
        RomBuildDialog success("C:/exports/Test ROM.v64"); success.show();
        success.finish({}, true);
        QCOMPARE(success.findChild<QProgressBar *>("romBuildOverall")->value(), BuildProgress::stageCount);
        QSignalSpy test(&success, &RomBuildDialog::testRequested);
        success.findChild<QPushButton *>("romBuildTest")->click();
        QCOMPARE(test.size(), 1); QVERIFY(!success.isVisible());
        RomBuildDialog automatic("C:/exports/Test ROM.v64"); automatic.show();
        automatic.finish(); automatic.accept(); QVERIFY(!automatic.isVisible());
    }
    void uniqueNameAndDeletionControls() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        MainWindow window;window.resize(1500,950);window.loadRom(path);window.show();
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QTableView *>("classesTable"),60000);
        auto grid=window.findChild<QTableView *>("classesTable");QVERIFY(grid);
        auto proxy=qobject_cast<QSortFilterProxyModel *>(grid->model());QVERIFY(proxy);
        auto model=qobject_cast<RecordModel *>(proxy->sourceModel());QVERIFY(model);
        auto select=[&](int id){grid->setCurrentIndex(proxy->mapFromSource(model->index(id,1)));QCoreApplication::processEvents();};
        select(0x76);auto names=window.findChild<QLabel *>("classUniqueName");QVERIFY(names);
        QCOMPARE(names->text(),"Asnabel");const auto className=model->index(0x76,1).data();
        QTimer::singleShot(0,[&]{
            auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());QVERIFY(dialog);
            auto close=qScopeGuard([&]{if(dialog->isVisible())dialog->reject();});
            auto list=dialog->findChild<QListWidget *>("personalNameList");QVERIFY(list);
            const auto match=list->findItems("Magnus",Qt::MatchExactly);QVERIFY(!match.isEmpty());list->setCurrentItem(match.first());
            dialog->findChild<QPushButton *>("usePersonalName")->click();
        });
        window.findChild<QPushButton *>("choosePersonalName")->click();
        QCOMPARE(names->text(),"Magnus");QCOMPARE(model->index(0x76,1).data(),className);
        auto undo=window.findChild<QUndoStack *>();QVERIFY(undo);undo->undo();QCOMPARE(names->text(),"Asnabel");undo->redo();QCOMPARE(names->text(),"Magnus");
        const int removed=model->createClass("Delete me"),kept=model->createClass("Keep me");QCOMPARE(removed,165);QCOMPARE(kept,166);
        select(removed);auto remove=window.findChild<QPushButton *>("deleteClass");QVERIFY(remove&&remove->isEnabled());remove->click();
        QVERIFY(!proxy->mapFromSource(model->index(removed,1)).isValid());QVERIFY(proxy->mapFromSource(model->index(kept,1)).isValid());
        undo->undo();QVERIFY(proxy->mapFromSource(model->index(removed,1)).isValid());undo->redo();QVERIFY(!proxy->mapFromSource(model->index(removed,1)).isValid());
        QCOMPARE(model->createClass("Reused slot"),removed);select(removed);QCOMPARE(names->text(),"Random name");
        QCOMPARE(model->index(kept,1).data().toString(),"Keep me");QCoreApplication::processEvents();QVERIFY(window.grab().save("unique-name-controls.png"));
    }
    void personalNameTablesPicker() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto p=Project::fromRom(Rom::open(path));p.loadNameTables();QUndoStack undo;RecordModel model(&p,Table::Classes,&undo);
        const auto original=p.manifest();const auto names=p.personalNames();
        auto choose=[&](const std::function<void(QDialog *)> &action){
            QTimer::singleShot(0,[&]{auto d=qobject_cast<QDialog *>(QApplication::activeModalWidget());QVERIFY(d);auto close=qScopeGuard([&]{if(d->isVisible())d->reject();});action(d);});
            const auto choice=pickPersonalName(nullptr,p,0x76);
            if(choice)model.setPersonalName(0x76,choice->id,choice->newName);
            return choice.has_value();
        };
        QVERIFY(!choose([&](QDialog *d){
            auto tables=d->findChild<QComboBox *>("personalNameTable");QCOMPARE(tables->count(),4);
            tables->setCurrentIndex(2);d->findChild<QLineEdit *>("newPersonalNameText")->setText("Unconfirmed");d->reject();
        }));QCOMPARE(p.manifest(),original);
        for(int pool=0;pool<3;++pool) {
            const auto expected=p.nameTables()->randomName(pool,8);
            QVERIFY(choose([&](QDialog *d){
                d->findChild<QComboBox *>("personalNameTable")->setCurrentIndex(pool+1);
                auto list=d->findChild<QListWidget *>("personalNameList");QCOMPARE(list->count(),256);
                d->findChild<QLineEdit *>("personalNameSearch")->setText(expected);
                list->setCurrentRow(8);QVERIFY(!list->item(8)->isHidden());d->findChild<QPushButton *>("usePersonalName")->click();
            }));
            QCOMPARE(p.personalNames().names()[p.personalNames().assignment(0x76)],expected.toLatin1());
            QCOMPARE(p.personalNames().assignment(0x51),names.assignment(0x51));
            undo.undo();QCOMPARE(p.manifest(),original);
        }
        QVERIFY(choose([&](QDialog *d){d->findChild<QLineEdit *>("newPersonalNameText")->setText("NewHero");d->findChild<QPushButton *>("createPersonalName")->click();}));
        const auto created=p.personalNames().json();QCOMPARE(p.personalNames().names()[p.personalNames().assignment(0x76)],QByteArray("NewHero"));
        undo.undo();QCOMPARE(p.manifest(),original);undo.redo();QCOMPARE(p.personalNames().json(),created);
        QTemporaryDir folder;p.saveFolder(folder.path());auto restored=Project::fromRom(Rom::open(path));restored.applyManifest(Project::readProject(folder.path()));QCOMPARE(restored.personalNames().json(),created);
        QVERIFY(choose([&](QDialog *d){d->findChild<QPushButton *>("useRandomName")->click();}));QCOMPARE(p.personalNames().assignment(0x76),-1);
        undo.undo();QCOMPARE(p.personalNames().json(),created);
    }
    void campaignBranchOverview() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));QUndoStack undo;CampaignEditor editor(&project,&undo,[]{});editor.resize(1500,950);editor.show();
        editor.selectNode(27);editor.findChild<QPushButton *>("campaignEditEntryRules")->click();QCoreApplication::processEvents();
        auto rows=editor.findChild<QTableWidget *>("storylineRules");QVERIFY(rows);QVERIFY(rows->rowCount()>=5);
        QSet<int> events;for(int row=0;row<rows->rowCount();++row){QCOMPARE(rows->item(row,2)->text(),QString("Game"));events.insert(rows->item(row,1)->data(Qt::UserRole).toInt());}
        for(int event:{22,23,24,53,54})QVERIFY(events.contains(event));
        QVERIFY(!editor.findChild<QPushButton *>("storylineEditBranch")->isEnabled());
        QVERIFY(!editor.findChild<QPushButton *>("storylineRemoveBranch")->isEnabled());
        QVERIFY(editor.findChild<QPushButton *>("storylineOpenEvent")->isEnabled());
        auto badge=editor.findChild<QLabel *>("storylineWipBadge");QVERIFY(badge);QVERIFY(!editor.findChild<QLabel *>("cutsceneWipBadge"));
        auto bar=editor.findChild<QTabWidget *>("campaignTabs")->tabBar();QVERIFY(bar);
        const QRect badgeBounds(badge->mapTo(bar,QPoint()),badge->size());
        QVERIFY(bar->tabRect(2).adjusted(0,0,-8,0).contains(badgeBounds));
        QVERIFY(editor.grab().save("native/build/campaign-branches-updated.png"));
        editor.findChild<QPushButton *>("storylineOpenEvent")->click();QCOMPARE(editor.findChild<QTabWidget *>("storylineTabs")->currentIndex(),2);
    }
    void suppliedProjectDeletion() {
        const auto rom=qEnvironmentVariable("LORDLY_TEST_ROM"),file=qEnvironmentVariable("LORDLY_TEST_PROJECT");
        if(rom.isEmpty()||file.isEmpty())QSKIP("Supplied audit project and private ROM required");
        QFile input(file);QVERIFY(input.open(QIODevice::ReadOnly));const auto original=input.readAll();input.close();
        QTemporaryDir copy;QVERIFY(copy.isValid());atomicWrite(copy.filePath("project.json"),original);
        auto project=Project::fromRom(Rom::open(rom));project.applyManifest(Project::readProject(copy.filePath("project.json")));
        QUndoStack undo;const auto before=project.manifest();
        for(int id:{165,166,167}) {
            QVERIFY(project.record(Table::Classes,id).name.contains("delete",Qt::CaseInsensitive));
            QVERIFY(project.deletionUses("classes",id).isEmpty());
        }
        QTimer::singleShot(0,[&]{
            auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if(!dialog){QFAIL("Missing delete dialog");return;}
            auto close=qScopeGuard([&]{dialog->reject();});
            auto rows=dialog->findChild<QListWidget *>("customContentList");
            auto remove=dialog->findChild<QPushButton *>("deleteCustomContent");QVERIFY(rows&&remove);
            for(int id:{165,166,167}) {
                QListWidgetItem *chosen=nullptr;
                for(int row=0;row<rows->count();++row)if(rows->item(row)->data(Qt::UserRole).toInt()==id)chosen=rows->item(row);
                QVERIFY(chosen);rows->setCurrentItem(chosen);QVERIFY(remove->isEnabled());remove->click();
                QVERIFY(project.contentDeleted("classes",id));
            }
        });
        showCustomContent(nullptr,&project,&undo,"classes");QCOMPARE(undo.count(),3);
        const auto after=project.manifest();
        for(int n=0;n<3;++n)undo.undo();QCOMPARE(project.manifest(),before);
        for(int n=0;n<3;++n)undo.redo();QCOMPARE(project.manifest(),after);
        project.saveFolder(copy.filePath("deleted"));
        auto reopened=Project::fromRom(Rom::open(rom));reopened.applyManifest(Project::readProject(copy.filePath("deleted")));
        QCOMPARE(reopened.deletedContent(),project.deletedContent());
        for(int id:{168,169,170})QCOMPARE(reopened.record(Table::Classes,id).bytes,project.record(Table::Classes,id).bytes);
        QVERIFY(input.open(QIODevice::ReadOnly));QCOMPARE(input.readAll(),original);
    }
    void campaignArtworkEditing() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);applyEditorTheme(*qApp);
        auto project=Project::fromRom(Rom::open(path));QUndoStack undo;CampaignEditor editor(&project,&undo,[]{});
        editor.resize(1500,950);editor.show();editor.selectNode(6);QCoreApplication::processEvents();
        auto canvas=editor.findChild<QWidget *>("campaignMapCanvas");QVERIFY(canvas);
        const auto before=canvas->grab().toImage(),originalBackground=CampaignMap::artworkImage(project.campaignMap()->backgroundBytes(),{512,320},256);
        QTemporaryDir files;
        auto exportPng=[&](const QString &path){
            QTimer choose;choose.setInterval(20);connect(&choose,&QTimer::timeout,&editor,[&]{
                if(auto dialog=qobject_cast<QFileDialog *>(QApplication::activeModalWidget())){
                    choose.stop();dialog->selectFile(path);QMetaObject::invokeMethod(dialog,"accept");
                }
            });
            choose.start();editor.findChild<QPushButton *>("campaignExportArtwork")->click();choose.stop();
            return QImage(path).convertToFormat(QImage::Format_RGBA8888);
        };
        const auto png=files.filePath("map.png");
        const auto originalMap=project.campaignMap()->json();
        QCOMPARE(exportPng(png),originalBackground);QCOMPARE(undo.count(),0);QCOMPARE(project.campaignMap()->json(),originalMap);
        // Edit the exported artwork, import it, then export the current edit.
        QImage red(512,320,QImage::Format_RGBA8888);red.fill(QColor(255,0,0));QVERIFY(red.save(png));
        QTimer picker;picker.setInterval(20);connect(&picker,&QTimer::timeout,&editor,[&]{
            auto active=QApplication::activeModalWidget();
            if(auto dialog=qobject_cast<QFileDialog *>(active)){dialog->selectFile(png);QMetaObject::invokeMethod(dialog,"accept");}
            else if(active && active->objectName()=="campaignBackgroundImport"){picker.stop();qobject_cast<QDialog *>(active)->accept();}
        });
        picker.start();editor.findChild<QPushButton *>("campaignReplaceArtwork")->click();picker.stop();
        QCOMPARE(CampaignMap::artworkImage(project.campaignMap()->backgroundBytes(),{512,320},256),red);
        QVERIFY(canvas->grab().toImage()!=before);undo.undo();QCOMPARE(CampaignMap::artworkImage(project.campaignMap()->backgroundBytes(),{512,320},256),originalBackground);undo.redo();
        const int edits=undo.count();const auto editedMap=project.campaignMap()->json();
        QCOMPARE(exportPng(files.filePath("edited.png")),red);QCOMPARE(undo.count(),edits);QCOMPARE(project.campaignMap()->json(),editedMap);
        const auto originalFlag=project.campaignMap()->iconFrameBytes(0,1,0);
        bool editedIcon=false;
        QTimer::singleShot(0,&editor,[&]{
            auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());QVERIFY(dialog);QCOMPARE(dialog->objectName(),QString("campaignIconEditor"));
            QTimer::singleShot(0,dialog,[&]{auto prompt=qobject_cast<QDialog *>(QApplication::activeModalWidget());QVERIFY(prompt);prompt->findChild<QLineEdit *>()->setText("Independent banner");prompt->accept();});
            dialog->findChild<QPushButton *>("campaignArtworkCopy")->click();
            const int created=project.campaignMap()->iconIds().last();QVERIFY(created>=2);
            auto pixels=dialog->findChild<SpriteCanvas *>("campaignArtworkCanvas");QVERIFY(pixels);
            const auto frame=project.campaignMap()->iconFrameBytes(created,1,0);QByteArray painted(256,1);
            QVERIFY(QMetaObject::invokeMethod(pixels,"indicesCommitted",Qt::DirectConnection,Q_ARG(QByteArray,painted),Q_ARG(QPoint,QPoint())));
            QVERIFY(project.campaignMap()->iconFrameBytes(created,1,0)!=frame);QCOMPARE(project.campaignMap()->iconFrameBytes(0,1,0),originalFlag);
            undo.undo();QCOMPARE(project.campaignMap()->iconFrameBytes(created,1,0),frame);undo.redo();
            QVERIFY(dialog->grab().save(QCoreApplication::applicationDirPath()+"/campaign-icon-editor.png"));editedIcon=true;dialog->reject();
        });
        editor.findChild<QPushButton *>("campaignEditIcons")->click();
        QVERIFY2(editedIcon,qPrintable(editor.findChild<QLabel *>("campaignNotice")->text()));
        const int created=project.campaignMap()->iconIds().last();auto selector=editor.findChild<QComboBox *>("campaignLocationIcon");
        QVERIFY(created>=2);
        const int index=selector->findData(created);QVERIFY(index>=0);selector->setCurrentIndex(index);QMetaObject::invokeMethod(selector,"activated",Qt::DirectConnection,Q_ARG(int,index));
        QCOMPARE(project.campaignMap()->icon(6),created);undo.undo();QCOMPARE(project.campaignMap()->icon(6),0);undo.redo();
        QTemporaryDir saved;project.saveFolder(saved.path());auto reopened=Project::fromRom(Rom::open(path));reopened.applyManifest(Project::readProject(saved.path()));
        QCOMPARE(reopened.campaignMap()->json(),project.campaignMap()->json());QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath()+"/campaign-artwork.png"));
    }
    void campaignRoadControls() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));QUndoStack undo;CampaignEditor editor(&project,&undo,[]{});editor.resize(1500,950);editor.show();editor.selectNode(6);QCoreApplication::processEvents();
        QObject unrelatedEditor;int unrelatedRefreshes=0;
        watchContentChanges(&undo,&unrelatedEditor,[&]{++unrelatedRefreshes;});
        QVERIFY(!editor.findChild<QPushButton *>("campaignConnectNodes"));
        auto roads=editor.findChild<QListWidget *>("campaignRoads");auto canvas=editor.findChild<QWidget *>("campaignMapCanvas");QVERIFY(roads&&canvas);
        auto icon=editor.findChild<QComboBox *>("campaignLocationIcon");QVERIFY(icon);QCOMPARE(icon->currentData().toInt(),0);
        const auto flagImage=canvas->grab().toImage();icon->setCurrentIndex(1);QMetaObject::invokeMethod(icon,"activated",Qt::DirectConnection,Q_ARG(int,1));
        QCOMPARE(project.campaignMap()->icon(6),1);QVERIFY(canvas->grab().toImage()!=flagImage);QCOMPARE(project.campaignMap()->icon(7),1);
        undo.undo();QCOMPARE(project.campaignMap()->icon(6),0);QCOMPARE(icon->currentData().toInt(),0);
        undo.redo();QCOMPARE(project.campaignMap()->icon(6),1);undo.undo();
        auto screen=[&](QPointF p){const double scale=qMin(canvas->width()/512.,canvas->height()/320.);return (QPointF((canvas->width()-512*scale)/2,(canvas->height()-320*scale)/2)+p*scale).toPoint();};
        int road=0,dot=-1;
        for(int id=1;id<=project.campaignMap()->totalRoads()&&!road;++id){const auto points=project.campaignMap()->roadDots(id);for(int n=0;n<points.size();++n){bool clear=true;for(int node=1;node<=40;++node)if(project.campaignMap()->position(node).x()&&QLineF(points[n]+QPoint(3,3),project.campaignMap()->position(node)).length()<15)clear=false;if(clear){road=id;dot=n;break;}}}
        QVERIFY(road>0);const auto before=project.campaignMap()->roadDots(road);const auto end=screen(before[dot]+QPoint(9,7));
        QTest::mousePress(canvas,Qt::LeftButton,Qt::NoModifier,screen(before[dot]+QPoint(3,3)));
        const auto image=canvas->grab().toImage();QTest::mouseMove(canvas,end,20);QCoreApplication::processEvents();
        QCOMPARE(project.campaignMap()->roadDots(road),before);QVERIFY(canvas->grab().toImage()!=image);
        QTest::mouseRelease(canvas,Qt::LeftButton,Qt::NoModifier,end);QVERIFY(project.campaignMap()->roadDots(road)!=before);
        QCOMPARE(undo.count(),1);QCOMPARE(unrelatedRefreshes,0);QVERIFY(project.campaignMap()->roadWaypoints(road).size()>1);
        const auto edited=project.campaignMap()->roadDots(road);undo.undo();QCOMPARE(project.campaignMap()->roadDots(road),before);undo.redo();QCOMPARE(project.campaignMap()->roadDots(road),edited);
        const auto ends=project.campaignMap()->road(road);editor.selectNode(ends.first);QCoreApplication::processEvents();
        int expected=0;for(int id=1;id<=project.campaignMap()->totalRoads();++id){auto e=project.campaignMap()->road(id);expected+=e.first==ends.first||e.second==ends.first;}QCOMPARE(roads->count(),expected);
        for(int row=0;row<roads->count();++row)if(roads->item(row)->data(Qt::UserRole).toInt()==road)roads->setCurrentRow(row);
        editor.findChild<QPushButton *>("campaignRemoveRoad")->click();QCOMPARE(project.campaignMap()->road(road),qMakePair(0,0));QCOMPARE(roads->count(),expected-1);undo.undo();QCOMPARE(project.campaignMap()->road(road),ends);
        const auto pos=project.campaignMap()->position(ends.first);const auto priorDots=project.campaignMap()->roadDots(road);const auto priorImage=canvas->grab().toImage();
        QTest::mousePress(canvas,Qt::LeftButton,Qt::NoModifier,screen(pos));QTest::mouseMove(canvas,screen(pos+QPoint(5,3)),20);QCoreApplication::processEvents();
        QCOMPARE(project.campaignMap()->position(ends.first),pos);QVERIFY(canvas->grab().toImage()!=priorImage);
        QTest::mouseRelease(canvas,Qt::LeftButton,Qt::NoModifier,screen(pos+QPoint(5,3)));QVERIFY(project.campaignMap()->roadDots(road)!=priorDots);undo.undo();QCOMPARE(project.campaignMap()->roadDots(road),priorDots);
        QCoreApplication::processEvents();QVERIFY(editor.grab().save("campaign-road-controls.png"));
    }
    void campaignRepeatedRoadDragging() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));project.loadCampaignMap();
        auto map=*project.campaignMap();const int road=2;const auto ends=map.road(road);
        map.setNode(ends.first,{40,20},map.label(ends.first));
        map.setNode(ends.second,{470,20},map.label(ends.second));
        QVector<QPoint> straight;for(int n=1;n<=7;++n)straight<<QPoint(40+qRound(430.0*n/8),20);
        map.setRoadDots(road,straight);project.setCampaignMap(map.json());
        QUndoStack undo;CampaignEditor editor(&project,&undo,[]{});editor.resize(1500,950);editor.show();
        editor.selectNode(ends.first);QCoreApplication::processEvents();
        auto roads=editor.findChild<QListWidget *>("campaignRoads");
        for(int row=0;row<roads->count();++row)if(roads->item(row)->data(Qt::UserRole).toInt()==road)roads->setCurrentRow(row);
        auto canvas=editor.findChild<QWidget *>("campaignMapCanvas");QVERIFY(canvas);
        auto screen=[&](QPointF p){const double scale=qMin(canvas->width()/512.,canvas->height()/320.);return (QPointF((canvas->width()-512*scale)/2,(canvas->height()-320*scale)/2)+p*scale).toPoint();};
        const auto original=project.campaignMap()->roadWaypoints(road);QPoint target(255,20);
        for(int step=0;step<30;++step) {
            QVector<QVector<QPoint>> all(project.campaignMap()->totalRoads()+1);
            for(int id=1;id<all.size();++id)all[id]=project.campaignMap()->roadDots(id);
            int selected=0;for(int n=1;n<all[road].size();++n)
                if(QLineF(all[road][n],target).length()<QLineF(all[road][selected],target).length())selected=n;
            const auto visible=CampaignMap::mergedRoadDots(all)[road][selected];
            const QPoint delta(step%4<2?2:-2,step%2?-3:4);
            const auto before=project.campaignMap()->roadWaypoints(road);
            const auto begin=screen(visible+QPoint(3,3)),end=screen(visible+QPoint(3,3)+delta);
            QTest::mousePress(canvas,Qt::LeftButton,Qt::NoModifier,begin);
            QTest::mouseMove(canvas,end,1);QTest::mouseRelease(canvas,Qt::LeftButton,Qt::NoModifier,end);
            QCoreApplication::processEvents();
            QVERIFY2(project.campaignMap()->roadWaypoints(road)!=before,qPrintable(QString("Road stopped on drag %1: %2").arg(step+1).arg(editor.findChild<QLabel *>("campaignNotice")->text())));
            QCOMPARE(undo.count(),step+1);
            target=all[road][selected]+delta;
        }
        const auto edited=project.campaignMap()->roadWaypoints(road);
        undo.setIndex(0);QCOMPARE(project.campaignMap()->roadWaypoints(road),original);
        undo.setIndex(undo.count());QCOMPARE(project.campaignMap()->roadWaypoints(road),edited);
    }
    void campaignNavigationAndEditing() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        QFile catalog(":/catalog.json");QVERIFY(catalog.open(QIODevice::ReadOnly));
        auto project=Project::fromRom(Rom::open(path),QJsonDocument::fromJson(catalog.readAll()).object());project.loadShops();project.loadEncounters();project.loadScenarios();project.loadWorld();
        QUndoStack undo;CampaignEditor editor(&project,&undo,[]{});editor.resize(1500,950);editor.show();
        auto tabs=editor.findChild<QTabWidget *>("campaignTabs");auto navigation=editor.findChild<QTabBar *>("scenarioNavigation");
        QVERIFY(tabs&&navigation);QCOMPARE(tabs->tabText(0),"World Map");QCOMPARE(tabs->tabText(1),"Scenarios");QCOMPARE(tabs->tabText(2),"Storyline");
        auto scenarioList=editor.findChild<QListWidget *>("scenarioList");
        auto selectScenario=[&](int key){for(int i=0;i<scenarioList->count();++i)if(scenarioList->item(i)->data(Qt::UserRole).toInt()==key){scenarioList->setCurrentRow(i);return;}};
        int shopKey=-1,shopId=-1,siteRow=-1;
        for(const auto &s:project.scenarios()->entries()) {
            for(int row=0;row<project.world()->siteCount(s.key);++row){const auto town=project.world()->town(u16(project.world()->scenario(s.key).sites,1+row*4));if((u8(town,25)&1)&&u8(town,27)>0&&u8(town,24)!=255){shopKey=s.key;shopId=u8(town,27);siteRow=row;break;}}
            if(shopKey>=0)break;
        }
        QVERIFY(shopKey>=0);selectScenario(shopKey);navigation->setCurrentIndex(5);
        auto scope=editor.findChild<QComboBox *>("shopScope");auto shops=editor.findChild<QListWidget *>("shopList");
        QCOMPARE(scope->currentIndex(),0);QVERIFY(shops->isVisible());
        bool found=false;for(int i=0;i<shops->count();++i)if(shops->item(i)->data(Qt::UserRole).toInt()==shopId){QVERIFY(!shops->item(i)->isHidden());shops->setCurrentRow(i);found=true;}
        QVERIFY(found);scope->setCurrentIndex(1);for(int i=0;i<shops->count();++i)QVERIFY(!shops->item(i)->isHidden());
        scope->setCurrentIndex(0);navigation->setCurrentIndex(0);
        auto objects=editor.findChild<QListWidget *>("mapObjects");for(int i=0;i<objects->count();++i)if(objects->item(i)->data(Qt::UserRole).toInt()==int(MapObject::Site)&&objects->item(i)->data(Qt::UserRole+1).toInt()==siteRow)objects->setCurrentRow(i);
        auto openShop=editor.findChild<QPushButton *>("worldEditShop");QVERIFY(openShop&&openShop->isEnabled());openShop->click();QCOMPARE(navigation->currentIndex(),5);QCOMPARE(shops->currentItem()->data(Qt::UserRole).toInt(),shopId);
        auto stock=project.shops()->stock(shopId);auto inventory=editor.findChild<QListWidget *>("shopConsumablesList");
        if(inventory->count()){inventory->setCurrentRow(0);editor.findChild<QPushButton *>("shopConsumablesRemove")->click();QCOMPARE(project.shops()->stock(shopId).consumables.size(),stock.consumables.size()-1);undo.undo();QCOMPARE(project.shops()->stock(shopId),stock);}
        QCoreApplication::processEvents();QVERIFY(editor.grab().save("campaign-shops.png"));
        navigation->setCurrentIndex(6);selectScenario(1);QCoreApplication::processEvents();
        QVERIFY(!editor.findChild<QComboBox *>("encounterScope"));auto map=editor.findChild<QComboBox *>("encounterMapKey");
        QCOMPARE(map->currentData().toInt(),1);QVERIFY(!editor.findChild<QListWidget *>("encounterPools")->isVisible());
        QVERIFY(!editor.findChild<QLineEdit *>("encounterSearch")->isVisible());
        QTimer::singleShot(0,[&] {
            auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());QVERIFY(dialog);
            auto close=qScopeGuard([&]{dialog->reject();});
            QVERIFY(dialog->findChild<QListWidget *>("encounterPools")->isVisible());
            QVERIFY(dialog->findChild<QLineEdit *>("encounterSearch")->isVisible());
        });
        editor.findChild<QPushButton *>("encounterOverview")->click();
        selectScenario(2);QCOMPARE(map->currentData().toInt(),2);QCoreApplication::processEvents();QVERIFY(editor.grab().save("campaign-encounters.png"));
        editor.selectNode(6);const auto point=project.campaignMap()->position(6);
        editor.findChild<QSpinBox *>("campaignNodeX")->setValue(point.x()+3);editor.findChild<QPushButton *>("campaignApplyNode")->click();QCOMPARE(project.campaignMap()->position(6),point+QPoint(3,0));undo.undo();QCOMPARE(project.campaignMap()->position(6),point);
        const int spare=project.campaignMap()->availableNodes().first();
        const auto storyBefore=project.sceneEvents()->json();
        auto createLocation=editor.findChild<QPushButton *>("campaignCreateNode");auto canvas=editor.findChild<QWidget *>("campaignMapCanvas");QVERIFY(createLocation&&canvas);
        auto screen=[&](QPoint p){const double scale=qMin(canvas->width()/512.,canvas->height()/320.);return (QPointF((canvas->width()-512*scale)/2,(canvas->height()-320*scale)/2)+QPointF(p)*scale).toPoint();};
        createLocation->click();QVERIFY(!QApplication::activeModalWidget());QCOMPARE(project.campaignMap()->position(spare),QPoint());
        QTest::keyClick(canvas,Qt::Key_Escape);QCOMPARE(project.campaignMap()->position(spare),QPoint());
        QPoint placement;
        for(int y=20;y<300&&placement.isNull();y+=20)for(int x=20;x<500;++x){bool clear=true;for(int id=1;id<=40;++id)if(QLineF(QPoint(x,y),project.campaignMap()->position(id)).length()<20)clear=false;if(clear){placement={x,y};break;}}
        QVERIFY(!placement.isNull());createLocation->click();QTest::mouseClick(canvas,Qt::LeftButton,Qt::NoModifier,screen(placement));
        QCOMPARE(project.campaignMap()->position(spare),placement);QCOMPARE(project.campaignMap()->name(spare),QString("New map location"));
        QCOMPARE(project.campaignMap()->totalRoads(),40);QCOMPARE(project.sceneEvents()->json(),storyBefore);
        undo.undo();QCOMPARE(project.campaignMap()->position(spare),QPoint());undo.redo();QCOMPARE(project.campaignMap()->position(spare),placement);
        auto locationName=editor.findChild<QLineEdit *>("campaignNodeName");QVERIFY(locationName);locationName->setText("Test map location");editor.findChild<QPushButton *>("campaignApplyNode")->click();
        QCOMPARE(project.campaignMap()->name(spare),QString("Test map location"));QCOMPARE(project.sceneEvents()->json(),storyBefore);
        auto addRoad=editor.findChild<QPushButton *>("campaignAddRoad");QVERIFY(addRoad);
        addRoad->click();QTest::mouseClick(canvas,Qt::LeftButton,Qt::NoModifier,screen(project.campaignMap()->position(6)));
        QCOMPARE(project.campaignMap()->totalRoads(),41);QCOMPARE(project.campaignMap()->road(41),qMakePair(spare,6));
        editor.selectNode(spare);
        const auto sourcePosition=project.campaignMap()->position(spare),targetPosition=project.campaignMap()->position(8);
        addRoad->click();QVERIFY(!QApplication::activeModalWidget());QCOMPARE(canvas->cursor().shape(),Qt::CrossCursor);
        QTest::mouseClick(canvas,Qt::LeftButton,Qt::NoModifier,screen(sourcePosition));QCOMPARE(project.campaignMap()->totalRoads(),41);
        QTest::mouseClick(canvas,Qt::LeftButton,Qt::NoModifier,screen(project.campaignMap()->position(6)));QCOMPARE(project.campaignMap()->totalRoads(),41);
        QTest::keyClick(canvas,Qt::Key_Escape);QVERIFY(canvas->cursor().shape()!=Qt::CrossCursor);
        addRoad->click();QTest::mouseClick(canvas,Qt::RightButton);QVERIFY(canvas->cursor().shape()!=Qt::CrossCursor);QCOMPARE(project.campaignMap()->totalRoads(),41);
        addRoad->click();addRoad->click();QVERIFY(canvas->cursor().shape()!=Qt::CrossCursor);
        addRoad->click();QTest::mouseClick(canvas,Qt::LeftButton,Qt::NoModifier,screen(targetPosition));
        QVERIFY(canvas->cursor().shape()!=Qt::CrossCursor);QCOMPARE(project.campaignMap()->position(spare),sourcePosition);QCOMPARE(project.campaignMap()->position(8),targetPosition);
        QCOMPARE(editor.findChild<QListWidget *>("campaignNodes")->currentItem()->data(Qt::UserRole).toInt(),spare);
        QCOMPARE(project.campaignMap()->totalRoads(),42);QCOMPARE(project.campaignMap()->road(42),qMakePair(spare,8));undo.undo();QCOMPARE(project.campaignMap()->totalRoads(),41);undo.redo();QCOMPARE(project.campaignMap()->totalRoads(),42);
        QCoreApplication::processEvents();
QVERIFY(editor.grab().save("campaign-world-map.png"));
        editor.selectNode(spare);editor.findChild<QPushButton *>("campaignEditEntryRules")->click();
        bool unlockSaved=false;QTimer::singleShot(0,&editor,[&]{
            auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());if(!dialog)return;
            auto missions=dialog->findChild<QListWidget *>("mapLocationUnlockMissions");if(!missions){dialog->reject();return;}
            for(int i=0;i<missions->count();++i)if(missions->item(i)->data(Qt::UserRole).toInt()==4)missions->item(i)->setCheckState(Qt::Checked);
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();unlockSaved=dialog->result()==QDialog::Accepted;
        });
        editor.findChild<QPushButton *>("storylineUnlockLocation")->click();QVERIFY(unlockSaved);
        QVERIFY(project.sceneEvents()->flagEdits()["4"].toObject()["unlockNodes"].toObject()[QString::number(spare)].toBool());
        undo.undo();QCOMPARE(project.sceneEvents()->json(),storyBefore);QCOMPARE(project.campaignMap()->name(spare),QString("Test map location"));
        editor.selectNode(6);editor.findChild<QPushButton *>("campaignEditEntryRules")->click();
        bool branchSaved=false;QTimer::singleShot(0,&editor,[&]{auto dlg=qobject_cast<QDialog *>(QApplication::activeModalWidget());if(!dlg)return;dlg->findChild<QPushButton *>("storyBranchAddCondition")->click();auto conditions=dlg->findChild<QTableWidget *>("storyBranchConditions");qobject_cast<QComboBox *>(conditions->cellWidget(0,0))->setCurrentIndex(250);dlg->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();branchSaved=dlg->result()==QDialog::Accepted;if(!branchSaved)dlg->reject();});
        editor.findChild<QPushButton *>("storylineAddBranch")->click();QVERIFY(branchSaved);QCOMPARE(project.sceneEvents()->flagEdits()["0"].toObject()["6"].toArray().size(),1);QVERIFY(editor.grab().save("campaign-storyline.png"));
        undo.undo();QVERIFY(project.sceneEvents()->flagEdits()["0"].toObject()["6"].toArray().isEmpty());
        QTemporaryDir work;project.saveFolder(work.filePath("campaign"));auto saved=Project::fromRom(project.rom());saved.applyManifest(Project::readProject(work.filePath("campaign")));saved.loadSceneEvents();QCOMPARE(saved.campaignMap()->position(spare),project.campaignMap()->position(spare));QCOMPARE(saved.sceneEvents()->json(),project.sceneEvents()->json());
    }
    void animationPlaybackTransport() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));QUndoStack undo;
        QImage image(4,4,QImage::Format_RGBA8888);image.fill(Qt::red);
        auto asset=SpriteAsset::fromImage("Playback test",image);asset.frames[0].ticks=2;
        auto second=asset.frames[0];second.id=SpriteLibrary::newId();second.layers[0].image.fill(Qt::green);asset.frames<<second;
        SpriteEditor sprites(&project,&undo,[]{});sprites.addAsset(asset);sprites.show();
        const auto saved=project.manifest();
        auto play=sprites.findChild<QPushButton *>("spritePlay");auto loop=sprites.findChild<QCheckBox *>("spriteLoop");
        auto frames=sprites.findChild<QListWidget *>("spriteFrames");QVERIFY(play&&loop&&frames);QVERIFY(!loop->isChecked());
        play->click();QTRY_VERIFY_WITH_TIMEOUT(!play->isChecked(),2000);QCOMPARE(frames->currentRow(),1);
        play->click();QCOMPARE(frames->currentRow(),0);play->click();QVERIFY(!play->isChecked());
        const int paused=frames->currentRow();QTest::qWait(100);QCOMPARE(frames->currentRow(),paused);
        loop->setChecked(true);play->click();QTest::qWait(300);QVERIFY(play->isChecked());
        loop->setChecked(false);QTRY_VERIFY_WITH_TIMEOUT(!play->isChecked(),2000);
        QCOMPARE(project.manifest(),saved);sprites.hide();

        EffectComposer composer(&project,{});composer.addPiece(asset);
        auto composition=composer.composition();composition.duration=4;composition.pieces[0].duration=4;composer.setComposition(composition);composer.show();
        auto effectPlay=composer.findChild<QPushButton *>("compositionPlay");
        auto effectLoop=composer.findChild<QCheckBox *>("compositionPlaybackLoop");auto time=composer.findChild<QSlider *>("compositionTime");
        QVERIFY(effectPlay&&effectLoop&&time);QVERIFY(!effectLoop->isChecked());
        effectPlay->click();QTRY_COMPARE_WITH_TIMEOUT(effectPlay->text(),QString("Play"),2000);QCOMPARE(time->value(),3);
        effectPlay->click();QCOMPARE(time->value(),0);effectPlay->click();
        effectLoop->setChecked(true);effectPlay->click();QTest::qWait(300);QCOMPARE(effectPlay->text(),QString("Pause"));
        effectPlay->click();QCOMPARE(composer.composition(),composition);composer.hide();

        project.loadAttacks();AttackEffectEditor effect(&project,63,2);AttackVisual visual;visual.frames=asset;
        effect.setVisual(visual);effect.show();
        auto clipPlay=effect.findChild<QPushButton *>("effectPlay");auto clipLoop=effect.findChild<QCheckBox *>("effectLoop");
        QVERIFY(clipPlay&&clipLoop);QVERIFY(!clipLoop->isChecked());
        QTRY_VERIFY_WITH_TIMEOUT(clipPlay->isEnabled(),20000);QTRY_COMPARE_WITH_TIMEOUT(clipPlay->text(),QString("Play"),3000);
        clipPlay->click();QVERIFY(effect.findChild<QLabel *>("effectFramePosition")->text().startsWith("Frame 1 /"));clipPlay->click();
        clipLoop->setChecked(true);clipPlay->click();QTest::qWait(300);QCOMPARE(clipPlay->text(),QString("Pause"));
        clipLoop->setChecked(false);QTRY_COMPARE_WITH_TIMEOUT(clipPlay->text(),QString("Play"),2000);
        QCOMPARE(effect.visual(),visual);
    }
    void fullEffectSequenceExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadAttacks();
            QUndoStack undo;
            AttackEffectEditor dialog(&project, 63, 2);
            dialog.show();
            QImage red(4, 4, QImage::Format_RGBA8888);
            red.fill(QColor(255, 0, 0, 128));
            AttackVisual visual;
            visual.frames = SpriteAsset::fromImage("Two frames", red);
            visual.frames->anchor = {2, 2};
            visual.frames->provenance["effectRgba32"] = true;
            visual.frames->frames[0].ticks = 2;
            auto frame = visual.frames->frames[0];
            frame.id = "green";
            frame.ticks = 3;
            frame.layers[0].image.fill(Qt::green);
            visual.frames->frames << frame;
            dialog.setVisual(visual);
            QTRY_VERIFY_WITH_TIMEOUT(
                dialog.findChild<QPushButton *>("effectExportFrame")->isEnabled(), 20000);
            const auto sequence = dialog.renderedSequence();
            QCOMPARE(sequence.size, QSize(320, 240));
            QTemporaryDir folder;
            QVERIFY(folder.isValid());
            const auto zip = folder.filePath("effect.zip");
            QVERIFY(dialog.exportRenderedSequence(zip, EffectExport::FramesZip));
            const auto read = importAnimationFiles({zip});
            QCOMPARE(read.frames.size(), sequence.frames.size());
            for (int i = 0; i < read.frames.size(); ++i) {
                QCOMPARE(read.frames[i].ticks, sequence.frames[i].ticks);
                QCOMPARE(read.composite(i), sequence.composite(i));
            }
            const auto png = folder.filePath("effect.png");
            QVERIFY(dialog.exportRenderedSequence(png, EffectExport::AnimatedPng));
            QFile file(png);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QVERIFY(file.readAll().contains("acTL"));
            if (!QStandardPaths::findExecutable("ffmpeg").isEmpty()) {
                const auto webm = folder.filePath("effect.webm");
                QVERIFY(dialog.exportRenderedSequence(webm, EffectExport::Webm));
                QVERIFY(QFileInfo(webm).size() > 100);
                QProcess decode;
                decode.start("ffmpeg",
                             {"-v", "error", "-i", png, "-f", "rawvideo", "-pix_fmt", "rgba", "-"});
                QVERIFY(decode.waitForFinished(20000));
                QCOMPARE(decode.exitCode(), 0);
                auto pixels = decode.readAllStandardOutput();
                QByteArray expected;
                for (int i = 0; i < sequence.frames.size(); ++i) {
                    const auto image =
                        sequence.composite(i).convertToFormat(QImage::Format_RGBA8888);
                    const QByteArray bytes(reinterpret_cast<const char *>(image.constBits()),
                                           image.sizeInBytes());
                    for (int j = 0; j < sequence.frames[i].ticks; ++j)
                        expected += bytes;
                }
                QCOMPARE(pixels, expected);
            }
            QVERIFY_THROWS_EXCEPTION(Error,
                                     dialog.exportRenderedSequence(path, EffectExport::FramesZip));
            auto data = *project.attacks();
            auto healing = data.entry(63); healing.targetSide = 2; healing.targetPattern = 1;
            data.set(63, healing); project.setAttacks(data.json());
            visual.frames->provenance["effectPlacement"] = 1;
            dialog.setVisual(visual);
            QTRY_VERIFY_WITH_TIMEOUT(dialog.findChild<QPushButton *>("effectExportFrame")->isEnabled(),20000);
            const auto healed = dialog.renderedSequence().composite(0);
            QCOMPARE(healed.pixelColor(220,143),QColor(255,0,0,128));
            QCOMPARE(healed.pixelColor(100,143).alpha(),0);
            QCOMPARE(healed.pixelColor(220,148).alpha(),0);
            dialog.hide();
            EffectLibraryEditor library(&project, &undo, [] {}, nullptr, true);
            library.resize(1050, 720);
            library.show();
            library.selectEffect(29);
            auto pieces = library.findChild<QListWidget *>("effectLibraryPieces");
            QTRY_VERIFY_WITH_TIMEOUT(pieces->count() > 1, 20000);
            pieces->setCurrentRow(pieces->count() - 1);
            QVERIFY(pieces->currentItem()->text().startsWith("Full rendered animation"));
            const auto fenrir = library.selectedPiece();
            QVERIFY(fenrir.frames.size() > 10);
            QVERIFY(fenrir.pixels() <= 4 * 1024 * 1024);
            int ticks = 0;
            for (const auto &f : fenrir.frames)
                ticks += f.ticks;
            QCOMPARE(ticks, 278);
            EffectComposer composer(&project, {});
            composer.addPiece(fenrir);
            QCOMPARE(composer.composition().pieces.size(), 1);
            QCOMPARE(composer.composition().pieces[0].duration, 278);
            QVERIFY(library.grab().save("native/build/fenrir-library.png"));
            const auto fullZip = folder.filePath("fenrir.zip");
            QVERIFY(library.exportSequence(fullZip, EffectExport::FramesZip));
            const auto full = importAnimationFiles({fullZip});
            QCOMPARE(full.size, QSize(320, 240));
            int fullTicks = 0;
            for (const auto &f : full.frames)
                fullTicks += f.ticks;
            QCOMPARE(fullTicks, 278);
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void composedEffectWorkflow() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        try {
            applyEditorTheme(*qobject_cast<QApplication *>(QCoreApplication::instance()));
            auto project=Project::fromRom(Rom::open(path));QUndoStack undo;
            EffectLibraryEditor browser(&project,&undo,[]{},nullptr,true);browser.resize(1050,720);browser.show();browser.selectEffect(8);
            auto pieces=browser.findChild<QListWidget *>("effectLibraryPieces");QTRY_VERIFY_WITH_TIMEOUT(pieces->count()>0,20000);
            const auto fire=browser.selectedPiece();browser.selectEffect(20);
            QTRY_VERIFY_WITH_TIMEOUT(pieces->count()>0,20000);auto poison=browser.selectedPiece();
            // Retail programs share some textures. Choose genuinely different artwork.
            for(int row=0;row<pieces->count() && fire.frames[0].layers[0].image==poison.frames[0].layers[0].image;++row){pieces->setCurrentRow(row);poison=browser.selectedPiece();}
            QVERIFY(fire.frames[0].layers[0].image!=poison.frames[0].layers[0].image);browser.hide();
            EffectComposer dialog(&project,{});dialog.show();QVERIFY(dialog.findChild<RecoveryDraft *>()->capture().isEmpty());dialog.addPiece(fire);dialog.addPiece(poison);
            QCOMPARE(dialog.composition().pieces.size(),2);
            auto placement = dialog.findChild<QComboBox *>("compositionPlacement");
            QVERIFY(placement);
            QCOMPARE(placement->currentData().toInt(), 1);
            placement->setCurrentIndex(placement->findData(3));
            QCOMPARE(dialog.composition().placement, 3);
            dialog.findChild<QPushButton *>("compositionUndo")->click();
            QCOMPARE(dialog.composition().placement, 1);
            dialog.findChild<QPushButton *>("compositionRedo")->click();
            QCOMPARE(dialog.composition().placement, 3);
            auto start=dialog.findChild<QSpinBox *>("compositionStart");auto fromX=dialog.findChild<QSpinBox *>("compositionFromX");auto toX=dialog.findChild<QSpinBox *>("compositionToX");
            start->setValue(40);fromX->setValue(-24);toX->setValue(24);
            QCOMPARE(dialog.composition().pieces[0].start,0);QCOMPARE(dialog.composition().pieces[0].from,QPoint());
            QCOMPARE(dialog.composition().pieces[1].start,40);QCOMPARE(dialog.composition().pieces[1].from.x(),-24);
            const auto before=dialog.composition();toX->setValue(40);dialog.findChild<QPushButton *>("compositionUndo")->click();QCOMPARE(dialog.composition(),before);
            dialog.findChild<QPushButton *>("compositionRedo")->click();QCOMPARE(dialog.composition().pieces[1].to.x(),40);
            // Typed values commit on Enter and invalid timing is rejected without losing the draft.
            start->setFocus();start->selectAll();QTest::keyClicks(start,"45");QTest::keyClick(start,Qt::Key_Return);QCOMPARE(dialog.composition().pieces[1].start,45);QVERIFY(dialog.isVisible());
            start->setValue(89);QCOMPARE(dialog.composition().pieces[1].start,89);
            QCOMPARE(dialog.composition().duration,119);
            dialog.findChild<QPushButton *>("compositionUndo")->click();
            QCOMPARE(dialog.composition().pieces[1].start,45);QCOMPARE(dialog.composition().duration,90);
            auto tabs=dialog.findChild<QTabWidget *>("compositionPieceTabs");QVERIFY(tabs);QCOMPARE(tabs->count(),3);
            auto replacement=poison;replacement.frames.append(replacement.frames[0]);replacement.frames[1].id=SpriteLibrary::newId();replacement.kind="sequence";
            const auto beforeReplace=dialog.composition().pieces[1];
            dialog.replacePiece(1,replacement);
            QCOMPARE(dialog.composition().pieces[1].from,beforeReplace.from);
            QCOMPARE(dialog.composition().pieces[1].to,beforeReplace.to);
            QCOMPARE(dialog.composition().pieces[1].start,beforeReplace.start);
            auto speed=dialog.findChild<QSpinBox *>("compositionSpeed");QVERIFY(speed->isEnabled());speed->setValue(50);
            QCOMPARE(dialog.composition().pieces[1].speedPercent,50);
            dialog.findChild<QPushButton *>("compositionUndo")->click();QCOMPARE(dialog.composition().pieces[1].speedPercent,100);
            dialog.findChild<QPushButton *>("compositionRedo")->click();QCOMPARE(dialog.composition().pieces[1].speedPercent,50);
            tabs->setCurrentIndex(1);
            dialog.findChild<QPushButton *>("compositionPlaceEnd")->click();
            auto canvas=dialog.findChild<QWidget *>("compositionCanvas");const double scale=std::min((canvas->width()-24)/320.0,(canvas->height()-24)/240.0);
            const auto point=canvas->rect().center()+QPoint(qRound(-24*scale),qRound(-12*scale));
            QTest::mousePress(canvas,Qt::LeftButton,Qt::NoModifier,point);QTest::mouseRelease(canvas,Qt::LeftButton,Qt::NoModifier,point);
            QVERIFY(std::abs(dialog.composition().pieces[1].to.x()+24)<=1);QVERIFY(std::abs(dialog.composition().pieces[1].to.y()+12)<=1);
            const auto model=dialog.composition();const auto asset=dialog.asset();
            QCOMPARE(EffectComposition::fromAsset(asset),model);
            const auto recovery=dialog.findChild<RecoveryDraft *>()->capture();QVERIFY(!recovery.isEmpty());
            validateRecoveryDrafts(project,QJsonArray{recovery});
            QVERIFY(dialog.grab().save("native/build/effect-composer.png"));
            auto library=project.sprites();library.replace(asset);undo.push(new SpriteLibraryCommand(&project,"Save effect",library));
            undo.undo();QVERIFY(project.sprites().assets().isEmpty());undo.redo();QCOMPARE(project.sprites().asset(asset.id),asset);
            const auto donor=project.attacks()->entry(55);
            const int id=assignComposedEffect(&project,&undo,asset,55,true);QCOMPARE(id,159);QCOMPARE(project.attacks()->entry(55),donor);
            const auto assigned=project.attacks()->entry(id);QVERIFY(assigned.visual.fullColorFrames());
            undo.undo();QCOMPARE(project.attacks()->entries().size(),159);undo.redo();QCOMPARE(project.attacks()->entry(id),assigned);
            auto updated=model;updated.pieces[0].to={12,10};auto changedLibrary=project.sprites();changedLibrary.replace(updated.bake());
            undo.push(new SpriteLibraryCommand(&project,"Edit saved effect",changedLibrary));QCOMPARE(project.attacks()->entry(id),assigned);
            QTemporaryDir folder;QVERIFY(folder.isValid());project.saveFolder(folder.path());
            auto reopened=Project::fromRom(Rom::open(path));reopened.applyManifest(Project::readProject(folder.path()));
            QCOMPARE(EffectComposition::fromAsset(reopened.sprites().asset(asset.id)),updated);
            QCOMPARE(reopened.attacks()->entry(id),assigned);
            EffectLibraryEditor gallery(&project,&undo,[]{});gallery.resize(1100,720);gallery.show();gallery.selectSavedEffect(asset.id);
            QVERIFY(gallery.findChild<QPushButton *>("effectLibraryEdit")->isEnabled());QVERIFY(gallery.findChild<QPushButton *>("effectLibraryAssign")->isEnabled());
            AttackEffectEditor attack(&project,id,2);attack.show();
            QCOMPARE(attack.findChild<QPushButton *>("composeAttackEffect")->text(),"Edit piecesâ€¦");
            QVERIFY(!attack.findChild<QListWidget *>("effectFrames")->isVisible());
            QTimer::singleShot(0,&attack,[&]{auto composer=attack.findChild<EffectComposer *>();QVERIFY(composer);QCOMPARE(composer->composition().pieces,model.pieces);composer->reject();});
            attack.findChild<QPushButton *>("composeAttackEffect")->click();QCOMPARE(attack.visual(),assigned.visual);
            dialog.reject();
        } catch(const std::exception &e){QFAIL(e.what());}
    }
    void effectLibraryArtworkWorkflow() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            applyEditorTheme(*qobject_cast<QApplication *>(QCoreApplication::instance()));
            auto project = Project::fromRom(Rom::open(path));
            QUndoStack undo;
            EffectLibraryEditor editor(&project, &undo, [] {});
            editor.resize(1100, 720); editor.show();
            editor.selectEffect(8);
            auto sources = editor.findChild<QListWidget *>("effectLibrarySources");
            auto pieces = editor.findChild<QListWidget *>("effectLibraryPieces");
            QCOMPARE(sources->count(), 133);
            QTRY_VERIFY_WITH_TIMEOUT(pieces->count() > 1, 20000);
            pieces->setCurrentRow(0);
            const auto key = pieces->currentItem()->data(Qt::UserRole).toUInt();
            const auto raw = retailEffectResource(project.rom(), key >> 16, key & 65535);
            const auto id = editor.saveArtwork("Fireball piece");
            const auto asset = project.sprites().asset(id);
            // Stored/raw PNG pixels retain RGB even where alpha is zero.
            // Compositing a layer correctly discards those invisible colours.
            const auto image = asset.frames[0].layers[0].image.convertToFormat(QImage::Format_RGBA8888);
            QCOMPARE(QByteArray(reinterpret_cast<const char *>(image.constBits()), image.sizeInBytes()), raw);
            QCOMPARE(asset.provenance["source"].toString(), "effect-artwork");
            undo.undo(); QVERIFY(project.sprites().assets().isEmpty());
            undo.redo(); QCOMPARE(project.sprites().asset(id), asset);
            QTemporaryDir folder; QVERIFY(folder.isValid());
            const auto png = folder.filePath("piece.png");
            editor.exportArtwork(png);
            QCOMPARE(QImage(png).convertToFormat(QImage::Format_RGBA8888), image);
            QVERIFY_THROWS_EXCEPTION(Error, editor.exportArtwork(path));
            // Compose a donor as a new layer without changing the saved piece.
            SpriteEditor artwork(&project, &undo, [] {});
            const auto base = SpriteAsset::blank("Mixed artwork", {64, 64});
            artwork.addAsset(base);
            artwork.addLibraryLayer(id);
            const auto mixed = project.sprites().asset(base.id);
            QCOMPARE(mixed.frames[0].layers.size(), 2);
            QCOMPARE(mixed.frames[0].layers.last().image, asset.composite(0));
            QCOMPARE(project.sprites().asset(id), asset);
            undo.undo(); QCOMPARE(project.sprites().asset(base.id), base);
            undo.redo(); QCOMPARE(project.sprites().asset(base.id), mixed);
            // The saved piece can replace a texture in a different native effect.
            AttackEffectEditor target(&project, 55, 2);
            auto targetVisual = target.visual(); targetVisual.nativeEffect = 9; target.setVisual(targetVisual);
            auto targetPieces = target.findChild<QListWidget *>("effectRetailArtwork");
            QTRY_VERIFY_WITH_TIMEOUT(targetPieces->count() > 0, 20000);
            const auto targetKey = targetPieces->item(0)->data(Qt::UserRole).toUInt();
            target.replaceArtwork(targetKey, project.sprites().asset(id).composite(0));
            targetVisual = target.visual(); targetVisual.artwork[targetKey].color = 0x00ff00;
            target.setVisual(targetVisual);
            auto data = *project.attacks(); auto a = data.entry(55);
            a.visual = target.visual(); data.set(55, a);
            data.rename(55, QString(110, 'A'), false);
            project.setAttacks(data.json());
            project.saveFolder(folder.filePath("project"));
            auto reopened = Project::fromRom(Rom::open(path));
            reopened.applyManifest(Project::readProject(folder.filePath("project")));
            QCOMPARE(reopened.sprites().asset(id), asset);
            QCOMPARE(reopened.sprites().asset(base.id), mixed);
            QCOMPARE(reopened.attacks()->entry(55).visual, target.visual());
            QCOMPARE(retailEffectResource(project.rom(), key >> 16, key & 65535), raw);
            QUndoStack editedUndo;
            EffectLibraryEditor edited(&reopened, &editedUndo, [] {});
            edited.resize(1100, 720); edited.show(); edited.selectAttack(55);
            auto editedPieces = edited.findChild<QListWidget *>("effectLibraryPieces");
            QTRY_VERIFY_WITH_TIMEOUT(editedPieces->count() > 0, 20000);
            bool found = false;
            for (int row = 0; row < editedPieces->count(); ++row)
                if (editedPieces->item(row)->data(Qt::UserRole).toUInt() == targetKey) {
                    editedPieces->setCurrentRow(row); found = true; break;
                }
            QVERIFY(found);
            const auto editedId = edited.saveArtwork();
            const auto editedAsset = reopened.sprites().asset(editedId);
            QVERIFY(editedAsset.name.size() <= 120);
            const auto &rgba = targetVisual.artwork[targetKey].rgba;
            const auto extracted = editedAsset.frames[0].layers[0].image.convertToFormat(QImage::Format_RGBA8888);
            QCOMPARE(extracted.sizeInBytes(), rgba.size());
            for (int at = 0; at < rgba.size(); at += 4) {
                const auto out = extracted.constBits() + at;
                QCOMPARE(out[0], uchar(0)); QCOMPARE(out[2], uchar(0));
                QCOMPARE(out[1], std::max({u8(rgba, at), u8(rgba, at + 1), u8(rgba, at + 2)}));
                QCOMPARE(out[3], u8(rgba, at + 3));
            }
            edited.hide();
            // Only geometry is reported as such, with no fake image to export.
            editor.selectEffect(94);
            QTRY_VERIFY_WITH_TIMEOUT(editor.findChild<QLabel *>("effectLibraryNotice")->text().contains("no image artwork"), 20000);
            QCOMPARE(pieces->count(), 0);
            QVERIFY(!editor.findChild<QPushButton *>("effectLibrarySave")->isEnabled());
            editor.selectEffect(8);
            QTRY_VERIFY_WITH_TIMEOUT(pieces->count() > 1, 20000);
            auto search = editor.findChild<QLineEdit *>("effectLibrarySearch");
            search->setText("Effect 8");
            QVERIFY(!sources->item(8)->isHidden()); QVERIFY(sources->item(1)->isHidden());
            search->clear();
            QVERIFY(editor.grab().save("native/build/effects-library.png"));
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void retailEffectAppearanceWorkflow() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadAttacks();
            auto data = *project.attacks();
            auto source = data.entry(63);
            source.visual.hue = 120; source.visual.sizePercent = 150;
            source.visual.offsetX = 12; source.visual.offsetY = -6;
            data.set(63, source);
            const auto target = data.create("New visual copy");
            project.setAttacks(data.json());
            AttackEffectEditor dialog(&project, target, 2);
            dialog.show();
            dialog.copyFromAttack(63);
            QCOMPARE(dialog.visual().nativeEffect, 8);
            QCOMPARE(dialog.visual().hue, 120);
            QCOMPARE(project.attacks()->entry(63), source);
            auto size = dialog.findChild<QSpinBox *>("effectSize");
            auto x = dialog.findChild<QSpinBox *>("effectOffsetX");
            auto y = dialog.findChild<QSpinBox *>("effectOffsetY");
            auto mode = dialog.findChild<QComboBox *>("effectColourMode");
            QVERIFY(size); QVERIFY(x); QVERIFY(y); QVERIFY(mode);
            size->setValue(175); x->setValue(-18); y->setValue(9);
            QCOMPARE(dialog.visual().sizePercent, 175);
            QCOMPARE(dialog.visual().offsetX, -18); QCOMPARE(dialog.visual().offsetY, 9);
            auto nativeTabs=dialog.findChild<QTabWidget *>("effectRetailTabs");QVERIFY(nativeTabs);nativeTabs->setCurrentIndex(1);
            auto motionMode=dialog.findChild<QComboBox *>("effectMotionMode");QVERIFY(motionMode);motionMode->setCurrentIndex(1);
            dialog.findChild<QSpinBox *>("effectMotionX")->setValue(25);
            dialog.findChild<QSpinBox *>("effectMotionTicks")->setValue(60);
            dialog.findChild<QSpinBox *>("effectMotionStart")->setValue(10);
            dialog.findChild<QComboBox *>("effectMotionEasing")->setCurrentIndex(3);
            QCOMPARE(dialog.visual().motionTicks,60);QCOMPARE(dialog.visual().motionX,25);QCOMPARE(dialog.visual().motionEasing,3);
            const auto movedVisual=dialog.visual();motionMode->setCurrentIndex(0);QCOMPARE(dialog.visual().motionTicks,0);
            dialog.findChild<QPushButton *>("effectUndo")->click();QCOMPARE(dialog.visual(),movedVisual);
            nativeTabs->setCurrentIndex(0);
            auto clearLegacy = dialog.findChild<QPushButton *>("effectClearLegacyColour");
            QVERIFY(clearLegacy->isVisible());
            clearLegacy->click();
            QCOMPARE(dialog.visual().hue, 0); QCOMPARE(dialog.visual().color, -1);
            auto list = dialog.findChild<QListWidget *>("effectRetailArtwork");
            QVERIFY(list);
            QTRY_VERIFY_WITH_TIMEOUT(list->count() > 1, 20000);
            list->setCurrentRow(0);
            const auto key = list->item(0)->data(Qt::UserRole).toUInt();
            const auto secondKey = list->item(1)->data(Qt::UserRole).toUInt();
            mode->setCurrentIndex(2);
            QCOMPARE(dialog.visual().artwork[key].color, 0x4080ff);
            QVERIFY(!dialog.visual().artwork.contains(secondKey));
            list->setCurrentRow(1);
            QCOMPARE(mode->currentIndex(), 0);
            mode->setCurrentIndex(1);
            dialog.findChild<QSpinBox *>("effectHue")->setValue(30);
            QCOMPARE(dialog.visual().artwork[secondKey].hue, 30);
            QCOMPARE(dialog.visual().artwork[key].color, 0x4080ff);
            list->setCurrentRow(0);
            QCOMPARE(mode->currentIndex(), 2);
            mode->setCurrentIndex(0);
            QVERIFY(!dialog.visual().artwork.contains(key));
            QCOMPARE(dialog.visual().artwork[secondKey].hue, 30);
            dialog.findChild<QPushButton *>("effectUndo")->click();
            QCOMPARE(dialog.visual().artwork[key].color, 0x4080ff);
            QCOMPARE(dialog.visual().hue, 0); QCOMPARE(dialog.visual().color, -1);
            QImage image(8, 8, QImage::Format_RGBA8888);
            image.fill(Qt::green);
            const auto before = dialog.visual();
            dialog.replaceArtwork(key, image);
            QVERIFY(dialog.visual().artwork.contains(key));
            QCOMPARE(dialog.visual().artwork[key].color, 0x4080ff);
            const auto replaced = dialog.visual();
            dialog.findChild<QPushButton *>("effectUndo")->click();
            QCOMPARE(dialog.visual(), before);
            dialog.findChild<QPushButton *>("effectRedo")->click();
            QCOMPARE(dialog.visual(), replaced);
            QCOMPARE(AttackVisual::fromJson(dialog.visual().json()), replaced);
            QCOMPARE(project.attacks()->entry(63), source);
            QVERIFY(project.attacks()->entry(target).visual == AttackVisual{});
            const auto preview = dialog.findChild<QLabel *>("attackEffectPreview");
            QTRY_VERIFY_WITH_TIMEOUT(!preview->pixmap().isNull(), 20000);
            auto play = dialog.findChild<QPushButton *>("effectPlay");
            if (play->text() == "Pause") play->click();
            auto previewLoop=dialog.findChild<QCheckBox *>("effectLoop");QVERIFY(previewLoop);
            previewLoop->setChecked(false);
            int duration=0;for(const auto &frame:dialog.renderedSequence().frames)duration+=frame.ticks;
            for(int i=0;i<duration;++i)dialog.findChild<QPushButton *>("effectStep")->click();
            play->click();play->click();
            auto framePosition = dialog.findChild<QLabel *>("effectFramePosition");
            QVERIFY(framePosition->text().startsWith("Frame 1 /"));
            auto step = dialog.findChild<QPushButton *>("effectStep");
            step->click();
            QVERIFY(framePosition->text().startsWith("Frame 2 /"));
            QCOMPARE(play->text(), "Play");
            for (int i = 0; i < 12; ++i) step->click();
            QVERIFY(framePosition->text().startsWith("Frame 14 /"));
            const auto rendered = dialog.renderedFrame();
            QCOMPARE(rendered.size(), QSize(320, 240));
            QTemporaryDir exportFolder; QVERIFY(exportFolder.isValid());
            const auto framePath = exportFolder.filePath("render.png");
            dialog.exportRenderedFrame(framePath);
            QCOMPARE(QImage(framePath).convertToFormat(QImage::Format_RGBA8888), rendered.convertToFormat(QImage::Format_RGBA8888));
            QVERIFY_THROWS_EXCEPTION(Error, dialog.exportRenderedFrame(path));
            QVERIFY(dialog.grab().save("native/build/retail-effect-appearance.png"));
            for (int i=14;i<55;++i) step->click();
            nativeTabs->setCurrentIndex(1);
            QVERIFY(dialog.grab().save("native/build/retail-effect-motion.png"));
            dialog.reject();
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void dropdownWheelRequiresClick() {
        MainWindow window;
        window.show();
        QScrollArea page(&window);
        page.setGeometry(20, 100, 360, 240);
        page.setWidgetResizable(true);
        auto body = new QWidget;
        auto layout = new QVBoxLayout(body);
        auto combo = new QComboBox;
        for (int i = 0; i < 60; ++i)
            combo->addItem(QString("Choice %1").arg(i));
        combo->setCurrentIndex(2);
        layout->addWidget(combo);
        auto other = new QLineEdit;
        layout->addWidget(other);
        layout->addStretch();
        body->setMinimumHeight(1600);
        page.setWidget(body);
        page.show();
        QTest::qWait(30);
        auto wheel = [](QWidget *target) {
            const auto point = target->rect().center();
            QWheelEvent event(point, target->mapToGlobal(point), {}, {0, -120},
                              Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(target, &event);
        };
        for (bool focused : {false, true}) {
            page.verticalScrollBar()->setValue(0);
            (focused ? static_cast<QWidget *>(combo) : static_cast<QWidget *>(other))->setFocus();
            QCOMPARE(combo->hasFocus(), focused);
            QSignalSpy changes(combo, &QComboBox::currentIndexChanged);
            wheel(combo);
            QCOMPARE(combo->currentIndex(), 2);
            QCOMPARE(changes.count(), 0);
            QVERIFY(page.verticalScrollBar()->value() > 0);
        }
        page.verticalScrollBar()->setValue(0);
        combo->setFocus();
        QTest::keyClick(combo, Qt::Key_Down);
        QCOMPARE(combo->currentIndex(), 3);
        QTest::mouseClick(combo, Qt::LeftButton, Qt::NoModifier, combo->rect().center());
        auto view = combo->view();
        QTRY_VERIFY(view->isVisible());
        QVERIFY(view->verticalScrollBar()->maximum() > 0);
        view->verticalScrollBar()->setValue(0);
        wheel(view->viewport());
        QVERIFY(view->verticalScrollBar()->value() > 0);
        QCOMPARE(combo->currentIndex(), 3);
        const auto choice = view->indexAt(view->viewport()->rect().center());
        QVERIFY(choice.isValid());
        QVERIFY(choice.row() != 3);
        QTest::mouseMove(view->viewport(), view->visualRect(choice).center());
        QTest::qWait(QApplication::doubleClickInterval() + 1);
        QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                          view->visualRect(choice).center());
        QCOMPARE(combo->currentIndex(), choice.row());
        QTRY_VERIFY(combo->hasFocus());
        wheel(combo);
        QCOMPARE(combo->currentIndex(), choice.row() + 1);
        other->setFocus(Qt::TabFocusReason);
        combo->setFocus(Qt::TabFocusReason);
        const auto chosen = combo->currentIndex();
        wheel(combo);
        QCOMPARE(combo->currentIndex(), chosen);
        QDialog dialog(&window);
        auto form = new QVBoxLayout(&dialog);
        auto nested = new QComboBox;
        nested->addItems({"One", "Two", "Three"});
        nested->setCurrentIndex(1);
        form->addWidget(nested);
        dialog.show();
        dialog.activateWindow();
        nested->setFocus();
        QTRY_VERIFY(nested->hasFocus());
        wheel(nested);
        QCOMPARE(nested->currentIndex(), 1);
        QTest::mouseClick(nested, Qt::LeftButton, Qt::NoModifier, nested->rect().center());
        QTest::keyClick(nested->view(), Qt::Key_Escape);
        QTRY_VERIFY(nested->hasFocus());
        wheel(nested);
        QCOMPARE(nested->currentIndex(), 2);
    }
    void attackEffectWorkflow() {
        applyEditorTheme(*qobject_cast<QApplication *>(QCoreApplication::instance()));
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadAttacks();
        const auto original = project.attacks()->entry(55);
        QImage image(24, 24, QImage::Format_RGBA8888);
        image.fill(QColor(246, 80, 17, 255));
        auto asset = SpriteAsset::fromImage("Effect source", image);
        asset.anchor = {12, 12};
        asset.frames[0].ticks = 3;
        auto library = project.sprites();
        library.replace(asset);
        project.setSprites(library);
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const auto png = folder.filePath("effect.png");
        QVERIFY(image.save(png));
        QUndoStack undo;
        AttackEditor editor(&project, &undo, [] {});
        editor.resize(1300, 900);
        editor.show();
        editor.selectAttack(55);
        bool edited = false;
        QTimer::singleShot(0, &editor, [&] {
            auto dialog = editor.findChild<AttackEffectEditor *>("attackEffectEditor");
            QVERIFY(dialog);
            auto close = qScopeGuard([&] { dialog->reject(); });
            try {
                auto playerView = dialog->findChild<QPushButton *>("effectPreviewPlayer");
                auto enemyView = dialog->findChild<QPushButton *>("effectPreviewEnemy");
                QVERIFY(playerView); QVERIFY(enemyView); QVERIFY(playerView->isChecked());
                enemyView->click(); QVERIFY(enemyView->isChecked());
                QCOMPARE(dialog->visual(), original.visual);
                playerView->click(); QVERIFY(playerView->isChecked());
                dialog->importLibrary(asset.id);
                QVERIFY(dialog->visual().frames);
                QCOMPARE(project.attacks()->entry(55), original);
                dialog->findChild<QSpinBox *>("effectDelay")->setValue(2);
                dialog->findChild<QSpinBox *>("effectFrameTicks")->setValue(7);
                QCOMPARE(dialog->visual().frames->frames[0].ticks, 7);
                dialog->findChild<QPushButton *>("effectUndo")->click();
                QCOMPARE(dialog->visual().frames->frames[0].ticks, 3);
                dialog->findChild<QPushButton *>("effectRedo")->click();
                QCOMPARE(dialog->visual().frames->frames[0].ticks, 7);
                const auto pending = captureRecoveryDrafts(&editor, project);
                QVERIFY(!pending.isEmpty());
                auto recovered = Project::fromRom(Rom::open(path));
                QUndoStack recoveryUndo;
                restoreRecoveryDrafts(&recovered, &recoveryUndo, pending);
                QCOMPARE(recovered.attacks()->entry(55).visual, dialog->visual());
                recoveryUndo.undo();
                QCOMPARE(recovered.attacks()->entry(55), original);
                bool painted = false;
                QTimer::singleShot(0, dialog, [&] {
                    auto painting = dialog->findChild<QDialog *>("attackEffectFramePaint");
                    QVERIFY(painting);
                    auto closePaint = qScopeGuard([&] { painting->reject(); });
                    auto canvas = painting->findChild<SpriteCanvas *>();
                    QVERIFY(canvas);
                    auto blue = image;
                    blue.fill(Qt::blue);
                    canvas->committed(blue, {});
                    const auto unfinished = captureRecoveryDrafts(&editor, project);
                    QCOMPARE(unfinished.size(), 2);
                    auto restoredDraft = Project::fromRom(Rom::open(path));
                    QUndoStack draftUndo;
                    restoreRecoveryDrafts(&restoredDraft, &draftUndo, unfinished);
                    QCOMPARE(restoredDraft.attacks()->entry(55).visual.frames->composite(0), blue);
                    painted = true;
                });
                dialog->findChild<QPushButton *>("effectPaintFrame")->click();
                QVERIFY(painted);
                QCOMPARE(dialog->visual().frames->composite(0), image);
                dialog->importFiles({png});
                QCOMPARE(dialog->visual().frames->composite(0), image);
                const auto canvas = dialog->findChild<QLabel *>("attackEffectPreview");
                QVERIFY(canvas);
                QTRY_VERIFY_WITH_TIMEOUT(!canvas->pixmap().isNull(), 10000);
                QVERIFY(dialog->grab().save("native/build/attack-effects-editor.png"));
                edited = true;
                close.dismiss();
                dialog->accept();
            } catch (const std::exception &e) {
                QFAIL(e.what());
            }
        });
        editor.findChild<QPushButton *>("attackEditEffect")->click();
        QVERIFY(edited);
        QVERIFY(project.attacks()->entry(55).visual.frames);
        QCOMPARE(project.attacks()->entry(56).visual, AttackVisual{});
        QCOMPARE(project.sprites().asset(asset.id), asset);
        const auto applied = project.attacks()->entry(55);
        undo.undo();
        QCOMPARE(project.attacks()->entry(55), original);
        undo.redo();
        QCOMPARE(project.attacks()->entry(55), applied);
        QTimer::singleShot(0, &editor, [&] {
            auto d = editor.findChild<AttackEffectEditor *>("attackEffectEditor");
            QVERIFY(d);
            auto changed = d->visual();
            changed.delay = 17;
            d->setVisual(changed);
            d->reject();
        });
        editor.findChild<QPushButton *>("attackEditEffect")->click();
        QCOMPARE(project.attacks()->entry(55), applied);
        project.saveFolder(folder.path());
        auto restored = Project::fromRom(Rom::open(path));
        restored.applyManifest(Project::readProject(folder.filePath("project.json")));
        QCOMPARE(restored.attacks()->entry(55), applied);
    }
    void attackMissingOptionsEditing() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");
        if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));project.loadAttacks();
        auto data=*project.attacks();const int id=data.duplicate(55,"New magic"),array=data.duplicate(45,"Own array");
        project.setAttacks(data.json());QUndoStack undo;AttackEditor editor(&project,&undo,[]{});
        auto select=[&](const char *name,int value) {
            auto box=editor.findChild<QComboBox *>(name);QVERIFY(box);QVERIFY(box->findData(value)>=0);
            box->setCurrentIndex(box->findData(value));emit box->activated(box->currentIndex());QCoreApplication::processEvents();
        };
        editor.selectAttack(id);select("attackPattern",10);select("attackStatus",7);
        QCOMPARE(project.attacks()->entry(id).targetPattern,10);QCOMPARE(project.attacks()->entry(id).status,7);
        QCOMPARE(project.attacks()->entry(id).condition,2);
        QVERIFY(!editor.findChild<QComboBox *>("attackCondition")->isEnabled());
        undo.undo();QCoreApplication::processEvents();QVERIFY(project.attacks()->entry(id).status!=7);
        undo.redo();QCoreApplication::processEvents();QCOMPARE(project.attacks()->entry(id).status,7);
        select("attackStatus",10);QCOMPARE(project.attacks()->entry(id).condition,1);
        QVERIFY(editor.findChild<QComboBox *>("attackCondition")->isEnabled());
        undo.undo();QCoreApplication::processEvents();QCOMPARE(project.attacks()->entry(id).condition,2);
        undo.redo();QCoreApplication::processEvents();QCOMPARE(project.attacks()->entry(id).condition,1);
        QVERIFY(editor.findChild<QLabel *>("attackPlayback")->text().contains("one sequence"));
        editor.selectAttack(4);select("attackPattern",6);
        QVERIFY(editor.findChild<QLabel *>("attackPlayback")->text().contains("three strikes"));
        editor.selectAttack(array);select("attackSpellLink1",id);
        QCOMPARE(project.attacks()->resolvedTargets(array).value(1),id);
        QCOMPARE(project.attacks()->resolvedTargets(45).value(1),55);
        const auto after=project.manifest();
        undo.undo();QCoreApplication::processEvents();QCOMPARE(project.attacks()->resolvedTargets(array).value(1),55);
        undo.redo();QCoreApplication::processEvents();QCOMPARE(project.manifest(),after);
        auto restored=Project::fromRom(Rom::open(path));restored.applyManifest(after);QCOMPARE(restored.attacks()->json(),project.attacks()->json());
        auto knight=project.record(Table::Classes,5).bytes;put8(knight,55,array);
        project.replaceBytes(Table::Classes,5,knight);editor.selectAttack(id);
        auto users=editor.findChild<QTableWidget *>("attackUsers");bool found=false;
        for(int row=0;row<users->rowCount();row++)
            found|=users->item(row,0)->data(Qt::UserRole).toInt()==5 && users->item(row,0)->text().contains("via spell array");
        QVERIFY(found);
        QTimer::singleShot(0,[&]{
            auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());QVERIFY(dialog);
            auto kind=dialog->findChild<QComboBox *>("attackNewKind");QVERIFY(kind);
            kind->setCurrentIndex(kind->findData(1));
            auto starter=dialog->findChild<QComboBox *>("attackStartingPoint");
            starter->setCurrentIndex(starter->findData(array));
            dialog->findChild<QLineEdit *>("attackNewName")->setText("Independent spell array");dialog->accept();
        });
        editor.findChild<QPushButton *>("createAttack")->click();QCoreApplication::processEvents();
        const int created=project.attacks()->entries().size()-1;
        QCOMPARE(created,161);QCOMPARE(project.attacks()->entry(created).behavior,45);
        QCOMPARE(project.attacks()->resolvedTargets(created).value(1),id);
        select("attackSpellLink1",63);
        QCOMPARE(project.attacks()->resolvedTargets(created).value(1),63);
        QCOMPARE(project.attacks()->resolvedTargets(array).value(1),id);
    }
    void attackRepeatedStrikes() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        QUndoStack undo;
        AttackEditor editor(&project, &undo, [] {});
        auto pattern = editor.findChild<QComboBox *>("attackPattern");
        auto count = editor.findChild<QSpinBox *>("attackStrikeCount");
        QVERIFY(pattern->findData(11) >= 0);
        QCOMPARE(count->minimum(), 1); QCOMPARE(count->maximum(), 18);
        editor.selectAttack(76); // Retail Gnome: opening the control changes nothing.
        const auto before = project.manifest();
        QVERIFY(!count->parentWidget()->isHidden()); QCOMPARE(count->value(), 6);
        emit count->editingFinished(); QCOMPARE(project.manifest(), before);
        count->setValue(3); emit count->editingFinished(); QCoreApplication::processEvents();
        QCOMPARE(project.attacks()->entry(76).targetPattern, 11);
        QCOMPARE(project.attacks()->entry(76).strikeCount, 3);
        undo.undo(); QCoreApplication::processEvents(); QCOMPARE(project.manifest(), before);
        QCOMPARE(count->value(), 6);
        undo.redo(); QCoreApplication::processEvents(); QCOMPARE(count->value(), 3);
        auto restored = Project::fromRom(Rom::open(path)); restored.applyManifest(project.manifest());
        QCOMPARE(restored.attacks()->entry(76), project.attacks()->entry(76));
        pattern->setCurrentIndex(pattern->findData(0)); emit pattern->activated(pattern->currentIndex());
        QCoreApplication::processEvents(); QCOMPARE(count->value(), 6);
        const auto retailAgain = project.manifest();
        emit count->editingFinished(); QCOMPARE(project.manifest(), retailAgain);
        editor.selectAttack(4); // New option is available for ordinary physical attacks too.
        QVERIFY(count->parentWidget()->isHidden());
        pattern->setCurrentIndex(pattern->findData(11)); emit pattern->activated(pattern->currentIndex());
        QCoreApplication::processEvents(); QVERIFY(!count->parentWidget()->isHidden());
        QCOMPARE(count->value(), 6);
        count->setValue(18); emit count->editingFinished(); QCoreApplication::processEvents();
        QCOMPARE(project.attacks()->entry(4).strikeCount, 18);
        pattern->setCurrentIndex(pattern->findData(10)); emit pattern->activated(pattern->currentIndex());
        QCoreApplication::processEvents(); QVERIFY(count->parentWidget()->isHidden());
        pattern->setCurrentIndex(pattern->findData(11)); emit pattern->activated(pattern->currentIndex());
        QCoreApplication::processEvents(); QCOMPARE(count->value(), 18);
        const auto screenshot = qEnvironmentVariable("LORDLY_ATTACK_SCREENSHOT");
        if (!screenshot.isEmpty()) {
            editor.resize(1160, 850);editor.show();QTest::qWait(30);
            QVERIFY(editor.grab().save(screenshot));
        }
    }
    void attackPatternSwitching() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadAttacks();
        auto attacks = *project.attacks();
        const int id = attacks.duplicate(124, "Liquification");
        auto attack = attacks.entry(id);
        attack.targetPattern = 7;
        attack.targetSide = 1;
        attacks.set(id, attack);
        project.setAttacks(attacks.json());
        QUndoStack undo;
        AttackEditor editor(&project, &undo, [] {});
        editor.selectAttack(id);
        auto pattern = editor.findChild<QComboBox *>("attackPattern");
        auto side = editor.findChild<QComboBox *>("attackSide");
        const auto select = [&](QComboBox *box, int value) {
            box->setCurrentIndex(box->findData(value));
            emit box->activated(box->currentIndex());
            QCoreApplication::processEvents();
        };
        select(pattern, 9);
        QCOMPARE(project.attacks()->entry(id).targetSide, 1); // Effective self, retained opponent choice.
        QCOMPARE(side->currentData().toInt(), 3);
        QVERIFY(!side->isEnabled());
        select(pattern, 7);
        QCOMPARE(project.attacks()->entry(id).targetSide, 1);
        QCOMPARE(side->currentData().toInt(), 1);
        QVERIFY(side->isEnabled());
        undo.undo(); QCoreApplication::processEvents();
        QCOMPARE(project.attacks()->entry(id).targetPattern, 9);
        QCOMPARE(side->currentData().toInt(), 3);
        undo.redo(); QCoreApplication::processEvents();
        QCOMPARE(project.attacks()->entry(id).targetPattern, 7);
        QCOMPARE(project.attacks()->entry(id).targetSide, 1);
        select(side, 0);
        select(pattern, 9);
        select(pattern, 6);
        QCOMPARE(project.attacks()->entry(id).targetSide, 0);
        QVERIFY(side->currentText().startsWith("Opponents"));
        // Old projects contain the redundant pair 9/3. Leaving it resets to auto.
        select(side, 3);
        select(pattern, 9);
        const auto legacy = project.manifest();
        select(pattern, 7);
        QCOMPARE(project.attacks()->entry(id).targetSide, 0);
        const auto corrected = project.manifest();
        undo.undo(); QCoreApplication::processEvents();
        QCOMPARE(project.manifest(), legacy);
        undo.redo(); QCoreApplication::processEvents();
        QCOMPARE(project.manifest(), corrected);
        auto reopened = Project::fromRom(Rom::open(path));
        reopened.applyManifest(corrected);
        QCOMPARE(reopened.attacks()->entry(id).targetSide, 0);
        QCOMPARE(reopened.attacks()->entry(id).targetPattern, 7);
        // An explicit self side on a non-self pattern remains a deliberate edit.
        select(side, 3);
        select(pattern, 8);
        QCOMPARE(project.attacks()->entry(id).targetSide, 3);
    }
    void attackNativeRuleDisplay() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        QUndoStack undo;
        AttackEditor editor(&project, &undo, [] {});
        const auto before = project.manifest();
        const auto choice = [&](const char *name) { return editor.findChild<QComboBox *>(name); };
        const auto detail = [&](const char *name) { return editor.findChild<QLabel *>(name)->text(); };
        editor.selectAttack(4); // Slash: one opponent and weapon-dependent on-hit effect.
        QVERIFY(choice("attackSide")->currentText().startsWith("Opponents"));
        QVERIFY(choice("attackPattern")->currentText().startsWith("One target"));
        QVERIFY(choice("attackStatus")->currentText().startsWith("From equipped weapon"));
        QVERIFY(choice("attackAfter")->currentText().startsWith("None"));
        editor.selectAttack(21); // Mesmerize: a cross and Sleep on success, then half accuracy.
        QVERIFY(choice("attackPattern")->currentText().startsWith("Cross through target"));
        QVERIFY(choice("attackStatus")->currentText().startsWith("Sleep"));
        QVERIFY(detail("attackNativeStatus").contains("80% hit chance â†’ 40% effect chance"));
        editor.selectAttack(22); // Life Drain uses calculated amount, not clipped HP loss.
        QVERIFY(choice("attackAfter")->currentText().startsWith("Heal the attacker (drain)"));
        QVERIFY(detail("attackNativeAfter").contains("Last target's calculated amount"));
        editor.selectAttack(17);
        QVERIFY(choice("attackAfter")->currentText().startsWith("Damage the attacker (recoil)"));
        QVERIFY(detail("attackNativeAfter").contains("alternate physical power / 6"));
        editor.selectAttack(23); // Friendly and hostile Throw a Kiss must remain distinct.
        QVERIFY(choice("attackSide")->currentText().startsWith("Allies"));
        QVERIFY(choice("attackStatus")->currentText().startsWith("Power Up"));
        editor.selectAttack(24);
        QVERIFY(choice("attackSide")->currentText().startsWith("Opponents"));
        QVERIFY(choice("attackStatus")->currentText().startsWith("Power Down"));
        QVERIFY(detail("attackNativeStatus").contains("100% of eligible hits"));
        editor.selectAttack(28);
        QVERIFY(choice("attackStatus")->currentText().startsWith("Banish eligible undead"));
        editor.selectAttack(55);
        QVERIFY(choice("attackStatus")->currentText().startsWith("None"));
        editor.selectAttack(60);
        QVERIFY(choice("attackPattern")->currentText().startsWith("Six strikes"));
        editor.selectAttack(88);
        QVERIFY(choice("attackSide")->currentText().startsWith("Allies"));
        QVERIFY(choice("attackStatus")->currentText().startsWith("Remove ailments"));
        editor.selectAttack(125);
        QVERIFY(choice("attackStatus")->currentText().startsWith("Poison"));
        QVERIFY(detail("attackNativeStatus").contains("80% hit chance â†’ 20% effect chance"));
        editor.selectAttack(45);
        QCOMPARE(choice("attackPattern")->currentText(), QString("From resolved spell"));
        QVERIFY(!choice("attackPattern")->isEnabled());
        QCOMPARE(project.manifest(), before);
        QCOMPARE(undo.count(), 0);

        editor.selectAttack(21);
        auto status = choice("attackStatus");
        status->setCurrentIndex(status->findData(0));
        emit status->activated(status->currentIndex());
        QCoreApplication::processEvents();
        QCOMPARE(project.attacks()->entry(21).status, 0);
        undo.undo();
        QCoreApplication::processEvents();
        QVERIFY(status->currentText().startsWith("Sleep"));
        QCOMPARE(project.manifest(), before);

        // A main-effect edit must not falsely relabel the untouched native target rule.
        editor.selectAttack(55);
        auto effect = choice("attackEffect");
        effect->setCurrentIndex(effect->findData(2));
        emit effect->activated(effect->currentIndex());
        QCoreApplication::processEvents();
        QVERIFY(choice("attackSide")->currentText().startsWith("Opponents"));
        auto pattern = choice("attackPattern");
        pattern->setCurrentIndex(pattern->findData(2));
        emit pattern->activated(pattern->currentIndex());
        QCoreApplication::processEvents();
        QVERIFY(choice("attackSide")->currentText().startsWith("Allies"));
    }
    void attackWorkspaceEditing() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        applyEditorTheme(*qApp);auto project=Project::fromRom(Rom::open(path));QUndoStack undo;
        AttackEditor editor(&project,&undo,[]{});editor.resize(1400,940);editor.show();editor.selectAttack(4);
        auto power=editor.findChild<QSpinBox *>("attackPower");QVERIFY(power);power->setValue(175);emit power->editingFinished();
        QCOMPARE(project.attacks()->entry(4).powerPercent,175);undo.undo();QCOMPARE(project.attacks()->entry(4).powerPercent,100);undo.redo();QCOMPARE(project.attacks()->entry(4).powerPercent,175);QCoreApplication::processEvents();
        auto select=[&](const char *name,int value){auto box=editor.findChild<QComboBox *>(name);QVERIFY(box);const int index=box->findData(value);QVERIFY(index>=0);box->setCurrentIndex(index);emit box->activated(index);QCoreApplication::processEvents();};
        select("attackPattern",12);select("attackSide",2);QCOMPARE(project.attacks()->entry(4).targetMask,16);
        select("attackPattern",9);select("attackPattern",12);
        QCOMPARE(project.attacks()->entry(4).targetSide,2);
        editor.findChild<QCheckBox *>("attackCell0")->click();QCoreApplication::processEvents();QCOMPARE(project.attacks()->entry(4).targetMask,17);
        select("attackStatus",10);auto chance=editor.findChild<QSpinBox *>("attackChance");chance->setValue(35);emit chance->editingFinished();QCOMPARE(project.attacks()->entry(4).chance,35);
        select("attackAfter",1);select("attackAfterBasis",0);QCOMPARE(project.attacks()->entry(4).afterEffect,1);
        auto users=editor.findChild<QTableWidget *>("attackUsers");QVERIFY(users->rowCount()>0);const int classId=users->item(0,0)->data(Qt::UserRole).toInt();
        auto count=qobject_cast<QSpinBox *>(users->cellWidget(0,2));QVERIFY(count);const auto before=project.record(Table::Classes,classId).bytes;count->setValue(7);emit count->editingFinished();QCOMPARE(u8(project.record(Table::Classes,classId).bytes,56),quint8(7));undo.undo();QCOMPARE(project.record(Table::Classes,classId).bytes,before);QCoreApplication::processEvents();
        // Classes edits must become visible in this workspace after the shared undo signal.
        RecordModel classes(&project,Table::Classes,&undo);int front=-1;for(int i=0;i<fields(Table::Classes).size();++i)if(fields(Table::Classes)[i].key=="attack0")front=i;
        int otherClass=1;while(otherClass<project.records(Table::Classes).size()&&u8(project.record(Table::Classes,otherClass).bytes,55)==4)++otherClass;
        QVERIFY(otherClass<project.records(Table::Classes).size());QVERIFY(front>=0);QVERIFY(classes.setData(classes.index(otherClass,front+2),4,Qt::EditRole));QCoreApplication::processEvents();bool found=false;for(int i=0;i<users->rowCount();++i)found|=users->item(i,0)->data(Qt::UserRole).toInt()==otherClass;QVERIFY(found);
        QTimer::singleShot(0,[&]{auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());QVERIFY(dialog);dialog->findChild<QLineEdit *>("attackNewName")->setText("New spell");auto starter=dialog->findChild<QComboBox *>("attackStartingPoint");starter->setCurrentIndex(starter->findData(55));dialog->accept();});editor.findChild<QPushButton *>("createAttack")->click();QCoreApplication::processEvents();QCOMPARE(project.attacks()->entry(159).behavior,55);QCOMPARE(u8(project.attacks()->entry(159).data,0),quint8(3));undo.undo();QCoreApplication::processEvents();QCOMPARE(project.attacks()->entries().size(),159);editor.selectAttack(4);
        QTimer::singleShot(0,[&]{auto dialog=qobject_cast<QInputDialog *>(QApplication::activeModalWidget());QVERIFY(dialog);dialog->setTextValue("Independent test attack");dialog->accept();});editor.findChild<QPushButton *>("duplicateAttack")->click();QCoreApplication::processEvents();QCOMPARE(project.attacks()->entries().size(),160);QCOMPARE(AttackData::displayName(project.attacks()->entry(159)),QString("Independent test attack"));QCOMPARE(AttackData::displayName(project.attacks()->entry(4)),QString("Slash"));
        auto search=editor.findChild<QLineEdit *>("attackSearch");search->setText("Independent test");auto list=editor.findChild<QListWidget *>("attackList");int visible=0;for(int i=0;i<list->count();++i)visible+=!list->item(i)->isHidden();QCOMPARE(visible,1);search->clear();
        editor.selectAttack(159);QVERIFY(editor.grab().save("native/build/attacks-workspace.png"));
        auto scroll=editor.findChild<QScrollArea *>();QVERIFY(scroll);scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());QCoreApplication::processEvents();QVERIFY(editor.grab().save("native/build/attacks-workspace-bottom.png"));
        const auto beforePreview=project.manifest();editor.findChild<QPushButton *>("attackPreview")->click();QCoreApplication::processEvents();
        auto preview=editor.findChild<QDialog *>();QVERIFY(preview);QVERIFY(preview->isVisible());QVERIFY(preview->grab().save("native/build/attack-preview.png"));preview->close();QCOMPARE(project.manifest(),beforePreview);
        const auto json=project.manifest();auto reopened=Project::fromRom(Rom::open(path));reopened.applyManifest(json);QCOMPARE(reopened.attacks()->json(),project.attacks()->json());
    }
    void sharedAttackPlayback() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));QUndoStack undo;
        AttackEditor editor(&project,&undo,[]{});editor.selectAttack(4);
        auto box=editor.findChild<QComboBox *>("attackPlaybackMode");QVERIFY(box);QVERIFY(box->isEnabled());
        QCOMPARE(box->currentData().toInt(),0);
        box->setCurrentIndex(box->findData(1));emit box->activated(box->currentIndex());
        QCoreApplication::processEvents();QCOMPARE(project.attacks()->entry(4).playback,1);
        QVERIFY(project.changeReport().text().contains("One strike"));
        undo.undo();QCoreApplication::processEvents();QCOMPARE(project.attacks()->entry(4).playback,0);
        QCOMPARE(box->currentData().toInt(),0);
        undo.redo();QCoreApplication::processEvents();QCOMPARE(box->currentData().toInt(),1);
        auto reopened=Project::fromRom(Rom::open(path));reopened.applyManifest(project.manifest());
        QCOMPARE(reopened.attacks()->entry(4).playback,1);
        auto data=*reopened.attacks();const auto copy=data.duplicate(4,"Shared strike copy");
        QCOMPARE(data.entry(copy).playback,1);auto attack=data.entry(copy);attack.playback=0;data.set(copy,attack);
        QCOMPARE(data.entry(4).playback,1);attack.playback=2;QVERIFY_THROWS_EXCEPTION(Error,data.set(copy,attack));
        auto legacy=data.json();auto rows=legacy["entries"].toArray();auto row=rows[3].toObject();
        row.remove("playback");rows[3]=row;legacy["entries"]=rows;
        auto loaded=AttackData::loadRetail(Rom::open(path));loaded.apply(legacy);QCOMPARE(loaded.entry(4).playback,0);
        editor.selectAttack(55);QVERIFY(!box->isEnabled());
        editor.selectAttack(4);QCOMPARE(box->currentData().toInt(),1);
    }
    void attackAnimationNavigation() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        MainWindow window;window.resize(1400,940);window.show();window.loadRom(path);
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<AttackEditor *>(),30000);
        auto attacks=window.findChild<AttackEditor *>();window.navigateTo({"attacks",4});QVERIFY(attacks->isVisible());
        auto previewClass=attacks->findChild<QComboBox *>("attackPreviewClass");previewClass->setCurrentIndex(previewClass->findData(2));
        attacks->findChild<QPushButton *>("attackEditAnimation")->click();QCoreApplication::processEvents();
        auto action=window.findChild<QComboBox *>("combatAnimationAction");auto cls=window.findChild<QComboBox *>("combatAnimationClass");QVERIFY(action);QVERIFY(cls);
        QCOMPARE(cls->currentData().toInt(),2);QCOMPARE(action->currentData().toInt(),4);QVERIFY(action->isVisible());
    }
    void dataTableCopyAndCsv() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
        applyEditorTheme(*qApp);
        MainWindow window; window.loadRom(path); window.show();
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QTableView *>("classesTable"), 30000);
        window.navigateTo({"classes", 5});
        auto table = window.findChild<QTableView *>("classesTable"); QVERIFY(table);
        auto columns = window.findChild<QComboBox *>("classesColumnView"); QVERIFY(columns);
        const auto stats = tableCsv(*table);
        columns->setCurrentIndex(columns->findData("Growth"));
        auto search = window.findChild<QLineEdit *>("classesSearch"); QVERIFY(search);
        search->setText("Knight");
        QVERIFY(table->model()->rowCount() > 0);
        QVERIFY(table->model()->rowCount() < 100);
        const auto growth = tableCsv(*table);
        QVERIFY(growth != stats);
        QVERIFY(growth.toLower().contains("growth"));
        window.activateWindow();
        QTest::qWait(50);
        table->selectAll(); table->setFocus();
        QTRY_VERIFY2(table->hasFocus(), qPrintable(QString("visible=%1 enabled=%2 active=%3 focus=%4")
            .arg(table->isVisible()).arg(table->isEnabled())
            .arg(QApplication::activeWindow() ? QApplication::activeWindow()->objectName() : "none")
            .arg(QApplication::focusWidget() ? QApplication::focusWidget()->objectName() : "none")));
        QTest::keyClick(table, Qt::Key_C, Qt::ControlModifier);
        const auto copied = QApplication::clipboard()->text();
        QVERIFY(copied.contains('\t')); QVERIFY(copied.contains("Knight"));
        QCOMPARE(copied, tableSelectionText(*table));
        QTemporaryDir directory;
        const auto output = directory.filePath("growth.csv");
        QTimer::singleShot(0, [&] {
            auto picker = qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
            if (picker) { picker->selectFile(output); QMetaObject::invokeMethod(picker, "accept"); }
        });
        window.findChild<QPushButton *>("classesExportCsv")->click();
        QFile exported(output); QVERIFY(exported.open(QIODevice::ReadOnly));
        QCOMPARE(exported.readAll(), growth);
        QVERIFY(window.grab().save("native/build/data-table-export.png"));
    }
    void combatAffinityGroupEditing() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));project.loadTools();
        const int leader=project.createClass("Group leader");
        QUndoStack undo;LeaderAffinityEditor editor(&project,&undo,[]{});editor.selectClass(leader);
        const auto original=project.tools()->leaderAffinity().json();
        auto manage=editor.findChild<QPushButton *>("leaderAffinityManageGroups");QVERIFY(manage);
        QTimer::singleShot(0,[&] {
            auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());QVERIFY(dialog);
            QTimer::singleShot(0,[&] {
                auto prompt=qobject_cast<QInputDialog *>(QApplication::activeModalWidget());QVERIFY(prompt);
                prompt->setTextValue("Guardians");prompt->accept();
            });
            dialog->findChild<QPushButton *>("combatAffinityAddGroup")->click();
            auto groups=dialog->findChild<QListWidget *>("combatAffinityGroupsList");QVERIFY(groups);
            QCOMPARE(groups->count(),5);QCOMPARE(groups->currentRow(),4);
            auto members=dialog->findChild<QListWidget *>("combatAffinityMembers");QVERIFY(members);
            for(int i=0;i<members->count();++i)if(members->item(i)->data(Qt::UserRole).toInt()==0x45)
                members->item(i)->setCheckState(Qt::Checked);
            auto leaders=dialog->findChild<QTableWidget *>("combatAffinityLeaders");QVERIFY(leaders);
            for(int row=0;row<leaders->rowCount();++row)if(leaders->item(row,0)->data(Qt::UserRole).toInt()==leader) {
                leaders->item(row,0)->setCheckState(Qt::Checked);
                auto strength=qobject_cast<QSpinBox *>(leaders->cellWidget(row,1));QVERIFY(strength);QVERIFY(strength->isEnabled());strength->setValue(23);
            }
            // The modal edits a draft until confirmed.
            QCOMPARE(project.tools()->leaderAffinity().json(),original);dialog->accept();
        });
        manage->click();QCOMPARE(project.tools()->leaderAffinity().groupFor(0x45),4);
        QCOMPARE(project.tools()->leaderAffinity().strength(leader,4),23);QCOMPARE(undo.count(),1);
        QCOMPARE(editor.findChild<QSpinBox *>("leaderAffinityStrength4")->value(),23);
        const auto applied=project.tools()->leaderAffinity().json();
        undo.undo();QCOMPARE(project.tools()->leaderAffinity().json(),original);
        undo.redo();QCOMPARE(project.tools()->leaderAffinity().json(),applied);
        QTimer::singleShot(0,[&] {
            auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());QVERIFY(dialog);
            dialog->findChild<QListWidget *>("combatAffinityGroupsList")->setCurrentRow(4);
            dialog->findChild<QPushButton *>("combatAffinityDeleteGroup")->click();dialog->reject();
        });
        manage->click();QCOMPARE(project.tools()->leaderAffinity().json(),applied);QCOMPARE(undo.count(),1);
        QTimer::singleShot(0,[&] {
            auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());QVERIFY(dialog);
            dialog->findChild<QListWidget *>("combatAffinityGroupsList")->setCurrentRow(4);
            dialog->findChild<QPushButton *>("combatAffinityDeleteGroup")->click();dialog->accept();
        });
        manage->click();QCOMPARE(project.tools()->leaderAffinity().groups().size(),4);
        QCOMPARE(project.tools()->leaderAffinity().groupFor(0x45),-1);undo.undo();QCOMPARE(project.tools()->leaderAffinity().json(),applied);
    }
    void leaderAffinityEditing() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM"); if(path.isEmpty()) QSKIP("Private ROM required");
        applyEditorTheme(*qApp);
        auto project=Project::fromRom(Rom::open(path)); project.loadTools();
        QUndoStack undo; LeaderAffinityEditor editor(&project,&undo,[]{});
        editor.selectClass(0x18);
        auto beast=editor.findChild<QSpinBox *>("leaderAffinityStrength0"); QVERIFY(beast);
        QCOMPARE(beast->value(),15); beast->setValue(-25);
        QCOMPARE(project.tools()->leaderAffinity().bonuses(0x18)[0],-25);
        undo.undo(); QCOMPARE(beast->value(),15); undo.redo(); QCOMPARE(beast->value(),-25);
        editor.findChild<QPushButton *>("leaderAffinityRetail")->click(); QCOMPARE(beast->value(),15);
        int link=-1; const auto schema=fields(Table::Classes);
        for(int i=0;i<schema.size();++i) if(schema[i].key=="classCopyMatch") link=i;
        QVERIFY(link>=0); project.setValue(Table::Classes,0x76,link,6); editor.selectClass(0x76);
        QVERIFY(editor.findChild<QLabel *>("leaderAffinityScope")->text().contains("shared"));
        beast->setValue(12); QCOMPARE(project.tools()->leaderAffinity().bonuses(6)[0],12);
        QCOMPARE(project.tools()->leaderAffinity().bonuses(0x76)[0],0);
        editor.selectClass(6); QCOMPARE(beast->value(),12);
        const int custom=project.appendClass(5,"Affinity tester"); project.setValue(Table::Classes,custom,link,0);
        editor.selectClass(custom); beast->setValue(21); QCOMPARE(project.tools()->leaderAffinity().bonuses(custom)[0],21);
        editor.selectClass(-1); QVERIFY(!beast->isEnabled());
        MainWindow window; window.resize(1440,900); window.show(); window.loadRom(path);
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QWidget *>("leaderAffinityEditor"),30000);
        window.navigateTo({"classes",0x18,-1,"section:Combat"}); QTest::qWait(40);
        QVERIFY(window.grab().save(QCoreApplication::applicationDirPath()+"/leader-affinity.png"));
    }
    void persuasionEditing() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM"); if(path.isEmpty()) QSKIP("Private ROM required");
        applyEditorTheme(*qApp);
        auto project=Project::fromRom(Rom::open(path)); project.loadEncounters();
        const int custom=project.appendClass(5,"Persuasion tester");
        QUndoStack undo; EncounterEditor editor(&project,&undo,[]{});
        editor.resize(1440,950); editor.show();
        auto tabs=editor.findChild<QTabWidget *>("encounterTabs"); QVERIFY(tabs);
        for(int tab=0;tab<tabs->count();++tab) if(tabs->tabText(tab)=="Persuasion") tabs->setCurrentIndex(tab);
        auto table=editor.findChild<QTableWidget *>("persuasionTable"); QVERIFY(table);
        auto rowFor=[&](int id) {
            const auto groups=project.encounters()->persuasion().leaderGroups();
            for(int col=0;col<groups.size();++col) if(groups[col].members.contains(id)) return col;
            return -1;
        };
        const int beasts=project.encounters()->persuasion().groupFor(0x45);
        QCOMPARE(table->columnCount(),project.encounters()->persuasion().groups().size());
        QCOMPARE(table->rowCount(),project.encounters()->persuasion().leaderGroups().size()+1);
        const int masters=rowFor(24); QVERIFY(masters>=0);
        table->setCurrentCell(masters,beasts);
        QTest::qWait(40); QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath()+"/persuasion-matrix.png"));
        QCOMPARE(table->item(masters,beasts)->data(Qt::EditRole).toInt(),80);
        table->item(masters,beasts)->setData(Qt::EditRole,42);
        QCOMPARE(project.encounters()->persuasion().score(24,beasts),42);
        QCOMPARE(project.encounters()->persuasion().score(0x45,beasts),42);
        QCOMPARE(project.encounters()->persuasion().score(9,beasts),60);
        undo.undo(); QCOMPARE(table->item(masters,beasts)->data(Qt::EditRole).toInt(),80); undo.redo();
        // A prior individual edit must remain visible, without flattening it on display.
        auto mixed=project.encounters()->persuasion(); mixed.setScore(24,beasts,73);
        auto changed=*project.encounters(); changed.setPersuasion(mixed.json(),project.records(Table::Classes).size());
        project.setEncounters(changed.json());
        static_cast<PersuasionEditor *>(editor.findChild<QWidget *>("persuasionEditor"))->refresh();
        QCOMPARE(table->item(masters,beasts)->text(),QString("Mixed"));
        QCOMPARE(project.encounters()->persuasion().score(0x45,beasts),42);
        table->editItem(table->item(masters,beasts));
        auto spin=table->findChild<QSpinBox *>(); QVERIFY(spin); QVERIFY(spin->text().isEmpty());
        QTest::keyClick(spin,Qt::Key_Escape);
        QTest::qWait(10);
        QCOMPARE(project.encounters()->persuasion().score(24,beasts),73);
        table->editItem(table->item(masters,beasts));
        spin=table->findChild<QSpinBox *>(); QVERIFY(spin);
        QTest::keyClick(spin,Qt::Key_Tab); QTest::qWait(10);
        QCOMPARE(project.encounters()->persuasion().score(24,beasts),73);
        QCOMPARE(project.encounters()->persuasion().score(0x45,beasts),42);
        // Close any editor Tab opened in the next cell before editing this one again.
        if(auto nextSpin=table->findChild<QSpinBox *>()) { QTest::keyClick(nextSpin,Qt::Key_Escape); QTest::qWait(10); }
        table->editItem(table->item(masters,beasts));
        spin=table->findChild<QSpinBox *>(); QVERIFY(spin);
        spin->setValue(55); QTest::keyClick(spin,Qt::Key_Return);
        QTRY_COMPARE(project.encounters()->persuasion().score(24,beasts),55);
        QCOMPARE(project.encounters()->persuasion().score(0x45,beasts),55);
        undo.clear();
        QTimer::singleShot(0,&editor,[&] {
            auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget()); QVERIFY(dialog);
            dialog->findChild<QLineEdit *>("persuasionGroupName")->setText("Custom leaders");
            auto list=dialog->findChild<QListWidget *>("persuasionGroupMembers"); QVERIFY(list);
            for(int row=0;row<list->count();++row) if(list->item(row)->data(Qt::UserRole).toInt()==custom || list->item(row)->data(Qt::UserRole).toInt()==24)
                list->item(row)->setCheckState(Qt::Checked);
            dialog->accept();
        });
        editor.findChild<QPushButton *>("persuasionAddLeader")->click();
        const int leaders=rowFor(custom); QVERIFY(leaders>=0); QCOMPARE(rowFor(24),leaders);
        QCOMPARE(project.encounters()->persuasion().score(custom,beasts),30);
        QCOMPARE(table->item(leaders,beasts)->text(),QString("Mixed"));
        table->item(leaders,beasts)->setData(Qt::EditRole,91);
        QCOMPARE(project.encounters()->persuasion().score(custom,beasts),91);
        QCOMPARE(project.encounters()->persuasion().score(24,beasts),91);
        QCOMPARE(project.encounters()->persuasion().score(0x45,beasts),55);
        const auto leaderMembership=project.encounters()->persuasion().json()["leaderGroups"];
        table->setCurrentCell(leaders,beasts);
        QTimer::singleShot(0,&editor,[&] {
            auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget()); QVERIFY(dialog);
            dialog->findChild<QLineEdit *>("persuasionGroupName")->setText("Custom creatures");
            auto list=dialog->findChild<QListWidget *>("persuasionGroupMembers"); QVERIFY(list);
            for(int row=0;row<list->count();++row) if(list->item(row)->data(Qt::UserRole).toInt()==custom || list->item(row)->data(Qt::UserRole).toInt()==0x45)
                list->item(row)->setCheckState(Qt::Checked);
            dialog->accept();
        });
        editor.findChild<QPushButton *>("persuasionCreateGroup")->click();
        const int created=table->currentColumn(); QVERIFY(created!=beasts);
        QCOMPARE(project.encounters()->persuasion().groupFor(custom),created);
        QCOMPARE(project.encounters()->persuasion().groupFor(0x45),created);
        QCOMPARE(project.encounters()->persuasion().score(24,created),91);
        QCOMPARE(project.encounters()->persuasion().json()["leaderGroups"],leaderMembership);
        table->item(leaders,created)->setData(Qt::EditRole,67);
        QCOMPARE(project.encounters()->persuasion().score(custom,created),67);
        QCOMPARE(project.encounters()->persuasion().score(24,created),67);
        const auto manifest=project.manifest(); QTemporaryDir saved; project.saveFolder(saved.path());
        auto reopened=Project::fromRom(Rom::open(path)); reopened.applyManifest(Project::readProject(saved.path()));
        QCOMPARE(reopened.manifest(),manifest);
        undo.undo(); undo.undo(); QCOMPARE(project.encounters()->persuasion().groupFor(0x45),beasts);
        undo.redo(); QCOMPARE(project.encounters()->persuasion().groupFor(0x45),created);
        table->setCurrentCell(rowFor(custom),created);
        QTimer::singleShot(0,&editor,[&] {
            auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget()); QVERIFY(dialog);
            dialog->findChild<QLineEdit *>("persuasionGroupName")->setText("Custom leader only");
            auto list=dialog->findChild<QListWidget *>("persuasionGroupMembers");
            for(int row=0;row<list->count();++row) if(list->item(row)->data(Qt::UserRole).toInt()==24) list->item(row)->setCheckState(Qt::Unchecked);
            dialog->accept();
        });
        editor.findChild<QPushButton *>("persuasionEditLeader")->click();
        QVERIFY(rowFor(24)!=rowFor(custom));
        QCOMPARE(project.encounters()->persuasion().score(24,beasts),91);
        QCOMPARE(project.encounters()->persuasion().score(custom,beasts),91);
    }
    void pendingConsumableAndRouteEdits() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadTexts(); project.loadConsumableEffects(); project.loadScenarios(); project.loadWorld();
        QUndoStack undo;
        ConsumableEditor effects(&project, &undo, [] {});
        effects.selectConsumable(1);
        auto amount = effects.findChild<QSpinBox *>("effectMinimum");
        const int original = amount->value();
        const auto before = project.consumableEffects()->json();
        amount->setValue(original + 1);
        QCOMPARE(pendingEdits(&effects).size(), 1);
        QCOMPARE(project.consumableEffects()->json(), before);
        effects.selectConsumable(2); effects.selectConsumable(1);
        QCOMPARE(amount->value(), original + 1);
        QTimer::singleShot(0, &effects, [&] { effects.findChild<QDialog *>("pendingEditsDialog")->reject(); });
        QVERIFY(!resolvePendingEdits(&effects, "saving", [](const EditLocation &) {}));
        QCOMPARE(pendingEdits(&effects).size(), 1);
        QTimer::singleShot(0, &effects, [&] { effects.findChild<QPushButton *>("pendingApplyAll")->click(); });
        QVERIFY(resolvePendingEdits(&effects, "saving", [](const EditLocation &) {}));
        QVERIFY(pendingEdits(&effects).isEmpty());
        QVERIFY(project.consumableEffects()->json() != before);
        undo.undo(); QCOMPARE(project.consumableEffects()->json(), before);
        effects.selectConsumable(11);
        effects.findChild<QSpinBox *>("effectMinimum")->setValue(20);
        effects.findChild<QSpinBox *>("effectMaximum")->setValue(1);
        auto pending = effects.findChild<PendingEdits *>();
        QVERIFY(!pending->apply());
        QCOMPARE(pendingEdits(&effects).size(), 1);
        QCOMPARE(project.consumableEffects()->json(), before);
        pending->discard(); QVERIFY(pendingEdits(&effects).isEmpty());

        ScenarioRouteEditor *routes = nullptr;
        ScenarioRouteEditor editor(&project, [&](const Scenarios &next, const QString &) {
            project.setScenarios(next.json()); routes->refresh(1);
        });
        routes = &editor; editor.refresh(1);
        int row = -1;
        for (int i = 0; i < project.scenarios()->scenario(1).layout.nodes.size(); ++i)
            if (u8(project.scenarios()->scenario(1).layout.nodes[i], 1) <= 1) { row = i; break; }
        QVERIFY(row >= 0); editor.selectNode(row);
        auto wait = editor.findChild<QComboBox *>("routeWait");
        const auto originalNode = project.scenarios()->scenario(1).layout.nodes[row];
        const int chosen = (u8(originalNode, 3) + 1) % 3;
        wait->setCurrentIndex(wait->findData(chosen));
        QCOMPARE(pendingEdits(&editor).size(), 1);
        editor.refresh(2); editor.refresh(1); editor.selectNode(row);
        QCOMPARE(wait->currentData().toInt(), chosen);
        QCOMPARE(project.scenarios()->scenario(1).layout.nodes[row], originalNode);
        auto guard = editor.findChild<PendingEdits *>(QString(), Qt::FindDirectChildrenOnly);
        QVERIFY(guard); QVERIFY(guard->apply());
        QVERIFY(pendingEdits(&editor).isEmpty());
        QCOMPARE(u8(project.scenarios()->scenario(1).layout.nodes[row], 3), chosen);
    }
    void weaponStatusSelection() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        MainWindow window; window.resize(1440, 1000); window.show(); window.loadRom(path);
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QComboBox *>("weaponStatusOnHit"), 30000);
        window.navigateTo({"items", 1, -1, "section:Stats"});
        auto choice = window.findChild<QComboBox *>("weaponStatusOnHit");
        QVERIFY(choice->isVisible()); QVERIFY(choice->isEnabled());
        auto table = window.findChild<QTableView *>("itemsTable"); QVERIFY(table);
        auto proxy = qobject_cast<QAbstractProxyModel *>(table->model()); QVERIFY(proxy);
        auto model = qobject_cast<RecordModel *>(proxy->sourceModel()); QVERIFY(model);
        auto undo = window.findChild<QUndoGroup *>(); QVERIFY(undo);
        const int statusColumn = model->weaponStatusColumn();
        QVERIFY(statusColumn >= 2);
        QVERIFY(!table->isColumnHidden(statusColumn));
        int flags = -1, type = -1;
        for (int n=0; n<fields(Table::Items).size(); ++n) {
            if (fields(Table::Items)[n].key == "b23Raw") flags=n+2;
            if (fields(Table::Items)[n].key == "equipType") type=n+2;
        }
        QVERIFY(flags >= 2); QVERIFY(type >= 2);
        QVERIFY(model->setData(model->index(1, flags), 0x11, Qt::EditRole));
        for (const int effect : {32, 2, 4, 8, 64, 128, 0}) {
            const int index = choice->findData(effect); QVERIFY(index >= 0);
            choice->setCurrentIndex(index);
            QVERIFY(QMetaObject::invokeMethod(choice, "activated", Qt::DirectConnection, Q_ARG(int,index)));
            QCOMPARE(model->index(1, flags).data(Qt::EditRole).toInt(), 0x11|effect);
            QCOMPARE(model->index(1, statusColumn).data().toString(), choice->currentText());
            undo->undo(); undo->redo();
            QCOMPARE(choice->currentData().toInt(), effect);
            if (effect==64 || effect==128) {
                auto help=window.findChild<QLabel *>("weaponStatusHelp");
                QVERIFY(help->text().contains("Power Up strengthens the target"));
                QTRY_VERIFY(help->height() >= help->heightForWidth(help->width()));
                QVERIFY(window.grab().save(QCoreApplication::applicationDirPath()+"/weapon-power-ui.png"));
            }
        }
        QVERIFY(model->setData(model->index(1, flags), 0xd7, Qt::EditRole));
        QVERIFY(choice->currentText().startsWith("Existing combination:"));
        window.navigateTo({"items", 2, -1, "section:Stats"});
        window.navigateTo({"items", 1, -1, "section:Stats"});
        QCOMPARE(model->index(1, flags).data(Qt::EditRole).toInt(), 0xd7);
        const int poison = choice->findData(32);
        choice->setCurrentIndex(poison);
        QMetaObject::invokeMethod(choice, "activated", Qt::DirectConnection, Q_ARG(int,poison));
        QCOMPARE(model->index(1, flags).data(Qt::EditRole).toInt(), 0x31);
        QCOMPARE(model->appendItem(1, "Poison blade copy"), 278);
        window.navigateTo({"items", 278, -1, "section:Stats"});
        QCOMPARE(choice->currentText(), "Poison");
        QVERIFY(window.grab().save(QCoreApplication::applicationDirPath()+"/weapon-status-ui.png"));
        QVERIFY(model->setData(model->index(278, type), 14, Qt::EditRole));
        QVERIFY(!choice->isEnabled()); QCOMPARE(choice->currentText(), "Poison");
        QCOMPARE(model->index(278, flags).data(Qt::EditRole).toInt(), 0x31);
        QVERIFY(!(model->flags(model->index(278, statusColumn)) & Qt::ItemIsEditable));
        QVERIFY(!model->setData(model->index(278, statusColumn), 2, Qt::EditRole));
        undo->undo(); QVERIFY(choice->isEnabled());
        // Edit through the table dropdown, preserving unrelated weapon bits and Undo.
        auto index = proxy->mapFromSource(model->index(278, statusColumn));
        auto delegate = table->itemDelegateForColumn(statusColumn);
        std::unique_ptr<QWidget> cell(delegate->createEditor(table->viewport(), {}, index));
        auto cellChoice = qobject_cast<QComboBox *>(cell.get()); QVERIFY(cellChoice);
        delegate->setEditorData(cellChoice, index);
        QCOMPARE(cellChoice->currentText(), "Poison");
        cellChoice->setCurrentIndex(cellChoice->findData(2));
        delegate->setModelData(cellChoice, proxy, index);
        QCOMPARE(model->index(278, flags).data(Qt::EditRole).toInt(), 0x13);
        QCOMPARE(choice->currentText(), "Sleep");
        QCOMPARE(model->index(278, statusColumn).data().toString(), "Sleep");
        undo->undo(); QCOMPARE(choice->currentText(), "Poison");
        QCOMPARE(model->index(278, flags).data(Qt::EditRole).toInt(), 0x31);
        undo->redo(); QCOMPARE(choice->currentText(), "Sleep");
        auto views = window.findChild<QComboBox *>("itemsColumnView");
        views->setCurrentIndex(views->findData("Resistance"));
        QVERIFY(table->isColumnHidden(statusColumn));
        views->setCurrentIndex(views->findData("Stats"));
        QVERIFY(!table->isColumnHidden(statusColumn));
        views->setCurrentIndex(views->findData(""));
        QVERIFY(!table->isColumnHidden(statusColumn));
    }
    void fixedEquipmentChoices() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        MainWindow window;
        window.loadRom(path);
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QComboBox *>("fixedEquipmentLocks"), 30000);
        window.navigateTo({"classes", 1, -1, "section:Equipment"});
        auto locks = window.findChild<QComboBox *>("fixedEquipmentLocks");
        QCOMPARE(locks->currentData().toInt(), 3);
        QCOMPARE(locks->currentText(), "Weapon + Body armor");
        auto table = window.findChild<QTableView *>("classesTable");
        auto proxy = qobject_cast<QAbstractProxyModel *>(table->model()); QVERIFY(proxy);
        auto model = qobject_cast<RecordModel *>(proxy->sourceModel()); QVERIFY(model);
        auto undo = window.findChild<QUndoGroup *>(); QVERIFY(undo);
        int column = -1;
        for (int n = 0; n < fields(Table::Classes).size(); ++n)
            if (fields(Table::Classes)[n].key == "fixedEquipment") column = n + 2;
        QVERIFY(column >= 2);
        // Modified ROMs can carry unknown upper bits; named choices must leave them alone.
        QVERIFY(model->setData(model->index(1, column), 0xa3, Qt::EditRole));
        for (int mask : {0, 1, 2, 4, 8, 15, 3}) {
            const int selection = locks->findData(mask); QVERIFY(selection >= 0);
            locks->setCurrentIndex(selection);
            QVERIFY(QMetaObject::invokeMethod(locks, "activated", Qt::DirectConnection, Q_ARG(int, selection)));
            QCOMPARE(model->index(1, column).data(Qt::EditRole).toInt(), 0xa0 | mask);
            undo->undo(); undo->redo();
            QCOMPARE(locks->currentData().toInt(), mask);
        }
        const int previous = model->index(2, column).data(Qt::EditRole).toInt();
        window.navigateTo({"classes", 2, -1, "section:Equipment"});
        QCOMPARE(locks->currentData().toInt(), previous & 15);
        window.navigateTo({"classes", 1, -1, "section:Equipment"});
        QCOMPARE(locks->currentText(), "Weapon + Body armor");
        QCOMPARE(model->index(1, column).data(Qt::EditRole).toInt(), 0xa3);
        QCOMPARE(model->index(2, column).data(Qt::EditRole).toInt(), previous);
    }
    void classSetupAndTestingHandoff() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        MainWindow window; window.resize(1440, 1000); window.show(); window.loadRom(path);
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QWidget *>("classSetup"), 30000);
        window.navigateTo({"classes", 5, -1, "section:Overview"});
        auto section = window.findChild<QComboBox *>("classesInspectorSection");
        QCOMPARE(section->currentText(), "Overview");
        window.findChild<QPushButton *>("classSetupEquipment")->click();
        QCOMPARE(section->currentText(), "Equipment");
        auto back = window.findChild<QAction *>("returnToClassSetup"); QVERIFY(back);
        back->trigger(); QCOMPARE(section->currentText(), "Overview");
        window.findChild<QPushButton *>("classSetup_portrait")->click();
        auto portraits = window.findChild<QListWidget *>("portraitClasses");
        QCOMPARE(portraits->currentItem()->data(Qt::UserRole).toInt(), 5);
        QVERIFY(portraits->isVisible());
        back->trigger(); QVERIFY(window.findChild<QWidget *>("classSetup")->isVisible());
        QVERIFY(window.grab().save(QCoreApplication::applicationDirPath() + "/class-setup-review.png"));
        QCOMPARE(window.findChild<QAction *>("whereUsed")->text(), "Find uses");
        auto testing = window.findChild<TestingEditor *>(); QVERIFY(testing);
        testing->setProjectRevision(10, false);
        testing->setBuildPath(path);
        testing->useExport(path, 10, sha256(Rom::open(path).bytes()));
        testing->selectScenario(1); // A sidebar handoff can precede the async ROM load.
        QTRY_VERIFY_WITH_TIMEOUT(testing->ready(), 30000);
        QVERIFY(testing->findChild<QLabel *>("seederStatus")->text().contains("scenario 1"));
        auto freshness = testing->findChild<QLabel *>("testingFreshness");
        QVERIFY(freshness->text().startsWith("Latest build"));
        testing->setProjectRevision(11, false); QVERIFY(freshness->text().contains("changed since"));
        testing->setProjectRevision(10, true); QVERIFY(freshness->text().contains("changed since"));
        testing->setProjectRevision(10, false);
        testing->selectScenario(1);
        auto missions = testing->findChild<QTreeWidget *>("seederMissions");
        QVERIFY(missions->currentItem()); QVERIFY(!missions->currentItem()->isHidden());
        QVERIFY(testing->findChild<QLabel *>("seederStatus")->text().contains("scenario 1"));
        testing->selectScenario(999); QVERIFY(!missions->currentItem());
        QVERIFY(testing->findChild<QLabel *>("seederStatus")->text().contains("no supported"));
        testing->findChild<QLineEdit *>("seederSearch")->setText(" ");
        QVERIFY(missions->topLevelItemCount() > 0);
    }
    void namedSharedEditImpact() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path)); project.loadScenarios(); project.loadWorld(); project.loadEncounters();
        const int town = u16(project.world()->scenario(1).sites, 1);
        const auto towns = findProjectUses(project, "town", town);
        QVERIFY(!towns.isEmpty());
        for (const auto &use : towns) {
            QVERIFY(use.subject.contains(QString::number(use.location.id)));
            QCOMPARE(u16(project.world()->scenario(use.location.id).sites, 1 + use.location.child * 4), town);
        }
        const auto maps = findProjectUses(project, "map", 1);
        QVERIFY(!maps.isEmpty());
        for (const auto &use : maps)
            QCOMPARE(u8(project.rom().bytes(), 0x654a0 + use.location.id * 9 + 3), u8(project.rom().bytes(), 0x654a0 + 12));
        for (int group = 1; group <= Encounters::poolCount; ++group) {
            const auto uses = findProjectUses(project, "encounterPool", group);
            QCOMPARE(uses.size(), project.encounters()->scenarioKeys(group).size());
            for (const auto &use : uses) QVERIFY(project.encounters()->scenarioKeys(group).contains(use.location.child));
        }
        QUndoStack undo;
        ScenarioEditor editor(&project, &undo, [] {}); editor.resize(1300, 900); editor.show();
        auto scenarios = editor.findChild<QListWidget *>("scenarioList");
        QVERIFY(scenarios); scenarios->setCurrentRow(0);
        auto objects = editor.findChild<QListWidget *>("mapObjects"); QVERIFY(objects);
        bool selected = false;
        for (int row = 0; row < objects->count(); ++row)
            if (objects->item(row)->data(Qt::UserRole).toInt() == int(MapObject::Site)) { objects->setCurrentRow(row); selected = true; break; }
        QVERIFY(selected);
        auto impact = editor.findChild<QLabel *>("townEditImpact"); QVERIFY(impact);
        QVERIFY(impact->text().contains("href="));
    }
    void pendingEditsProtectWindowClose() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        MainWindow window; window.loadRom(path);
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QTableView *>("consumablesTable"), 30000);
        window.navigateTo({"consumables", 1});
        auto amount = window.findChild<QSpinBox *>("effectMinimum");
        amount->setValue(amount->value() + 1);
        QVERIFY(window.isWindowModified());
        auto action = window.findChild<QAction *>("pendingProjectChanges");
        QVERIFY(action->isVisible());
        bool prompted = false;
        QTimer::singleShot(0, &window, [&] {
            auto dialog = window.findChild<QDialog *>("pendingEditsDialog");
            if (dialog) { prompted = true; dialog->reject(); }
        });
        QVERIFY(!window.close()); QVERIFY(prompted);
        QCOMPARE(pendingEdits(&window).size(), 1);
        int checked = 0;
        for (auto command : window.findChildren<QAction *>())
            if (command->text() == "Save project &folderâ€¦" || command->text() == "Save sharing &archiveâ€¦" || command->text() == "Build and &export ROMâ€¦") {
                prompted = false;
                QTimer::singleShot(0, &window, [&] {
                    auto dialog = window.findChild<QDialog *>("pendingEditsDialog");
                    if (dialog) { prompted = true; dialog->reject(); }
                });
                command->trigger(); QVERIFY(prompted);
                QCOMPARE(pendingEdits(&window).size(), 1); ++checked;
            }
        QCOMPARE(checked, 3);
        QTimer::singleShot(0, &window, [&] { window.findChild<QPushButton *>("pendingDiscardAll")->click(); });
        action->trigger();
        QVERIFY(pendingEdits(&window).isEmpty()); QVERIFY(!window.isWindowModified());
    }
    void expandedSourceProjectOpening() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_EXPANDED_ROM");
        if (path.isEmpty()) QSKIP("Expanded-army ROM fixture required");
        auto project = Project::fromRom(Rom::open(path));
        QVERIFY(project.expandedArmies());
        QVERIFY(project.runtimeRom().bytes() != project.rom().bytes());
        project.loadTexts();
        const auto name = QString("Reopened source");
        project.setClassNameBytes(5, encodeClassName(name));
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        project.saveFolder(folder.path());
        project.saveArchive(folder.filePath("saved.lcproject"));
        MainWindow window;
        window.loadRom(path);
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QTableView *>("classesTable"), 30000);
        QVERIFY(window.openProjectFile(folder.filePath("project.json")));
        auto table = window.findChild<QTableView *>("classesTable");
        QVERIFY(table);
        bool found = false;
        for (int row = 0; row < table->model()->rowCount(); ++row)
            found |= table->model()->index(row, 1).data().toString() == name;
        QVERIFY(found);
        QVERIFY(window.openProjectFile(folder.filePath("saved.lcproject")));
        QVERIFY(!window.isWindowModified());
    }
    void projectSourceMismatchChoice() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        auto source = Project::fromRom(Rom::open(path));
        const auto name = QString("Imported Knight");
        source.setClassNameBytes(5, encodeClassName(name));
        auto manifest = source.manifest();
        manifest["sourceSha256"] = QString(64, 'a');
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        QFile file(folder.filePath("project.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(manifest).toJson());
        file.close();
        MainWindow window;
        window.loadRom(path);
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QTableView *>("classesTable"), 30000);
        const auto hasName = [&] {
            auto table = window.findChild<QTableView *>("classesTable");
            for (int row = 0; table && row < table->model()->rowCount(); ++row)
                if (table->model()->index(row, 1).data().toString() == name) return true;
            return false;
        };
        QVERIFY(!hasName());
        bool prompted = false, warned = false;
        QTimer::singleShot(0, &window, [&] {
            if (auto dialog = window.findChild<QMessageBox *>("projectSourceMismatch")) {
                prompted = true;
                warned = dialog->informativeText().contains("edits that are not included in the project");
                dialog->reject();
            }
        });
        QVERIFY(!window.openProjectFile(file.fileName()));
        QVERIFY(prompted); QVERIFY(warned);
        QVERIFY(!hasName()); QVERIFY(!window.isWindowModified());
        bool imported = false;
        QTimer::singleShot(0, &window, [&] {
            if (auto dialog = window.findChild<QMessageBox *>("projectSourceMismatch")) {
                if (auto button = dialog->findChild<QPushButton *>("importProjectIntoCurrentRom")) {
                    imported = true;
                    button->click();
                } else dialog->reject();
            }
        });
        QVERIFY(window.openProjectFile(file.fileName()));
        QVERIFY(imported); QVERIFY(hasName()); QVERIFY(window.isWindowModified());
    }
    void artLibraryRomImport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            QUndoStack undo;
            SpriteEditor editor(&project, &undo, [] {});
            editor.show();
            bool selected = false;
            QTimer::singleShot(100, &editor, [&] {
                auto dialog = editor.findChild<RomArtImport *>();
                if (!dialog)
                    return;
                auto tabs = dialog->findChild<QTabWidget *>();
                tabs->setCurrentIndex(1);
                auto list = dialog->findChild<QListWidget *>("romArtList");
                list->setCurrentRow(0);
                auto button = dialog->findChild<QPushButton *>("romArtImportSelected");
                selected = button && button->isEnabled();
                if (selected)
                    button->click();
                else
                    dialog->reject();
            });
            editor.findChild<QPushButton *>("spriteImportRom")->click();
            QVERIFY(selected);
            QCOMPARE(project.sprites().assets().size(), 1);
            const auto asset = project.sprites().assets().first();
            QCOMPARE(asset.frames.first().layers.first().image,
                     IconArtwork::load(project.rom()).image(1));
            undo.undo();
            QVERIFY(project.sprites().assets().isEmpty());
            undo.redo();
            QCOMPARE(project.sprites().asset(asset.id), asset);
            auto reopened = Project::fromRom(project.rom());
            reopened.applyManifest(project.manifest());
            QCOMPARE(reopened.sprites().asset(asset.id), asset);
            RomArtImport picker(project.rom(), [](SpriteAsset) {});
            picker.show();
            auto categories = picker.findChild<QComboBox *>("romArtCategory");
            auto list = picker.findChild<QListWidget *>("romArtList");
            for (int kind : {1, 2, 3, 4}) {
                categories->setCurrentIndex(kind);
                QVERIFY(list->count() > 0);
                list->setCurrentRow(0);
                QVERIFY(picker.findChild<QPushButton *>("romArtImportSelected")->isEnabled());
            }
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void testingSaveAssignments() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto oldName = QCoreApplication::applicationName();
        const auto oldOrganization = QCoreApplication::organizationName();
        QCoreApplication::setOrganizationName("LordlyCaliber-Tests");
        QCoreApplication::setApplicationName("TestingAssignments-" +
                                             QUuid::createUuid().toString(QUuid::Id128));
        const auto cleanup = qScopeGuard([&] {
            QSettings().clear();
            QCoreApplication::setApplicationName(oldName);
            QCoreApplication::setOrganizationName(oldOrganization);
        });
        const auto write = [](const QString &path, const QByteArray &bytes) {
            QFile f(path);
            return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size();
        };
        const auto rom = Rom::open(path);
        ScenarioSeeder model(rom);
        auto donor = SaveDocument::open(rom, model.defaultArmyBytes());
        donor.selectSlot(0);
        donor.apply(donor.planEdit(SaveTable::Characters, 1, "luck", 37), true);
        const auto armyPath = temp.filePath("my-army.sra"),
                   fullPath = temp.filePath("whole-save.sra");
        const auto full = model.seed(12);
        QVERIFY(write(armyPath, donor.serialize()));
        QVERIFY(write(fullPath, full));
        const auto exe = temp.filePath("Project64.exe");
        QVERIFY(write(exe, "inert test path; never executed"));
        QSettings().setValue("testing/project64", exe);
        QCOMPARE(QSettings().value("testing/project64").toString(), exe);
        QSettings().setValue("testing/army", armyPath);
        QSettings().setValue("testing/armySlot", 0);
        QByteArray assigned;
        QString assignedRom, assignedExe, assignedDestination;
        bool merged = false, started = false;
        int calls = 0;
        {
            TestingEditor editor(path);
            editor.resize(1200, 900);
            editor.show();
            editor.setLauncher([&](const Rom &, const QString &romPath, const QString &exePath,
                                   const QByteArray &save, const QString &destination, bool merge,
                                   bool start) {
                assigned = save;
                assignedRom = romPath;
                assignedExe = exePath;
                assignedDestination = destination;
                merged = merge;
                started = start;
                ++calls;
                return QString(); // Exercise preparation without any emulator/process or
                                  // save-directory access.
            });
            QTRY_VERIFY_WITH_TIMEOUT(editor.ready(), 30000);
            editor.findChild<QComboBox *>("testingSourceMode")->setCurrentIndex(1);
            auto list = editor.findChild<QTreeWidget *>("seederMissions");
            auto status = editor.findChild<QLabel *>("seederStatus");
            auto search = editor.findChild<QLineEdit *>("seederSearch");
            editor.loadSave(fullPath);
            QCOMPARE(list->topLevelItemCount(), model.choices().size());
            QCOMPARE(calls, 0); // Both files are required.
            editor.selectScenario(999);
            search->setText("no matching missions");
            for (int i = 0; i < list->topLevelItemCount(); ++i)
                QVERIFY(list->topLevelItem(i)->isHidden());
            QVERIFY(editor.findChild<QPushButton *>("seederUseSave")->isVisible());
            QVERIFY(editor.findChild<QPushButton *>("testingSelectSaveRom")->isVisible());
            editor.loadSaveRom(path);
            QTRY_VERIFY_WITH_TIMEOUT(list->isEnabled(), 10000);
            QVERIFY2(status->text().startsWith("Save assigned:"), qPrintable(status->text()));
            QCOMPARE(calls, 1);
            QCOMPARE(assigned, full);
            QVERIFY(!merged);
            QVERIFY(!started);
            QCOMPARE(assignedRom, path);
            QCOMPARE(assignedExe, exe);
            QCOMPARE(assignedDestination, ScenarioSeeder::batteryPath(rom, exe));
            QVERIFY(editor.findChild<QLabel *>("testingAssignmentDetails")
                        ->text()
                        .contains("editor is not required"));
            // Choosing a mission uses the independently selected army, not the complete save.
            search->clear();
            list->setCurrentItem(list->topLevelItem(2));
            qobject_cast<QPushButton *>(list->itemWidget(list->topLevelItem(8), 2))->click();
            QTRY_VERIFY_WITH_TIMEOUT(list->isEnabled(), 10000);
            QVERIFY2(status->text().startsWith("Started:"), qPrintable(status->text()));
            QCOMPARE(calls, 2);
            QVERIFY(merged);
            QVERIFY(started);
            QCOMPARE(assigned, model.seed(8, &donor));
            auto seeded = SaveDocument::open(rom, assigned);
            seeded.selectSlot(0);
            QCOMPARE(seeded.value(SaveTable::Characters, 1, "luck").toInt(), 37);
            // Loading a ROM again keeps both assignments. An invalid save selection keeps the
            // previous one.
            editor.loadRom(path);
            QTRY_VERIFY_WITH_TIMEOUT(editor.ready(), 30000);
            QCOMPARE(editor.findChild<QLabel *>("testingCompleteSaveName")->text(),
                     "whole-save.sra");
            QVERIFY(editor.findChild<QLabel *>("testingArmyName")->text().contains("my-army.sra"));
            editor.loadSave(exe);
            QVERIFY(!status->text().isEmpty());
            QCOMPARE(editor.findChild<QLabel *>("testingCompleteSaveName")->text(),
                     "whole-save.sra");
            editor.loadSaveRom(exe);
            editor.loadSave({});
            editor.loadSaveRom({});
            QCOMPARE(calls, 2); // Invalid/cancelled selections never install anything.
            QCOMPARE(editor.findChild<QLabel *>("testingCompleteSaveRomName")->toolTip(), path);
            editor.findChild<QPushButton *>("seederDefaultArmy")->click();
            qobject_cast<QPushButton *>(list->itemWidget(list->topLevelItem(8), 2))->click();
            QTRY_VERIFY_WITH_TIMEOUT(list->isEnabled(), 10000);
            QVERIFY2(status->text().startsWith("Started:"), qPrintable(status->text()));
            QCOMPARE(calls, 3);
            QCOMPARE(assigned, model.seed(8));
            // Current-project requests capture the clicked mission rather than the selected row.
            editor.findChild<QComboBox *>("testingSourceMode")->setCurrentIndex(0);
            QSignalSpy requests(&editor, &TestingEditor::testProjectRequested);
            list->setCurrentItem(list->topLevelItem(2));
            qobject_cast<QPushButton *>(list->itemWidget(list->topLevelItem(8), 2))->click();
            QCOMPARE(requests.size(), 1);
            QCOMPARE(requests.takeFirst()[0].toInt(), 8);
            // Complete-save assignment bypasses project builds, even in current-project mode.
            editor.loadSave(fullPath);
            QTRY_VERIFY_WITH_TIMEOUT(list->isEnabled(), 10000);
            QVERIFY2(status->text().startsWith("Save assigned:"), qPrintable(status->text()));
            QCOMPARE(requests.size(), 0);
            QCOMPARE(calls, 4);
            QCOMPARE(assigned, full);
            QVERIFY(!merged);
            QVERIFY(!started);
            editor.resize(1000, 800);
            QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath() +
                                       "/testing-assignments.png"));
        }
        TestingEditor reopened(path);
        reopened.setLauncher([&](const Rom &, const QString &, const QString &, const QByteArray &,
                                 const QString &, bool, bool) {
            ++calls;
            return QString();
        });
        reopened.show();
        QTRY_VERIFY_WITH_TIMEOUT(reopened.ready(), 30000);
        QCOMPARE(reopened.findChild<QLabel *>("testingCompleteSaveName")->text(), "whole-save.sra");
        QCOMPARE(reopened.findChild<QLabel *>("testingCompleteSaveRomName")->toolTip(), path);
        QCOMPARE(calls, 4); // Reopening the editor never silently overwrites a save.
        QSettings().remove("testing/completeSave");
        QSettings().remove("testing/completeSaveRom");
        TestingEditor reverse(path);
        reverse.setLauncher([&](const Rom &, const QString &, const QString &,
                                const QByteArray &save, const QString &, bool merge, bool start) {
            assigned = save;
            merged = merge;
            started = start;
            ++calls;
            return QString();
        });
        reverse.loadSaveRom(path);
        QCOMPARE(calls, 4);
        reverse.loadSave(fullPath);
        QTRY_VERIFY_WITH_TIMEOUT(reverse.findChild<QTreeWidget *>("seederMissions")->isEnabled(),
                                 10000);
        QCOMPARE(calls, 5); // ROM-first selection also assigns when the second file is selected.
        QCOMPARE(assigned, full);
        QVERIFY(!merged);
        QVERIFY(!started);
    }
    void testingBuildAndSeed() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and matching compiler required");
        const auto root = QFileInfo(path).absolutePath() + "/Test ROMs";
        QVERIFY(QDir().mkpath(root));
        QTemporaryDir output(root + "/testing-workflow-XXXXXX");
        QVERIFY(output.isValid());
        const auto outputPath = output.filePath(QFileInfo(path).fileName());
        QTemporaryDir fixture;
        QVERIFY(fixture.isValid());
        const auto oldName = QCoreApplication::applicationName();
        const auto oldOrganization = QCoreApplication::organizationName();
        QCoreApplication::setOrganizationName("LordlyCaliber-Tests");
        QCoreApplication::setApplicationName("TestingBuild-" +
                                             QUuid::createUuid().toString(QUuid::Id128));
        const auto cleanup = qScopeGuard([&] {
            qputenv("LORDLY_N64_TOOLCHAIN", tools.toUtf8());
            QSettings().clear();
            QCoreApplication::setApplicationName(oldName);
            QCoreApplication::setOrganizationName(oldOrganization);
        });
        const auto write = [](const QString &path, const QByteArray &bytes) {
            QFile f(path);
            return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size();
        };
        const auto read = [](const QString &path) {
            QFile f(path);
            return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
        };
        const auto source = Rom::open(path);
        const auto sourceHash = sha256(source.bytes());
        const auto exe = fixture.filePath("Project64.exe");
        QVERIFY(write(exe, "inert test path; never executed"));
        QSettings().setValue("testing/project64", exe);
        QCOMPARE(QSettings().value("testing/project64").toString(), exe);
        QSettings().setValue("testing/buildPath/" + QString::fromLatin1(sourceHash.left(16)),
                             outputPath);
        ScenarioSeeder original(source);
        const auto full = original.seed(12);
        const auto fullPath = fixture.filePath("full.sra");
        QVERIFY(write(fullPath, full));
        QByteArray assigned, built;
        QString launchedPath;
        bool merged = false, started = false;
        int calls = 0;
        {
            MainWindow window;
            window.resize(1280, 1000);
            window.show();
            window.loadRom(path);
            QTRY_VERIFY_WITH_TIMEOUT(window.findChild<TestingEditor *>(), 30000);
            auto testing = window.findChild<TestingEditor *>();
            window.findChild<QTabWidget *>()->setCurrentWidget(testing);
            QTRY_VERIFY_WITH_TIMEOUT(testing->ready(), 30000);
            testing->setLauncher([&](const Rom &rom, const QString &romPath, const QString &,
                                     const QByteArray &save, const QString &, bool merge,
                                     bool start) {
                assigned = save;
                built = rom.bytes();
                launchedPath = romPath;
                merged = merge;
                started = start;
                ++calls;
                return QString();
            });
            auto status = testing->findChild<QLabel *>("seederStatus");
            auto list = testing->findChild<QTreeWidget *>("seederMissions");
            auto table = window.findChild<QTableView *>("classesTable");
            QVERIFY(table);
            auto proxy = qobject_cast<QAbstractProxyModel *>(table->model());
            QVERIFY(proxy);
            auto model = qobject_cast<RecordModel *>(proxy->sourceModel());
            QVERIFY(model);
            int hpColumn = -1;
            for (int i = 0; i < fields(Table::Classes).size(); ++i)
                if (fields(Table::Classes)[i].key == "baseHp")
                    hpColumn = i + 2;
            QVERIFY(hpColumn >= 2);
            const int hp = model->index(2, hpColumn).data(Qt::EditRole).toInt();
            QVERIFY(model->setData(model->index(2, hpColumn), hp + 1, Qt::EditRole));
            testing->loadSave(fullPath);
            auto seedMission = [&] {
                qobject_cast<QPushButton *>(list->itemWidget(list->topLevelItem(8), 2))->click();
            };
            seedMission();
            QTRY_VERIFY_WITH_TIMEOUT(list->isEnabled(), 180000);
            QVERIFY2(status->text().startsWith("Started:"), qPrintable(status->text()));
            QCOMPARE(calls, 1);
            QVERIFY(merged);
            QVERIFY(started);
            QCOMPARE(launchedPath, outputPath);
            const auto reopened = Project::fromRom(Rom::open(outputPath));
            QCOMPARE(u16(reopened.record(Table::Classes, 2).bytes, 8), hp + 1);
            auto checked = built;
            Rom::updateChecksum(checked);
            QCOMPARE(checked, built);
            QCOMPARE(assigned, ScenarioSeeder(Rom::open(outputPath)).seed(8));
            QVERIFY(testing->currentProjectReady());
            const auto firstImage = read(outputPath);
            const auto firstTime = QFileInfo(outputPath).lastModified();
            // Same revision: no compiler or rewrite is needed, even if the compiler is now
            // unavailable.
            QVERIFY(write(fixture.filePath("cc1.exe"), "bad compiler identity"));
            qputenv("LORDLY_N64_TOOLCHAIN", fixture.path().toUtf8());
            seedMission();
            QTRY_VERIFY_WITH_TIMEOUT(status->text().startsWith("Started:"), 10000);
            QCOMPARE(calls, 2);
            QVERIFY(started);
            QCOMPARE(QFileInfo(outputPath).lastModified(), firstTime);
            // A changed project cannot launch the cached older image when compilation fails.
            QVERIFY(model->setData(model->index(2, hpColumn), hp + 2, Qt::EditRole));
            seedMission();
            QTRY_VERIFY_WITH_TIMEOUT(status->text().startsWith("Build failed"), 30000);
            QCOMPARE(calls, 2);
            QCOMPARE(read(outputPath), firstImage);
            qputenv("LORDLY_N64_TOOLCHAIN", tools.toUtf8());
            seedMission();
            QTRY_VERIFY_WITH_TIMEOUT(list->isEnabled(), 180000);
            QVERIFY2(status->text().startsWith("Started:"), qPrintable(status->text()));
            QCOMPARE(calls, 3);
            QVERIFY(merged);
            QCOMPARE(
                u16(Project::fromRom(Rom::open(outputPath)).record(Table::Classes, 2).bytes, 8),
                hp + 2);
            // The independent assignment targets the selected source ROM, without compiling
            // or replacing the mission test output, even with current-project mode selected.
            const auto outputTime = QFileInfo(outputPath).lastModified();
            qputenv("LORDLY_N64_TOOLCHAIN", fixture.path().toUtf8());
            testing->loadSaveRom(path);
            QTRY_VERIFY_WITH_TIMEOUT(list->isEnabled(), 10000);
            QVERIFY2(status->text().startsWith("Save assigned:"), qPrintable(status->text()));
            QCOMPARE(calls, 4);
            QVERIFY(!merged);
            QVERIFY(!started);
            QCOMPARE(assigned, full);
            QCOMPARE(launchedPath, path);
            QCOMPARE(built, source.bytes());
            QCOMPARE(QFileInfo(outputPath).lastModified(), outputTime);
            QVERIFY(testing->currentProjectReady());
            qputenv("LORDLY_N64_TOOLCHAIN", tools.toUtf8());
            QCOMPARE(testing->findChild<QLabel *>("testingCompleteSaveName")->text(), "full.sra");
            // Pending forms are resolved before the cached-build shortcut as well.
            window.navigateTo({"consumables", 1});
            auto amount = window.findChild<QSpinBox *>("effectMinimum");
            amount->setValue(amount->value() + 1);
            window.findChild<QTabWidget *>()->setCurrentWidget(testing);
            bool prompted = false;
            QTimer::singleShot(0, &window, [&] {
                if (auto dialog = window.findChild<QDialog *>("pendingEditsDialog")) {
                    prompted = true;
                    dialog->reject();
                }
            });
            seedMission();
            QVERIFY(prompted);
            QCOMPARE(calls, 4);
            QCOMPARE(pendingEdits(&window).size(), 1);
            QVERIFY(status->text().startsWith("Test cancelled"));
            for (auto drafts : window.findChildren<PendingEdits *>())
                if (drafts->discard)
                    drafts->discard();
            QVERIFY(window.grab().save(QCoreApplication::applicationDirPath() +
                                       "/testing-project-workflow.png"));
        }
        QVERIFY(QFileInfo::exists(outputPath)); // The generated ROM survives closing the editor.
        QCOMPARE(sha256(Rom::open(path).bytes()), sourceHash); // Source ROM remains read-only.
    }
    void testingStoryIdentityAfterBuild() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
        const auto rom = Rom::open(path);
        ScenarioSeeder original(rom);
        const int oldRow = original.choices().size()-1;
        const auto identity = original.choices()[oldRow].route;
        StoryRoutes routes(rom);
        auto script = routes.scripts.value(identity.event);
        int changed = 0;
        // This fixture changes only a story script's mission selector, causing the
        // existing wiki-order sort to move that story entry near the start.
        for (int i = 0; i + 5 < script.size(); i += 2)
            if (u8(script, i) == 0x68 && script.mid(i + 2, 4) == QByteArray::fromHex("70e970e7")) {
                put8(script, i + 1, 1);
                ++changed;
            }
        QVERIFY(changed > 0);
        auto bytes = rom.bytes();
        while (bytes.size() % 4)
            bytes.append(char(0));
        const auto offset = bytes.size();
        const auto compressed = encodeLzss(script);
        QByteArray envelope(8, char(0));
        put32(envelope, 0, compressed.size() + 4);
        put32(envelope, 4, script.size());
        bytes += envelope + compressed;
        while (bytes.size() % 4) bytes.append(char(0));
        put32(bytes, 0x9398ec + identity.event * 4, offset - 0x594280);
        Rom::updateChecksum(bytes);
        ScenarioSeeder reordered(Rom::fromBytes(bytes));
        int newRow = -1;
        for (int i = 0; i < reordered.choices().size(); ++i)
            if (reordered.choices()[i].route.node == identity.node &&
                reordered.choices()[i].route.event == identity.event)
                newRow = i;
        QVERIFY(newRow >= 0);
        QVERIFY(newRow != oldRow);
        QTemporaryDir candidate(QFileInfo(path).absolutePath() + "/Test ROMs/testing-order-XXXXXX");
        QVERIFY(candidate.isValid());
        const auto output = candidate.filePath(QFileInfo(path).fileName());
        QFile file(output);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(rom.serialize(bytes)), bytes.size());
        file.close();
        const auto oldName = QCoreApplication::applicationName(),
                   oldOrg = QCoreApplication::organizationName();
        QCoreApplication::setOrganizationName("LordlyCaliber-Tests");
        QCoreApplication::setApplicationName("TestingOrder-" +
                                             QUuid::createUuid().toString(QUuid::Id128));
        const auto cleanup = qScopeGuard([&] {
            QSettings().clear();
            QCoreApplication::setApplicationName(oldName);
            QCoreApplication::setOrganizationName(oldOrg);
        });
        const auto exe = candidate.filePath("Project64.exe");
        QFile fake(exe);
        QVERIFY(fake.open(QIODevice::WriteOnly));
        fake.close();
        QSettings().setValue("testing/project64", exe);
        QByteArray assigned;
        int calls = 0;
        TestingEditor editor(path);
        editor.show();
        QTRY_VERIFY_WITH_TIMEOUT(editor.ready(), 30000);
        editor.setLauncher([&](const Rom &, const QString &, const QString &,
                               const QByteArray &save, const QString &, bool, bool) {
            assigned = save;
            ++calls;
            return QString();
        });
        editor.setBuildPath(output);
        editor.setProjectRevision(1, false);
        editor.useTestBuild(output, 1, sha256(bytes), oldRow);
        auto list = editor.findChild<QTreeWidget *>("seederMissions");
        QTRY_VERIFY_WITH_TIMEOUT(list->isEnabled(), 30000);
        auto status = editor.findChild<QLabel *>("seederStatus");
        QVERIFY2(status->text().startsWith("Started:"), qPrintable(status->text()));
        QCOMPARE(calls, 1);
        QCOMPARE(assigned, reordered.seed(newRow));
        QCOMPARE(list->currentItem()->data(0, Qt::UserRole).toInt(), newRow);
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void testingColdBoot() {
#ifndef Q_OS_WIN
        QSKIP("Project64 is Windows-only");
#endif
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        if (ScenarioSeeder::project64Running())
            QSKIP("Do not disturb a real Project64 session");
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const auto exe = temp.filePath("Project64.exe"), log = temp.filePath("boots.jsonl");
        QVERIFY(
            QFile::copy(QCoreApplication::applicationDirPath() + "/project64_fixture.exe", exe));
        const auto oldName = QCoreApplication::applicationName();
        const auto oldOrganization = QCoreApplication::organizationName();
        QCoreApplication::setOrganizationName("LordlyCaliber-Tests");
        QCoreApplication::setApplicationName("Cold-boot-ui-" +
                                             QUuid::createUuid().toString(QUuid::Id128));
        const QList<QByteArray> envs{"LORDLY_P64_FIXTURE_SAVE", "LORDLY_P64_FIXTURE_FLUSH",
                                     "LORDLY_P64_FIXTURE_LOG"};
        QList<QByteArray> previous;
        for (const auto &name : envs)
            previous << qgetenv(name.constData());
        const auto cleanup = qScopeGuard([&] {
            QFile::remove(temp.filePath("flush.sra"));
            try {
                closeProject64(exe);
            } catch (...) {
            }
            for (int i = 0; i < envs.size(); ++i) {
                if (previous[i].isNull())
                    qunsetenv(envs[i].constData());
                else
                    qputenv(envs[i].constData(), previous[i]);
            }
            QSettings().clear();
            QCoreApplication::setApplicationName(oldName);
            QCoreApplication::setOrganizationName(oldOrganization);
        });
        const auto read = [](const QString &p) {
            QFile f(p);
            return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
        };
        const auto write = [](const QString &p, const QByteArray &bytes) {
            QDir().mkpath(QFileInfo(p).absolutePath());
            QFile f(p);
            return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size();
        };
        try {
            const auto rom = Rom::open(path);
            ScenarioSeeder model(rom);
            const auto dest = ScenarioSeeder::batteryPath(rom, exe);
            const auto flushed = model.seed(4);
            QVERIFY(write(dest, model.seed(0)));
            QVERIFY(write(temp.filePath("flush.sra"), flushed));
            qputenv(envs[0].constData(), dest.toUtf8());
            qputenv(envs[1].constData(), temp.filePath("flush.sra").toUtf8());
            qputenv(envs[2].constData(), log.toUtf8());
            QSettings().setValue("testing/project64", exe);
            QSettings().setValue("testing/rom", path);
            QCOMPARE(QSettings().value("testing/project64").toString(), exe);
            TestingEditor editor(path);
            editor.findChild<QComboBox *>("testingSourceMode")->setCurrentIndex(1);
            editor.resize(1150, 740);
            editor.show();
            QTRY_VERIFY_WITH_TIMEOUT(editor.ready(), 30000);
            auto list = editor.findChild<QTreeWidget *>("seederMissions");
            auto status = editor.findChild<QLabel *>("seederStatus");
            QVERIFY(!editor.findChild<QPushButton *>("seederLaunch"));
            QCOMPARE(list->topLevelItemCount(), model.choices().size());
            for (int i = 0; i < list->topLevelItemCount(); ++i)
                QVERIFY(qobject_cast<QPushButton *>(list->itemWidget(list->topLevelItem(i), 2)));
            // Selecting a row no longer writes the save or restarts the emulator.
            list->setCurrentItem(list->topLevelItem(1));
            QCOMPARE(read(dest), model.seed(0));
            QProcess old;
            old.start(exe, {path});
            QVERIFY(old.waitForStarted());
            QTRY_VERIFY_WITH_TIMEOUT(read(log).count('\n') == 1, 10000);
            const int choice = 8;
            auto target = list->topLevelItem(choice);
            qobject_cast<QPushButton *>(list->itemWidget(target, 2))->click();
            QTRY_VERIFY_WITH_TIMEOUT(status->text().startsWith("Started:") || list->isEnabled(),
                                     30000);
            QVERIFY2(status->text().startsWith("Started:"), qPrintable(status->text()));
            QTRY_COMPARE_WITH_TIMEOUT(old.state(), QProcess::NotRunning, 10000);
            QTRY_VERIFY_WITH_TIMEOUT(read(log).count('\n') == 2, 10000);
            const auto installed = read(log + ".sra");
            QCOMPARE(SaveDocument::open(rom, installed).memory(),
                     SaveDocument::open(rom, model.seed(choice)).memory());
            QCOMPARE(read(dest), installed);
            const auto backups =
                QDir(QFileInfo(dest).absolutePath()).entryList({"*.bak"}, QDir::Files);
            QCOMPARE(backups.size(), 1);
            QCOMPARE(read(QFileInfo(dest).dir().filePath(backups[0])), flushed);
            const auto boot =
                QJsonDocument::fromJson(read(log).trimmed().split('\n').last()).object();
            QCOMPARE(boot["args"].toArray()[1].toString(), path);
            QVERIFY(status->text().contains(model.choices()[choice].location));
            QVERIFY(editor.grab().save("testing-cold-boot.png"));
            // Selecting both files assigns the entire save and closes/flushes the stand-in
            // without restarting.
            const auto imported = temp.filePath("my-save.sra");
            const auto full = model.seed(12);
            QVERIFY(write(imported, full));
            editor.loadSave(imported);
            QCOMPARE(list->topLevelItemCount(), model.choices().size());
            list->setCurrentItem(list->topLevelItem(5));
            editor.loadSaveRom(path);
            QTRY_VERIFY_WITH_TIMEOUT(list->isEnabled(), 30000);
            QVERIFY2(status->text().startsWith("Save assigned:"), qPrintable(status->text()));
            QVERIFY(!ScenarioSeeder::project64Running());
            QCOMPARE(read(log).count('\n'), 2);
            QCOMPARE(read(dest), full);
            // Reselecting a file explicitly reassigns, without launching.
            QFile::remove(temp.filePath("flush.sra"));
            editor.loadSave(imported);
            QTRY_VERIFY_WITH_TIMEOUT(list->isEnabled(), 30000);
            QVERIFY2(status->text().startsWith("Save assigned:"), qPrintable(status->text()));
            QVERIFY(!ScenarioSeeder::project64Running());
            QCOMPARE(read(log).count('\n'), 2);
            QCOMPARE(read(dest), full);
            // A different installation must be left running, with no save replacement.
            const auto otherDir = temp.filePath("other");
            QVERIFY(QDir().mkpath(otherDir));
            const auto otherExe = QDir(otherDir).filePath("Project64.exe");
            QVERIFY(QFile::copy(exe, otherExe));
            QProcess other;
            const auto stopOther = qScopeGuard([&] {
                try {
                    closeProject64(otherExe);
                } catch (...) {
                }
                other.waitForFinished(3000);
            });
            other.start(otherExe, {path});
            QVERIFY(other.waitForStarted());
            QTRY_VERIFY_WITH_TIMEOUT(read(log).count('\n') == 3, 10000);
            qobject_cast<QPushButton *>(list->itemWidget(list->topLevelItem(3), 2))->click();
            QTRY_VERIFY_WITH_TIMEOUT(list->isEnabled(), 30000);
            QVERIFY2(status->text().contains("Another Project64 installation"),
                     qPrintable(status->text()));
            QCOMPARE(other.state(), QProcess::Running);
            QCOMPARE(read(dest), full);
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void testingDefaultArmy() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        QStandardPaths::setTestModeEnabled(true);
        const auto oldName = QCoreApplication::applicationName();
        QCoreApplication::setApplicationName("Testing-ui-" +
                                             QUuid::createUuid().toString(QUuid::Id128));
        QString defaultPath;
        const auto cleanup = qScopeGuard([&] {
            if (!defaultPath.isEmpty())
                QFile::remove(defaultPath);
            QSettings().clear();
            QCoreApplication::setApplicationName(oldName);
            QStandardPaths::setTestModeEnabled(false);
        });
        try {
            MainWindow window;
            window.resize(1500, 950);
            window.show();
            QTimer messages;
            messages.setInterval(100);
            connect(&messages, &QTimer::timeout, &window, [&] {
                for (auto widget : QApplication::topLevelWidgets())
                    if (auto box = qobject_cast<QMessageBox *>(widget)) {
                        qWarning() << "Unexpected test dialog:" << box->text();
                        box->reject();
                    }
            });
            messages.start();
            window.loadRom(path);
            QTRY_VERIFY_WITH_TIMEOUT(window.findChild<TestingEditor *>() != nullptr, 30000);
            auto seeder = window.findChild<TestingEditor *>();
            auto save = window.findChild<SaveEditor *>();
            auto tabs = window.findChild<QTabWidget *>();
            QVERIFY(save);
            QVERIFY(tabs);
            QCOMPARE(tabs->tabText(tabs->count() - 1), QString("Art Library"));
            QVERIFY(window.findChild<QLabel *>("storylineWipBadge"));
            tabs->setCurrentWidget(seeder);
            QTRY_VERIFY_WITH_TIMEOUT(seeder->ready(), 30000);
            QCOMPARE(seeder->findChild<QTreeWidget *>("seederMissions")->topLevelItemCount(), 58);
            defaultPath = seeder->defaultArmyPath();
            seeder->findChild<QPushButton *>("seederEditArmy")->click();
            QCOMPARE(tabs->currentWidget(), save);
            QVERIFY(save->document());
            QVERIFY(save->edit(SaveTable::Characters, 1, "luck", 33));
            save->findChild<QPushButton *>("saveDefaultArmy")->click();
            QCOMPARE(tabs->currentWidget(), seeder);
            QFile f(defaultPath);
            QVERIFY(f.open(QIODevice::ReadOnly));
            auto army = SaveDocument::open(save->project()->rom(), f.readAll());
            f.close();
            QCOMPARE(army.value(SaveTable::Characters, 1, "luck").toInt(), 33);
            seeder->findChild<QPushButton *>("seederEditArmy")->click();
            QCOMPARE(save->document()->value(SaveTable::Characters, 1, "luck").toInt(), 33);
            save->findChild<QPushButton *>("saveDefaultArmy")->click();
            QTest::qWait(100);
            QVERIFY(window.grab().save("testing.png"));
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void inheritedSceneReplay_data() {
        QTest::addColumn<quint32>("key");
        QTest::addColumn<int>("mode");
        QTest::newRow("world") << 0x19abed8U << 2;
        QTest::newRow("room") << 0x19b46a6U << 0;
        QTest::newRow("ending-high") << 0x19f4e1aU << 2;
        QTest::newRow("ending-middle") << 0x19f4ebaU << 2;
    }
    void inheritedSceneReplay() {
        QFETCH(quint32, key);
        QFETCH(int, mode);
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        const auto scenes = ScenePrograms::load(rom);
        ScenePreview preview(rom);
        preview.resize(1250, 950);
        preview.show();
        preview.selectScene(scenes, key, mode);
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        QTRY_VERIFY_WITH_TIMEOUT(!preview.property("scenePreparing").toBool(), 30000);
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        QCOMPARE(preview.property("sceneTick").toInt(), 0);
        QVERIFY(preview.findChild<QComboBox *>("sceneLaunchContext")->isVisible());
        QVERIFY(preview.findChild<QComboBox *>("sceneLiveActors")->count() > 0);
        const auto initial = preview.renderedFrame();
        QVERIFY(!initial.isNull());
        auto step = preview.findChild<QPushButton *>("sceneStep");
        auto confirm = preview.findChild<QPushButton *>("panelConfirm");
        QMap<int, QImage> frames;
        for (int i = 0; i < 5000 && !preview.property("sceneStopped").toBool(); ++i) {
            if (i % 45 == 0 && confirm->isVisible())
                confirm->click();
            else
                step->click();
            if (i == 49 || i % 500 == 499)
                frames.insert(preview.property("sceneTick").toInt(), preview.renderedFrame());
            if (i == 49) {
                const auto frame = preview.renderedFrame();
                int visible = 0;
                for (int y = 0; y < frame.height(); ++y)
                    for (int x = 0; x < frame.width(); ++x) {
                        const auto color = frame.pixelColor(x, y);
                        visible += color.alpha() && (color.red() || color.green() || color.blue());
                    }
                QVERIFY2(visible > 100, "Inherited scene must show its scenery or dialogue before fading out");
                QVERIFY(frame.save(QString("native-inherited-%1-early.png").arg(QString::number(key,16))));
            }
        }
        QVERIFY2(status->text().contains("Scene released.") ||
                     status->text().contains("Scene returned to the game"),
                 qPrintable(status->text()));
        qInfo() << QString::number(key, 16) << "finished" << preview.property("sceneTick");
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        position->setValue(0);
        QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), 0, 20000);
        QCOMPARE(preview.renderedFrame(), initial);
        for (auto it = frames.cbegin(); it != frames.cend(); ++it) {
            position->setValue(it.key());
            QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), it.key(), 20000);
            QCOMPARE(preview.renderedFrame(), it.value());
        }
        preview.grab().save(QString("native-inherited-%1.png").arg(QString::number(key, 16)));
    }
    void inheritedSceneEventEdits() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadSceneEvents();
        auto programs = *project.scenePrograms();
        const auto id = programs.createScene("Replacement launch", 0x1a14970, true);
        project.setScenePrograms(programs.json());
        QUndoStack undo;
        CutsceneEditor editor(project.rom(), nullptr, &project, &undo);
        editor.resize(1200, 800);
        editor.show();
        auto scenes = editor.findChild<QListWidget *>("cutsceneScenes");
        QTRY_COMPARE_WITH_TIMEOUT(scenes->count(), 1499, 10000);
        for (int i = 0; i < scenes->count(); ++i)
            if (scenes->item(i)->data(Qt::UserRole + 1).toString() == id.authoredId)
                scenes->setCurrentRow(i);
        auto contexts = editor.findChild<QComboBox *>("sceneLaunchContext");
        QCOMPARE(contexts->currentData().toString(), QString("direct"));
        auto sites = editor.findChild<QTableWidget *>("sceneEventSites");
        for (int i = 0; i < sites->rowCount(); ++i)
            if (sites->item(i, 0)->text() == "event-director:4:b0058")
                sites->setCurrentCell(i, 0);
        auto target = editor.findChild<QComboBox *>("sceneEventTarget");
        target->setCurrentIndex(target->findData(programs.resource(id).selectors.first()));
        editor.findChild<QPushButton *>("sceneEventAssign")->click();
        QVERIFY(contexts->currentData().toString().startsWith("event-director:4:b0058:"));
        undo.undo();
        QCOMPARE(contexts->currentData().toString(), QString("direct"));
        undo.redo();
        QVERIFY(contexts->currentData().toString().startsWith("event-director:4:b0058:"));
    }
    void partySceneReplay_data() {
        QTest::addColumn<quint32>("offset");
        QTest::addColumn<quint32>("key");
        QTest::newRow("preserved") << 0x1f450d0U << 0x19b0e4cU;
        QTest::newRow("preserved-return") << 0x1f4c45aU << 0x19b81d6U;
        QTest::newRow("encounter") << 0x1f40650U << 0x19ac3ccU;
        QTest::newRow("formation") << 0x1f3f242U << 0x19aafc2U;
        QTest::newRow("world-party") << 0x1fa4d0aU << 0x1a10a8aU;
    }
    void partySceneReplay() {
        QFETCH(quint32, offset);
        QFETCH(quint32, key);
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        const auto scenes = ScenePrograms::load(rom);
        const auto before = scenes.json();
        ScenePreview preview(rom);
        preview.resize(1250, 950);
        preview.show();
        auto step = preview.findChild<QPushButton *>("sceneStep");
        auto confirm = preview.findChild<QPushButton *>("panelConfirm");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        auto objects = preview.findChild<QComboBox *>("sceneLiveActors");
        {
            preview.selectScene(scenes, key, 2);
            QMap<int, QImage> frames;
            int mostActors = 0;
            for (int i = 0; i < 5000 && !preview.property("sceneStopped").toBool(); ++i) {
                if (i % 45 == 0 && confirm->isVisible())
                    confirm->click();
                else
                    step->click();
                mostActors = qMax(mostActors, objects->count());
                if (i == 49 || i % 250 == 249)
                    frames.insert(preview.property("sceneTick").toInt(), preview.renderedFrame());
            }
            QVERIFY2(status->text().contains(offset == 0x1f40650U ? "Scene returned to the game"
                                                                  : "Scene released."),
                     qPrintable(QString("%1: %2").arg(offset, 0, 16).arg(status->text())));
            QVERIFY(mostActors >= 2);
            QVERIFY(!frames.isEmpty());
            qInfo() << QString::number(offset, 16) << "released at" << preview.property("sceneTick")
                    << "with up to" << mostActors << "Actors";
            auto position = preview.findChild<QSpinBox *>("scenePosition");
            for (auto it = frames.cbegin(); it != frames.cend(); ++it) {
                position->setValue(it.key());
                QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), it.key(), 20000);
                QCOMPARE(preview.renderedFrame(), it.value());
            }
            if (offset == 0x1fa4d0aU) {
                QApplication::processEvents();
                preview.grab().save("native-party-preview.png");
            }
        }
        QCOMPARE(scenes.json(), before);
        QCOMPARE(Rom::open(path).bytes(), rom.bytes());
    }
    void sceneCompletionControls() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto scenes = *project.scenePrograms();
            const auto id = scenes.createScene("Completion controls", 0x1fa8bf4 - 0x594284, true);
            project.setScenePrograms(scenes.json());
            QUndoStack undo;
            CutsceneEditor editor(project.rom(), nullptr, &project, &undo);
            editor.resize(1400, 850);
            editor.show();
            auto list = editor.findChild<QListWidget *>("cutsceneScenes");
            QTRY_VERIFY_WITH_TIMEOUT(list->count() > 1000, 10000);
            for (int i = 0; i < list->count(); ++i)
                if (list->item(i)->data(Qt::UserRole + 1).toString() == id.authoredId) {
                    list->setCurrentRow(i);
                    break;
                }
            auto actions = editor.findChild<SceneActionEditor *>();
            editor.findChild<QTabWidget *>("cutsceneDetailPages")->setCurrentWidget(actions);
            auto kind = actions->findChild<QComboBox *>("sceneActionKind");
            auto position = actions->findChild<QComboBox *>("sceneActionPosition");
            auto add = actions->findChild<QPushButton *>("sceneActionInsert");
            auto field = [&](int i) {
                return actions->findChild<QLineEdit *>(QString("sceneActionField%1").arg(i));
            };
            for (auto type : {7U, 0x1bU, 0x2cU}) {
                position->setCurrentIndex(position->count() - 1);
                kind->setCurrentIndex(kind->findData(type));
                QVERIFY(add->isEnabled());
                add->click();
                const int at = actions->findChild<QListWidget *>("sceneActionList")
                                   ->currentItem()
                                   ->data(Qt::UserRole)
                                   .toInt();
                const auto before = project.scenePrograms()->program(id).source;
                auto wait = actions->findChild<QCheckBox *>("sceneActionWait");
                QVERIFY(wait && wait->isEnabled());
                QVERIFY(!wait->isChecked());
                wait->click();
                const auto after = project.scenePrograms()->program(id).source;
                QVERIFY(after != before);
                undo.undo();
                QCOMPARE(project.scenePrograms()->program(id).source, before);
                undo.redo();
                QCOMPARE(project.scenePrograms()->program(id).source, after);
                actions->selectScene(id, at);
                QVERIFY(actions->findChild<QCheckBox *>("sceneActionWait")->isChecked());
                if (type == 0x2c) {
                    field(0)->setText("15");
                    field(2)->setText("1.25");
                    field(4)->setText("12");
                    const auto p = project.scenePrograms()->program(id);
                    QCOMPARE(p.commands[at].operands[0], quint32(15000));
                    QCOMPARE(p.commands[at + 5].operands[1], quint32(12));
                }
            }
            QApplication::processEvents();
            QVERIFY(editor.grab().save("native-action-completion.png"));
            QTemporaryDir work;
            project.saveFolder(work.path());
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.path()));
            QCOMPARE(restored.scenePrograms()->program(id).source,
                     project.scenePrograms()->program(id).source);
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void sceneChoiceAuthoring() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            project.loadDialogues();
            auto scenes = *project.scenePrograms();
            const auto id =
                scenes.createScene("Desktop two-option choice", 0x1fa8bf4 - 0x594284, true);
            project.setScenePrograms(scenes.json());
            const auto blank = scenes.program(id);
            const auto originalText = project.dialogues()->json();
            QUndoStack undo;
            SceneActionEditor editor(&project, &undo, [] {});
            editor.resize(1150, 650);
            editor.show();
            editor.selectScene(id, SceneActions::positions(blank).last().command);
            auto kind = editor.findChild<QComboBox *>("sceneActionKind");
            auto position = editor.findChild<QComboBox *>("sceneActionPosition");
            auto add = editor.findChild<QPushButton *>("sceneActionInsert");
            auto editOptions = [&](const QString &first, const QString &second, bool accept) {
                QTimer::singleShot(0, &editor, [&, first, second, accept] {
                    auto dialog = editor.findChild<QDialog *>("sceneChoiceDialog");
                    QVERIFY(dialog);
                    auto a = dialog->findChild<QLineEdit *>("sceneChoiceOption1");
                    auto b = dialog->findChild<QLineEdit *>("sceneChoiceOption2");
                    auto buttons = dialog->findChild<QDialogButtonBox *>();
                    a->setText("@");
                    QVERIFY(!buttons->button(QDialogButtonBox::Ok)->isEnabled());
                    a->setText(first);
                    b->setText(second);
                    QVERIFY(buttons->button(QDialogButtonBox::Ok)->isEnabled());
                    buttons->button(accept ? QDialogButtonBox::Ok : QDialogButtonBox::Cancel)
                        ->click();
                });
            };
            kind->setCurrentIndex(kind->findData(SceneActions::Choice));
            QVERIFY(add->isEnabled());
            editOptions("Continue", "Return", false);
            add->click();
            QCOMPARE(undo.count(), 0);
            QCOMPARE(project.dialogues()->json(), originalText);
            editOptions("Continue", "Return", true);
            add->click();
            QCOMPARE(undo.count(), 1);
            const auto added = project.scenePrograms()->program(id);
            QCOMPARE(SceneActions::choices(added).size(), 1);
            auto choice = SceneActions::choices(added).first();
            const int entry = added.commands[choice.command].operands[2];
            QCOMPARE(SceneActions::choiceOptions(
                         project.dialogues()->entry(Dialogues::Table::Primary, 5, entry))
                         ->first,
                     QString("Continue"));
            undo.undo();
            QCOMPARE(project.scenePrograms()->program(id).source, blank.source);
            QCOMPARE(project.dialogues()->json(), originalText);
            undo.redo();
            QCOMPARE(project.scenePrograms()->program(id).source, added.source);
            editor.selectScene(id, choice.command);
            editOptions("Proceed", "Go back", true);
            editor.findChild<QPushButton *>("sceneActionEditText")->click();
            QCOMPARE(undo.count(), 2);
            QCOMPARE(SceneActions::choiceOptions(
                         project.dialogues()->entry(Dialogues::Table::Primary, 5, entry))
                         ->second,
                     QString("Go back"));
            undo.undo();
            QCOMPARE(SceneActions::choiceOptions(
                         project.dialogues()->entry(Dialogues::Table::Primary, 5, entry))
                         ->second,
                     QString("Return"));
            undo.redo();
            for (int option : {1, 2}) {
                choice = SceneActions::choices(project.scenePrograms()->program(id)).first();
                int end = option == 1 ? choice.firstEnd : choice.secondEnd;
                editor.selectScene(id, choice.command);
                position->setCurrentIndex(position->findData(end));
                QVERIFY(position->currentText().contains(QString("After option %1").arg(option)));
                kind->setCurrentIndex(kind->findData(0x1b));
                QVERIFY(add->isEnabled());
                add->click();
                editor.findChild<QLineEdit *>("sceneActionField0")->setText("1");
                editor.findChild<QLineEdit *>(QString("sceneActionField%1").arg(option))
                    ->setText("255");
                editor.findChild<QLineEdit *>("sceneActionField4")->setText("0");
                editor.findChild<QLineEdit *>("sceneActionField5")->setText("255");
                choice = SceneActions::choices(project.scenePrograms()->program(id)).first();
                end = option == 1 ? choice.firstEnd : choice.secondEnd;
                position->setCurrentIndex(position->findData(end));
                kind->setCurrentIndex(kind->findData(0xe));
                QVERIFY(add->isEnabled());
                add->click();
                editor.findChild<QLineEdit *>("sceneActionField0")->setText("3");
            }
            const auto complete = project.scenePrograms()->program(id).source;
            editor.selectScene(
                id, SceneActions::choices(project.scenePrograms()->program(id)).first().firstBegin);
            QVERIFY(editor.findChild<QListWidget *>("sceneActionList")
                        ->currentItem()
                        ->text()
                        .startsWith("Option 1"));
            QApplication::processEvents();
            QVERIFY(editor.findChild<QLineEdit *>("sceneActionField0")->isVisible());
            QVERIFY(editor.grab().save("native-choice-authoring.png"));
            QTemporaryDir work;
            project.saveFolder(work.path());
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.path()));
            QCOMPARE(restored.scenePrograms()->program(id).source, complete);
            QImage outcomes[2];
            for (int option : {1, 2}) {
                ScenePreview preview(restored.rom());
                preview.resize(1250, 900);
                preview.show();
                preview.selectScene(*restored.scenePrograms(), id, 2, restored.dialogues());
                auto step = preview.findChild<QPushButton *>("sceneStep");
                QVERIFY(step);
                auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
                for (int i = 0; i < 300 && preview.property("sceneDialogueState").toInt() != 7; ++i)
                    step->click();
                QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
                QCOMPARE(preview.property("sceneDialogueState").toInt(), 7);
                if (option == 2)
                    preview.findChild<QPushButton *>("panelDown")->click();
                preview.findChild<QPushButton *>("panelConfirm")->click();
                for (int i = 0; i < 200 && !preview.property("sceneStopped").toBool(); ++i)
                    step->click();
                QVERIFY2(status->text().contains("Scene released"), qPrintable(status->text()));
                outcomes[option - 1] = preview.renderedFrame();
                // Imported automatic selection must reach the same visible branch as manual input.
                QFile inputFile(QDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()).filePath("../../tests/fixtures/cutscene-room-input.json"));
                QVERIFY(inputFile.open(QIODevice::ReadOnly));
                auto input = QJsonDocument::fromJson(inputFile.readAll()).object();
                input["assetId"] = preview.playbackAssetId(); input.remove("actorInputRows");
                auto group = input["externalProducers"].toObject(); auto services = group["value"].toObject();
                for (const auto key : {"directorLaunch", "sharedActor", "mapMenu", "imageEcho", "framebuffer"}) services.remove(key);
                auto schedule = services["resourceSchedule"].toObject();
                schedule["pageAdvancePolicy"] = "automatic";
                schedule["dialogueChoicePolicy"] = option == 1 ? "first" : "second";
                schedule["controller"] = QJsonObject{{"throughPass", 3999}, {"changes", QJsonArray{
                    QJsonObject{{"pass",0},{"actionMask",0},{"directionMask",0},{"historyMask",0},{"dummyMask",0}}}}};
                services["resourceSchedule"] = schedule; group["value"] = services; input["externalProducers"] = group;
                preview.importPlaybackInputs(QJsonDocument(input).toJson(QJsonDocument::Compact));
                preview.seekToUpdate(500);
                QTRY_VERIFY_WITH_TIMEOUT(preview.property("sceneStopped").toBool(), 10000);
                QVERIFY2(status->text().contains("Scene released"), qPrintable(status->text()));
                QCOMPARE(preview.renderedFrame(), outcomes[option - 1]);
            }
            QVERIFY(outcomes[0] != outcomes[1]);
            QCOMPARE(project.scenePrograms()->program(id).source, complete);
            editor.selectScene(
                id, SceneActions::choices(project.scenePrograms()->program(id)).first().command);
            editor.findChild<QPushButton *>("sceneActionRemove")->click();
            QCOMPARE(project.scenePrograms()->program(id).source, blank.source);
            undo.undo();
            QCOMPARE(project.scenePrograms()->program(id).source, complete);
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void sceneActionExecutionTimeline() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadScenePrograms();
        auto programs = *project.scenePrograms();
        const auto id = programs.createScene("Timed editing", 0x1fa8bf4 - 0x594284, true);
        auto program = SceneActions::insert(programs.program(id), 1, 0xe, 5);
        int exit = -1;
        for (int i = 0; i < program.commands.size(); ++i) if (program.commands[i].opcode == 0x7d) { exit = i; break; }
        QVERIFY(exit >= 0);
        program = SceneActions::insert(program, exit, 0x1b);
        programs.replace(id, program.source); project.setScenePrograms(programs.json());
        const auto before = project.scenePrograms()->json();
        QUndoStack undo;
        CutsceneEditor editor(project.rom(), nullptr, &project, &undo);
        editor.resize(1300, 900); editor.show();
        auto scenes = editor.findChild<QListWidget *>("cutsceneScenes");
        QTRY_COMPARE_WITH_TIMEOUT(scenes->count(), 1499, 10000);
        for (int i = 0; i < scenes->count(); ++i) if (scenes->item(i)->data(Qt::UserRole + 1).toString() == id.authoredId) scenes->setCurrentRow(i);
        auto actions = editor.findChild<SceneActionEditor *>();
        auto list = actions->findChild<QListWidget *>("sceneActionList");
        auto preview = editor.findChild<ScenePreview *>();
        editor.findChild<QTabWidget *>("cutsceneDetailPages")->setCurrentWidget(actions);
        int fade = -1, wait = -1;
        for (int i = 0; i < list->count(); ++i) {
            const auto opcode = program.commands[list->item(i)->data(Qt::UserRole).toInt()].opcode;
            if (opcode == 0x1b) fade = i;
            if (opcode == 1) wait = i;
        }
        QVERIFY(fade >= 0 && wait >= 0);
        const int fadeWord = program.commands[list->item(fade)->data(Qt::UserRole).toInt()].word;
        list->setCurrentRow(-1); list->setCurrentRow(fade);
        QTRY_COMPARE_WITH_TIMEOUT(preview->property("sceneTick").toInt(), 6, 10000);
        QCOMPARE(preview->commandTimes().value(fadeWord, -1), 6);
        QVERIFY(list->item(fade)->text().contains("update 6 (0.10 s)"));
        list->setCurrentRow(wait);
        QTRY_COMPARE(preview->property("sceneTick").toInt(), 1);
        QVERIFY(list->item(wait)->text().startsWith("At playhead"));
        preview->seekToCommand(fadeWord);
        QTRY_COMPARE(preview->property("sceneTick").toInt(), 6);
        const auto text = list->item(fade)->text();
        actions->updateTimeline(SceneId(123), {{fadeWord, 999}}, fadeWord, 999);
        QCOMPARE(list->item(fade)->text(), text);
        QCOMPARE(project.scenePrograms()->json(), before); QCOMPARE(undo.count(), 0);
    }
    void importedPlaybackInputs() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path)); project.loadScenePrograms();
            auto programs = *project.scenePrograms();
            const auto id = programs.createScene("Imported actor playback", 0x1fa8bf4 - 0x594284, true);
            auto importedProgram = SceneActions::insert(programs.program(id), 1, 0xe, 10);
            const int queryIndex = SceneActions::positions(importedProgram).last().command;
            importedProgram.commands.insert(queryIndex, {0, 0x11, {0, 0, 0}});
            importedProgram.commands.insert(queryIndex + 1, {0, 0x80000002, {}});
            importedProgram.commands.insert(queryIndex + 2, {0, 0x80000000, {}});
            importedProgram = DirectorProgram::decode(importedProgram.encode());
            const int queryWord = importedProgram.commands[queryIndex].word;
            programs.replace(id, importedProgram.source);
            project.setScenePrograms(programs.json());
            const auto unchanged = project.manifest();
            QUndoStack undo;
            CutsceneEditor editor(project.rom(), nullptr, &project, &undo);
            editor.resize(1400, 900); editor.show();
            auto scenes = editor.findChild<QListWidget *>("cutsceneScenes");
            QTRY_COMPARE_WITH_TIMEOUT(scenes->count(), 1499, 10000);
            for (int i = 0; i < scenes->count(); ++i)
                if (scenes->item(i)->data(Qt::UserRole + 1).toString() == id.authoredId) scenes->setCurrentRow(i);
            auto preview = editor.findChild<ScenePreview *>();
            editor.findChild<QTabWidget *>("cutsceneDetailPages")->setCurrentWidget(preview);
            auto instance = ActorInstance::create(project.rom(), {0, 1, 0, 0, 0, {10, 7, 20}, 0, 0}, 2);
            auto record = instance.record();
            QByteArray movement(16, 0); put32(movement, 0, std::bit_cast<quint32>(2.f)); put16(movement, 12, 5);
            QJsonArray entries;
            entries << QJsonObject{{"identity", "imported-actor"}, {"recordHex", QString::fromLatin1(record.toHex())},
                                   {"movementHex", QString::fromLatin1(movement.toHex())}};
            for (int i = 1; i < 28; ++i) entries << QJsonValue(QJsonValue::Null);
            auto known = [](QJsonValue value) { return QJsonObject{{"status", "known"}, {"value", value}}; };
            QJsonObject input{{"schema", "ob64-cutscene-launch-inputs.v1"}, {"assetId", preview->playbackAssetId()},
                {"invocationId", "native-desktop-import"}, {"sourceIdentity", "Explicit position/movement fixture"},
                {"evidenceGrade", "Candidate"}, {"schedulerBranch", known("normal")},
                {"existingActors", known(QJsonObject{{"slots", entries}, {"otherJobsEmpty", true}})}};
            preview->importPlaybackInputs(QJsonDocument(input).toJson());
            auto state = preview->findChild<QLabel *>("sceneActorState");
            QVERIFY2(state->text().contains("X 10.000 Â· Y 7.000 Â· Z 20.000"), qPrintable(state->text()));
            preview->seekToUpdate(1); QTRY_COMPARE(preview->property("sceneTick").toInt(), 1);
            QVERIFY2(state->text().contains("X 12.000 Â· Y 7.000 Â· Z 20.000"), qPrintable(state->text()));
            const auto frame = preview->renderedFrame();
            auto bad = input; bad["assetId"] = "wrong-resource";
            QVERIFY_THROWS_EXCEPTION(Error, preview->importPlaybackInputs(QJsonDocument(bad).toJson()));
            QVERIFY_THROWS_EXCEPTION(Error, preview->importPlaybackInputs("{broken"));
            QVERIFY_THROWS_EXCEPTION(Error, preview->importPlaybackInputs(QByteArray(131073, ' ')));
            auto unresolved = input;
            auto invalidRecord = record; put32(invalidRecord, 0xe8, 9999);
            auto invalidEntries = entries; auto invalidActor = entries[0].toObject();
            invalidActor["recordHex"] = QString::fromLatin1(invalidRecord.toHex()); invalidEntries[0] = invalidActor;
            unresolved["existingActors"] = known(QJsonObject{{"slots", invalidEntries}, {"otherJobsEmpty", true}});
            QVERIFY_THROWS_EXCEPTION(Error, preview->importPlaybackInputs(QJsonDocument(unresolved).toJson()));
            QCOMPARE(preview->property("sceneTick").toInt(), 1); QCOMPARE(preview->renderedFrame(), frame);
            preview->seekToUpdate(0); QTRY_COMPARE(preview->property("sceneTick").toInt(), 0);
            preview->seekToUpdate(1); QTRY_COMPARE(preview->property("sceneTick").toInt(), 1);
            QCOMPARE(preview->renderedFrame(), frame);
            input.remove("existingActors");
            input["capturedSnapshot"] = known(QJsonObject{{"slots", entries}, {"sceneMode", 2},
                {"observedParserCursor", 123}, {"resumeState", "unknown"}, {"otherJobOwners", "unknown"}});
            preview->importPlaybackInputs(QJsonDocument(input).toJson());
            QCOMPARE(preview->property("sceneTick").toInt(), 0);
            QVERIFY(preview->property("sceneStopped").toBool());
            QVERIFY(state->text().contains("X 10.000 Â· Y 7.000 Â· Z 20.000"));
            QVERIFY(preview->findChild<QLabel *>("scenePlaybackStatus")->text().contains("no elapsed updates"));
            // The qualified one-update profile supplies a real held query and disjoint owners.
            auto captured = input["capturedSnapshot"].toObject()["value"].toObject();
            captured["sceneMode"] = 0; captured["observedParserCursor"] = queryWord;
            input["capturedSnapshot"] = known(captured);
            QByteArray primary(0x1cb2, 0); put32(primary, 0x18, 0x80320000); put32(primary, 0xf8, 0x80320400);
            QJsonArray menuOwners; for (int i = 0; i < 14; ++i) menuOwners << QJsonValue(QJsonValue::Null);
            QJsonObject resume{{"entry", "normal-director-held-movement-query"}, {"origin", "prospective-captured-state"},
                {"updates", 1}, {"registeredCounter", 0}, {"tailTimer", -1},
                {"primaryOwnerAddress", qint64(0x80300000)}, {"primaryOwnerHex", QString::fromLatin1(primary.toHex())},
                {"secondaryOwnerAddress", qint64(0x80310000)}, {"secondaryOwnerHex", QString::fromLatin1(QByteArray(0x844,0).toHex())},
                {"menuRootAddress", qint64(0x80330000)}, {"menuRootHex", QString::fromLatin1(QByteArray(0xd8,0).toHex())}, {"menuOwners", menuOwners}};
            input["capturedResume"] = known(resume);
            preview->importPlaybackInputs(QJsonDocument(input).toJson());
            QVERIFY(!preview->property("sceneStopped").toBool());
            preview->seekToUpdate(10); QTRY_VERIFY(preview->property("sceneStopped").toBool());
            QCOMPARE(preview->property("sceneTick").toInt(), 1);
            QVERIFY2(state->text().contains("X 12.000 Â· Y 7.000 Â· Z 20.000"), qPrintable(state->text()));
            QVERIFY(preview->findChild<QLabel *>("scenePlaybackStatus")->text().contains("declared 1 updates"));
            const auto resumedFrame = preview->renderedFrame();
            auto endingMovement = movement; put16(endingMovement, 12, 1);
            auto endingEntries = entries; auto endingActor = endingEntries[0].toObject();
            endingActor["movementHex"] = QString::fromLatin1(endingMovement.toHex()); endingEntries[0] = endingActor;
            captured["slots"] = endingEntries; auto badResume = input; badResume["capturedSnapshot"] = known(captured);
            QVERIFY_THROWS_EXCEPTION(Error, preview->importPlaybackInputs(QJsonDocument(badResume).toJson()));
            QCOMPARE(preview->renderedFrame(), resumedFrame);
            preview->findChild<QPushButton *>("sceneClearPlaybackInputs")->click();
            QVERIFY(!preview->property("sceneStopped").toBool());
            QCOMPARE(preview->findChild<QComboBox *>("sceneLiveActors")->count(), 0);
            const auto context = preview->findChild<QLabel *>("scenePreviewContext")->text();
            QVERIFY_THROWS_EXCEPTION(Error, preview->importPlaybackInputs(QJsonDocument(unresolved).toJson()));
            QCOMPARE(preview->findChild<QLabel *>("scenePreviewContext")->text(), context);
            QCOMPARE(project.manifest(), unchanged); QCOMPARE(undo.count(), 0);
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void importedRoomLaunch() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            const auto programs = ScenePrograms::load(rom);
            QFile file(QDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()).filePath("../../tests/fixtures/cutscene-room-input.json"));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto data = file.readAll();
            ScenePreview preview(rom); preview.resize(1000, 900); preview.show();
            preview.selectScene(programs, programs.original().selectors()[39], 0);
            QCOMPARE(preview.playbackAssetId(), QString("rom-director:01F4558C"));
            const auto inputs = ScenePlaybackInputs::parse(data, preview.playbackAssetId());
            ScenePlaybackStart start; start.mode = 0;
            auto cameras = SceneCameras::freshModeTwo(rom);
            inputs.configure(start, cameras);
            QFile expected(QDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()).filePath("../../tests/fixtures/cutscene-director-launch.json"));
            QVERIFY(expected.open(QIODevice::ReadOnly));
            const auto prefix = QByteArray::fromHex(QJsonDocument::fromJson(expected.readAll()).object()["prefix"].toObject()["cameraHex"].toString().toLatin1());
            QCOMPARE(cameras.actor().bank(), prefix.first(56));
            QCOMPARE(cameras.registered().bank(), prefix.mid(88, 56));
            preview.importPlaybackInputs(data);
            auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
            QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
            preview.seekToUpdate(600);
            QTRY_VERIFY_WITH_TIMEOUT(preview.property("sceneTick").toInt() == 600 || preview.property("sceneStopped").toBool(), 30000);
            QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
            QVERIFY(preview.findChild<QComboBox *>("sceneLiveActors")->count() > 0);
            const auto frame = preview.renderedFrame();
            preview.seekToUpdate(500); QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), 500, 30000);
            preview.seekToUpdate(600); QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), 600, 30000);
            QCOMPARE(preview.renderedFrame(), frame);
            QApplication::processEvents();
            QVERIFY2(preview.rect().contains(preview.findChild<QComboBox *>("sceneLiveActors")->geometry()), "Live Actor controls must remain within the preview");
            QVERIFY(preview.grab().save("native-imported-room.png"));
            preview.seekToUpdate(4000);
            QTRY_VERIFY_WITH_TIMEOUT(preview.property("sceneStopped").toBool(), 60000);
            QVERIFY2(status->text().contains("Scene released"), qPrintable(status->text()));
            qInfo() << "Imported room released at update" << preview.property("sceneTick").toInt();
            auto omitted = QJsonDocument::fromJson(data).object();
            auto group = omitted["externalProducers"].toObject(); auto services = group["value"].toObject();
            auto drawing = services["framebuffer"].toObject(); drawing["backgroundPolicy"] = "omit";
            services["framebuffer"] = drawing; group["value"] = services; omitted["externalProducers"] = group;
            preview.importPlaybackInputs(QJsonDocument(omitted).toJson(QJsonDocument::Compact));
            preview.seekToUpdate(600); QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), 600, 30000);
            QVERIFY(preview.renderedFrame() != frame);
            auto confirm = preview.findChild<QPushButton *>("panelConfirm");
            QVERIFY(confirm->isVisible() && confirm->isEnabled());
            confirm->click();
            QCOMPARE(preview.property("sceneTick").toInt(), 601);
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void importedGraduationCapture_data() {
        QTest::addColumn<bool>("continuous");
        QTest::newRow("held") << false;
        QTest::newRow("continuous") << true;
    }
    void importedGraduationCapture() {
        QFETCH(bool, continuous);
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            const auto programs = ScenePrograms::load(rom);
            QFile file(QDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()).filePath(continuous ? "../../tests/fixtures/cutscene-graduation-continuous.json" : "../../tests/fixtures/cutscene-graduation-resume.json"));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto fixture = QJsonDocument::fromJson(file.readAll()).object();
            ScenePreview preview(rom); preview.resize(1000, 900); preview.show();
            preview.selectScene(programs, programs.original().selectors()[2], 2);
            preview.importPlaybackInputs(QJsonDocument(fixture["input"].toObject()).toJson(QJsonDocument::Compact));
            auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
            QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
            preview.seekToUpdate(69);
            QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), 69, 30000);
            if (!continuous) QVERIFY2(status->text().contains("declared 69 updates"), qPrintable(status->text()));
            auto objects = preview.findChild<QComboBox *>("sceneLiveActors");
            QCOMPARE(objects->count(), 8); objects->setCurrentIndex(objects->findData(1));
            const auto expected = fixture["expected"].toArray()[68].toObject();
            QCOMPARE(expected["update"].toInt(), 69);
            QByteArray record;
            for (const auto &entry : expected["actors"].toArray()) if (entry.toObject()["slot"].toInt() == 1)
                record = QByteArray::fromHex(entry.toObject()["recordHex"].toString().toLatin1());
            QCOMPARE(record.size(), 336);
            const auto position = QString("X %1 Â· Y %2 Â· Z %3")
                .arg(std::bit_cast<float>(u32(record, 0x11c)),0,'f',3)
                .arg(std::bit_cast<float>(u32(record, 0x120)),0,'f',3)
                .arg(std::bit_cast<float>(u32(record, 0x124)),0,'f',3);
            QVERIFY2(preview.findChild<QLabel *>("sceneActorState")->text().contains(position),
                     qPrintable(preview.findChild<QLabel *>("sceneActorState")->text()));
            const auto frame = preview.renderedFrame();
            preview.seekToUpdate(1); QTRY_COMPARE(preview.property("sceneTick").toInt(), 1);
            preview.seekToUpdate(69); QTRY_COMPARE(preview.property("sceneTick").toInt(), 69);
            QCOMPARE(preview.renderedFrame(), frame);
            QVERIFY(preview.grab().save("native-imported-graduation.png"));
            if (continuous) {
                preview.seekToUpdate(1000);
                QTRY_VERIFY_WITH_TIMEOUT(preview.property("sceneStopped").toBool(), 30000);
                QVERIFY2(status->text().contains("Scene released"), qPrintable(status->text()));
                QVERIFY(preview.property("sceneTick").toInt() < 1000);
                qInfo() << "Imported continuous capture released at update" << preview.property("sceneTick").toInt();
                auto neutral = fixture["input"].toObject();
                auto group = neutral["capturedResume"].toObject(); auto resume = group["value"].toObject();
                auto services = resume["resourceServices"].toObject(); auto schedule = services["resourceSchedule"].toObject();
                schedule["pageAdvancePolicy"] = "neutral"; services["resourceSchedule"] = schedule;
                resume["resourceServices"] = services; group["value"] = resume; neutral["capturedResume"] = group;
                preview.importPlaybackInputs(QJsonDocument(neutral).toJson(QJsonDocument::Compact));
                preview.seekToUpdate(400); QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), 400, 30000);
                QVERIFY(!preview.property("sceneStopped").toBool());
                QVERIFY(preview.property("sceneDialogueState").toInt() == 3 || preview.property("sceneDialogueState").toInt() == 6);
            }
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void sceneTimedPlacementAndHold() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty()) QSKIP("Private ROM and export tools required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto programs = *project.scenePrograms();
            const auto id = programs.createScene("Timed placement and hold", 0x1fa8bf4 - 0x594284, true);
            auto p = SceneActions::insert(programs.program(id), 1, 0x2a);
            p = SceneActions::edit(p, 1, 1, "84");
            p = SceneActions::edit(p, 1, 2, "85");
            p = SceneActions::insert(p, SceneActions::positions(p).last().command, 0xe, 5);
            programs.replace(id, p.source); project.setScenePrograms(programs.json());
            const auto original = p.source;
            QUndoStack undo;
            CutsceneEditor editor(project.rom(), nullptr, &project, &undo);
            editor.resize(1400, 900); editor.show();
            auto scenes = editor.findChild<QListWidget *>("cutsceneScenes");
            QTRY_COMPARE_WITH_TIMEOUT(scenes->count(), 1499, 10000);
            for (int i = 0; i < scenes->count(); ++i)
                if (scenes->item(i)->data(Qt::UserRole + 1).toString() == id.authoredId) scenes->setCurrentRow(i);
            auto actions = editor.findChild<SceneActionEditor *>();
            auto preview = editor.findChild<ScenePreview *>();
            auto pages = editor.findChild<QTabWidget *>("cutsceneDetailPages");
            pages->setCurrentWidget(preview);
            preview->seekToUpdate(3);
            QTRY_COMPARE(preview->property("sceneTick").toInt(), 3);
            auto state = preview->findChild<QLabel *>("sceneActorState");
            const auto before = preview->renderedFrame();
            QPoint actorPixel(-1, -1);
            for (int y = 0; y < 240 && actorPixel.x() < 0; ++y)
                for (int x = 0; x < 320; ++x) if (preview->actorAt({x, y}) == 0) { actorPixel = {x, y}; break; }
            QVERIFY(actorPixel.x() >= 0);
            auto view = preview->findChild<QGraphicsView *>("sceneCanvas");
            const auto pixel = view->mapFromScene(QPointF(actorPixel) + QPointF(.5, .5));
            const auto destination = pixel + QPoint(35, 0);
            QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, pixel);
            QTest::mouseMove(view->viewport(), destination);
            QTest::keyClick(view, Qt::Key_Escape);
            QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, destination);
            QCOMPARE(undo.count(), 0); QCOMPARE(project.scenePrograms()->program(id).source, original);
            QCOMPARE(preview->renderedFrame(), before);
            QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::NoModifier, pixel);
            QTest::mouseMove(view->viewport(), destination);
            QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::NoModifier, destination);
            QCOMPARE(undo.count(), 1);
            QTRY_COMPARE(preview->property("sceneTick").toInt(), 3);
            QVERIFY(preview->renderedFrame() != before);
            QVERIFY(state->text().startsWith("Class pair 84 / 85"));
            QVERIFY(state->text().contains("Y 0.000"));
            const auto placed = project.scenePrograms()->program(id);
            int place = -1;
            for (const auto &action : SceneActions::inspect(placed)) if (action.name == "Place Actor") place = action.command;
            QVERIFY(place >= 0);
            QCOMPARE(preview->commandTimes().value(placed.commands[place].word, -1), 3);
            const auto placedState = state->text();
            preview->seekToUpdate(2); QTRY_COMPARE(preview->property("sceneTick").toInt(), 2);
            QVERIFY(state->text() != placedState);
            preview->seekToUpdate(3); QTRY_COMPARE(preview->property("sceneTick").toInt(), 3);
            QCOMPARE(state->text(), placedState);
            preview->seekToUpdate(10); QTRY_VERIFY(preview->property("sceneStopped").toBool());
            QCOMPARE(preview->property("sceneTick").toInt(), 6); // Splitting the wait preserves the exit.
            undo.undo(); QCOMPARE(project.scenePrograms()->program(id).source, original);
            undo.redo(); QCOMPARE(project.scenePrograms()->program(id).source, placed.source);

            // Add at playhead uses the same preserved timing; no selection-based insertion.
            preview->seekToUpdate(3); QTRY_COMPARE(preview->property("sceneTick").toInt(), 3);
            pages->setCurrentWidget(actions);
            auto kind = actions->findChild<QComboBox *>("sceneActionKind");
            kind->setCurrentIndex(kind->findData(0x1b));
            actions->findChild<QPushButton *>("sceneActionInsertAtPlayhead")->click();
            QCOMPARE(undo.index(), 2);
            QTRY_COMPARE(preview->property("sceneTick").toInt(), 3);
            auto field = actions->findChild<QLineEdit *>("sceneActionField0");
            QVERIFY(field); field->setText("10");
            // Concurrent movement and pan deliberately continue beyond the six-update exit.
            p = project.scenePrograms()->program(id);
            auto exit = SceneActions::positions(p).last().command;
            p = SceneActions::insert(p, exit, 7);
            p.commands[exit].operands = {0, quint32(-1000), quint32(-1000), 80000, 10000, 10, 0};
            p = DirectorProgram::decode(p.encode());
            exit = SceneActions::positions(p).last().command;
            p = SceneActions::insert(p, exit, 0x2c);
            p = SceneActions::edit(p, exit, 4, "8");
            auto changed = *project.scenePrograms(); changed.replace(id, p.source);
            pushSceneProgramChange(&project, &undo, "Concurrent actions", changed);
            pages->setCurrentWidget(preview);
            preview->seekToUpdate(30); QTRY_VERIFY(preview->property("sceneStopped").toBool());
            QCOMPARE(preview->property("sceneTick").toInt(), 6);
            auto warning = actions->findChild<QLabel *>("sceneExitWarning");
            QVERIFY2(warning->text().contains("Actor 0 movement"), qPrintable(warning->text()));
            QVERIFY(warning->text().contains("screen fade"));
            QVERIFY(warning->text().contains("camera pan"));
            pages->setCurrentWidget(actions);
            auto hold = actions->findChild<QPushButton *>("sceneHoldUntilFinished");
            QVERIFY(hold->isVisible() && hold->isEnabled());
            const int beforeHold = undo.index();
            hold->click(); QCOMPARE(undo.index(), beforeHold + 1);
            const auto held = project.scenePrograms()->program(id).source;
            undo.undo(); QCOMPARE(project.scenePrograms()->program(id).source, p.source);
            undo.redo(); QCOMPARE(project.scenePrograms()->program(id).source, held);
            pages->setCurrentWidget(preview);
            preview->seekToUpdate(60); QTRY_VERIFY(preview->property("sceneStopped").toBool());
            QVERIFY(preview->property("sceneTick").toInt() >= 16);
            QVERIFY2(warning->text().isEmpty(), qPrintable(warning->text()));
            QVERIFY2(preview->findChild<QLabel *>("scenePlaybackStatus")->text().contains("Scene released"),
                     qPrintable(preview->findChild<QLabel *>("scenePlaybackStatus")->text()));
            const auto heldFrame = preview->renderedFrame();
            const int heldTick = preview->property("sceneTick").toInt();
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.scenePrograms()->program(id).source, held);
            const auto built = rebuild(restored, tools, work.path());
            const auto root = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
            QTemporaryDir output(QDir(root).filePath("native-timed-scene-XXXXXX"));
            QVERIFY(output.isValid());
            const auto candidate = output.filePath(QFileInfo(path).fileName());
            exportRom(restored, built, candidate);
            const auto reopenedRom = Rom::open(candidate);
            const auto reopened = ScenePrograms::load(reopenedRom);
            const int selector = restored.scenePrograms()->resource(id).selectors.first();
            const SceneId reopenedId(reopened.original().selectors()[selector]);
            QCOMPARE(reopened.program(reopenedId).source, held);
            ScenePreview check(reopenedRom); check.selectScene(reopened, reopenedId, 2);
            check.seekToUpdate(60); QTRY_VERIFY(check.property("sceneStopped").toBool());
            QCOMPARE(check.property("sceneTick").toInt(), heldTick);
            QCOMPARE(check.renderedFrame(), heldFrame);
            // Editing both fields to the native retain-position sentinel must still place.
            auto negative = SceneActions::edit(placed, place, 1, "-1");
            negative = SceneActions::edit(negative, place, 2, "-1");
            auto negativePrograms = *project.scenePrograms(); negativePrograms.replace(id, negative.source);
            ScenePreview negativePreview(project.rom()); negativePreview.selectScene(negativePrograms, id, 2);
            negativePreview.seekToUpdate(3); QTRY_COMPARE(negativePreview.property("sceneTick").toInt(), 3);
            QVERIFY2(negativePreview.findChild<QLabel *>("sceneActorState")->text().contains("X -1.000 Â· Y 0.000 Â· Z -1.000"),
                     qPrintable(negativePreview.findChild<QLabel *>("sceneActorState")->text()));
            auto checksum = built.image; Rom::updateChecksum(checksum); QCOMPARE(checksum, built.image);
            QCOMPARE(Rom::open(path).bytes(), project.rom().bytes());
            output.setAutoRemove(false); qInfo().noquote() << "Timed scene export:" << candidate;
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void sceneActorClassChoices() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadScenePrograms();
        auto programs = *project.scenePrograms();
        const auto id = programs.createScene("Named Actor choices", 0x1fa8bf4 - 0x594284, true);
        programs.replace(id, SceneActions::insert(programs.program(id), 1, 0x2a).source);
        project.setScenePrograms(programs.json());
        QUndoStack undo;
        RecordModel classes(&project, Table::Classes, &undo);
        CutsceneEditor editor(project.rom(), nullptr, &project, &undo);
        editor.resize(1300, 900);
        editor.show();
        auto list = editor.findChild<QListWidget *>("cutsceneScenes");
        QTRY_COMPARE_WITH_TIMEOUT(list->count(), 1499, 10000);
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->data(Qt::UserRole + 1).toString() == id.authoredId)
                list->setCurrentRow(i);
        auto actions = editor.findChild<SceneActionEditor *>();
        editor.findChild<QTabWidget *>("cutsceneDetailPages")->setCurrentWidget(actions);
        actions->findChild<QListWidget *>("sceneActionList")->setCurrentRow(0);
        auto field = [&](int i) { return actions->findChild<QLineEdit *>(QString("sceneActionField%1").arg(i)); };
        auto choice = [&](int i) { return actions->findChild<QComboBox *>(QString("sceneActionClassChoice%1").arg(i)); };
        QVERIFY(choice(1) && choice(2));
        QCOMPARE(choice(1)->currentData().toInt(), 2);
        QVERIFY(choice(1)->currentText().contains(project.record(Table::Classes, 2).name));
        QVERIFY(classes.setData(classes.index(2, 1), "Scene Fighter", Qt::EditRole));
        QVERIFY(choice(1)->currentText().contains("Scene Fighter"));
        undo.undo();
        QVERIFY(choice(1)->currentText().contains(project.record(Table::Classes, 2).name));
        const int added = classes.appendClass(2, "New Actor");
        QVERIFY(added >= 0);
        QVERIFY(choice(1)->findData(added) >= 0);
        choice(1)->setFocus();
        QTest::keyClick(choice(1), Qt::Key_End);
        QCOMPARE(choice(1)->currentData().toInt(), added);
        QCOMPARE(field(1)->text(), QString::number(added));
        QCOMPARE(project.scenePrograms()->program(id).commands[2].operands[1], quint32(added));
        undo.undo();
        QVERIFY(choice(1));
        QCOMPARE(choice(1)->currentData().toInt(), 2);
        undo.undo();
        QCOMPARE(choice(1)->findData(added), -1);
        undo.redo();
        QVERIFY(choice(1)->findData(added) >= 0);
        undo.redo();
        QCOMPARE(choice(1)->currentData().toInt(), added);
        const auto beforeDraft = project.scenePrograms()->program(id).source;
        field(4)->setText("-");
        QVERIFY(actions->property("invalidEdits").toBool());
        QVERIFY(classes.setData(classes.index(added, 1), "Renamed Actor", Qt::EditRole));
        QCOMPARE(field(4)->text(), QString("-"));
        QVERIFY(actions->property("invalidEdits").toBool());
        QVERIFY(choice(1)->currentText().contains("Renamed Actor"));
        QCOMPARE(project.scenePrograms()->program(id).source, beforeDraft);
        undo.undo();
        QCOMPARE(field(4)->text(), QString("-"));
        QVERIFY(choice(1)->currentText().contains("New Actor"));
        actions->findChild<QPushButton *>("sceneActionDiscard")->click();
        field(2)->setText("0x54");
        QCOMPARE(choice(2)->currentData().toInt(), 84);
        QCoreApplication::processEvents();
        QVERIFY(choice(1)->isVisible());
        QVERIFY(editor.grab().save("native-action-class-choices.png"));
        auto symbolic = *project.scenePrograms();
        symbolic.setOperand(id, 2, 1, 0x08880001);
        pushSceneProgramChange(&project, &undo, "Use launch class", symbolic);
        actions->selectCommand(1);
        QVERIFY(field(1)->isReadOnly());
        QCOMPARE(field(1)->text(), QString("Launch value 1"));
        QVERIFY(!choice(1));
        auto separated = *project.scenePrograms();
        separated.insertCommand(id, 2, 0);
        pushSceneProgramChange(&project, &undo, "Separate Actor body action", separated);
        actions->selectCommand(3);
        QVERIFY(!choice(1));
        QVERIFY(choice(2)->findData(-1) >= 0);
        choice(2)->setFocus();
        QTest::keyClick(choice(2), Qt::Key_Home);
        QCOMPARE(project.scenePrograms()->program(id).commands[3].operands[5], quint32(-1));
        QCOMPARE(project.scenePrograms()->program(id).commands[3].operands[1], quint32(0x08880001));
    }
    void classActorCreationControls() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto programs = *project.scenePrograms();
            const auto id = programs.createScene("Class Actor preview", 0x1fa8bf4 - 0x594284, true);
            project.setScenePrograms(programs.json());
            const auto blank = programs.program(id);
            QUndoStack undo;
            CutsceneEditor editor(project.rom(), nullptr, &project, &undo);
            editor.resize(1400, 1000);
            editor.show();
            auto scenes = editor.findChild<QListWidget *>("cutsceneScenes");
            QTRY_COMPARE_WITH_TIMEOUT(scenes->count(), 1499, 10000);
            for (int i = 0; i < scenes->count(); ++i)
                if (scenes->item(i)->data(Qt::UserRole + 1).toString() == id.authoredId)
                    scenes->setCurrentRow(i);
            auto actions = editor.findChild<SceneActionEditor *>();
            auto kind = actions->findChild<QComboBox *>("sceneActionKind");
            auto position = actions->findChild<QComboBox *>("sceneActionPosition");
            auto add = actions->findChild<QPushButton *>("sceneActionInsert");
            kind->setCurrentIndex(kind->findData(0x2a));
            QVERIFY(add->isEnabled());
            add->click();
            QCOMPARE(
                SceneActions::inspect(project.scenePrograms()->program(id)).first().commandCount,
                2);
            auto field = [&](int i) {
                return actions->findChild<QLineEdit *>(QString("sceneActionField%1").arg(i));
            };
            QVERIFY(field(8));
            field(0)->setText("5");
            field(1)->setText("84");
            field(2)->setText("85");
            field(4)->setText("40.000");
            field(6)->setText("10.000");
            const auto created = project.scenePrograms()->program(id);
            QCOMPARE(created.commands[1].operands[0], quint32(5));
            QCOMPARE(created.commands[2].operands[0], quint32(5));
            field(1)->setText(QString::number(project.records(Table::Classes).size()));
            QVERIFY(actions->property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, created.source);
            actions->findChild<QPushButton *>("sceneActionDiscard")->click();
            auto remove = actions->findChild<QPushButton *>("sceneActionRemove");
            remove->click();
            QCOMPARE(project.scenePrograms()->program(id).source, blank.source);
            undo.undo();
            QCOMPARE(project.scenePrograms()->program(id).source, created.source);
            undo.redo();
            QCOMPARE(project.scenePrograms()->program(id).source, blank.source);
            undo.undo();
            kind->setCurrentIndex(kind->findData(0xe));
            position->setCurrentIndex(position->count() - 1);
            add->click();
            auto preview = editor.findChild<ScenePreview *>();
            auto pages = editor.findChild<QTabWidget *>("cutsceneDetailPages");
            pages->setCurrentWidget(preview);
            QVERIFY(preview->findChild<QLabel *>("scenePreviewContext")
                        ->text()
                        .contains("class-default equipment"));
            auto step = preview->findChild<QPushButton *>("sceneStep");
            auto status = preview->findChild<QLabel *>("scenePlaybackStatus");
            step->click();
            QVERIFY2(!preview->property("sceneStopped").toBool(), qPrintable(status->text()));
            auto actors = preview->findChild<QComboBox *>("sceneLiveActors");
            QCOMPARE(actors->count(), 1);
            QCOMPARE(actors->currentData().toInt(), 5);
            const auto expectedState =
                QString("Class pair 84 / 85 Â· X 40.000 Â· Y 0.000 Â· Z 10.000");
            auto actorState = preview->findChild<QLabel *>("sceneActorState");
            QCOMPARE(actorState->text(), expectedState);
            step->click();
            const auto frame = preview->renderedFrame();
            QVERIFY(!frame.isNull());
            auto seek = preview->findChild<QSpinBox *>("scenePosition");
            seek->setValue(0);
            QTRY_COMPARE(preview->property("sceneTick").toInt(), 0);
            QCOMPARE(actors->count(), 0);
            seek->setValue(2);
            QTRY_COMPARE(preview->property("sceneTick").toInt(), 2);
            QCOMPARE(actorState->text(), expectedState);
            QCOMPARE(preview->renderedFrame(), frame);
            QCoreApplication::processEvents();
            QVERIFY(preview->isVisible());
            QVERIFY(editor.grab().save("native-created-class-actor.png"));
            QPoint actorPixel(-1, -1);
            for (int y = 0; y < 240 && actorPixel.x() < 0; ++y)
                for (int x = 0; x < 320; ++x)
                    if (preview->actorAt({x, y}) == 5) {
                        actorPixel = {x, y};
                        break;
                    }
            QVERIFY(actorPixel.x() >= 0);
            actors->setCurrentIndex(-1);
            auto view = preview->findChild<QGraphicsView *>("sceneCanvas");
            const auto viewportPoint = view->mapFromScene(QPointF(actorPixel) + QPointF(.5, .5));
            const auto beforeClick = project.scenePrograms()->json();
            QTest::mousePress(view->viewport(), Qt::LeftButton, Qt::ControlModifier, viewportPoint);
            QTest::mouseMove(view->viewport(), viewportPoint + QPoint(40, 0));
            QTest::mouseRelease(view->viewport(), Qt::LeftButton, Qt::ControlModifier,
                                viewportPoint + QPoint(40, 0));
            QCOMPARE(actors->currentIndex(), -1); // Control-drag pans without editing.
            QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, viewportPoint);
            QCOMPARE(actors->currentData().toInt(), 5);
            QCOMPARE(preview->property("sceneTick").toInt(), 2);
            QCOMPARE(preview->renderedFrame(), frame);
            QCOMPARE(project.scenePrograms()->json(), beforeClick);
            seek->setValue(0);
            QTRY_COMPARE(preview->property("sceneTick").toInt(), 0);
            QCOMPARE(preview->actorAt(actorPixel), -1);
            seek->setValue(2);
            QTRY_COMPARE(preview->property("sceneTick").toInt(), 2);
            QCOMPARE(preview->actorAt(actorPixel), 5);
            QTest::mouseDClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, viewportPoint);
            QCOMPARE(pages->currentWidget(), actions);
            QCOMPARE(field(0)->text(), QString("5"));
            QCOMPARE(field(1)->text(), QString("84"));
            const auto beforePosition = project.scenePrograms()->program(id).source;
            const int beforeUndo = undo.index();
            auto choose = actions->findChild<QPushButton *>("sceneActionChoosePosition");
            QVERIFY(choose->isVisible() && choose->isEnabled());
            choose->click();
            QCOMPARE(pages->currentWidget(), preview);
            QVERIFY(preview->findChild<QPushButton *>("sceneCancelPosition")->isVisible());
            preview->findChild<QPushButton *>("sceneCancelPosition")->click();
            QCOMPARE(undo.index(), beforeUndo);
            QCOMPARE(project.scenePrograms()->program(id).source, beforePosition);
            pages->setCurrentWidget(actions);
            choose->click();
            QCoreApplication::processEvents();
            QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                              view->mapFromScene(QPointF(170, 150)));
            QCOMPARE(pages->currentWidget(), actions);
            QCOMPARE(undo.index(), beforeUndo + 1);
            const auto placed = project.scenePrograms()->program(id);
            QVERIFY(placed.source != beforePosition);
            QCOMPARE(placed.commands[1].operands[5], created.commands[1].operands[5]);
            undo.undo();
            QCOMPARE(project.scenePrograms()->program(id).source, beforePosition);
            undo.redo();
            QCOMPARE(project.scenePrograms()->program(id).source, placed.source);
            pages->setCurrentWidget(preview);
            seek->setValue(2);
            QTRY_COMPARE(preview->property("sceneTick").toInt(), 2);
            QVERIFY(preview->renderedFrame() != frame);
            pages->setCurrentWidget(actions);
            position->setCurrentIndex(position->count() - 1);
            kind->setCurrentIndex(kind->findData(7));
            add->click();
            const int moveCommand = actions->findChild<QListWidget *>("sceneActionList")
                                        ->currentItem()
                                        ->data(Qt::UserRole)
                                        .toInt();
            choose->click();
            QCoreApplication::processEvents();
            QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                              view->mapFromScene(QPointF(150, 140)));
            QCOMPARE(pages->currentWidget(), actions);
            const auto destination = project.scenePrograms()->program(id);
            QCOMPARE(destination.commands[1].operands, placed.commands[1].operands);
            QCOMPARE(destination.commands[moveCommand].opcode, quint32(7));
            QVERIFY(destination.commands[moveCommand].operands[3] != 0 ||
                    destination.commands[moveCommand].operands[4] != 0);
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto reopened = Project::fromRom(project.rom());
            reopened.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(reopened.scenePrograms()->program(id).source,
                     project.scenePrograms()->program(id).source);
            // A pre-existing native retain sentinel stays opaque when placement changes.
            auto retained = *project.scenePrograms();
            retained.setOperand(id, 2, 5, quint32(-1));
            pushSceneProgramChange(&project, &undo, "Retain Actor body source", retained);
            actions->selectCommand(1);
            QVERIFY(field(2)->isReadOnly());
            field(4)->setText("41.000");
            QVERIFY(!actions->property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).commands[1].operands[4], quint32(41000));
            QCOMPARE(project.scenePrograms()->program(id).commands[2].operands[5], quint32(-1));
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void sceneActionInsertion() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto programs = *project.scenePrograms();
            SceneId source;
            for (auto id : programs.scenes())
                if (programs.canCreateBlank(id)) {
                    source = id;
                    break;
                }
            QVERIFY(source);
            const auto id = programs.createScene("Desktop action authoring", source, true);
            project.setScenePrograms(programs.json());
            const auto blank = programs.program(id);
            QUndoStack undo;
            CutsceneEditor editor(project.rom(), nullptr, &project, &undo);
            editor.resize(1400, 850);
            editor.show();
            auto scenes = editor.findChild<QListWidget *>("cutsceneScenes");
            QTRY_VERIFY_WITH_TIMEOUT(scenes->count() > 1000, 10000);
            for (int i = 0; i < scenes->count(); ++i)
                if (scenes->item(i)->data(Qt::UserRole + 1).toString() == id.authoredId) {
                    scenes->setCurrentRow(i);
                    break;
                }
            auto actions = editor.findChild<SceneActionEditor *>("sceneActionEditor");
            auto kind = actions->findChild<QComboBox *>("sceneActionKind");
            auto position = actions->findChild<QComboBox *>("sceneActionPosition");
            auto add = actions->findChild<QPushButton *>("sceneActionInsert");
            auto remove = actions->findChild<QPushButton *>("sceneActionRemove");
            QCOMPARE(position->count(), 1);
            QCOMPARE(position->currentData().toInt(), 3);
            kind->setCurrentIndex(kind->findData(0xe));
            QVERIFY(add->isEnabled());
            add->click();
            QCOMPARE(project.scenePrograms()->program(id).source,
                     SceneActions::insert(blank, 3, 0xe).source);
            auto duration = actions->findChild<QLineEdit *>("sceneActionField0");
            QVERIFY(duration);
            duration->setText("7");
            const auto expected = SceneActions::insert(blank, 3, 0xe, 7).source;
            QCOMPARE(project.scenePrograms()->program(id).source, expected);
            duration->setText("-");
            QVERIFY(!add->isEnabled());
            QVERIFY(!remove->isEnabled());
            actions->findChild<QPushButton *>("sceneActionDiscard")->click();
            QVERIFY(remove->isEnabled());
            remove->click();
            QCOMPARE(project.scenePrograms()->program(id).source, blank.source);
            undo.undo();
            QCOMPARE(project.scenePrograms()->program(id).source, expected);
            undo.redo();
            QCOMPARE(project.scenePrograms()->program(id).source, blank.source);
            undo.undo();
            QTemporaryDir work;
            project.saveFolder(work.path());
            auto reopened = Project::fromRom(project.rom());
            reopened.applyManifest(Project::readProject(work.path()));
            QCOMPARE(reopened.scenePrograms()->program(id).source, expected);
            editor.findChild<QTabWidget *>("cutsceneDetailPages")->setCurrentWidget(actions);
            QVERIFY(editor.grab().save("native-action-insertion.png"));
            position->setCurrentIndex(position->count() - 1);
            project.loadDialogues();
            const auto originalText = project.dialogues()->json();
            const auto sharedLine = project.dialogues()->entry(Dialogues::Table::Primary, 5, 0);
            const auto priorScene = project.scenePrograms()->program(id).source;
            const auto undoCount = undo.index();
            kind->setCurrentIndex(kind->findData(0xbf));
            QVERIFY(add->isEnabled());
            QTimer::singleShot(0, actions, [&] {
                auto dialog = actions->findChild<QDialog *>("sceneDialogueDialog");
                QVERIFY(dialog);
                auto speaker = dialog->findChild<QLineEdit *>("sceneDialogueSpeaker");
                auto text = dialog->findChild<QPlainTextEdit *>("sceneDialogueText");
                auto buttons = dialog->findChild<QDialogButtonBox *>();
                speaker->setText("Joe");
                text->setPlainText("@");
                QVERIFY(!buttons->button(QDialogButtonBox::Ok)->isEnabled());
                text->setPlainText("A new line with its own text entry.");
                QVERIFY(buttons->button(QDialogButtonBox::Ok)->isEnabled());
                buttons->button(QDialogButtonBox::Ok)->click();
            });
            add->click();
            QCOMPARE(undo.index(), undoCount + 1);
            QCOMPARE(project.dialogues()->entry(Dialogues::Table::Primary, 5, 0), sharedLine);
            undo.undo();
            QCOMPARE(project.dialogues()->json(), originalText);
            QCOMPARE(project.scenePrograms()->program(id).source, priorScene);
            undo.redo();
            const auto beforeId = project.scenePrograms()->program(id);
            int dialogueCommand = -1;
            for (const auto &a : SceneActions::inspect(beforeId))
                if (a.name == "Dialogue")
                    dialogueCommand = a.command;
            QVERIFY(dialogueCommand >= 0);
            auto windowId = actions->findChild<QLineEdit *>("sceneActionField0");
            QVERIFY(windowId);
            QCOMPARE(windowId->accessibleName(), QString("Window ID"));
            windowId->setText("7");
            const auto connected = project.scenePrograms()->program(id);
            QCOMPARE(connected.commands[dialogueCommand].operands[0], quint32(7));
            QCOMPARE(connected.commands[dialogueCommand + 4].operands[2], quint32(7));
            undo.undo();
            QCOMPARE(project.scenePrograms()->program(id).source, beforeId.source);
            undo.redo();
            QCOMPARE(project.scenePrograms()->program(id).source, connected.source);
            const int dialogueEntry = connected.commands[dialogueCommand].operands[2];
            QVERIFY(dialogueEntry > 0);
            QTimer::singleShot(0, actions, [&] {
                auto dialog = actions->findChild<QDialog *>("sceneDialogueDialog");
                QVERIFY(dialog);
                QCOMPARE(dialog->findChild<QLineEdit *>("sceneDialogueSpeaker")->text(),
                         QString("Joe"));
                dialog->findChild<QPlainTextEdit *>("sceneDialogueText")
                    ->setPlainText("Revised spoken dialogue.");
                dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
            });
            actions->findChild<QPushButton *>("sceneActionEditText")->click();
            QCOMPARE(Dialogues::spokenFields(
                         project.dialogues()->entry(Dialogues::Table::Primary, 5, dialogueEntry))
                         ->second,
                     QString("Revised spoken dialogue."));
            undo.undo();
            undo.redo();
            QTimer::singleShot(0, actions, [&] {
                auto dialog = actions->findChild<QDialog *>("sceneDialogueDialog");
                QVERIFY(dialog);
                dialog->findChild<QPushButton *>("sceneDialogueRaw")->click();
            });
            actions->findChild<QPushButton *>("sceneActionEditText")->click();
            auto pages = editor.findChild<QTabWidget *>("cutsceneDetailPages");
            auto dialogue = qobject_cast<DialogueInspector *>(pages->currentWidget());
            QVERIFY(dialogue);
            QCOMPARE(dialogue->findChild<QComboBox *>("dialogueArchives")->currentData().toInt(),
                     5);
            QCOMPARE(dialogue->findChild<QListWidget *>("dialogueEntries")->currentRow(),
                     dialogueEntry);
            dialogue->findChild<QPlainTextEdit *>("dialogueSource")
                ->setPlainText("Desktop dialogue action.");
            QCOMPARE(project.dialogues()->entry(Dialogues::Table::Primary, 5, dialogueEntry),
                     QByteArray("Desktop dialogue action."));
            project.saveFolder(work.path());
            auto withDialogue = Project::fromRom(project.rom());
            withDialogue.applyManifest(Project::readProject(work.path()));
            QCOMPARE(withDialogue.scenePrograms()->program(id).source, connected.source);
            QCOMPARE(withDialogue.dialogues()->entry(Dialogues::Table::Primary, 5, dialogueEntry),
                     QByteArray("Desktop dialogue action."));
            pages->setCurrentWidget(actions);
            QTRY_VERIFY(actions->findChild<QLineEdit *>("sceneActionField0"));
            QTRY_VERIFY(actions->findChild<QLineEdit *>("sceneActionField0")->isVisible());
            QCOMPARE(actions->findChild<QLineEdit *>("sceneActionField0")->text(), QString("7"));
            QApplication::processEvents();
            QVERIFY(editor.grab().save("native-dialogue-action.png"));
            position->setCurrentIndex(position->findData(3));
            auto move = actions->findChild<QPushButton *>("sceneActionMove");
            QVERIFY(move->isEnabled());
            const auto reordered = SceneActions::move(connected, dialogueCommand, 3);
            move->click();
            QCOMPARE(project.scenePrograms()->program(id).source, reordered.source);
            QCOMPARE(actions->findChild<QListWidget *>("sceneActionList")
                         ->currentItem()
                         ->data(Qt::UserRole)
                         .toInt(),
                     3);
            undo.undo();
            QCOMPARE(project.scenePrograms()->program(id).source, connected.source);
            undo.redo();
            QCOMPARE(project.scenePrograms()->program(id).source, reordered.source);
            project.saveFolder(work.path());
            auto orderedProject = Project::fromRom(project.rom());
            orderedProject.applyManifest(Project::readProject(work.path()));
            QCOMPARE(orderedProject.scenePrograms()->program(id).source, reordered.source);
            QApplication::processEvents();
            QVERIFY(editor.grab().save("native-action-order.png"));
            kind->setCurrentIndex(kind->findData(0x14));
            add->click();
            QCOMPARE(project.scenePrograms()->program(id).commands[3].opcode, quint32(0x14));
            position->setCurrentIndex(position->findData(4));
            kind->setCurrentIndex(kind->findData(0x15));
            add->click();
            auto direction = actions->findChild<QLineEdit *>("sceneActionField3");
            QVERIFY(direction);
            QCOMPARE(direction->accessibleName(), QString("Turn phase direction"));
            const auto beforeTurn = project.scenePrograms()->program(id).source;
            direction->setText("0");
            QVERIFY(actions->property("invalidEdits").toBool());
            QVERIFY(!add->isEnabled());
            QVERIFY(!move->isEnabled());
            QCOMPARE(project.scenePrograms()->program(id).source, beforeTurn);
            direction->setText("-1");
            QVERIFY(!actions->property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).commands[4].operands[3], quint32(-1));
            position->setCurrentIndex(position->findData(5));
            kind->setCurrentIndex(kind->findData(0x1c));
            add->click();
            auto channel = actions->findChild<QLineEdit *>("sceneActionField1");
            QVERIFY(channel);
            QCOMPARE(channel->accessibleName(), QString("Drawing channel"));
            channel->setText("3");
            QCOMPARE(project.scenePrograms()->program(id).commands[5].operands[1], quint32(3));
            const auto actorControls = project.scenePrograms()->program(id);
            QCOMPARE(actorControls.commands[4].operands[0], actorControls.commands[3].operands[0]);
            QCOMPARE(actorControls.commands[5].operands[0], actorControls.commands[3].operands[0]);
            QApplication::processEvents();
            QVERIFY(editor.grab().save("native-actor-actions.png"));
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void bodyActionEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadScenePrograms();
        const SceneId key(0x1fa8bf4 - 0x594284);
        const auto original = project.scenePrograms()->program(key);
        int command = -1;
        for (int i = 0; i < original.commands.size(); ++i)
            if (original.commands[i].opcode == 0x2a) {
                command = i;
                break;
            }
        QVERIFY(command >= 0);
        QUndoStack undo;
        SceneActionEditor editor(&project, &undo, [] {});
        editor.resize(1100, 700);
        editor.show();
        editor.selectScene(key, command);
        auto primary = editor.findChild<QLineEdit *>("sceneActionField1");
        auto secondary = editor.findChild<QLineEdit *>("sceneActionField2");
        auto pose = editor.findChild<QLineEdit *>("sceneActionField3");
        QVERIFY(primary && secondary && pose);
        QVERIFY(primary->isReadOnly() && secondary->isReadOnly());
        QVERIFY(primary->toolTip().contains("launch event"));
        QVERIFY(!pose->isReadOnly());
        const int changed = original.commands[command].operands[2] == 0 ? 1 : 0;
        pose->setText(QString::number(changed));
        QMetaObject::invokeMethod(pose, "editingFinished");
        QCOMPARE(undo.count(), 1);
        auto expected = original.source;
        put32(expected, (original.commands[command].word + 3) * 4, quint32(changed));
        QCOMPARE(project.scenePrograms()->program(key).source, expected);
        undo.undo();
        QCOMPARE(project.scenePrograms()->program(key).source, original.source);
        undo.redo();
        QCOMPARE(project.scenePrograms()->program(key).source, expected);
        editor.selectScene(key, command);
        QCoreApplication::processEvents();
        QTRY_VERIFY(editor.findChild<QLineEdit *>("sceneActionField3")->isVisible());
        QCOMPARE(editor.findChild<QLineEdit *>("sceneActionField3")->text(), QString::number(changed));
        QVERIFY(editor.findChild<QLineEdit *>("sceneActionField1")->text().startsWith("Launch value "));
        QVERIFY(editor.grab().save("native-body-action.png"));
        QTemporaryDir work;
        project.saveFolder(work.filePath("project"));
        auto reopened = Project::fromRom(project.rom());
        reopened.applyManifest(Project::readProject(work.filePath("project")));
        QCOMPARE(reopened.scenePrograms()->program(key).source, expected);
    }
    void structuredSceneActions() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            QUndoStack undo;
            CutsceneEditor editor(project.rom(), nullptr, &project, &undo);
            editor.resize(1400, 850); editor.show();
            auto scenes = editor.findChild<QListWidget *>("cutsceneScenes");
            QTRY_VERIFY_WITH_TIMEOUT(scenes->count() > 1000, 10000);
            for (int i = 0; i < scenes->count(); ++i)
                if (scenes->item(i)->data(Qt::UserRole).toUInt() == 0x1a14970) { scenes->setCurrentRow(i); break; }
            const auto source = project.scenePrograms()->program(0x1a14970);
            int command = -1;
            for (const auto &a : SceneActions::inspect(source)) if (a.name == "Move Actor") { command = a.command; break; }
            QVERIFY(command >= 0);
            auto actions = editor.findChild<SceneActionEditor *>("sceneActionEditor");
            QVERIFY(actions);
            auto list = actions->findChild<QListWidget *>("sceneActionList");
            for (int i = 0; i < list->count(); ++i)
                if (list->item(i)->data(Qt::UserRole).toInt() == command) { list->setCurrentRow(i); break; }
            auto field = actions->findChild<QLineEdit *>("sceneActionField3");
            QVERIFY(field); QCOMPARE(field->accessibleName(), QString("Target X"));
            QCOMPARE(editor.findChild<QTableWidget *>("cutsceneCommands")->currentRow(), command);
            field->setText("-12.375");
            auto expected = source.source; put32(expected, source.commands[command].word * 4 + 16, quint32(-12375));
            QCOMPARE(project.scenePrograms()->program(0x1a14970).source, expected);
            QCOMPARE(editor.findChild<QLineEdit *>("sceneOperand3")->text(), QString("-12375"));
            undo.undo(); QCOMPARE(project.scenePrograms()->program(0x1a14970).source, source.source);
            undo.redo(); QCOMPARE(project.scenePrograms()->program(0x1a14970).source, expected);
            field = actions->findChild<QLineEdit *>("sceneActionField3");
            QVERIFY(field); QCOMPARE(field->text(), QString("-12.375"));
            field->setText("-"); QVERIFY(actions->property("invalidEdits").toBool());
            QVERIFY(!scenes->isEnabled());
            QVERIFY(!editor.findChild<SceneCommandEditor *>("sceneCommandEditor")->isEnabled());
            QVERIFY(!editor.findChild<QComboBox *>("cutsceneBackgroundChoice")->isEnabled());
            QCOMPARE(project.scenePrograms()->program(0x1a14970).source, expected);
            actions->findChild<QPushButton *>("sceneActionDiscard")->click();
            QVERIFY(!actions->property("invalidEdits").toBool()); QVERIFY(scenes->isEnabled());
            auto raw = editor.findChild<QLineEdit *>("sceneOperand3");
            raw->setText("-"); QVERIFY(!actions->isEnabled());
            editor.findChild<QPushButton *>("sceneCommandDiscard")->click(); QVERIFY(actions->isEnabled());
            QTemporaryDir work; project.saveFolder(work.path());
            auto reopened = Project::fromRom(project.rom()); reopened.applyManifest(Project::readProject(work.path()));
            QCOMPARE(reopened.scenePrograms()->program(0x1a14970).source, expected);
            editor.findChild<QTabWidget *>("cutsceneDetailPages")->setCurrentWidget(actions);
            QVERIFY(editor.grab().save("native-scene-actions.png"));
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void cutsceneBackgroundSelection() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            QUndoStack undo;
            CutsceneEditor editor(project.rom(), nullptr, &project, &undo);
            editor.resize(1400, 850); editor.show();
            auto list = editor.findChild<QListWidget *>("cutsceneScenes");
            QTRY_VERIFY_WITH_TIMEOUT(list->count() > 1000, 10000);
            int row = -1;
            for (int i = 0; i < list->count(); ++i) if (list->item(i)->data(Qt::UserRole).toUInt() == 0x1a14970) row = i;
            QVERIFY(row >= 0); list->setCurrentRow(row);
            auto choice = editor.findChild<QComboBox *>("cutsceneBackgroundChoice");
            auto apply = editor.findChild<QPushButton *>("cutsceneBackgroundApply");
            auto restore = editor.findChild<QPushButton *>("cutsceneBackgroundRestore");
            QCOMPARE(choice->count(), 63); QCOMPARE(choice->currentData().toInt(), 34);
            const auto original = project.scenePrograms()->program(0x1a14970).source;
            choice->setCurrentIndex(choice->findData(57));
            QVERIFY(apply->isEnabled()); QCOMPARE(project.scenePrograms()->program(0x1a14970).source, original);
            auto images = editor.findChild<QComboBox *>("cutsceneImages");
            QVERIFY(images->findData("archive:49") >= 0);
            apply->click();
            auto expected = original; put32(expected, 4, 57);
            QCOMPARE(project.scenePrograms()->program(0x1a14970).source, expected);
            QCOMPARE(choice->currentData().toInt(), 57); QVERIFY(!apply->isEnabled()); QVERIFY(restore->isEnabled());
            undo.undo(); QCOMPARE(project.scenePrograms()->program(0x1a14970).source, original);
            QCOMPARE(choice->currentData().toInt(), 34);
            undo.redo(); QCOMPARE(project.scenePrograms()->program(0x1a14970).source, expected);
            restore->click(); QCOMPARE(project.scenePrograms()->program(0x1a14970).source, original);
            undo.undo(); QCOMPARE(project.scenePrograms()->program(0x1a14970).source, expected);
            QTemporaryDir work; project.saveFolder(work.path());
            auto reopened = Project::fromRom(project.rom()); reopened.applyManifest(Project::readProject(work.path()));
            QCOMPARE(reopened.scenePrograms()->program(0x1a14970).source, expected);
            editor.findChild<QTableWidget *>("cutsceneCommands")->setCurrentCell(0, 0);
            auto operand = editor.findChild<QLineEdit *>("sceneOperand0");
            QVERIFY(operand); operand->setText("-");
            QVERIFY(!choice->isEnabled() && !apply->isEnabled() && !restore->isEnabled());
            QCOMPARE(project.scenePrograms()->program(0x1a14970).source, expected);
            editor.findChild<QPushButton *>("sceneCommandDiscard")->click();
            QVERIFY(choice->isEnabled() && restore->isEnabled());
            auto catalog = editor.findChild<QTabWidget *>("cutsceneCatalogTabs");
            catalog->setCurrentIndex(1);
            QVERIFY(!choice->isEnabled() && !apply->isEnabled() && !restore->isEnabled());
            catalog->setCurrentIndex(0);
            QCOMPARE(choice->currentData().toInt(), 57);
            QCOMPARE(project.scenePrograms()->program(0x1a14970).source, expected);
            QTRY_VERIFY_WITH_TIMEOUT(!editor.property("displayedImage").toString().isEmpty(), 30000);
            QVERIFY(editor.grab().save("native-background-selection.png"));
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void equipmentWarningReview() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));project.loadScenarios();
        const auto before=project.manifest();
        const auto warnings=equipmentWarnings(project);QVERIFY(!warnings.isEmpty());
        ProjectChange warning;bool found=false;
        for(const auto &entry:warnings)
            if(entry.location.editor=="scenarios" && squadMembers(project.scenarios()->scenario(entry.location.id).squads[entry.location.child]).size()<=5) {
                warning=entry;found=true;break;
            }
        QVERIFY(found);
        const auto parts=warning.location.key.split(':');const int member=parts[1].toInt(),slot=parts[2].toInt();
        const int item=squadMembers(project.scenarios()->scenario(warning.location.id).squads[warning.location.child])[member].equipment[slot];
        const auto itemName=project.record(Table::Items,item).name.simplified();
        ChangeReportDialog dialog(project);dialog.show();
        QTRY_VERIFY_WITH_TIMEOUT(dialog.findChild<QPushButton *>("copyChangeReport")->isEnabled(),30000);
        auto view=dialog.findChild<QComboBox *>("changeReportView"); QVERIFY(view);
        QCOMPARE(view->currentIndex(),0);
        auto rows=dialog.findChild<QTreeWidget *>("changeReportEntries");QTreeWidgetItem *target=nullptr;
        for(int i=0;i<rows->topLevelItemCount();i++) QVERIFY(rows->topLevelItem(i)->isHidden());
        QVERIFY(!dialog.findChild<QPlainTextEdit *>("changeReportText")->toPlainText().contains("Incompatible assignment"));
        view->setCurrentIndex(2);
        dialog.findChild<QLineEdit *>("changeReportSearch")->setText(itemName);
        for(int i=0;i<rows->topLevelItemCount();i++) {
            auto row=rows->topLevelItem(i);if(row->isHidden())continue;
            QCOMPARE(row->text(0),QString("Existing ROM warning"));
            if(row->text(1)==warning.subject)target=row;
        }
        QVERIFY(target);rows->setCurrentItem(target);
        QVERIFY(dialog.findChild<QPlainTextEdit *>("changeReportText")->toPlainText().contains(itemName));
        dialog.findChild<QPushButton *>("copyChangeReport")->click();
        QVERIFY(QApplication::clipboard()->text().contains("Existing ROM equipment warnings"));
        QVERIFY(dialog.grab().save("native/build/equipment-warning-review.png"));
        QSignalSpy jump(&dialog,&ChangeReportDialog::editRequested);dialog.findChild<QPushButton *>("openChangeEdit")->click();
        QCOMPARE(jump.count(),1);QCOMPARE(jump[0][0].value<EditLocation>().key,warning.location.key);
        QCOMPARE(project.manifest(),before);
        ProjectReviewDialog exportReview(project);exportReview.show();
        QTRY_VERIFY_WITH_TIMEOUT(exportReview.property("reviewReady").toBool(),30000);
        auto exportRows=exportReview.findChild<QTreeWidget *>("projectReviewEntries");
        for(int i=0;i<exportRows->topLevelItemCount();i++)
            if(exportRows->topLevelItem(i)->text(0)=="Existing ROM warning") QVERIFY(exportRows->topLevelItem(i)->isHidden());
        exportReview.findChild<QComboBox *>("projectReviewView")->setCurrentIndex(1);
        int existingCount=0;
        for(int i=0;i<exportRows->topLevelItemCount();i++) if(!exportRows->topLevelItem(i)->isHidden()) {
            QCOMPARE(exportRows->topLevelItem(i)->text(0),QString("Existing ROM warning")); ++existingCount;
        }
        QCOMPARE(existingCount,warnings.size());
        QVERIFY(exportReview.findChild<QPushButton *>("confirmRomExport")->isEnabled());exportReview.reject();
        // New assignments are visible by default without bringing retail warnings back.
        auto edited=project; auto scenarios=*edited.scenarios(); auto record=scenarios.scenario(1).squads[0];
        put8(record,0,21);put16(record,2,31);put16(record,4,0);
        scenarios.setSquad(1,0,record,edited.classData());edited.setScenarios(scenarios.json());
        const auto introduced=reviewEquipmentWarnings(edited).introduced;QVERIFY(!introduced.isEmpty());
        ChangeReportDialog changed(edited);changed.show();
        QTRY_VERIFY_WITH_TIMEOUT(changed.findChild<QPushButton *>("copyChangeReport")->isEnabled(),30000);
        auto changedText=changed.findChild<QPlainTextEdit *>("changeReportText");
        QVERIFY(changedText->toPlainText().contains(introduced.first().subject));
        QVERIFY(!changedText->toPlainText().contains(warning.subject));
        changed.findChild<QComboBox *>("changeReportView")->setCurrentIndex(1);
        QVERIFY(changedText->toPlainText().contains("New equipment warnings"));
        changed.findChild<QComboBox *>("changeReportView")->setCurrentIndex(2);
        QVERIFY(!changedText->toPlainText().contains(introduced.first().subject));
        QVERIFY(changedText->toPlainText().contains(warning.subject));
        QTemporaryDir savedReports;changed.saveReport(savedReports.filePath("existing.txt"));
        QFile savedReport(savedReports.filePath("existing.txt"));QVERIFY(savedReport.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(savedReport.readAll()),changedText->toPlainText());changed.reject();
        MainWindow window;window.loadRom(path);window.show();bool opened=false;
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QAction *>("reviewProjectChanges")->isEnabled(),10000);
        QTimer::singleShot(0,&window,[&]{
            auto members=window.findChild<QDialog *>("scenarioMemberDialog");QVERIFY(members);
            auto cleanup=qScopeGuard([&]{members->reject();});
            auto box=members->findChild<QComboBox *>(QString("scenarioEquipment_%1_%2").arg(member).arg(slot));
            QVERIFY(box);QCOMPARE(box->currentData().toInt(),item);QVERIFY(box->currentText().contains(itemName));
            QVERIFY(box->currentText().contains("incompatible assignment"));
            QCOMPARE(members->findChild<QTableWidget *>("scenarioMembers")->currentRow(),member);opened=true;
        });
        window.navigateTo(warning.location);QVERIFY(opened);
    }
    void exportReferenceReview() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            auto original = project.record(Table::Classes, 2).bytes;
            auto broken = original; put16(broken, 46, 999);
            project.replaceBytes(Table::Classes, 2, broken);
            ProjectReviewDialog review(project);
            review.show();
            QTRY_VERIFY_WITH_TIMEOUT(review.property("reviewReady").toBool(), 30000);
            QVERIFY(!review.findChild<QPushButton *>("confirmRomExport")->isEnabled());
            auto rows = review.findChild<QTreeWidget *>("projectReviewEntries");
            QTreeWidgetItem *problem = nullptr;
            for (int i = 0; i < rows->topLevelItemCount(); ++i) {
                auto row = rows->topLevelItem(i);
                const auto target = row->data(0, Qt::UserRole).value<EditLocation>();
                if (target.editor == "classes" && target.id == 2 && row->text(1).contains("999")) problem = row;
            }
            QVERIFY(problem); rows->setCurrentItem(problem);
            QSignalSpy jump(&review, &ProjectReviewDialog::editRequested);
            QVERIFY(review.grab().save("native/build/export-reference-problem.png"));
            review.findChild<QPushButton *>("openReviewEdit")->click();
            QCOMPARE(review.result(), int(QDialog::Rejected)); QCOMPARE(jump.count(), 1);
            QCOMPARE(jump.at(0).at(0).value<EditLocation>().id, 2);
            project.replaceBytes(Table::Classes, 2, original);
            ProjectReviewDialog fixed(project); fixed.show();
            QTRY_VERIFY_WITH_TIMEOUT(fixed.property("reviewReady").toBool(), 30000);
            QVERIFY(fixed.findChild<QPushButton *>("confirmRomExport")->isEnabled());
            fixed.reject();
            const int item = u16(original, 46);
            ProjectReviewDialog uses(project, {"items", item}); uses.show();
            QTRY_VERIFY_WITH_TIMEOUT(uses.property("reviewReady").toBool(), 30000);
            QVERIFY(uses.findChild<QTreeWidget *>("projectReviewEntries")->topLevelItemCount() > 0);
            QVERIFY(!uses.findChild<QPushButton *>("confirmRomExport"));
            uses.reject();
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void projectChangeReview() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            QTemporaryDir dir;
            const auto source = dir.filePath("source.v64");
            QVERIFY(QFile::copy(path, source));
            auto project = Project::fromRom(Rom::open(source));
            project.setClassNameBytes(2, encodeClassName("Review Fighter"));
            const auto before = project.manifest();
            const auto changes = project.changeReport().text();
            ChangeReportDialog dialog(project);
            dialog.show();
            auto copy = dialog.findChild<QPushButton *>("copyChangeReport");
            QTRY_VERIFY_WITH_TIMEOUT(copy->isEnabled(), 30000);
            auto text = dialog.findChild<QPlainTextEdit *>("changeReportText");
            const auto expected = text->toPlainText();
            QVERIFY(expected.startsWith(changes));
            copy->click();
            QCOMPARE(QApplication::clipboard()->text(), expected);
            auto search = dialog.findChild<QLineEdit *>("changeReportSearch");
            search->setText("Review Fighter");
            QCOMPARE(text->textCursor().selectedText(), QString("Review Fighter"));
            const auto output = dir.filePath("changes.txt");
            QVERIFY_THROWS_EXCEPTION(Error, dialog.saveReport(source));
            QCOMPARE(Rom::open(source).bytes(), project.rom().bytes());
            dialog.saveReport(output);
            QFile saved(output); QVERIFY(saved.open(QIODevice::ReadOnly));
            QCOMPARE(saved.readAll(), expected.toUtf8());
            QCOMPARE(project.manifest(), before);
            dialog.compareWithRom(path);
            QTRY_VERIFY_WITH_TIMEOUT(copy->isEnabled(),30000);
            QVERIFY(text->toPlainText().contains("Review Fighter"));
            QVERIFY_THROWS_EXCEPTION(Error,dialog.saveReport(path));
            dialog.compareWithRom({});QTRY_VERIFY_WITH_TIMEOUT(copy->isEnabled(),30000);
            QCOMPARE(text->toPlainText(),expected);
            auto entries = dialog.findChild<QTreeWidget *>("changeReportEntries");
            QVERIFY(entries && entries->topLevelItemCount() > 0);
            QTreeWidgetItem *classChange = nullptr;
            for (int i = 0; i < entries->topLevelItemCount(); ++i) {
                const auto row = entries->topLevelItem(i);
                const auto target = row->data(0, Qt::UserRole).value<EditLocation>();
                if (target.editor == "classes" && target.id == 2) classChange = row;
            }
            QVERIFY(classChange); entries->setCurrentItem(classChange);
            QVERIFY(dialog.grab().save("project-changes.png"));
            QSignalSpy jump(&dialog, &ChangeReportDialog::editRequested);
            dialog.findChild<QPushButton *>("openChangeEdit")->click();
            QCOMPARE(jump.count(), 1);
            QCOMPARE(jump.at(0).at(0).value<EditLocation>().id, 2);
            dialog.close();
            MainWindow window;
            auto action = window.findChild<QAction *>("reviewProjectChanges");
            QVERIFY(action); QVERIFY(!action->isEnabled());
            window.loadRom(source);
            QTRY_VERIFY_WITH_TIMEOUT(action->isEnabled(), 10000);
            window.show();
            auto classes = window.findChild<QTableView *>("classesTable");
            classes->sortByColumn(1, Qt::DescendingOrder);
            window.navigateTo({"classes", 2});
            QVERIFY(classes->isVisible());
            QCOMPARE(classes->currentIndex().siblingAtColumn(0).data(Qt::EditRole).toInt(), 2);
            window.navigateTo({"shops", 35});
            auto shops = window.findChild<QListWidget *>("shopList");
            QVERIFY(shops->isVisible());
            QCOMPARE(shops->currentItem()->data(Qt::UserRole).toInt(), 35);
            window.navigateTo({"scenarioSettings",1});
            QVERIFY(window.findChild<QTimeEdit *>("scenarioStartingTime")->isVisible());
            window.navigateTo({"scenario-event",4,0x2f0});
            auto eventSteps = window.findChild<QTreeWidget *>("scenarioEventSteps");
            QVERIFY(eventSteps && eventSteps->isVisible() && eventSteps->currentItem());
            QCOMPARE(window.findChild<QComboBox *>("scenarioEventRow")->currentData().toInt(), 4);
            QCOMPARE(eventSteps->currentItem()->data(0, Qt::UserRole).toInt(), 0x2f0);
            window.navigateTo({"rule",-1,-1,"dayLength"});
            QVERIFY(window.findChild<QSpinBox *>("rule_dayLength")->isVisible());
            window.navigateTo({"rule",-1,-1,"reserveXp"});
            QVERIFY(window.findChild<QSpinBox *>("rule_reserveXp")->isVisible());
            window.navigateTo({"shops",35});
            bool usesOpened = false;
            QTimer::singleShot(0, &window, [&] {
                if (auto uses = window.findChild<ProjectReviewDialog *>("projectUses")) {
                    usesOpened = true; uses->reject();
                }
            });
            window.findChild<QAction *>("whereUsed")->trigger();
            QVERIFY(usesOpened);
            bool opened = false;
            QTimer::singleShot(0, &window, [&] {
                const auto review = window.findChild<ChangeReportDialog *>("projectChanges");
                opened = review != nullptr;
                if (review) review->reject();
            });
            action->trigger();
            QVERIFY(opened);
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void portraitDitherAssignment() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            QUndoStack undo;
            QImage source(40, 48, QImage::Format_RGBA8888);
            for (int y = 0; y < 48; ++y)
                for (int x = 0; x < 40; ++x)
                    source.setPixelColor(x, y, {x * 255 / 39, y * 255 / 47, (x * 7 + y * 3) % 256});
            const auto asset = SpriteAsset::fromImage("Dither example", source);
            auto library = project.sprites();
            library.replace(asset);
            project.setSprites(library);
            PortraitEditor editor(&project, &undo, [] {});
            editor.resize(1250, 790);
            editor.show();
            editor.selectPortrait(1, 0);
            auto choices = editor.findChild<QComboBox *>("portraitLibrary");
            choices->setCurrentIndex(choices->findData(asset.id));
            const auto before = project.avatarArtwork()->json();
            const auto expected = AvatarArtwork::mapImage(source, true, Qt::black, true);
            bool visited = false;
            const auto visit = [&](bool accept) {
                QTimer::singleShot(0, &editor, [&, accept] {
                    auto dialog = editor.findChild<QDialog *>("portraitImagePreparation");
                    QVERIFY(dialog);
                    auto cleanup = qScopeGuard([&] { dialog->reject(); });
                    auto check = dialog->findChild<QCheckBox *>("portraitImportDither");
                    QVERIFY(check);
                    QVERIFY(!check->isChecked());
                    check->setChecked(true);
                    const auto expectedPixmap =
                        QPixmap::fromImage(AvatarArtwork::render(expected).scaled(200, 240));
                    bool matches = false;
                    for (auto label : dialog->findChildren<QLabel *>())
                        if (!label->pixmap().isNull() &&
                            label->pixmap().toImage() == expectedPixmap.toImage())
                            matches = true;
                    QVERIFY(matches);
                    QCOMPARE(project.avatarArtwork()->json(), before);
                    QCOMPARE(undo.count(), 0);
                    QVERIFY(dialog->grab().save("portrait-dither-preparation.png"));
                    visited = true;
                    if (accept) {
                        cleanup.dismiss();
                        dialog->accept();
                    }
                });
                editor.findChild<QPushButton *>("portraitAssignLibrary")->click();
            };
            visit(false);
            QVERIFY(visited);
            QCOMPARE(project.avatarArtwork()->json(), before);
            QCOMPARE(undo.count(), 0);
            visited = false;
            visit(true);
            QVERIFY(visited);
            QCOMPARE(project.avatarArtwork()->words(project.classData(), 1, 0), expected);
            QCOMPARE(undo.count(), 1);
            undo.undo();
            QCOMPARE(project.avatarArtwork()->json(), before);
            undo.redo();
            QCOMPARE(project.avatarArtwork()->words(project.classData(), 1, 0), expected);
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.avatarArtwork()->words(restored.classData(), 1, 0), expected);
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void spriteVideoExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path)); QUndoStack undo;
            SpriteEditor editor(&project, &undo, [] {});
            editor.resize(1400, 900); editor.show();
            auto asset = SpriteAsset::blank("Video export", {32, 32});
            asset.frames[0].layers[0].image.fill(Qt::red); asset.frames[0].ticks = 3;
            editor.addAsset(asset);
            const auto original = project.sprites().json(); const int commands = undo.count();
            QTimer::singleShot(0, &editor, [&] {
                auto dialog = editor.findChild<QDialog *>("spriteVideoOptions"); QVERIFY(dialog);
                dialog->findChild<QSpinBox *>("spriteVideoScale")->setValue(2);
                dialog->findChild<QSpinBox *>("spriteVideoRepeats")->setValue(2);
                QVERIFY(dialog->grab().save("sprite-video-options.png"));
                dialog->reject();
            });
            auto menu = editor.findChild<QPushButton *>("spriteExport")->menu();
            const auto actions = menu->actions();
            const auto action = std::find_if(actions.cbegin(), actions.cend(),
                                              [](QAction *a) { return a->text().startsWith("MP4 video"); });
            QVERIFY(action != actions.cend()); (*action)->trigger();
            QTemporaryDir work;
            QVERIFY(editor.exportVideo(work.filePath("sprite.mp4"), {2, 2, Qt::black}));
            QVERIFY(QFileInfo(work.filePath("sprite.mp4")).size() > 100);
            QCOMPARE(project.sprites().json(), original); QCOMPARE(undo.count(), commands);
            QVERIFY(!editor.findChild<QDialog *>("spriteVideoProgress"));
            QVERIFY(editor.exportVideo(work.filePath("sprite.apng"), {1, 0, Qt::black, SpriteVideoFormat::AnimatedPng}));
            const auto preview = QImage(work.filePath("sprite.apng")).convertToFormat(QImage::Format_RGBA8888);
            QCOMPARE(preview.size(), asset.size);
            const auto expected = asset.composite(0);
            QCOMPARE(QByteArray(reinterpret_cast<const char *>(preview.constBits()), preview.sizeInBytes()),
                     QByteArray(reinterpret_cast<const char *>(expected.constBits()), expected.sizeInBytes()));
            QCOMPARE(project.sprites().json(), original); QCOMPARE(undo.count(), commands);
        } catch (const Error &e) { QFAIL(e.what()); }
    }
    void spriteLayerImagePreparation() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path)); QUndoStack undo;
            SpriteEditor editor(&project, &undo, [] {}); editor.resize(1400, 900); editor.show();
            auto original = SpriteAsset::blank("Prepared layer", {6, 4});
            original.kind = "sequence";
            auto &first = original.frames.first();
            first.layers.first().position = {-2, 3};
            auto other = first.layers.first(); other.id = SpriteLibrary::newId(); other.image.fill(Qt::blue);
            first.layers << other;
            auto second = first; second.id = SpriteLibrary::newId(); original.frames << second;
            editor.addAsset(original);
            editor.findChild<QListWidget *>("spriteLayers")->setCurrentRow(1);
            QImage source(2, 2, QImage::Format_RGBA8888);
            source.setPixelColor(0, 0, QColor(120, 40, 20, 128)); source.setPixelColor(1, 0, Qt::green);
            source.setPixelColor(0, 1, QColor(9, 8, 7, 0)); source.setPixelColor(1, 1, Qt::red);
            QTemporaryDir work; const auto imagePath = work.filePath("source.png"); QVERIFY(source.save(imagePath));
            const int initialCommands = undo.count();
            QTimer::singleShot(0, &editor, [&] {
                auto dialog = editor.findChild<QDialog *>("spriteLayerImagePreparation"); QVERIFY(dialog);
                dialog->reject();
            });
            editor.importLayerImage(imagePath);
            QCOMPARE(undo.count(), initialCommands); QCOMPARE(project.sprites().asset(original.id), original);
            QTimer::singleShot(0, &editor, [&] {
                auto dialog = editor.findChild<QDialog *>("spriteLayerImagePreparation"); QVERIFY(dialog);
                dialog->findChild<QComboBox *>("spriteLayerImportFit")->setCurrentIndex(3);
                dialog->findChild<QSpinBox *>("spriteLayerImportX")->setValue(1);
                QVERIFY(dialog->grab().save("sprite-layer-preparation.png"));
                dialog->accept();
            });
            editor.importLayerImage(imagePath);
            QCOMPARE(undo.count(), initialCommands + 1);
            const auto changed = project.sprites().asset(original.id);
            QCOMPARE(changed.frames[1], original.frames[1]);
            QCOMPARE(changed.frames[0].layers[1], original.frames[0].layers[1]);
            QCOMPARE(changed.frames[0].layers[0].position, QPoint(-2, 3));
            QCOMPARE(changed.frames[0].layers[0].id, original.frames[0].layers[0].id);
            const auto result = changed.frames[0].layers[0].image;
            QCOMPARE(result.size(), QSize(6, 4));
            for (int y = 0; y < 2; ++y) for (int x = 0; x < 2; ++x)
                QCOMPARE(result.pixelColor(3 + x, 1 + y), source.pixelColor(x, y));
            QCOMPARE(result.pixelColor(0, 0), QColor(Qt::transparent));
            undo.undo(); QCOMPARE(project.sprites().asset(original.id), original);
            undo.redo(); QCOMPARE(project.sprites().asset(original.id), changed);
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(project.rom());
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.sprites().asset(original.id), changed);
            QTimer::singleShot(0, &editor, [&] {
                auto dialog = editor.findChild<QDialog *>("spriteLayerImagePreparation"); QVERIFY(dialog);
                undo.undo(); dialog->accept();
            });
            QVERIFY_THROWS_EXCEPTION(Error, editor.importLayerImage(imagePath));
            QCOMPARE(project.sprites().asset(original.id), original);
        } catch (const Error &e) { QFAIL(e.what()); }
    }
    void actorPoseLibraryTransfer() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            QUndoStack undo;
            CutsceneEditor editor(rom, nullptr, &project, &undo, [] {});
            editor.resize(1500, 1050); editor.show();
            auto inspector = editor.findChild<ActorPoseEditor *>(); QVERIFY(inspector);
            for (auto tabs : editor.findChildren<QTabWidget *>())
                if (tabs->indexOf(inspector) >= 0) tabs->setCurrentWidget(inspector);
            auto kind = inspector->findChild<QComboBox *>("actorSourceKind");
            auto bank = inspector->findChild<QComboBox *>("actorBank");
            auto pose = inspector->findChild<QComboBox *>("actorPose");
            auto copy = inspector->findChild<QPushButton *>("actorCopyToLibrary");
            QVERIFY(copy && copy->isVisible());
            bank->setCurrentIndex(bank->findData(30));
            pose->setCurrentIndex(pose->findData(12));
            inspector->findChild<QSpinBox *>("actorVariant")->setValue(3);
            copy->click();
            QCOMPARE(project.sprites().assets().size(), 1);
            const auto first = project.sprites().assets().first();
            const auto expected = spriteAssetFromPose(rom, ActorArt::load(rom, 30), 12, "Expected", 3);
            QCOMPARE(first.provenance["variant"].toInt(), 3);
            QCOMPARE(first.frames.size(), expected.frames.size());
            for (int f = 0; f < first.frames.size(); ++f) QCOMPARE(first.composite(f), expected.composite(f));
            kind->setCurrentIndex(1); bank->setCurrentIndex(bank->findData(2));
            pose->setCurrentIndex(pose->findData(40));
            auto equipment = inspector->findChild<QComboBox *>("actorEquipment");
            equipment->setCurrentIndex(equipment->findData(11));
            copy->click();
            QCOMPARE(project.sprites().assets().size(), 2);
            const auto second = project.sprites().assets().last();
            QCOMPARE(second.provenance["equipment"].toInt(), 11);
            const auto art = ActorArt::loadBody(rom, 2, 2, 0, 0, project.bodyAnimationData());
            const auto body = spriteAssetFromPose(rom, art, 40, "Expected", 0, BodyAppearance{11, {}});
            QCOMPARE(second.frames.size(), body.frames.size());
            for (int f = 0; f < second.frames.size(); ++f) QCOMPARE(second.composite(f), body.composite(f));
            undo.undo(); QCOMPARE(project.sprites().assets().size(), 1);
            QCOMPARE(project.sprites().assets().first(), first);
            undo.redo(); QCOMPARE(project.sprites().assets().last(), second);
            QTemporaryDir work; project.saveFolder(work.path());
            auto restored = Project::fromRom(rom); restored.applyManifest(Project::readProject(work.path()));
            QCOMPARE(restored.sprites().json(), project.sprites().json());
            QVERIFY(inspector->grab().save("actor-library-transfer.png"));
        } catch (const Error &e) { QFAIL(e.what()); }
    }
    void bodyConstructionPolicies() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path)); QUndoStack undo;
            AnimationEditor editor(&project, &undo, [] {}); editor.selectClass(135);
            auto construction = editor.findChild<QComboBox *>("animationConstruction");
            auto fixed = editor.findChild<QCheckBox *>("animationFixedPose");
            QVERIFY(construction); QVERIFY(fixed);
            QCOMPARE(construction->currentIndex(), 1); QVERIFY(fixed->isChecked());
            construction->setCurrentIndex(2); fixed->setChecked(false);
            QCOMPARE(project.animationRoutes()->entry(135).construction, 2);
            QVERIFY(!project.animationRoutes()->entry(135).fixedPose);
            undo.undo(); QVERIFY(fixed->isChecked()); undo.undo(); QCOMPARE(construction->currentIndex(), 1);
            undo.redo(); undo.redo();
            const int id = project.appendClass(135, "Body policy clone");
            editor.selectClass(id); QCOMPARE(construction->currentIndex(), 2); QVERIFY(!fixed->isChecked());
            auto source = editor.findChild<QComboBox *>("animationSource");
            source->setCurrentIndex(source->findData(135));
            // Copying an ordinary set clears both inherited special policies.
            source->setCurrentIndex(source->findData(30));
            editor.findChild<QPushButton *>("animationCopySource")->click();
            QCOMPARE(construction->currentIndex(), 0); QVERIFY(!fixed->isChecked());
            undo.undo(); QCOMPARE(construction->currentIndex(), 2);
            QTemporaryDir work; project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(Rom::open(path)); restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.animationRoutes()->entry(id).construction, 2);
            QVERIFY(!restored.animationRoutes()->entry(id).fixedPose);
            editor.resize(1250, 850); editor.show();
            QVERIFY(editor.grab().save("body-construction-policies.png"));
        } catch (const Error &e) { QFAIL(e.what()); }
    }
    void equipmentArtworkPolicies() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path)); QUndoStack undo;
            AnimationEditor editor(&project, &undo, [] {}); editor.selectClass(57);
            auto items = editor.findChild<QComboBox *>("animationEquipmentArt");
            auto children = editor.findChild<QCheckBox *>("animationEquipmentChildren");
            QVERIFY(items); QVERIFY(children);
            QCOMPARE(items->currentData().toInt(), 2); QVERIFY(children->isChecked());
            items->setCurrentIndex(items->findData(11)); children->setChecked(false);
            QCOMPARE(project.animationRoutes()->entry(57).equipmentArt, qint16(11));
            QVERIFY(!project.animationRoutes()->entry(57).equipmentChildArt);
            undo.undo(); QVERIFY(children->isChecked()); undo.undo(); QCOMPARE(items->currentData().toInt(), 2);
            undo.redo(); undo.redo();
            const int id = project.appendClass(57, "Equipment art clone");
            editor.selectClass(id); QCOMPARE(items->currentData().toInt(), 11); QVERIFY(!children->isChecked());
            QTemporaryDir work; project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(Rom::open(path)); restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.animationRoutes()->entry(id).equipmentArt, qint16(11));
            QVERIFY(!restored.animationRoutes()->entry(id).equipmentChildArt);
            editor.resize(1250, 800); editor.show();
            QVERIFY(editor.grab().save("equipment-artwork-policies.png"));
        } catch (const Error &e) { QFAIL(e.what()); }
    }
    void stagedAnimationPreviewAndUndo() {
        try {
            const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
            const auto rom=Rom::open(path);auto project=Project::fromRom(rom);QUndoStack undo;
            CutsceneEditor cutscenes(rom,nullptr,&project,&undo,[]{});
            QTRY_VERIFY(cutscenes.findChild<QListWidget *>("cutsceneScenes")->count()>0);
            AnimationEditor edits(&project,&undo,[]{});
            AnimationOverrideEditor overrides(&project,&undo,[]{});
            RecordModel classEditor(&project,Table::Classes,&undo);
            auto inspector=cutscenes.findChild<ActorPoseEditor *>();QVERIFY(inspector);
            auto kind=inspector->findChild<QComboBox *>("actorSourceKind"),bank=inspector->findChild<QComboBox *>("actorBank");
            kind->setCurrentIndex(1);bank->setCurrentIndex(bank->findData(2));
            auto position=inspector->findChild<QSpinBox *>("actorPosition");position->setValue(1);
            auto view=inspector->findChild<QGraphicsView *>("actorCanvas");
            auto flat=[&]{QImage image(320,240,QImage::Format_RGBA8888);image.fill(Qt::transparent);QPainter painter(&image);view->scene()->render(&painter);return image;};
            const auto original=flat();
            edits.selectClass(2);auto source=edits.findChild<QComboBox *>("animationSource");
            source->setCurrentIndex(source->findData(3));edits.findChild<QPushButton *>("animationCopySource")->click();
            QCOMPARE(position->value(),1);const auto changed=flat();QVERIFY(changed!=original);
            undo.undo();QCOMPARE(flat(),original);QCOMPARE(position->value(),1);undo.redo();QCOMPARE(flat(),changed);
            undo.undo();
            auto cls=overrides.findChild<QComboBox *>("overrideClass"),action=overrides.findChild<QComboBox *>("overrideAction");
            cls->setCurrentIndex(cls->findData(2));action->setCurrentIndex(action->findData(5));
            auto normal=overrides.findChild<QComboBox *>("overrideNormal"),blocked=overrides.findChild<QComboBox *>("overrideBlocked");
            normal->setCurrentIndex(normal->findData(40));blocked->setCurrentIndex(blocked->findData(41));
            overrides.findChild<QPushButton *>("overrideAdd")->click();
            QCOMPARE(project.animationOverrides()->entries().size(),1);QVERIFY2(overrides.findChild<QLabel *>("overrideNotice")->text().isEmpty(),qPrintable(overrides.findChild<QLabel *>("overrideNotice")->text()));
            auto previewAction=inspector->findChild<QComboBox *>("actorAttackAction"),mode=inspector->findChild<QComboBox *>("actorAttackMode"),pose=inspector->findChild<QComboBox *>("actorPose");
            previewAction->setCurrentIndex(previewAction->findData(5));QCOMPARE(pose->currentData().toInt(),40);
            mode->setCurrentIndex(2);QCOMPARE(pose->currentData().toInt(),41);position->setValue(1);const auto blockedFrame=flat();
            undo.undo();QCOMPARE(pose->currentData().toInt(),project.bodyAnimationData()->selector(2,2,5,2,0));QCOMPARE(position->value(),1);
            undo.redo();QCOMPARE(pose->currentData().toInt(),41);QCOMPARE(flat(),blockedFrame);
            const int id=classEditor.appendClass(2,"Preview class");QCOMPARE(id,165);
            bank->setCurrentIndex(bank->findData(id));QCOMPARE(bank->currentData().toInt(),id);QCOMPARE(pose->currentData().toInt(),41);
            position->setValue(1);auto projected=inspector->findChild<QCheckBox *>("actorProjected");projected->setChecked(true);
            QVERIFY(!inspector->property("placementFrame").value<QImage>().isNull());
            undo.undo();QVERIFY(bank->findData(id)<0);undo.redo();QVERIFY(bank->findData(id)>=0);
            // A body-route edit must invalidate captured scene frames and replay to the same tick.
            auto scenes=ScenePrograms::load(rom);const quint32 key=0x1fa8bf4-0x594284;auto program=scenes.program(key);
            for(auto &command:program.commands)if(command.opcode==0x1b){command.operands[4]=0;command.operands[5]=0;break;}
            scenes.replace(key,program.encode());auto preview=cutscenes.findChild<ScenePreview *>();QVERIFY(preview);
            preview->selectScene(scenes,key,2);auto scenePosition=preview->findChild<QSpinBox *>("scenePosition");
            scenePosition->setValue(3);QTRY_COMPARE(preview->property("sceneTick").toInt(),3);const auto sceneOriginal=preview->renderedFrame();
            edits.selectClass(0x58);source->setCurrentIndex(source->findData(2));edits.findChild<QPushButton *>("animationCopySource")->click();
            QTRY_COMPARE(preview->property("sceneTick").toInt(),3);const auto sceneChanged=preview->renderedFrame();QVERIFY(sceneChanged!=sceneOriginal);
            scenePosition->setValue(1);QTRY_COMPARE(preview->property("sceneTick").toInt(),1);scenePosition->setValue(3);QTRY_COMPARE(preview->property("sceneTick").toInt(),3);QCOMPARE(preview->renderedFrame(),sceneChanged);
            undo.undo();QTRY_COMPARE(preview->property("sceneTick").toInt(),3);QCOMPARE(preview->renderedFrame(),sceneOriginal);
            undo.redo();QTRY_COMPARE(preview->property("sceneTick").toInt(),3);QCOMPARE(preview->renderedFrame(),sceneChanged);
            bank->setCurrentIndex(bank->findData(id));position->setValue(1);inspector->resize(1280,850);inspector->show();QVERIFY(inspector->grab().save("staged-animation-preview.png"));
        }catch(const Error &e){QFAIL(e.what());}
    }

    void attackAnimationOverrides() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty()) QSKIP("Private ROM required");
            auto project = Project::fromRom(Rom::open(path));
            QUndoStack undo;
            AnimationEditor animations(&project, &undo, []{});
            AnimationOverrideEditor editor(&project, &undo, []{});
            RecordModel classes(&project, Table::Classes, &undo);
            editor.resize(1100, 820); editor.show();
            auto cls = editor.findChild<QComboBox *>("overrideClass");
            auto action = editor.findChild<QComboBox *>("overrideAction");
            auto route = editor.findChild<QComboBox *>("overrideRoute");
            auto normal = editor.findChild<QComboBox *>("overrideNormal");
            auto blocked = editor.findChild<QComboBox *>("overrideBlocked");
            auto rows = editor.findChild<QTableWidget *>("overrideRows");
            auto add = editor.findChild<QPushButton *>("overrideAdd");
            auto update = editor.findChild<QPushButton *>("overrideUpdate");
            auto remove = editor.findChild<QPushButton *>("overrideRemove");
            auto clear = editor.findChild<QPushButton *>("overrideClear");
            auto notice = editor.findChild<QLabel *>("overrideNotice");
            QVERIFY(cls && action && route && normal && blocked && rows && add && update && remove && clear && notice);
            cls->setCurrentIndex(cls->findData(2));
            action->setCurrentIndex(action->findData(5));
            QVERIFY(normal->findData(40) >= 0); QVERIFY(blocked->findData(41) >= 0);
            normal->setCurrentIndex(normal->findData(40));
            blocked->setCurrentIndex(blocked->findData(41));
            add->click(); QCOMPARE(rows->rowCount(), 1); QCOMPARE(undo.count(), 1);
            QJsonObject expected{{"classId",2},{"actionId",5},{"normalSelector",40},{"blockedSelector",41}};
            QCOMPARE(project.animationOverrides()->entries()[0].toObject(), expected);
            normal->setCurrentIndex(normal->findData(41)); update->click();
            expected["normalSelector"] = 41;
            QCOMPARE(project.animationOverrides()->entries()[0].toObject(), expected);
            undo.undo(); QCOMPARE(normal->currentData().toInt(), 40);
            undo.redo(); QCOMPARE(normal->currentData().toInt(), 41);
            const auto beforeBad = project.animationOverrides()->json(); const int count = undo.count();
            add->click(); QCOMPARE(project.animationOverrides()->json(), beforeBad); QCOMPARE(undo.count(), count);
            QVERIFY(!notice->text().isEmpty());
            action->setEditText("No such action"); update->click();
            QCOMPARE(project.animationOverrides()->json(), beforeBad); QVERIFY(notice->text().contains("search results"));
            action->setCurrentIndex(action->findData(5));
            // Clearing a selection must not reselect its still-current table row.
            cls->setCurrentIndex(cls->findData(1)); QCOMPARE(cls->currentData().toInt(), 1);
            QVERIFY(rows->selectedItems().isEmpty()); update->click();
            QCOMPARE(project.animationOverrides()->json(), beforeBad);
            cls->setCurrentIndex(cls->findData(2)); route->setCurrentIndex(route->findData(0));
            QCOMPARE(route->currentData().toInt(), 0); normal->setCurrentIndex(normal->findData(40));
            blocked->setCurrentIndex(blocked->findData(41)); add->click();
            QCOMPARE(rows->rowCount(), 2); QVERIFY2(notice->text().isEmpty(), qPrintable(notice->text()));
            const auto twoRows = project.animationOverrides()->json();
            remove->click(); QCOMPARE(rows->rowCount(), 1); undo.undo();
            QCOMPARE(project.animationOverrides()->json(), twoRows);
            const int id = classes.appendClass(2, "Override test"); QCOMPARE(id, 165);
            QCOMPARE(rows->rowCount(), 4); QVERIFY(cls->findData(id) >= 0);
            animations.selectClass(id); cls->setCurrentIndex(cls->findData(id));
            undo.undo(); QCOMPARE(rows->rowCount(), 2); QCOMPARE(cls->findData(id), -1);
            QVERIFY(animations.findChild<QComboBox *>("animationClass")->findData(id) < 0);
            undo.redo(); QCOMPARE(rows->rowCount(), 4); QVERIFY(cls->findData(id) >= 0);
            const auto fourRows = project.animationOverrides()->json();
            clear->click(); QCOMPARE(rows->rowCount(), 0); undo.undo();
            QCOMPARE(project.animationOverrides()->json(), fourRows); QCOMPARE(rows->rowCount(), 4);
            QTemporaryDir work; project.saveFolder(work.filePath("project"));
            auto reopened = Project::fromRom(project.rom()); reopened.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(reopened.animationOverrides()->json(), fourRows);
            rows->selectRow(0); QVERIFY(editor.grab().save("attack-overrides-workspace.png"));
        } catch (const Error &e) { QFAIL(e.what()); }
    }

    void classAnimationAssignments() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                       tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (path.isEmpty() || tools.isEmpty())
                QSKIP("Private ROM and compiler required");
            const auto rom = Rom::open(path);
            const auto original = AnimationRoutes::load(rom);
            auto project = Project::fromRom(rom);
            const int id = project.appendClass(2, "Animation test");
            auto missing = *project.animationRoutes();
            missing.requireAssignment(id);
            project.setAnimationRoutes(missing.json());
            QUndoStack undo;
            AnimationEditor editor(&project, &undo, [] {});
            editor.resize(1100, 820);
            editor.show();
            editor.selectClass(id);
            auto sources = editor.findChild<QComboBox *>("animationSource");
            auto copy = editor.findChild<QPushButton *>("animationCopySource");
            auto handle = editor.findChild<QSpinBox *>("animationHandle0");
            auto speed = editor.findChild<QSpinBox *>("animationSpeed0");
            auto action = editor.findChild<QComboBox *>("animationAction");
            auto actions = editor.findChild<QTableWidget *>("animationActions");
            auto add = editor.findChild<QPushButton *>("animationAddAction");
            auto update = editor.findChild<QPushButton *>("animationUpdateAction");
            auto remove = editor.findChild<QPushButton *>("animationRemoveAction");
            auto notice = editor.findChild<QLabel *>("animationNotice");
            QVERIFY(sources && copy && handle && speed && action && actions && add && update &&
                    remove && notice);
            QVERIFY(!handle->isEnabled());
            QVERIFY(notice->text().contains("needs an animation source"));
            sources->setEditText("No such animation source");
            copy->click();
            QVERIFY(project.animationRoutes()->pending().contains(id));
            QVERIFY(notice->text().contains("search results"));
            sources->setCurrentIndex(sources->findData(3));
            copy->click();
            QVERIFY(project.animationRoutes()->entry(id) == original.entry(3));
            QVERIFY(handle->isEnabled());
            undo.undo();
            QVERIFY(project.animationRoutes()->pending().contains(id));
            QVERIFY(!handle->isEnabled());
            undo.redo();
            QVERIFY(project.animationRoutes()->entry(id) == original.entry(3));
            handle->setValue(original.row(2)[0]);
            speed->setValue(37);
            QCOMPARE(project.animationRoutes()->row(id)[0], original.row(2)[0]);
            QCOMPARE(project.animationRoutes()->entry(id).speeds[0], 37);
            const auto beforeBad = project.animationRoutes()->json();
            handle->setValue(65535);
            QCOMPARE(project.animationRoutes()->json(), beforeBad);
            QVERIFY(notice->text().contains("missing descriptor"));
            QCOMPARE(handle->value(), int(original.row(2)[0]));
            action->setCurrentIndex(action->findData(200));
            for (int mode = 0; mode < 3; ++mode) {
                auto input = editor.findChild<QSpinBox *>(QString("animationSelector%1").arg(mode));
                QVERIFY(input);
                input->setValue(40 + mode);
            }
            const int priorRows = actions->rowCount();
            add->click();
            QCOMPARE(actions->rowCount(), priorRows + 1);
            QCOMPARE(project.animationRoutes()->entry(id).selectors.right(5),
                     QByteArray::fromHex("c828292aff"));
            const auto added = project.animationRoutes()->json();
            add->click();
            QCOMPARE(project.animationRoutes()->json(), added);
            QVERIFY(notice->text().contains("already has"));
            editor.findChild<QSpinBox *>("animationSelector1")->setValue(38);
            update->click();
            QCOMPARE(project.animationRoutes()->entry(id).selectors.right(5),
                     QByteArray::fromHex("c828262aff"));
            remove->click();
            QCOMPARE(actions->rowCount(), priorRows);
            undo.undo();
            QCOMPARE(actions->rowCount(), priorRows + 1);
            const auto edited = project.animationRoutes()->entry(id);
            QCOMPARE(edited.kind, original.entry(3).kind);
            QCOMPARE(edited.attributes, original.entry(3).attributes);
            QVERIFY(project.animationRoutes()->entry(2) == original.entry(2));
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto reopened = Project::fromRom(rom);
            reopened.applyManifest(Project::readProject(work.filePath("project")));
            QVERIFY(reopened.animationRoutes()->entry(id) == edited);
            const auto built = rebuild(reopened, tools, work.path());
            QTemporaryDir candidates(
                QFileInfo(path).dir().filePath("Test ROMs/native-animation-ui-XXXXXX"));
            QVERIFY(candidates.isValid());
            candidates.setAutoRemove(false);
            const auto output = candidates.filePath(QFileInfo(path).fileName());
            exportRom(reopened, built, output);
            QVERIFY(AnimationRoutes::load(Rom::open(output)).entry(id) == edited);
            qInfo().noquote() << "Animation assignment candidate:" << output;
            QVERIFY(editor.grab().save("animation-assignments-workspace.png"));
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }

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
            auto models=editor.findChild<QListWidget *>("armyModels");QVERIFY(models);QVERIFY(models->count()>0);
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
    void classLabelsPreserveNames() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");
        if(path.isEmpty()) QSKIP("Private ROM required");
        QFile labels(":/catalog.json"); QVERIFY(labels.open(QIODevice::ReadOnly));
        const auto catalog=QJsonDocument::fromJson(labels.readAll()).object();
        auto project=Project::fromRom(Rom::open(path),catalog);
        const auto original=project.classData();
        const auto names=catalog["CLASS_NAMES"].toObject();
        for (int id=0; id<165; ++id)
            QCOMPARE(project.record(Table::Classes,id).name,names[QString::number(id)].toString());
        QUndoStack undo;
        RecordModel model(&project,Table::Classes,&undo);
        const auto index=model.index(154,1);
        QCOMPARE(index.data().toString(),QString("Sword Master (Kageiye)"));
        QCOMPARE(index.data(Qt::EditRole).toString(),QString("Sword Master"));
        QVERIFY(!model.setData(index,index.data(Qt::EditRole),Qt::EditRole));
        QCOMPARE(project.classNameBytes(154),original.entries()[154].name);
        QVERIFY(model.setData(index,"Custom duelist",Qt::EditRole));
        QCOMPARE(index.data().toString(),QString("Custom duelist"));
        undo.undo();
        QCOMPARE(index.data().toString(),QString("Sword Master (Kageiye)"));
        QCOMPARE(project.classNameBytes(154),original.entries()[154].name);
        undo.redo();
        QTemporaryDir work; project.saveFolder(work.filePath("project"));
        auto reopened=Project::fromRom(project.rom(),catalog);
        reopened.applyManifest(Project::readProject(work.filePath("project")));
        QCOMPARE(reopened.record(Table::Classes,154).name,QString("Custom duelist"));
        QCOMPARE(reopened.classNameBytes(154),QByteArray("Custom duelist"));
        QCOMPARE(reopened.record(Table::Classes,103).name,QString("Temple Cmdr (Baldwin)"));
    }
    void armyClassIsolation() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");
        if(path.isEmpty()) QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));
        const int added=project.appendClass(5,"Separate Knight");
        QUndoStack undo; ArmyEditor editor(&project,&undo,[]{}); editor.show();
        editor.selectClass(added);
        auto models=editor.findChild<QListWidget *>("armyModels");
        QSet<int> listed;
        for(int row=0;row<models->count();++row) listed << models->item(row)->data(Qt::UserRole+1).toInt();
        QVERIFY(listed.contains(5)); QVERIFY(listed.contains(added));
        const int source=ArmyArtwork::classModel(project.classData().entries()[5]);
        const auto front=project.armyArtwork()->indices(0,source), back=project.armyArtwork()->indices(1,source);
        auto canvas=editor.findChild<SpriteCanvas *>("armyCanvas");
        editor.findChild<QSpinBox *>("armyInk")->setValue((u8(front,0)+1)%256);
        QTest::mouseClick(canvas,Qt::LeftButton,Qt::NoModifier,QPoint(2,2));
        const int copy=ArmyArtwork::classModel(project.classData().entries()[added]);
        QVERIFY(copy!=source);
        QCOMPARE(project.armyArtwork()->indices(0,source),front);
        QCOMPARE(project.armyArtwork()->indices(1,source),back);
        QCOMPARE(project.armyArtwork()->indices(1,copy),back);
        const auto copied=project.armyArtwork()->indices(0,copy);
        QVERIFY(copied!=front);
        undo.undo(); QCOMPARE(ArmyArtwork::classModel(project.classData().entries()[added]),source);
        undo.redo(); QCOMPARE(project.armyArtwork()->indices(0,copy),copied);
        editor.selectClass(5);
        editor.findChild<QSpinBox *>("armyInk")->setValue((u8(front,0)+2)%256);
        QTest::mouseClick(canvas,Qt::LeftButton,Qt::NoModifier,QPoint(2,2));
        QCOMPARE(project.armyArtwork()->indices(0,copy),copied);
        QTemporaryDir work; project.saveFolder(work.filePath("project"));
        auto reopened=Project::fromRom(project.rom());
        reopened.applyManifest(Project::readProject(work.filePath("project")));
        QCOMPARE(reopened.armyArtwork()->json(),project.armyArtwork()->json());
        QCOMPARE(reopened.armyArtwork()->indices(0,copy),copied);
        editor.selectClass(added);
        editor.findChild<QPushButton *>("armyRestore")->click();
        QCOMPARE(ArmyArtwork::classModel(project.classData().entries()[added]),source);
        QCOMPARE(project.armyArtwork()->indices(0,source),front);
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
    void spriteUsedColorPicker() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        QUndoStack undo;
        SpriteEditor editor(&project, &undo, [] {});
        editor.resize(1500, 900);
        editor.show();
        auto asset = SpriteAsset::blank("Color scopes", {300, 1});
        asset.kind = "sequence";
        for (int x = 0; x < 300; ++x)
            asset.frames[0].layers[0].image.setPixel(x, 0,
                                                     qRgba(x & 255, x >> 8, 17, 64 + x % 128));
        auto hidden = asset.frames[0].layers[0];
        hidden.id = SpriteLibrary::newId();
        hidden.visible = false;
        hidden.image = QImage(1, 1, QImage::Format_RGBA8888);
        const QColor hiddenColor(8, 9, 10, 0);
        hidden.image.fill(hiddenColor);
        asset.frames[0].layers << hidden;
        auto second = asset.frames[0];
        second.id = SpriteLibrary::newId();
        second.layers.resize(1);
        second.layers[0].image.fill(QColor(2, 3, 4, 7));
        asset.frames << second;
        editor.addAsset(asset);
        editor.findChild<QListWidget *>("spriteLayers")->setCurrentRow(1);
        const auto original = project.sprites().json();
        const auto initialUndo = undo.count();
        QTimer::singleShot(0, &editor, [&] {
            auto dialog = editor.findChild<QDialog *>("spriteUsedColorsDialog");
            QVERIFY(dialog);
            struct Cleanup {
                QDialog *d;
                ~Cleanup() {
                    if (d->isVisible())
                        d->reject();
                }
            } cleanup{dialog};
            QTRY_VERIFY_WITH_TIMEOUT(dialog->property("colorsReady").toBool(), 10000);
            auto scope = dialog->findChild<QComboBox *>("spriteColorScope");
            auto grid = dialog->findChild<QListWidget *>("spriteUsedColorGrid");
            auto count = dialog->findChild<QLabel *>("spriteColorCount");
            QVERIFY(count->text().startsWith("300 used colors"));
            QCOMPARE(grid->count(), 128);
            const auto first = grid->item(0)->data(Qt::UserRole);
            dialog->findChild<QPushButton *>("spriteColorsNext")->click();
            QVERIFY(grid->item(0)->data(Qt::UserRole) != first);
            scope->setCurrentIndex(2);
            scope->setCurrentIndex(0);
            scope->setCurrentIndex(1);
            QTRY_VERIFY_WITH_TIMEOUT(dialog->property("colorsReady").toBool(), 10000);
            QVERIFY(count->text().startsWith("301 used colors"));
            scope->setCurrentIndex(2);
            QTRY_VERIFY_WITH_TIMEOUT(dialog->property("colorsReady").toBool(), 10000);
            QVERIFY(count->text().startsWith("302 used colors"));
            auto item = grid->item(0);
            QCOMPARE(item->data(Qt::UserRole).toUInt(), hiddenColor.rgba());
            grid->setFocus();
            QTest::mouseClick(grid->viewport(), Qt::LeftButton, Qt::NoModifier,
                              grid->visualItemRect(item).center());
            QTest::keyClick(grid, Qt::Key_Right);
            QTest::keyClick(grid, Qt::Key_Left);
            QCOMPARE(grid->currentItem()->data(Qt::UserRole).toUInt(), hiddenColor.rgba());
            QVERIFY(dialog->grab().save("sprite-used-colors.png"));
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
        editor.findChild<QPushButton *>("spriteUsedColors")->click();
        QCOMPARE(editor.findChild<QPushButton *>("spriteColor")->text(),
                 hiddenColor.name(QColor::HexArgb));
        QCOMPARE(project.sprites().json(), original);
        QCOMPARE(undo.count(), initialUndo);
        QTimer::singleShot(0, &editor, [&] {
            auto d = editor.findChild<QDialog *>("spriteUsedColorsDialog");
            QVERIFY(d);
            d->findChild<QComboBox *>("spriteColorScope")->setCurrentIndex(2);
            d->reject();
        });
        editor.findChild<QPushButton *>("spriteUsedColors")->click();
        QCOMPARE(editor.findChild<QPushButton *>("spriteColor")->text(),
                 hiddenColor.name(QColor::HexArgb));
        QCOMPARE(project.sprites().json(), original);
        QCOMPARE(undo.count(), initialUndo);
        auto canvas = editor.findChild<SpriteCanvas *>();
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier,
                          QPoint(2 * canvas->zoom() + 2, 2));
        QCOMPARE(project.sprites().asset(asset.id).frames[0].layers[0].image.pixelColor(2, 0),
                 hiddenColor);
        undo.undo();
        QCOMPARE(project.sprites().json(), original);
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
        applyEditorTheme(*qApp);
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
    void saveArmyLiveWorkflow() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        try {
            QTemporaryDir work;
            for(const auto script:{"save-unit-mips.cjs","army-create-mips.cjs","army-legion-mips.cjs"}) {
                QProcess generator;generator.start("node",{QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath(script),work.path(),"generate"});
                QVERIFY(generator.waitForFinished(30000));QVERIFY2(!generator.exitCode(),generator.readAllStandardError().constData());
            }
            auto fixture=[&](const QString &file) {
                QFile payload(work.filePath(file));if(!payload.open(QIODevice::ReadOnly))throw Error("Missing save fixture");
                QByteArray bytes(0x10000,0);bytes.replace(0x3a,6180,payload.readAll());
                if(!lc_save_record_seal(reinterpret_cast<unsigned char *>(bytes.data()+0x10),0x1850,0,0))throw Error("Invalid save fixture");return bytes;
            };
            auto project=Project::fromRom(Rom::open(path));SaveEditor editor(&project);editor.resize(1850,1050);editor.show();
            const auto input=fixture("unit-payload.bin");editor.loadBytes(input);
            project.loadNameTables();const auto originalLists=*project.nameTables();auto renamedLists=originalLists;
            for(int i=0;i<256;++i){renamedLists.setRandomName(0,i,"MaleName");renamedLists.setRandomName(1,i,"FemaleName");renamedLists.setRandomName(2,i,"BeastName");}
            const auto classes=project.classData();
            QCOMPARE(editor.document()->randomCharacterName(project.rom(),classes,5,42,{},&renamedLists),QString("MaleName"));
            QCOMPARE(editor.document()->randomCharacterName(project.rom(),classes,4,42,{},&renamedLists),QString("FemaleName"));
            QCOMPARE(editor.document()->randomCharacterName(project.rom(),classes,71,42,{},&renamedLists),QString("BeastName"));
            for(int id=1;id<classes.entries().size();++id)QVERIFY(!editor.document()->randomCharacterName(project.rom(),classes,id,42,{},&originalLists).isEmpty());
            auto panel=editor.findChild<SaveUnitEditor *>("saveSquads");QVERIFY(panel);
            auto tabs=editor.findChild<QTabWidget *>();QCOMPARE(tabs->widget(0),panel);QCOMPARE(tabs->currentIndex(),0);
            for(const auto *removed:{"saveUnitApply","saveUnitDiscard","saveSwapUnit","saveReserveMember","saveNewUnitLeader","saveNewUnitCell"})QVERIFY(!editor.findChild<QWidget *>(removed));
            panel->selectUnit(0);panel->selectCharacter(2);
            auto strength=editor.findChild<QSpinBox *>("saveCard_strength");QVERIFY(strength);
            const int oldStrength=editor.document()->value(SaveTable::Characters,2,"strength").toInt();
            strength->setValue(oldStrength+3);QCOMPARE(editor.document()->value(SaveTable::Characters,2,"strength").toInt(),oldStrength+3);
            QVERIFY(!editor.hasPendingUnitEdits());QVERIFY(editor.findChild<QPushButton *>("exportSave")->isEnabled());
            editor.undoStack()->undo();QCOMPARE(strength->value(),oldStrength);editor.undoStack()->redo();QCOMPARE(strength->value(),oldStrength+3);
            editor.undoStack()->undo();QCOMPARE(editor.document()->serialize(),input);
            auto unity=editor.findChild<QSpinBox *>("saveUnitUnity");unity->setValue(44);
            QCOMPARE(editor.document()->units().first().unity,44);editor.undoStack()->undo();QCOMPARE(editor.document()->serialize(),input);
            auto board=editor.findChild<FormationBoard *>("saveUnitCellBoard");QVERIFY(board);
            editor.findChild<QTabWidget *>("saveArmyDetails")->setCurrentIndex(1);
            QCoreApplication::processEvents();
            // Grab near the head; moving the sprite base to tile 2 must place it
            // there even though the pointer itself remains above that tile.
            const QPoint from=board->cellCenter(4)-QPoint(0,35),delta=board->cellCenter(2)-board->cellCenter(4),to=from+delta;
            QTest::mousePress(board,Qt::LeftButton,Qt::NoModifier,from);
            QMouseEvent move(QEvent::MouseMove,to,board->mapToGlobal(to),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(board,&move);
            QTest::mouseRelease(board,Qt::LeftButton,Qt::NoModifier,to);
            auto actual=editor.document()->units().first();QCOMPARE(actual.cells[1],2);
            editor.undoStack()->undo();QCOMPARE(editor.document()->serialize(),input);
            auto reserves=editor.findChild<QListWidget *>("saveArmyReserve");QVERIFY(reserves);
            auto drop=[&](QWidget *target,QPoint point,const QMimeData &mime) {
                QDragEnterEvent enter(point,Qt::MoveAction,&mime,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(target,&enter);
                QDropEvent event(point,Qt::MoveAction,&mime,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(target,&event);return event.isAccepted();
            };
            SaveArmyDrag member(panel,0,1,2),foreign(&editor,0,1,2),leader(panel,0,0,1);
            QVERIFY(!drop(reserves->viewport(),{20,20},foreign));QVERIFY(!drop(reserves->viewport(),{20,20},leader));
            QVERIFY(drop(reserves->viewport(),{20,20},member));QVERIFY(editor.document()->reserveMembers().contains(2));
            SaveArmyDrag reserve(panel,-1,-1,2);QVERIFY(drop(board,board->cellCenter(4),reserve));
            QCOMPARE(editor.document()->units().first().members[1],2);
            editor.undoStack()->undo();editor.undoStack()->undo();QCOMPARE(editor.document()->serialize(),input);
            // Army additions and squad assignment use the icon picker and share stock.
            bool picked=false;
            QTimer::singleShot(0,&editor,[&] {
                auto dialog=editor.findChild<QDialog *>("shopItemPicker");if(!dialog)return;
                auto list=dialog->findChild<QListWidget *>("shopItemChoices");
                for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole).toInt()==24) {
                    picked=!list->item(i)->icon().isNull();list->setCurrentRow(i);dialog->accept();return;
                }dialog->reject();
            });
            editor.findChild<QPushButton *>("saveAddConsumables")->click();QVERIFY(picked);
            panel->selectUnit(0);auto supply=static_cast<SaveItemPicker *>(editor.findChild<QComboBox *>("saveUnitSupply1"));QVERIFY(supply->findData(24)>=0);
            picked=false;QTimer::singleShot(0,&editor,[&] {
                auto dialog=editor.findChild<QDialog *>("shopItemPicker");if(!dialog)return;
                auto list=dialog->findChild<QListWidget *>("shopItemChoices");for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole).toInt()==24){list->setCurrentRow(i);picked=true;dialog->accept();return;}dialog->reject();
            });supply->showPopup();QVERIFY(picked);QCOMPARE(editor.document()->units().first().supplies[1],24);
            auto prepared=*editor.document();const auto refresh=prepared.planArmyRefresh(project);prepared.apply(refresh,true);
            const auto dest=work.filePath("live-edits.sra");editor.exportFile(dest);QFile exported(dest);QVERIFY(exported.open(QIODevice::ReadOnly));
            QCOMPARE(SaveDocument::open(project.rom(),exported.readAll()).memory(),prepared.memory());
            // New recruits start with a game name; reroll and manual text both work.
            editor.loadBytes(input);const int newId=editor.document()->freeCharacterSlot();bool rolled=false;
            QTimer::singleShot(0,&editor,[&] {
                auto dialog=editor.findChild<QDialog *>("saveNewCharacterDialog");if(!dialog)return;
                auto name=dialog->findChild<QLineEdit *>("saveNewCharacterName");const auto first=name->text();
                dialog->findChild<QPushButton *>("saveNewCharacterReroll")->click();rolled=!first.isEmpty() && name->text()!=first;
                name->setText("Rowan");dialog->accept();
            });editor.findChild<QPushButton *>("saveArmyAddCharacter")->click();QVERIFY(rolled);
            QCOMPARE(editor.document()->value(SaveTable::Characters,newId,"name").toString(),QString("Rowan"));editor.undoStack()->undo();QCOMPARE(editor.document()->serialize(),input);
            // Form Squad highlights eligible reserves, then creates a real squad.
            const auto createInput=fixture("create.bin");editor.loadBytes(createInput);
            auto form=editor.findChild<QPushButton *>("saveCreateUnit");QVERIFY(form->isEnabled());form->click();
            QListWidgetItem *candidate=nullptr;for(int row=0;row<reserves->count();++row)if(reserves->item(row)->data(Qt::UserRole).toInt()==10)candidate=reserves->item(row);
            QVERIFY(candidate);QVERIFY(candidate->data(Qt::UserRole+10).toBool());
            QMetaObject::invokeMethod(reserves,"itemClicked",Qt::DirectConnection,Q_ARG(QListWidgetItem *,candidate));
            QCOMPARE(editor.document()->units().size(),6);QCOMPARE(editor.document()->units().last().members[0],10);QCOMPARE(editor.document()->units().last().cells[0],4);
            editor.undoStack()->undo();QCOMPARE(editor.document()->serialize(),createInput);
            // Legion rows move out of the independent grid; followers sit below.
            const auto legionInput=fixture("legion-create-holes.bin");editor.loadBytes(legionInput);panel->selectUnit(15);
            auto create=editor.findChild<QPushButton *>("saveCreateLegion");QVERIFY(create->isEnabled());create->click();
            auto independent=editor.findChild<QListWidget *>("saveUnitList"),legions=editor.findChild<QListWidget *>("saveLegionList");
            for(int i=0;i<independent->count();++i)QVERIFY(independent->item(i)->data(Qt::UserRole).toInt()!=15);
            panel->selectUnit(16);auto target=editor.findChild<QComboBox *>("saveLegionTarget");target->setCurrentIndex(target->findData(1));
            editor.findChild<QPushButton *>("saveJoinLegion")->click();
            int command=-1,follower=-1;for(int row=0;row<legions->count();++row){const int id=legions->item(row)->data(Qt::UserRole).toInt();if(id==15)command=row;if(id==16)follower=row;}
            QVERIFY(command>=0);QCOMPARE(follower,command+1);QCOMPARE(editor.document()->legions()[1].units,QVector<int>({15,16,255,255,255}));
            panel->selectUnit(16);editor.findChild<QPushButton *>("saveDetachLegionUnit")->click();QVERIFY(editor.document()->legions()[1].units[1]==255);
            editor.undoStack()->undo();panel->selectUnit(15);editor.findChild<QPushButton *>("saveDisbandLegion")->click();QCOMPARE(editor.document()->legions().size(),3);
            editor.undoStack()->undo();panel->selectCharacter(16);QCoreApplication::processEvents();QVERIFY(editor.grab().save("native/build/save-army-live.png"));
            while(editor.undoStack()->canUndo())editor.undoStack()->undo();QCOMPARE(editor.document()->serialize(),legionInput);
        } catch(const std::exception &e){QFAIL(e.what());}
    }
    void namesTextSynchronization() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));QUndoStack undo;NamesTextEditor editor(&project,&undo,[]{});editor.show();
        auto kind=editor.findChild<QComboBox *>("nameTableKind");auto text=editor.findChild<QLineEdit *>("nameTableText");
        auto table=editor.findChild<QTableWidget *>("nameTable");auto rename=editor.findChild<QPushButton *>("nameTableRename");
        auto select=[&](const QString &key,int id){kind->setCurrentIndex(kind->findData(key));for(int row=0;row<table->rowCount();++row)if(table->item(row,0)->text().toInt()==id)table->setCurrentCell(row,1);};
        const auto original=project.classNameBytes(5);select("classes",5);text->setText("Vanguard");rename->click();QCOMPARE(decodeText(project.classNameBytes(5)),QString("Vanguard"));
        RecordModel model(&project,Table::Classes,&undo);QCOMPARE(model.index(5,1).data(Qt::EditRole).toString(),QString("Vanguard"));
        QVERIFY(model.setData(model.index(5,1),"Guardian",Qt::EditRole));QCOMPARE(text->text(),QString("Guardian"));undo.undo();undo.undo();QCOMPARE(project.classNameBytes(5),original);
        select("random0",0);const auto old=text->text();text->setText("Rowan");rename->click();QCOMPARE(project.nameTables()->randomName(0,0),QString("Rowan"));undo.undo();QCOMPARE(text->text(),old);
        const auto beforeNames=project.manifest();
        auto add=editor.findChild<QPushButton *>("nameTableAdd");auto remove=editor.findChild<QPushButton *>("nameTableRemove");QVERIFY(add&&remove);
        for(int pool=0;pool<3;++pool) {
            select(QString("random%1").arg(pool),0);QVERIFY(add->isVisible());QVERIFY(!remove->isEnabled());
            QTimer::singleShot(0,[&] {auto dialog=qobject_cast<QInputDialog *>(QApplication::activeModalWidget());QVERIFY(dialog);dialog->setTextValue("NewName");dialog->accept();});
            add->click();QCOMPARE(table->rowCount(),257);QCOMPARE(table->currentRow(),256);
            QCOMPARE(project.nameTables()->randomName(pool,256),QString("NewName"));QVERIFY(remove->isEnabled());
            text->setText("Renamed");rename->click();QCOMPARE(project.nameTables()->randomName(pool,256),QString("Renamed"));
            remove->click();QCOMPARE(project.nameTables()->randomCount(pool),256);undo.undo();QCOMPARE(table->rowCount(),257);
            undo.redo();QCOMPARE(project.nameTables()->randomCount(pool),256);
        }
        QCOMPARE(project.manifest(),beforeNames);
        select("consumables",1);text->setText("Leaf");rename->click();QCOMPARE(project.record(Table::Consumables,1).name,QString("Leaf"));undo.undo();
        project.loadAttacks();select("attacks",4);text->setText("Blade hit");rename->click();QCOMPARE(AttackData::displayName(project.attacks()->entry(4)),QString("Blade hit"));undo.undo();
        editor.resize(950,700);QCoreApplication::processEvents();QVERIFY(editor.grab().save("native/build/names-and-text.png"));
        ToolsEditor tools(&project,&undo,[]{});auto pages=tools.findChild<QTabWidget *>("toolsPages");QVERIFY(pages);
        QCOMPARE(pages->tabText(pages->count()-1),QString("Names and Text"));
    }
    void saveExportConsistency() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            QProcess generator;
            generator.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                         .absoluteDir()
                                         .filePath("army-legion-mips.cjs"),
                                     work.path(), "generate"});
            QVERIFY(generator.waitForFinished(30000));
            QVERIFY2(!generator.exitCode(), generator.readAllStandardError().constData());
            QFile payload(work.filePath("legion-export.bin"));
            QVERIFY(payload.open(QIODevice::ReadOnly));
            const auto packed = payload.readAll();
            QByteArray input(0x10000, 0);
            for (int slot : {0, 1}) {
                const int at = int(lc_save_record_offset(slot, 0));
                input.replace(at + 42, 6180, packed);
                QVERIFY(lc_save_record_seal(reinterpret_cast<unsigned char *>(input.data() + at),
                                            lc_save_record_size(slot, 0), slot, 0));
            }
            const auto source = work.filePath("source.sra"), output = work.filePath("export.sra");
            atomicWrite(source, input);
            auto project = Project::fromRom(rom);
            SaveEditor editor(&project);
            editor.loadBytes(input, source);
            auto classes = project.record(Table::Classes, 5).bytes;
            classes[44] = 7;
            project.replaceBytes(Table::Classes, 5, classes);
            QVERIFY_THROWS_EXCEPTION(Error, editor.exportFile(source));
            QVERIFY_THROWS_EXCEPTION(Error, editor.exportFile(work.path()));
            QCOMPARE(editor.document()->serialize(), input);
            QCOMPARE(editor.undoStack()->count(), 0);
            editor.exportFile(output);
            QCOMPARE(editor.undoStack()->count(), 1);
            QVERIFY(editor.undoStack()->isClean());
            const auto saved = editor.document()->serialize();
            QVERIFY(saved != input);
            QFile file(output);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(file.readAll(), saved);
            file.close();
            auto reopened = SaveDocument::open(rom, saved);
            for (int slot : {0, 1}) {
                reopened.selectSlot(slot);
                QCOMPARE(u8(reopened.memory(), 0x1971f0 + 2 * 25 + 24), quint8(7));
                QCOMPARE(u8(reopened.memory(), 0x1969c1), quint8(7));
            }
            editor.exportFile(output);
            QCOMPARE(editor.undoStack()->count(), 1);
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), input);
            editor.undoStack()->redo();
            QCOMPARE(editor.document()->serialize(), saved);
            for (const auto &change :
                 {QVector<int>{5, 4, 2}, QVector<int>{5, 71, 0}, QVector<int>{33, 6, 1}}) {
                project = Project::fromRom(rom);
                editor.loadBytes(input, source);
                auto bytes = project.record(Table::Classes, change[0]).bytes;
                bytes[change[1]] = char(change[2]);
                project.replaceBytes(Table::Classes, change[0], bytes);
                const auto rejected = work.filePath(QString("rejected-%1.sra").arg(change[1]));
                QVERIFY_THROWS_EXCEPTION(Error, editor.exportFile(rejected));
                QVERIFY(!QFileInfo::exists(rejected));
                QCOMPARE(editor.document()->serialize(), input);
                QCOMPARE(editor.undoStack()->count(), 0);
            }
            QFile original(source);
            QVERIFY(original.open(QIODevice::ReadOnly));
            QCOMPARE(original.readAll(), input);
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void saveCharacterClassCoordination() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            QProcess generator;
            generator.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                         .absoluteDir()
                                         .filePath("army-legion-mips.cjs"),
                                     work.path(), "generate"});
            QVERIFY(generator.waitForFinished(30000));
            QVERIFY2(!generator.exitCode(), generator.readAllStandardError().constData());
            QFile payload(work.filePath("legion-class.bin"));
            QVERIFY(payload.open(QIODevice::ReadOnly));
            QByteArray input(0x10000, 0);
            input.replace(0x3a, 6180, payload.readAll());
            QVERIFY(lc_save_record_seal(reinterpret_cast<unsigned char *>(input.data() + 0x10),
                                        0x1850, 0, 0));
            auto project = Project::fromRom(rom);
            SaveEditor editor(&project);
            editor.resize(1250, 1200);
            editor.show();
            editor.loadBytes(input);
            editor.findChild<QTabWidget *>()->setCurrentWidget(
                editor.findChild<QWidget *>("saveSquads"));
            auto grid = editor.findChild<QTableView *>("saveCharacters");
            auto fields = editor.document()->fields(SaveTable::Characters);
            const auto field = std::find_if(fields.begin(), fields.end(), [](const auto &value) {
                return value.key == "secondaryClass";
            });
            QVERIFY(grid && field != fields.end());
            const int column = int(field - fields.begin()) + 1;
            const int row = editor.document()->rows(SaveTable::Characters).indexOf(18);
            QVERIFY(row >= 0);
            QVERIFY(grid->model()->setData(grid->model()->index(row, column), 44, Qt::EditRole));
            QCOMPARE(u8(editor.document()->memory(), 0x1971f0 + 2 * 25 + 24), quint8(7));
            QCOMPARE(u8(editor.document()->memory(), 0x1969c1), quint8(7));
            QCOMPARE(editor.undoStack()->count(), 1);
            const auto result = editor.document()->serialize();
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), input);
            editor.undoStack()->redo();
            QCOMPARE(editor.document()->serialize(), result);
            QVERIFY(!editor.edit(SaveTable::Characters, 1, "secondaryClass", 5));
            QCOMPARE(editor.document()->serialize(), result);
            QVERIFY(editor.findChild<QLabel *>("saveError")->text().contains("command"));
            const auto filename = work.filePath("class-coordinated.SaveRAM");
            editor.exportFile(filename);
            QFile file(filename);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(SaveDocument::open(rom, file.readAll()).memory(), editor.document()->memory());
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), input);
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void saveSquadStockControls() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            const auto rom = Rom::open(path);
            QTemporaryDir work;
            QProcess generator;
            generator.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                         .absoluteDir()
                                         .filePath("squad-stock-mips.cjs"),
                                     work.path(), "generate"});
            QVERIFY(generator.waitForFinished(30000));
            QVERIFY2(!generator.exitCode(), generator.readAllStandardError().constData());
            QFile payload(work.filePath("stock-two.bin"));
            QVERIFY(payload.open(QIODevice::ReadOnly));
            QByteArray input(0x10000, 0);
            input.replace(0x3a, 6180, payload.readAll());
            QVERIFY(lc_save_record_seal(reinterpret_cast<unsigned char *>(input.data() + 0x10),
                                        0x1850, 0, 0));
            auto project = Project::fromRom(rom);
            SaveEditor editor(&project);
            editor.resize(1250, 1150);
            editor.show();
            editor.loadBytes(input);
            editor.findChild<QTabWidget *>()->setCurrentWidget(
                editor.findChild<QWidget *>("saveSquads"));
            auto units = editor.findChild<QListWidget *>("saveUnitList");
            auto supply = editor.findChild<QComboBox *>("saveUnitSupply1");
            QVERIFY(units && supply);
            units->setCurrentRow(0);
            QVERIFY(supply->findData(24) >= 0);
            supply->setCurrentIndex(supply->findData(24));
            QCOMPARE(editor.document()->consumableAvailable(24), 1);
            units->setCurrentRow(1);
            supply->setCurrentIndex(supply->findData(24));
            QCOMPARE(editor.document()->consumableAvailable(24), 0);
            units->setCurrentRow(4);
            QCOMPARE(supply->findData(24), -1);
            QVERIFY(!editor.edit(SaveTable::Consumables, 23, "count", 1));
            QVERIFY(editor.findChild<QLabel *>("saveError")->text().contains("assigned"));
            units->setCurrentRow(0);
            QVERIFY(supply->findData(24) >= 0);
            supply->setCurrentIndex(supply->findData(0));
            QCOMPARE(editor.document()->consumableAvailable(24), 1);
            units->setCurrentRow(4);
            QVERIFY(supply->findData(24) >= 0);
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->consumableAvailable(24), 0);
            QCOMPARE(supply->findData(24), -1);
            editor.undoStack()->redo();
            QCOMPARE(editor.document()->consumableAvailable(24), 1);
            QVERIFY(supply->findData(24) >= 0);
            const auto filename = work.filePath("stock.SaveRAM");
            editor.exportFile(filename);
            QFile file(filename);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(SaveDocument::open(rom, file.readAll()).memory(), editor.document()->memory());
            editor.undoStack()->undo();
            editor.undoStack()->undo();
            editor.undoStack()->undo();
            QCOMPARE(editor.document()->serialize(), input);
        } catch (const Error &e) {
            QFAIL(e.what());
        }
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
    void recordNamesOnOneLine() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));QUndoStack undo;
        const auto before=project.manifest();
        for (auto table : {Table::Classes,Table::Items,Table::Consumables}) {
            RecordModel model(&project,table,&undo);
            QSortFilterProxyModel search;search.setSourceModel(&model);search.setFilterKeyColumn(1);
            int multiline=0;
            for(const auto &record:project.records(table)) {
                if(!record.name.contains('\n'))continue;
                ++multiline;const auto index=model.index(record.id,1);
                QVERIFY(!index.data().toString().contains('\n'));
                QCOMPARE(index.data(Qt::EditRole).toString(),record.name);
                search.setFilterFixedString(record.name.simplified());
                QVERIFY(search.mapFromSource(index).isValid());
            }
            QVERIFY(multiline>0);
            if(table==Table::Consumables) {
                QTableView view;view.setModel(&model);view.resize(900,680);view.setColumnWidth(0,55);view.setColumnWidth(1,700);
                view.show();QTest::qWait(30);
                QVERIFY(view.grab().save("native/build/full-consumable-names.png"));
            }
        }
        QCOMPARE(project.manifest(),before);QVERIFY(undo.isClean());
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
        QVERIFY(play->isEnabled());
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
        QVERIFY(play->isEnabled());
        auto loop=preview.findChild<QCheckBox *>("transformLoop");QVERIFY(loop);
        play->click();QCOMPARE(tick->value(),0);
        QTRY_COMPARE_WITH_TIMEOUT(play->text(),QString("Play"),2000);
        loop->setChecked(true);play->click();QTest::qWait(250);QCOMPARE(play->text(),QString("Pause"));play->click();
        preview.selectSequence(0, 99, 3, SceneCamera(rom.bytes().mid(0x2866f0, 56), 1));
        QVERIFY(preview.renderedFrame().isNull());
        QVERIFY(!step->isEnabled());
        QVERIFY(status->text().contains("group"));
        preview.selectCommand(program, 1);
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
        preview.seekToUpdate(0);position->setValue(14);
        QTRY_COMPARE(preview.property("sceneTick").toInt(),14);QCOMPARE(preview.renderedFrame(),skipped);
        preview.selectScene(scenes,id,0);position->setValue(10);QTRY_COMPARE(preview.property("sceneTick").toInt(),10);
        skip->click();position->setValue(14);QTRY_COMPARE(preview.property("sceneTick").toInt(),14);QCOMPARE(preview.renderedFrame(),skipped);
    }
    void pathSpritePreviewSeek() {
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
        const auto id = scenes.createScene("Path-oriented sprites", source, false);
        QVector<quint32> words{0x80000007U,
                               47,
                               0x3a,
                               1,
                               0,
                               0,
                               0,
                               70,
                               70,
                               130,
                               8,
                               0,
                               0x80000000U,
                               0x49,
                               0,
                               3,
                               0,
                               90,
                               quint32(-1),
                               3,
                               0,
                               0,
                               1,
                               0x63,
                               0,
                               30,
                               1,
                               0,
                               quint32(-36),
                               0,
                               180,
                               3,
                               0,
                               quint32(-1),
                               0x63,
                               1,
                               30,
                               1,
                               0,
                               48,
                               0,
                               180,
                               quint32(-1),
                               0,
                               quint32(-1)};
        for (int i = 0; i < 140; ++i) {
            if (i == 65)
                words << 0x63 << 0 << 0 << 0 << 0 << 0 << 0 << 180 << 4 << 0 << 0;
            words << 0 << 0x80000000U;
        }
        words << 0x80000001U << 0xff000004U;
        QByteArray bytes(words.size() * 4, 0);
        for (int i = 0; i < words.size(); ++i)
            put32(bytes, i * 4, words[i]);
        scenes.replace(id, bytes);
        ScenePreview preview(rom);
        preview.resize(1200, 900);
        preview.show();
        preview.selectScene(scenes, id, 0);
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        auto objects = preview.findChild<QComboBox *>("sceneLiveActors");
        position->setValue(35);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 35);
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        QCOMPARE(objects->count(), 2);
        const auto first = preview.renderedFrame();
        bool visible = false;
        for (int y = 0; y < 240 && !visible; ++y)
            for (int x = 0; x < 320 && !visible; ++x)
                visible = preview.spriteAt({x, y}) >= 0;
        QVERIFY(visible);
        position->setValue(124);
        QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), 124, 20000);
        QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        const auto later = preview.renderedFrame();
        QVERIFY(later != first);
        position->setValue(35);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 35);
        QCOMPARE(preview.renderedFrame(), first);
        position->setValue(124);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 124);
        QCOMPARE(preview.renderedFrame(), later);
        preview.seekToUpdate(0);
        position->setValue(35);
        QTRY_COMPARE(preview.property("sceneTick").toInt(), 35);
        QCOMPARE(preview.renderedFrame(), first);
        QCoreApplication::processEvents();
        QVERIFY(preview.grab().save("scene-path-sprites.png"));
    }
    void pathRibbonPreviewSeek() {
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
        const auto id = scenes.createScene("Path ribbon playback", source, false);
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
        preview.seekToUpdate(0);position->setValue(35);
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
        auto play=preview.findChild<QPushButton *>("scenePlay");
        auto loop=preview.findChild<QCheckBox *>("sceneLoop");QVERIFY(play&&loop);QVERIFY(play->isEnabled());
        play->click();QCOMPARE(preview.property("sceneTick").toInt(),0);QVERIFY(play->isChecked());play->click();
        QVERIFY(!preview.property("sceneStopped").toBool());
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
        preview.seekToUpdate(0);
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
    void savedStoryCastPreview() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        QTemporaryDir work;
        QProcess generator;
        generator.start("node", {QFileInfo(QString::fromUtf8(__FILE__))
                                     .absoluteDir()
                                     .filePath("story-cast-save-mips.cjs"),
                                 work.path()});
        QVERIFY(generator.waitForFinished(30000));
        QVERIFY2(generator.exitCode() == 0, generator.readAllStandardError().constData());
        QByteArray battery(0x10000, 0);
        for (int slot : {0, 1}) {
            QFile f(work.filePath(QString("cast-%1.bin").arg(slot)));
            QVERIFY(f.open(QIODevice::ReadOnly));
            const int at = int(lc_save_record_offset(slot, 0));
            battery.replace(at + 0x2a, 6180, f.readAll());
            QVERIFY(lc_save_record_seal(reinterpret_cast<unsigned char *>(battery.data() + at),
                                        lc_save_record_size(slot, 0), slot, 0));
        }
        MainWindow window;
        window.resize(1600, 1050);
        window.show();
        window.loadRom(path);
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<SaveEditor *>(), 20000);
        auto save = window.findChild<SaveEditor *>();
        auto cutscenes = window.findChild<CutsceneEditor *>();
        auto preview = window.findChild<ScenePreview *>();
        QVERIFY(save && cutscenes && preview);
        window.findChild<QTabWidget *>()->setCurrentWidget(cutscenes);
        cutscenes->findChild<QTabWidget *>("cutsceneDetailPages")->setCurrentWidget(preview);
        auto sceneList = cutscenes->findChild<QListWidget *>("cutsceneScenes");
        QVERIFY(sceneList);
        QTRY_VERIFY_WITH_TIMEOUT(sceneList->count() >= 1498, 20000);
        for (int row = 0; row < sceneList->count(); ++row)
            if (sceneList->item(row)->data(Qt::UserRole).toUInt() == 0x1fa8bf4 - 0x594284)
                sceneList->setCurrentRow(row);
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
        preview->selectScene(scenes, key, 2);
        auto use = preview->findChild<QPushButton *>("sceneUseSavedCast");
        auto reset = preview->findChild<QPushButton *>("sceneResetCast");
        auto source = preview->findChild<QLabel *>("sceneCastSource");
        auto primary = preview->findChild<QComboBox *>("sceneCast0");
        auto secondary = preview->findChild<QComboBox *>("sceneCastSecondary0");
        auto position = preview->findChild<QSpinBox *>("scenePosition");
        auto status = preview->findChild<QLabel *>("scenePlaybackStatus");
        QVERIFY(use->isEnabled());
        use->click();
        QVERIFY(source->text().contains("Open a save"));
        QCOMPARE(primary->currentData().toInt(), 0x58);
        save->loadBytes(battery);
        use->click();
        QCOMPARE(primary->currentData().toInt(), 0x54);
        QCOMPARE(secondary->currentData().toInt(), 0x55);
        QVERIFY(source->text().contains("slot 1"));
        QVERIFY2(!preview->property("sceneStopped").toBool(), qPrintable(status->text()));
        QCoreApplication::processEvents();
        QVERIFY(preview->isVisible());
        position->setValue(3);
        QTRY_VERIFY2(preview->property("sceneTick").toInt() == 3, qPrintable(status->text()));
        QVERIFY2(!preview->property("sceneStopped").toBool(), qPrintable(status->text()));
        auto actors = preview->findChild<QComboBox *>("sceneLiveActors");
        auto actorState = preview->findChild<QLabel *>("sceneActorState");
        auto findActor = preview->findChild<QPushButton *>("sceneFindActorAction");
        QVERIFY(actors && actorState && findActor);
        QVERIFY(actors->count() >= 5);
        actors->setCurrentIndex(actors->findData(1));
        QVERIFY(actorState->text().contains("Class pair 86 / 87"));
        const auto placement =
            std::find_if(program.commands.cbegin(), program.commands.cend(),
                         [](const auto &c) { return c.opcode == 0x14 && c.operands[0] == 1; });
        QVERIFY(placement != program.commands.cend());
        for (int axis = 0; axis < 3; ++axis)
            QVERIFY(actorState->text().contains(
                QString::number(qint32(placement->operands[4 + axis]) / 1000., 'f', 3)));
        const auto state = actorState->text();
        const auto fromSave = preview->renderedFrame();
        position->setValue(1);
        QTRY_COMPARE(preview->property("sceneTick").toInt(), 1);
        position->setValue(3);
        QTRY_VERIFY2(preview->property("sceneTick").toInt() == 3, qPrintable(status->text()));
        QCOMPARE(preview->renderedFrame(), fromSave);
        QCOMPARE(actors->currentData().toInt(), 1);
        QCOMPARE(actorState->text(), state);
        position->setValue(0);
        QTRY_COMPARE(preview->property("sceneTick").toInt(), 0);
        QVERIFY(!findActor->isEnabled());
        position->setValue(3);
        QTRY_COMPARE(preview->property("sceneTick").toInt(), 3);
        QCOMPARE(actors->currentData().toInt(), 1);
        QCOMPARE(actorState->text(), state);
        auto pages = cutscenes->findChild<QTabWidget *>("cutsceneDetailPages");
        auto actions = cutscenes->findChild<SceneActionEditor *>();
        QVERIFY(actions && findActor->isEnabled());
        findActor->click();
        QCOMPARE(pages->currentWidget(), actions);
        auto actionList = actions->findChild<QListWidget *>("sceneActionList");
        QVERIFY(actionList->currentItem()->text().contains("Create Actor"));
        QCOMPARE(actions->findChild<QLineEdit *>("sceneActionField0")->text(), QString("1"));
        const auto selectedAction = actionList->currentRow();
        auto field = actions->findChild<QLineEdit *>("sceneActionField4");
        field->setText("-");
        QMetaObject::invokeMethod(field, "editingFinished");
        QVERIFY(actions->property("invalidEdits").toBool());
        pages->setCurrentWidget(preview);
        actors->setCurrentIndex(actors->findData(0));
        findActor->click();
        QCOMPARE(pages->currentWidget(), preview);
        QCOMPARE(actionList->currentRow(), selectedAction);
        QCOMPARE(field->text(), QString("-"));
        actions->findChild<QPushButton *>("sceneActionDiscard")->click();
        preview->selectScene(scenes, key, 2);
        position->setValue(3);
        QTRY_COMPARE(preview->property("sceneTick").toInt(), 3);
        actors->setCurrentIndex(actors->findData(1));

        QCoreApplication::processEvents();
        QVERIFY(window.grab().save("scene-saved-cast.png"));
        actors->showPopup();
        QTRY_VERIFY(actors->view()->isVisible());
        actors->view()->setCurrentIndex(actors->model()->index(actors->findData(2), 0));
        QCOMPARE(actors->view()->currentIndex().data(Qt::UserRole).toInt(), 2);
        position->setValue(4);
        QTRY_COMPARE(preview->property("sceneTick").toInt(), 4);
        QVERIFY(actors->view()->isVisible());
        QCOMPARE(actors->view()->currentIndex().data(Qt::UserRole).toInt(), 2);
        actors->hidePopup();
        QCOMPARE(actors->currentData().toInt(), 1); // Browsing did not commit a selection.
        position->setValue(3);
        QTRY_COMPARE(preview->property("sceneTick").toInt(), 3);

        secondary->setCurrentIndex(secondary->findData(0x54));
        QCOMPARE(preview->property("sceneTick").toInt(), 0);
        QCOMPARE(primary->currentData().toInt(), 0x54);
        QCOMPARE(source->text(), QString("Manual preview cast"));
        use->click();
        QCOMPARE(secondary->currentData().toInt(), 0x55);
        auto slot = save->findChild<QComboBox *>("saveSlot");
        slot->setCurrentIndex(slot->findData(1));
        use->click();
        QCOMPARE(primary->currentData().toInt(), 0x58);
        QCOMPARE(secondary->currentData().toInt(), 0x58);
        QVERIFY(source->text().contains("slot 2"));
        auto invalid = save->document()->memory();
        put8(invalid, 0x193681, 255);
        save->loadBytes(invalid);
        use->click();
        QVERIFY(source->text().contains("Cast unchanged"));
        QCOMPARE(primary->currentData().toInt(), 0x58);
        QCOMPARE(secondary->currentData().toInt(), 0x58);
        QCOMPARE(save->document()->serialize(), invalid);
        save->loadBytes(battery);
        use->click();
        reset->click();
        QCOMPARE(primary->currentData().toInt(), 0x58);
        QCOMPARE(secondary->currentData().toInt(), 0x58);
        QCOMPARE(source->text(), QString("Sample cast"));
        auto army = preview->findChild<QPushButton *>("sceneUseArmyCast");
        QVERIFY(army && army->isEnabled());
        QVERIFY(save->edit(SaveTable::Characters, 1, "class", 0x56));
        QVERIFY(save->edit(SaveTable::Characters, 1, "secondaryClass", 0x57));
        QVERIFY(save->edit(SaveTable::Characters, 1, "hpMax", 100));
        QVERIFY(save->edit(SaveTable::Characters, 1, "hpCur", 100));
        const auto armySave = save->document()->serialize();
        army->click();
        QCOMPARE(primary->currentData().toInt(), 0x56);
        QCOMPARE(secondary->currentData().toInt(), 0x57);
        QVERIFY(source->text().contains("Current army from slot 1"));
        QCOMPARE(preview->findChild<QComboBox *>("sceneCast1")->currentData().toInt(), 4);
        position->setValue(3);
        QTRY_COMPARE(preview->property("sceneTick").toInt(), 3);
        actors->setCurrentIndex(actors->findData(0));
        QVERIFY(actorState->text().contains("Class pair 86 / 87"));
        const auto armyPlacement =
            std::find_if(program.commands.cbegin(), program.commands.cend(),
                         [](const auto &c) { return c.opcode == 0x14 && c.operands[0] == 0; });
        QVERIFY(armyPlacement != program.commands.cend());
        for (int axis = 0; axis < 3; ++axis)
            QVERIFY(actorState->text().contains(
                QString::number(qint32(armyPlacement->operands[4 + axis]) / 1000., 'f', 3)));
        const auto armyFrame = preview->renderedFrame();
        QVERIFY(!armyFrame.isNull());
        position->setValue(0);
        QTRY_COMPARE(preview->property("sceneTick").toInt(), 0);
        position->setValue(3);
        QTRY_COMPARE(preview->property("sceneTick").toInt(), 3);
        QCOMPARE(preview->renderedFrame(), armyFrame);
        QCOMPARE(save->document()->serialize(), armySave);
        QVERIFY(save->edit(SaveTable::Characters, 1, "hpCur", 0));
        army->click();
        QCOMPARE(primary->currentData().toInt(), 4);
        save->undoStack()->undo();
        army->click();
        QCOMPARE(primary->currentData().toInt(), 0x56);
        QCOMPARE(scenes.json(), before);
        save->loadBytes(battery);
        reset->click();
        auto proxy = qobject_cast<QAbstractProxyModel *>(
            window.findChild<QTableView *>("classesTable")->model());
        QVERIFY(proxy);
        auto classes = qobject_cast<RecordModel *>(proxy->sourceModel());
        QVERIFY(classes);
        const int added = classes->appendClass(0x55, "Cast companion");
        QCOMPARE(added, 165);
        QCOMPARE(secondary->itemText(secondary->findData(added)), QString("Cast companion"));
        auto withAdded = save->document()->memory();
        put8(withAdded, 0x193681, quint8(added));
        save->loadBytes(withAdded);
        use->click();
        QCOMPARE(primary->currentData().toInt(), 0x54);
        QCOMPARE(secondary->currentData().toInt(), added);
        position->setValue(3);
        QTRY_VERIFY2(preview->property("sceneTick").toInt() == 3, qPrintable(status->text()));
        QVERIFY2(!preview->property("sceneStopped").toBool(), qPrintable(status->text()));
        // The active project undo stack removes the new class, not any save bytes.
        auto undoGroup = window.findChild<QUndoGroup *>();
        QVERIFY(undoGroup);
        undoGroup->undo();
        QCOMPARE(secondary->findData(added), -1);
        QCOMPARE(secondary->currentData().toInt(), 0x54);
        QVERIFY(source->text().contains("removed preview class"));
        QCOMPARE(save->document()->serialize(), withAdded);
        save->loadBytes(battery);
        reset->click();
        QCOMPARE(save->document()->serialize(), battery);
        QCOMPARE(save->undoStack()->count(), 0);
        QCOMPARE(scenes.json(), before);
        QVERIFY(!window.isWindowModified());
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
        for (quint32 preset : {33U, 3U, 1U, 2U, 34U}) {
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
            if (preset == 3 || preset == 1 || preset == 2) {
                preview.findChild<QPushButton *>("panelDown")->click();
                QCOMPARE(preview.property("sceneTick").toInt(), 21);
                QVERIFY(preview.renderedFrame() != ready);
            }
            preview.findChild<QPushButton *>("panelConfirm")->click();
            const int acceptedTick = preview.property("sceneTick").toInt();
            const auto accepted = preview.renderedFrame();
            if (preset == 3)
                QVERIFY(preview.grab().save("scene-briefing-panel.png"));
            if (preset == 2)
                QVERIFY(preview.grab().save("scene-repeat-confirmation.png"));
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
            preview.seekToUpdate(0);
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
                preview.seekToUpdate(0);
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
        preview.seekToUpdate(0);
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
        preview.seekToUpdate(0);
        seek(8);
        QCOMPARE(preview.renderedFrame(), closed);
    }
    void displayControlPreviewSeek() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        const auto source = scenes.original().entries().first();
        const auto id = scenes.createScene("Display blackout playback", source.key, false);
        QVector<quint32> words{0x80000006U, 0};
        for (int i = 0; i < 3; ++i)
            words << 0 << 0x80000000U;
        words << 0x84;
        for (int i = 0; i < 140; ++i)
            words << 0 << 0x80000000U;
        words << 0x85;
        for (int i = 0; i < 10; ++i)
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
        const auto visible = preview.renderedFrame();
        QVERIFY(visible.pixelColor(160, 120) != QColor(Qt::black));
        seek(4);
        const auto hidden = preview.renderedFrame();
        QCOMPARE(hidden.pixelColor(160, 120), QColor(Qt::black));
        QVERIFY(status->text().contains("blacked out"));
        preview.repaint();
        QCOMPARE(preview.renderedFrame(), hidden);
        preview.grab().save("display-blackout-preview.png");
        seek(140);
        QCOMPARE(preview.renderedFrame(), hidden);
        seek(144);
        QCOMPARE(preview.renderedFrame(), visible);
        seek(100);
        QCOMPARE(preview.renderedFrame(), hidden);
        seek(145);
        QCOMPARE(preview.renderedFrame(), visible);
        preview.seekToUpdate(0);
        seek(4);
        QCOMPARE(preview.renderedFrame(), hidden);
    }
    void titleRevealPreviewSeek() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        const auto rom = Rom::open(path);
        auto scenes = ScenePrograms::load(rom);
        const SceneId id(scenes.original().selectors()[0]);
        QVector<quint32> words{0x3b, 0, 240, 80, 160, 4, 1};
        for (int i = 0; i < 140; ++i)
            words << 0 << 0x80000000U;
        words << 0xb8 << 0x3b << 160 << 320 << 0 << 480 << 4 << 0;
        for (int i = 0; i < 10; ++i)
            words << 0 << 0x80000000U;
        words << 0xb7 << 0 << 0x80000000U << 0x80000001U << 0xff000006U;
        QByteArray source(words.size() * 4, 0);
        for (int i = 0; i < words.size(); ++i)
            put32(source, i * 4, words[i]);
        scenes.replace(id, source);
        ScenePreview preview(rom);
        preview.resize(1100, 800);
        preview.show();
        preview.selectScene(scenes, id, 2);
        auto position = preview.findChild<QSpinBox *>("scenePosition");
        auto status = preview.findChild<QLabel *>("scenePlaybackStatus");
        const auto seek = [&](int tick) {
            position->setValue(tick);
            QTRY_COMPARE_WITH_TIMEOUT(preview.property("sceneTick").toInt(), tick, 20000);
            QVERIFY2(!preview.property("sceneStopped").toBool(), qPrintable(status->text()));
        };
        seek(1);
        const auto first = preview.renderedFrame();
        QCOMPARE(first.pixelColor(160, 19), QColor(Qt::black));
        QCOMPARE(first.pixelColor(160, 20).alpha(), 0);
        seek(4);
        const auto closed = preview.renderedFrame();
        QCOMPARE(closed.pixelColor(160, 79), QColor(Qt::black));
        QCOMPARE(closed.pixelColor(160, 80).alpha(), 0);
        QCOMPARE(closed.pixelColor(160, 159).alpha(), 0);
        QCOMPARE(closed.pixelColor(160, 160), QColor(Qt::black));
        preview.repaint();
        QCOMPARE(preview.renderedFrame(), closed);
        seek(125);
        QCOMPARE(preview.renderedFrame(), closed);
        seek(141);
        const auto high = preview.renderedFrame();
        QCOMPARE(high.pixelColor(160, 59), QColor(Qt::black));
        QCOMPARE(high.pixelColor(160, 60).alpha(), 0);
        QCOMPARE(high.pixelColor(319, 200), QColor(Qt::black));
        seek(145);
        const auto released = preview.renderedFrame();
        QCOMPARE(released.pixelColor(160, 0).alpha(), 0);
        QCOMPARE(released.pixelColor(160, 239).alpha(), 0);
        seek(1);
        QCOMPARE(preview.renderedFrame(), first);
        seek(141);
        QCOMPARE(preview.renderedFrame(), high);
        preview.grab().save("title-reveal-preview.png");
        preview.seekToUpdate(0);
        seek(145);
        QCOMPARE(preview.renderedFrame(), released);
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
        preview.seekToUpdate(0);position->setValue(35);
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
        preview.seekToUpdate(0);preview.findChild<QPushButton *>("sceneStep")->click();QCOMPARE(preview.renderedFrame(),first);
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
        preview.seekToUpdate(0);
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
            // Explicit registered camera matches the Actor world scale0.1.
            // Keep the fixture's Actor crossing the table's occluding edge.
            words = QVector<quint32>{0x35, 0, 0, 0, 27200, 8000, 12700, 45000} + words;
            words << 0 << 0x80000000U << 0 << 0x80000000U << 0x80000001U
                  << (0xff000000U | quint32(source.program.resourceClass()));
            QByteArray bytes(words.size() * 4, 0);
            for (int i = 0; i < words.size(); ++i)
                put32(bytes, i * 4, words[i]);
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
        QVector<QPoint> frontPicks;
        for (int y = 24; y < 215; ++y)
            for (int x = 0; x < 319; ++x)
                if (preview.actorAt({x, y}) == 0)
                    frontPicks << QPoint(x, y);
        QVERIFY(!frontPicks.isEmpty());
        const auto behind = run(room + actor);
        QVERIFY(behind != front);
        QVERIFY(behind != background);
        int occluded = 0, visible = 0;
        for (const auto point : frontPicks) {
            if (preview.actorAt(point) < 0) {
                QCOMPARE(behind.pixelColor(point), background.pixelColor(point));
                ++occluded;
            } else {
                QCOMPARE(preview.actorAt(point), 0);
                ++visible;
            }
        }
        QVERIFY(occluded > 0 && visible > 0);
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
    void spritePreviewSelection() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto programs = *project.scenePrograms();
            SceneId source;
            for (const auto &entry : programs.original().entries())
                if (entry.program.resourceClass() == 5 && programs.canCreateBlank(entry.key)) {
                    source = entry.key;
                    break;
                }
            QVERIFY(source);
            const auto id = programs.createScene("Sprite selection", source, true);
            auto program = programs.program(id);
            const int point = SceneActions::positions(program).first().command;
            program = SceneActions::insert(program, point, 0x14);
            for (const auto &[field, text] :
                 QVector<QPair<int, QString>>{{1, "30"}, {2, "1"}, {3, "2"}, {4, "-60.000"}})
                program = SceneActions::edit(program, point, field, text);
            program = SceneActions::insert(program, point + 1, 0x46);
            program = SceneActions::edit(program, point + 1, 4, "48");
            program = SceneActions::insert(program, point + 2, 0xe, 30);
            programs.replace(id, program.source);
            project.setScenePrograms(programs.json());
            QUndoStack undo;
            CutsceneEditor editor(project.rom(), nullptr, &project, &undo);
            editor.resize(1400, 1000);
            editor.show();
            auto scenes = editor.findChild<QListWidget *>("cutsceneScenes");
            QTRY_COMPARE_WITH_TIMEOUT(scenes->count(), 1499, 10000);
            for (int i = 0; i < scenes->count(); ++i)
                if (scenes->item(i)->data(Qt::UserRole + 1).toString() == id.authoredId)
                    scenes->setCurrentRow(i);
            auto preview = editor.findChild<ScenePreview *>();
            auto pages = editor.findChild<QTabWidget *>("cutsceneDetailPages");
            pages->setCurrentWidget(preview);
            auto seek = preview->findChild<QSpinBox *>("scenePosition");
            seek->setValue(2);
            QTRY_COMPARE(preview->property("sceneTick").toInt(), 2);
            QVERIFY(!preview->property("sceneStopped").toBool());
            auto objects = preview->findChild<QComboBox *>("sceneLiveActors");
            QCOMPARE(objects->count(), 2);
            QCOMPARE(objects->itemText(0), QString("Actor 0"));
            QCOMPARE(objects->itemText(1), QString("Sprite 0"));
            auto findPixel = [&] {
                for (int y = 0; y < 240; ++y)
                    for (int x = 0; x < 320; ++x)
                        if (preview->spriteAt({x, y}) == 0)
                            return QPoint(x, y);
                return QPoint(-1, -1);
            };
            const auto pixel = findPixel();
            QVERIFY(pixel.x() >= 0);
            QCOMPARE(preview->actorAt(pixel), -1);
            const auto frame = preview->renderedFrame();
            const auto before = project.scenePrograms()->json();
            auto view = preview->findChild<QGraphicsView *>("sceneCanvas");
            QCoreApplication::processEvents();
            const auto viewportPoint = view->mapFromScene(QPointF(pixel) + QPointF(.5, .5));
            QTest::mouseClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, viewportPoint);
            QCOMPARE(objects->currentText(), QString("Sprite 0"));
            QCOMPARE(objects->currentData(Qt::UserRole + 1).toInt(), int(SceneObjectKind::Sprite));
            QVERIFY(preview->findChild<QLabel *>("sceneActorState")->text().contains("X 48"));
            QCOMPARE(preview->renderedFrame(), frame);
            QCOMPARE(project.scenePrograms()->json(), before);
            seek->setValue(0);
            QTRY_COMPARE(preview->property("sceneTick").toInt(), 0);
            QCOMPARE(preview->spriteAt(pixel), -1);
            seek->setValue(2);
            QTRY_COMPARE(preview->property("sceneTick").toInt(), 2);
            QCOMPARE(preview->spriteAt(pixel), 0);
            QCOMPARE(preview->renderedFrame(), frame);
            QCOMPARE(objects->currentText(), QString("Sprite 0"));
            QSignalSpy selected(preview, &ScenePreview::findSpriteAction);
            QTest::mouseDClick(view->viewport(), Qt::LeftButton, Qt::NoModifier, viewportPoint);
            QCOMPARE(selected.count(), 1);
            auto actions = editor.findChild<SceneActionEditor *>();
            QCOMPARE(pages->currentWidget(), actions);
            QVERIFY(actions->findChild<QListWidget *>("sceneActionList")
                        ->currentItem()
                        ->text()
                        .contains("Create scene sprite"));
            QCOMPARE(actions->findChild<QLineEdit *>("sceneActionField0")->text(), QString("0"));
            QCOMPARE(actions->findChild<QLineEdit *>("sceneActionField4")->text(), QString("48"));
            QCOMPARE(project.scenePrograms()->json(), before);
            pages->setCurrentWidget(preview);
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("scene-sprite-selection.png"));
            // Direct extended placement now navigates to its rotation Actions fields.
            auto extended = *project.scenePrograms();
            auto raw = extended.program(id);
            raw.commands[point + 1].opcode = 0x63;
            raw.commands[point + 1].operands = {0, 30,  1,           0,           48,
                                                0, 180, quint32(-1), quint32(-1), quint32(-1)};
            extended.replace(id, raw.encode());
            pushSceneProgramChange(&project, &undo, "Use extended sprite placement", extended);
            pages->setCurrentWidget(preview);
            seek->setValue(2);
            QTRY_COMPARE(preview->property("sceneTick").toInt(), 2);
            QVERIFY(!preview->property("sceneStopped").toBool());
            const auto rawPixel = findPixel();
            QVERIFY(rawPixel.x() >= 0);
            QCoreApplication::processEvents();
            QTest::mouseDClick(view->viewport(), Qt::LeftButton, Qt::NoModifier,
                               view->mapFromScene(QPointF(rawPixel) + QPointF(.5, .5)));
            QCOMPARE(pages->currentWidget(), actions);
            QVERIFY(actions->findChild<QListWidget *>("sceneActionList")
                        ->currentItem()
                        ->text()
                        .contains("Create rotated scene sprite"));
            QCOMPARE(actions->findChild<QLineEdit *>("sceneActionField6")->text(), QString("180"));
            QCOMPARE(project.scenePrograms()->program(id).source, raw.encode());
            // Path orientation now has its own fields; an unknown launch-selected
            // creation mode still routes to its exact source command.
            raw.commands[point + 1].operands[8] = 0;
            extended.replace(id, raw.encode());
            pushSceneProgramChange(&project, &undo, "Use path orientation", extended);
            preview->findSpriteAction(extended.resource(id).selectors.first(), 0);
            QCOMPARE(pages->currentWidget(), actions);
            QVERIFY(actions->findChild<QListWidget *>("sceneActionList")->currentItem()->text()
                        .contains("Create sprite with path heading"));
            raw.commands[point + 1].operands[9] = 0x08880003;
            extended.replace(id, raw.encode());
            pushSceneProgramChange(&project, &undo, "Use launch creation mode", extended);
            preview->findSpriteAction(extended.resource(id).selectors.first(), 0);
            auto commands = editor.findChild<QTableWidget *>("cutsceneCommands");
            QCOMPARE(commands->currentRow(), point + 1);
            QVERIFY(pages->currentWidget()->isAncestorOf(commands));
            QCOMPARE(project.scenePrograms()->program(id).source, raw.encode());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void imageTransitionCompletionWaitControls() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto scenes = *project.scenePrograms();
            SceneId source;
            for (const auto &entry : scenes.original().entries())
                if (entry.program.resourceClass() == 4) {
                    source = entry.key;
                    break;
                }
            QVERIFY(source);
            const auto id = scenes.createScene("Image completion controls", source, false);
            const QVector<quint32> words{
                0x80000007U, 47,   0x3a,        1,           0,           0,          0,
                70,          70,   130,         8,           0x50,        0,          20,
                4,           0x7d, 0x80000002U, 0x80000000U, 0x80000001U, 0xff000004U};
            QByteArray bytes(words.size() * 4, 0);
            for (int i = 0; i < words.size(); ++i)
                put32(bytes, i * 4, words[i]);
            const int point = 3;
            auto base = SceneActions::insert(DirectorProgram::decode(bytes), point, 0x46);
            base = SceneActions::insert(base, point + 1, 0xe, 70);
            scenes.replace(id, base.source);
            project.setScenePrograms(scenes.json());
            QUndoStack undo;
            SceneActionEditor editor(&project, &undo, [] {});
            editor.resize(1100, 700);
            editor.show();
            editor.selectScene(id, point);
            auto kind = editor.findChild<QComboBox *>("sceneActionKind");
            auto insert = editor.findChild<QPushButton *>("sceneActionInsert");
            kind->setCurrentIndex(kind->findData(0x54));
            QVERIFY(insert->isEnabled());
            insert->click();
            const auto accepted = project.scenePrograms()->program(id).source;
            QVERIFY(!editor.findChild<QLineEdit *>("sceneActionField0"));
            QVERIFY(
                editor.findChild<QLabel *>("sceneActionStatus")->text().contains("both finished"));
            ScenePreview preview(project.rom());
            preview.resize(850, 700);
            preview.show();
            auto live = preview.findChild<QComboBox *>("sceneLiveActors");
            QVERIFY(live);
            auto sprite = [&] {
                for (int i = 0; i < live->count(); ++i)
                    if (live->itemData(i, Qt::UserRole + 1).toInt() ==
                            int(SceneObjectKind::Sprite) &&
                        live->itemData(i).toInt() == 0)
                        return true;
                return false;
            };
            auto select = [&] {
                preview.selectScene(*project.scenePrograms(), id, 0);
                preview.findChild<QPushButton *>("sceneStep")->click();
                if (preview.property("sceneStopped").toBool())
                    throw Error(preview.findChild<QLabel *>("scenePlaybackStatus")->text());
            };
            select();
            QVERIFY(!sprite());
            auto seek = preview.findChild<QSpinBox *>("scenePosition");
            seek->setValue(24);
            QTRY_COMPARE(preview.property("sceneTick").toInt(), 24);
            QVERIFY2(!preview.property("sceneStopped").toBool(),
                     qPrintable(preview.findChild<QLabel *>("scenePlaybackStatus")->text()));
            QVERIFY(!sprite());
            const auto draining = preview.renderedFrame();
            seek->setValue(60);
            QTRY_COMPARE(preview.property("sceneTick").toInt(), 60);
            QVERIFY(sprite());
            const auto finished = preview.renderedFrame();
            seek->setValue(24);
            QTRY_COMPARE(preview.property("sceneTick").toInt(), 24);
            QVERIFY(!sprite());
            QCOMPARE(preview.renderedFrame(), draining);
            seek->setValue(60);
            QTRY_COMPARE(preview.property("sceneTick").toInt(), 60);
            QVERIFY(sprite());
            QCOMPARE(preview.renderedFrame(), finished);
            undo.undo();
            QCOMPARE(project.scenePrograms()->program(id).source, base.source);
            select();
            QVERIFY(sprite());
            undo.redo();
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
            select();
            QVERIFY(!sprite());
            editor.selectScene(id, point);
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("scene-image-completion-wait.png"));
            editor.findChild<QPushButton *>("sceneActionRemove")->click();
            QCOMPARE(project.scenePrograms()->program(id).source, base.source);
            undo.undo();
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void imageConstructionActionControls() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto scenes = *project.scenePrograms();
            SceneId source;
            for (const auto &entry : scenes.original().entries())
                if (entry.program.resourceClass() == 4) {
                    source = entry.key;
                    break;
                }
            QVERIFY(source);
            const auto id = scenes.createScene("Image construction controls", source, false);
            const QVector<quint32> words{0x80000007U, 47,          0x7d,       0x80000002U,
                                         0x80000000U, 0x80000001U, 0xff000004U};
            QByteArray bytes(words.size() * 4, 0);
            for (int i = 0; i < words.size(); ++i)
                put32(bytes, i * 4, words[i]);
            const int point = 1;
            const auto base = SceneActions::insert(DirectorProgram::decode(bytes), point, 0xe, 70);
            scenes.replace(id, base.source);
            project.setScenePrograms(scenes.json());
            QUndoStack undo;
            SceneActionEditor editor(&project, &undo, [] {});
            editor.resize(1100, 750);
            editor.show();
            editor.selectScene(id, point);
            auto kind = editor.findChild<QComboBox *>("sceneActionKind");
            auto insert = editor.findChild<QPushButton *>("sceneActionInsert");
            auto field = [&](int i) {
                return editor.findChild<QLineEdit *>(QString("sceneActionField%1").arg(i));
            };
            kind->setCurrentIndex(kind->findData(0x3a));
            QVERIFY(insert->isEnabled());
            insert->click();
            QCOMPARE(field(0)->text(), QString("1"));
            QCOMPARE(field(3)->text(), QString("70"));
            QCOMPARE(field(5)->text(), QString("130"));
            const auto once = project.scenePrograms()->program(id).source;
            insert->click();
            QCOMPARE(project.scenePrograms()->program(id).source, once);
            QVERIFY(editor.findChild<QLabel *>("sceneActionStatus")
                        ->text()
                        .contains("already constructs"));
            ScenePreview preview(project.rom());
            preview.show();
            auto frame = [&] {
                preview.selectScene(*project.scenePrograms(), id, 0);
                preview.findChild<QPushButton *>("sceneStep")->click();
                if (preview.property("sceneStopped").toBool())
                    throw Error(preview.findChild<QLabel *>("scenePlaybackStatus")->text());
                return preview.renderedFrame();
            };
            const auto initial = frame();
            undo.clear();
            field(1)->setText("60");
            const auto translated = frame();
            QVERIFY(translated != initial);
            undo.undo();
            QCOMPARE(frame(), initial);
            undo.redo();
            QCOMPARE(frame(), translated);
            editor.selectScene(id, point);
            undo.clear();
            field(3)->setText("-70");
            const auto mirrored = frame();
            QVERIFY(mirrored != translated);
            undo.undo();
            QCOMPARE(frame(), translated);
            undo.redo();
            QCOMPARE(frame(), mirrored);
            editor.selectScene(id, point);
            undo.clear();
            field(5)->setText("0");
            const auto transparent = frame();
            QVERIFY(transparent != mirrored);
            undo.undo();
            QCOMPARE(frame(), mirrored);
            undo.redo();
            QCOMPARE(frame(), transparent);
            editor.selectScene(id, point);
            field(5)->setText("210");
            field(6)->setText("0");
            QVERIFY(!editor.property("invalidEdits").toBool());
            const auto accepted = project.scenePrograms()->program(id).source;
            field(0)->setText("0");
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
            editor.findChild<QPushButton *>("sceneActionDiscard")->click();
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("scene-image-construction-actions.png"));
            editor.findChild<QPushButton *>("sceneActionRemove")->click();
            QCOMPARE(project.scenePrograms()->program(id).source, base.source);
            undo.undo();
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void imageTransitionActionControls() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto scenes = *project.scenePrograms();
            SceneId source;
            for (const auto &entry : scenes.original().entries())
                if (entry.program.resourceClass() == 4) {
                    source = entry.key;
                    break;
                }
            QVERIFY(source);
            const auto id = scenes.createScene("Image transition controls", source, false);
            const QVector<quint32> words{
                0x80000007U, 47,   0x3a,        1,           0,           0,          0,
                70,          70,   130,         8,           0x50,        0,          1,
                0,           0x7d, 0x80000002U, 0x80000000U, 0x80000001U, 0xff000004U};
            QByteArray bytes(words.size() * 4, 0);
            for (int i = 0; i < words.size(); ++i)
                put32(bytes, i * 4, words[i]);
            // Establish a settled image so reverse motion has distinct endpoints.
            const auto ready = SceneActions::insert(DirectorProgram::decode(bytes), 3, 0xe, 3);
            const int point = 9;
            const auto base = SceneActions::insert(ready, point, 0xe, 70);
            scenes.replace(id, base.source);
            project.setScenePrograms(scenes.json());
            QUndoStack undo;
            SceneActionEditor editor(&project, &undo, [] {});
            editor.resize(1100, 750);
            editor.show();
            editor.selectScene(id, point);
            auto kind = editor.findChild<QComboBox *>("sceneActionKind");
            auto insert = editor.findChild<QPushButton *>("sceneActionInsert");
            auto field = [&](int i) {
                return editor.findChild<QLineEdit *>(QString("sceneActionField%1").arg(i));
            };
            kind->setCurrentIndex(kind->findData(0x50));
            QVERIFY(insert->isEnabled());
            insert->click();
            QCOMPARE(field(0)->text(), QString("0"));
            QCOMPARE(field(1)->text(), QString("20"));
            QCOMPARE(field(2)->text(), QString("4"));
            ScenePreview preview(project.rom());
            preview.show();
            auto frame = [&] {
                preview.selectScene(*project.scenePrograms(), id, 0);
                for (int i = 0; i < 12; ++i)
                    preview.findChild<QPushButton *>("sceneStep")->click();
                if (preview.property("sceneStopped").toBool())
                    throw Error(preview.findChild<QLabel *>("scenePlaybackStatus")->text());
                return preview.renderedFrame();
            };
            const auto forward = frame();
            undo.clear();
            field(0)->setText("1");
            const auto reverse = frame();
            QVERIFY(reverse != forward);
            undo.undo();
            QCOMPARE(frame(), forward);
            undo.redo();
            QCOMPARE(frame(), reverse);
            editor.selectScene(id, point);
            undo.clear();
            field(1)->setText("40");
            const auto slower = frame();
            QVERIFY(slower != reverse);
            undo.undo();
            QCOMPARE(frame(), reverse);
            undo.redo();
            QCOMPARE(frame(), slower);
            editor.selectScene(id, point);
            undo.clear();
            field(2)->setText("0");
            QCOMPARE(project.scenePrograms()->program(id).commands[point].operands,
                     QVector<quint32>({1, 40, 0}));
            const auto plain = frame();
            undo.undo();
            QCOMPARE(frame(), slower);
            undo.redo();
            QCOMPARE(frame(), plain);
            editor.selectScene(id, point);
            const auto accepted = project.scenePrograms()->program(id).source;
            field(1)->setText("256");
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
            editor.findChild<QPushButton *>("sceneActionDiscard")->click();
            auto seek = preview.findChild<QSpinBox *>("scenePosition");
            seek->setValue(60);
            QTRY_COMPARE(preview.property("sceneTick").toInt(), 60);
            seek->setValue(12);
            QTRY_COMPARE(preview.property("sceneTick").toInt(), 12);
            QCOMPARE(preview.renderedFrame(), plain);
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("scene-image-transition-actions.png"));
            editor.findChild<QPushButton *>("sceneActionRemove")->click();
            QCOMPARE(project.scenePrograms()->program(id).source, base.source);
            undo.undo();
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void imageMotionActionControls() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto scenes = *project.scenePrograms();
            SceneId source;
            for (const auto &entry : scenes.original().entries())
                if (entry.program.resourceClass() == 4) {
                    source = entry.key;
                    break;
                }
            QVERIFY(source);
            const auto id = scenes.createScene("Image motion controls", source, false);
            const QVector<quint32> words{
                0x80000007U, 47,   0x3a,        1,           0,           0,          0,
                70,          70,   130,         8,           0x50,        0,          1,
                0,           0x7d, 0x80000002U, 0x80000000U, 0x80000001U, 0xff000004U};
            QByteArray bytes(words.size() * 4, 0);
            for (int i = 0; i < words.size(); ++i)
                put32(bytes, i * 4, words[i]);
            // The initial image transition establishes the native zoom state.
            const auto ready = SceneActions::insert(DirectorProgram::decode(bytes), 3, 0xe, 3);
            const int point = 9;
            auto base = SceneActions::insert(ready, point, 0xe, 40);
            scenes.replace(id, base.source);
            project.setScenePrograms(scenes.json());
            QUndoStack undo;
            SceneActionEditor editor(&project, &undo, [] {});
            editor.resize(1100, 850);
            editor.show();
            editor.selectScene(id, point);
            auto kind = editor.findChild<QComboBox *>("sceneActionKind");
            auto insert = editor.findChild<QPushButton *>("sceneActionInsert");
            auto field = [&](int i) {
                return editor.findChild<QLineEdit *>(QString("sceneActionField%1").arg(i));
            };
            kind->setCurrentIndex(kind->findData(0x76));
            QVERIFY(insert->isEnabled());
            insert->click();
            QCOMPARE(field(0)->text(), QString("-1"));
            QCOMPARE(field(1)->text(), QString("-1"));
            ScenePreview preview(project.rom());
            preview.show();
            auto frame = [&] {
                preview.selectScene(*project.scenePrograms(), id, 0);
                for (int i = 0; i < 12; ++i)
                    preview.findChild<QPushButton *>("sceneStep")->click();
                if (preview.property("sceneStopped").toBool())
                    throw Error(preview.findChild<QLabel *>("scenePlaybackStatus")->text());
                return preview.renderedFrame();
            };
            const auto still = frame();
            field(2)->setText("50");
            field(5)->setText("20");
            QVERIFY(!editor.property("invalidEdits").toBool());
            const auto panned = frame();
            QVERIFY(panned != still);
            undo.clear();
            field(4)->setText("1");
            const auto zoomed = frame();
            QVERIFY(zoomed != panned);
            undo.undo();
            QCOMPARE(frame(), panned);
            undo.redo();
            QCOMPARE(frame(), zoomed);
            editor.selectScene(id, point);
            const auto accepted = project.scenePrograms()->program(id).source;
            field(5)->setText("0");
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
            editor.findChild<QPushButton *>("sceneActionDiscard")->click();
            auto seek = preview.findChild<QSpinBox *>("scenePosition");
            seek->setValue(20);
            QTRY_COMPARE(preview.property("sceneTick").toInt(), 20);
            QVERIFY(preview.renderedFrame() != zoomed);
            seek->setValue(12);
            QTRY_COMPARE(preview.property("sceneTick").toInt(), 12);
            QCOMPARE(preview.renderedFrame(), zoomed);
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("scene-image-motion-actions.png"));
            editor.findChild<QPushButton *>("sceneActionRemove")->click();
            QCOMPARE(project.scenePrograms()->program(id).source, base.source);
            undo.undo();
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void ribbonActionControls() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto scenes = *project.scenePrograms();
            SceneId source;
            for (const auto &entry : scenes.original().entries())
                if (entry.program.resourceClass() == 4) {
                    source = entry.key;
                    break;
                }
            QVERIFY(source);
            const auto id = scenes.createScene("Ribbon action controls", source, false);
            const QVector<quint32> words{0x80000007U, 47,          0x3a,        1,          0, 0,
                                         0,           70,          70,          130,        8, 0x7d,
                                         0x80000002U, 0x80000000U, 0x80000001U, 0xff000004U};
            QByteArray bytes(words.size() * 4, 0);
            for (int i = 0; i < words.size(); ++i)
                put32(bytes, i * 4, words[i]);
            const int point = 2;
            auto base = SceneActions::insert(DirectorProgram::decode(bytes), point, 0xe, 40);
            scenes.replace(id, base.source);
            project.setScenePrograms(scenes.json());
            QUndoStack undo;
            SceneActionEditor editor(&project, &undo, [] {});
            editor.resize(1100, 850);
            editor.show();
            editor.selectScene(id, point);
            auto kind = editor.findChild<QComboBox *>("sceneActionKind");
            auto insert = editor.findChild<QPushButton *>("sceneActionInsert");
            auto position = editor.findChild<QComboBox *>("sceneActionPosition");
            auto field = [&](int i) {
                return editor.findChild<QLineEdit *>(QString("sceneActionField%1").arg(i));
            };
            auto discard = editor.findChild<QPushButton *>("sceneActionDiscard");
            ScenePreview preview(project.rom());
            preview.show();
            auto frame = [&] {
                preview.selectScene(*project.scenePrograms(), id, 0);
                preview.findChild<QPushButton *>("sceneStep")->click();
                if (preview.property("sceneStopped").toBool())
                    throw Error(preview.findChild<QLabel *>("scenePlaybackStatus")->text());
                return preview.renderedFrame();
            };
            const auto background = frame();
            kind->setCurrentIndex(kind->findData(0x49));
            insert->click();
            QCOMPARE(project.scenePrograms()->program(id).commands[point].opcode, quint32(0x49));
            field(7)->setText("1");
            QVERIFY(!editor.property("invalidEdits").toBool());
            const auto first = frame();
            QVERIFY(first != background);
            undo.clear(); // Establish a baseline before testing one field's undo.
            field(6)->setText("2");
            const auto colored = frame();
            QVERIFY(colored != first);
            undo.undo();
            QCOMPARE(frame(), first);
            undo.redo();
            QCOMPARE(frame(), colored);
            editor.selectScene(id, point);
            const auto accepted = project.scenePrograms()->program(id).source;
            field(4)->setText("2");
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
            discard->click();
            field(1)->setText("59");
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
            discard->click();
            field(1)->setText("0");
            field(2)->setText("6"); // Five source points in the original ROM.
            QVERIFY(!editor.property("invalidEdits").toBool());
            kind->setCurrentIndex(kind->findData(0x4a));
            position->setCurrentIndex(position->findData(point + 1));
            insert->click();
            field(2)->setText("4");
            QVERIFY(!editor.property("invalidEdits").toBool());
            const auto endpoint = project.scenePrograms()->program(id).source;
            field(2)->setText("5");
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, endpoint);
            discard->click();
            editor.selectScene(id, point);
            field(2)->setText("0"); // This shorter path would invalidate the following point4.
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, endpoint);
            discard->click();
            editor.selectScene(id, point + 1);
            field(2)->setText("1");
            editor.selectScene(id, point);
            field(2)->setText("0");
            field(1)->setText("3");
            QVERIFY(!editor.property("invalidEdits").toBool());
            editor.selectScene(id, point + 1);
            kind->setCurrentIndex(kind->findData(0x4b));
            position->setCurrentIndex(position->findData(point + 2));
            insert->click();
            QCOMPARE(field(0)->text(), QString("0"));
            QCOMPARE(field(1)->text(), QString("1"));
            const auto faded = frame();
            QVERIFY(faded != background);
            field(1)->setText("0");
            QCOMPARE(frame(), background);
            undo.undo();
            QCOMPARE(frame(), faded);
            undo.redo();
            QCOMPARE(frame(), background);
            editor.selectScene(id, point + 2);
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("scene-ribbon-actions.png"));
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void pathSpriteActionControls() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto scenes = *project.scenePrograms();
            SceneId source;
            for (const auto &entry : scenes.original().entries())
                if (entry.program.resourceClass() == 4) {
                    source = entry.key;
                    break;
                }
            QVERIFY(source);
            const auto id = scenes.createScene("Path heading actions", source, false);
            const QVector<quint32> words{
                0x80000007U, 47, 0x3a, 1,    0,           0,           0,           70,          70,
                130,         8,  0x49, 0,    3,           0,           90,          quint32(-1), 3,
                0,           0,  1,    0x7d, 0x80000002U, 0x80000000U, 0x80000001U, 0xff000004U};
            QByteArray bytes(words.size() * 4, 0);
            for (int i = 0; i < words.size(); ++i)
                put32(bytes, i * 4, words[i]);
            auto base = DirectorProgram::decode(bytes);
            const int point = 3;
            base = SceneActions::insert(base, point, 0xe, 30);
            scenes.replace(id, base.source);
            project.setScenePrograms(scenes.json());
            QUndoStack undo;
            SceneActionEditor editor(&project, &undo, [] {});
            editor.resize(1100, 800);
            editor.show();
            editor.selectScene(id, point);
            auto kind = editor.findChild<QComboBox *>("sceneActionKind");
            auto insert = editor.findChild<QPushButton *>("sceneActionInsert");
            kind->setCurrentIndex(kind->findData(SceneActions::CreatePathSprite));
            insert->click();
            auto field = [&](int i) {
                return editor.findChild<QLineEdit *>(QString("sceneActionField%1").arg(i));
            };
            QCOMPARE(field(6)->text(), QString("3"));
            QCOMPARE(field(7)->text(), QString("0"));
            ScenePreview preview(project.rom());
            preview.show();
            auto frame = [&] {
                preview.selectScene(*project.scenePrograms(), id, 0);
                preview.findChild<QPushButton *>("sceneStep")->click();
                if (preview.property("sceneStopped").toBool())
                    throw Error(preview.findChild<QLabel *>("scenePlaybackStatus")->text());
                return preview.renderedFrame();
            };
            const auto first = frame();
            field(6)->setText("4");
            QVERIFY(!editor.property("invalidEdits").toBool());
            const auto other = frame();
            QVERIFY(other != first);
            undo.undo();
            QCOMPARE(frame(), first);
            undo.redo();
            QCOMPARE(frame(), other);
            editor.selectScene(id, point);
            const auto accepted = project.scenePrograms()->program(id).source;
            field(6)->setText("59");
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
            editor.findChild<QPushButton *>("sceneActionDiscard")->click();
            field(7)->setText("999");
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
            editor.findChild<QPushButton *>("sceneActionDiscard")->click();
            kind->setCurrentIndex(kind->findData(SceneActions::OrientSprite));
            auto position = editor.findChild<QComboBox *>("sceneActionPosition");
            position->setCurrentIndex(position->findData(point + 1));
            insert->click();
            field(1)->setText("-1");
            QVERIFY(!editor.property("invalidEdits").toBool());
            QCOMPARE(frame(), first);
            const auto active = project.scenePrograms()->program(id).source;
            field(2)->setText("20");
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, active);
            editor.findChild<QPushButton *>("sceneActionDiscard")->click();
            field(2)->setText("19");
            QVERIFY(!editor.property("invalidEdits").toBool());
            preview.selectScene(*project.scenePrograms(), id, 0);
            preview.findChild<QPushButton *>("sceneStep")->click();
            QVERIFY(preview.property("sceneStopped").toBool());
            QVERIFY(preview.findChild<QLabel *>("scenePlaybackStatus")
                        ->text()
                        .contains("existing sampled scene path"));
            field(2)->setText("0");
            QVERIFY(!editor.property("invalidEdits").toBool());
            QCOMPARE(frame(), first);
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("scene-path-heading-actions.png"));
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void rotatedSpriteActionControls() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto scenes = *project.scenePrograms();
            SceneId source;
            for (const auto &entry : scenes.original().entries())
                if (entry.program.resourceClass() == 5 && scenes.canCreateBlank(entry.key)) {
                    source = entry.key;
                    break;
                }
            QVERIFY(source);
            const auto id = scenes.createScene("Rotated sprite actions", source, true);
            auto base = scenes.program(id);
            const int point = SceneActions::positions(base).first().command;
            base = SceneActions::insert(base, point, 0xe, 30);
            scenes.replace(id, base.source);
            project.setScenePrograms(scenes.json());
            QUndoStack undo;
            SceneActionEditor editor(&project, &undo, [] {});
            editor.resize(1100, 800);
            editor.show();
            editor.selectScene(id, point);
            auto kind = editor.findChild<QComboBox *>("sceneActionKind");
            auto insert = editor.findChild<QPushButton *>("sceneActionInsert");
            kind->setCurrentIndex(kind->findData(0x63));
            QVERIFY(insert->isEnabled());
            insert->click();
            QCOMPARE(project.scenePrograms()->program(id).commands[point].opcode, quint32(0x63));
            QCOMPARE(editor.findChild<QLineEdit *>("sceneActionField6")->text(), QString("180"));
            ScenePreview preview(project.rom());
            preview.show();
            auto frame = [&] {
                preview.selectScene(*project.scenePrograms(), id, 0);
                preview.findChild<QPushButton *>("sceneStep")->click();
                if (preview.property("sceneStopped").toBool())
                    throw Error(preview.findChild<QLabel *>("scenePlaybackStatus")->text());
                return preview.renderedFrame();
            };
            const auto unrotated = frame();
            editor.findChild<QLineEdit *>("sceneActionField6")->setText("270");
            QVERIFY(!editor.property("invalidEdits").toBool());
            const auto rotated = frame();
            QVERIFY(rotated != unrotated);
            undo.undo();
            QCOMPARE(frame(), unrotated);
            undo.redo();
            QCOMPARE(frame(), rotated);
            editor.selectScene(id, point);
            const auto accepted = project.scenePrograms()->program(id).source;
            editor.findChild<QLineEdit *>("sceneActionField2")->setText("800");
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
            editor.findChild<QPushButton *>("sceneActionDiscard")->click();
            editor.findChild<QLineEdit *>("sceneActionField6")->setText("32768");
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
            editor.findChild<QPushButton *>("sceneActionDiscard")->click();
            kind->setCurrentIndex(kind->findData(SceneActions::RotateSprite));
            auto position = editor.findChild<QComboBox *>("sceneActionPosition");
            position->setCurrentIndex(position->findData(point + 1));
            insert->click();
            QCOMPARE(project.scenePrograms()->program(id).commands[point + 1].operands[9],
                     quint32(0));
            QVERIFY(!editor.findChild<QLineEdit *>("sceneActionField2"));
            editor.findChild<QLineEdit *>("sceneActionField1")->setText("90");
            QVERIFY(!editor.property("invalidEdits").toBool());
            const auto reusedFrame = frame();
            QVERIFY(reusedFrame != rotated);
            undo.undo();
            QCOMPARE(frame(), unrotated); // Undo only the reuse angle; default reuse is180.
            undo.redo();
            QCOMPARE(frame(), reusedFrame);
            editor.selectScene(id, point + 1);
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("scene-sprite-rotation.png"));
            // Preserve symbolic position when changing an unrelated direct rotation.
            auto symbolic = *project.scenePrograms();
            auto p = symbolic.program(id);
            p.commands[point].operands[4] = 0x08880004;
            symbolic.replace(id, p.encode());
            pushSceneProgramChange(&project, &undo, "Use launch position", symbolic);
            editor.selectScene(id, point);
            auto x = editor.findChild<QLineEdit *>("sceneActionField4");
            QVERIFY(x->isReadOnly());
            QCOMPARE(x->text(), QString("Launch value 4"));
            editor.findChild<QLineEdit *>("sceneActionField6")->setText("90");
            QVERIFY(!editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).commands[point].operands[4],
                     quint32(0x08880004));
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void sceneSpriteActionControls() {
        try {
            const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
            if (path.isEmpty())
                QSKIP("Private ROM required");
            auto project = Project::fromRom(Rom::open(path));
            project.loadScenePrograms();
            auto scenes = *project.scenePrograms();
            SceneId source;
            for (const auto &entry : scenes.original().entries())
                if (entry.program.resourceClass() == 5 && scenes.canCreateBlank(entry.key)) {
                    source = entry.key;
                    break;
                }
            QVERIFY(source);
            const auto id = scenes.createScene("Sprite actions", source, true);
            auto base = scenes.program(id);
            const int point = SceneActions::positions(base).first().command;
            base = SceneActions::insert(base, point, 0xe, 30);
            scenes.replace(id, base.source);
            project.setScenePrograms(scenes.json());
            QUndoStack undo;
            SceneActionEditor editor(&project, &undo, [] {});
            editor.resize(1100, 800);
            editor.show();
            editor.selectScene(id, point);
            auto kind = editor.findChild<QComboBox *>("sceneActionKind");
            auto insert = editor.findChild<QPushButton *>("sceneActionInsert");
            QVERIFY(kind && insert);
            kind->setCurrentIndex(kind->findData(0x46));
            QVERIFY(insert->isEnabled());
            insert->click();
            QCOMPARE(project.scenePrograms()->program(id).commands[point].opcode, quint32(0x46));
            QCOMPARE(editor.findChild<QLineEdit *>("sceneActionField0")->text(), QString("0"));
            ScenePreview preview(project.rom());
            preview.show();
            auto frame = [&] {
                preview.selectScene(*project.scenePrograms(), id, 0);
                preview.findChild<QPushButton *>("sceneStep")->click();
                if (preview.property("sceneStopped").toBool())
                    throw Error(preview.findChild<QLabel *>("scenePlaybackStatus")->text());
                return preview.renderedFrame();
            };
            const auto first = frame();
            QVERIFY(!first.isNull());
            editor.findChild<QLineEdit *>("sceneActionField4")->setText("48");
            QVERIFY(!editor.property("invalidEdits").toBool());
            QCOMPARE(qint32(project.scenePrograms()->program(id).commands[point].operands[4]), 48);
            const auto moved = frame();
            QVERIFY(moved != first);
            undo.undo();
            QCOMPARE(frame(), first);
            undo.redo();
            QCOMPARE(frame(), moved);
            const auto accepted = project.scenePrograms()->program(id).source;
            editor.findChild<QLineEdit *>("sceneActionField2")->setText("800");
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
            editor.findChild<QPushButton *>("sceneActionDiscard")->click();
            QVERIFY(!editor.property("invalidEdits").toBool());
            QCOMPARE(editor.findChild<QLineEdit *>("sceneActionField2")->text(), QString("1"));
            editor.findChild<QLineEdit *>("sceneActionField4")->setText("-1");
            const auto oneCoordinate = project.scenePrograms()->program(id).source;
            editor.findChild<QLineEdit *>("sceneActionField5")->setText("-1");
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, oneCoordinate);
            editor.findChild<QPushButton *>("sceneActionDiscard")->click();
            // Consecutive field keystrokes intentionally merge within one second.
            editor.findChild<QLineEdit *>("sceneActionField4")->setText("48");
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
            QCoreApplication::processEvents();
            QVERIFY(editor.grab().save("scene-sprite-actions.png"));
            editor.findChild<QPushButton *>("sceneActionRemove")->click();
            QCOMPARE(project.scenePrograms()->program(id).source, base.source);
            undo.undo();
            QCOMPARE(project.scenePrograms()->program(id).source, accepted);
            // The enclosing Cutscene editor normally refreshes the selected command on Undo.
            editor.selectScene(id, point);
            kind->setCurrentIndex(kind->findData(0x64));
            auto position = editor.findChild<QComboBox *>("sceneActionPosition");
            position->setCurrentIndex(position->findData(point + 1));
            QVERIFY(insert->isEnabled());
            insert->click();
            QCOMPARE(project.scenePrograms()->program(id).commands[point + 1].opcode,
                     quint32(0x64));
            const auto inherited = project.scenePrograms()->program(id).source;
            QCOMPARE(editor.findChild<QLineEdit *>("sceneActionField2")->text(), QString("-1"));
            editor.findChild<QLineEdit *>("sceneActionField1")->setText("0");
            QVERIFY(editor.property("invalidEdits").toBool());
            QCOMPARE(project.scenePrograms()->program(id).source, inherited);
            editor.findChild<QPushButton *>("sceneActionDiscard")->click();
            QVERIFY(!editor.property("invalidEdits").toBool());
        } catch (const Error &e) {
            QFAIL(e.what());
        }
    }
    void sceneSpriteComposition() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
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
        preview.seekToUpdate(0);
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
        auto next = editor.findChild<QPushButton *>("actorStep");
        auto loop = editor.findChild<QCheckBox *>("actorLoop");
        auto layers = editor.findChild<QListWidget *>("actorLayers");
        auto view = editor.findChild<QGraphicsView *>("actorCanvas");
        QVERIFY(banks && poses && position && variant && next && loop && layers && view);
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
        position->setValue(0);
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
        auto play=editor.findChild<QPushButton *>("actorPlay");
        auto timer=editor.findChild<QTimer *>("actorPlaybackTimer");QVERIFY(play&&timer);
        position->setValue(0);loop->setChecked(false);play->click();
        for(int tick=0;tick<2000 && play->isChecked();++tick)QMetaObject::invokeMethod(timer,"timeout",Qt::DirectConnection);
        QVERIFY(!play->isChecked());play->click();QCOMPARE(position->value(),0);play->click();
        loop->setChecked(true);play->click();
        for(int tick=0;tick<200;++tick)QMetaObject::invokeMethod(timer,"timeout",Qt::DirectConnection);
        QVERIFY(play->isChecked());play->click();loop->setChecked(false);
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
        const auto key = scenes->currentItem()->data(Qt::UserRole).toUInt();
        const auto baseline = project.scenePrograms()->json();
        const auto selectors = project.scenePrograms()->availableSelectors();
        QTimer::singleShot(0, [] {
            if (auto dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget())) {
                dialog->setTextValue("My replacement scene"); dialog->accept();
            }
        });
        editor.findChild<QPushButton *>("sceneUseTemplate")->click();
        QCOMPARE(scenes->count(), 1498);
        QVERIFY(scenes->currentItem()->text().contains("My replacement scene"));
        QCOMPARE(project.scenePrograms()->displayName(key), "My replacement scene");
        QCOMPARE(project.scenePrograms()->availableSelectors(), selectors);
        QVERIFY(project.scenePrograms()->authoredScenes().isEmpty());
        undo.undo(); QCOMPARE(project.scenePrograms()->json(), baseline);
        QVERIFY(!scenes->currentItem()->text().contains("My replacement scene"));
        undo.redo(); QVERIFY(scenes->currentItem()->text().contains("My replacement scene"));
        undo.undo();
        bool sawDialog = false;
        QTimer::singleShot(0, &editor, [&] {
            if (auto dialog = editor.findChild<QDialog *>("sceneCreateDialog")) {
                auto name = dialog->findChild<QLineEdit *>("newSceneName");
                auto choices = dialog->findChild<QComboBox *>("newSceneTemplate");
                sawDialog = name && choices && choices->count() > 0;
                if (choices)
                    for (int i = 0; i < choices->count(); ++i)
                        if (choices->itemText(i).startsWith("Room Â·")) {
                            choices->setCurrentIndex(i);
                            break;
                        }
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
    void blankEnvironmentCreationControls() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        QUndoStack undo;
        CutsceneEditor editor(project.rom(), nullptr, &project, &undo, [] {});
        editor.resize(1300, 950);
        editor.show();
        auto list = editor.findChild<QListWidget *>("cutsceneScenes");
        QTRY_COMPARE_WITH_TIMEOUT(list->count(), 1498, 10000);
        const SceneId source(0x1fa8bf4 - 0x594284);
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->data(Qt::UserRole).toUInt() == source.nativeKey)
                list->setCurrentRow(i);
        QCOMPARE(list->currentItem()->data(Qt::UserRole).toUInt(), source.nativeKey);
        const auto original = project.scenePrograms()->program(source).source;
        bool selectedEnvironment = false;
        QTimer::singleShot(0, &editor, [&] {
            if (auto dialog = editor.findChild<QDialog *>("sceneCreateDialog")) {
                auto choices = dialog->findChild<QComboBox *>("newSceneTemplate");
                selectedEnvironment = choices &&
                                      choices->currentData().toUInt() == source.nativeKey &&
                                      choices->currentText().startsWith("Environment Â·");
                dialog->findChild<QLineEdit *>("newSceneName")->setText("A new world scene");
                dialog->accept();
            }
        });
        editor.findChild<QPushButton *>("sceneCreate")->click();
        QVERIFY(selectedEnvironment);
        QCOMPARE(project.scenePrograms()->authoredScenes().size(), 1);
        const auto authored = project.scenePrograms()->authoredScenes().first();
        const auto id = SceneId::authored(authored.id);
        QCOMPARE(authored.current.source, original.first(8) + original.last(20));
        QCOMPARE(project.scenePrograms()->program(source).source, original);
        QCOMPARE(list->currentItem()->data(Qt::UserRole + 1).toString(), authored.id);
        QCOMPARE(undo.count(), 1);
        undo.undo();
        QVERIFY(project.scenePrograms()->authoredScenes().isEmpty());
        undo.redo();
        QCOMPARE(project.scenePrograms()->program(id).source, authored.current.source);
        QTemporaryDir work;
        project.saveFolder(work.filePath("project"));
        auto reopened = Project::fromRom(project.rom());
        reopened.applyManifest(Project::readProject(work.filePath("project")));
        QCOMPARE(reopened.scenePrograms()->program(id).source, authored.current.source);
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
        auto loop = preview->findChild<QCheckBox *>("dialogueLoop");
        auto resume = preview->findChild<QPushButton *>("dialogueResume");
        auto canvas = preview->findChild<QLabel *>("dialogueCanvas");
        auto status = preview->findChild<QLabel *>("dialoguePlaybackStatus");
        QVERIFY(step && play && loop && resume && canvas && status);
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
        play->click();
        QCOMPARE(preview->property("updateCount").toULongLong(), qulonglong(1));
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
        auto researchedNameSearch = editor.findChild<QLineEdit *>("cutsceneScenesSearch");QVERIFY(researchedNameSearch);
        researchedNameSearch->setText("Palatinus's four regions");
        int matches = 0;
        for (int i = 0; i < scenes->count(); ++i) if (!scenes->item(i)->isHidden()) {
            QVERIFY(scenes->item(i)->text().contains("610"));++matches;
        }
        QCOMPARE(matches,1);researchedNameSearch->clear();
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
    void scenarioMapSquadPlacement() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadScenarios();
        QUndoStack undo;
        ScenarioEditor editor(&project, &undo, [] {});
        editor.resize(1200, 850);
        editor.show();
        auto missions = editor.findChild<QListWidget *>("scenarioList");
        auto chooseMission = [&](int key) {
            for (int i = 0; i < missions->count(); ++i)
                if (missions->item(i)->data(Qt::UserRole).toInt() == key)
                    missions->setCurrentRow(i);
        };
        chooseMission(1);
        auto add = editor.findChild<QPushButton *>("mapSquadAdd");
        auto treasure = editor.findChild<QPushButton *>("mapTreasureAdd");
        auto canvas = editor.findChild<QWidget *>("scenarioMapCanvas");
        auto tool = editor.findChild<QComboBox *>("mapTool");
        QVERIFY(add && treasure && canvas && tool);
        QTest::qWait(30);
        const auto original = project.scenarios()->json();
        const int row = project.scenarios()->scenario(1).squads.size();
        auto pixel = [&](QPointF position) {
            const double scale = (std::min(canvas->width(), canvas->height()) - 72) / 256.;
            return (QPointF(canvas->width() / 2., canvas->height() / 2.) +
                    (position - QPointF(128, 128)) * scale).toPoint();
        };
        add->click();
        QVERIFY(add->isChecked());
        QCOMPARE(project.scenarios()->json(), original);
        QTest::keyClick(canvas, Qt::Key_Escape);
        QVERIFY(!add->isChecked());
        QCOMPARE(project.scenarios()->json(), original);
        add->click();
        treasure->click();
        QVERIFY(!add->isChecked());
        add->click();
        QVERIFY(!treasure->isChecked());
        tool->setCurrentText("Terrain");
        QVERIFY(!add->isChecked());
        add->click();
        QCOMPARE(tool->currentText(), QString("Select / move"));
        chooseMission(2);
        QVERIFY(!add->isChecked());
        chooseMission(1);
        add->click();
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, pixel({0, 128}));
        QCOMPARE(project.scenarios()->json(), original);
        QVERIFY(add->isChecked());
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, pixel({128, 128}));
        QVERIFY(!add->isChecked());
        QCOMPARE(undo.count(), 1);
        const auto &scenario = project.scenarios()->scenario(1);
        QCOMPARE(scenario.squads.size(), row + 1);
        const auto members = squadMembers(scenario.squads[row]);
        QCOMPARE(members.size(), 1);
        QCOMPARE(members[0].classId, 5); // Retail Knight; no follower slots.
        const auto deployment = scenario.layout.deployments[row];
        QVERIFY(qAbs(int(u8(deployment, 3)) - 128) <= 1);
        QVERIFY(qAbs(int(u8(deployment, 4)) - 128) <= 1);
        QCOMPARE(u8(deployment, 6), 1); // Hold position.
        auto objects = editor.findChild<QListWidget *>("mapObjects");
        QVERIFY(objects->currentItem());
        QCOMPARE(objects->currentItem()->data(Qt::UserRole).toInt(), int(MapObject::Squad));
        QCOMPARE(objects->currentItem()->data(Qt::UserRole + 1).toInt(), row);
        QCOMPARE(editor.findChild<QTabWidget *>("mapSquadTabs")->currentIndex(), 1);
        const auto placed = project.scenarios()->json();
        undo.undo();
        QCOMPARE(project.scenarios()->json(), original);
        undo.redo();
        QCOMPARE(project.scenarios()->json(), placed);
        QTemporaryDir work;
        project.saveFolder(work.filePath("project"));
        auto restored = Project::fromRom(Rom::open(path));
        restored.applyManifest(Project::readProject(work.filePath("project")));
        QCOMPARE(restored.scenarios()->json(), placed);

        // The last allowed squad is placeable; Undo reopens capacity.
        undo.clear();
        auto full = *project.scenarios();
        while (full.scenario(1).squads.size() < 29)
            full.addSquad(1, -1, project.classData());
        project.setScenarios(full.json());
        chooseMission(2);
        chooseMission(1);
        QVERIFY(add->isEnabled());
        add->click();
        QTest::qWait(20);
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, pixel({128, 128}));
        QCOMPARE(project.scenarios()->scenario(1).squads.size(), 30);
        QVERIFY(!add->isEnabled());
        const auto capped = project.scenarios()->json();
        add->click();
        QCOMPARE(project.scenarios()->json(), capped);
        undo.undo();
        QCOMPARE(project.scenarios()->json(), full.json());
        QVERIFY(add->isEnabled());
        undo.redo();
        QCOMPARE(project.scenarios()->json(), capped);
        QVERIFY(!add->isEnabled());
    }
    void scenarioMapLabelSelection() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadScenarios();
        auto scenarios = *project.scenarios();
        const int row = scenarios.addNode(1, 1, project.classData());
        auto node = scenarios.scenario(1).layout.nodes[row];
        put8(node, 4, 128); put8(node, 5, 10);
        scenarios.setNode(1, row, node, project.classData());
        project.setScenarios(scenarios.json());
        QUndoStack undo;
        ScenarioEditor editor(&project, &undo, [] {});
        editor.resize(1450, 950); editor.show();
        auto canvas = editor.findChild<QWidget *>("scenarioMapCanvas");
        auto objects = editor.findChild<QListWidget *>("mapObjects");
        const auto before = project.manifest();
        const auto target = [&] {
            const double side = std::min(canvas->width(), canvas->height()) - 72;
            return QPointF(canvas->width() / 2., canvas->height() / 2.) +
                   QPointF(0, -118) * side / 256. + QPointF(30, 0);
        };
        QVERIFY(selectMapObject(editor, MapObject::Squad, 0));
        QTest::qWait(50);
        canvas->repaint();
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, target().toPoint());
        QVERIFY(objects->currentItem());
        QCOMPARE(objects->currentItem()->data(Qt::UserRole).toInt(), int(MapObject::Waypoint));
        QCOMPARE(objects->currentItem()->data(Qt::UserRole + 1).toInt(), row);
        QCOMPARE(project.manifest(), before);
        canvas->repaint();
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, target().toPoint());
        QTest::mouseMove(canvas, target().toPoint() + QPoint(30, 15), 20);
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, target().toPoint() + QPoint(30, 15));
        QCOMPARE(project.manifest(), before); // Labels select; only marker drags move.
        QVERIFY(selectMapObject(editor, MapObject::Squad, 0));
        auto orders = editor.findChild<QWidget *>("mapSquadOrders");
        auto command = orders->findChild<QComboBox *>("squadCommand");
        command->setCurrentIndex(command->findData(int(SquadBehavior::Move)));
        orders->findChild<QPushButton *>("squadCommandPick")->click();
        canvas->repaint();
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, target().toPoint());
        QCOMPARE(objects->currentItem()->data(Qt::UserRole).toInt(), int(MapObject::Squad));
        QVERIFY(project.scenarios()->squadCommand(1,0));undo.undo();QCOMPARE(project.manifest(),before);
        QVERIFY(selectMapObject(editor, MapObject::Waypoint, row));
        canvas->repaint();
        QVERIFY(editor.grab().save("map-label-selection.png"));
    }
    void treasureAutoApply() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path)); project.loadScenarios();
        QUndoStack undo;
        ScenarioEditor editor(&project, &undo, [] {});
        editor.resize(1450, 950); editor.show();
        QVERIFY(selectMapObject(editor, MapObject::Treasure, 0));
        QTest::qWait(30);
        const auto before = project.manifest();
        const auto original = ScenarioWorld::treasureRecord(project.world()->scenario(1).treasures, 0);
        auto record = [&] { return ScenarioWorld::treasureRecord(project.world()->scenario(1).treasures, 0); };
        auto table = editor.findChild<QComboBox *>("worldTreasureTable");
        auto item = editor.findChild<QComboBox *>("worldTreasureItem");
        table->setCurrentIndex(table->findData(1));
        item->setCurrentIndex(item->findData(277));
        QCOMPARE(u16(record(), 4), 277);
        const auto equipment = record();
        table->setCurrentIndex(table->findData(2));
        QCOMPARE(u8(record(), 3), 2); QCOMPARE(u16(record(), 4), 1);
        undo.undo(); QCOMPARE(record(), equipment);
        undo.redo();
        item->setCurrentIndex(item->findData(44));
        QCOMPARE(u16(record(), 4), 44);
        auto x = editor.findChild<QSpinBox *>("worldTreasureX");
        auto z = editor.findChild<QSpinBox *>("worldTreasureZ");
        x->setFocus(); x->selectAll(); QTest::keyClicks(x, "123");
        QCOMPARE(u8(record(), 1), u8(original, 1));
        z->setFocus(); QCOMPARE(u8(record(), 1), 123);
        z->setValue(234); QCOMPARE(u8(record(), 2), 234);
        QCOMPARE(u8(record(), 0), u8(original, 0));
        const auto after = project.manifest(); const int count = undo.count();
        QVERIFY(selectMapObject(editor, MapObject::Site, 0));
        QVERIFY(selectMapObject(editor, MapObject::Treasure, 0));
        QCOMPARE(project.manifest(), after); QCOMPARE(undo.count(), count);
        QVERIFY(!editor.findChild<QPushButton *>("worldTreasureApply"));
        undo.setIndex(0); QCOMPARE(project.manifest(), before);
        undo.setIndex(undo.count()); QCOMPARE(project.manifest(), after);
    }
    void scenarioSiteAutoApply() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadScenarios();
        QUndoStack undo;
        ScenarioEditor editor(&project, &undo, [] {});
        editor.resize(1450, 950); editor.show();
        int row = -1, id = -1;
        for (int i = 0; i < project.world()->siteCount(1); ++i) {
            const int candidate = u16(project.world()->scenario(1).sites, 1 + i * 4);
            const auto town = project.world()->town(candidate);
            if (u8(town, 24) != 255 && !u8(town, 27) && !(u8(town, 25) & 3)) {
                row = i; id = candidate; break;
            }
        }
        QVERIFY(row >= 0);
        QVERIFY(selectMapObject(editor, MapObject::Site, row));
        QTest::qWait(50);
        const auto before = project.manifest();
        const auto original = project.world()->town(id);
        const int count = project.shops()->size();
        auto shop = editor.findChild<QCheckBox *>("worldFacility0");
        auto den = editor.findChild<QCheckBox *>("worldFacility1");
        auto inventory = editor.findChild<QSpinBox *>("worldShop");
        // Undo a new shop, reuse its former ID elsewhere, then enable this
        // site's shop again. The unrelated inventory must not be attached.
        shop->click();
        QCOMPARE(u8(project.world()->town(id), 27), count);
        undo.undo();
        QCOMPARE(addShop(&project, &undo), count);
        shop->click();
        QCOMPARE(project.shops()->size(), count + 2);
        QCOMPARE(u8(project.world()->town(id), 27), count + 1);
        undo.setIndex(0); QCOMPARE(project.manifest(), before);
        undo.clear();
        shop->click();
        QCOMPARE(project.shops()->size(), count + 1);
        QCOMPARE(u8(project.world()->town(id), 27), count);
        QCOMPARE(undo.count(), 1); // Stock creation and assignment undo together.
        undo.undo(); QCOMPARE(project.manifest(), before);
        undo.redo();
        shop->click();
        QCOMPARE(u8(project.world()->town(id), 27), 0);
        shop->click();
        QCOMPARE(u8(project.world()->town(id), 27), count);
        QCOMPARE(project.shops()->size(), count + 1); // Reuse the site's inventory.
        den->click();
        QCOMPARE(u8(project.world()->town(id), 25) & 3, 2);
        QCOMPARE(inventory->value(), 0);
        shop->click();
        QCOMPARE(u8(project.world()->town(id), 25) & 3, 1);
        QCOMPARE(inventory->value(), count);
        QVERIFY(!den->isChecked());
        undo.undo(); QCOMPARE(u8(project.world()->town(id), 25) & 3, 2);
        undo.redo();
        inventory->setValue(0); QVERIFY(!shop->isChecked());
        inventory->setValue(1); QVERIFY(shop->isChecked());
        QCOMPARE(u8(project.world()->town(id), 27), 1);
        QCOMPARE(u8(project.world()->town(id), 25) & 0xfc, u8(original, 25) & 0xfc);
        auto name = editor.findChild<QLineEdit *>("worldSiteName");
        name->setFocus(); name->selectAll();
        QTest::keyClicks(name, "Field Town");
        QCOMPARE(project.world()->townName(id), QString("Field Town"));
        name->setCursorPosition(5);
        QTest::keyClicks(name, "X");
        QCOMPARE(project.world()->townName(id), QString("FieldX Town"));
        QCOMPARE(name->cursorPosition(), 6);
        auto population = editor.findChild<QSpinBox *>("worldPopulation");
        const int oldPopulation = u16(project.world()->town(id), 22);
        population->setFocus(); population->selectAll();
        QTest::keyClicks(population, "1234");
        QCOMPARE(u16(project.world()->town(id), 22), oldPopulation);
        QTest::keyClick(population, Qt::Key_Return);
        QCOMPARE(u16(project.world()->town(id), 22), 1234);
        auto morale = editor.findChild<QSpinBox *>("worldMorale");
        morale->setValue(62);
        QCOMPARE(u8(project.world()->town(id), 24), 62);
        const auto valid = project.world()->town(id);
        const int edits = undo.index();
        QCOMPARE(morale->maximum(), 127);
        morale->setValue(127);
        QCOMPARE(u8(project.world()->town(id), 24), 127);
        undo.undo();
        QCOMPARE(project.world()->town(id), valid);
        QCOMPARE(undo.index(), edits);
        QCOMPARE(morale->value(), 62);
        auto type = editor.findChild<QComboBox *>("worldSiteType");
        type->setCurrentIndex(type->findData(0x29));
        QCOMPARE(u8(project.world()->town(id), 26), 0x29);
        auto allegiance = editor.findChild<QComboBox *>("worldAllegiance");
        allegiance->setCurrentIndex(allegiance->findData(1));
        QCOMPARE(u16(project.world()->scenario(1).sites, 3 + row * 4), quint16(0x2012));
        // A real selection click commits typed numbers to the old site before
        // loading the new site's controls, without requiring Enter.
        population->setFocus(); population->selectAll();
        QTest::keyClicks(population, "2345");
        auto objects = editor.findChild<QListWidget *>("mapObjects");
        const int otherRow = (row + 1) % project.world()->siteCount(1);
        const int otherId = u16(project.world()->scenario(1).sites, 1 + otherRow * 4);
        const auto otherTown = project.world()->town(otherId);
        for (int i = 0; i < objects->count(); ++i) {
            auto item = objects->item(i);
            if (item->data(Qt::UserRole).toInt() != int(MapObject::Site) ||
                item->data(Qt::UserRole + 1).toInt() != otherRow) continue;
            objects->scrollToItem(item);
            QTest::mouseClick(objects->viewport(), Qt::LeftButton, Qt::NoModifier,
                              objects->visualItemRect(item).center());
            break;
        }
        QCOMPARE(u16(project.world()->town(id), 22), 2345);
        QCOMPARE(project.world()->town(otherId), otherTown);
        const auto after = project.manifest();
        const int undoCount = undo.count();
        QVERIFY(selectMapObject(editor, MapObject::Site, (row + 1) % project.world()->siteCount(1)));
        QVERIFY(selectMapObject(editor, MapObject::Site, row));
        QCOMPARE(name->text(), QString("FieldX Town"));
        QCOMPARE(population->value(), 2345);
        QCOMPARE(project.manifest(), after);
        QCOMPARE(undo.count(), undoCount);
        QVERIFY(!editor.findChild<QPushButton *>("worldSiteApply"));
        QVERIFY(!editor.findChild<QPushButton *>("worldShopAdd"));
        QVERIFY(!editor.findChild<QPushButton *>("worldShopRemove"));
        QVERIFY(editor.grab().save("site-auto-apply.png"));
        undo.setIndex(0); QCOMPARE(project.manifest(), before);
        undo.setIndex(undo.count()); QCOMPARE(project.manifest(), after);
    }
    void scenarioFacilityExport() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"), tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty()) QSKIP("Private ROM and compiler required");
        const auto rom = Rom::open(path);
        auto project = Project::fromRom(rom);
        project.loadScenarios();
        QUndoStack undo;
        ScenarioEditor editor(&project, &undo, [] {});
        editor.resize(1450, 950); editor.show();
        auto missions = editor.findChild<QListWidget *>("scenarioList");
        for (int i = 0; i < missions->count(); ++i)
            if (missions->item(i)->data(Qt::UserRole).toInt() == 5) missions->setCurrentRow(i);
        const auto &world = *project.world();
        int row = -1, id = -1;
        for (int i = 0; i < world.siteCount(5); ++i) {
            const int candidate = u16(world.scenario(5).sites, 1 + i * 4);
            if (world.townName(candidate).startsWith("Quell")) { row = i; id = candidate; break; }
        }
        QVERIFY(row >= 0);
        QVERIFY(selectMapObject(editor, MapObject::Site, row));
        const auto before = project.manifest();
        editor.findChild<QCheckBox *>("worldFacility0")->setChecked(true);
        const int shop = u8(project.world()->town(id), 27);
        QVERIFY(shop > 0);
        QCOMPARE(project.shops()->stock(shop).consumables, QVector<quint16>({1, 2, 3, 4, 5, 6, 8}));
        editor.findChild<QCheckBox *>("worldFacility1")->setChecked(true);
        QVERIFY(!editor.findChild<QCheckBox *>("worldFacility0")->isChecked());
        QCOMPARE(u8(project.world()->town(id), 25) & 3, 2);
        const int shopRow = row == 0 ? 1 : 0;
        const int shopTown = u16(project.world()->scenario(5).sites, 1 + shopRow * 4);
        QVERIFY(selectMapObject(editor, MapObject::Site, shopRow));
        editor.findChild<QCheckBox *>("worldObjective")->setChecked(false);
        editor.findChild<QSpinBox *>("worldShop")->setValue(shop);
        QCOMPARE(u8(project.world()->town(shopTown), 25) & 3, 1);
        const auto after = project.manifest();
        while (undo.canUndo()) undo.undo();
        QCOMPARE(project.manifest(), before);
        while (undo.canRedo()) undo.redo();
        QCOMPARE(project.manifest(), after);
        QTemporaryDir work;
        project.saveFolder(work.filePath("project"));
        auto restored = Project::fromRom(rom);
        restored.applyManifest(Project::readProject(work.filePath("project")));
        QCOMPARE(restored.world()->town(id), project.world()->town(id));
        const auto result = rebuild(restored, tools, work.path());
        const auto root = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
        QVERIFY(QDir().mkpath(root));
        QTemporaryDir output(QDir(root).filePath("native-site-facilities-XXXXXX"));
        QVERIFY(output.isValid());
        const auto candidate = output.filePath(QFileInfo(path).fileName());
        exportRom(restored, result, candidate);
        const auto reopenedRom = Rom::open(candidate);
        const auto reopened = ScenarioWorld::load(reopenedRom);
        QCOMPARE(reopened.town(id), project.world()->town(id));
        QCOMPARE(u8(reopened.town(id), 25) & 3, 2);
        QCOMPARE(u8(reopened.town(shopTown), 25) & 3, 1);
        QCOMPARE(u8(reopened.town(id), 27), 0);
        QCOMPARE(u8(reopened.town(shopTown), 27), shop);
        QCOMPARE(Shops::load(reopenedRom).stock(shop), restored.shops()->stock(shop));
        auto sealed = reopenedRom.bytes();
        Rom::updateChecksum(sealed);
        QCOMPARE(sealed.mid(0x10, 8), reopenedRom.bytes().mid(0x10, 8));
        auto both = reopened.town(id);
        put8(both, 25, quint8(u8(both, 25) | 3));
        const auto example = [&](QString name, QByteArray town, int point, QJsonArray menu) {
            return QJsonObject{{"name", name}, {"town", QString::fromLatin1(town.toHex())},
                {"addend", u16(reopened.scenario(5).sites, 3 + point * 4)}, {"menu", menu}};
        };
        const auto fixture = work.filePath("facilities.json");
        QFile f(fixture); QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(QJsonObject{{"rom", candidate}, {"cases", QJsonArray{
            example("Shop", reopened.town(shopTown), shopRow, {108}),
            example("Witch's den", reopened.town(id), row, {144}),
            example("Loaded conflicting flags", both, row, {108})}}}).toJson());
        f.close();
        QProcess probe;
        probe.start("node", {QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("site-facilities-mips.cjs"), fixture});
        QVERIFY(probe.waitForFinished(30000));
        const auto errors = probe.readAllStandardError();
        QVERIFY2(probe.exitCode() == 0, errors.constData());
        qInfo().noquote() << probe.readAllStandardOutput();
        output.setAutoRemove(false);
        qInfo().noquote() << "Facility test ROM:" << candidate;
    }
    void scenarioTerrainRoute() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        applyEditorTheme(*qApp);
        auto project = Project::fromRom(Rom::open(path));
        project.loadScenarios(); project.loadMapEdits();
        auto s = *project.scenarios(); const auto classes = project.classData();
        auto d = s.scenario(5).layout.deployments[1]; put8(d, 3, 50); put8(d, 4, 100);
        s.setDeployment(5, 1, d, classes); s.setRouteGroup(5, 1, 0, 0, classes);
        auto members = squadMembers(s.scenario(5).squads[1]);
        for (auto &m : members) m.classId = 2;
        s.setSquad(5, 1, editSquadMembers(s.scenario(5).squads[1], members), classes);
        s.setSquadCommand(5, 1, {SquadBehavior::Move, 170, 100}, classes);
        project.setScenarios(s.json());
        auto maps = *project.mapEdits();
        maps.setBoundary(5, QRectF(QPointF(8,8), QPointF(248,248)));
        maps.paint(5, {{40,100},{180,100}}, 3, 25);
        maps.paint(5, {{110,72},{110,128}}, 8, 5);
        project.setMapEdits(maps.json());
        QUndoStack undo;
        ScenarioMapEditor editor(&project, &undo, [](const Scenarios &, const QString &) {}, [](MapObject, int) {});
        editor.resize(1250, 850); editor.show(); editor.refresh(5);
        QVERIFY(selectMapObject(editor, MapObject::Squad, 1));
        auto route = editor.findChild<QLabel *>("mapTerrainRoute");
        QVERIFY(route && route->isVisible());
        QVERIFY2(route->text().contains("Terrain route shown"), qPrintable(route->text()));
        QTest::qWait(50); QVERIFY(editor.grab().save("terrain-route-preview.png"));
        // Changed orders and switching selection must refresh and clear the overlay.
        s.setSquadCommand(5, 1, {SquadBehavior::Guard}, classes);
        project.setScenarios(s.json()); editor.refresh(5);
        QVERIFY(route->text().contains("No fixed destination"));
        s.setSquadCommand(5, 1, {SquadBehavior::Move, 170, 100}, classes);
        project.setScenarios(s.json()); editor.refresh(5);
        QVERIFY(route->text().contains("Terrain route shown"));
        QVERIFY(selectMapObject(editor, MapObject::Site, 0)); QVERIFY(!route->isVisible());
    }
    void scenarioMapOrders() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadScenarios();
        QUndoStack undo;
        ScenarioEditor editor(&project, &undo, [] {});
        editor.resize(1450, 950); editor.show();
        auto pages = editor.findChild<QStackedWidget *>("scenarioPages");
        pages->setCurrentIndex(0);
        QVERIFY(selectMapObject(editor, MapObject::Squad, 0));
        QTest::qWait(50);
        auto orders = editor.findChild<QWidget *>("mapSquadOrders");
        QVERIFY(orders && orders->isVisible());
        auto command = orders->findChild<QComboBox *>("squadCommand");
        auto pick = orders->findChild<QPushButton *>("squadCommandPick");
        auto apply = orders->findChild<QPushButton *>("squadCommandApply");
        auto objects = editor.findChild<QListWidget *>("mapObjects");
        auto canvas = editor.findChild<QWidget *>("scenarioMapCanvas");
        const auto before = project.scenarios()->json();
        const auto deployment = project.scenarios()->scenario(1).layout.deployments[0];
        const int row = objects->currentRow();
        command->setCurrentIndex(command->findData(int(SquadBehavior::MoveAndAttack)));
        pick->click(); QVERIFY(pick->isChecked());
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, canvas->rect().center());
        QVERIFY(!pick->isChecked());
        QCOMPARE(objects->currentRow(), row);
        QCOMPARE(orders->findChild<QSpinBox *>("squadCommandX")->value(), 128);
        QCOMPARE(orders->findChild<QSpinBox *>("squadCommandZ")->value(), 128);
        if(apply->isVisible()) {QCOMPARE(project.scenarios()->json(),before);apply->click();}
        const auto result = project.scenarios()->squadCommand(1, 0);
        QVERIFY(result);
        QCOMPARE(result->behavior, SquadBehavior::MoveAndAttack);
        QCOMPARE(result->x, 128); QCOMPARE(result->z, 128);
        QCOMPARE(project.scenarios()->scenario(1).layout.deployments[0].mid(3, 2), deployment.mid(3, 2));
        undo.undo(); QCOMPARE(project.scenarios()->json(), before);
        undo.redo();
        const auto applied = project.scenarios()->json();
        pick->click(); QVERIFY(pick->isChecked());
        QTest::keyClick(canvas, Qt::Key_Escape);
        QVERIFY(!pick->isChecked()); QCOMPARE(project.scenarios()->json(), applied);
        pick->click();
        editor.findChild<QComboBox *>("mapTool")->setCurrentIndex(1);
        QVERIFY(!pick->isChecked()); QCOMPARE(project.scenarios()->json(), applied);
        editor.findChild<QComboBox *>("mapTool")->setCurrentIndex(0);
        pick->click();
        QVERIFY(selectMapObject(editor, MapObject::Site, 0));
        QVERIFY(!pick->isChecked()); QVERIFY(!orders->isVisible());
        QVERIFY(selectMapObject(editor, MapObject::Squad, 1));
        command->setCurrentIndex(command->findData(int(SquadBehavior::Guard)));
        apply->click();
        QCOMPARE(project.scenarios()->squadCommand(1, 0)->behavior, SquadBehavior::MoveAndAttack);
        const auto guard=project.scenarios()->squadCommand(1,1);
        QVERIFY((guard && guard->behavior==SquadBehavior::Guard) || u8(project.scenarios()->scenario(1).layout.deployments[1],6)==1);
        QVERIFY(selectMapObject(editor, MapObject::Squad, 0));
        QVERIFY(editor.grab().save("scenario-map-orders.png"));
    }
    void scenarioRouteSteps() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM"),tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if(path.isEmpty() || tools.isEmpty())QSKIP("Private ROM and compiler required");
        try {
            applyEditorTheme(*qApp);
            auto project=Project::fromRom(Rom::open(path));project.loadScenarios();
            const auto classes=project.classData();auto model=*project.scenarios();
            const int key=1;
            const int first=model.addNode(key,1,classes),second=model.addNode(key,1,classes),third=model.addNode(key,0,classes);
            const int ca=model.addCondition(key,9,classes)+1,cb=model.addCondition(key,9,classes)+1;
            for(int row:{first,second,third}) {
                auto n=model.scenario(key).layout.nodes[row];
                put8(n,4,quint8(row==first?64:160));put8(n,5,quint8(row==first?80:128));
                put8(n,17,quint8(row==first?second+4:row==second?third+4:0));
                model.setNode(key,row,n,classes);
            }
            auto deployment=model.scenario(key).layout.deployments[0];put8(deployment,6,quint8(first+4));
            model.setDeployment(key,0,deployment,classes);model.setRouteGroup(key,1,0,0,classes);model.setRouteGroup(key,2,0,0,classes);
            project.setScenarios(model.json());const auto original=model.json();
            QUndoStack undo;ScenarioEditor editor(&project,&undo,[]{});editor.resize(1100,800);editor.show();
            auto scenarios=editor.findChild<QListWidget *>("scenarioList");
            for(int i=0;i<scenarios->count();++i)if(scenarios->item(i)->data(Qt::UserRole).toInt()==key)scenarios->setCurrentRow(i);
            QVERIFY(selectMapObject(editor,MapObject::Squad,0));
            auto steps=static_cast<SquadRouteSteps *>(editor.findChild<QWidget *>("squadRouteSteps"));QVERIFY(steps && steps->isVisible());
            auto list=steps->findChild<QListWidget *>("squadSteps");QCOMPARE(list->count(),3);
            QCOMPARE(steps->route().rows,QVector<int>({first,second,third}));
            QVERIFY(list->item(0)->text().contains("Next step: Step 2"));
            auto activate=[&](const char *name,int value) {
                auto box=steps->findChild<QComboBox *>(name);QVERIFY(box);const int index=box->findData(value);QVERIFY(index>=0);
                box->setCurrentIndex(index);QVERIFY(QMetaObject::invokeMethod(box,"activated",Q_ARG(int,index)));
            };
            activate("squadStepAction",int(SquadBehavior::MoveAndAttack));
            QCOMPARE(u8(project.scenarios()->scenario(key).layout.nodes[first],3),1);
            QCOMPARE(project.scenarios()->scenario(key).layout.nodes[first].mid(10),model.scenario(key).layout.nodes[first].mid(10));
            undo.undo();QCOMPARE(project.scenarios()->json(),original);undo.redo();
            // Clicking a numbered marker selects its step without moving or deselecting the squad.
            auto canvas=editor.findChild<QWidget *>("scenarioMapCanvas");QTest::qWait(60);canvas->repaint();
            QVERIFY(editor.grab().save("native/build/route-steps-sidebar.png"));
            const auto beforeClick=project.scenarios()->json();
            const double scale=(std::min(canvas->width(),canvas->height())-72)/256.;
            const QPoint badge=(QPointF(canvas->width()/2.,canvas->height()/2.)+QPointF(32,0)*scale+QPointF(21,-20)).toPoint();
            QTest::mouseClick(canvas,Qt::LeftButton,Qt::NoModifier,badge);
            QCOMPARE(steps->selectedNode(),second);QCOMPARE(project.scenarios()->json(),beforeClick);
            auto objects=editor.findChild<QListWidget *>("mapObjects");QCOMPARE(objects->currentItem()->data(Qt::UserRole).toInt(),int(MapObject::Squad));
            const auto start=project.scenarios()->scenario(key).layout.deployments[0];
            auto pick=steps->findChild<QPushButton *>("squadStepPick");pick->click();QVERIFY(pick->isChecked());
            const auto click=canvas->rect().center();
            const qreal clickScale=(std::min(canvas->width(),canvas->height())-72)/256.;
            const int expectedX=qRound(128+(click.x()-canvas->width()/2.)/clickScale);
            const int expectedZ=qRound(128+(click.y()-canvas->height()/2.)/clickScale);
            QTest::mouseClick(canvas,Qt::LeftButton,Qt::NoModifier,click);
            QCOMPARE(u8(project.scenarios()->scenario(key).layout.nodes[second],4),expectedX);
            QCOMPARE(u8(project.scenarios()->scenario(key).layout.nodes[second],5),expectedZ);
            QCOMPARE(project.scenarios()->scenario(key).layout.deployments[0],start);
            QVERIFY(!pick->isChecked());QCOMPARE(steps->selectedNode(),second);
            pick->click();QTest::keyClick(canvas,Qt::Key_Escape);QVERIFY(!pick->isChecked());
            pick->click();editor.findChild<QComboBox *>("mapTool")->setCurrentIndex(1);QVERIFY(!pick->isChecked());
            editor.findChild<QComboBox *>("mapTool")->setCurrentIndex(0);
            QVERIFY(selectMapObject(editor,MapObject::Site,0));QVERIFY(selectMapObject(editor,MapObject::Squad,0));QCOMPARE(steps->selectedNode(),second);
            // Adding a step inserts it and retains the existing follow-up; undo restores exact records.
            const auto beforeAdd=project.scenarios()->json();steps->findChild<QPushButton *>("squadStepAdd")->click();
            QCOMPARE(list->count(),4);const int added=steps->selectedNode();
            QCOMPARE(u8(project.scenarios()->scenario(key).layout.nodes[second],17),added+4);
            QCOMPARE(u8(project.scenarios()->scenario(key).layout.nodes[added],17),third+4);
            undo.undo();QCOMPARE(project.scenarios()->json(),beforeAdd);
            steps->selectNode(third);activate("squadStepNext",first+4);QCOMPARE(list->count(),3); // loop, no infinite traversal
            steps->selectNode(first);activate("squadStepWhen",4);activate("squadStepConditionA",ca);activate("squadStepConditionB",cb);
            activate("squadStepFork",second+4);activate("squadStepNext",third+4);
            const auto branch=project.scenarios()->json();
            QVERIFY(list->item(0)->text().contains("A â†’ Step 2; B â†’ Step 3"));
            QCOMPARE(u8(project.scenarios()->scenario(key).layout.nodes[first],11),1);
            const auto nodesBefore=project.scenarios()->scenario(key).layout.nodes;
            // Other squads sharing this graph cannot be changed silently.
            auto shared=*project.scenarios();auto other=shared.scenario(key).layout.deployments[2];put8(other,6,quint8(first+4));shared.setDeployment(key,2,other,classes);
            project.setScenarios(shared.json());scenarios->currentRowChanged(scenarios->currentRow());QVERIFY(selectMapObject(editor,MapObject::Squad,0));steps->selectNode(first);
            QVERIFY(!steps->findChild<QComboBox *>("squadStepAction")->isEnabled());
            auto consent=steps->findChild<QCheckBox *>("squadStepShared");QVERIFY(consent->isVisible());consent->setChecked(true);
            QVERIFY(steps->findChild<QComboBox *>("squadStepAction")->isEnabled());
            project.setScenarios(branch);scenarios->currentRowChanged(scenarios->currentRow());
            // Extended original payloads and all hidden destination pairs survive ordinary edits.
            auto extended=*project.scenarios();auto n=extended.scenario(key).layout.nodes[first];put8(n,13,2);put8(n,14,quint8(ca));put8(n,6,90);put8(n,7,100);
            extended.setNode(key,first,n,classes);project.setScenarios(extended.json());scenarios->currentRowChanged(scenarios->currentRow());steps->selectNode(first);
            QVERIFY(!steps->findChild<QComboBox *>("squadStepWhen")->isEnabled());QVERIFY(!steps->findChild<QComboBox *>("squadStepNext")->isEnabled());
            activate("squadStepAction",int(SquadBehavior::Evade));
            QCOMPARE(project.scenarios()->scenario(key).layout.nodes[first].mid(6),n.mid(6));
            project.setScenarios(branch);undo.clear();scenarios->currentRowChanged(scenarios->currentRow());steps->selectNode(first);
            // A group follower sees its leader's route, but edits stay with the leader.
            auto grouped=*project.scenarios();grouped.setRouteGroup(key,1,7,5,classes);grouped.setRouteGroup(key,2,7,5,classes);
            auto leader=grouped.scenario(key).layout.deployments[1];put8(leader,6,quint8(first+4));grouped.setDeployment(key,1,leader,classes);
            project.setScenarios(grouped.json());scenarios->currentRowChanged(scenarios->currentRow());QVERIFY(selectMapObject(editor,MapObject::Squad,2));
            QCOMPARE(steps->route().leader,1);QVERIFY(!steps->findChild<QComboBox *>("squadStepAction")->isEnabled());
            steps->findChild<QPushButton *>("squadStepLeader")->click();QCOMPARE(objects->currentItem()->data(Qt::UserRole+1).toInt(),1);
            project.setScenarios(branch);scenarios->currentRowChanged(scenarios->currentRow());QVERIFY(selectMapObject(editor,MapObject::Squad,0));steps->selectNode(first);
            auto scroll=editor.findChild<QScrollArea *>("mapInspectorScroll");scroll->verticalScrollBar()->setValue(0);QTest::qWait(50);
            QCOMPARE(editor.size(),QSize(1100,800));
            for(auto box:steps->findChildren<QComboBox *>())if(box->isVisible())QVERIFY(box->height()>=box->sizeHint().height());
            QVERIFY(editor.grab().save("native/build/route-steps-sidebar.png"));
            scroll->ensureWidgetVisible(steps->findChild<QComboBox *>("squadStepNext"));QTest::qWait(40);QVERIFY(editor.grab().save("native/build/route-steps-details.png"));
            QTemporaryDir folder,work;project.saveFolder(folder.path());
            auto loaded=Project::fromRom(Rom::open(path));loaded.applyManifest(Project::readProject(folder.path()));QCOMPARE(loaded.scenarios()->json(),branch);
            const auto result=rebuild(project,tools,work.path());
            QTemporaryDir output(QFileInfo(path).absolutePath()+"/Test ROMs/native-route-steps-XXXXXX");QVERIFY(output.isValid());
            const auto candidate=output.filePath(QFileInfo(path).fileName());exportRom(project,result,candidate);
            auto reopened=Project::fromRom(Rom::open(candidate));reopened.loadScenarios();QCOMPARE(reopened.scenarios()->json(),branch);
            auto checksum=reopened.rom().bytes();Rom::updateChecksum(checksum);QCOMPARE(checksum,reopened.rom().bytes());
            const auto &exported=reopened.scenarios()->scenario(key).layout;
            QJsonArray nativeNodes,nativeConditions;for(const auto &r:exported.nodes)nativeNodes<<QString::fromLatin1(r.toHex());for(const auto &r:exported.conditions)nativeConditions<<QString::fromLatin1(r.toHex());
            QFile fixture(output.filePath("route-steps.json"));QVERIFY(fixture.open(QIODevice::WriteOnly));
            fixture.write(QJsonDocument(QJsonObject{{"rom",candidate},{"nodes",nativeNodes},{"conditions",nativeConditions},{"first",first+4},{"second",second+4},{"third",third+4},{"conditionA",ca},{"conditionB",cb}}).toJson());fixture.close();
            QProcess process;process.start("node",{QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("route-steps-mips.cjs"),fixture.fileName()});
            QVERIFY(process.waitForFinished(30000));const auto evidence=process.readAllStandardOutput()+process.readAllStandardError();QVERIFY2(process.exitCode()==0,evidence.constData());qInfo().noquote()<<evidence;
            output.setAutoRemove(false);qInfo().noquote()<<"Route steps ROM:"<<candidate;
            // A native hold order can become an editable route, with exact Undo back to its sentinel.
            model.restore(key);project.setScenarios(model.json());undo.clear();scenarios->currentRowChanged(scenarios->currentRow());
            QVERIFY(selectMapObject(editor,MapObject::Squad,0));
            auto create=editor.findChild<QPushButton *>("mapCreateRoute");QVERIFY(create->isVisible());
            const auto hold=project.scenarios()->json();create->click();QCOMPARE(list->count(),1);
            steps->findChild<QPushButton *>("squadStepAdd")->click();QCOMPARE(list->count(),2);
            undo.undo();undo.undo();QCOMPARE(project.scenarios()->json(),hold);
        } catch(const Error &e){QFAIL(e.what());}
    }
    void scenarioSidebarTriggers() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM"),tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if(path.isEmpty() || tools.isEmpty())QSKIP("Private ROM and compiler required");
        try {
            applyEditorTheme(*qApp);
            auto project=Project::fromRom(Rom::open(path));project.loadScenarios();
            const auto classes=project.classData();auto model=*project.scenarios();const int key=1;
            const int first=model.addNode(key,1,classes),other=model.addNode(key,0,classes);
            auto n=model.scenario(key).layout.nodes[first];put8(n,4,100);put8(n,5,120);put8(n,17,quint8(other+4));model.setNode(key,first,n,classes);
            auto deployment=model.scenario(key).layout.deployments[0];put8(deployment,6,quint8(first+4));model.setDeployment(key,0,deployment,classes);
            model.setRouteGroup(key,1,0,0,classes);model.setRouteGroup(key,2,0,0,classes);project.setScenarios(model.json());
            // A mission with no existing conditions can author its first trigger here.
            auto emptyProject=Project::fromRom(project.rom());emptyProject.loadScenarios();auto empty=*emptyProject.scenarios();
            QCOMPARE(empty.scenario(10).layout.conditions.size(),0);
            empty.setSquadCommand(10,0,{SquadBehavior::Guard},classes);emptyProject.setScenarios(empty.json());
            SquadRouteSteps emptySteps(&emptyProject,[&](const Scenarios &s,const QString &){emptyProject.setScenarios(s.json());});emptySteps.refresh(10,0);
            auto emptyWhen=emptySteps.findChild<QComboBox *>("squadStepWhen");emptyWhen->setCurrentIndex(emptyWhen->findData(1));
            QVERIFY(QMetaObject::invokeMethod(emptyWhen,"activated",Q_ARG(int,emptyWhen->currentIndex())));
            QCOMPARE(emptyProject.scenarios()->scenario(10).layout.conditions.size(),1);
            QCOMPARE(u8(emptyProject.scenarios()->scenario(10).layout.conditions[0],1),16);
            QCOMPARE(u8(emptyProject.scenarios()->scenario(10).layout.nodes[emptySteps.selectedNode()],10),1);
            QUndoStack undo;ScenarioEditor editor(&project,&undo,[]{});editor.resize(1100,800);editor.show();
            auto scenarios=editor.findChild<QListWidget *>("scenarioList");
            for(int i=0;i<scenarios->count();++i)if(scenarios->item(i)->data(Qt::UserRole).toInt()==key)scenarios->setCurrentRow(i);
            QVERIFY(selectMapObject(editor,MapObject::Squad,0));
            auto steps=static_cast<SquadRouteSteps *>(editor.findChild<QWidget *>("squadRouteSteps"));QVERIFY(steps);
            auto activate=[&](const char *name,int value) {
                auto box=steps->findChild<QComboBox *>(name);QVERIFY(box && box->isEnabled());const int index=box->findData(value);QVERIFY(index>=0);
                box->setCurrentIndex(index);QVERIFY(QMetaObject::invokeMethod(box,"activated",Q_ARG(int,index)));
            };
            auto refresh=[&]{scenarios->currentRowChanged(scenarios->currentRow());QVERIFY(selectMapObject(editor,MapObject::Squad,0));steps->selectNode(first);};
            activate("squadStepWhen",1);
            auto newTrigger=steps->findChild<QPushButton *>("squadTriggerANew");QVERIFY(newTrigger && newTrigger->isEnabled());newTrigger->click();
            const int areaId=u8(project.scenarios()->scenario(key).layout.nodes[first],10);
            QCOMPARE(u8(project.scenarios()->scenario(key).layout.conditions[areaId-1],1),16);
            const auto beforeArea=project.scenarios()->json();
            const auto deploymentBeforeArea=project.scenarios()->scenario(key).layout.deployments[0];
            auto canvas=editor.findChild<QWidget *>("scenarioMapCanvas");QTest::qWait(40);
            auto clickGrid=[&](QPointF grid) {
                QTest::qWait(20); // Pick instructions can change the map's available height.
                const qreal scale=(std::min(canvas->width(),canvas->height())-72)/256.;
                const auto center=QPointF(canvas->width()/2.,canvas->height()/2.);
                const auto pixel=(center+(grid-QPointF(128,128))*scale).toPoint();
                const auto actual=(QPointF(pixel)-center)/scale+QPointF(128,128);
                QTest::mouseClick(canvas,Qt::LeftButton,Qt::NoModifier,pixel);
                return QPoint(qRound(actual.x()),qRound(actual.y()));
            };
            auto pick=steps->findChild<QPushButton *>("squadTriggerAPick");QVERIFY(pick);
            pick->click();clickGrid({128,144});
            QCOMPARE(project.scenarios()->json(),beforeArea);QVERIFY(steps->isPickingArea());
            QTest::keyClick(canvas,Qt::Key_Escape);QVERIFY(!steps->isPicking());QCOMPARE(project.scenarios()->json(),beforeArea);
            pick->click();const auto corner1=clickGrid({128,144});
            const auto corner2=clickGrid({80,96});
            QVERIFY(!steps->isPicking());
            const auto area=project.scenarios()->scenario(key).layout.conditions[areaId-1];
            const QRect expectedArea=QRect(corner1,corner2).normalized();
            QCOMPARE(u8(area,2),expectedArea.left());QCOMPARE(u8(area,3),expectedArea.top());
            QCOMPARE(u8(area,4),expectedArea.right());QCOMPARE(u8(area,5),expectedArea.bottom());
            QCOMPARE(project.scenarios()->scenario(key).layout.deployments[0],deploymentBeforeArea);
            QCOMPARE(steps->selectedNode(),first);QCOMPARE(steps->triggerAreas().size(),1);
            undo.undo();QCOMPARE(project.scenarios()->json(),beforeArea);undo.redo();
            // Coordinate edits keep rectangles valid in one Undo operation.
            auto bound=steps->findChild<QSpinBox *>("squadTriggerABound0");bound->setValue(150);
            QCOMPARE(u8(project.scenarios()->scenario(key).layout.conditions[areaId-1],4),150);undo.undo();
            // Destination picking, tool changes and squad selection cancel unfinished areas.
            pick->click();steps->findChild<QPushButton *>("squadStepPick")->click();QVERIFY(!steps->isPickingArea());QVERIFY(steps->isPicking());steps->cancelPick();
            pick->click();editor.findChild<QComboBox *>("mapTool")->setCurrentIndex(1);QVERIFY(!steps->isPicking());editor.findChild<QComboBox *>("mapTool")->setCurrentIndex(0);
            pick->click();QVERIFY(selectMapObject(editor,MapObject::Site,0));QVERIFY(!steps->isPicking());QVERIFY(selectMapObject(editor,MapObject::Squad,0));
            activate("squadStepWhen",2);activate("squadStepConditionB",-9);
            auto count=steps->findChild<QSpinBox *>("squadTriggerBCount");count->setValue(4);
            const int countId=u8(project.scenarios()->scenario(key).layout.nodes[first],12);
            const int conditionCount=project.scenarios()->scenario(key).layout.conditions.size();
            count->setValue(6);QCOMPARE(project.scenarios()->scenario(key).layout.conditions.size(),conditionCount);undo.undo();
            const auto edited=project.scenarios()->json();
            // Changing a private trigger's type reuses its slot; Undo restores all bytes.
            activate("squadTriggerBType",16);QCOMPARE(project.scenarios()->scenario(key).layout.conditions.size(),conditionCount);undo.undo();QCOMPARE(project.scenarios()->json(),edited);
            // A shared trigger gets a copy, leaving the other step's original predicate intact.
            auto shared=*project.scenarios();auto otherNode=shared.scenario(key).layout.nodes[other];put8(otherNode,10,quint8(countId));shared.setNode(key,other,otherNode,classes);
            project.setScenarios(shared.json());refresh();count->setValue(7);
            const auto &split=project.scenarios()->scenario(key).layout;
            QVERIFY(u8(split.nodes[first],12)!=countId);QCOMPARE(u8(split.conditions[countId-1],6),4);QCOMPARE(u8(split.conditions[u8(split.nodes[first],12)-1],6),7);
            undo.undo();QCOMPARE(project.scenarios()->json(),shared.json());
            project.setScenarios(edited);undo.clear();refresh();
            // Native count success is consumed: AND must not test one ID twice.
            activate("squadStepConditionA",countId);
            QVERIFY(u8(project.scenarios()->scenario(key).layout.nodes[first],10)!=countId);undo.undo();QCOMPARE(project.scenarios()->json(),edited);
            // Original predicates are copied even if no other ESET node references them.
            bool checkedOriginal=false;
            for(const auto &entry:model.entries()) {
                for(int c=0;c<model.original(entry.key).layout.conditions.size();++c) {
                    if(u8(model.original(entry.key).layout.conditions[c],1)!=9 || entry.layout.nodes.size()>=16 || entry.layout.conditions.size()>=16)continue;
                    auto copy=model;const int row=copy.addNode(entry.key,0,classes);auto node=copy.scenario(entry.key).layout.nodes[row];put8(node,10,quint8(c+1));copy.setNode(entry.key,row,node,classes);
                    const auto original=copy.scenario(entry.key).layout.conditions[c];auto predicate=original;put8(predicate,6,u8(original,6)==5?6:5);
                    copy.setStepTrigger(entry.key,row,0,predicate,classes);
                    QCOMPARE(copy.scenario(entry.key).layout.conditions[c],original);QVERIFY(u8(copy.scenario(entry.key).layout.nodes[row],10)!=c+1);checkedOriginal=true;break;
                }
                if(checkedOriginal)break;
            }
            QVERIFY(checkedOriginal);
            // Capacity failure is atomic; a private trigger still edits at capacity.
            auto full=*project.scenarios();while(full.scenario(key).layout.conditions.size()<16)full.addCondition(key,9,classes);
            project.setScenarios(full.json());refresh();const auto atCapacity=project.scenarios()->json();activate("squadStepConditionA",-16);
            QCOMPARE(project.scenarios()->json(),atCapacity);QVERIFY(steps->findChild<QLabel *>("squadStepNotice")->text().contains("16"));
            steps->findChild<QSpinBox *>("squadTriggerBCount")->setValue(8);QCOMPARE(u8(project.scenarios()->scenario(key).layout.conditions[countId-1],6),8);
            auto fullShared=*project.scenarios();auto sharedNode=fullShared.scenario(key).layout.nodes[other];put8(sharedNode,10,quint8(countId));fullShared.setNode(key,other,sharedNode,classes);
            project.setScenarios(fullShared.json());refresh();steps->findChild<QSpinBox *>("squadTriggerBCount")->setValue(9);
            QCOMPARE(project.scenarios()->json(),fullShared.json());QVERIFY(steps->findChild<QLabel *>("squadStepNotice")->text().contains("protect"));
            project.setScenarios(edited);undo.clear();refresh();
            auto invalid=*project.scenarios();auto bad=area;put8(bad,2,200);put8(bad,4,20);
            QVERIFY_THROWS_EXCEPTION(Error,invalid.setCondition(key,areaId-1,bad,classes));QCOMPARE(invalid.json(),edited);
            auto scroll=editor.findChild<QScrollArea *>("mapInspectorScroll");scroll->ensureWidgetVisible(steps->findChild<QComboBox *>("squadTriggerAType"));QTest::qWait(40);
            for(auto box:steps->findChildren<QComboBox *>())if(box->isVisible())QVERIFY(box->height()>=box->sizeHint().height());
            QVERIFY(editor.grab().save("native/build/sidebar-triggers-area.png"));
            scroll->ensureWidgetVisible(count);QTest::qWait(40);QVERIFY(editor.grab().save("native/build/sidebar-triggers-count.png"));
            QTemporaryDir folder,work;project.saveFolder(folder.path());
            auto loaded=Project::fromRom(Rom::open(path));loaded.applyManifest(Project::readProject(folder.path()));QCOMPARE(loaded.scenarios()->json(),edited);
            const auto result=rebuild(project,tools,work.path());QTemporaryDir output(QFileInfo(path).absolutePath()+"/Test ROMs/native-sidebar-triggers-XXXXXX");QVERIFY(output.isValid());
            const auto candidate=output.filePath(QFileInfo(path).fileName());exportRom(project,result,candidate);
            auto reopened=Project::fromRom(Rom::open(candidate));reopened.loadScenarios();QCOMPARE(reopened.scenarios()->json(),edited);
            auto checksum=reopened.rom().bytes();Rom::updateChecksum(checksum);QCOMPARE(checksum,reopened.rom().bytes());
            QJsonArray conditions;for(const auto &c:reopened.scenarios()->scenario(key).layout.conditions)conditions<<QString::fromLatin1(c.toHex());
            QJsonArray nodes;for(const auto &r:reopened.scenarios()->scenario(key).layout.nodes)nodes<<QString::fromLatin1(r.toHex());
            QFile fixture(output.filePath("sidebar-triggers.json"));QVERIFY(fixture.open(QIODevice::WriteOnly));fixture.write(QJsonDocument(QJsonObject{{"rom",candidate},{"conditions",conditions},{"nodes",nodes},{"first",first+4},{"next",other+4},{"area",areaId},{"count",countId}}).toJson());fixture.close();
            QProcess process;process.start("node",{QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("sidebar-triggers-mips.cjs"),fixture.fileName()});
            QVERIFY(process.waitForFinished(30000));const auto evidence=process.readAllStandardOutput()+process.readAllStandardError();QVERIFY2(process.exitCode()==0,evidence.constData());qInfo().noquote()<<evidence;
            output.setAutoRemove(false);qInfo().noquote()<<"Sidebar triggers ROM:"<<candidate;
        }catch(const Error &e){QFAIL(e.what());}
    }
    void scenarioScriptedContinuation() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));project.loadScenarios();const auto original=*project.scenarios();
        QSet<int> checked;
        for(const auto &scenario:original.entries()) {
            if(scenario.layout.nodes.isEmpty() || scenario.layout.deployments.isEmpty())continue;
            for(const auto &condition:scenario.layout.conditions) {
                const int kind=u8(condition,1);if((kind!=14 && kind!=20) || checked.contains(kind))continue;
                auto next=original;auto n=scenario.layout.nodes[0];
                put8(n,10,u8(condition,0));for(int at:{11,12,13,14,15,16})put8(n,at,0);put8(n,17,1);
                next.setNode(scenario.key,0,n,project.classData());auto d=scenario.layout.deployments[0];put8(d,6,4);
                next.setDeployment(scenario.key,0,d,project.classData());project.setScenarios(next.json());
                const auto before=project.scenarios()->json();
                SquadRouteSteps widget(&project,[&](const Scenarios &edited,const QString &){project.setScenarios(edited.json());});
                widget.refresh(scenario.key,0);widget.findChild<QCheckBox *>("squadStepShared")->setChecked(true);
                QVERIFY(!widget.findChild<QComboBox *>("squadStepWhen")->isEnabled());
                QVERIFY(!widget.findChild<QComboBox *>("squadStepNext")->isEnabled());
                QCOMPARE(project.scenarios()->json(),before);
                auto action=widget.findChild<QComboBox *>("squadStepAction");QVERIFY(action->isEnabled());
                const int index=action->findData(int(SquadBehavior::MoveAndAttack));action->setCurrentIndex(index);
                QVERIFY(QMetaObject::invokeMethod(action,"activated",Q_ARG(int,index)));
                QCOMPARE(project.scenarios()->scenario(scenario.key).layout.nodes[0].mid(10),n.mid(10));
                QCOMPARE(project.scenarios()->scenario(scenario.key).layout.conditions,scenario.layout.conditions);
                checked.insert(kind);
            }
        }
        QVERIFY(checked.contains(20));
        // The retail resolver also supports kind 14, absent from this ROM's condition corpus.
        // Its special routing must be recognized from condition context even with zero extension bytes.
        Eset context;QByteArray predicate(10,0);put8(predicate,0,1);put8(predicate,1,14);context.conditions<<predicate;
        QByteArray n(18,0);put8(n,0,4);put8(n,10,1);put8(n,17,5);
        QVERIFY(!SquadRoute::extended(n));QVERIFY(SquadRoute::extended(n,&context));
    }
    void scenarioOrderDrafts() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));project.loadScenarios();
        auto scripted=*project.scenarios();scripted.setSquadCommand(1,0,{SquadBehavior::Move,128,128},project.classData());
        const int node=u8(scripted.scenario(1).layout.deployments[0],6)-4;
        QVERIFY(node>=0);auto record=scripted.scenario(1).layout.nodes[node];put8(record,17,1);
        scripted.setNode(1,node,record,project.classData());project.setScenarios(scripted.json());
        QUndoStack undo;ScenarioEditor editor(&project,&undo,[]{});editor.resize(1100,800);editor.show();
        QVERIFY(selectMapObject(editor,MapObject::Squad,0));
        editor.findChild<QCheckBox *>("mapReplaceRoute")->setChecked(true);
        auto orders=editor.findChild<QWidget *>("mapSquadOrders");auto command=orders->findChild<QComboBox *>("squadCommand");
        auto x=orders->findChild<QSpinBox *>("squadCommandX"),z=orders->findChild<QSpinBox *>("squadCommandZ");
        auto replace=orders->findChild<QPushButton *>("squadCommandApply");
        command->setCurrentIndex(command->findData(int(SquadBehavior::MoveAndAttack)));
        orders->findChild<QComboBox *>("squadCommandDestination")->setCurrentIndex(0);x->setValue(96);z->setValue(144);
        QCOMPARE(project.scenarios()->json(),scripted.json());QVERIFY(replace->isVisible());
        QVERIFY(selectMapObject(editor,MapObject::Site,0));QVERIFY(selectMapObject(editor,MapObject::Squad,0));
        QCOMPARE(x->value(),96);QCOMPARE(z->value(),144);QVERIFY(orders->findChild<QLabel *>("squadCommandHelp")->text().contains("not applied"));
        QVERIFY(editor.findChild<QLabel *>("mapTerrainRoute")->text().contains("Pending order preview"));
        replace->click();const auto committed=project.scenarios()->json();const auto result=project.scenarios()->squadCommand(1,0);
        QVERIFY(result);QCOMPARE(result->x,96);QCOMPARE(result->z,144);
        QCOMPARE(project.scenarios()->scenario(1).layout.nodes.first(scripted.scenario(1).layout.nodes.size()),scripted.scenario(1).layout.nodes);
        undo.undo();QCOMPARE(project.scenarios()->json(),scripted.json());undo.redo();QCOMPARE(project.scenarios()->json(),committed);
        QVERIFY(!replace->isVisible());const int before=undo.index();x->setValue(80);z->setValue(100);
        QCOMPARE(project.scenarios()->squadCommand(1,0)->x,80);QCOMPARE(project.scenarios()->squadCommand(1,0)->z,100);
        undo.setIndex(before);QCOMPARE(project.scenarios()->json(),committed);
        command->setCurrentIndex(command->findData(int(SquadBehavior::Guard)));const auto guard=project.scenarios()->json();
        command->setCurrentIndex(command->findData(int(SquadBehavior::Move)));QCOMPARE(project.scenarios()->json(),guard);
        QVERIFY(selectMapObject(editor,MapObject::Site,0));QVERIFY(selectMapObject(editor,MapObject::Squad,0));
        QCOMPARE(command->currentData().toInt(),int(SquadBehavior::Move));QCOMPARE(project.scenarios()->json(),guard);
        orders->findChild<QPushButton *>("squadCommandDiscard")->click();QCOMPARE(command->currentData().toInt(),int(SquadBehavior::Guard));
    }
    void scenarioAdvancedDrafts() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));project.loadScenarios();auto fixture=*project.scenarios();
        const int node=fixture.addNode(1,1,project.classData());fixture.addNode(1,1,project.classData());
        const int condition=fixture.addCondition(1,9,project.classData());fixture.addCondition(1,9,project.classData());project.setScenarios(fixture.json());
        QUndoStack undo;ScenarioEditor editor(&project,&undo,[]{});editor.resize(1100,800);editor.show();
        editor.findChild<QStackedWidget *>("scenarioPages")->setCurrentIndex(2);editor.findChild<QCheckBox *>("scenarioRouteAdvanced")->setChecked(true);
        auto nodes=editor.findChild<QListWidget *>("scenarioNodes");nodes->setCurrentRow(node);
        editor.findChild<QComboBox *>("routePlacement")->setCurrentIndex(1);editor.findChild<QSpinBox *>("routeX")->setValue(73);editor.findChild<QSpinBox *>("routeZ")->setValue(94);
        nodes->setCurrentRow(node+1);nodes->setCurrentRow(node);QCOMPARE(editor.findChild<QSpinBox *>("routeX")->value(),73);QCOMPARE(project.scenarios()->json(),fixture.json());
        auto scenarios=editor.findChild<QListWidget *>("scenarioList");
        for(int key:{10,1})for(int i=0;i<scenarios->count();++i)if(scenarios->item(i)->data(Qt::UserRole).toInt()==key)scenarios->setCurrentRow(i);
        nodes->setCurrentRow(node);QCOMPARE(editor.findChild<QSpinBox *>("routeZ")->value(),94);
        editor.findChild<QPushButton *>("routeNodeApply")->click();QCOMPARE(u8(project.scenarios()->scenario(1).layout.nodes[node],4),73);
        undo.undo();QCOMPARE(project.scenarios()->json(),fixture.json());
        auto conditions=editor.findChild<QListWidget *>("scenarioConditions");conditions->setCurrentRow(condition);
        editor.findChild<QSpinBox *>("routePayload_6")->setValue(35);editor.findChild<QPushButton *>("routeConditionApply")->click();QCOMPARE(project.scenarios()->json(),fixture.json());
        conditions->setCurrentRow(condition+1);conditions->setCurrentRow(condition);QCOMPARE(editor.findChild<QSpinBox *>("routePayload_6")->value(),35);
        editor.findChild<QPushButton *>("routeDiscardCondition")->click();QCOMPARE(editor.findChild<QSpinBox *>("routePayload_6")->value(),1);
        auto group=editor.findChild<QComboBox *>("routeGroupSquad");group->setCurrentIndex(group->findData(1));
        const int originalGroup=editor.findChild<QSpinBox *>("routeGroup")->value();editor.findChild<QSpinBox *>("routeGroup")->setValue(7);
        group->setCurrentIndex(group->findData(2));group->setCurrentIndex(group->findData(1));QCOMPARE(editor.findChild<QSpinBox *>("routeGroup")->value(),7);
        editor.findChild<QPushButton *>("routeDiscardGroup")->click();QCOMPARE(editor.findChild<QSpinBox *>("routeGroup")->value(),originalGroup);QCOMPARE(project.scenarios()->json(),fixture.json());
    }
    void scenarioAdvancedSmallWindow() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        applyEditorTheme(*qApp);
        auto project = Project::fromRom(Rom::open(path));
        project.loadScenarios();
        QUndoStack undo;
        ScenarioEditor editor(&project, &undo, [] {});
        editor.resize(1080, 720); editor.show();
        editor.findChild<QStackedWidget *>("scenarioPages")->setCurrentIndex(2);
        editor.findChild<QCheckBox *>("scenarioRouteAdvanced")->setChecked(true);
        auto scroll = editor.findChild<QScrollArea *>("scenarioOrdersScroll");
        auto tabs = editor.findChild<QTabWidget *>("scenarioRouteTabs");
        QTRY_VERIFY(scroll->verticalScrollBar()->maximum() > 0);
        QCOMPARE(editor.size(), QSize(1080, 720));
        for (int page = 0; page < 3; ++page) {
            tabs->setCurrentIndex(page);
            QTest::qWait(30);
            for (auto field : tabs->currentWidget()->findChildren<QComboBox *>()) {
                if (!field->isVisible()) continue;
                QVERIFY2(field->height() >= field->sizeHint().height(), qPrintable(field->objectName()));
            }
            for (auto field : tabs->currentWidget()->findChildren<QSpinBox *>()) {
                if (!field->isVisible()) continue;
                QVERIFY2(field->height() >= field->sizeHint().height(), qPrintable(field->objectName()));
            }
        }
        tabs->setCurrentIndex(0);
        auto apply = editor.findChild<QPushButton *>("routeNodeApply");
        scroll->ensureWidgetVisible(apply);
        QTest::qWait(50);
        QVERIFY(scroll->viewport()->rect().contains(apply->mapTo(scroll->viewport(), apply->rect().center())));
        QVERIFY(editor.grab().save("scenario-advanced-small.png"));
    }
    void mountainPeakEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadScenarios();
        project.loadWorld();
        project.loadMapEdits();
        QUndoStack undo;
        ScenarioMapEditor editor(
            &project, &undo, [](const Scenarios &, const QString &) {}, [](MapObject, int) {});
        editor.resize(1300, 950);
        editor.refresh(5);
        editor.show();
        connect(&undo, &QUndoStack::indexChanged, &editor, [&] { editor.refresh(5); });
        auto tool = editor.findChild<QComboBox *>("mapTool");
        tool->setCurrentText("Scenery");
        editor.findChild<QComboBox *>("mapSceneryAction")->setCurrentText("Mountain peaks");
        QTest::qWait(20);
        auto objects = editor.findChild<QListWidget *>("mapObjects");
        auto canvas = editor.findChild<QWidget *>("scenarioMapCanvas");
        auto height = editor.findChild<QDoubleSpinBox *>("mapPeakHeight");
        auto restore = editor.findChild<QPushButton *>("mapPeakRestore");
        QVERIFY(objects && canvas && height && restore);
        const auto peaks = project.mapEdits()->mountainPeaks(5);
        QCOMPARE(objects->count(), peaks.size());
        int selected = -1;
        for (int i = 0; i < peaks.size(); ++i)
            if (QRectF(70, 80, 130, 145).contains(peaks[i].position)) {
                selected = i;
                break;
            }
        QVERIFY(selected >= 0);
        const auto peak = peaks[selected];
        auto pixel = [&] {
            const double side = std::min(canvas->width(), canvas->height()) - 72;
            const double h = project.mapEdits()->mountainPeaks(5)[selected].height;
            return (QPointF(canvas->width() / 2., canvas->height() / 2.) +
                    (peak.position - QPointF(128, 128)) * side / 256. - QPointF(0, 12 + h * 24))
                .toPoint();
        };
        objects->setCurrentRow(selected);
        QVERIFY(height->isVisible());
        QVERIFY(!editor.findChild<QPushButton *>("mapApply"));
        const auto original = project.mapEdits()->json();
        auto start = pixel();
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(canvas, start + QPoint(25, -24), 20);
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, start + QPoint(25, -24));
        QCOMPARE(undo.count(), 1);
        QCOMPARE(project.mapEdits()->mountainPeaks(5)[selected].position, peak.position);
        QVERIFY(std::abs(project.mapEdits()->mountainPeaks(5)[selected].height - peak.height - 1) <
                .001);
        const auto raised = project.mapEdits()->json();
        start = pixel();
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(canvas, start + QPoint(0, 48), 20);
        QTest::keyClick(canvas, Qt::Key_Escape);
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, start + QPoint(0, 48));
        QCOMPARE(project.mapEdits()->json(), raised);
        QCOMPARE(undo.count(), 1);
        undo.undo();
        QCOMPARE(project.mapEdits()->json(), original);
        undo.redo();
        QCOMPARE(project.mapEdits()->json(), raised);
        start = pixel();
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(canvas, start + QPoint(0, 24), 20);
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, start + QPoint(0, 24));
        QCOMPARE(project.mapEdits()->json(), original);
        QCOMPARE(undo.count(), 2);
        height->setValue(.75);
        QCOMPARE(project.mapEdits()->mountainPeaks(5)[selected].height, .75);
        restore->click();
        QCOMPARE(project.mapEdits()->json(), original);
        // At zero height the base and diamond are just 12 pixels apart. They
        // must still choose different operations, without a mode selector.
        height->setValue(0);
        const auto zeroHeight = project.mapEdits()->json();
        QTest::qWait(20);
        start = pixel() + QPoint(0, 12);
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, start);
        QCOMPARE(project.mapEdits()->json(), zeroHeight);
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(canvas, start + QPoint(8, 4), 20);
        QCOMPARE(canvas->cursor().shape(), Qt::SizeAllCursor);
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, start + QPoint(8, 4));
        QCOMPARE(project.mapEdits()->mountainPeaks(5)[selected].height, 0.);
        QVERIFY(QLineF(project.mapEdits()->mountainPeaks(5)[selected].position, peak.position)
                    .length() > 1);
        undo.undo();
        QCOMPARE(project.mapEdits()->json(), zeroHeight);
        // The bottom of the zero-height diamond is closer to the base centre,
        // but grabbing the visible diamond must still edit height.
        start = pixel() + QPoint(0, 6);
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(canvas, start + QPoint(20, -12), 20);
        QCOMPARE(canvas->cursor().shape(), Qt::SizeVerCursor);
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, start + QPoint(20, -12));
        QCOMPARE(project.mapEdits()->mountainPeaks(5)[selected].position, peak.position);
        QCOMPARE(project.mapEdits()->mountainPeaks(5)[selected].height, .5);
        undo.undo();
        QCOMPARE(project.mapEdits()->json(), zeroHeight);
        height->setValue(.75);
        const auto heightOnly = project.mapEdits()->json();
        auto add = editor.findChild<QPushButton *>("mapPeakAdd");
        auto remove = editor.findChild<QPushButton *>("mapPeakRemove");
        auto resetPeaks = editor.findChild<QPushButton *>("mapPeaksRestore");
        QVERIFY(add && remove && resetPeaks);
        QVERIFY(!editor.findChild<QComboBox *>("mapPeakDrag"));
        add->click();
        QTest::keyClick(canvas, Qt::Key_Escape);
        QVERIFY(!add->isChecked());
        QCOMPARE(project.mapEdits()->json(), heightOnly);
        auto mapPixel = [&](QPointF grid) {
            const double side = std::min(canvas->width(), canvas->height()) - 72;
            return QPointF(canvas->width() / 2., canvas->height() / 2.) +
                   (grid - QPointF(128, 128)) * side / 256.;
        };
        QPoint place(-1, -1);
        const auto terrain = project.mapEdits()->source(5).visualTerrain;
        for (int y = 80; y < 210 && place.x() < 0; ++y)
            for (int x = 70; x < 200 && place.x() < 0; ++x) {
                bool inside = true;
                for (int dy = -4; dy <= 4 && inside; ++dy)
                    for (int dx = -4; dx <= 4; ++dx)
                        inside &= u8(terrain, (y + dy) * 256 + x + dx) == 7;
                for (const auto &p : peaks)
                    inside &= QLineF(p.position, QPointF(x, y)).length() > 5;
                if (inside)
                    place = {x, y};
            }
        QVERIFY(place.x() >= 0);
        add->click();
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, mapPixel({-5, -5}).toPoint());
        QCOMPARE(project.mapEdits()->json(), heightOnly);
        QVERIFY(add->isChecked());
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, mapPixel(place).toPoint());
        QVERIFY2(!add->isChecked(), qPrintable(editor.findChild<QLabel *>("mapNotice")->text()));
        QCOMPARE(objects->count(), peaks.size() + 1);
        QCOMPARE(objects->currentRow(), objects->count() - 1);
        const auto added = project.mapEdits()->mountainPeaks(5).last();
        QVERIFY(added.addedId > 0);
        const auto addedState = project.mapEdits()->json();
        undo.undo();
        QCOMPARE(project.mapEdits()->json(), heightOnly);
        undo.redo();
        QCOMPARE(project.mapEdits()->json(), addedState);
        objects->setCurrentRow(objects->count() - 1);
        QTest::qWait(20);
        start = mapPixel(added.position).toPoint();
        const auto delta =
            (mapPixel(added.position + QPointF(3, 1)) - mapPixel(added.position)).toPoint();
        const int actions = undo.count();
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(canvas, start + delta, 20);
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, start + delta);
        QCOMPARE(undo.count(), actions + 1);
        const auto movedPeak = project.mapEdits()->mountainPeaks(5).last();
        QCOMPARE(movedPeak.addedId, added.addedId);
        QCOMPARE(movedPeak.height, added.height);
        QVERIFY(QLineF(movedPeak.position, added.position + QPointF(3, 1)).length() < .4);
        const auto movedState = project.mapEdits()->json();
        start = mapPixel(movedPeak.position).toPoint();
        QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(canvas, start + QPoint(10, 10), 20);
        QTest::keyClick(canvas, Qt::Key_Escape);
        QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, start + QPoint(10, 10));
        QCOMPARE(project.mapEdits()->json(), movedState);
        remove->click();
        QCOMPARE(project.mapEdits()->json(), heightOnly);
        QCOMPARE(objects->count(), peaks.size());
        undo.undo();
        QCOMPARE(project.mapEdits()->json(), movedState);
        objects->setCurrentRow(objects->count() - 1);
        QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath() +
                                   "/mountain-peak-layout.png"));
        QTemporaryDir work;
        project.saveFolder(work.filePath("project"));
        auto restored = Project::fromRom(Rom::open(path));
        restored.applyManifest(Project::readProject(work.filePath("project")));
        QCOMPARE(restored.mapEdits()->json(), project.mapEdits()->json());
        const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        QVERIFY(!tools.isEmpty());
        const auto result = rebuild(restored, tools, work.path());
        const auto root = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
        QVERIFY(QDir().mkpath(root));
        QTemporaryDir output(QDir(root).filePath("native-mountain-peaks-XXXXXX"));
        QVERIFY(output.isValid());
        const auto candidate = output.filePath(QFileInfo(path).fileName());
        exportRom(restored, result, candidate);
        const auto reopened = MapEdits::load(Rom::open(candidate));
        QCOMPARE(reopened.model(5), project.mapEdits()->model(5));
        QCOMPARE(reopened.source(5).geometry[1].data,
                 editFieldHeight(project.mapEdits()->source(5), project.mapEdits()->changes(5)));
        auto checksum = result.image;
        Rom::updateChecksum(checksum);
        QCOMPARE(checksum, result.image);
        output.setAutoRemove(false);
        qInfo().noquote() << "Peak edit export:" << candidate;
        resetPeaks->click();
        QCOMPARE(project.mapEdits()->json(), original);
        undo.undo();
        QCOMPARE(project.mapEdits()->json(), movedState);
        undo.redo();
        QCOMPARE(project.mapEdits()->json(), original);
        tool->setCurrentIndex(0);
        QVERIFY(!height->isVisible());
        QVERIFY(selectMapObject(editor, MapObject::Squad, 0));
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
            auto pages = editor.findChild<QStackedWidget *>("scenarioPages");
            pages->setCurrentIndex(0);
            QTest::qWait(20);
            auto objects = editor.findChild<QListWidget *>("mapObjects");
            auto fields = editor.findChild<QPushButton *>("mapEditFields");
            auto x = editor.findChild<QSpinBox *>("mapX"), z = editor.findChild<QSpinBox *>("mapZ");
            auto canvas = editor.findChild<QWidget *>("scenarioMapCanvas");
            QVERIFY(objects && fields && x && z && canvas);
            // Marker artwork follows the current records, including edits and Undo.
            QVERIFY(!objects->item(0)->icon().isNull());
            const auto leaderIcon = objects->item(0)->icon().pixmap(32, 32).toImage();
            QCOMPARE(leaderIcon.pixelColor(0, 0).alpha(), 0); // Circular crop, not a square portrait.
            QVERIFY(leaderIcon.pixelColor(16, 16).alpha() > 0);
            QSet<QRgb> portraitColors;
            for (int py = 2; py < 30; ++py)
                for (int px = 2; px < 30; ++px)
                    if (leaderIcon.pixelColor(px, py).alpha() == 255) portraitColors.insert(leaderIcon.pixel(px, py));
            QVERIFY(portraitColors.size() > 8); // The stock boss has an enemy portrait, not a blank badge.
            const auto beforeLeader = project.scenarios()->json();
            objects->setCurrentRow(0);
            const int alternateClass=u8(project.scenarios()->scenario(1).squads[0],0)==5?6:5;
            editScenarioMembers(editor,"mapSquad",[&](QDialog *dialog){
                auto leaderClass=dialog->findChild<QComboBox *>("mapSquadClass_0");
                leaderClass->setCurrentIndex(leaderClass->findData(alternateClass));
                dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
            });
            QCOMPARE(u8(project.scenarios()->scenario(1).squads[0], 0), quint8(alternateClass));
            QVERIFY(objects->item(0)->icon().pixmap(32, 32).toImage() != leaderIcon);
            undo.undo();
            QCOMPARE(project.scenarios()->json(), beforeLeader);
            QCOMPARE(objects->item(0)->icon().pixmap(32, 32).toImage(), leaderIcon);
            int treasureIconRow = -1;
            for (int row = 0; row < objects->count(); ++row)
                if (objects->item(row)->text().startsWith("Treasure 1 Â·")) treasureIconRow = row;
            QVERIFY(treasureIconRow >= 0);
            QVERIFY(!objects->item(treasureIconRow)->icon().isNull());
            const auto treasureIcon = objects->item(treasureIconRow)->icon().pixmap(32, 32).toImage();
            const auto beforeItem = project.world()->json();
            const auto firstTreasure = ScenarioWorld::treasureRecord(project.world()->scenario(1).treasures, 0);
            pages->setCurrentIndex(0);
            objects->setCurrentRow(treasureIconRow);
            QCOMPARE(pages->currentIndex(), 0);
            QVERIFY(editor.findChild<QWidget *>("worldTreasureFields")->isVisible());
            auto itemTable = editor.findChild<QComboBox *>("worldTreasureTable");
            itemTable->setCurrentIndex(itemTable->findData(u8(firstTreasure, 3) == 1 ? 2 : 1));
            auto treasureChoice = editor.findChild<QComboBox *>("worldTreasureItem");
            treasureChoice->setCurrentIndex(treasureChoice->findData(1));
            QVERIFY(objects->item(treasureIconRow)->icon().pixmap(32, 32).toImage() != treasureIcon);
            undo.undo();
            QCOMPARE(project.world()->json(), beforeItem);
            QCOMPARE(objects->item(treasureIconRow)->icon().pixmap(32, 32).toImage(), treasureIcon);
            pages->setCurrentIndex(0);
            objects->setCurrentRow(0);
            auto terrain = editor.findChild<QCheckBox *>("mapTerrain");
            QVERIFY(terrain);
            QVERIFY(terrain->isEnabled());
            QVERIFY(terrain->isChecked());
            QVERIFY(editor.findChild<QLabel *>("mapTerrainStatus")->text().contains("In-scenario battlefield"));
            const auto terrainImage = canvas->grab().toImage();
            const auto dataBeforeToggle = project.scenarios()->json();
            const auto undoBeforeToggle = undo.count();
            terrain->setChecked(false);
            const auto withoutTerrain = canvas->grab().toImage();
            QVERIFY(withoutTerrain != terrainImage);
            terrain->setChecked(true);
            QCOMPARE(canvas->grab().toImage(), terrainImage);
            QCOMPARE(project.scenarios()->json(), dataBeforeToggle);
            QCOMPARE(undo.count(), undoBeforeToggle);
            // Applying allegiance must repaint the actual building texture;
            // undo must also restore it, rather than leave a stale cached map.
            QVERIFY(selectMapObject(editor, MapObject::Site, 0));
            const auto beforeAllegiance = canvas->grab().toImage();
            auto allegiance = editor.findChild<QComboBox *>("worldAllegiance");
            allegiance->setCurrentIndex(allegiance->findData(2));
            QVERIFY(beforeAllegiance != canvas->grab().toImage());
            undo.undo();
            QCOMPARE(canvas->grab().toImage(), beforeAllegiance);
            QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath() + "/scenario-battlefield.png"));
            const auto beforeTreasure = project.world()->json();
            const int treasuresBefore = ScenarioWorld::treasureCount(project.world()->scenario(1).treasures);
            editor.findChild<QPushButton *>("mapTreasureAdd")->click();
            QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(canvas->width() / 2, canvas->height() / 2));
            QCOMPARE(ScenarioWorld::treasureCount(project.world()->scenario(1).treasures), treasuresBefore + 1);
            const auto placed = ScenarioWorld::treasureRecord(project.world()->scenario(1).treasures, treasuresBefore);
            QCOMPARE(u8(placed, 1), quint8(128));
            QCOMPARE(u8(placed, 2), quint8(128));
            const auto afterTreasure = project.world()->json();
            undo.undo(); QCOMPARE(project.world()->json(), beforeTreasure);
            undo.redo(); QCOMPARE(project.world()->json(), afterTreasure);
            // Keep the newly placed treasure for this workflow's project/export/reopen checks.
            QVERIFY(objects->count() > project.scenarios()->scenario(1).squads.size());
            const auto original = project.scenarios()->json(),
                       originalWorld = project.world()->json();
            objects->setCurrentRow(0);
            QCOMPARE(pages->currentIndex(),0);
            QCOMPARE(editor.findChild<QListWidget *>("scenarioSquads")->currentRow(),0);
            pages->setCurrentIndex(0);
            objects->setCurrentRow(0);
            const int beforePosition=undo.index();
            x->setValue(128);z->setValue(128);const int afterPosition=undo.index();
            const auto centered = project.scenarios()->json();
            QCOMPARE(u8(project.scenarios()->scenario(1).layout.deployments[0], 3), quint8(128));
            QCOMPARE(u8(project.scenarios()->scenario(1).layout.deployments[0], 4), quint8(128));
            undo.setIndex(beforePosition);QCOMPARE(project.scenarios()->json(),original);
            undo.setIndex(afterPosition);QCOMPARE(project.scenarios()->json(),centered);
            const QPoint center(canvas->width() / 2, canvas->height() / 2);
            const int shift = (std::min(canvas->width(), canvas->height()) - 72) / 4;
            const QPoint pan(24, -18), anchor = center + pan;
            QTest::mousePress(canvas, Qt::RightButton, Qt::NoModifier, center);
            QTest::mouseMove(canvas, anchor, 20);
            QTest::mouseRelease(canvas, Qt::RightButton, Qt::NoModifier, anchor);
            QWheelEvent wheel(anchor, canvas->mapToGlobal(anchor), QPoint(), QPoint(0, 120),
                              Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(canvas, &wheel);
            const int zoomedShift = qRound(shift * std::pow(1.0015, 120));
            const QPoint destination = anchor + QPoint(zoomedShift, -zoomedShift);
            const QPoint iconEdge(14, 0); // The visible portrait edge must hit and keep its grab offset.
            QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, anchor + iconEdge);
            QTest::mouseMove(canvas, destination + iconEdge, 20);
            QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, destination + iconEdge);
            const auto moved = project.scenarios()->scenario(1).layout.deployments[0];
            QVERIFY(std::abs(int(u8(moved, 3)) - 192) <= 1);
            QVERIFY(std::abs(int(u8(moved, 4)) - 64) <= 1);
            undo.undo();
            QCOMPARE(project.scenarios()->json(), centered);
            undo.redo();
            editor.findChild<QPushButton *>("mapFit")->click();
            int waypointItem = -1, treasureItem = -1, siteItem = -1;
            const int nodeId = u8(project.scenarios()->scenario(1).layout.nodes[waypoint], 0);
            const auto townName =
                project.world()->townName(u16(project.world()->scenario(1).sites, 1));
            for (int i = 0; i < objects->count(); ++i) {
                const auto label = objects->item(i)->text();
                if (label.startsWith(QString("Waypoint %1").arg(nodeId)))
                    waypointItem = i;
                if (label.startsWith("Treasure 1 Â·"))
                    treasureItem = i;
                if (label == townName)
                    siteItem = i;
            }
            QVERIFY(waypointItem >= 0 && treasureItem >= 0 && siteItem >= 0);
            objects->setCurrentRow(waypointItem);
            x->setValue(55);
            z->setValue(66);
            fields->click();
            QCOMPARE(pages->currentIndex(), 2);
            QCOMPARE(editor.findChild<QListWidget *>("scenarioNodes")->currentRow(), waypoint);
            pages->setCurrentIndex(0);
            objects->setCurrentRow(treasureItem);
            const int beforeTreasureMove = undo.index();
            editor.findChild<QSpinBox *>("worldTreasureX")->setValue(255);
            editor.findChild<QSpinBox *>("worldTreasureZ")->setValue(0);
            auto treasure =
                ScenarioWorld::treasureRecord(project.world()->scenario(1).treasures, 0);
            QCOMPARE(u8(treasure, 1), quint8(255));
            QCOMPARE(u8(treasure, 2), quint8(0));
            const int afterTreasureMove = undo.index();
            undo.setIndex(beforeTreasureMove);
            QCOMPARE(project.world()->json(), originalWorld);
            undo.setIndex(afterTreasureMove);
            QCOMPARE(pages->currentIndex(), 0);
            QVERIFY(editor.findChild<QWidget *>("worldTreasureFields")->isVisible());
            QCOMPARE(editor.findChild<QSpinBox *>("worldTreasureX")->value(), 255);
            // A real map click clears the old inspector, then opens the hit treasure.
            QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
            QVERIFY(!editor.findChild<QWidget *>("mapWorldInspector")->isVisible());
            QTest::qWait(20);
            const qreal gridSize = std::min(canvas->width(), canvas->height()) - 72;
            const QPointF treasurePixel = QPointF(canvas->width() / 2., canvas->height() / 2.) +
                                          QPointF(127, -128) * gridSize / 256.;
            QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, treasurePixel.toPoint());
            QCOMPARE(objects->currentRow(), treasureItem);
            QVERIFY(editor.findChild<QWidget *>("worldTreasureFields")->isVisible());
            pages->setCurrentIndex(0);
            objects->setCurrentRow(siteItem);
            QVERIFY(x->isEnabled());
            const auto originalMaps = project.mapEdits()->json();
            const auto sourceMap = ScenarioMap::load(rom, 1);
            const auto oldSite = sourceMap.points[0];
            const auto startGrid =
                QPointF((oldSite.x() - sourceMap.bounds.left()) / sourceMap.bounds.width() * 256.,
                        (oldSite.y() - sourceMap.bounds.top()) / sourceMap.bounds.height() * 256.);
            auto pixel = [&](QPointF grid) {
                const double side = std::min(canvas->width(), canvas->height()) - 72;
                return (QPointF(canvas->width() / 2., canvas->height() / 2.) +
                        (grid - QPointF(128, 128)) * side / 256.)
                    .toPoint();
            };
            QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, pixel(startGrid));
            QTest::mouseMove(canvas, pixel(startGrid + QPointF(15, 8)), 20);
            QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier,
                                pixel(startGrid + QPointF(15, 8)));
            QVERIFY2(project.mapEdits()->json() != originalMaps,
                     qPrintable(editor.findChild<QLabel *>("mapNotice")->text()));
            const auto movedMap = project.mapEdits()->json();
            undo.undo();
            QCOMPARE(project.mapEdits()->json(), originalMaps);
            undo.redo();
            QCOMPARE(project.mapEdits()->json(), movedMap);
            auto mapTool = editor.findChild<QComboBox *>("mapTool");
            QVERIFY(mapTool);
            mapTool->setCurrentText("Terrain");
            auto terrainAction = editor.findChild<QComboBox *>("mapTerrainAction");
            QVERIFY(terrainAction);
            terrainAction->setCurrentText("Draw road");
            auto brushSize = editor.findChild<QDoubleSpinBox *>("mapBrushSize");
            QVERIFY(brushSize);
            QCOMPARE(brushSize->singleStep(), .5);
            brushSize->setValue(1.5);
            QCOMPARE(brushSize->value(), 1.5);
            const auto beforeRoad = canvas->grab().toImage();
            QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, pixel({105, 120}));
            QTest::mouseMove(canvas, pixel({125, 130}), 20);
            QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, pixel({125, 130}));
            QCOMPARE(u8(project.mapEdits()->fineTerrain(1), 125 * 256 + 115), quint8(1));
            QVERIFY(beforeRoad != canvas->grab().toImage());
            const auto painted = project.mapEdits()->json();
            undo.undo();
            QCOMPARE(project.mapEdits()->json(), movedMap);
            undo.redo();
            QCOMPARE(project.mapEdits()->json(), painted);
            terrainAction->setCurrentText("Paint terrain");
            auto paintType = editor.findChild<QComboBox *>("mapPaintType");
            paintType->setCurrentIndex(paintType->findData(7));
            brushSize->setValue(12);
            QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, pixel({110, 155}));
            QCOMPARE(u8(project.mapEdits()->fineTerrain(1), 155 * 256 + 110), quint8(7));
            // A second landform exercises the separate forest-border texture.
            paintType->setCurrentIndex(paintType->findData(5));
            QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, pixel({145, 160}));
            QCOMPARE(u8(project.mapEdits()->fineTerrain(1), 160 * 256 + 145), quint8(5));
            paintType->setCurrentIndex(paintType->findData(2));
            brushSize->setValue(1.5);
            QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, pixel({175, 68}));
            QTest::mouseMove(canvas, pixel({193, 55}), 20);
            QTest::mouseMove(canvas, pixel({211, 68}), 20);
            QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, pixel({211, 68}));
            QCOMPARE(u8(project.mapEdits()->fineTerrain(1), 68 * 256 + 193), quint8(2));
            QCOMPARE(u8(project.mapEdits()->changes(1).paint, 55 * 256 + 193), quint8(255));
            brushSize->setValue(12);
            const auto terrainPaint = project.mapEdits()->json();
            terrainAction->setCurrentText("Erase painting");
            QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, pixel({110, 155}));
            QCOMPARE(u8(project.mapEdits()->fineTerrain(1), 155 * 256 + 110),
                     u8(project.mapEdits()->source(1).terrain[0].data, 155 * 256 + 110));
            undo.undo();
            QCOMPARE(project.mapEdits()->json(), terrainPaint);
            editor.findChild<QPushButton *>("mapRestore")->click();
            QCOMPARE(project.mapEdits()->json(), originalMaps);
            undo.undo();
            QCOMPARE(project.mapEdits()->json(), terrainPaint);
            // Visibility alone is not an edit; numeric edges and corner dragging are undoable.
            auto boundaryToggle = editor.findChild<QCheckBox *>("mapBoundary");
            QVERIFY(boundaryToggle && boundaryToggle->isChecked());
            const auto withBoundary = canvas->grab().toImage();
            const int beforeBoundaryUndo = undo.index();
            boundaryToggle->setChecked(false);
            QVERIFY(canvas->grab().toImage() != withBoundary);
            boundaryToggle->setChecked(true);
            QCOMPARE(undo.index(), beforeBoundaryUndo);
            const auto originalBoundary = project.mapEdits()->boundary(1);
            mapTool->setCurrentText("Select / move");
            QTest::qWait(20);
            QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier,
                              pixel(originalBoundary.bottomRight()));
            QTest::qWait(20);
            QVERIFY(!editor.findChild<QSpinBox *>("mapBoundaryTop"));
            const auto numericBoundary=originalBoundary;
            QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier,
                              pixel(numericBoundary.bottomRight()));
            QTest::mouseMove(canvas, pixel({248, 249}), 20);
            QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, pixel({248, 249}));
            QVERIFY(std::abs(project.mapEdits()->boundary(1).right() - 248) <= 1);
            QVERIFY(std::abs(project.mapEdits()->boundary(1).bottom() - 249) <= 1);
            const auto resizedBoundary = project.mapEdits()->boundary(1);
            undo.undo();
            QCOMPARE(project.mapEdits()->boundary(1), numericBoundary);
            undo.redo();
            QCOMPARE(project.mapEdits()->boundary(1), resizedBoundary);
            QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath() +
                                       "/scenario-map-boundary.png"));
            mapTool->setCurrentIndex(0);
            QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath()+"/scenario-map-painting.png"));
            QCOMPARE(pages->currentIndex(), 0);
            QVERIFY(editor.findChild<QWidget *>("worldSiteFields")->isVisible());
            QCOMPARE(editor.findChild<QLineEdit *>("worldSiteName")->text(), townName);
            // City scenery and paving use real map input, the shared Undo stack,
            // and the full save/compile/export/reopen checks below.
            const auto beforeCity = project.mapEdits()->json();
            mapTool->setCurrentText("Scenery");
            editor.findChild<QComboBox *>("mapSceneryAction")->setCurrentText("City pieces");
            QTest::qWait(20);
            auto cityType = editor.findChild<QComboBox *>("mapCityType");
            auto cityRotation = editor.findChild<QSpinBox *>("mapCityRotation");
            QVERIFY(cityType && cityRotation && cityType->isVisible());
            QCOMPARE(cityType->count(), 23);
            cityType->setCurrentIndex(cityType->findData(32));
            brushSize->setValue(7);
            QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, pixel({140, 140}));
            QTest::mouseMove(canvas, pixel({164, 140}), 20);
            QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, pixel({164, 140}));
            QCOMPARE(u8(project.mapEdits()->fineTerrain(1), 140 * 256 + 152), quint8(1));
            const auto paving = project.mapEdits()->json();
            undo.undo();
            QCOMPARE(project.mapEdits()->json(), beforeCity);
            undo.redo();
            QCOMPARE(project.mapEdits()->json(), paving);
            const int houseIndex = cityType->findData(QString("latium-house"));
            QVERIFY(houseIndex >= 0 && !cityType->itemIcon(houseIndex).isNull());
            cityType->setCurrentIndex(houseIndex);
            cityRotation->setValue(90);
            QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, pixel({150, 140}));
            QCOMPARE(project.mapEdits()->changes(1).city.size(), 1);
            QCOMPARE(project.mapEdits()->changes(1).city[0].rotation, 90);
            const auto placedCity = project.mapEdits()->json();
            const int placedUndo = undo.index();
            QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, pixel({0, 0}));
            QCOMPARE(project.mapEdits()->json(), placedCity);
            QCOMPARE(undo.index(), placedUndo);
            QTest::qWait(20);
            QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath() + "/scenario-city-palette.png"));
            mapTool->setCurrentIndex(0);
            QVERIFY(!cityType->isVisible());
            int cityItem = -1;
            for (int i = 0; i < objects->count(); ++i)
                if (objects->item(i)->data(Qt::UserRole).toInt() == int(MapObject::City))
                    cityItem = i;
            QVERIFY(cityItem >= 0);
            objects->setCurrentRow(cityItem);
            auto placedAngle = editor.findChild<QSpinBox *>("mapPlacedCityRotation");
            QVERIFY(placedAngle && placedAngle->isVisible());
            placedAngle->setValue(180);
            QCOMPARE(project.mapEdits()->changes(1).city[0].rotation, 180);
            QTest::mousePress(canvas, Qt::LeftButton, Qt::NoModifier, pixel({150, 140}));
            QTest::mouseMove(canvas, pixel({155, 140}), 20);
            QTest::mouseRelease(canvas, Qt::LeftButton, Qt::NoModifier, pixel({155, 140}));
            QVERIFY(QLineF(project.mapEdits()->changes(1).city[0].position,
                            sourceMap.world(155, 140)).length() < .2);
            const auto movedCity = project.mapEdits()->json();
            undo.undo();
            QVERIFY(project.mapEdits()->json() != movedCity);
            undo.redo();
            QCOMPARE(project.mapEdits()->json(), movedCity);
            editor.findChild<QPushButton *>("mapCityRemove")->click();
            QVERIFY(project.mapEdits()->changes(1).city.isEmpty());
            undo.undo();
            QCOMPARE(project.mapEdits()->json(), movedCity);
            objects->setCurrentRow(cityItem);
            QTest::qWait(20);
            QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath() + "/scenario-city-inspector.png"));
            editor.resize(1100, 800);
            mapTool->setCurrentText("Scenery");
            QTest::qWait(20);
            QVERIFY(cityType->isVisible() && cityType->height() >= 42);
            QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath() + "/scenario-city-small.png"));
            editor.resize(1450, 950);
            mapTool->setCurrentIndex(0);
            QTest::qWait(20);
            pages->setCurrentIndex(0);
            objects->setCurrentRow(0);
            QTest::qWait(20);
            const auto squadSummary = editor.findChild<QLabel *>("mapSelectionSummary");
            QVERIFY(!squadSummary->isVisible());
            QVERIFY(editor.findChild<QTabWidget *>("mapSquadTabs")->isVisible());
            QVERIFY(editor.grab().save("scenario-map.png"));
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(rom);
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.scenarios()->json(), project.scenarios()->json());
            QCOMPARE(restored.world()->json(), project.world()->json());
            QCOMPARE(restored.mapEdits()->json(),project.mapEdits()->json());
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
            QCOMPARE(reopened.world()->scenario(1).treasures, project.world()->scenario(1).treasures);
            QCOMPARE(ScenarioWorld::treasureRecord(reopened.world()->scenario(1).treasures, 0),
                     treasure);
            auto reopenedMaps=MapEdits::load(reopened.rom());
            QCOMPARE(reopenedMaps.boundary(1), resizedBoundary);
            QCOMPARE(reopenedMaps.fineTerrain(1),project.mapEdits()->fineTerrain(1));
            QCOMPARE(reopenedMaps.model(1),project.mapEdits()->model(1));
            QCOMPARE(reopenedMaps.source(1).geometry[1].data,
                     editFieldHeight(project.mapEdits()->source(1), project.mapEdits()->changes(1)));
            QVERIFY(reopenedMaps.source(1).geometry[1].data != project.mapEdits()->source(1).geometry[1].data);
            QVERIFY(QLineF(reopenedMaps.points(1)[0],project.mapEdits()->points(1)[0]).length()<.00001);
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
    void scenarioSiteCountEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            project.loadScenarios();
            QUndoStack undo;
            ScenarioEditor editor(&project, &undo, [] {});
            editor.resize(1450, 950);
            editor.show();
            auto missions = editor.findChild<QListWidget *>("scenarioList");
            for (int i = 0; i < missions->count(); ++i)
                if (missions->item(i)->data(Qt::UserRole).toInt() == 64)
                    missions->setCurrentRow(i);
            auto pages = editor.findChild<QStackedWidget *>("scenarioPages");
            pages->setCurrentIndex(0);

            auto allegiance = editor.findChild<QComboBox *>("worldAllegiance");
            QCOMPARE(project.world()->siteCount(64), 6);
            const auto original = project.world()->scenario(64).sites;
            QCOMPARE(original,
                     QByteArray::fromHex("010118200201190000011a2012011b0004011c0004011d0004"));
            auto expected = original;
            for (int row = 1; row < 6; ++row) {
                QVERIFY(selectMapObject(editor, MapObject::Site, row));
                const int target = row == 2 ? 2 : 1;
                allegiance->setCurrentIndex(allegiance->findData(target));
                put16(expected, 3 + row * 4, target == 1 ? 0x2012 : 4);
                QCOMPARE(project.world()->scenario(64).sites, expected);
            }
            QCOMPARE(undo.count(), 5);
            while (undo.canUndo())
                undo.undo();
            QCOMPARE(project.world()->scenario(64).sites, original);
            while (undo.canRedo())
                undo.redo();
            QCOMPARE(project.world()->scenario(64).sites, expected);
            pages->setCurrentIndex(0);
            auto objects = editor.findChild<QListWidget *>("mapObjects");
            for (int row = 0; row < 6; ++row) {
                const auto name = project.world()->townName(u16(expected, 1 + row * 4));
                const auto matches = objects->findItems(name, Qt::MatchExactly);
                QCOMPARE(matches.size(), 1);
                objects->setCurrentItem(matches.first());
                QCOMPARE(editor.findChild<QLineEdit *>("worldSiteName")->text(), name);
                QVERIFY(editor.findChild<QWidget *>("worldSiteFields")->isVisible());
                pages->setCurrentIndex(0);
            }
            QVERIFY(editor.grab().save("scenario-64-sites.png"));
            QTemporaryDir work;
            project.saveFolder(work.filePath("project"));
            auto restored = Project::fromRom(rom);
            restored.applyManifest(Project::readProject(work.filePath("project")));
            QCOMPARE(restored.world()->scenario(64).sites, expected);
            const auto result = rebuild(restored, tools, work.path());
            const auto root = QDir(QFileInfo(path).absolutePath()).filePath("Test ROMs");
            QVERIFY(QDir().mkpath(root));
            QTemporaryDir output(QDir(root).filePath("native-site-count-XXXXXX"));
            QVERIFY(output.isValid());
            const auto candidate = output.filePath(QFileInfo(path).fileName());
            exportRom(restored, result, candidate);
            const auto reopenedRom = Rom::open(candidate);
            const auto reopened = ScenarioWorld::load(reopenedRom);
            QCOMPARE(reopened.siteCount(64), 6);
            QCOMPARE(reopened.scenario(64).sites, expected);
            const auto resources = WorldResources::load(reopenedRom);
            const auto allocation = resources.placement();
            QVERIFY(allocation);
            QVERIFY(quint64(allocation->offset) + allocation->allocation <=
                    quint64(reopenedRom.bytes().size()));
            QCOMPARE(resources.sites(reopenedRom, 64), expected);
            for (int key = 1; key < 64; ++key)
                QCOMPARE(reopened.scenario(key), project.world()->scenario(key));
            auto checksum = result.image;
            Rom::updateChecksum(checksum);
            QCOMPARE(checksum, result.image);
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
            output.setAutoRemove(false);
            qInfo().noquote() << "Site count export:" << candidate;
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
        editor.findChild<QStackedWidget *>("scenarioPages")->setCurrentIndex(0);
        auto button = [&](const char *name) { return editor.findChild<QPushButton *>(name); };
        auto spin = [&](const char *name) { return editor.findChild<QSpinBox *>(name); };
        auto missions = editor.findChild<QListWidget *>("scenarioList");
        auto objects = editor.findChild<QListWidget *>("mapObjects");
        auto treasureCount = [&] { return ScenarioWorld::treasureCount(project.world()->scenario(52).treasures); };
        const auto base = project.world()->json();
        int penaltyKey = -1, penaltyRow = -1;
        for (int i = 0; i < missions->count(); ++i) {
            if (!(missions->item(i)->flags() & Qt::ItemIsEnabled)) continue;
            missions->setCurrentRow(i);
            const int key = missions->currentItem()->data(Qt::UserRole).toInt();
            for (int row = 0; row < project.world()->siteCount(key); ++row) {
                QVERIFY(selectMapObject(editor, MapObject::Site, row));
                QCOMPARE(project.world()->json(), base);
                const int id = u16(project.world()->scenario(key).sites, 1 + row * 4);
                const int packed = u8(project.world()->town(id), 24);
                if (packed != 255) {
                    QCOMPARE(spin("worldMorale")->value(), packed & 127);
                    QCOMPARE(editor.findChild<QCheckBox *>("worldTakeoverPenalty")->isChecked(), bool(packed & 128));
                    if (packed & 128) { penaltyKey = key; penaltyRow = row; }
                }
            }
            for (int row = 0; row < ScenarioWorld::treasureCount(project.world()->scenario(key).treasures); ++row) {
                QVERIFY(selectMapObject(editor, MapObject::Treasure, row));
                QCOMPARE(project.world()->json(), base);
            }
        }
        QCOMPARE(undo.count(), 0);
        QVERIFY(penaltyKey > 0);
        for (int i = 0; i < missions->count(); ++i)
            if (missions->item(i)->data(Qt::UserRole).toInt() == penaltyKey) missions->setCurrentRow(i);
        QVERIFY(selectMapObject(editor, MapObject::Site, penaltyRow));
        const int penaltyId = u16(project.world()->scenario(penaltyKey).sites, 1 + penaltyRow * 4);
        auto penalty = editor.findChild<QCheckBox *>("worldTakeoverPenalty");
        auto objectiveFlag = editor.findChild<QCheckBox *>("worldObjective");
        const auto originalPenaltyTown = project.world()->town(penaltyId);
        const int changedMorale = (u8(originalPenaltyTown, 24) & 127) == 60 ? 61 : 60;
        spin("worldMorale")->setValue(changedMorale);
        QCOMPARE(u8(project.world()->town(penaltyId), 24), changedMorale | 128);
        penalty->setChecked(false);
        QCOMPARE(u8(project.world()->town(penaltyId), 24), changedMorale);
        undo.undo();
        QCOMPARE(u8(project.world()->town(penaltyId), 24), changedMorale | 128);
        undo.redo();
        QCOMPARE(u8(project.world()->town(penaltyId), 24), changedMorale);
        penalty->setChecked(true);
        objectiveFlag->setChecked(true);
        QCOMPARE(u8(project.world()->town(penaltyId), 24), 255);
        QVERIFY(!penalty->isEnabled());
        objectiveFlag->setChecked(false);
        QCOMPARE(u8(project.world()->town(penaltyId), 24), changedMorale | 128);
        while (undo.canUndo()) undo.undo();
        QCOMPARE(project.world()->json(), base);
        for (int i = 0; i < missions->count(); ++i)
            if (missions->item(i)->data(Qt::UserRole).toInt() == 52)
                missions->setCurrentRow(i);
        QVERIFY(selectMapObject(editor, MapObject::Site, 0));
        const int id = u16(project.world()->scenario(52).sites, 1);
        const auto beforeShop = project.manifest();
        const int shopCount = project.shops()->size();
        const int assigned = u8(project.world()->town(id), 27) == 1 ? 2 : 1;
        spin("worldShop")->setValue(assigned);
        QCOMPARE(project.shops()->size(), shopCount);
        QCOMPARE(u8(project.world()->town(id), 27), assigned);
        editor.findChild<QCheckBox *>("worldFacility0")->setChecked(false);
        QCOMPARE(u8(project.world()->town(id), 27), 0);
        undo.undo();
        QCOMPARE(u8(project.world()->town(id), 27), assigned);
        undo.undo();
        QCOMPARE(project.manifest(), beforeShop);
        auto den = editor.findChild<QCheckBox *>("worldFacility1");
        const auto oldFlags = u8(project.world()->town(id), 25);
        den->setChecked(!(oldFlags & 2));
        QCOMPARE(u8(project.world()->town(id), 25), quint8(oldFlags & 2 ? oldFlags & ~2 : (oldFlags & ~1) | 2));
        undo.undo();
        QCOMPARE(project.manifest(), beforeShop);
        const int beforePopulation = undo.index();
        spin("worldPopulation")->setValue(4567);
        auto allegiance = editor.findChild<QComboBox *>("worldAllegiance");
        allegiance->setCurrentIndex(allegiance->findData(2));
        QCOMPARE(u16(project.world()->town(id), 22), quint16(4567));
        QCOMPARE(u16(project.world()->scenario(52).sites, 3), quint16(4));
        const int afterAllegiance = undo.index();
        undo.setIndex(beforePopulation);
        QCOMPARE(project.world()->json(), base);
        undo.setIndex(afterAllegiance);
        auto objective = editor.findChild<QCheckBox *>("worldObjective");
        objective->setChecked(true);
        QCOMPARE(u8(project.world()->town(id), 24), quint8(255));
        QVERIFY(!spin("worldMorale")->isEnabled());
        editor.grab().save("scenario-sites.png");
        editor.resize(1100, 800);
        QTest::qWait(20);
        QVERIFY(editor.grab().save("map-inspector-site-1100.png"));
        editor.resize(1450, 950);
        QTest::qWait(20);
        const int count = treasureCount();
        auto canvas = editor.findChild<QWidget *>("scenarioMapCanvas");
        button("mapTreasureAdd")->click();
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, canvas->rect().center());
        QCOMPARE(treasureCount(), count + 1);
        const int row = objects->currentItem()->data(Qt::UserRole + 1).toInt();
        QCOMPARE(objects->currentItem()->data(Qt::UserRole).toInt(), int(MapObject::Treasure));
        QVERIFY(editor.findChild<QWidget *>("worldTreasureFields")->isVisible());
        spin("worldTreasureX")->setValue(255);
        spin("worldTreasureZ")->setValue(0);
        auto table = editor.findChild<QComboBox *>("worldTreasureTable");
        table->setCurrentIndex(table->findData(1));
        auto item = editor.findChild<QComboBox *>("worldTreasureItem");
        item->setCurrentIndex(item->findData(277));
        const auto record =
            ScenarioWorld::treasureRecord(project.world()->scenario(52).treasures, row);
        QCOMPARE(u16(record, 4), quint16(277));
        QCOMPARE(u8(record, 1), quint8(255));
        editor.grab().save("scenario-treasure.png");
        button("worldTreasureRemove")->click();
        QCOMPARE(treasureCount(), count);
        undo.undo();
        QCOMPARE(treasureCount(), count + 1);
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
        editor.findChild<QStackedWidget *>("scenarioPages")->setCurrentIndex(2);
        auto routes = editor.findChild<QWidget *>("scenarioRoutesEditor");
        QVERIFY(routes);
        auto button = [&](const char *name) { return routes->findChild<QPushButton *>(name); };
        auto spin = [&](const char *name) { return routes->findChild<QSpinBox *>(name); };
        auto select = [&](const char *name, int id) {
            auto box = routes->findChild<QComboBox *>(name);
            QVERIFY(box);
            const int at = box->findData(id);
            QVERIFY(at >= 0);
            box->setCurrentIndex(at);
        };
        auto missions = editor.findChild<QListWidget *>("scenarioList");
        auto nodes = editor.findChild<QListWidget *>("scenarioNodes");
        auto conditions = editor.findChild<QListWidget *>("scenarioConditions");
        auto tabs = editor.findChild<QTabWidget *>("scenarioRouteTabs");
        editor.findChild<QCheckBox *>("scenarioRouteAdvanced")->setChecked(true);
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
        const auto beforeRoute = project.scenarios()->json();
        select("behaviorSquad", 0);
        const int nodeId = u8(project.scenarios()->scenario(10).layout.nodes.last(), 0);
        select("behaviorStart", nodeId);
        button("behaviorAssign")->click();
        QCOMPARE(u8(project.scenarios()->scenario(10).layout.deployments[0], 6), quint8(nodeId));
        QVERIFY(editor.findChild<QTextBrowser *>("behaviorSummary")->toPlainText().contains("map position 80, 160"));
        undo.undo(); QCOMPARE(project.scenarios()->json(), beforeRoute);
        undo.redo();
        QTest::qWait(50);
        QVERIFY(
            editor.grab().save(QCoreApplication::applicationDirPath() + "/scenario-routes.png"));
        const auto scripted = project.scenarios()->scenario(10).layout.nodes;
        editor.findChild<QCheckBox *>("scenarioRouteAdvanced")->setChecked(false);
        select("squadCommand", int(SquadBehavior::MoveAndAttack));
        select("squadCommandDestination", 0);
        spin("squadCommandX")->setValue(96);
        spin("squadCommandZ")->setValue(144);
        button("squadCommandApply")->click();
        const auto command = project.scenarios()->squadCommand(10, 0);
        QVERIFY(command);
        QCOMPARE(command->behavior, SquadBehavior::MoveAndAttack);
        QCOMPARE(command->x, 96);
        QCOMPARE(command->z, 144);
        QCOMPARE(project.scenarios()->scenario(10).layout.nodes.first(scripted.size()), scripted);
        const auto commanded = project.scenarios()->json();
        undo.undo();
        QCOMPARE(project.scenarios()->scenario(10).layout.nodes, scripted);
        undo.redo();
        QCOMPARE(project.scenarios()->json(), commanded);
        for (int i = 0; i < missions->count(); ++i)
            if (missions->item(i)->data(Qt::UserRole).toInt() == 1) missions->setCurrentRow(i);
        select("behaviorSquad", 0);
        select("squadCommand", int(SquadBehavior::MoveAndAttack));
        select("squadCommandDestination", 1);
        const auto destination = routes->findChild<QComboBox *>("squadCommandDestination");
        const int town = u16(project.world()->scenario(1).sites, 1);
        QCOMPARE(destination->currentText(), project.world()->townName(town));
        button("squadCommandApply")->click();
        const auto townCommand = project.scenarios()->squadCommand(1, 0);
        QVERIFY(townCommand);
        QCOMPARE(townCommand->x, 1);
        QCOMPARE(townCommand->z, 0);
        QTest::qWait(50);
        QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath() + "/scenario-commands.png"));
    }
    void trainingEditing() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));QUndoStack undo;ToolsEditor tools(&project,&undo,[]{});
        tools.resize(1100,780);tools.show();auto pages=tools.findChild<QTabWidget *>("toolsPages");
        for(int i=0;i<pages->count();++i)if(pages->tabText(i)=="Training")pages->setCurrentIndex(i);
        auto location=tools.findChild<QComboBox *>("trainingLocation");location->setCurrentIndex(location->findData(7));
        auto price=tools.findChild<QSpinBox *>("trainingPrice");QCOMPARE(price->value(),15);price->setValue(0);
        QCOMPARE(tools.findChild<QLabel *>("trainingFee")->text(),QString("0 Goth"));undo.undo();QCOMPARE(price->value(),15);undo.redo();QCOMPARE(price->value(),0);
        auto preview=tools.findChild<QSpinBox *>("trainingPreviewLevel");preview->setValue(41);price->setValue(20);
        QCOMPARE(tools.findChild<QLabel *>("trainingFee")->text(),QString("1260 Goth"));
        QTest::qWait(30);QVERIFY(tools.grab().save("native/build/training-editor.png"));
    }
    void unfinishedSpriteAutosave() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        QStandardPaths::setTestModeEnabled(true);struct Restore {~Restore(){QStandardPaths::setTestModeEnabled(false);}} restore;
        RecoveryStore store;QTemporaryDir savedCopy;
        MainWindow window;window.loadRom(path);
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QPushButton *>("spriteNew"),60000);
        QVERIFY(!window.isWindowModified());
        QString latest;QJsonObject recoveredManifest;
        QTimer::singleShot(0,&window,[&]{
            auto dialog=window.findChild<QDialog *>("spriteCreateDialog");QVERIFY(dialog);
            QTimer::singleShot(30000,dialog,&QDialog::reject);
            auto name=dialog->findChild<QLineEdit *>("spriteCreateName");QVERIFY(name);name->setText("First unfinished sprite");
            auto timer=window.findChild<QTimer *>("recoveryTimer");
            QMetaObject::invokeMethod(timer,"timeout",Qt::DirectConnection);
            QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QLabel *>("recoveryStatus")->text().contains("unfinished art included"),15000);
            QVERIFY(!store.entries().isEmpty());latest=store.entries().front().path;
            const auto first=store.load(latest);QCOMPARE(first["editorRecoveryDrafts"].toArray().size(),1);
            QVERIFY(!store.entries().front().drafts.isEmpty());
            name->setText("Second unfinished sprite");
            QMetaObject::invokeMethod(timer,"timeout",Qt::DirectConnection);
            QTRY_VERIFY_WITH_TIMEOUT(store.entries().front().path!=latest,15000);
            latest=store.entries().front().path;recoveredManifest=store.load(latest);
            QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QLabel *>("recoveryStatus")->text().contains("unfinished art included"),15000);
            QMetaObject::invokeMethod(timer,"timeout",Qt::DirectConnection);QTest::qWait(150);
            QCOMPARE(store.entries().front().path,latest); // No new copy for an unchanged draft.
            QVERIFY(QFile::copy(latest,savedCopy.filePath("unfinished.lcrecovery")));
            dialog->reject();
        });
        window.findChild<QPushButton *>("spriteNew")->click();
        QVERIFY(!latest.isEmpty());QTRY_VERIFY_WITH_TIMEOUT(!QFile::exists(latest),15000);
        QVERIFY(!window.isWindowModified());
        MainWindow recovered;QVERIFY(recovered.openRecoveryFile(savedCopy.filePath("unfinished.lcrecovery"),path));
        auto assets=recovered.findChild<QListWidget *>("spriteAssets");QVERIFY(assets);QCOMPARE(assets->count(),1);
        QVERIFY(assets->item(0)->text().contains("Second unfinished sprite"));QVERIFY(recovered.isWindowModified());
        auto undo=recovered.findChild<QUndoGroup *>();undo->undo();QCOMPARE(assets->count(),0);QVERIFY(recovered.isWindowModified());
        undo->redo();QCOMPARE(assets->count(),1);
    }
    void automaticRecoveryLifecycle() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        QStandardPaths::setTestModeEnabled(true);
        struct Restore {~Restore(){QStandardPaths::setTestModeEnabled(false);}} restore;
        RecoveryStore store;const auto previous=store.entries();QString saved,session;
        {
            MainWindow window;window.loadRom(path);
            QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QSpinBox *>("rule_powerUpStrength"),60000);
            window.findChild<QSpinBox *>("rule_powerUpStrength")->setValue(25);
            auto timer=window.findChild<QTimer *>("recoveryTimer");QVERIFY(timer);
            QVERIFY(QMetaObject::invokeMethod(timer,"timeout",Qt::DirectConnection));
            QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QLabel *>("recoveryStatus")->text().startsWith("Recovery saved"),30000);
            for(const auto &entry:store.entries()) {
                bool old=false;for(const auto &before:previous)if(before.path==entry.path)old=true;
                if(!old){saved=entry.path;session=entry.session;break;}
            }
            QVERIFY(!saved.isEmpty());
        } // Simulates an interrupted window without running its deliberate-close cleanup.
        MainWindow recovered;QVERIFY(recovered.openRecoveryFile(saved,path));
        QCOMPARE(recovered.findChild<QSpinBox *>("rule_powerUpStrength")->value(),25);QVERIFY(recovered.isWindowModified());
        QVERIFY(recovered.findChild<QAction *>("openCrashLogs"));QVERIFY(recovered.findChild<QAction *>("recoverProject"));
        recovered.show();QTest::qWait(50);QVERIFY(recovered.grab().save("native/build/recovered-project.png"));
        recovered.findChild<QSpinBox *>("rule_powerUpStrength")->setValue(50);
        QMetaObject::invokeMethod(recovered.findChild<QTimer *>("recoveryTimer"),"timeout",Qt::DirectConnection);
        QTimer::singleShot(0,[]{if(auto dialog=qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))dialog->button(QMessageBox::Discard)->click();});
        QVERIFY(recovered.close());
        QTRY_VERIFY_WITH_TIMEOUT(!QFile::exists(saved),30000);
        QTest::qWait(500);
        QTRY_COMPARE_WITH_TIMEOUT(store.entries().size(),previous.size(),30000);
        for(const auto &entry:store.entries())QVERIFY(entry.session!=session);
    }
    void statusEffectRules() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));QUndoStack undo;
        ToolsEditor tools(&project,&undo,[]{});tools.resize(1440,1000);tools.show();
        auto pages=tools.findChild<QTabWidget *>("toolsPages");
        for(int i=0;i<pages->count();++i)if(pages->tabText(i)=="Status effects")pages->setCurrentIndex(i);
        QCOMPARE(pages->tabText(pages->currentIndex()),QString("Status effects"));QVERIFY(undo.isClean());
        auto spin=[&](const char *key){return tools.findChild<QSpinBox *>(QString("rule_")+key);};
        QCOMPARE(spin("sleepRecoveryChance")->text(),QString("Luck-based (retail)"));
        spin("sleepRecoveryChance")->setValue(0);QCOMPARE(project.tools()->gameRules().value("sleepRecoveryChance"),0);
        undo.undo();QCOMPARE(spin("sleepRecoveryChance")->value(),-1);undo.redo();
        tools.findChild<QPushButton *>("ruleReset_sleepRecoveryChance")->click();QCOMPARE(spin("sleepRecoveryChance")->value(),-1);
        spin("poisonStrength")->setValue(25);
        QVERIFY(tools.findChild<QLabel *>("ruleExample_statusPoison")->text().contains("150"));
        spin("powerUpStrength")->setValue(50);
        QVERIFY(tools.findChild<QLabel *>("ruleExample_statusPowerUp")->text().contains("400"));
        const auto stone=tools.findChild<QWidget *>("ruleGroup_statusPetrification");QVERIFY(stone);
        QVERIFY(stone->findChildren<QSpinBox *>().isEmpty());
        for(const auto name:{"statusPoison","statusPowerUp","statusPowerDown","statusSleep","statusParalysis","statusPetrification"}) {
            auto box=tools.findChild<QWidget *>(QString("ruleGroup_")+name);QVERIFY(box);
            QTest::qWait(20);QVERIFY(box->grab().save(QString("native/build/status-%1.png").arg(name)));
        }
    }
    void ruleStrengthExplanations() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));QUndoStack undo;
        ToolsEditor tools(&project,&undo,[]{});tools.resize(1100,850);tools.show();
        auto spin=[&](const char *key){return tools.findChild<QSpinBox *>(QString("rule_")+key);};
        auto example=[&](const char *group){return tools.findChild<QLabel *>(QString("ruleExample_")+group)->text();};
        QVERIFY(spin("alignmentCap") && spin("restHealing"));
        QCOMPARE(spin("alignmentCap")->value(),5);QCOMPARE(spin("restHealing")->value(),1);
        spin("alignmentCap")->setValue(10);QCOMPARE(project.tools()->gameRules().value("alignmentCap"),10);
        QVERIFY(example("tactics").contains("+8 becomes +8"));
        spin("alignmentCap")->setValue(0);QVERIFY(example("tactics").contains("disabled"));
        tools.findChild<QPushButton *>("ruleReset_alignmentCap")->click();QCOMPARE(spin("alignmentCap")->value(),5);
        undo.undo();QCOMPARE(spin("alignmentCap")->value(),0);undo.redo();QCOMPARE(spin("alignmentCap")->value(),5);
        spin("weaponAffinity")->setValue(10);QVERIFY(example("weapons").contains("110%"));QVERIFY(example("weapons").contains("90%"));
        undo.undo();QVERIFY(example("weapons").contains("102%"));QVERIFY(example("weapons").contains("98%"));
        spin("elementAffinity")->setValue(8);QVERIFY(example("elements").contains("108%"));QVERIFY(example("elements").contains("92%"));
        QCOMPARE(spin("elementAffinity")->suffix(),QString(" points"));
        spin("resistanceStrength")->setValue(0);QVERIFY(example("resistance").contains("Current setting (0%): about 100 damage"));
        spin("resistanceStrength")->setValue(200);QVERIFY(example("resistance").contains("Current setting (200%): about 20 damage"));
        spin("resistanceStrength")->setValue(400);QVERIFY(example("resistance").contains("Current setting (400%): about 1 damage"));
        undo.undo();QVERIFY(example("resistance").contains("Current setting (200%): about 20 damage"));
        tools.findChild<QPushButton *>("ruleReset_resistanceStrength")->click();
        QVERIFY(example("resistance").contains("Current setting (100%): about 60 damage"));
        spin("restHealing")->setValue(5);spin("recoveryRate")->setValue(200);
        QCOMPARE(project.tools()->gameRules().value("restHealing"),5);QVERIFY(example("recovery").contains("10Ã—"));
        spin("restHealing")->setValue(0);QVERIFY(example("recovery").contains("HP healing is disabled"));
        undo.undo();QCOMPARE(spin("restHealing")->value(),5);QVERIFY(example("recovery").contains("10Ã—"));
        tools.findChild<QPushButton *>("ruleReset_restHealing")->click();QCOMPARE(spin("restHealing")->value(),1);
        QTest::qWait(30);QVERIFY(tools.grab().save("native/build/rule-strength-tools.png"));
        for(const auto group:{"tactics","elements","weapons","recovery","resistance"}) {
            auto box=tools.findChild<QWidget *>(QString("ruleGroup_")+group);QVERIFY(box);
            QVERIFY(box->grab().save(QString("native/build/rule-strength-%1.png").arg(group)));
        }
        auto foreign=project.rom().bytes();foreign[0x23336e]^=1;foreign[0x133bee]^=1;
        auto unknown=Project::fromRom(Rom::fromBytes(foreign));QUndoStack unknownUndo;
        ToolsEditor unavailable(&unknown,&unknownUndo,[]{});
        QVERIFY(!unavailable.findChild<QSpinBox *>("rule_alignmentCap")->isEnabled());
        QVERIFY(!unavailable.findChild<QSpinBox *>("rule_restHealing")->isEnabled());
        QVERIFY(unavailable.findChild<QLabel *>("ruleExample_tactics")->text().contains("unavailable"));
        QVERIFY(unavailable.findChild<QLabel *>("ruleExample_recovery")->text().contains("unavailable"));
    }
    void gameRulesFullWorkspaceSteps() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");
        if(path.isEmpty())QSKIP("Private ROM required");
        MainWindow window;window.resize(1100,780);window.loadRom(path);window.show();
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QSpinBox *>("rule_deploymentLimit"),60000);
        auto navigation=window.findChild<QTabBar *>("workspaceNavigation");QVERIFY(navigation);
        for(int i=0;i<navigation->count();++i)if(navigation->tabText(i)=="Tools")navigation->setCurrentIndex(i);
        auto spin=window.findChild<QSpinBox *>("rule_deploymentLimit");QVERIFY(spin);
        auto scenarios=window.findChild<QListWidget *>("scenarioList");QVERIFY(scenarios);
        QSignalSpy resets(scenarios->model(),&QAbstractItemModel::modelReset);
        spin->setFocus();QElapsedTimer elapsed;elapsed.start();
        for(int i=0;i<10;++i)QTest::keyClick(spin,Qt::Key_Up);
        qInfo()<<"Ten deployment arrow presses:"<<elapsed.elapsed()<<"ms; unrelated scenario resets:"<<resets.count();
        QCOMPARE(spin->value(),20);
        auto undo=window.findChild<QUndoStack *>();QVERIFY(undo);
        undo->undo();QCOMPARE(spin->value(),19);undo->redo();QCOMPARE(spin->value(),20);
        QCOMPARE(resets.count(),0);
        undo->setClean();
    }
    void gameRulesEditing() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));project.loadScenarios();project.loadTools();
        const auto before=project.tools()->json();QUndoStack undo;
        ToolsEditor tools(&project,&undo,[]{});tools.resize(1100,780);tools.show();
        auto tactic=tools.findChild<QSpinBox *>("rule_tacticLeader");QVERIFY(tactic);tactic->setValue(-3);
        QCOMPARE(project.tools()->gameRules().value("tacticLeader"),-3);
        auto fatigue=tools.findChild<QSpinBox *>("rule_fatigueRate");fatigue->setValue(200);
        QCOMPARE(project.tools()->gameRules().value("fatigueRate"),200);
        undo.undo();undo.undo();QCOMPARE(project.tools()->json(),before);undo.redo();undo.redo();
        ScenarioEditor scenario(&project,&undo,[]{});scenario.resize(1100,780);scenario.show();
        scenario.findChild<QTabBar *>("scenarioNavigation")->setCurrentIndex(3);
        auto day=scenario.findChild<QSpinBox *>("rule_dayLength");QVERIFY(day);day->setValue(150);
        QCOMPARE(project.tools()->gameRules().value("dayLength"),150);
        undo.undo();QCOMPARE(day->value(),100);undo.redo();QCOMPARE(day->value(),150);
        auto reserve=tools.findChild<QSpinBox *>("rule_reserveXp");
        auto deployment=tools.findChild<QSpinBox *>("rule_deploymentLimit");
        QVERIFY(reserve && deployment); reserve->setValue(25); deployment->setValue(30);
        QCOMPARE(project.tools()->gameRules().value("reserveXp"),25);
        QCOMPARE(deployment->maximum(),20);
        QCOMPARE(project.tools()->gameRules().value("deploymentLimit"),20);
        undo.undo(); QCOMPARE(deployment->value(),10); undo.undo(); QCOMPARE(reserve->value(),0);
        undo.redo(); undo.redo();
        tools.findChild<QPushButton *>("ruleReset_reserveXp")->click(); QCOMPARE(reserve->value(),0);
        tools.findChild<QPushButton *>("ruleReset_deploymentLimit")->click(); QCOMPARE(deployment->value(),10);
        tools.findChild<QPushButton *>("ruleReset_tacticLeader")->click();
        QCOMPARE(project.tools()->gameRules().value("tacticLeader"),1);
        QTest::qWait(20);QVERIFY(scenario.grab().save("native/build/scenario-clock-rules.png"));
        QVERIFY(tools.grab().save("native/build/tools-game-rules.png"));
        tools.findChild<QTabWidget *>("toolsPages")->setCurrentIndex(2);QTest::qWait(20);
        QVERIFY(tools.grab().save("native/build/tools-mission-rewards.png"));
        auto expanded=*project.scenarios();
        while(expanded.scenario(1).squads.size()<30)expanded.addSquad(1,0,project.classData());
        project.setScenarios(expanded.json());QVERIFY(project.expandedArmies());
        ToolsEditor capacityTools(&project,&undo,[]{});
        auto capped=capacityTools.findChild<QSpinBox *>("rule_deploymentLimit");QVERIFY(capped);
        QCOMPARE(capped->maximum(),20);capped->setValue(30);
        QCOMPARE(project.tools()->gameRules().value("deploymentLimit"),20);
        undo.undo();QCOMPARE(capped->value(),10);undo.redo();QCOMPARE(capped->value(),20);
        // Older settings remain visible and recoverable, but cannot be edited
        // above20. Correcting them is an explicit undoable action.
        auto legacy=project.tools()->json(),legacyRules=legacy["gameRules"].toObject();
        legacyRules["deploymentLimit"]=23;legacy["gameRules"]=legacyRules;project.setTools(legacy);
        QUndoStack legacyUndo;ToolsEditor legacyTools(&project,&legacyUndo,[]{});
        auto oldLimit=legacyTools.findChild<QSpinBox *>("rule_deploymentLimit");QVERIFY(oldLimit);
        QCOMPARE(oldLimit->value(),23);QVERIFY(!oldLimit->isEnabled());
        auto correction=legacyTools.findChild<QPushButton *>("ruleFix_deploymentLimit");QVERIFY(correction);
        correction->click();QCOMPARE(oldLimit->value(),20);QCOMPARE(oldLimit->maximum(),20);QVERIFY(oldLimit->isEnabled());
        legacyUndo.undo();QCOMPARE(oldLimit->value(),23);QVERIFY(!oldLimit->isEnabled());
        legacyUndo.redo();QCOMPARE(oldLimit->value(),20);QVERIFY(oldLimit->isEnabled());
    }
    void scenarioEventSemanticLabels() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path)); project.loadSceneEvents();
            const auto initial = project.manifest();
            const auto bytes = project.sceneEvents()->program(10);
            QUndoStack undo;
            ScenarioEventEditor editor(&project, &undo, [] {});
            editor.setRawInstructions(true);
            editor.resize(1200, 800); editor.showAllRows(); editor.show();
            editor.selectEventRow(10, 0x424); QApplication::processEvents();
            auto tree = editor.findChild<QTreeWidget *>("scenarioEventSteps");
            auto flags = editor.findChild<QComboBox *>("scenarioEventFlag");
            auto action = editor.findChild<QComboBox *>("scenarioEventAction");
            QVERIFY(tree && flags && action && tree->currentItem());
            QCOMPARE(tree->currentItem()->text(1), QString("SET Flag 98 (Asnabel recruitment recorded) = ON"));
            QCOMPARE(flags->itemText(flags->findData(202)), QString("Flag 202 (Katreda encountered this mission)"));
            QCOMPARE(project.manifest(), initial); QCOMPARE(undo.count(), 0);
            QCOMPARE(project.sceneEvents()->program(10), bytes);
            flags->setCurrentIndex(flags->findData(250));
            action->setCurrentIndex(action->findData(false));
            editor.findChild<QPushButton *>("scenarioEventApply")->click();
            QCOMPARE(tree->currentItem()->text(1), QString("CLEAR Flag 250 = OFF"));
            const auto edited = project.sceneEvents()->program(10);
            auto expected = bytes; put8(expected, 0x425, 250); expected[0x427] = 0;
            QCOMPARE(edited, expected);
            undo.undo(); QApplication::processEvents();
            QCOMPARE(tree->currentItem()->text(1), QString("SET Flag 98 (Asnabel recruitment recorded) = ON"));
            QCOMPARE(project.manifest(), initial);
            undo.redo(); QApplication::processEvents();
            QCOMPARE(tree->currentItem()->text(1), QString("CLEAR Flag 250 = OFF"));
            QTemporaryDir folder; project.saveFolder(folder.path());
            auto reopened = Project::fromRom(Rom::open(path));
            reopened.applyManifest(Project::readProject(folder.path())); reopened.loadSceneEvents();
            QCOMPARE(reopened.sceneEvents()->program(10), edited);
            editor.selectEventRow(77);
            QCOMPARE(flags->itemText(flags->findData(202)), QString("Flag 202"));
            auto rows = editor.findChild<QComboBox *>("scenarioEventRow");
            editor.selectEventRow(113);
            QVERIFY(editor.findChild<QCheckBox *>("scenarioEventOtherPrograms")->isChecked());
            QCOMPARE(rows->currentData().toInt(), 113);
            QVERIFY(tree->topLevelItemCount() > 0);
            editor.selectEventRow(10, 0xa4);
            QVERIFY(tree->currentItem()->text(1).contains("527"));
            QVERIFY(tree->currentItem()->text(1).contains("Katreda"));
            undo.undo(); QApplication::processEvents();
            QVERIFY(editor.grab().save("native/build/event-row-labels.png"));
            QCOMPARE(project.manifest(), initial);
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void scenarioEventResearchRows() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path)); project.loadSceneEvents();
            const auto initial = project.manifest();
            QUndoStack undo;
            ScenarioEventEditor editor(&project, &undo, [] {});
            editor.setRawInstructions(true);
            editor.resize(1440, 950); editor.showAllRows(); editor.show();
            editor.findChild<QCheckBox *>("scenarioEventOtherPrograms")->setChecked(true);
            auto rows = editor.findChild<QComboBox *>("scenarioEventRow");
            auto tree = editor.findChild<QTreeWidget *>("scenarioEventSteps");
            QCOMPARE(rows->count(), 113);
            for (int row : {0, 1, 66, 67, 68, 70, 114}) {
                editor.selectEventRow(row); QApplication::processEvents();
                QCOMPARE(rows->currentData().toInt(), row);
                QVERIFY(tree->topLevelItemCount() > 0);
            }
            const auto directory = qEnvironmentVariable("LORDLY_EVENT_SCREENSHOTS");
            const QList<QPair<int,int>> samples{{28, 318}, {62, 654}, {89, 72}};
            for (const auto &sample : samples) {
                editor.selectEventRow(sample.first, sample.second); QApplication::processEvents();
                QVERIFY(tree->currentItem());
                QCOMPARE(tree->currentItem()->data(0, Qt::UserRole).toInt(), sample.second);
                if (sample.first == 28)
                    QVERIFY(tree->currentItem()->text(1).contains("R1 (flag to check) = 201"));
                // Include working flag controls as well as read-only register
                // instructions in the representative captures.
                tree->scrollToItem(sample.first == 89 ? tree->currentItem()->parent() : tree->currentItem(),
                    QAbstractItemView::PositionAtTop);
                QApplication::processEvents();
                if (sample.first == 89) {
                    auto handler = tree->currentItem()->parent();
                    bool wrapped = false;
                    for (int n = 0; n < handler->childCount(); ++n) {
                        auto item = handler->child(n);
                        if (editor.fontMetrics().horizontalAdvance(item->text(1)) > tree->columnWidth(1)) {
                            QVERIFY(tree->visualItemRect(item).height() > editor.fontMetrics().height() + 6);
                            wrapped = true; break;
                        }
                    }
                    QVERIFY(wrapped);
                }
                if (!directory.isEmpty()) {
                    QDir().mkpath(directory);
                    QVERIFY(editor.grab().save(QDir(directory).filePath(QString("event-row-%1.png").arg(sample.first))));
                }
            }
            QCOMPARE(project.manifest(), initial); QCOMPARE(undo.count(), 0);
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void scenarioEventFlags() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        try {
        auto project = Project::fromRom(Rom::open(path)); project.loadScenarios();
        QUndoStack undo;
        ScenarioEditor editor(&project, &undo, [] {}); editor.resize(1300, 800); editor.show();
        auto list = editor.findChild<QListWidget *>("scenarioList");
        for (int i = 0; i < list->count(); ++i) if (list->item(i)->data(Qt::UserRole).toInt() == 1) list->setCurrentRow(i);
        editor.findChild<QTabBar *>("scenarioNavigation")->setCurrentIndex(4);
        QApplication::processEvents();
        auto rows = editor.findChild<QComboBox *>("scenarioEventRow");
        QVERIFY(rows); QCOMPARE(rows->currentData().toInt(), 4); QCOMPARE(rows->count(), 1);
        editor.findChild<QCheckBox *>("scenarioEventRaw")->setChecked(true);
        auto tree = editor.findChild<QTreeWidget *>("scenarioEventSteps");
        auto select = [&](int offset) {
            for (int h = 0; h < tree->topLevelItemCount(); ++h)
                for (int n = 0; n < tree->topLevelItem(h)->childCount(); ++n) {
                    auto item = tree->topLevelItem(h)->child(n);
                    if (item->data(0, Qt::UserRole).toInt() == offset) { tree->setCurrentItem(item); tree->scrollToItem(item); return true; }
                }
            return false;
        };
        const auto original = project.sceneEvents()->json();
        QVERIFY(select(0x2f0)); QVERIFY(tree->currentItem()->text(1).contains("Tenne Plains: The Southern Reaches completed"));
        auto flags = editor.findChild<QComboBox *>("scenarioEventFlag");
        auto action = editor.findChild<QComboBox *>("scenarioEventAction");
        flags->setCurrentIndex(flags->findData(250)); action->setCurrentIndex(action->findData(false));
        editor.findChild<QPushButton *>("scenarioEventApply")->click();
        QCOMPARE(u8(project.sceneEvents()->program(4), 0x2f1), quint8(250));
        QCOMPARE(u8(project.sceneEvents()->program(4), 0x2f3), quint8(0));
        QVERIFY(tree->currentItem()->text(1).contains("CLEAR Flag 250 = OFF"));
        undo.undo(); QCOMPARE(project.sceneEvents()->json(), original);
        undo.redo(); QVERIFY(select(0x2f0));
        editor.findChild<QPushButton *>("scenarioEventRestore")->click(); QCOMPARE(project.sceneEvents()->json(), original);
        undo.undo(); QCOMPARE(flags->currentData().toInt(), 250);
        flags->setEditText("not a valid flag");
        const int count = undo.index(); editor.findChild<QPushButton *>("scenarioEventApply")->click(); QCOMPARE(undo.index(), count);
        QVERIFY(editor.findChild<QLabel *>("scenarioEventNotice")->text().contains("Choose a listed flag"));
        QVERIFY(select(0x2f0));
        // A real conditional check changes only its branch sense, not its destination.
        const auto controls = EventScript::flagSteps(SceneEvents::retailRow(project.rom(), 4));
        int check = -1;
        for (auto it = controls.begin(); it != controls.end(); ++it) if (it->check) { check = it.key(); break; }
        QVERIFY(check >= 0); QVERIFY(select(check));
        const auto before = project.sceneEvents()->program(4);
        action->setCurrentIndex(action->findData(!controls[check].value));
        editor.findChild<QPushButton *>("scenarioEventApply")->click();
        QCOMPARE(u8(project.sceneEvents()->program(4), check + 5), u8(before, check + 5));
        QVERIFY(u8(project.sceneEvents()->program(4), check + 4) != u8(before, check + 4));
        editor.findChild<QPushButton *>("scenarioEventJump")->click();
        QCOMPARE(tree->currentItem()->data(0, Qt::UserRole).toInt(), controls[check].target);
        editor.findChild<QCheckBox *>("scenarioEventAll")->setChecked(false);
        QVERIFY(select(0x2f0)); QApplication::processEvents();
        QVERIFY(editor.grab().save("native/build/scenario-events.png"));
        editor.findChild<QTabBar *>("scenarioEventPhase")->setCurrentIndex(2);
        QCOMPARE(rows->currentData().toInt(),71);QVERIFY(tree->topLevelItemCount() > 0);
        while (undo.index()) undo.undo(); QCOMPARE(project.sceneEvents()->json(), original);
        } catch (const std::exception &e) { QFAIL(e.what()); }
    }
    void scenarioSettingsEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path)); project.loadScenarios();
        const auto initial = project.scenarios()->json();
        QUndoStack undo;
        ScenarioEditor editor(&project, &undo, [] {});
        editor.resize(1100, 780); editor.show();
        auto list = editor.findChild<QListWidget *>("scenarioList");
        for (int i=0;i<list->count();++i) if(list->item(i)->data(Qt::UserRole).toInt()==1) list->setCurrentRow(i);
        auto tabs = editor.findChild<QTabBar *>("scenarioNavigation");
        QTest::mouseClick(tabs, Qt::LeftButton, Qt::NoModifier, tabs->tabRect(3).center());
        auto clock = editor.findChild<QTimeEdit *>("scenarioStartingTime");
        auto reward = editor.findChild<QComboBox *>("scenarioMissionReward0");
        QVERIFY(clock && clock->isVisible() && reward && reward->isEnabled());
        clock->setTime(QTime(23, 59));
        QCOMPARE(project.scenarios()->scenario(1).layout.startHour(), 23);
        QCOMPARE(project.scenarios()->scenario(1).layout.startMinute(), 59);
        const auto timeOnly = project.scenarios()->json();
        reward->setCurrentIndex(reward->findData(0x8000 | 277));
        QVERIFY(QMetaObject::invokeMethod(reward, "activated", Q_ARG(int, reward->currentIndex())));
        bool found = false;
        for (const auto &row : project.scenarios()->scenario(1).layout.deployments)
            if ((u8(row, 0) & 127) == 30) { QCOMPARE(u8(row, 10), 1); QCOMPARE(u16(row, 11), 277); found = true; }
        QVERIFY(found);
        undo.undo(); QCOMPARE(project.scenarios()->json(), timeOnly);
        undo.undo(); QCOMPARE(project.scenarios()->json(), initial);
        undo.redo(); QCOMPARE(clock->time(), QTime(23, 59));
        undo.redo(); QCOMPARE(reward->currentData().toInt(), 0x8000 | 277);
        QTest::qWait(20);
        QVERIFY(editor.grab().save("native/build/scenario-settings.png"));
    }
    void scenarioBattleStrategies() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"),
                   tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (path.isEmpty() || tools.isEmpty())
            QSKIP("Private ROM and compiler required");
        try {
            const auto rom = Rom::open(path);
            auto project = Project::fromRom(rom);
            project.loadScenarios();
            const auto original = project.scenarios()->json();
            QUndoStack undo;
            ScenarioEditor editor(&project, &undo, [] {});
            editor.resize(1100, 800);
            editor.show();
            auto scenarios = editor.findChild<QListWidget *>("scenarioList");
            auto squads = editor.findChild<QListWidget *>("scenarioSquads");
            auto pages = editor.findChild<QStackedWidget *>("scenarioPages");
            auto strategy = editor.findChild<QComboBox *>("scenarioStrategy");
            auto mapStrategy = editor.findChild<QComboBox *>("mapSquadStrategy");
            QVERIFY(strategy);
            QVERIFY(mapStrategy);
            auto selectKey = [&](int key) {
                for (int i = 0; i < scenarios->count(); ++i)
                    if (scenarios->item(i)->data(Qt::UserRole).toInt() == key)
                        scenarios->setCurrentRow(i);
            };
            auto choose = [](QComboBox *box, int value) {
                box->setCurrentIndex(box->findData(value));
                QMetaObject::invokeMethod(box, "activated", Q_ARG(int, box->currentIndex()));
            };
            // Opening original enemy/allied squads must not normalize their data.
            for (int key : {1, 4, 10, 64}) {
                selectKey(key);
                for (int row = 0; row < squads->count(); ++row) {
                    squads->setCurrentRow(row);
                    QCOMPARE(strategy->count(), 4);
                    QCOMPARE(
                        strategy->currentData().toInt(),
                        int(u8(project.scenarios()->scenario(key).layout.deployments[row], 8)));
                }
            }
            QCOMPARE(project.scenarios()->json(), original);
            QCOMPARE(undo.count(), 0);
            selectKey(10);
            squads->setCurrentRow(0);
            pages->setCurrentIndex(1);
            const auto originalRow = project.scenarios()->scenario(10).layout.deployments[0];
            QCOMPARE(u8(originalRow, 8), 1);
            for (int value : {2, 3, 4, 1}) {
                const auto before = project.scenarios()->json();
                const int oldValue = strategy->currentData().toInt(), index = undo.index();
                choose(strategy, value);
                QCOMPARE(undo.index(), index + 1);
                auto expected = originalRow;
                expected[8] = char(value);
                // Project serialization clears managed template IDs; export assigns
                // them again. All authored bytes must be unchanged except strategy.
                auto actual = project.scenarios()->scenario(10).layout.deployments[0];
                expected.replace(1, 2, QByteArray(2, 0));
                actual.replace(1, 2, QByteArray(2, 0));
                QCOMPARE(actual, expected);
                QCOMPARE(project.scenarios()->scenario(10).squads,
                         project.scenarios()->original(10).squads);
                QCOMPARE(project.scenarios()->scenario(10).layout.nodes,
                         project.scenarios()->original(10).layout.nodes);
                QCOMPARE(project.scenarios()->scenario(22).json(),
                         project.scenarios()->original(22).json());
                undo.undo();
                QCOMPARE(project.scenarios()->json(), before);
                QCOMPARE(strategy->currentData().toInt(), oldValue);
                undo.redo();
                QCOMPARE(strategy->currentData().toInt(), value);
            }
            editor.findChild<QPushButton *>("scenarioLocateSquad")->click();
            auto squadTabs = editor.findChild<QTabWidget *>("mapSquadTabs");
            squadTabs->setCurrentIndex(1);
            QCOMPARE(pages->currentIndex(), 0);
            QCOMPARE(mapStrategy->currentData().toInt(), 1);
            const int selection = editor.findChild<QListWidget *>("mapObjects")->currentRow();
            choose(mapStrategy, 4);
            QCOMPARE(strategy->currentData().toInt(), 4);
            QCOMPARE(editor.findChild<QListWidget *>("mapObjects")->currentRow(), selection);
            QCOMPARE(pages->currentIndex(), 0);
            QVERIFY(
                editor.findChild<QLabel *>("mapSquadStrategyHelp")->text().contains("most damage"));
            undo.undo();
            QCOMPARE(mapStrategy->currentData().toInt(), 1);
            undo.redo();
            QCOMPARE(mapStrategy->currentData().toInt(), 4);
            auto scroll = editor.findChild<QScrollArea *>("mapInspectorScroll");
            scroll->ensureWidgetVisible(mapStrategy);
            QTest::qWait(40);
            QVERIFY(mapStrategy->isVisible());
            QVERIFY(mapStrategy->height() >= mapStrategy->sizeHint().height());
            QVERIFY(editor.grab().save("native/build/squad-strategy-sidebar.png"));
            // Restore includes the strategy; undo restores the authored choice.
            editor.findChild<QPushButton *>("mapSquadRestoreSquad")->click();
            QCOMPARE(mapStrategy->currentData().toInt(), 1);
            undo.undo();
            QCOMPARE(mapStrategy->currentData().toInt(), 4);
            // Put all four settings into one source-built candidate.
            selectKey(1);
            pages->setCurrentIndex(1);
            for (int value = 1; value <= 4; ++value) {
                squads->setCurrentRow(value - 1);
                choose(strategy, value);
            }
            const auto edited = project.scenarios()->json();
            QTemporaryDir folder, work;
            project.saveFolder(folder.path());
            auto loaded = Project::fromRom(rom);
            loaded.applyManifest(Project::readProject(folder.path()));
            QCOMPARE(loaded.scenarios()->json(), edited);
            const auto result = rebuild(loaded, tools, work.path());
            QTemporaryDir output(QFileInfo(path).absolutePath() +
                                 "/Test ROMs/native-squad-strategies-XXXXXX");
            QVERIFY(output.isValid());
            const auto candidate = output.filePath(QFileInfo(path).fileName());
            exportRom(loaded, result, candidate);
            auto reopened = Project::fromRom(Rom::open(candidate));
            reopened.loadScenarios();
            QCOMPARE(reopened.scenarios()->json(), edited);
            for (int value = 1; value <= 4; ++value) {
                auto expected = project.scenarios()->original(1).layout.deployments[value - 1];
                expected[8] = char(value);
                QCOMPARE(reopened.scenarios()->scenario(1).layout.deployments[value - 1], expected);
            }
            QCOMPARE(reopened.scenarios()->scenario(22).json(),
                     project.scenarios()->original(22).json());
            auto checksum = reopened.rom().bytes();
            Rom::updateChecksum(checksum);
            QCOMPARE(checksum, reopened.rom().bytes());
            QCOMPARE(Rom::open(path).bytes(), rom.bytes());
            output.setAutoRemove(false);
            qInfo().noquote() << "Squad strategy ROM:" << candidate;
            // Existing unsupported bytes survive unrelated editing, with no stale
            // unknown option offered when switching back to a normal squad.
            auto unknown = *project.scenarios();
            auto row = unknown.scenario(1).layout.deployments[0];
            row[8] = 0;
            unknown.setDeployment(1, 0, row, project.classData());
            project.setScenarios(unknown.json());
            undo.clear();
            scenarios->currentRowChanged(scenarios->currentRow());
            squads->setCurrentRow(0);
            QCOMPARE(strategy->currentData().toInt(), 0);
            QVERIFY(strategy->currentText().contains("Keep unknown"));
            auto facing = editor.findChild<QComboBox *>("scenarioFacing");
            const int direction = u8(row, 5) == 1 ? 2 : 1;
            choose(facing, direction);
            row[5] = char(direction);
            QCOMPARE(project.scenarios()->scenario(1).layout.deployments[0], row);
            squads->setCurrentRow(1);
            QCOMPARE(strategy->count(), 4);
            squads->setCurrentRow(0);
            choose(strategy, 2);
            QCOMPARE(u8(project.scenarios()->scenario(1).layout.deployments[0], 8), 2);
            undo.undo();
            QCOMPARE(strategy->currentData().toInt(), 0);
        } catch (const Error &error) {
            QFAIL(error.what());
        }
    }
    void scenarioOverloadedSupplyEditing() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");
        if(path.isEmpty()) QSKIP("Private ROM required");
        try {
            auto project=Project::fromRom(Rom::open(path)); project.loadScenarios();
            const auto original=project.scenarios()->json();
            const auto &scene=project.scenarios()->scenario(39);
            int row=-1;
            for(int i=0;i<scene.squads.size();++i)
                if((u8(scene.layout.deployments[i],0)&127)==31) row=i;
            QVERIFY(row>=0);
            const auto originalRow=scene.layout.deployments[row];
            const int cell=squadMembers(scene.squads[row])[0].cell;
            QUndoStack undo; ScenarioEditor editor(&project,&undo,[]{});
            editor.resize(1100,800); editor.show();
            auto scenarios=editor.findChild<QListWidget *>("scenarioList");
            for(int i=0;i<scenarios->count();++i)
                if(scenarios->item(i)->data(Qt::UserRole).toInt()==39) scenarios->setCurrentRow(i);
            editor.findChild<QListWidget *>("scenarioSquads")->setCurrentRow(row);
            for(const QString prefix : {QString("scenario"),QString("mapSquad")}) {
                auto board=editor.findChild<FormationBoard *>(prefix+"Cell_Board"); QVERIFY(board);
                QVERIFY(QMetaObject::invokeMethod(board,"cellMoved",Q_ARG(int,cell),Q_ARG(int,(cell+1)%9)));
                QCOMPARE(squadMembers(project.scenarios()->scenario(39).squads[row])[0].cell,(cell+1)%9);
                QCOMPARE(project.scenarios()->scenario(39).layout.deployments[row].mid(13,3),originalRow.mid(13,3));
                undo.undo(); QCOMPARE(project.scenarios()->json(),original);
                const QStringList fields{"Healing","Fruit","Revival"};
                for(int i=0;i<fields.size();++i) for(int value:{0,i==2?1:3}) {
                    auto box=editor.findChild<QComboBox *>(prefix+fields[i]); QVERIFY(box);
                    box->setCurrentIndex(box->findData(value));
                    QVERIFY(QMetaObject::invokeMethod(box,"activated",Q_ARG(int,box->currentIndex())));
                    QCOMPARE(u8(project.scenarios()->scenario(39).layout.deployments[row],13+i),value);
                    QCOMPARE(box->currentData().toInt(),value);
                    const auto reduced=project.scenarios()->json();
                    undo.undo(); QCOMPARE(project.scenarios()->json(),original);
                    QCOMPARE(box->currentData().toInt(),u8(originalRow,13+i));
                    undo.redo(); QCOMPARE(project.scenarios()->json(),reduced);
                    QCOMPARE(box->currentData().toInt(),value);
                    undo.undo();
                }
            }
        } catch(const Error &error) { QFAIL(error.what()); }
    }
    void scenarioSquadEditing() {
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));project.loadScenarios();QCOMPARE(project.appendClass(5,"Scenario fighter"),165);
        QUndoStack undo;ScenarioEditor editor(&project,&undo,[]{});editor.resize(1100,800);editor.show();
        auto list=editor.findChild<QListWidget *>("scenarioList");auto pages=editor.findChild<QStackedWidget *>("scenarioPages");
        auto squads=editor.findChild<QListWidget *>("scenarioSquads");auto base=editor.findChild<QSpinBox *>("scenarioBase");
        QCOMPARE(list->count(),64);
        const auto original=project.scenarios()->json();
        for(int i=0;i<list->count();++i)if(QList<int>{1,10,25,64}.contains(list->item(i)->data(Qt::UserRole).toInt())) {
            list->setCurrentRow(i);for(int row=0;row<squads->count();++row)squads->setCurrentRow(row);
            QCOMPARE(project.scenarios()->json(),original);
        }
        for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole).toInt()==10)list->setCurrentRow(i);
        squads->setCurrentRow(0);pages->setCurrentIndex(1);
        const int originalBase=base->value();base->setValue(originalBase+1);
        QCOMPARE(project.scenarios()->scenario(10).layout.baseLevel(),originalBase+1);undo.undo();QCOMPARE(project.scenarios()->json(),original);
        editScenarioMembers(editor,"scenario",[&](QDialog *dialog){
            dialog->findChild<QSpinBox *>("scenarioMemberCount")->setValue(1);
            auto cls=dialog->findChild<QComboBox *>("scenarioClass_0");cls->setCurrentIndex(cls->findData(165));
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();QVERIFY(!dialog->isVisible());
        });
        QCOMPARE(squadMembers(project.scenarios()->scenario(10).squads[0])[0].classId,165);
        const auto authored=project.scenarios()->json();squads->setCurrentRow(1);squads->setCurrentRow(0);QCOMPARE(project.scenarios()->json(),authored);
        undo.undo();QCOMPARE(project.scenarios()->json(),original);undo.redo();QCOMPARE(project.scenarios()->json(),authored);
        editScenarioMembers(editor,"scenario",[&](QDialog *dialog){
            dialog->findChild<QSpinBox *>("scenarioLevel_0")->setValue(127);
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
            QVERIFY(dialog->isVisible());QVERIFY(dialog->findChild<QLabel *>("scenarioMemberError")->text().contains("level 1â€“99"));
            QCOMPARE(project.scenarios()->json(),authored);
        });
        editScenarioMembers(editor,"scenario",[&](QDialog *dialog){
            auto cls=dialog->findChild<QComboBox *>("scenarioClass_0");cls->setEditText("unfinished class search");
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();QVERIFY(dialog->isVisible());
            QCOMPARE(project.scenarios()->json(),authored);
        });
        editScenarioMembers(editor,"scenario",[&](QDialog *dialog){
            dialog->findChild<QSpinBox *>("scenarioMemberCount")->setValue(5);
            for(int i=0;i<5;++i) {auto cls=dialog->findChild<QComboBox *>(QString("scenarioClass_%1").arg(i));cls->setCurrentIndex(cls->findData(5+i));dialog->findChild<QSpinBox *>(QString("scenarioLevel_%1").arg(i))->setValue(i);}
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();QVERIFY(!dialog->isVisible());
        });
        const auto five=project.scenarios()->scenario(10).squads[0];QVERIFY(independentSquad(five));
        for(int i=0;i<5;++i)QCOMPARE(squadMembers(five)[i].classId,5+i);
        undo.undo();QCOMPARE(project.scenarios()->json(),authored);
        auto board=editor.findChild<FormationBoard *>("scenarioCell_Board");const int from=squadMembers(project.scenarios()->scenario(10).squads[0])[0].cell,to=(from+1)%9;
        QMetaObject::invokeMethod(board,"cellMoved",Qt::DirectConnection,Q_ARG(int,from),Q_ARG(int,to));
        QCOMPARE(squadMembers(project.scenarios()->scenario(10).squads[0])[0].cell,to);undo.undo();QCOMPARE(project.scenarios()->json(),authored);
        auto drop=editor.findChild<QComboBox *>("scenarioDrop");drop->setCurrentIndex(drop->findData(0x8115));QMetaObject::invokeMethod(drop,"activated",Q_ARG(int,drop->currentIndex()));
        QCOMPARE(u16(project.scenarios()->scenario(10).layout.deployments[0],11),0x115);undo.undo();QCOMPARE(project.scenarios()->json(),authored);
        editor.findChild<QPushButton *>("scenarioAddSquad")->click();QCOMPARE(project.scenarios()->scenario(10).squads.size(),project.scenarios()->original(10).squads.size()+1);
        editor.findChild<QPushButton *>("scenarioRemoveSquad")->click();QCOMPARE(project.scenarios()->json(),authored);
        squads->setCurrentRow(0);editor.findChild<QPushButton *>("scenarioRestoreSquad")->click();QCOMPARE(project.scenarios()->scenario(10).squads[0],project.scenarios()->original(10).squads[0]);undo.undo();
        editor.findChild<QPushButton *>("scenarioLocateSquad")->click();QCOMPARE(pages->currentIndex(),0);
        const auto selection=editor.findChild<QListWidget *>("mapObjects")->currentRow();
        editScenarioMembers(editor,"mapSquad",[&](QDialog *dialog){dialog->findChild<QSpinBox *>("mapSquadLevel_0")->setValue(1);dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();});
        QCOMPARE(pages->currentIndex(),0);QCOMPARE(editor.findChild<QListWidget *>("mapObjects")->currentRow(),selection);QCOMPARE(squadMembers(project.scenarios()->scenario(10).squads[0])[0].levelOffset,1);undo.undo();
        editor.findChild<QPushButton *>("scenarioBrowse")->click();QVERIFY(!list->isVisible());QCOMPARE(list->currentItem()->data(Qt::UserRole).toInt(),10);
        auto type=editor.findChild<QComboBox *>("mapObjectType");type->setCurrentIndex(type->findData(int(MapObject::Squad)));
        auto search=editor.findChild<QLineEdit *>("mapObjectSearch");search->setText("no such object");auto objects=editor.findChild<QListWidget *>("mapObjects");
        for(int i=0;i<objects->count();++i)QVERIFY(objects->item(i)->isHidden());search->clear();
        for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole).toInt()==1)list->setCurrentRow(i);
        QVERIFY(selectMapObject(editor,MapObject::Squad,0));
        editor.findChild<QTabWidget *>("mapSquadTabs")->setCurrentIndex(1);QTest::qWait(30);QCOMPARE(editor.width(),1100);
        QVERIFY(editor.grab().save("native/build/scenario-workflow-members.png"));
        editor.findChild<QTabWidget *>("mapSquadTabs")->setCurrentIndex(0);QVERIFY(editor.grab().save("native/build/scenario-workflow-orders.png"));
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
        // Adding after moving a member into a formerly default cell must use free cells.
        editor.findChild<QTableWidget *>("customMembers")->setCurrentCell(0, 0);
        button("customCell_4")->click();
        spin("customMemberCount")->setValue(5);
        button("customApply")->click();
        const auto five = project.encounters()->customSquads().profiles().first();
        QCOMPARE(five.members.size(), 5);
        QSet<int> occupied;
        for (const auto &member : five.members) occupied.insert(member.cell);
        QCOMPARE(occupied.size(), 5);
        QCOMPARE(five.members[0].cell, 4);
        undo.undo();
        button("customNew")->click();
        select("customScenario", 33);
        select("customTerrain", 9);
        editor.findChild<QLineEdit *>("customLabel")->setText("Raiding party");
        spin("customMemberCount")->setValue(3);
        select("customClass_0", 165);
        select("customClass_1", 71);
        spin("customLevel_1")->setValue(-10);
        QVERIFY(combo("customEquipment_0_0")->findData(2) >= 0); // Sword fits copied Knight.
        QVERIFY(combo("customEquipment_0_0")->findData(0x71) < 0); // Mace does not.
        select("customEquipment_0_0", 2);
        select("customEquipment_0_1", 1);
        editor.findChild<QTableWidget *>("customMembers")->setCurrentCell(2, 0);
        button("customCell_0")->click();
        auto cell = button("customCell_0");
        auto board = editor.findChild<FormationBoard *>("customCell_Board");
        QVERIFY(board);
        // Known tile centers on the ROM board, including its opposite corners.
        QCOMPARE(board->cellAt(QPoint(228, 132)), 0);
        QCOMPARE(board->cellAt(QPoint(156, 284)), 8);
        QCOMPARE(board->cellAt(board->cellPolygon(0).boundingRect().topLeft().toPoint()), -1);
        QCOMPARE(board->cellAt(QPoint(410, 10)), -1);
        for (int i = 0; i < 9; ++i) {
            QCOMPARE(board->cellAt(board->cellCenter(i)), i);
            QCOMPARE(board->childAt(board->cellCenter(i)), board->cell(i));
        }
        QVERIFY(!cell->icon().isNull());
        QVERIFY(button("customCell_8")->mapToGlobal(QPoint(0, 0)).y() > cell->mapToGlobal(QPoint(0, 0)).y());
        QCOMPARE(editor.findChild<QTableWidget *>("customMembers")->columnCount(), 4);
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
        const auto saved = project.encounters()->customSquads().profiles()[CustomSquads::key(33, 9)];
        QCOMPARE(saved.members.size(), 3);
        QCOMPARE(saved.members[0].classId, 165);
        QCOMPARE(saved.members[0].equipment[0], 2);
        QCOMPARE(saved.members[1].levelOffset, -10);
        QCOMPARE(saved.members[2].cell, 0);
        QCOMPARE(saved.chance, 70);
        QCOMPARE(saved.retreatHp, 12);
        QCOMPARE(saved.bonuses[0].second, 25);
        QCOMPARE(saved.rewards.outcomes[0], 44);
        const auto before = project.manifest();
        auto drag = [&](int from, int to) {
            const auto start = board->cellCenter(from);
            const auto end = to < 0 ? QPoint(410, 10) : board->cellCenter(to);
            const auto beforeDrag = board->grab().toImage();
            QTest::mousePress(board, Qt::LeftButton, Qt::NoModifier, start);
            QMouseEvent movement(QEvent::MouseMove, end, board->mapToGlobal(end),
                                 Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(board, &movement);
            QVERIFY(board->grab().toImage() != beforeDrag);
            if (from == 0 && to == 8)
                QVERIFY(board->grab().save(QCoreApplication::applicationDirPath()+"/army-board-drag.png"));
            QTest::mouseRelease(board, Qt::LeftButton, Qt::NoModifier, end);
        };
        drag(0, -1); // Releasing outside the angled board must leave the member in place.
        QVERIFY(button("customCell_0")->toolTip().startsWith("3 Â·"));
        drag(0, 8); // Swap two ordinary members.
        QVERIFY(button("customCell_8")->toolTip().startsWith("3 Â·"));
        drag(8, 0);
        drag(0, 4); // Adjacent to a large member: refuse without altering the draft.
        QVERIFY(editor.findChild<QLabel *>("customFormationNotice")->text().contains("adjacent"));
        button("customApply")->click();
        QCOMPARE(project.manifest(), before);
        undo.undo();
        QVERIFY(!project.encounters()->customSquads().hasProfiles());
        undo.redo();
        QCOMPARE(project.manifest(), before);
        combo("customClass_0")->setEditText("No matching class");
        button("customApply")->click();
        QCOMPARE(project.manifest(), before);
        select("customClass_0", 5);
        select("customClass_0", 165);
        editor.resize(1100, 640);
        QApplication::processEvents();
        // At a small window size the board scrolls intact; its angled targets retain identity.
        for (int i = 0; i < 9; ++i) QCOMPARE(board->cellAt(board->cellCenter(i)), i);
        QVERIFY(board->grab().save(QCoreApplication::applicationDirPath()+"/army-board-custom.png"));
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
        auto encounters = *project.encounters();
        encounters.setPair(1, 0, 5, 2, 166);
        CustomSquad custom;
        custom.members[0].classId = 165;
        encounters.setCustomSquad(custom, 166);
        custom.terrainSlot = 1;
        encounters.setCustomSquad(custom, 166);
        project.setEncounters(encounters.json());
        QUndoStack undo;
        EncounterEditor editor(&project, &undo, [] {});
        editor.resize(1250, 760);
        editor.show();
        auto classes = editor.findChild<QListWidget *>("rewardClasses");
        QCOMPARE(classes->count(), 167);
        QVERIFY(classes->item(0)->text().contains("New reward class Squad 1"));
        QVERIFY(classes->item(1)->text().contains("New reward class Squad 2"));
        QSet<int> used{165};
        for (int pool = 1; pool <= Encounters::poolCount; ++pool)
            for (int slot = 0; slot < Encounters::slotCount; ++slot)
                for (int side = 0; side < 2; ++side)
                    used.insert(project.encounters()->classAt(pool, slot, side));
        bool unusedSeen = false;
        for (int row = 0; row < classes->count(); ++row) {
            if (classes->item(row)->data(Qt::UserRole).toInt() < 0) continue;
            const bool active = used.contains(classes->item(row)->data(Qt::UserRole).toInt());
            QVERIFY(!active || !unusedSeen);
            unusedSeen |= !active;
        }
        const auto classReward = project.encounters()->rewards().row(165);
        classes->setCurrentRow(1);
        auto squadItem = editor.findChild<QComboBox *>("rewardItem_0");
        QTimer::singleShot(0, &editor, [&] {
            auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            auto choices = dialog->findChild<QListWidget *>("shopItemChoices");
            QVERIFY(choices);
            for (int i = 0; i < choices->count(); ++i)
                if (choices->item(i)->data(Qt::UserRole).toInt() == 277) {
                    QVERIFY(!choices->item(i)->icon().isNull());
                    choices->setCurrentRow(i); choices->item(i)->setSelected(true);
                }
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
        squadItem->showPopup();
        editor.findChild<QSpinBox *>("rewardWeight_0")->setValue(100);
        editor.findChild<QPushButton *>("rewardApply")->click();
        QCOMPARE(project.encounters()->customSquads().profiles().value(CustomSquads::key(1, 1)).rewards.outcomes[0], 0x8115);
        QCOMPARE(project.encounters()->customSquads().profiles().value(CustomSquads::key(1, 0)).rewards, CreatureReward{});
        QCOMPARE(project.encounters()->rewards().row(165), classReward);
        undo.undo();
        QCOMPARE(project.encounters()->customSquads().profiles().value(CustomSquads::key(1, 1)).rewards, CreatureReward{});
        editor.findChild<QLineEdit *>("rewardSearch")->setText("New reward class");
        for (int row = 0; row < classes->count(); ++row)
            if (classes->item(row)->data(Qt::UserRole).toInt() == 165) classes->setCurrentRow(row);
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
        auto waits = editor.findChild<QTableWidget *>("encounterWaitEstimates");
        auto squads = editor.findChild<QSpinBox *>("encounterDeployed");
        const double single = waits->item(3, 1)->data(Qt::UserRole).toDouble();
        QVERIFY(single > waits->item(3, 2)->data(Qt::UserRole).toDouble());
        const double p = EncounterRate::passingFraction(100, 15);
        squads->setValue(10);
        const double ten = waits->item(3, 1)->data(Qt::UserRole).toDouble();
        QVERIFY(qAbs(ten - single * p / (1 - std::pow(1 - p, 10))) < 0.001);
        squads->setValue(0); QCOMPARE(waits->item(3, 1)->text(), QString("Never"));
        squads->setValue(10);
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
        tabs->setCurrentIndex(1);
        QApplication::processEvents();
        QVERIFY(editor.grab().save(QCoreApplication::applicationDirPath() + "/global-rates.png"));
    }
    void encounterEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadEncounters();
        project.appendClass(1, "Encounter tester");
        QUndoStack undo;
        EncounterEditor editor(&project, &undo, [] {});
        editor.resize(1350, 800); editor.show();
        auto list = editor.findChild<QListWidget *>("encounterPools");
        auto rows = editor.findChild<QTableWidget *>("encounterPairs");
        auto tabs = editor.findChild<QTabWidget *>("encounterTabs");
        auto assignments = static_cast<TerrainAssignments *>(editor.findChild<QTableWidget *>("terrainAssignments"));
        auto tiles = editor.findChild<QListWidget *>("encounterPoolTiles");
        auto mapKey = editor.findChild<QComboBox *>("encounterMapKey");
        QCOMPARE(list->count(), 45); QCOMPARE(rows->rowCount(), 32);
        auto selectMap = [&](int key) {
            for (int row = 0; row < list->count(); ++row)
                if (project.encounters()->scenarioKeys(list->item(row)->data(Qt::UserRole).toInt()).contains(key)) list->setCurrentRow(row);
            mapKey->setCurrentIndex(mapKey->findData(key));
        };
        selectMap(14);
        const int group = list->currentItem()->data(Qt::UserRole).toInt();
        QCOMPARE(mapKey->currentData().toInt(), 14);
        const auto original = project.encounters()->json();
        rows->setCurrentCell(1, 0);
        auto creatures = [&](int first, int second, bool accept = true) {
            QTimer::singleShot(0, &editor, [&, first, second, accept] {
                auto dialog = editor.findChild<QDialog *>("encounterCreatureDialog"); QVERIFY(dialog);
                for (int side = 0; side < 2; ++side) {
                    auto choice = dialog->findChild<QComboBox *>(QString("encounterCreature_%1").arg(side));
                    choice->setCurrentIndex(choice->findData(side ? second : first));
                }
                if (accept) dialog->accept(); else dialog->reject();
            });
            editor.findChild<QPushButton *>("encounterChooseCreatures")->click();
        };
        creatures(165, 165, false); QCOMPARE(project.encounters()->json(), original);
        creatures(165, 165);
        QCOMPARE(project.encounters()->classAt(group, 0, 0), 165);
        QVERIFY(rows->item(1, 2)->text().contains("Encounter tester"));
        assignments->assign(-1, 3, 1); // Shared terrain rows show the same active encounter.
        QVERIFY(rows->item(3, 2)->text().contains("Encounter tester"));
        undo.undo(); undo.undo(); QCOMPARE(project.encounters()->json(), original);
        // Create directly from the selected map/terrain; Apply must not choose another map.
        QTimer::singleShot(0, &editor, [&] {
            auto dialog = editor.findChild<QDialog *>("encounterSquadDialog"); QVERIFY(dialog); dialog->accept();
        });
        editor.findChild<QPushButton *>("encounterAssignSquad")->click();
        QCOMPARE(editor.findChild<QComboBox *>("customScenario")->currentData().toInt(), 14);
        QCOMPARE(editor.findChild<QComboBox *>("customTerrain")->currentData().toInt(), 0);
        editor.findChild<QLineEdit *>("customLabel")->setText("Road guard");
        editor.findChild<QSpinBox *>("customChance")->setValue(43);
        editor.findChild<QSpinBox *>("customRetreat")->setValue(17);
        editor.findChild<QSpinBox *>("customLevel_1")->setValue(3);
        auto equipmentChoice = editor.findChild<QComboBox *>("customEquipment_0_0");
        equipmentChoice->setCurrentIndex(equipmentChoice->findData(2));
        auto rewardChoice = editor.findChild<QComboBox *>("customReward_0");
        rewardChoice->setCurrentIndex(rewardChoice->findData(44));
        editor.findChild<QTableWidget *>("customMembers")->setCurrentCell(0, 0);
        editor.findChild<QPushButton *>("customCell_4")->click();
        editor.findChild<QPushButton *>("customApply")->click();
        const auto squad = project.encounters()->customSquads().profiles().value(CustomSquads::key(14, 0));
        QCOMPARE(squad.members.size(), 2);
        QCOMPARE(squad.chance, 43); QCOMPARE(squad.members[0].cell, 4);
        QCOMPARE(squad.members[0].equipment[0], 2); QCOMPARE(squad.rewards.outcomes[0], 44);
        QVERIFY(rows->item(1, 2)->text().contains("Squad: Knight Squad"));
        QVERIFY(tiles->item(0)->text().contains("Assigned squad:"));
        const auto assigned = project.encounters()->json();
        tabs->setCurrentIndex(0);
        creatures(165, 165);
        QVERIFY(!project.encounters()->customSquads().profiles().value(CustomSquads::key(14, 0)).enabled);
        QVERIFY(rows->item(1, 2)->text().contains("Creature: Encounter tester"));
        undo.undo(); QCOMPARE(project.encounters()->json(), assigned);
        undo.redo(); undo.undo();
        // Assign an independent copy to a different map, preserving every squad field.
        selectMap(1); rows->setCurrentCell(1, 0);
        QTimer::singleShot(0, &editor, [&] {
            auto dialog = editor.findChild<QDialog *>("encounterSquadDialog"); QVERIFY(dialog);
            auto choice = dialog->findChild<QComboBox *>("encounterSquadChoice");
            choice->setCurrentIndex(choice->findData(CustomSquads::key(14, 0))); dialog->accept();
        });
        editor.findChild<QPushButton *>("encounterAssignSquad")->click();
        auto expected = squad; expected.scenarioKey = 1;
        QCOMPARE(project.encounters()->customSquads().profiles().value(CustomSquads::key(1, 0)).json(), expected.json());
        QCOMPARE(project.encounters()->customSquads().profiles().value(CustomSquads::key(14, 0)).json(), squad.json());
        // Copying creatures alone keeps destination squads; inclusive copy replaces them.
        const auto targetWithSquad = project.encounters()->json();
        auto copyFrom = [&](int sourceGroup, int sourceKey, bool include, bool accept = true) {
            QTimer::singleShot(0, &editor, [&, sourceGroup, sourceKey, include, accept] {
                auto dialog = editor.findChild<QDialog *>("encounterCopyDialog"); QVERIFY(dialog);
                auto source = dialog->findChild<QComboBox *>("encounterCopySource");
                source->setCurrentIndex(source->findData(sourceGroup));
                auto key = dialog->findChild<QComboBox *>("encounterCopyMapKey"); key->setCurrentIndex(key->findData(sourceKey));
                dialog->findChild<QCheckBox *>("encounterCopySquads")->setChecked(include);
                if (accept) dialog->accept(); else dialog->reject();
            });
            editor.findChild<QPushButton *>("encounterCopy")->click();
        };
        copyFrom(group, 14, true, false); QCOMPARE(project.encounters()->json(), targetWithSquad);
        copyFrom(group, 14, false);
        QCOMPARE(project.encounters()->customSquads().profiles().value(CustomSquads::key(1, 0)).json(), expected.json());
        // Add a destination-only squad so inclusive copy must remove its stale assignment.
        rows->setCurrentCell(3, 0);
        QTimer::singleShot(0, &editor, [&] {
            auto dialog = editor.findChild<QDialog *>("encounterSquadDialog");
            auto choice = dialog->findChild<QComboBox *>("encounterSquadChoice");
            choice->setCurrentIndex(choice->findData(CustomSquads::key(14, 0))); dialog->accept();
        });
        editor.findChild<QPushButton *>("encounterAssignSquad")->click();
        const auto beforeCopy = project.encounters()->json();
        copyFrom(group, 14, true);
        QVERIFY(!project.encounters()->customSquads().profiles().contains(CustomSquads::key(1, 1)));
        QCOMPARE(project.encounters()->customSquads().profiles().value(CustomSquads::key(1, 0)).json(), expected.json());
        undo.undo(); QCOMPARE(project.encounters()->json(), beforeCopy);
        undo.redo();
        const auto retail = Encounters::load(project.rom(), 166);
        for (int t = 0; t < 32; ++t) {
            QCOMPARE(project.encounters()->lookup(t), retail.lookup(t));
            QCOMPARE(project.encounters()->rate(t), retail.rate(t));
        }
        rows->setCurrentCell(1, 0); QApplication::processEvents();
        QVERIFY(editor.grab().save("native/build/map-encounter-workflow.png"));
        // Map versions can share creature choices, but must retain separate squad assignments.
        QVector<int> sharedKeys;
        for (int row = 0; row < list->count() && sharedKeys.isEmpty(); ++row) {
            const auto keys = project.encounters()->scenarioKeys(list->item(row)->data(Qt::UserRole).toInt());
            if (keys.size() > 1) sharedKeys = keys;
        }
        QVERIFY(sharedKeys.size() > 1);
        selectMap(sharedKeys[0]); rows->setCurrentCell(1, 0);
        QTimer::singleShot(0, &editor, [&] {
            auto dialog = editor.findChild<QDialog *>("encounterSquadDialog"); QVERIFY(dialog);
            auto choice = dialog->findChild<QComboBox *>("encounterSquadChoice");
            choice->setCurrentIndex(choice->findData(CustomSquads::key(14, 0))); dialog->accept();
        });
        editor.findChild<QPushButton *>("encounterAssignSquad")->click();
        QVERIFY(rows->item(1, 2)->text().startsWith("Squad:"));
        mapKey->setCurrentIndex(mapKey->findData(sharedKeys[1]));
        QVERIFY(!rows->item(1, 2)->text().startsWith("Squad:"));
        QVERIFY(!project.encounters()->customSquads().profiles().contains(CustomSquads::key(sharedKeys[1], 0)));
        while (undo.index()) undo.undo(); QCOMPARE(project.encounters()->json(), original);
        // Advanced retains shared assignments, rates, new pools, DnD and Undo.
        editor.findChild<QPushButton *>("encounterAdvancedTerrain")->click();
        auto advanced = editor.findChild<QDialog *>("encounterTerrainDialog"); QVERIFY(advanced->isVisible());
        auto rate = editor.findChild<QSpinBox *>("encounterRate_31"); QVERIFY(!rate->isEnabled());
        assignments->assign(-1, 31, 10); rate->setValue(100);
        QCOMPARE(project.encounters()->rate(31), 100);
        assignments->assign(31, 2, 10);
        QCOMPARE(project.encounters()->lookup(2), 10); QCOMPARE(project.encounters()->lookup(31), 0);
        undo.undo(); QCOMPARE(rate->value(), 100);
        assignments->setCurrentCell(24, 2); editor.findChild<QPushButton *>("encounterAddPool")->click();
        QCOMPARE(project.encounters()->slotsPerPool(), 11); QCOMPARE(project.encounters()->lookup(24), 11);
        QApplication::processEvents(); assignments->scrollToItem(assignments->item(2, 2));
        const QPoint target = assignments->visualItemRect(assignments->item(2, 2)).center();
        QMimeData mime; mime.setData(poolMime, "11:24");
        QDragEnterEvent enter(QPointF(target), Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(assignments->viewport(), &enter); QVERIFY(enter.isAccepted());
        QDragMoveEvent move(QPointF(target), Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(assignments->viewport(), &move);
        QDropEvent drop(target, Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(assignments->viewport(), &drop); QCOMPARE(project.encounters()->lookup(2), 11);
        editor.findChild<QPushButton *>("encounterBackToMap")->click(); QVERIFY(!advanced->isVisible());
        while (undo.index()) undo.undo(); QCOMPARE(project.encounters()->json(), original);
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
        QTest::qWait(20);
        shops->scrollToBottom();
        const int bottom = shops->verticalScrollBar()->value();
        shops->setCurrentRow(shops->count() - 2);
        QCOMPARE(shops->verticalScrollBar()->value(), bottom);
        auto townChoice = editor.findChild<QComboBox *>("shopStronghold");
        QSet<int> towns;
        for (int i = 0; i < townChoice->count(); ++i) towns.insert(townChoice->itemData(i).toInt());
        for (int id = 1; id <= ScenarioWorld::townCount; ++id) QVERIFY(towns.contains(id));
        shops->setCurrentRow(1);
        const int assignedTown = townChoice->currentData().toInt();
        QVERIFY(assignedTown > 0);
        QCOMPARE(u8(project.world()->town(assignedTown), 27), quint8(2));
        auto pick = [&](bool equipment, int id, bool accept = true) {
            QTimer::singleShot(0, &editor, [&, equipment, id, accept] {
                auto dialog = editor.findChild<QDialog *>("shopItemPicker");
                QVERIFY(dialog);
                auto list = dialog->findChild<QListWidget *>("shopItemChoices");
                QListWidgetItem *target = nullptr;
                for (int row = 0; row < list->count(); ++row)
                    if (list->item(row)->data(Qt::UserRole).toInt() == id) target = list->item(row);
                QVERIFY(target);
                if (equipment) {
                    const int type = u8(project.record(Table::Items, id).bytes, 4);
                    auto filter = dialog->findChild<QPushButton *>(QString("shopItemType_%1").arg(type));
                    QVERIFY(filter && !filter->icon().isNull());
                    filter->click();
                    QVERIFY(!target->isHidden());
                    for (int at = 0; at < list->count(); ++at)
                        QCOMPARE(!list->item(at)->isHidden(), list->item(at)->data(Qt::UserRole + 1).toInt() == type);
                }
                const auto image = project.iconArtwork()->image(id | (equipment ? 0 : 0x200))
                                       .scaled(48, 48, Qt::KeepAspectRatio, Qt::FastTransformation);
                QCOMPARE(target->icon().pixmap(48, 48).toImage().convertToFormat(QImage::Format_RGBA8888),
                         image.convertToFormat(QImage::Format_RGBA8888));
                target->setSelected(true);
                if (equipment && !accept && !qEnvironmentVariable("LORDLY_SHOP_PICKER_SCREENSHOT").isEmpty()) {
                    QTest::qWait(20);
                    QVERIFY(dialog->grab().save(qEnvironmentVariable("LORDLY_SHOP_PICKER_SCREENSHOT")));
                }
                dialog->findChild<QDialogButtonBox *>()->button(accept ? QDialogButtonBox::Ok : QDialogButtonBox::Cancel)->click();
            });
            editor.findChild<QPushButton *>(equipment ? "shopEquipmentAdd" : "shopConsumablesAdd")->click();
        };
        pick(true, 277, false);
        QCOMPARE(project.shops()->json(), original);
        pick(true, 277);
        QCOMPARE(project.shops()->stock(2).equipment.last(), quint16(277));
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
        pick(false, 44);
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
        const auto beforeNew = project.manifest();
        editor.findChild<QPushButton *>("shopAdd")->click();
        QCOMPARE(project.shops()->size(), 37);
        QCOMPARE(shops->currentItem()->data(Qt::UserRole).toInt(), 36);
        auto town = editor.findChild<QComboBox *>("shopStronghold");
        int townId = 1;
        while (u8(project.world()->town(townId), 24) == 255) ++townId;
        town->setCurrentIndex(town->findData(townId));
        editor.findChild<QPushButton *>("shopAssign")->click();
        QCOMPARE(u8(project.world()->town(townId), 27), quint8(36));
        QVERIFY(u8(project.world()->town(townId), 25) & 1);
        editor.findChild<QPushButton *>("shopDetach")->click();
        QCOMPARE(u8(project.world()->town(townId), 27), quint8(0));
        undo.undo();
        QCOMPARE(u8(project.world()->town(townId), 27), quint8(36));
        undo.undo();
        undo.undo();
        QCOMPARE(project.manifest(), beforeNew);
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
        QCOMPARE(combo->itemText(combo->findData(164)), QString("Ancient Bahamut"));
        QCOMPARE(index.data().toString(),QString("Ancient Bahamut"));
        QCOMPARE(index.data(Qt::EditRole).toString(),QString("Ancient\nBahamut"));
        undo.undo();
        QCOMPARE(project.classNameBytes(164), original);
        QCOMPARE(combo->itemText(combo->findData(164)), QString("Death Bahamut"));
        undo.redo();
        QCOMPARE(project.classNameBytes(164), encodeClassName("Ancient\nBahamut"));
        QSignalSpy rejected(&model, &RecordModel::editRejected);
        QVERIFY(!model.setData(index, "", Qt::EditRole));
        QCOMPARE(rejected.size(), 1);
        QCOMPARE(project.classNameBytes(164), encodeClassName("Ancient\nBahamut"));
    }
    void addItemUndoRedo() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        QUndoStack undo;
        RecordModel model(&project, Table::Items, &undo);
        const auto before = project.manifest();
        QCOMPARE(model.appendItem(1, "Fresh blade"), 278);
        QCOMPARE(model.rowCount(), 279);
        const auto added = project.manifest();
        QVERIFY(model.setData(model.index(278, 1), "Named blade", Qt::EditRole));
        undo.undo();
        QCOMPARE(project.manifest(), added);
        undo.undo();
        QCOMPARE(project.manifest(), before);
        QCOMPARE(model.rowCount(), 278);
        undo.redo();
        QCOMPARE(project.manifest(), added);
        undo.redo();
        QCOMPARE(project.record(Table::Items, 278).name, QString("Named blade"));
        const auto current = project.manifest();
        QSignalSpy rejected(&model, &RecordModel::editRejected);
        QCOMPARE(model.appendItem(0, "Invalid"), -1);
        QCOMPARE(rejected.size(), 1);
        QCOMPARE(project.manifest(), current);
        QFile save(QFileInfo(path).dir().filePath("tools/pj64_golden.sra"));
        QVERIFY(save.open(QIODevice::ReadOnly));
        SaveEditor saveEditor(&project);
        saveEditor.loadBytes(save.readAll());
        const int slot = saveEditor.document()->rows(SaveTable::Equipment).last();
        QCOMPARE(saveEditor.document()->value(SaveTable::Equipment, slot, "id").toInt(), 0);
        QVERIFY(saveEditor.edit(SaveTable::Equipment, slot, "id", 278));
        QVERIFY(saveEditor.edit(SaveTable::Equipment, slot, "owned", 3));
        QTemporaryDir output;
        saveEditor.exportFile(output.filePath("new-item.sra"));
        QFile saved(output.filePath("new-item.sra"));
        QVERIFY(saved.open(QIODevice::ReadOnly));
        const auto readback = SaveDocument::open(project.rom(), saved.readAll());
        QCOMPARE(readback.value(SaveTable::Equipment, slot, "id").toInt(), 278);
        undo.undo();
        undo.undo();
        QCOMPARE(model.rowCount(), 278);
        QVERIFY_THROWS_EXCEPTION(Error, saveEditor.exportFile(output.filePath("missing-item.sra")));
        const auto beforeBlank = project.manifest();
        QCOMPARE(model.createItem("Empty accessory"), 278);
        auto expected = QByteArray(32, 0);
        put8(expected, 4, 25);
        put8(expected, 6, 1);
        QCOMPARE(project.record(Table::Items, 278).bytes, expected);
        const auto blankIcon = project.iconArtwork()->image(278);
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) QCOMPARE(blankIcon.pixelColor(x, y).alpha(), 0);
        QCOMPARE(project.textBanks()["items"].entries[278].bytes, QByteArray());
        const auto blank = project.manifest();
        undo.undo();
        QCOMPARE(project.manifest(), beforeBlank);
        undo.redo();
        QCOMPARE(project.manifest(), blank);
    }
    void locationDescriptionEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        QUndoStack undo;
        CampaignEditor campaign(&project, &undo, [] {});
        NamesTextEditor names(&project, &undo, [] {});
        campaign.selectNode(6);
        auto panel = campaign.findChild<QWidget *>("campaignLocationDescription");
        QVERIFY(panel);
        auto preview = panel->findChild<QLabel *>("recordDescriptionPreview");
        auto banks = names.findChild<QComboBox *>("descriptionBank");
        auto entries = names.findChild<QListWidget *>("descriptionEntries");
        auto text = names.findChild<QPlainTextEdit *>("descriptionEditor");
        QVERIFY(preview && banks && entries && text);
        banks->setCurrentIndex(banks->findData("locations"));
        QCOMPARE(entries->count(), 48);
        entries->setCurrentRow(5);
        QVERIFY(entries->item(5)->text().contains("Tenne Plains"));
        const auto original = project.textBanks()["locations"].text(5);
        QCOMPARE(preview->text(), original);
        text->setPlainText("A peaceful southern plain.");
        QCOMPARE(preview->text(), QString("A peaceful southern plain."));
        undo.undo(); QCOMPARE(preview->text(), original); QCOMPARE(text->toPlainText(), original);
        undo.redo(); QCOMPARE(preview->text(), QString("A peaceful southern plain."));
        bool saved = false;
        QTimer::singleShot(0, [&] {
            auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            auto close = qScopeGuard([&] { if (dialog->isVisible()) dialog->reject(); });
            auto edit = dialog->findChild<QPlainTextEdit *>("recordDescriptionText");
            QVERIFY(edit);
            edit->setPlainText("The southern plain.\nHome to new settlers.");
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
            saved = true;
        });
        panel->findChild<QPushButton *>("editRecordDescription")->click();
        QVERIFY(saved);
        QCOMPARE(text->toPlainText(), QString("The southern plain.\nHome to new settlers."));
        QCOMPARE(preview->text(), text->toPlainText());
        undo.undo(); QCOMPARE(text->toPlainText(), QString("A peaceful southern plain."));
        undo.redo(); QCOMPARE(preview->text(), text->toPlainText());
        // Moving/renaming a map location retains its independent description ID.
        campaign.findChild<QLineEdit *>("campaignNodeName")->setText("New Tenne");
        campaign.findChild<QPushButton *>("campaignApplyNode")->click();
        QVERIFY(entries->item(5)->text().contains("New Tenne"));
        QCOMPARE(preview->text(), text->toPlainText());
        campaign.selectNode(4);
        QCOMPARE(preview->text(), project.textBanks()["locations"].text(3));
        campaign.selectNode(6);
        QCOMPARE(preview->text(), text->toPlainText());
    }
    void descriptionWorkspaceEditing() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        auto project = Project::fromRom(Rom::open(path));
        project.loadTexts();
        const auto before = project.manifest();
        const auto action55 = project.textBanks()["actions"].entries[55].bytes;
        QVERIFY(!action55.isEmpty());
        QVERIFY(project.textBanks()["actions"].entries[56].bytes.isEmpty());
        QUndoStack undo;
        auto page = std::unique_ptr<QWidget>(createTextWorkspace(&project, &undo, [] {}));
        auto banks = page->findChild<QComboBox *>("descriptionBank");
        auto entries = page->findChild<QListWidget *>("descriptionEntries");
        auto editor = page->findChild<QPlainTextEdit *>("descriptionEditor");
        auto empty = page->findChild<QCheckBox *>("descriptionShowEmpty");
        auto search = page->findChild<QLineEdit *>("descriptionSearch");
        auto counts = page->findChild<QLabel *>("descriptionCounts");
        QCOMPARE(banks->currentData().toString(), QString("actions"));
        QCOMPARE(counts->text(), QString("102 with text Â· 57 empty entries"));
        QVERIFY(!entries->item(55)->isHidden());
        QVERIFY(entries->item(56)->isHidden());
        entries->setCurrentRow(55);
        QVERIFY(!editor->toPlainText().isEmpty());
        empty->setChecked(true);
        search->setText("Empty");
        entries->setCurrentRow(56);
        QVERIFY(editor->toPlainText().isEmpty());
        QVERIFY(editor->placeholderText().contains("No description text"));
        QVERIFY(entries->item(56)->text().contains("Empty"));
        QCOMPARE(project.manifest(), before); // Displaying empty slots never authors replacement text.
        editor->setPlainText("Authored action description.");
        QCOMPARE(project.textBanks()["actions"].text(56), QString("Authored action description."));
        QCOMPARE(entries->currentRow(), 56);
        QCOMPARE(editor->toPlainText(), QString("Authored action description."));
        QVERIFY(!entries->item(56)->isHidden()); // Keep typing even when the label no longer matches.
        QCOMPARE(project.textBanks()["actions"].entries[55].bytes, action55);
        QVERIFY(!entries->item(56)->text().contains("Empty"));
        QCOMPARE(counts->text(), QString("103 with text Â· 56 empty entries"));
        search->clear();
        empty->setChecked(false);
        undo.undo();
        QCOMPARE(project.manifest(), before);
        QCOMPARE(entries->currentRow(), 56);
        QVERIFY(editor->toPlainText().isEmpty());
        QVERIFY(!entries->item(56)->isHidden()); // Retain the edit target across Undo.
        undo.redo();
        QCOMPARE(project.textBanks()["actions"].text(56), QString("Authored action description."));
        search->setText("nothing matches this description");
        QCOMPARE(entries->currentRow(), -1);
        QVERIFY(!editor->isEnabled());
        QVERIFY(editor->toPlainText().isEmpty());
        search->clear();
        QVERIFY(editor->isEnabled());
        banks->setCurrentIndex(banks->findData("classes"));
        QCOMPARE(counts->text(), QString("188 with text Â· 37 empty entries"));
        QVERIFY(entries->item(165)->isHidden());
        QVERIFY(entries->item(165)->text().contains("Reserved slot"));
        QVERIFY(entries->item(200)->text().contains("Class-change help 2800"));
        RecordModel model(&project, Table::Classes, &undo);
        QCOMPARE(model.createClass("Blank hero"), 165);
        empty->setChecked(true);
        entries->setCurrentRow(225);
        QVERIFY(entries->item(225)->text().contains("Class 165 Â· Blank hero"));
        QVERIFY(editor->toPlainText().isEmpty());
        banks->setCurrentIndex(banks->findData("consumables"));
        QVERIFY(entries->item(1)->text().contains(project.record(Table::Consumables, 1).name));
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
        // Fresh gameplay settings must not inherit even a modified starter's data.
        project.setText("classes", 5, "Do not copy this description");
        auto promotion = *project.promotion();
        promotion.setGate(5, 0, 123);
        promotion.setMask(5, 0x5a);
        project.setPromotion(promotion.json());
        auto record = project.record(Table::Classes, 5).bytes;
        put16(record, 8, 999);
        project.replaceBytes(Table::Classes, 5, record);
        const auto beforeFresh = project.manifest();
        QCOMPARE(model.createClass("Fresh class"), 166);
        const auto &fresh = project.record(Table::Classes, 166).bytes;
        QCOMPARE(u16(fresh, 8), 1U);
        QCOMPARE(fresh.mid(46, 15), QByteArray(15, 0)); // No equipment or actions.
        QCOMPARE(u8(fresh, 65), quint8(166)); // Own growth; no Fighter gameplay dependency.
        QVERIFY(project.textBanks()["classes"].entries[226].bytes.isEmpty());
        QCOMPARE(project.promotion()->gates.mid(166 * 8, 8), QByteArray::fromHex("0000000000000064"));
        QCOMPARE(u8(project.promotion()->masks, 166), quint8(1));
        QCOMPARE(project.promotion()->families["assignments"].toArray()[166].toInt(), 0);
        const auto freshManifest = project.manifest();
        undo.undo();
        QCOMPARE(project.manifest(), beforeFresh);
        undo.redo();
        QCOMPARE(project.manifest(), freshManifest);
        QCOMPARE(model.createClass(""), -1);
        QCOMPARE(project.manifest(), freshManifest);
    }
    void itemNameFonts() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM not supplied");
        auto project = Project::fromRom(Rom::open(path));
        const auto original = project.manifest();
        const auto originalName = project.itemNameBytes(1);
        QUndoStack undo;
        RecordModel model(&project, Table::Items, &undo);
        QVERIFY(model.renameItem(1, decodeText(originalName), ItemNameFont::Small));
        QCOMPARE(undo.count(), 1);
        QCOMPARE(project.itemNameBytes(1), QByteArray(1, 0x0e) + originalName + QByteArray(1, 0x0f));
        QVERIFY(project.changeReport().text().contains("Name font"));
        QVERIFY(project.compareWithRom(project.rom()).text().contains("Name font"));
        undo.undo();
        QCOMPARE(project.manifest(), original);
        undo.redo();
        QVERIFY(model.setData(model.index(1, 1), "Moth-eaten Clothing", Qt::EditRole));
        QCOMPARE(itemNameFont(project.itemNameBytes(1)), ItemNameFont::Small);
        // A no-op must retain a nondefault native second-line offset exactly.
        const auto formatted = QByteArray::fromHex("0e") + "Sword of" + QByteArray::fromHex("1034") + "Firedrake" + QByteArray::fromHex("0f");
        project.setItemNameBytes(8, formatted);
        const int count = undo.count();
        QVERIFY(model.renameItem(8, "Sword of\nFiredrake", ItemNameFont::Small));
        QCOMPARE(project.itemNameBytes(8), formatted);
        QCOMPARE(undo.count(), count);

        MainWindow window;
        window.show();
        window.loadRom(path);
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QPushButton *>("renameItem"), 10000);
        auto rename = window.findChild<QPushButton *>("renameItem");
        auto stack = window.findChild<QUndoStack *>();
        QVERIFY(rename && stack);
        window.navigateTo({"items", 8});
        auto runDialog = [&](const std::function<void(QDialog *)> &action) {
            bool opened = false;
            QTimer::singleShot(0, &window, [&] {
                auto dialog = window.findChild<QDialog *>("renameItemDialog");
                if (!dialog) return;
                opened = true;
                action(dialog);
                if (dialog->isVisible()) dialog->reject();
            });
            rename->click();
            QVERIFY(opened);
        };
        runDialog([&](QDialog *dialog) {
            auto font = dialog->findChild<QComboBox *>("renameItemFont");
            auto name = dialog->findChild<QPlainTextEdit *>("renameItemName");
            QVERIFY(font && name);
            QCOMPARE(font->currentText(), QString("Small"));
            QCOMPARE(name->toPlainText(), QString("Sword of\nFiredrake"));
            font->setCurrentIndex(0);
            QCOMPARE(name->toPlainText(), QString("Sword of Firedrake"));
            // Cancel both the font change and the automatic joining of lines.
        });
        QCOMPARE(stack->count(), 0);
        runDialog([&](QDialog *dialog) {
            QCOMPARE(dialog->findChild<QComboBox *>("renameItemFont")->currentText(), QString("Small"));
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        });
        QCOMPARE(stack->count(), 0);
        runDialog([&](QDialog *dialog) {
            dialog->findChild<QComboBox *>("renameItemFont")->setCurrentIndex(0);
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        });
        QCOMPARE(stack->count(), 1);
        stack->undo();
        runDialog([&](QDialog *dialog) {
            QCOMPARE(dialog->findChild<QComboBox *>("renameItemFont")->currentText(), QString("Small"));
            QCOMPARE(dialog->findChild<QPlainTextEdit *>("renameItemName")->toPlainText(), QString("Sword of\nFiredrake"));
        });
        stack->redo();
        runDialog([&](QDialog *dialog) {
            QCOMPARE(dialog->findChild<QComboBox *>("renameItemFont")->currentText(), QString("Normal"));
            auto name = dialog->findChild<QPlainTextEdit *>("renameItemName");
            name->setPlainText(QString(33, 'a'));
            QVERIFY(!dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->isEnabled());
            QVERIFY(!dialog->findChild<QLabel *>("renameItemError")->text().isEmpty());
        });
        window.navigateTo({"items", 1});
        runDialog([&](QDialog *dialog) {
            auto font = dialog->findChild<QComboBox *>("renameItemFont");
            QCOMPARE(font->currentText(), QString("Normal"));
            font->setCurrentIndex(1);
            dialog->findChild<QPlainTextEdit *>("renameItemName")->setPlainText("Moth-eaten Clothing");
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        });
        runDialog([&](QDialog *dialog) {
            QCOMPARE(dialog->findChild<QComboBox *>("renameItemFont")->currentText(), QString("Small"));
            QCOMPARE(dialog->findChild<QPlainTextEdit *>("renameItemName")->toPlainText(), QString("Moth-eaten Clothing"));
        });
        stack->setIndex(0);
    }
    void loadDesktopWorkspace() {
        auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM not supplied");
        const auto baseline = Project::fromRom(Rom::open(path));
        const int classCount = int(baseline.records(Table::Classes).size());
        const int itemCount = int(baseline.records(Table::Items).size());
        MainWindow window;
        window.show();
        window.loadRom(path);
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QListWidget *>("encounterPools"), 10000);
        auto libraryTabs = window.findChild<QTabWidget *>("artLibraryTabs");
        QVERIFY(libraryTabs);
        QCOMPARE(libraryTabs->tabText(0), "Artwork");
        QCOMPARE(libraryTabs->tabText(1), "Effects");
        auto classes = window.findChild<QTableView *>("classesTable");
        auto items = window.findChild<QTableView *>("itemsTable");
        auto consumables = window.findChild<QTableView *>("consumablesTable");
        QVERIFY(classes);
        QVERIFY(items);
        QVERIFY(consumables);
        QCOMPARE(classes->model()->rowCount(), classCount);
        QCOMPARE(items->model()->rowCount(), itemCount);
        QCOMPARE(consumables->model()->rowCount(), 45);
        // Column views must let the user compare all items without losing selection.
        auto itemView = window.findChild<QComboBox *>("itemsColumnView");
        QVERIFY(itemView);
        const auto selectedItem = items->currentIndex().siblingAtColumn(0).data(Qt::EditRole);
        const QList<QPair<QString, QString>> itemViews{{"Identity", "equipType"},
                                                       {"Stats", "str"},
                                                       {"Resistance", "resFire"},
                                                       {"Growth", "growthHpStr"},
                                                       {"Advanced", "b12Raw"},
                                                       {"", "price"}};
        for (const auto &[group, field] : itemViews) {
            const int choice = itemView->findData(group);
            QVERIFY(choice >= 0);
            itemView->setCurrentIndex(choice);
            int column = -1;
            for (int i = 0; i < fields(Table::Items).size(); ++i)
                if (fields(Table::Items)[i].key == field) column = i + 2;
            QVERIFY(column >= 2);
            QVERIFY(!items->isColumnHidden(column));
            QVERIFY(!items->isColumnHidden(0));
            QVERIFY(!items->isColumnHidden(1));
            QCOMPARE(items->model()->rowCount(), itemCount);
            QCOMPARE(items->currentIndex().siblingAtColumn(0).data(Qt::EditRole), selectedItem);
            if (!group.isEmpty())
                QCOMPARE(window.findChild<QComboBox *>("itemsInspectorSection")->currentText(), group);
        }
        itemView->setCurrentIndex(itemView->findData("Stats"));
        QCOMPARE(window.findChild<QListWidget *>("encounterPools")->count(), 45);
        QCOMPARE(window.findChild<QListWidget *>("scenarioList")->count(), 64);
        auto textBaseline = baseline;
        textBaseline.loadTexts();
        auto recordUndo = window.findChild<QUndoStack *>();
        auto editDescriptionFrom = [&](QPushButton *button, const QString &expected,
                                       const QString &text, bool save, bool invalid = false) {
            QVERIFY(button && button->isEnabled());
            QTimer::singleShot(0, &window, [&, expected, text, save, invalid] {
                auto dialog = window.findChild<QDialog *>("recordDescriptionDialog");
                QVERIFY(dialog);
                const auto close = qScopeGuard([dialog] { if (dialog->isVisible()) dialog->reject(); });
                auto draft = dialog->findChild<QPlainTextEdit *>("recordDescriptionText");
                auto buttons = dialog->findChild<QDialogButtonBox *>();
                QCOMPARE(draft->toPlainText(), expected);
                if (invalid) {
                    draft->setPlainText("One\nTwo\nThree\nFour\nFive");
                    QVERIFY(!buttons->button(QDialogButtonBox::Save)->isEnabled());
                }
                draft->setPlainText(text);
                QVERIFY(buttons->button(QDialogButtonBox::Save)->isEnabled());
                buttons->button(save ? QDialogButtonBox::Save : QDialogButtonBox::Cancel)->click();
            });
            button->click();
        };
        // Follow the selected record's identity through sorted tables, not its visible row.
        for (const auto table : {Table::Classes, Table::Items, Table::Consumables}) {
            auto grid = table == Table::Classes ? classes : table == Table::Items ? items : consumables;
            const int id = table == Table::Classes ? 5 : 1;
            const auto kind = tableKey(table);
            grid->sortByColumn(1, Qt::DescendingOrder);
            for (int row = 0; row < grid->model()->rowCount(); ++row)
                if (grid->model()->index(row, 0).data(Qt::EditRole).toInt() == id)
                    grid->setCurrentIndex(grid->model()->index(row, 1));
            auto panel = window.findChild<QWidget *>(kind + "DescriptionPanel");
            auto preview = panel->findChild<QLabel *>("recordDescriptionPreview");
            auto button = panel->findChild<QPushButton *>("editRecordDescription");
            const auto original = textBaseline.textBanks()[kind].text(id);
            QCOMPARE(preview->text(), original);
            const int index = recordUndo->index();
            editDescriptionFrom(button, original, "Cancelled draft", false);
            QCOMPARE(recordUndo->index(), index);
            QCOMPARE(preview->text(), original);
            editDescriptionFrom(button, original, "Edited from " + kind, true, true);
            QCOMPARE(recordUndo->index(), index + 1);
            QCOMPARE(preview->text(), "Edited from " + kind);
            editDescriptionFrom(button, "Edited from " + kind, "Cancelled draft", false);
            recordUndo->undo();
            QCOMPARE(preview->text(), original);
            recordUndo->redo();
            QCOMPARE(preview->text(), "Edited from " + kind);
            recordUndo->undo();
            grid->sortByColumn(0, Qt::AscendingOrder);
        }
        const int frontAction = u8(textBaseline.record(Table::Classes, 5).bytes, 55);
        editDescriptionFrom(window.findChild<QPushButton *>("attack0Description"),
                            textBaseline.textBanks()["actions"].text(frontAction), "Action from class", true);
        editDescriptionFrom(window.findChild<QPushButton *>("attack0Description"),
                            "Action from class", "Cancelled draft", false);
        recordUndo->undo();
        auto addItem = window.findChild<QPushButton *>("addItem");
        QVERIFY(addItem);
        items->setCurrentIndex(items->model()->index(1, 1));
        QVERIFY(addItem->isEnabled());
        QTimer::singleShot(0, &window, [&] {
            auto dialog = window.findChild<QInputDialog *>();
            QVERIFY(dialog);
            dialog->setTextValue("Created item");
            dialog->accept();
        });
        addItem->click();
        QCOMPARE(items->model()->rowCount(), itemCount + 1);
        QCOMPARE(items->currentIndex().siblingAtColumn(1).data().toString(),
                 QString("Created item"));
        auto itemDescription = window.findChild<QWidget *>("itemsDescriptionPanel");
        editDescriptionFrom(itemDescription->findChild<QPushButton *>("editRecordDescription"),
                            textBaseline.textBanks()["items"].text(1), "New item description", true);
        QCOMPARE(itemDescription->findChild<QLabel *>("recordDescriptionPreview")->text(),
                 QString("New item description"));
        recordUndo->undo(); // Undo only the description, keeping the newly added item selected.
        for (const auto name : {"scenarioDrop", "rewardItem_0"}) {
            const auto box = window.findChild<QComboBox *>(name);
            QVERIFY(box);
            QVERIFY(box->findData(0x8000 | itemCount) >= 0);
        }
        const auto reward = window.findChild<QComboBox *>("sceneReward");
        QVERIFY(reward);
        QVERIFY(reward->findData(itemCount) >= 0);
        const auto equipment = window.findChild<QComboBox *>("actorEquipment");
        QVERIFY(equipment);
        QVERIFY(equipment->findData(itemCount) >= 0);
        window.findChild<QUndoStack *>()->undo();
        QCOMPARE(items->model()->rowCount(), itemCount);
        auto fresh = window.findChild<QPushButton *>("newClass");
        QVERIFY(fresh);
        // Scrolling over an unfocused field must not silently edit the ROM.
        auto untouchedSpin = window.findChild<QSpinBox *>();
        untouchedSpin->clearFocus();
        const int beforeWheel = untouchedSpin->value();
        QWheelEvent passiveWheel(QPointF(2, 2), QPointF(2, 2), QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(untouchedSpin, &passiveWheel);
        QCOMPARE(untouchedSpin->value(), beforeWheel);
        classes->setCurrentIndex({}); // Creating a class needs no template selection.
        QVERIFY(fresh->isEnabled());
        QTimer::singleShot(0, &window, [&] {
            auto dialog = window.findChild<QInputDialog *>();
            QVERIFY(dialog);
            dialog->reject();
        });
        fresh->click();
        QCOMPARE(classes->model()->rowCount(), classCount);
        QTimer::singleShot(0, &window, [&] {
            auto dialog = window.findChild<QInputDialog *>();
            QVERIFY(dialog);
            dialog->setTextValue("Fresh class");
            dialog->accept();
        });
        fresh->click();
        QCOMPARE(classes->model()->rowCount(), classCount + 1);
        QCOMPARE(classes->currentIndex().siblingAtColumn(1).data().toString(), QString("Fresh class"));
        auto classDescription = window.findChild<QWidget *>("classesDescriptionPanel");
        editDescriptionFrom(classDescription->findChild<QPushButton *>("editRecordDescription"),
                            {}, "New class description", true);
        editDescriptionFrom(classDescription->findChild<QPushButton *>("editRecordDescription"),
                            "New class description", "Cancelled draft", false);
        recordUndo->undo();
        auto undo = window.findChild<QUndoStack *>();
        QVERIFY(undo);
        undo->undo();
        QCOMPARE(classes->model()->rowCount(), classCount);
        undo->redo();
        QCOMPARE(classes->model()->rowCount(), classCount + 1);
        undo->undo(); // Leave the window clean for teardown.
        auto screenshot = qEnvironmentVariable("LORDLY_TEST_SCREENSHOT");
        if (!screenshot.isEmpty()) {
            QTest::qWait(100);
            QVERIFY(window.grab().save(screenshot));
            const auto capturePrefix = QFileInfo(screenshot).absolutePath() + "/" + QFileInfo(screenshot).completeBaseName();
            auto workspaceTabs = qobject_cast<QTabWidget *>(window.centralWidget());
            for (int i = 0; i < workspaceTabs->count(); ++i)
                if (workspaceTabs->tabText(i) == "Consumables" || workspaceTabs->tabText(i) == "Items") {
                    workspaceTabs->setCurrentIndex(i);
                    window.resize(1440, 900);
                    QTest::qWait(50);
                    QVERIFY(window.grab().save(capturePrefix + "-" + workspaceTabs->tabText(i).toLower() + ".png"));
                }
            window.navigateTo({"items", 1, -1, "section:Resistance"});
            itemView->setCurrentIndex(itemView->findData("Resistance"));
            QTest::qWait(50);
            QVERIFY(window.grab().save(capturePrefix + "-item-resistance.png"));
            window.navigateTo({"classes", 0x76, -1, "section:Growth"});
            auto help = window.findChild<QLabel *>("defaultCurrentClassHelp");
            QVERIFY(help);
            for (QWidget *parent = help->parentWidget(); parent; parent = parent->parentWidget())
                if (auto scroll = qobject_cast<QScrollArea *>(parent)) {
                    scroll->ensureWidgetVisible(help);
                    break;
                }
            QTest::qWait(50);
            QVERIFY(window.grab().save(capturePrefix + "-class-help.png"));
            window.navigateTo({"classes", 5, -1, "section:Promotion"});
            auto promotion = window.findChild<PromotionEditor *>();
            for (QWidget *parent = promotion->parentWidget(); parent; parent = parent->parentWidget())
                if (auto scroll = qobject_cast<QScrollArea *>(parent)) {
                    scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
                    break;
                }
            QTest::qWait(50);
            QVERIFY(window.grab().save(capturePrefix + "-family-links.png"));
            for (const auto name : {"encounterEditor", "scenarioEditor"}) {
                workspaceTabs->setCurrentWidget(window.findChild<QWidget *>(name));
                for (int width : {1440, 1100}) {
                    window.resize(width, 900);
                    QTest::qWait(50);
                    QVERIFY(window.grab().save(capturePrefix + QString("-%1-%2.png").arg(name).arg(width)));
                }
            }
            // Optional visual captures use the same loaded desktop, including its
            // advanced-tool navigation. No extra ROM or export fixture is needed.
            auto artwork = window.findChild<QTabWidget *>("artWorkspace");
            if (artwork) {
                auto workspaces = qobject_cast<QTabWidget *>(window.centralWidget());
                workspaces->setCurrentWidget(artwork);
                artwork->setCurrentIndex(artwork->count() - 1);
                const auto prefix = QFileInfo(screenshot).absolutePath() + "/" + QFileInfo(screenshot).completeBaseName();
                for (int width : {1440, 1100}) {
                    window.resize(width, 900);
                    QTest::qWait(50);
                    QVERIFY(window.grab().save(prefix + QString("-advanced-%1.png").arg(width)));
                }
            }
        }
    }
    void categoryIconEditingAndNavigation() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty())
            QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path));
            project.loadPromotion();
            project.loadCategoryIcons();
            auto promotion = *project.promotion();
            const int family = promotion.createFamily("Test crest family");
            promotion.assign(1, family);
            project.setPromotion(promotion.json());
            QUndoStack undo;
            PromotionEditor families(&project, &undo, [] {});
            families.selectClass(1);
            CategoryIconEditor editor(&project, &undo, [] {});
            editor.resize(1000, 650);
            editor.show();
            QTest::qWait(20);
            QCOMPARE(editor.findChild<QListWidget *>("categoryIcons")->count(), 8);
            const auto before = *project.categoryIcons();
            auto canvas = editor.findChild<SpriteCanvas *>("categoryIconCanvas");
            QVERIFY(canvas);
            const int point = 5 * 14 + 5;
            const auto color = quint8(before.indices(1)[point]) ^ 1;
            canvas->setIndexColor(color);
            QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier, {110, 110});
            QCOMPARE(quint8(project.categoryIcons()->indices(1)[point]), color);
            QCOMPARE(project.categoryIcons()->indices(2), before.indices(2));
            undo.undo();
            QCOMPARE(project.categoryIcons()->json(), before.json());
            undo.redo();
            QTimer::singleShot(0, [] {
                for (auto widget : QApplication::topLevelWidgets())
                    if (auto dialog = qobject_cast<QInputDialog *>(widget)) {
                        dialog->setTextValue("New crest");
                        dialog->accept();
                    }
            });
            editor.createIcon(family, true);
            const auto newIcon = editor.selectedCategory();
            QCOMPARE(newIcon, 9);
            QCOMPARE(project.categoryIcons()->familyIcon(family), 9);
            const auto beforeInvalid = project.categoryIcons()->json();
            QTimer::singleShot(0, [] {
                for (auto widget : QApplication::topLevelWidgets())
                    if (auto dialog = qobject_cast<QInputDialog *>(widget)) {
                        dialog->setTextValue("   ");
                        dialog->accept();
                    }
            });
            editor.createIcon(family);
            QCOMPARE(project.categoryIcons()->json(), beforeInvalid);
            QVERIFY(!editor.findChild<QLabel *>("categoryIconNotice")->text().isEmpty());
            QCOMPARE(families.findChild<QComboBox *>("familyTabIcon")->currentData().toInt(), 9);
            undo.undo();
            QCOMPARE(project.categoryIcons()->iconIds().size(), 8);
            undo.redo();
            editor.selectCategory(9);
            QImage replacement(14, 14, QImage::Format_RGBA8888);
            replacement.fill(project.categoryIcons()->palette()[0]);
            editor.assignImage(replacement);
            QCOMPARE(project.categoryIcons()->image(9), replacement);
            auto picker = families.findChild<QComboBox *>("familyTabIcon");
            picker->setCurrentIndex(picker->findData(2));
            QCOMPARE(project.categoryIcons()->familyIcon(family), 2);
            undo.undo();
            QCOMPARE(project.categoryIcons()->familyIcon(family), 9);
            const auto capture = qEnvironmentVariable("LORDLY_TEST_SCREENSHOT");
            if (!capture.isEmpty())
                QVERIFY(editor.grab().save(capture + "-category-icons.png"));
            MainWindow window;
            window.resize(1100, 780);
            window.show();
            window.loadRom(path);
            QTRY_VERIFY_WITH_TIMEOUT(window.findChild<CategoryIconEditor *>() != nullptr, 30000);
            auto familyEditor = window.findChild<PromotionEditor *>();
            QVERIFY(familyEditor);
            familyEditor->findChild<QPushButton *>("editFamilyIcon")->click();
            auto workspaces = qobject_cast<QTabWidget *>(window.centralWidget());
            auto art = window.findChild<QTabWidget *>("artWorkspace");
            auto icons = window.findChild<QTabWidget *>("uiAndIcons");
            QCOMPARE(workspaces->currentWidget(), art);
            QCOMPARE(art->currentWidget(), icons);
            QCOMPARE(icons->currentWidget(), window.findChild<CategoryIconEditor *>());
            if (!capture.isEmpty())
                QVERIFY(window.grab().save(capture + "-category-workspace.png"));
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void promotionSourcesForAllClasses() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM");
        if (path.isEmpty()) QSKIP("Private ROM required");
        auto project = Project::fromRom(Rom::open(path));
        project.loadPromotion();
        QUndoStack undo;
        PromotionEditor editor(&project, &undo, [] {});
        ClassSetup overview(&project, [](const EditLocation &) {});
        auto family = editor.findChild<QComboBox *>("classFamily");
        auto mask = editor.findChild<QComboBox *>("compatibilityMask");
        auto equipment = editor.findChild<QCheckBox *>("standardEquipmentList");
        auto details = editor.findChild<QWidget *>("promotionDetails");
        auto notice = editor.findChild<QLabel *>("promotionNotice");
        auto jump = editor.findChild<QPushButton *>("goToPromotionClass");
        auto summary = overview.findChild<QLabel *>("classSetupFamily");
        auto checklist = overview.findChild<QLabel *>("classSetupChecklist");
        QVERIFY(family && mask && equipment && details && notice && jump && summary && checklist);
        QSignalSpy requests(&editor, &PromotionEditor::promotionClassRequested);
        // US Rev 0 creation defaults and named-menu aliases, audited directly
        // from the read-only ROM. Unlisted records keep their own class ID.
        const QMap<int, int> mapped{
            {0x5A,0x18}, {0x5B,0x20}, {0x5D,0x72}, {0x5E,0x23},
            {0x6A,0x69}, {0x6B,0x69}, {0x6D,0x6C}, {0x6E,0x6C},
            {0x75,0x08}, {0x76,0x06}, {0x77,0x12}, {0x78,0x0D},
            {0x79,0x27}, {0x7A,0x1F}, {0x7B,0x19}, {0x7C,0x15},
            {0x89,0x0D}, {0x8A,0x06}, {0x8B,0x09}, {0x8C,0x0F},
            {0x8D,0x0C}, {0x8E,0x08}, {0x8F,0x06}, {0x90,0x05},
            {0x91,0x1A}, {0x92,0x0A}, {0x93,0x73}, {0x94,0x1B},
            {0x95,0x20}, {0x96,0x15}, {0x97,0x1D}, {0x98,0x1F},
            {0x99,0x36}, {0x9A,0x16}, {0x9B,0x73}, {0x9C,0x14},
            {0x9D,0x31}, {0x9E,0x26}, {0x9F,0x74}, {0xA0,0x13}, {0xA2,0x13}};
        for (int expanded = 0; expanded < 2; ++expanded) {
            if (expanded) {
                RecordModel model(&project, Table::Classes, &undo);
                QCOMPARE(model.appendClass(5, "Test class"), 165);
            }
            const auto original = project.promotion()->json();
            const auto classes = project.classData();
            QCOMPARE(classes.entries().size(), 165 + expanded);
            for (int id = 0; id < classes.entries().size(); ++id) {
                // Seed unrelated values before every selection, including
                // classes without a promotion row, to expose stale controls.
                editor.selectClass(23);
                editor.selectClass(id);
                overview.selectClass(id);
                const int source = mapped.value(id, id);
                QCOMPARE(classes.defaultPromotionClass(id), source);
                const bool present = source > 0 && source < project.promotion()->classCount();
                const bool redirected = present && source != id;
                QCOMPARE(details->isHidden(), !present);
                QCOMPARE(jump->isHidden(), !redirected);
                QCOMPARE(family->isEnabled(), present && !redirected);
                QCOMPARE(mask->isEnabled(), present && !redirected);
                QCOMPARE(equipment->isEnabled(), present && !redirected);
                for (int n = 0; n < 8; ++n) {
                    auto gate = editor.findChild<QSpinBox *>(QString("promotionGate%1").arg(n));
                    QVERIFY(gate);
                    QCOMPARE(gate->isEnabled(), present && !redirected);
                    QCOMPARE(gate->value(), present ? int(u8(project.promotion()->gates, source * 8 + n)) : 0);
                    if (present) QVERIFY(!gate->text().isEmpty());
                }
                if (present) {
                    const int expectedFamily = project.promotion()->families["assignments"].toArray()[source].toInt();
                    QCOMPARE(family->currentData().toInt(), expectedFamily);
                    QCOMPARE(mask->currentData().toInt(), int(u8(project.promotion()->masks, source)));
                    QCOMPARE(equipment->isChecked(), bool(u8(project.promotion()->flags, source) & 0x80));
                    if (expectedFamily) QVERIFY(!checklist->text().contains("No class family assigned"));
                } else {
                    QCOMPARE(family->currentIndex(), -1);
                    QCOMPARE(mask->count(), 0);
                    QVERIFY(!equipment->isChecked());
                    QVERIFY(summary->text().contains("No manual class-change entry"));
                }
                if (redirected) {
                    const auto name = project.record(Table::Classes, source).name.simplified();
                    QVERIFY(notice->text().contains(name));
                    QVERIFY(summary->text().contains(name));
                    requests.clear(); jump->click();
                    QCOMPARE(requests.size(), 1);
                    QCOMPARE(requests.first().first().toInt(), source);
                }
                QCOMPARE(project.promotion()->json(), original);
            }
        }
        // A modified default must change the preview; a zero default must
        // not keep applying a named alias that the runtime no longer uses.
        RecordModel model(&project, Table::Classes, &undo);
        int column = -1;
        for (int n = 0; n < fields(Table::Classes).size(); ++n)
            if (fields(Table::Classes)[n].key == "classCopyMatch") column = n + 2;
        QVERIFY(column >= 2);
        QVERIFY(model.setData(model.index(0x89, column), 23, Qt::EditRole));
        editor.selectClass(0x89);
        QVERIFY(jump->text().contains("Cataphract"));
        undo.undo(); editor.selectClass(0x89);
        QVERIFY(jump->text().contains("Archer"));
        QVERIFY(model.setData(model.index(0x75, column), 0, Qt::EditRole));
        editor.selectClass(0x75);
        QVERIFY(jump->isHidden());
        QCOMPARE(family->currentData().toInt(), 0);
        undo.undo(); editor.selectClass(0x75);
        QCOMPARE(family->currentData().toInt(), 1);
        QVERIFY(jump->text().contains("Phalanx"));

        // The real window handoff must select the referenced class even when
        // a search hides it, and keep the user on the Promotion page.
        MainWindow window; window.resize(1440, 1000); window.show(); window.loadRom(path);
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<PromotionEditor *>(), 30000);
        window.navigateTo({"classes", 0x75, -1, "section:Promotion"});
        auto search = window.findChild<QLineEdit *>("classesSearch");
        search->setText("Troi");
        auto panel = window.findChild<PromotionEditor *>();
        QVERIFY(panel->findChild<QLabel *>("promotionNotice")->text().contains("Phalanx"));
        QVERIFY(window.grab().save(QCoreApplication::applicationDirPath() + "/promotion-troi.png"));
        panel->findChild<QPushButton *>("goToPromotionClass")->click();
        QCOMPARE(search->text(), QString());
        auto table = window.findChild<QTableView *>("classesTable");
        QCOMPARE(table->model()->index(table->currentIndex().row(), 0).data(Qt::EditRole).toInt(), 8);
        QCOMPARE(window.findChild<QComboBox *>("classesInspectorSection")->currentText(), "Promotion");
        QVERIFY(panel->findChild<QComboBox *>("classFamily")->isEnabled());
        // If an edited default points to another named identity, navigation
        // must open that exact promotion row, not follow its alias again.
        auto proxy = qobject_cast<QAbstractProxyModel *>(table->model()); QVERIFY(proxy);
        auto records = qobject_cast<RecordModel *>(proxy->sourceModel()); QVERIFY(records);
        QCOMPARE(records->appendClass(5, "Navigation test"), 165);
        QVERIFY(records->setData(records->index(0x89, column), 0x75, Qt::EditRole));
        window.navigateTo({"classes", 0x89, -1, "section:Promotion"});
        panel->findChild<QPushButton *>("goToPromotionClass")->click();
        QCOMPARE(table->model()->index(table->currentIndex().row(), 0).data(Qt::EditRole).toInt(), 0x75);
        auto ownFamily = panel->findChild<QComboBox *>("classFamily");
        QVERIFY(ownFamily->isEnabled()); QCOMPARE(ownFamily->currentData().toInt(), 0);
        panel->findChild<QPushButton *>("goToPromotionClass")->click();
        QVERIFY(!ownFamily->isEnabled()); QCOMPARE(ownFamily->currentData().toInt(), 1);
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
        auto category=editor.findChild<QComboBox *>("compatibilityMask");QVERIFY(category);
        auto family = editor.findChild<QComboBox *>("classFamily");
        QCOMPARE(family->currentText(), QString("1 Â· Male"));
        editor.selectClass(38); // Lich uses both compatibility bits, but family None.
        QCOMPARE(category->currentData().toInt(), 3);
        QCOMPARE(family->currentData().toInt(), 0);
        editor.selectClass(5);
        QCOMPARE(project.promotion()->json(), original); // Labels alone never rewrite a project.
        QCOMPARE(category->currentText(),QString("Male"));
        category->setCurrentIndex(category->findData(2));
        QCOMPARE(u8(project.promotion()->masks,5),quint8(2));
        QCOMPARE(project.promotion()->families,project.promotion()->originalFamilies);
        undo.undo();QCOMPARE(category->currentText(),QString("Male"));
        QTimer::singleShot(0,&editor,[&] {
            auto dialog=editor.findChild<QDialog *>("compatibilityCategoryDialog");QVERIFY(dialog);
            dialog->findChild<QCheckBox *>("compatibilityBit_128")->setChecked(true);dialog->accept();
        });
        category->setCurrentIndex(category->findData(-1));
        QCOMPARE(u8(project.promotion()->masks,5),quint8(129));
        QVERIFY(category->currentText().contains("Male + Goblins"));
        undo.undo();QCOMPARE(project.promotion()->json(),original);
        undo.redo();QCOMPARE(category->currentData().toInt(),129);undo.undo();
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
        auto rules = editor.findChild<QComboBox *>("newFamilyRules");
        QVERIFY(rules);
        QCOMPARE(rules->currentData().toInt(), int(Promotion::FamilyRules::WithinFamily));
        const auto priorAssignments = project.promotion()->families["assignments"].toArray();
        editor.findChild<QPushButton *>("createFamily")->click();
        QCOMPARE(project.promotion()->families["assignments"].toArray()[5].toInt(), 16);
        QCOMPARE(project.promotion()->families["families"].toArray().last().toObject()["behavior"].toInt(), 5);
        for (int id = 0; id < priorAssignments.size(); ++id)
            if (id != 5)
                QCOMPARE(project.promotion()->families["assignments"].toArray()[id], priorAssignments[id]);
        QCOMPARE(gate->value(), 90); // Creating a family does not overwrite class requirements.
        undo.undo();
        QCOMPARE(project.promotion()->families, project.promotion()->originalFamilies);
        undo.redo();
        QCOMPARE(editor.findChild<QComboBox *>("classFamily")->currentData().toInt(), 16);
        QVERIFY(family->currentText().contains("Order of Dawn"));
        auto links = editor.findChild<QListWidget *>("familyLinks");
        for (int row = 0; row < links->count(); ++row)
            if (links->item(row)->data(Qt::UserRole).toInt() == 1) links->item(row)->setCheckState(Qt::Checked);
        const auto unlinked = project.promotion()->json();
        editor.findChild<QPushButton *>("applyFamilyLinks")->click();
        QCOMPARE(project.promotion()->linkedFamilies(16), QSet<int>{1});
        QCOMPARE(project.promotion()->linkedFamilies(1), QSet<int>{16});
        QCOMPARE(u32(project.promotion()->decodedFamilies(), 648 + 5 * 4), quint32(0x50001));
        undo.undo(); QCOMPARE(project.promotion()->json(), unlinked);
        undo.redo();
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
        const QJsonValue previousFamily = project.promotion()->families["assignments"].toArray()[165];
        editor.findChild<QLineEdit *>("newFamilyName")->setText("Added class family");
        rules->setCurrentIndex(rules->findData(int(Promotion::FamilyRules::WithLich)));
        editor.findChild<QPushButton *>("createFamily")->click();
        QCOMPARE(project.promotion()->families["assignments"].toArray()[165].toInt(), 17);
        QCOMPARE(project.promotion()->families["families"].toArray().last().toObject()["behavior"].toInt(), 2);
        undo.undo();
        QCOMPARE(project.promotion()->families["assignments"].toArray()[165], previousFamily);
        undo.redo();
        QCOMPARE(editor.findChild<QComboBox *>("classFamily")->currentData().toInt(), 17);
        QCOMPARE(model.createClass("Blank class"),166);editor.selectClass(166);
        QCOMPARE(editor.findChild<QComboBox *>("classFamily")->currentData().toInt(),0);
        QCOMPARE(category->currentText(),QString("Male"));
        QScrollArea inspector;inspector.setWidgetResizable(true);inspector.setWidget(&editor);
        const auto detach=qScopeGuard([&]{inspector.takeWidget();});
        inspector.resize(520,760);inspector.show();QTest::qWait(30);
        inspector.ensureWidgetVisible(category);
        for(auto label:editor.findChildren<QLabel *>())
            if(label->wordWrap())QVERIFY2(label->height()>=label->heightForWidth(label->width()),qPrintable(label->text()));
        QVERIFY(inspector.grab().save("native/build/named-compatibility-category.png"));
    }
};
QTEST_MAIN(UiTests)
#include "ui_tests.moc"
