#include "engine/event_tree.h"
#include "app/event_flow_editor.h"
#include "app/theme.h"
#include <QComboBox>
#include <QFontDatabase>
#include <QGroupBox>
#include <QSpinBox>
#include <QUndoStack>
#include <QtTest>
using namespace lordly;
class EventTreeTests:public QObject {
    Q_OBJECT
    static StoryAction action(int at,QString kind,int target=-1,int otherwise=-1,QString title={}){
        StoryAction result;result.instructions={at};result.kind=kind;result.title=title;
        result.target=target;result.otherwise=otherwise;
        if(kind=="condition"){result.condition.operation="test";result.condition.title=title;}
        return result;
    }
    static bool evaluate(const StoryCondition &condition,const QMap<QString,bool> &values){
        if(condition.operation=="test")return values.value(condition.title);
        if(condition.operation=="not")return !evaluate(condition.children.first(),values);
        bool result=condition.operation=="all";
        for(const auto &child:condition.children)
            result=condition.operation=="all"?(result&&evaluate(child,values)):(result||evaluate(child,values));
        return result;
    }
    static QVector<int> execute(const QVector<StoryTreeNode> &nodes,const QMap<QString,bool> &values){
        QVector<int> trace;
        for(const auto &node:nodes){
            if(node.kind==StoryTreeNode::Reference)throw Error("Unexpected reference in acyclic test");
            if(node.kind==StoryTreeNode::Action)trace<<node.offset;
            else trace+=execute(evaluate(node.condition,values)?node.thenBranch:node.elseBranch,values);
        }
        return trace;
    }
  private slots:
    void initTestCase(){
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeuib.ttf");
        qApp->setFont(QFont("Segoe UI",10));applyEditorTheme(*qApp);
    }
    void diamondAndCommonContinuation(){
        StoryEvent event;event.actions={action(0,"condition",6,2,"A"),action(2,"action"),action(4,"route",8),action(6,"action"),action(8,"end")};
        const auto tree=storyTree(event,QByteArray::fromHex("ac021307010113082f00"));
        QCOMPARE(tree.nodes.size(),2);QCOMPARE(tree.nodes[0].kind,StoryTreeNode::Decision);
        QCOMPARE(execute(tree.nodes,{{"A",true}}),(QVector<int>{6,8}));
        QCOMPARE(execute(tree.nodes,{{"A",false}}),(QVector<int>{2,8}));
    }
    void andOrTruthTable(){
        StoryEvent event;event.actions={action(0,"condition",2,4,"A"),action(2,"condition",6,4,"B"),action(4,"condition",6,8,"C"),action(6,"action"),action(8,"end")};
        const auto tree=storyTree(event,QByteArray::fromHex("ac00ac00ac0013072f00"));
        QCOMPARE(tree.nodes.first().condition.operation,QString("any"));
        QCOMPARE(tree.nodes.first().condition.children.first().operation,QString("all"));
        for(int mask=0;mask<8;++mask){
            const bool a=mask&1,b=mask&2,c=mask&4;
            const auto expected=(a&&b)||c?QVector<int>{6,8}:QVector<int>{8};
            QCOMPARE(execute(tree.nodes,{{"A",a},{"B",b},{"C",c}}),expected);
        }
    }
    void differentExternalTargetsStaySeparate(){
        StoryEvent event;event.actions={action(0,"condition",2,100,"A"),action(2,"condition",4,200,"B"),action(4,"end")};
        const auto tree=storyTree(event,QByteArray::fromHex("ac00ac002f00"));
        QCOMPARE(tree.nodes.first().condition.operation,QString("test"));
        QCOMPARE(tree.nodes.first().elseBranch.first().offset,100);
        QCOMPARE(tree.nodes.first().thenBranch.first().elseBranch.first().offset,200);
    }
    void loopsAndLargeProgramsRemainBounded(){
        StoryEvent loop;loop.actions={action(0,"action"),action(2,"route",0)};
        const auto tree=storyTree(loop,QByteArray::fromHex("130001fe"));
        QCOMPARE(tree.nodes.size(),2);QCOMPARE(tree.nodes.last().kind,StoryTreeNode::Reference);
        QCOMPARE(tree.nodes.last().offset,0);QVERIFY(tree.nodes.last().referenceLabel.contains("Repeat"));
        StoryEvent large;QByteArray code(20000,'\0');
        for(int i=0;i<10000;++i)large.actions<<action(i*2,i==9999?"end":"action");
        const auto page=storyTree(large,code);QCOMPARE(page.nodes.size(),41);
        QVERIFY(page.nodes.last().continuation);QCOMPARE(page.nodes.last().offset,80);
    }
    void liedelConditionControls(){
        const auto path=qEnvironmentVariable("LORDLY_TEST_ROM");if(path.isEmpty())QSKIP("Private ROM required");
        auto project=Project::fromRom(Rom::open(path));project.loadSceneEvents();QUndoStack undo;
        EventFlowEditor editor(&project,&undo,[&]{editor.setRow(16);});editor.setRow(16);editor.selectOffset(330);
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        editor.resize(1450,950);editor.show();QCoreApplication::processEvents();
        QVERIFY(editor.grab().save("native/build/event-tree-liedel.png"));
        const auto flow=EventFlow::read(project.rom(),*project.sceneEvents(),*project.scenePrograms(),16);
        int comparisonAt=-1;
        for(const auto &event:flow)if(event.entry==330)for(const auto &a:event.actions)
            if(a.title.contains("Chaos Frame"))for(const auto &field:a.condition.fields)if(field.key=="comparison")comparisonAt=field.offset;
        QVERIFY(comparisonAt>=0);
        editor.selectOffset(comparisonAt);
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        auto comparison=editor.findChild<QComboBox *>(QString("storyValue_%1_comparison").arg(comparisonAt));QVERIFY(comparison);
        QVERIFY(editor.findChildren<QGroupBox *>().size()<200);
        const auto original=project.sceneEvents()->program(16);const int choice=comparison->findData(15);QVERIFY(choice>=0);
        comparison->setCurrentIndex(choice);QMetaObject::invokeMethod(comparison,"activated",Q_ARG(int,choice));
        QTRY_COMPARE(u8(project.sceneEvents()->program(16),comparisonAt)>>4,15);
        undo.undo();QCOMPARE(project.sceneEvents()->program(16),original);
    }
};
QTEST_MAIN(EventTreeTests)
#include "event_tree_tests.moc"
