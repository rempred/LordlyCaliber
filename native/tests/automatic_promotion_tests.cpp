#include "app/promotion_editor.h"
#include "engine/automatic_promotion.h"
#include "engine/automatic_promotion_owner.h"
#include "engine/capacity_placement.h"
#include "rebuild/compiler.h"
#include <QComboBox>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QtTest>
using namespace lordly;
class AutomaticPromotionTests : public QObject {
    Q_OBJECT
    Rom source() const { return Rom::open(qEnvironmentVariable("LORDLY_TEST_ROM")); }
  private slots:
    void initTestCase() {
        if (qEnvironmentVariableIsEmpty("LORDLY_TEST_ROM")) QSKIP("Private ROM required");
    }
    void routesAndLegacyProjects() {
        auto project = Project::fromRom(source()); project.loadPromotion();
        auto p = *project.promotion();
        QCOMPARE(p.automatic.routes(79).first().id, 16);
        QCOMPARE(p.automatic.value(16, 0), 104); QCOMPARE(p.automatic.value(16, 1), 129);
        QCOMPARE(p.automatic.routes(80).first().id, 17);
        QCOMPARE(p.automatic.routes(47).size(), 2);
        QCOMPARE(p.automatic.routes(48).first().id, 20);
        QCOMPARE(p.automatic.routes(37).first().id, 15);
        QCOMPARE(p.automatic.routes(36).first().id, 21);
        QVERIFY(p.automatic.routes(5).isEmpty());
        auto old = p.json(); old.remove("automatic");
        p.automatic.set(16, 0, 10); p.apply(old);
        QCOMPARE(p.automatic.value(16, 0), 104);
        QVERIFY(!p.modified());
        auto invalid = p.automatic.json(); auto bytes = QByteArray::fromHex(invalid["attacks"].toString().toLatin1());
        bytes[0] = 5; invalid["attacks"] = QString::fromLatin1(bytes.toHex());
        QVERIFY_THROWS_EXCEPTION(Error, p.automatic.apply(invalid));
        QCOMPARE(p.automatic.value(16, 0), 104);
        QCOMPARE(p.automatic.rebuild(source().bytes()), source().bytes());
        const auto originalOwner = source().bytes().mid(0x1b6538, 0xedc);
        p.automatic.set(21, 2, 17);
        const auto editedOwner = p.automatic.rebuild(source().bytes()).mid(0x1b6538, 0xedc);
        QCOMPARE(canonicalAutomaticRequirementOwner(editedOwner, 0x1b6538), originalOwner);
        auto unknown = editedOwner; unknown[0] = char(quint8(unknown[0]) ^ 1);
        QVERIFY(canonicalAutomaticRequirementOwner(unknown, 0x1b6538) != originalOwner);
        unknown = editedOwner; unknown[0x1b6680 - 0x1b6538] = 0;
        QVERIFY(canonicalAutomaticRequirementOwner(unknown, 0x1b6538) != originalOwner);
    }
    void classControlsUndoAndPersistence() {
        auto project = Project::fromRom(source()); project.loadPromotion();
        QUndoStack undo; PromotionEditor editor(&project, &undo, [] {});
        editor.selectClass(79); editor.show();
        auto route = editor.findChild<QComboBox *>("promotionRequirementRoute");
        auto strength = editor.findChild<QSpinBox *>("promotionGate0");
        auto vitality = editor.findChild<QSpinBox *>("promotionGate1");
        auto intelligence = editor.findChild<QSpinBox *>("promotionGate2");
        QCOMPARE(route->currentData().toInt(), 16);
        QCOMPARE(strength->value(), 104); QCOMPARE(vitality->value(), 129);
        QVERIFY(intelligence->isHidden());
        const auto manual = project.promotion()->gates;
        strength->setValue(10); vitality->setValue(10);
        QCOMPARE(project.promotion()->automatic.value(16, 0), 10);
        QCOMPARE(project.promotion()->automatic.value(16, 1), 10);
        QCOMPARE(project.promotion()->gates, manual);
        QVERIFY(project.changeReport().text().contains("Petrification"));
        undo.undo(); QCOMPARE(vitality->value(), 129);
        undo.redo(); QCOMPARE(vitality->value(), 10);
        auto reopened = Project::fromRom(source()); reopened.applyManifest(project.manifest()); reopened.loadPromotion();
        QCOMPARE(reopened.promotion()->json(), project.promotion()->json());
        route->setCurrentIndex(route->findData(-1));
        QVERIFY(!intelligence->isHidden()); strength->setValue(11);
        QCOMPARE(u8(project.promotion()->gates, 79 * 8), 11);
        QCOMPARE(project.promotion()->automatic.value(16, 0), 10);
        editor.selectClass(36); QCOMPARE(route->currentData().toInt(), 21);
        QVERIFY(strength->isHidden()); QCOMPARE(intelligence->value(), 60);
        intelligence->setValue(17); QCOMPARE(project.promotion()->automatic.value(21, 2), 17);
        editor.selectClass(45); QCOMPARE(route->currentData().toInt(), 22);
        QVERIFY(strength->isHidden()); QVERIFY(intelligence->isHidden());
        editor.selectClass(5); QCOMPARE(route->currentData().toInt(), -1);
        QVERIFY(!strength->isHidden());
    }
    void exportedConsumers_data() {
        QTest::addColumn<bool>("expanded");
        QTest::newRow("retail") << false;
        QTest::newRow("expanded-classes") << true;
    }
    void exportedConsumers() {
        QFETCH(bool, expanded);
        const auto tools = qEnvironmentVariable("LORDLY_N64_TOOLCHAIN");
        if (tools.isEmpty()) QSKIP("Private compiler required");
        try {
        auto project = Project::fromRom(source()); project.loadPromotion();
        if (expanded) QCOMPARE(project.appendClass(5, "Requirement test class"), 165);
        auto p = *project.promotion();
        p.automatic.set(16, 0, 10); p.automatic.set(16, 1, 11);
        p.automatic.set(17, 0, 12); p.automatic.set(17, 1, 13);
        p.automatic.set(12, 0, 14); p.automatic.set(12, 4, 15); p.automatic.set(12, 5, 16);
        p.automatic.set(21, 2, 17); p.automatic.set(21, 3, 18); p.automatic.set(21, 5, 19);
        p.automatic.set(21, 6, 20); p.automatic.set(21, 7, 80);
        project.setPromotion(p.json());
        QTemporaryDir directory(QFileInfo(qEnvironmentVariable("LORDLY_TEST_ROM")).absoluteDir()
                                    .filePath("Test ROMs/automatic-requirements-XXXXXX"));
        QVERIFY(directory.isValid()); directory.setAutoRemove(false);
        const auto output = directory.filePath(QFileInfo(qEnvironmentVariable("LORDLY_TEST_ROM")).fileName());
        const auto built = rebuild(project, tools, directory.path());
        exportRom(project, built, output);
        auto reopened = Project::fromRom(Rom::open(output)); reopened.loadPromotion();
        QCOMPARE(reopened.promotion()->json(), p.json());
        const auto again = rebuild(reopened, tools, directory.path());
        QCOMPARE(again.image, Rom::open(output).bytes());
        QProcess probe;
        probe.start("node", {QFINDTESTDATA("automatic-promotion-mips.cjs"), output});
        QVERIFY(probe.waitForFinished(60000));
        QVERIFY2(probe.exitCode() == 0, (probe.readAllStandardError() + probe.readAllStandardOutput()).constData());
        qInfo().noquote() << probe.readAllStandardOutput();
        qInfo().noquote() << "Automatic requirements export:" << output;
        } catch (const std::exception &error) { QFAIL(error.what()); }
    }
};
QTEST_MAIN(AutomaticPromotionTests)
#include "automatic_promotion_tests.moc"
