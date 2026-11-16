#include "rebuild/export_plan.h"
#include "engine/rom.h"
#include <QtTest>
using namespace lordly;

class ExportPlanTests final : public QObject {
    Q_OBJECT
  private slots:
    void dependenciesBeforeExecution() {
        QByteArray image(64, 0);
        int called = 0;
        ExportPlan missing(image);
        missing.add("child", {"missing"}, [&] { ++called; });
        QVERIFY_THROWS_EXCEPTION(Error, missing.run());
        QCOMPARE(called, 0);
        ExportPlan reversed(image);
        reversed.add("child", {"parent"}, [&] { ++called; });
        reversed.add("parent", {}, [&] { ++called; });
        QVERIFY_THROWS_EXCEPTION(Error, reversed.run());
        QCOMPARE(called, 0);
        ExportPlan cycle(image);
        cycle.add("one", {"two"}, [&] { ++called; });
        cycle.add("two", {"one"}, [&] { ++called; });
        QVERIFY_THROWS_EXCEPTION(Error, cycle.run());
        QCOMPARE(called, 0);
        QStringList actual;
        ExportPlan ordered(image);
        ordered.add("one", {}, [&] { actual << "one"; });
        ordered.add("independent", {}, [&] { actual << "independent"; });
        ordered.add("two", {"one"}, [&] { actual << "two"; });
        ordered.run();
        QCOMPARE(actual, QStringList({"one", "independent", "two"}));
    }
    void overlapIsNotAuthorizedByDependency() {
        const QByteArray original(64, 0);
        auto image = original;
        ExportPlan plan(image);
        plan.add("first", {}, [&] { image[20] = 1; });
        plan.add("second", {"first"}, [&] { image[20] = 2; });
        try { plan.run(); QFAIL("Undeclared overwrite accepted"); }
        catch (const Error &error) {
            const auto message = QString::fromUtf8(error.what());
            QVERIFY(message.contains("second overwrites first"));
            QVERIFY(message.contains("0x14..0x15"));
        }
        QCOMPARE(image, original);
    }
    void boundedCompositionAndAdjacentWrites() {
        QByteArray image(64, 0);
        ExportPlan plan(image);
        plan.add("first", {}, [&] { image[20] = 1; image[21] = 1; });
        plan.add("second", {"first"}, [&] { image[20] = 2; });
        plan.add("adjacent", {}, [&] { image[22] = 3; });
        plan.compose("second", "first", {{20,21}});
        plan.run();
        QCOMPARE(image.mid(20,3), QByteArray::fromHex("020103"));
        QCOMPARE(plan.changedSteps(), 3);

        auto original = image;
        ExportPlan tooWide(image);
        tooWide.add("first", {}, [&] { image[20] = 4; image[21] = 4; });
        tooWide.add("second", {"first"}, [&] { image[20] = 5; image[21] = 5; });
        tooWide.compose("second", "first", {{20,21}});
        QVERIFY_THROWS_EXCEPTION(Error, tooWide.run());
        QCOMPARE(image, original);
    }
    void fullOwnerCatchesUnchangedHookBytes() {
        const QByteArray original(64, 0);
        auto image = original;
        ExportPlan plan(image);
        // The replacement's changed byte is disjoint from the future hook.
        plan.add("whole-function", {}, [&] { image[8] = 1; }, {{8,32}});
        plan.add("inner-hook", {"whole-function"}, [&] { image[20] = 2; });
        QVERIFY_THROWS_EXCEPTION(Error, plan.run());
        QCOMPARE(image, original);
        ExportPlan noop(image);
        noop.add("inactive", {}, [] {}, {{8,32}});
        noop.add("hook", {}, [&] { image[20] = 2; });
        noop.run();
        QCOMPARE(image[20], char(2));
    }
    void failedBuilderRollsBackEarlierSteps() {
        const QByteArray original(64, 0);
        auto image = original;
        ExportPlan plan(image);
        plan.add("first", {}, [&] { image[8] = 1; });
        plan.add("throws", {"first"}, [&] { image[10] = 2; throw Error("compile failed"); });
        QVERIFY_THROWS_EXCEPTION(Error, plan.run());
        QCOMPARE(image, original);
    }
    void shrinkAppendAndReuse() {
        const QByteArray original(64, 7);
        auto image = original;
        ExportPlan bad(image);
        bad.add("reclaim", {}, [&] { image.resize(32); });
        bad.add("reuse", {"reclaim"}, [&] { image.append(QByteArray(16,9)); });
        QVERIFY_THROWS_EXCEPTION(Error, bad.run());
        QCOMPARE(image, original);
        ExportPlan good(image);
        good.add("reclaim", {}, [&] { image.resize(32); });
        good.add("reuse", {"reclaim"}, [&] { image.append(QByteArray(16,9)); });
        good.compose("reuse", "reclaim", {{32,48}});
        good.run();
        QCOMPARE(image, QByteArray(32,7)+QByteArray(16,9));
    }
    void derivedChecksumDoesNotHideNeighbor() {
        QByteArray image(64, 0);
        ExportPlan plan(image);
        plan.derived({{16,24}});
        plan.add("first", {}, [&] { image[16]=1; image[24]=1; });
        plan.add("second", {"first"}, [&] { image[16]=2; });
        plan.run();
        QCOMPARE(image[16], char(2));
        auto original = image;
        ExportPlan bad(image);
        bad.derived({{16,24}});
        bad.add("first", {}, [&] { image[24]=2; });
        bad.add("second", {"first"}, [&] { image[24]=3; });
        QVERIFY_THROWS_EXCEPTION(Error, bad.run());
        QCOMPARE(image, original);
    }
    void compositionRequiresOrder() {
        QByteArray image(64, 0);
        int called=0;
        ExportPlan plan(image);
        plan.add("one", {}, [&] { ++called; });
        plan.add("two", {}, [&] { ++called; });
        plan.compose("two", "one", {{0,4}});
        QVERIFY_THROWS_EXCEPTION(Error, plan.run());
        QCOMPARE(called, 0);
    }
    void compositionTransfersOnlyItsOwnedRanges() {
        QByteArray image(64, 0);
        ExportPlan plan(image);
        plan.add("original-owner", {}, [&] { image[8]=1; }, {{8,32}});
        plan.add("composer", {"original-owner"}, [&] { image[8]=2; }, {{8,24}});
        plan.compose("composer", "original-owner", {{8,24}});
        plan.add("hook", {"composer"}, [&] { image[16]=3; });
        plan.compose("hook", "composer", {{16,20}});
        plan.run();
        QCOMPARE(image[16], char(3));
        auto original = image;
        ExportPlan bad(image);
        bad.add("original-owner", {}, [&] { image[8]=4; }, {{8,32}});
        bad.add("composer", {"original-owner"}, [&] { image[8]=5; }, {{8,24}});
        bad.compose("composer", "original-owner", {{8,24}});
        bad.add("outside-hook", {"composer"}, [&] { image[28]=6; });
        bad.compose("outside-hook", "composer", {{28,32}});
        QVERIFY_THROWS_EXCEPTION(Error, bad.run());
        QCOMPARE(image, original);
    }
};
QTEST_GUILESS_MAIN(ExportPlanTests)
#include "export_plan_tests.moc"
