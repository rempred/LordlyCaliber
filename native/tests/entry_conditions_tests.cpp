#include "engine/entry_conditions.h"
#include "engine/project.h"
#include <QtTest>
using namespace lordly;

class EntryConditionsTests : public QObject {
    Q_OBJECT
    static EntryFlagCondition flag(const EntryConditions &entry, int id) {
        for (const auto &condition : entry.flags) if (condition.flag == id) return condition;
        throw Error("Missing native flag condition");
    }
    static StoryRoutes routing(const Rom &rom, const SceneEvents &events) {
        StoryRoutes result(rom);
        result.scripts[0] = events.program(0);
        const auto retail = SceneEvents::retailRow(rom, 0);
        // The native branch panel follows original entry pointers. Added rules
        // have their own controls and can precede these same native instructions.
        for (int node = 1; node < 54; ++node) put16(result.scripts[0], node * 2, u16(retail, node * 2));
        return result;
    }
    static StoryRoute withFlag(const QVector<StoryRoute> &routes, int id, int value, int event = -1) {
        for (const auto &route : routes)
            if (route.requirements.contains(id) && route.requirements[id] == value && (event < 0 || route.event == event)) return route;
        throw Error("Missing native route");
    }
    static bool relation(int operation, int left, int right) {
        return operation == 10 ? left == right : operation == 11 ? left != right : operation == 12 ? left > right :
               operation == 13 ? left < right : operation == 14 ? left >= right : left <= right;
    }
  private slots:
    void everyRetailFlagAndChaosPath() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"); if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path)); project.loadSceneEvents();
            const auto &events = *project.sceneEvents(); const auto &scenes = *project.scenePrograms();
            const auto before = events.json(); const auto world = events.program(0);
            const auto routes = routing(project.rom(), events);
            QSet<int> comparisons; int paths = 0, numericPaths = 0;
            for (int chaos = 0; chaos <= 100; ++chaos) for (int node : routes.nodes.keys()) {
                for (const auto &route : routes.routes(node, chaos)) {
                    const auto entry = EntryConditions::read(project.rom(), events, scenes, route, chaos);
                    QCOMPARE(entry.event, route.event); QCOMPARE(entry.flags.size(), route.requirements.size());
                    for (const auto &condition : entry.flags) {
                        QCOMPARE(condition.value, route.requirements[condition.flag]);
                        QVERIFY2(condition.unavailable.isEmpty(), qPrintable(condition.unavailable));
                        QVERIFY(condition.stateEditable); QVERIFY(!condition.flagOffsets.isEmpty()); QVERIFY(!condition.comparisonOffsets.isEmpty());
                        for (int offset : condition.comparisonOffsets) comparisons.insert(offset);
                    }
                    for (const auto &condition : entry.numbers) {
                        QCOMPARE(condition.value, 33); QCOMPARE(condition.relation, 15);
                        QVERIFY(relation(condition.relation, chaos, condition.value)); ++numericPaths;
                    }
                    ++paths;
                }
            }
            QCOMPARE(comparisons.size(), 106); QVERIFY(paths > 7000); QVERIFY(numericPaths > 0);
            QCOMPARE(events.json(), before); QCOMPARE(events.program(0), world);
            qInfo() << paths << "native paths across CF0..100; all106 physical flag tests supported;" << numericPaths << "CF paths";
        } catch (const std::exception &error) { QFAIL(error.what()); }
    }
    void celesExactEditsAndModelUndo() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"); if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path)); project.loadSceneEvents();
            auto events = *project.sceneEvents(); const auto &scenes = *project.scenePrograms();
            const auto snapshot = events.json(); const auto original = events.program(0);
            const auto celes = routing(project.rom(), events).routes(20, 50); QCOMPARE(celes.size(), 3);
            const auto route = withFlag(celes, 30, 1, 30);
            const auto entry = EntryConditions::read(project.rom(), events, scenes, route, 50);
            const auto condition = flag(entry, 30); QCOMPARE(condition.value, 1);
            EntryConditions::setState(events, scenes, condition, 0);
            auto expected = original;
            for (int offset : condition.comparisonOffsets) put8(expected, offset, quint8(u8(expected, offset) ^ 0x10));
            QCOMPARE(events.program(0), expected);
            const auto changed = routing(project.rom(), events).routes(20, 50);
            QCOMPARE(withFlag(changed, 30, 0, 30).event, 30);
            const auto after = events.json(); auto roundtrip = *project.sceneEvents(); roundtrip.apply(after, scenes);
            QCOMPARE(roundtrip.program(0), expected);
            events.apply(snapshot, scenes); QCOMPARE(events.program(0), original); QCOMPARE(events.json(), snapshot);

            const auto sharedRoute = withFlag(celes, 128, 0);
            const auto shared = EntryConditions::read(project.rom(), events, scenes, sharedRoute, 50);
            const auto oldFlag = flag(shared, 128);
            EntryConditions::setFlag(events, scenes, oldFlag, 129);
            expected = original;
            for (int offset : oldFlag.flagOffsets) put8(expected, offset + 1, 129);
            QCOMPARE(events.program(0), expected);
            const auto retargeted = routing(project.rom(), events).routes(20, 50);
            int affected = 0;
            for (const auto &candidate : retargeted) {
                QVERIFY(!candidate.requirements.contains(128));
                if (candidate.requirements.contains(129)) { QCOMPARE(candidate.requirements[129], 0); ++affected; }
            }
            QCOMPARE(affected, 2);
            const auto stable = events.json();
            QVERIFY_THROWS_EXCEPTION(Error, EntryConditions::setFlag(events, scenes, oldFlag, 130));
            QCOMPARE(events.json(), stable);
            events.apply(snapshot, scenes);
            EntryConditions::setEvent(events, scenes, shared, 32);
            expected = original; put8(expected, shared.eventOffset + 1, 32); QCOMPARE(events.program(0), expected);
            int sharedSwitch = 0;
            for (const auto &candidate : routing(project.rom(), events).routes(20, 50)) sharedSwitch += candidate.event == 32;
            QCOMPARE(sharedSwitch, 2);
            events.apply(snapshot, scenes); QCOMPARE(events.program(0), original);
        } catch (const std::exception &error) { QFAIL(error.what()); }
    }
    void chaosThresholdAndAllRelations() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"); if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path)); project.loadSceneEvents();
            auto events = *project.sceneEvents(); const auto &scenes = *project.scenePrograms();
            const auto snapshot = events.json(); const auto original = events.program(0);
            const auto baseline = routing(project.rom(), events);
            StoryRoute chosen; EntryNumberCondition number; bool found = false;
            for (int node : baseline.nodes.keys()) for (const auto &route : baseline.routes(node, 33)) {
                const auto entry = EntryConditions::read(project.rom(), events, scenes, route, 33);
                if (!entry.numbers.isEmpty()) { chosen = route; number = entry.numbers.first(); found = true; break; }
            }
            QVERIFY(found); QCOMPARE(number.value, 33); QCOMPARE(number.relation, 15); QVERIFY(!number.branchTaken);
            EntryConditions::setNumberValue(events, scenes, number, 37);
            auto expected = original; put8(expected, number.valueOffset + 1, 37); QCOMPARE(events.program(0), expected);
            const auto moved = routing(project.rom(), events);
            for (int chaos : {0, 33, 34, 37, 38, 100}) {
                const auto result = moved.routes(chosen.node, chaos, chosen.requirements);
                QCOMPARE(result.size(), chaos <= 37 ? 1 : 0);
                if (!result.isEmpty()) QCOMPARE(result.first().event, chosen.event);
            }
            for (int operation = 10; operation <= 15; ++operation) {
                events.apply(snapshot, scenes); EntryConditions::setNumberRelation(events, scenes, number, operation);
                const auto edited = routing(project.rom(), events);
                for (int chaos = 0; chaos <= 100; ++chaos) {
                    const auto result = edited.routes(chosen.node, chaos, chosen.requirements);
                    QCOMPARE(result.size(), relation(operation, chaos, 33) ? 1 : 0);
                    if (!result.isEmpty()) QCOMPARE(result.first().event, chosen.event);
                }
            }
            events.apply(snapshot, scenes); QCOMPARE(events.program(0), original);
            const auto stable = events.json();
            QVERIFY_THROWS_EXCEPTION(Error, EntryConditions::setNumberValue(events, scenes, number, 101));
            QCOMPARE(events.json(), stable);
        } catch (const std::exception &error) { QFAIL(error.what()); }
    }
    void addedRuleCoexistsWithNativeEdits() {
        const auto path = qEnvironmentVariable("LORDLY_TEST_ROM"); if (path.isEmpty()) QSKIP("Private ROM required");
        try {
            auto project = Project::fromRom(Rom::open(path)); project.loadSceneEvents();
            auto events = *project.sceneEvents(); const auto &scenes = *project.scenePrograms();
            const QJsonArray rules{QJsonObject{{"conditions", QJsonArray{QJsonObject{{"flag", 200}, {"value", true}}}},
                {"event", 4}, {"clearFrom", 200}, {"clearTo", 255}}};
            events.setWorldRules(20, rules);
            const auto before = events.program(0);
            const auto route = withFlag(routing(project.rom(), events).routes(20, 50), 30, 1, 30);
            const auto entry = EntryConditions::read(project.rom(), events, scenes, route, 50);
            const auto condition = flag(entry, 30);
            EntryConditions::setState(events, scenes, condition, 0);
            auto expected = before;
            for (int offset : condition.comparisonOffsets) put8(expected, offset, quint8(u8(expected, offset) ^ 0x10));
            QCOMPARE(events.program(0), expected);
            QCOMPARE(events.flagEdits()["0"].toObject()["20"].toArray(), rules);
            QVERIFY(u16(before, 40) != u16(SceneEvents::retailRow(project.rom(), 0), 40));
            QCOMPARE(withFlag(routing(project.rom(), events).routes(20, 50), 30, 0, 30).event, 30);
        } catch (const std::exception &error) { QFAIL(error.what()); }
    }
};
QTEST_APPLESS_MAIN(EntryConditionsTests)
#include "entry_conditions_tests.moc"
