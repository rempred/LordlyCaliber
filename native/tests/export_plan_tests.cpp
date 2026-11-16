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
        missing.add("child", {"missing"}, [&](QByteArray &) { ++called; });
        QVERIFY_THROWS_EXCEPTION(Error, missing.run());
        QCOMPARE(called, 0);
        ExportPlan reversed(image);
        reversed.add("child", {"parent"}, [&](QByteArray &) { ++called; });
        reversed.add("parent", {}, [&](QByteArray &) { ++called; });
        QVERIFY_THROWS_EXCEPTION(Error, reversed.run());
        QCOMPARE(called, 0);
        ExportPlan cycle(image);
        cycle.add("one", {"two"}, [&](QByteArray &) { ++called; });
        cycle.add("two", {"one"}, [&](QByteArray &) { ++called; });
        QVERIFY_THROWS_EXCEPTION(Error, cycle.run());
        QCOMPARE(called, 0);
        QStringList actual;
        ExportPlan ordered(image);
        ordered.add("one", {}, [&](QByteArray &) { actual << "one"; });
        ordered.add("independent", {}, [&](QByteArray &) { actual << "independent"; });
        ordered.add("two", {"one"}, [&](QByteArray &) { actual << "two"; });
        ordered.run();
        QCOMPARE(actual, QStringList({"one", "independent", "two"}));
    }
    void overlapIsNotAuthorizedByDependency() {
        const QByteArray original(64, 0);
        auto image = original;
        ExportPlan plan(image);
        plan.add("first", {}, [&](QByteArray &image) { image[20] = 1; });
        plan.add("second", {"first"}, [&](QByteArray &image) { image[20] = 2; });
        try { image=plan.run(); QFAIL("Undeclared overwrite accepted"); }
        catch (const Error &error) {
            const auto message = QString::fromUtf8(error.what());
            QVERIFY(message.contains("second overwrites first"));
            QVERIFY(message.contains("0x14..0x15"));
        }
        QCOMPARE(plan.image(), original);
    }
    void boundedCompositionAndAdjacentWrites() {
        QByteArray image(64, 0);
        ExportPlan plan(image);
        plan.add("first", {}, [&](QByteArray &image) { image[20] = 1; image[21] = 1; });
        plan.add("second", {"first"}, [&](QByteArray &image) { image[20] = 2; });
        plan.add("adjacent", {}, [&](QByteArray &image) { image[22] = 3; });
        plan.compose("second", "first", {{20,21}});
        image=plan.run();
        QCOMPARE(image.mid(20,3), QByteArray::fromHex("020103"));
        QCOMPARE(plan.changedSteps(), 3);

        auto original = image;
        ExportPlan tooWide(image);
        tooWide.add("first", {}, [&](QByteArray &image) { image[20] = 4; image[21] = 4; });
        tooWide.add("second", {"first"}, [&](QByteArray &image) { image[20] = 5; image[21] = 5; });
        tooWide.compose("second", "first", {{20,21}});
        QVERIFY_THROWS_EXCEPTION(Error, tooWide.run());
        QCOMPARE(tooWide.image(), original);
    }
    void fullOwnerCatchesUnchangedHookBytes() {
        const QByteArray original(64, 0);
        auto image = original;
        ExportPlan plan(image);
        // The replacement's changed byte is disjoint from the future hook.
        plan.add("whole-function", {}, [&](QByteArray &image) { image[8] = 1; }, {{8,32}});
        plan.add("inner-hook", {"whole-function"}, [&](QByteArray &image) { image[20] = 2; });
        QVERIFY_THROWS_EXCEPTION(Error, plan.run());
        QCOMPARE(plan.image(), original);
        ExportPlan noop(image);
        noop.add("inactive", {}, [](QByteArray &) {}, {{8,32}});
        noop.add("hook", {}, [&](QByteArray &image) { image[20] = 2; });
        image=noop.run();
        QCOMPARE(image[20], char(2));
    }
    void failedBuilderRollsBackEarlierSteps() {
        const QByteArray original(64, 0);
        auto image = original;
        ExportPlan plan(image);
        plan.add("first", {}, [&](QByteArray &image) { image[8] = 1; });
        plan.add("throws", {"first"}, [&](QByteArray &image) { image[10] = 2; throw Error("compile failed"); });
        QVERIFY_THROWS_EXCEPTION(Error, plan.run());
        QCOMPARE(plan.image(), original);
    }
    void shrinkAppendAndReuse() {
        const QByteArray original(64, 7);
        auto image = original;
        ExportPlan bad(image);
        bad.add("reclaim", {}, [&](QByteArray &image) { image.resize(32); });
        bad.add("reuse", {"reclaim"}, [&](QByteArray &image) { image.append(QByteArray(16,9)); });
        QVERIFY_THROWS_EXCEPTION(Error, bad.run());
        QCOMPARE(bad.image(), original);
        ExportPlan good(image);
        good.add("reclaim", {}, [&](QByteArray &image) { image.resize(32); });
        good.add("reuse", {"reclaim"}, [&](QByteArray &image) { image.append(QByteArray(16,9)); });
        good.compose("reuse", "reclaim", {{32,48}});
        image=good.run();
        QCOMPARE(image, QByteArray(32,7)+QByteArray(16,9));
    }
    void derivedChecksumDoesNotHideNeighbor() {
        QByteArray image(64, 0);
        ExportPlan plan(image);
        plan.derived({{16,24}});
        plan.add("first", {}, [&](QByteArray &image) { image[16]=1; image[24]=1; });
        plan.add("second", {"first"}, [&](QByteArray &image) { image[16]=2; });
        image=plan.run();
        QCOMPARE(image[16], char(2));
        auto original = image;
        ExportPlan bad(image);
        bad.derived({{16,24}});
        bad.add("first", {}, [&](QByteArray &image) { image[24]=2; });
        bad.add("second", {"first"}, [&](QByteArray &image) { image[24]=3; });
        QVERIFY_THROWS_EXCEPTION(Error, bad.run());
        QCOMPARE(bad.image(), original);
    }
    void compositionRequiresOrder() {
        QByteArray image(64, 0);
        int called=0;
        ExportPlan plan(image);
        plan.add("one", {}, [&](QByteArray &) { ++called; });
        plan.add("two", {}, [&](QByteArray &) { ++called; });
        plan.compose("two", "one", {{0,4}});
        QVERIFY_THROWS_EXCEPTION(Error, plan.run());
        QCOMPARE(called, 0);
    }
    void compositionTransfersOnlyItsOwnedRanges() {
        QByteArray image(64, 0);
        ExportPlan plan(image);
        plan.add("original-owner", {}, [&](QByteArray &image) { image[8]=1; }, {{8,32}});
        plan.add("composer", {"original-owner"}, [&](QByteArray &image) { image[8]=2; }, {{8,24}});
        plan.compose("composer", "original-owner", {{8,24}});
        plan.add("hook", {"composer"}, [&](QByteArray &image) { image[16]=3; });
        plan.compose("hook", "composer", {{16,20}});
        image=plan.run();
        QCOMPARE(image[16], char(3));
        auto original = image;
        ExportPlan bad(image);
        bad.add("original-owner", {}, [&](QByteArray &image) { image[8]=4; }, {{8,32}});
        bad.add("composer", {"original-owner"}, [&](QByteArray &image) { image[8]=5; }, {{8,24}});
        bad.compose("composer", "original-owner", {{8,24}});
        bad.add("outside-hook", {"composer"}, [&](QByteArray &image) { image[28]=6; });
        bad.compose("outside-hook", "composer", {{28,32}});
        QVERIFY_THROWS_EXCEPTION(Error, bad.run());
        QCOMPARE(bad.image(), original);
    }
    void individualWritesInsideOneBuilder() {
        RomWriter writer(QByteArray(64,0));
        writer.write("selection",8,QByteArray::fromHex("01000000"));
        const auto before=writer.bytes();
        // Even unchanged bytes of an explicitly written function belong to it.
        QVERIFY_THROWS_EXCEPTION(Error,writer.write("pose",10,QByteArray(1,2)));
        QCOMPARE(writer.bytes(),before);
        writer.allow("pose","selection",{{10,11}});
        writer.write("pose",10,QByteArray(1,2));
        QCOMPARE(writer.bytes().mid(8,4),QByteArray::fromHex("01000200"));
        QVERIFY_THROWS_EXCEPTION(Error,writer.write("pose",9,QByteArray(2,3)));
        QCOMPARE(writer.changes().size(),2);
        QCOMPARE(writer.changes().last().owner,QString("pose"));
        QCOMPARE(writer.changes().last().ranges.first().begin,10);
        QVERIFY_THROWS_EXCEPTION(Error,writer.write("outside",64,QByteArray(1,3)));
    }
    void privateCandidateCannotLeakOnFailure() {
        RomWriter writer(QByteArray(64,0));
        QVERIFY_THROWS_EXCEPTION(Error,writer.edit("broken",[](QByteArray &candidate) {
            candidate[0]=5;throw Error("compile failed");
        }));
        QCOMPARE(writer.bytes(),QByteArray(64,0));
        QVERIFY(writer.changes().isEmpty());
    }
    void restorationIsCheckedAndThenReleased() {
        const QByteArray original(64,7);
        ExportPlan bad(original);
        bad.restore("remove-one",[](QByteArray &image){image[8]=0;});
        bad.restore("remove-two",[](QByteArray &image){image[8]=1;});
        QVERIFY_THROWS_EXCEPTION(Error,bad.run());
        QCOMPARE(bad.image(),original);
        ExportPlan good(original);
        good.restore("remove-one",[](QByteArray &image){image[8]=0;});
        good.add("replacement",{},[](QByteArray &image){image[8]=2;});
        QCOMPARE(good.run()[8],char(2));
        QVERIFY_THROWS_EXCEPTION(Error,good.restore("late",[](QByteArray &){}));
        ExportPlan failed(original);
        failed.restore("remove-one",[](QByteArray &image){image[8]=0;});
        failed.add("fails",{},[](QByteArray &image){image[10]=3;throw Error("failed");});
        QVERIFY_THROWS_EXCEPTION(Error,failed.run());
        QCOMPARE(failed.image(),original);
    }
};
QTEST_GUILESS_MAIN(ExportPlanTests)
#include "export_plan_tests.moc"
