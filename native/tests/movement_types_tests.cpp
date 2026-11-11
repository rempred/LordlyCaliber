#include "app/main_window.h"
#include "app/movement_types_editor.h"
#include "app/record_model.h"
#include "app/value_editor.h"
#include "engine/movement_runtime.h"
#include "engine/new_campaign.h"
#include "engine/route_preview.h"
#include "engine/save_document.h"
#include "rebuild/compiler.h"
#include <QComboBox>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineEdit>
#include <QListWidget>
#include <QProcess>
#include <QPushButton>
#include <QSpinBox>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
#include <algorithm>
using namespace lordly;
class MovementTypesTests : public QObject {
    Q_OBJECT
    Rom rom() {
        return Rom::open(qEnvironmentVariable("LORDLY_TEST_ROM"));
    }
    QJsonObject catalog() {
        QFile f(":/catalog.json");
        if (!f.open(QIODevice::ReadOnly))
            throw Error("Missing catalog");
        return QJsonDocument::fromJson(f.readAll()).object();
    }
  private slots:
    void initTestCase() {
        if (qEnvironmentVariableIsEmpty("LORDLY_TEST_ROM"))
            QSKIP("Private original ROM required");
    }
    void retailAndCustom() {
        try {
            auto m = MovementTypes::load(rom());
            QVERIFY(!m.requiresRuntime());
            QCOMPARE(m.speed(2, 1), 33.);
            QCOMPARE(m.speed(4, 1), 31.);
            QCOMPARE(m.speed(4, 7), 27.);
            QCOMPARE(m.speed(7, 1), 0.);
            int id = m.create("Scout", 2);
            QCOMPARE(id, 8);
            auto t = m.entries()[id];
            t.baseSpeed = 60;
            m.set(id, t);
            QCOMPARE(m.speed(id, 1), 58.);
            QCOMPARE(m.speed(id, 5), 52.);
            QCOMPARE(m.speed(id, 7), 44.);
            QCOMPARE(m.speed(id, 8), 0.);
            auto invalid = m.json();
            invalid["priority"] = QJsonArray{8};
            QVERIFY_THROWS_EXCEPTION(Error, m.apply(invalid));
            QCOMPARE(m.entries()[id].name, "Scout");
            m.remove(id);
            QVERIFY(!m.modified());
            QVERIFY_THROWS_EXCEPTION(Error, m.remove(2));
            const auto neutral=m.create("Neutral");
            const auto &n=m.entries()[neutral];
            QCOMPARE(n.rules,MovementRules{});QCOMPARE(n.costs[5],1);QCOMPARE(n.costs[0],65535);
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void authoringAndPersistence() {
        try {
            auto p = Project::fromRom(rom(), catalog());
            QUndoStack undo;
            RecordModel model(&p, Table::Classes, &undo);
            const auto before = p.manifest();
            const auto f =
                *std::find_if(fields(Table::Classes).begin(), fields(Table::Classes).end(),
                              [](const Field &f) { return f.key == "moveType"; });
            ValueEditor select(f, p.catalog(), [](int) {});
            select.setValue(2);
            select.setCreateChoice("New movement type…", [&] {
                if (auto c = requestMovementType(&select, p))
                    model.createMovementType(c->name, c->copyFrom, c->baseSpeed, 5);
            });
            QTimer::singleShot(0, [&] {
                auto d = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                QVERIFY(d);
                QCOMPARE(d->findChild<QComboBox *>("newMovementCopy")->currentData().toInt(),0);
                d->findChild<QLineEdit *>("newMovementName")->setText("Scout");
                d->findChild<QSpinBox *>("newMovementSpeed")->setValue(60);
                d->accept();
            });
            auto combo = select.findChild<QComboBox *>();
            combo->setCurrentIndex(combo->findData(-1));
            QCOMPARE(u8(p.record(Table::Classes, 5).bytes, 44), 8);
            QCOMPARE(p.movementTypes().entries()[8].baseSpeed, 60);
            undo.undo();
            QCOMPARE(p.manifest(), before);
            undo.redo();
            QCOMPARE(p.movementTypes().entries()[8].name, "Scout");
            MovementTypesEditor editor(&p, &undo, [] {});
            editor.show();
            auto list = editor.findChild<QListWidget *>("movementTypesList");
            for (int i = 0; i < list->count(); i++)
                if (list->item(i)->data(Qt::UserRole).toInt() == 8)
                    list->setCurrentRow(i);
            editor.findChild<QSpinBox *>("movementBaseSpeed")->setValue(64);
            QCOMPARE(p.movementTypes().entries()[8].baseSpeed,64);
            QVERIFY(list->currentItem()->text().contains("speed 64"));
            QCOMPARE(p.movementTypes().speed(8,1),62.);
            undo.undo();QCOMPARE(p.movementTypes().entries()[8].baseSpeed,60);
            editor.findChild<QSpinBox *>("movementCost5")->setValue(3);
            QCOMPARE(p.movementTypes().speed(8, 5), 54.);
            undo.undo();
            QCOMPARE(p.movementTypes().speed(8, 5), 58.);
            undo.redo();
            editor.findChild<QSpinBox *>("movementRule_fatigue")->setValue(7);
            QCOMPARE(p.movementTypes().entries()[8].rules.fatigue,7);
            QVERIFY(!p.movementTypes().entries()[8].rules.mapFlight);
            editor.findChild<QCheckBox *>("movementRule_mapFlight")->setChecked(true);
            QVERIFY(p.movementTypes().entries()[8].rules.mapFlight);
            QVERIFY(!p.movementTypes().entries()[8].rules.airborne);
            undo.undo();QVERIFY(!p.movementTypes().entries()[8].rules.mapFlight);undo.redo();
            editor.resize(1180,820);QApplication::processEvents();
            QVERIFY(editor.grab().save(QString::fromUtf8(LORDLY_SOURCE_DIR)+"/build/movement-rules-ui.png"));
            QVERIFY_THROWS_EXCEPTION(Error, p.deleteMovementType(8));
            QTemporaryDir dir;
            p.saveArchive(dir.filePath("movement.lcproj"));
            auto reopened = Project::fromRom(rom(), catalog());
            reopened.applyManifest(Project::readProject(dir.filePath("movement.lcproj")));
            QCOMPARE(reopened.manifest(), p.manifest());
            auto priority = p.movementTypes().priority();
            priority.removeAll(8);
            priority.prepend(8);
            auto types = p.movementTypes();
            types.setPriority(priority);
            p.setMovementTypes(types.json());
            QCOMPARE(
                p.classData().unitMovementType({{5, 5}, {6, 6}}, rom().bytes().mid(0x5cebc, 7)), 8);
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void mainWindowCreation() {
        try {
            MainWindow window;
            window.loadRom(qEnvironmentVariable("LORDLY_TEST_ROM"));
            window.show();
            QTRY_VERIFY_WITH_TIMEOUT(window.findChild<QWidget *>("classMovementType"), 60000);
            auto editor = window.findChild<QWidget *>("classMovementType");
            QVERIFY(editor->isEnabled());
            QTimer::singleShot(0, [&] {
                auto dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                QVERIFY(dialog);
                dialog->findChild<QLineEdit *>("newMovementName")->setText("Window Scout");
                dialog->findChild<QSpinBox *>("newMovementSpeed")->setValue(60);
                dialog->accept();
            });
            auto choice = editor->findChild<QComboBox *>();
            choice->setCurrentIndex(choice->findData(-1));
            QTRY_VERIFY(choice->currentText().contains("Window Scout"));
            auto movement = window.findChild<QWidget *>("movementTypesEditor");
            QVERIFY(movement);
            auto list = movement->findChild<QListWidget *>("movementTypesList");
            QCOMPARE(list->findItems("Window Scout", Qt::MatchContains).size(), 1);
            auto undo = window.findChild<QUndoStack *>();
            QVERIFY(undo);
            undo->undo();
            QVERIFY(choice->findText("Window Scout", Qt::MatchContains) < 0);
            undo->redo();
            QVERIFY(choice->currentText().contains("Window Scout"));
            undo->undo();
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void exportAndReopen() {
        try {
            const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (tools.isEmpty())
                QSKIP("Local N64 compiler required");
            auto p = Project::fromRom(rom(), catalog());
            QTemporaryDir work;
            QCOMPARE(rebuild(p, tools, work.path()).image, rom().bytes());
            int id = p.createMovementType("Scout", 2);
            auto types = p.movementTypes();
            auto t = types.entries()[id];
            t.baseSpeed = 60;
            types.set(id, t);
            auto order = types.priority();
            order.removeAll(id);
            order.prepend(id);
            types.setPriority(order);
            p.setMovementTypes(types.json());
            auto cls = p.record(Table::Classes, 5).bytes;
            put8(cls, 44, quint8(id));
            p.replaceBytes(Table::Classes, 5, cls);
            int fly = p.createMovementType("Custom sky", 1);
            QCOMPARE(fly, 9);
            int fast = p.createMovementType("Fast", 2);
            types = p.movementTypes();
            t = types.entries()[fast];
            t.baseSpeed = 255;
            types.set(fast, t);
            p.setMovementTypes(types.json());
            QCOMPARE(p.createMovementType("Custom immobile", 7), 11);
            QCOMPARE(p.createMovementType("Independent rules",0),12);
            types=p.movementTypes();t=types.entries()[12];
            t.rules.mapFlight=true;t.rules.airborne=true;t.rules.flightProfile=3;
            t.rules.fatigue=7;t.rules.groupFatigue=4;t.rules.nightFatigue=2;
            t.rules.recovery=5;t.rules.nightRecovery=3;types.set(12,t);p.setMovementTypes(types.json());
            for(const auto pair:{qMakePair(6,12),qMakePair(7,9)}) {
                auto row=p.record(Table::Classes,pair.first).bytes;put8(row,44,pair.second);
                p.replaceBytes(Table::Classes,pair.first,row);
            }
            auto magnus = p.record(Table::Classes, 0x51).bytes;
            put8(magnus, 44, 8);
            p.replaceBytes(Table::Classes, 0x51, magnus);
            const auto result = rebuild(p, tools, work.path());
            auto open = Project::fromRom(Rom::fromBytes(result.image), catalog());
            QCOMPARE(open.movementTypes().json(), p.movementTypes().json());
            QCOMPARE(u8(open.record(Table::Classes, 5).bytes, 44), 8);
            QCOMPARE(rebuild(open, tools, work.path()).image, result.image);
            auto army = SaveDocument::newArmy(open);
            QCOMPARE(u8(army.memory(), 0x1971f0 + 24), 8);
            const auto packed = army.serialize();
            auto loaded = SaveDocument::open(Rom::fromBytes(result.image), packed);
            loaded.selectSlot(0);
            QCOMPARE(u8(loaded.memory(), 0x1971f0 + 24), 8);
            QCOMPARE(loaded.serialize(), packed);
            QCOMPARE(u8(NewCampaign::armyMemory(p, packed), 100 * 56 + 24), 8);
            const auto out = qEnvironmentVariable("LORDLY_MOVEMENT_TEST_OUTPUT");
            QVERIFY2(!out.isEmpty(), "Run-specific parent test-ROM directory required");
            QDir().mkpath(out);
            const auto file =
                QDir(out).filePath("Ogre Battle 64 - Person of Lordly Caliber (USA).z64");
            atomicWrite(file, result.image);
            QProcess probe;
            probe.start("node",
                        {QString::fromUtf8(LORDLY_SOURCE_DIR) + "/tests/movement-types-mips.cjs",
                         file, qEnvironmentVariable("LORDLY_TEST_ROM")});
            QVERIFY(probe.waitForFinished(120000));
            QVERIFY2(probe.exitCode() == 0,
                     (probe.readAllStandardOutput() + probe.readAllStandardError()).constData());
            probe.start("node",{QString::fromUtf8(LORDLY_SOURCE_DIR)+"/tests/movement-rules-mips.cjs",file,qEnvironmentVariable("LORDLY_TEST_ROM")});
            QVERIFY(probe.waitForFinished(120000));
            qInfo().noquote()<<probe.readAllStandardOutput();
            QVERIFY2(probe.exitCode()==0,probe.readAllStandardError().constData());
            p.loadTools();auto settings=p.tools()->json();auto global=settings["gameRules"].toObject();
            global["fatigueRate"]=200;global["recoveryRate"]=50;settings["gameRules"]=global;p.setTools(settings);
            const auto combined=rebuild(p,tools,work.path());
            auto combinedOpen=Project::fromRom(Rom::fromBytes(combined.image),catalog());combinedOpen.loadTools();
            QCOMPARE(combinedOpen.tools()->gameRules().value("fatigueRate"),200);
            QCOMPARE(combinedOpen.tools()->gameRules().value("recoveryRate"),50);
            QCOMPARE(combinedOpen.movementTypes().json(),p.movementTypes().json());
            QCOMPARE(rebuild(combinedOpen,tools,work.path()).image,combined.image);
            QDir().mkpath(QDir(out).filePath("global-cycles"));
            const auto combinedFile=QDir(out).filePath("global-cycles/Ogre Battle 64 - Person of Lordly Caliber (USA).z64");
            atomicWrite(combinedFile,combined.image);
            probe.start("node",{QString::fromUtf8(LORDLY_SOURCE_DIR)+"/tests/movement-rules-mips.cjs",combinedFile,qEnvironmentVariable("LORDLY_TEST_ROM"),"global"});
            QVERIFY(probe.waitForFinished(120000));qInfo().noquote()<<probe.readAllStandardOutput();
            QVERIFY2(probe.exitCode()==0,probe.readAllStandardError().constData());
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
    void flightAssignmentWithoutTypeEdits() {
        try {
            const auto tools=qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");if(tools.isEmpty())QSKIP("Local N64 compiler required");
            for(bool added:{false,true}) {
                auto p=Project::fromRom(rom(),catalog());int id=5;
                if(added)id=p.appendClass(0x27,"New flier");
                else {auto row=p.record(Table::Classes,5).bytes;put8(row,44,1);p.replaceBytes(Table::Classes,5,row);}
                QVERIFY(!p.movementTypes().modified());
                QTemporaryDir work;const auto built=rebuild(p,tools,work.path());
                auto reopened=Project::fromRom(Rom::fromBytes(built.image),catalog());
                QVERIFY(MovementPlacement::read(Rom::fromBytes(built.image)).has_value());
                QCOMPARE(reopened.movementTypes().json(),p.movementTypes().json());
                QCOMPARE(rebuild(reopened,tools,work.path()).image,built.image);
                const auto out=QDir(qEnvironmentVariable("LORDLY_MOVEMENT_TEST_OUTPUT")).filePath(added?"added-flier":"assigned-flier");QDir().mkpath(out);
                const auto file=QDir(out).filePath("Ogre Battle 64 - Person of Lordly Caliber (USA).z64");atomicWrite(file,built.image);
                QProcess probe;probe.start("node",{QString::fromUtf8(LORDLY_SOURCE_DIR)+"/tests/movement-rules-mips.cjs",file,qEnvironmentVariable("LORDLY_TEST_ROM"),"assignment",QString::number(id)});
                QVERIFY(probe.waitForFinished(120000));qInfo().noquote()<<probe.readAllStandardOutput();QVERIFY2(probe.exitCode()==0,probe.readAllStandardError().constData());
            }
        }catch(const std::exception &e){QFAIL(e.what());}
    }
    void expandedIntegration() {
        try {
            const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
            if (tools.isEmpty())
                QSKIP("Local N64 compiler required");
            const auto path = QDir(qEnvironmentVariable("LORDLY_MOVEMENT_TEST_OUTPUT"))
                                  .filePath("Ogre Battle 64 - Person of Lordly Caliber (USA).z64");
            auto p = Project::fromRom(Rom::open(path), catalog());
            QVERIFY(!p.expandedArmies());
            const int custom = p.appendClass(5, "Scout knight");
            QCOMPARE(u8(p.record(Table::Classes, custom).bytes, 44), 8);
            p.loadScenarios();
            auto scenarios = *p.scenarios();
            while (!p.expandedArmies()) {
                scenarios.addSquad(1, -1, p.classData());
                p.setScenarios(scenarios.json());
            }
            QTemporaryDir work;
            const auto built = rebuild(p, tools, work.path());
            const auto rom = Rom::fromBytes(built.image);
            auto opened = Project::fromRom(rom, catalog());
            QVERIFY(opened.expandedArmies());
            QCOMPARE(opened.movementTypes().json(), p.movementTypes().json());
            QCOMPARE(u8(opened.record(Table::Classes, custom).bytes, 44), 8);
            auto army = SaveDocument::newArmy(opened);
            const auto bytes = army.serialize();
            auto loaded = SaveDocument::open(rom, bytes, opened.records(Table::Classes).size());
            loaded.selectSlot(0);
            QCOMPARE(u8(loaded.memory(), 0x1971f0 + 24), 8);
            QCOMPARE(rebuild(opened, tools, work.path()).image, built.image);
        } catch (const std::exception &e) {
            QFAIL(e.what());
        }
    }
};
QTEST_MAIN(MovementTypesTests)
#include "movement_types_tests.moc"
