#include "engine/event_helper_names.h"
#include "engine/event_helper_summary.h"
#include "engine/event_script.h"
#include "engine/scene_events.h"
#include <QtTest>

using namespace lordly;
class EventHelperNamesTests : public QObject {
    Q_OBJECT
private slots:
    void completeBodyRequired() {
        using namespace EventHelperNames;
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");
        if(path.isEmpty())QSKIP("Private ROM required");
        const auto rom=Rom::open(path);
        const auto bytes=SceneEvents::retailRow(rom,10);
        const auto offsets=EventScript::instructionOffsets(10);
        const int entries[]{1190,1286,1328};
        const Kind kinds[]{Kind::IdentityFamily,Kind::AngelUndeadClass,Kind::LichVampireClass};
        const int lengths[]{96,42,30};
        for(int n=0;n<3;++n){
            const auto helper=match(bytes,offsets,entries[n]);
            QCOMPARE(helper.kind,kinds[n]);QCOMPARE(helper.entry,entries[n]);
            QCOMPARE(helper.byteLength,lengths[n]);QCOMPARE(helper.preservedRegisters,quint8(0xfe));
            QVERIFY(!name(helper.kind).isEmpty());QVERIFY(!detail(helper.kind).isEmpty());
            for(int byte=0;byte<lengths[n];++byte){
                auto changed=bytes;changed[entries[n]+byte]=char(quint8(changed[entries[n]+byte])^1);
                QVERIFY2(!match(changed,offsets,entries[n]).known(),qPrintable(QString("Changed helper byte %1/%2 retained its label").arg(n).arg(byte)));
            }
            for(int byte=0;byte<lengths[n];++byte)
                QVERIFY(!match(bytes.left(entries[n]+byte),offsets,entries[n]).known());
            for(int word=0;word<lengths[n];word+=2){
                auto missing=offsets;missing.remove(entries[n]+word);
                QVERIFY(!match(bytes,missing,entries[n]).known());
            }
            QVERIFY(!match(bytes,offsets,entries[n]+1).known());
            QVERIFY(!match(bytes,offsets,entries[n]+2).known());
            // No row number, digest or original location may be required.
            const int relocated=8;
            const auto moved=QByteArray(relocated,'\0')+bytes.mid(entries[n],lengths[n]);
            QSet<int> movedOffsets;for(int at=relocated;at<moved.size();at+=2)movedOffsets.insert(at);
            QCOMPARE(match(moved,movedOffsets,relocated).kind,kinds[n]);
            auto unrelated=bytes;unrelated[17]=char(quint8(unrelated[17])^1);
            QCOMPARE(match(unrelated,offsets,entries[n]).kind,kinds[n]);
        }
        QVERIFY(!match(bytes,offsets,-2).known());
        QVERIFY(!match(bytes,offsets,bytes.size()+2).known());
    }
    void currentRetailCopies() {
        using namespace EventHelperNames;
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");
        if(path.isEmpty())QSKIP("Private ROM required");
        const auto rom=Rom::open(path);std::array<int,4> counts{};int rows=0;
        for(int row=0;row<115;++row){
            if(row==2||row==3)continue;
            const auto bytes=SceneEvents::retailRow(rom,row);
            const auto offsets=EventScript::instructionOffsets(row);++rows;
            for(int at:offsets){const auto helper=match(bytes,offsets,at);if(helper.known())++counts[int(helper.kind)];}
        }
        QCOMPARE(rows,113);QCOMPARE(counts[1],75);QCOMPARE(counts[2],73);QCOMPARE(counts[3],73);
    }
    void predicateRequiresCurrentInputs() {
        using namespace EventHelperNames;
        QVERIFY(predicate(Kind::IdentityFamily,"Candidate",{}).isEmpty());
        QVERIFY(predicate(Kind::IdentityFamily,{},"Katreda").isEmpty());
        QCOMPARE(predicate(Kind::IdentityFamily,"Identity of contacting squad member","Katreda"),
                 QString("Identity of contacting squad member matches Katreda (including equivalent class variants)"));
        QCOMPARE(predicate(Kind::AngelUndeadClass,"Current class of contacting squad member"),
                 QString("Current class of contacting squad member is Angel Knight, Seraph, Zombie, Skeleton or Ghost"));
        QVERIFY(predicate(Kind::Unknown,"Candidate","Katreda").isEmpty());
    }
    void summaryPreservesReachableBoundaries() {
        using namespace EventHelperNames;
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");
        if(path.isEmpty())QSKIP("Private ROM required");
        const auto rom=Rom::open(path);const auto bytes=SceneEvents::retailRow(rom,10);
        const auto offsets=EventScript::instructionOffsets(10);
        QVector<int> ordered=offsets.values();std::sort(ordered.begin(),ordered.end());
        StoryEvent original;original.entry=16;
        for(int at:ordered){StoryAction action;action.instructions<<at;original.actions<<action;}
        const auto inventory=[&](const QVector<StoryEvent> &events){
            QMap<int,int> result;for(const auto &event:events)for(const auto &action:event.actions)
                for(int at:action.instructions)++result[at];return result;
        };
        const auto expected=inventory({original});
        const auto helperPage=[](const QVector<StoryEvent> &events,int entry){
            for(const auto &event:events)if(event.entry==entry)return true;return false;
        };
        auto projected=QVector<StoryEvent>{original};
        QCOMPARE(summarize(projected,bytes,offsets,{}),3);QCOMPARE(inventory(projected),expected);
        for(int entry:{1190,1286,1328}){
            QVERIFY(helperPage(projected,entry));
            for(const auto &event:projected)if(event.entry==entry){
                QCOMPARE(event.actions.size(),1);QCOMPARE(event.actions.first().kind,QString("return"));
                QCOMPARE(event.actions.first().instructions.size(),match(bytes,offsets,entry).byteLength/2);
                QVERIFY(event.actions.first().detail.isEmpty());
            }
        }
        // An outside route to an internal comparison cannot be hidden under
        // the whole-helper contract. Other isolated helpers remain summarized.
        projected={original};QMap<int,QSet<int>> incoming;incoming[1200].insert(608);
        QCOMPARE(summarize(projected,bytes,offsets,incoming),2);
        QVERIFY(!helperPage(projected,1190));QCOMPARE(inventory(projected),expected);
        // A modified predecessor that resumes at the next word must retain
        // this routine in the same event, rather than severing that path.
        auto fallthrough=bytes;fallthrough[1188]=char(0x13);projected={original};
        QCOMPARE(summarize(projected,fallthrough,offsets,{}),2);
        QVERIFY(!helperPage(projected,1190));QCOMPARE(inventory(projected),expected);
        // Do not extract part of an existing compound instruction owner.
        auto straddling=original;int preceding=-1,first=-1;
        for(int n=0;n<straddling.actions.size();++n){
            if(straddling.actions[n].instructions.contains(1188))preceding=n;
            if(straddling.actions[n].instructions.contains(1190))first=n;
        }
        QVERIFY(preceding>=0&&first>=0);
        straddling.actions[preceding].instructions+=straddling.actions[first].instructions;
        straddling.actions.removeAt(first);projected={straddling};
        QCOMPARE(summarize(projected,bytes,offsets,{}),2);
        QVERIFY(!helperPage(projected,1190));QCOMPARE(inventory(projected),expected);
    }
};
QTEST_GUILESS_MAIN(EventHelperNamesTests)
#include "event_helper_names_tests.moc"
