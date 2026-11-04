#include "app/event_tree_dialog.h"
#include "engine/event_authoring.h"
#include "engine/event_script.h"
#include "engine/squad_members.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFormLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTreeWidget>
#include <QUuid>
#include <QVBoxLayout>
namespace lordly {
namespace {
const QJsonObject &catalog(){static const auto c=[] {QFile f(":/game/director/event-semantics.json");f.open(QIODevice::ReadOnly);return QJsonDocument::fromJson(f.readAll()).object();}();return c;}
QMap<int,QString> named(const QString &kind,int row){
    QString scope="global";const auto keys=catalog()["rows"].toObject()[QString::number(row)].toObject()["scenarioKeys"].toArray();
    if(kind.startsWith("scenario-")){if(keys.size()!=1)return {};scope=QString("scenario:%1").arg(keys.first().toInt());}
    QMap<int,QString> out;for(const auto &v:catalog()["names"].toArray()){const auto a=v.toArray();if(a[0]==kind&&a[1]==scope)out[a[2].toInt()]=a[3].toString();}return out;
}
QMap<int,QString> scenes(Project *p){QMap<int,QString> out;for(const auto &id:p->scenePrograms()->scenes())for(int n:p->scenePrograms()->resource(id).selectors)out[n]=p->scenePrograms()->displayName(id);return out;}
QMap<int,QString> targets(int row,int group){auto out=named(group<=2?"scenario-site":"scenario-squad",row);out[-1]="Any";return out;}
QJsonObject conditionDefault(QString type="flag"){
    if(type=="all"||type=="any")return {{"type",type},{"conditions",QJsonArray{conditionDefault()}}};
    if(type=="not")return {{"type",type},{"condition",conditionDefault()}};
    return {{"type",type},{"flag",200},{"value",type=="answer"?255:type=="chaos"?50:1},{"comparison",10}};
}
QJsonObject actionDefault(QString type){
    if(type=="if")return {{"type",type},{"condition",conditionDefault()},{"then",QJsonArray{}},{"else",QJsonArray{}}};
    return {{"type",type},{"flag",200},{"value",type=="chaos"?50:1},{"scene",0},{"template",527},{"mode",1},{"amount",1},{"program",4},{"clearFrom",200},{"clearTo",255}};
}
QString conditionText(const QJsonObject &n,int row,const SceneEvents *flags=nullptr){const auto type=n["type"].toString();const auto rel=QMap<int,QString>{{10,"="},{11,"≠"},{12,">"},{13,"<"},{14,"≥"},{15,"≤"}}.value(n["comparison"].toInt(10));
    if(type=="flag")return (flags?flags->flagName(n["flag"].toInt(),row):EventScript::flagName(n["flag"].toInt(),row))+" "+rel+" "+(n["value"].toInt()?"ON":"OFF");
    if(type=="all"||type=="any"||type=="not")return EventAuthoring::conditions().value(type);
    if(type=="squadState")return "Contacting squad "+rel+" "+QMap<int,QString>{{0,"absent, friendly"},{1,"present, friendly"},{128,"absent, hostile"},{129,"present, hostile"}}.value(n["value"].toInt(1));
    return EventAuthoring::conditions().value(type)+" "+rel+" "+QString::number(n["value"].toInt());
}
class TreeDialog:public QDialog {
  public:
    Project *project;int row;QJsonObject original;SceneEvents flagState;QLineEdit *name;QComboBox *trigger,*target;QCheckBox *after;QTreeWidget *tree;QScrollArea *details;QLabel *error;
    TreeDialog(Project *p,int r,QJsonObject value,QWidget *parent):QDialog(parent),project(p),row(r),original(value),flagState(*p->sceneEvents()){
        setWindowTitle("Edit event logic");resize(1050,740);auto outer=new QVBoxLayout(this);auto when=new QFormLayout;outer->addLayout(when);
        name=new QLineEdit(value["name"].toString("New event"));name->setMaxLength(120);name->setObjectName("eventTreeName");when->addRow("Name",name);
        trigger=new QComboBox;trigger->setObjectName("eventTreeWhen");for(auto [id,label]:EventAuthoring::triggers().asKeyValueRange())trigger->addItem(label,id);
        trigger->setCurrentIndex(trigger->findData(value["group"].toInt(3)));when->addRow("WHEN",trigger);target=new QComboBox;target->setObjectName("eventTreeTarget");when->addRow("Target",target);
        auto refreshTarget=[this](int wanted){target->clear();const int group=trigger->currentData().toInt();for(auto [id,label]:eventTargets(project,row,group).asKeyValueRange())target->addItem(label+(id<0?QString{}:QString(" [%1]").arg(id)),id);
            if(target->findData(wanted)<0)target->addItem(QString("Target ID %1").arg(wanted),wanted);target->setCurrentIndex(target->findData(wanted));target->setEnabled(group>=1&&group<=4);};
        refreshTarget(value["target"].toInt(-1));connect(trigger,&QComboBox::activated,this,[refreshTarget](int){refreshTarget(-1);});
        after=new QCheckBox("Run after existing events");after->setChecked(value["after"].toBool());when->addRow(after);
        auto bar=new QHBoxLayout;outer->addLayout(bar);
        auto addIf=new QPushButton("Add IF / ELSE");addIf->setObjectName("eventTreeAddIf");bar->addWidget(addIf);
        auto addAction=new QPushButton("Add action…");addAction->setObjectName("eventTreeAddAction");bar->addWidget(addAction);
        auto addCondition=new QPushButton("Add condition");bar->addWidget(addCondition);
        auto remove=new QPushButton("Remove");bar->addWidget(remove);auto up=new QPushButton("Move up"),down=new QPushButton("Move down");bar->addWidget(up);bar->addWidget(down);
        auto split=new QSplitter;outer->addWidget(split,1);tree=new QTreeWidget;tree->setObjectName("eventTreeSteps");tree->setHeaderHidden(true);tree->setWordWrap(true);tree->setMinimumWidth(300);split->addWidget(tree);
        details=new QScrollArea;details->setWidgetResizable(true);split->addWidget(details);split->setSizes({480,500});
        for(const auto &v:value["nodes"].toArray())append(tree->invisibleRootItem(),v.toObject(),"node");tree->expandAll();
        connect(tree,&QTreeWidget::currentItemChanged,this,[this]{showFields();});
        connect(addIf,&QPushButton::clicked,this,[this]{addNode(actionDefault("if"));});
        connect(addAction,&QPushButton::clicked,this,[this]{const auto choices=EventAuthoring::actions();bool ok=false;const auto text=QInputDialog::getItem(this,"Add action","Action",choices.values(),0,false,&ok);if(ok){auto v=actionDefault(choices.key(text));if(v["type"]=="scene"&&!scenes(project).isEmpty())v["scene"]=scenes(project).firstKey();addNode(v);}});
        connect(addCondition,&QPushButton::clicked,this,[this]{auto item=tree->currentItem();if(!item)return;
            if(item->data(0,Qt::UserRole).toJsonObject()["type"]=="if")item=item->child(0);
            if(item->data(0,Qt::UserRole+1)!="condition"){error->setText("Select the IF condition to add another test.");return;}
            const auto n=serialize(item);const auto type=n["type"].toString();
            if(type=="all"||type=="any"){append(item,conditionDefault(),"condition");item->setExpanded(true);tree->setCurrentItem(item->child(item->childCount()-1));}
            else {auto parent=item->parent();const int index=parent->indexOfChild(item);append(parent,{{"type","all"},{"conditions",QJsonArray{n,conditionDefault()}}},"condition");auto replacement=parent->takeChild(parent->childCount()-1);parent->insertChild(index,replacement);delete item;replacement->setExpanded(true);tree->setCurrentItem(replacement);}});
        connect(remove,&QPushButton::clicked,this,[this]{auto item=tree->currentItem();if(!item)return;auto parent=item->parent();const auto role=item->data(0,Qt::UserRole+1).toString();
            if(role=="branch"||(role=="condition"&&parent&&(parent->data(0,Qt::UserRole+1)=="node"||parent->data(0,Qt::UserRole).toJsonObject()["type"]=="not"))){error->setText("Change this condition using its selector.");return;}delete item;});
        auto move=[this](int d){auto item=tree->currentItem();if(!item||item->data(0,Qt::UserRole+1)!="node")return;auto parent=item->parent();if(!parent)parent=tree->invisibleRootItem();const int at=parent->indexOfChild(item),to=at+d;if(to<0||to>=parent->childCount())return;parent->takeChild(at);parent->insertChild(to,item);tree->setCurrentItem(item);};
        connect(up,&QPushButton::clicked,this,[move]{move(-1);});connect(down,&QPushButton::clicked,this,[move]{move(1);});
        error=new QLabel;error->setWordWrap(true);outer->addWidget(error);auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);outer->addWidget(buttons);connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
        connect(buttons,&QDialogButtonBox::accepted,this,[this]{try{auto trees=project->sceneEvents()->flagEdits()[QString::number(row)].toObject()["trees"].toArray();const auto value=result();bool found=false;for(int i=0;i<trees.size();++i)if(trees[i].toObject()["id"]==value["id"]){trees[i]=value;found=true;}if(!found)trees<<value;auto events=flagState;events.setTrees(row,trees,*project->scenePrograms());accept();}catch(const std::exception &e){error->setText(QString::fromUtf8(e.what()));}});
        if(tree->topLevelItemCount())tree->setCurrentItem(tree->topLevelItem(0));else showFields();
    }
    QJsonObject result()const{auto out=original;if(out["id"].toString().isEmpty())out["id"]=QUuid::createUuid().toString(QUuid::WithoutBraces);out["name"]=name->text().trimmed();out["group"]=trigger->currentData().toInt();out["target"]=target->isEnabled()?target->currentData().toInt():-1;out["after"]=after->isChecked();out["nodes"]=nodes(tree->invisibleRootItem());return out;}
    QJsonArray nodes(QTreeWidgetItem *parent)const{QJsonArray out;for(int i=0;i<parent->childCount();++i)out<<serialize(parent->child(i));return out;}
    QJsonObject serialize(QTreeWidgetItem *item)const{auto n=item->data(0,Qt::UserRole).toJsonObject();const auto type=n["type"].toString();
        if(type=="if"){n["condition"]=serialize(item->child(0));n["then"]=nodes(item->child(1));n["else"]=nodes(item->child(2));}
        else if(type=="all"||type=="any")n["conditions"]=nodes(item);else if(type=="not")n["condition"]=serialize(item->child(0));return n;}
    void append(QTreeWidgetItem *parent,QJsonObject n,const QString &role){auto item=new QTreeWidgetItem(parent);item->setData(0,Qt::UserRole,n);item->setData(0,Qt::UserRole+1,role);title(item);
        const auto type=n["type"].toString();if(type=="if"){append(item,n["condition"].toObject(),"condition");for(const auto &key:{"then","else"}){auto branch=new QTreeWidgetItem(item,{QString(key).toUpper()});branch->setData(0,Qt::UserRole+1,"branch");for(const auto &v:n[key].toArray())append(branch,v.toObject(),"node");}}
        else if(type=="all"||type=="any")for(const auto &v:n["conditions"].toArray())append(item,v.toObject(),"condition");else if(type=="not")append(item,n["condition"].toObject(),"condition");}
    void title(QTreeWidgetItem *item){const auto n=item->data(0,Qt::UserRole).toJsonObject();item->setText(0,item->data(0,Qt::UserRole+1)=="condition"?conditionText(n,row,&flagState):eventTreeSummary(n,row,&flagState));}
    void addNode(QJsonObject n){auto parent=tree->currentItem();while(parent&&parent->data(0,Qt::UserRole+1)!="branch"){
        if(parent->data(0,Qt::UserRole).toJsonObject()["type"]=="if"){parent=parent->child(1);break;}parent=parent->parent();}
        if(!parent)parent=tree->invisibleRootItem();append(parent,n,"node");parent->setExpanded(true);auto item=parent->child(parent->childCount()-1);item->setExpanded(true);tree->setCurrentItem(n["type"]=="if"?item->child(0):item);}
    void showFields(){if(auto old=details->takeWidget())old->deleteLater();auto widget=new QWidget;auto form=new QFormLayout(widget);details->setWidget(widget);auto item=tree->currentItem();if(!item){form->addRow(new QLabel("Add a condition or action to start."));return;}
        if(item->data(0,Qt::UserRole+1)=="branch"){form->addRow(new QLabel("Add actions or another IF / ELSE here."));return;}
        const auto n=item->data(0,Qt::UserRole).toJsonObject();const auto type=n["type"].toString();const bool condition=item->data(0,Qt::UserRole+1)=="condition";
        auto set=[this,item](const QString &key,int value){auto n=item->data(0,Qt::UserRole).toJsonObject();n[key]=value;item->setData(0,Qt::UserRole,n);title(item);};
        auto number=[&](QString key,QString label,int lo,int hi,int initial=0){auto spin=new QSpinBox;spin->setKeyboardTracking(false);spin->setRange(lo,hi);spin->setValue(n[key].toInt(initial));form->addRow(label,spin);connect(spin,&QSpinBox::valueChanged,this,[set,key](int value){set(key,value);});};
        auto choice=[&](QString key,QString label,QMap<int,QString> options,int initial=0,bool ids=true){auto combo=new QComboBox;combo->setEditable(true);combo->setInsertPolicy(QComboBox::NoInsert);combo->setMinimumContentsLength(22);combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
            combo->completer()->setFilterMode(Qt::MatchContains);combo->completer()->setCaseSensitivity(Qt::CaseInsensitive);
            const int value=n[key].toInt(initial);if(!options.contains(value))options[value]=QString("ID %1").arg(value);for(auto [id,text]:options.asKeyValueRange())combo->addItem(text+(ids&&key!="flag"?QString(" [%1]").arg(id):QString{}),id);
            combo->setCurrentIndex(combo->findData(value));
            if(key=="flag"){
                combo->setObjectName("eventTreeFlag");auto field=new QHBoxLayout;field->addWidget(combo,1);
                auto create=new QPushButton("Create flag…");create->setObjectName("eventTreeCreateFlag");field->addWidget(create);form->addRow(label,field);
                connect(create,&QPushButton::clicked,this,[this,set,key,combo]{
                    bool ok=false;const auto text=QInputDialog::getText(this,"Create flag","Name",QLineEdit::Normal,{},&ok);if(!ok)return;
                    try{
                        const int id=flagState.createFlag(text);combo->clear();
                        for(auto [flag,label]:flagState.flagChoices(row).asKeyValueRange())combo->addItem(label,flag);
                        combo->setCurrentIndex(combo->findData(id));set(key,id);error->clear();
                    }catch(const std::exception &e){error->setText(QString::fromUtf8(e.what()));}
                });
            }else form->addRow(label,combo);
            connect(combo,&QComboBox::activated,this,[set,key,combo](int at){set(key,combo->itemData(at).toInt());});};
        if(condition){auto kinds=new QComboBox;auto kindsAvailable=EventAuthoring::conditions();const int group=trigger->currentData().toInt();
            if(group!=1&&group!=2)kindsAvailable.remove("town");if(group!=3&&group!=4)kindsAvailable.remove("squad");if(group<1||group>3)kindsAvailable.remove("squadState");
            if(!kindsAvailable.contains(type))kindsAvailable[type]=EventAuthoring::conditions().value(type);for(auto [id,text]:kindsAvailable.asKeyValueRange())kinds->addItem(text,id);kinds->setCurrentIndex(kinds->findData(type));form->addRow("Condition",kinds);
            connect(kinds,&QComboBox::activated,this,[this,item,kinds](int at){auto parent=item->parent();const int index=parent->indexOfChild(item);const auto value=conditionDefault(kinds->itemData(at).toString());append(parent,value,"condition");auto replacement=parent->takeChild(parent->childCount()-1);parent->insertChild(index,replacement);delete item;tree->setCurrentItem(replacement);replacement->setExpanded(true);});
            if(type=="all"||type=="any"||type=="not")return;
        }else form->addRow(new QLabel(EventAuthoring::actions().value(type,type=="if"?"Choose the condition below this IF.":type)));
        if(type=="flag"){choice("flag","Flag",flagState.flagChoices(row));choice("value","State",{{0,"OFF"},{1,"ON"}},1,false);}
        else if(type=="chaos")number("value","Chaos Frame",0,100,50);
        else if(type=="answer"){number("value","Answer ID",0,255,255);form->addRow(new QLabel("Uses the answer from the preceding cutscene. Answer IDs depend on the question."));}
        else if(type=="squad"||type=="town"){auto options=eventTargets(project,row,type=="town"?2:3);options.remove(-1);choice("value",type=="town"?"Town":"Squad",options,1);}
        else if(type=="squadState")choice("value","Squad state",{{0,"Absent · friendly"},{1,"Present · friendly"},{128,"Absent · hostile"},{129,"Present · hostile"}},1,false);
        else if(type=="scene")choice("scene","Cutscene",scenes(project));
        else if(type=="recruit"){choice("template","Recruitment template",named("recruitment-template",row),527);choice("mode","Create",{{0,"Squad and its characters"},{1,"Characters in reserve"}},1,false);}
        else if(type=="goth")number("amount","Goth (negative = take)",-32767,32767,1);
        else if(type=="days")number("amount","Days (negative = earlier)",-32767,32767,1);
        else if(type=="program"){QMap<int,QString> programs;for(int r=1;r<115;++r)if(r!=2&&r!=3)programs[r]=EventScript::rowName(r);choice("program","Event program",programs,4);number("clearFrom","Clear flags from",0,255,200);number("clearTo","Clear flags before",0,255,255);}
        if(condition){auto options=QMap<int,QString>{{10,"Equals"},{11,"Does not equal"},{12,"Greater than"},{13,"Less than"},{14,"At least"},{15,"At most"}};if(type=="flag"||type=="squadState")options={{10,"Is"},{11,"Is not"}};choice("comparison","Test",options,10,false);}
    }
};
}
QString eventTreeSummary(const QJsonObject &n,int row,const SceneEvents *flags){const auto type=n["type"].toString();if(type=="if")return "IF";if(type=="flag")return QString("%1 %2").arg(n["value"].toInt()?"Set":"Clear",flags?flags->flagName(n["flag"].toInt(),row):EventScript::flagName(n["flag"].toInt(),row));if(type=="scene")return QString("Play cutscene · %1").arg(named("scene",row).value(n["scene"].toInt(),QString::number(n["scene"].toInt())));
    if(type=="recruit")return "Recruit · "+named("recruitment-template",row).value(n["template"].toInt(),QString("Template ID %1").arg(n["template"].toInt()));
    if(type=="program")return "Load "+EventScript::rowName(n["program"].toInt());
    if(type=="goth")return QString("%1 %2 Goth").arg(n["amount"].toInt()<0?"Take":"Give").arg(qAbs(n["amount"].toInt()));
    if(type=="days")return QString("Change calendar by %1 days").arg(n["amount"].toInt());
    if(type=="chaos")return QString("Set Chaos Frame to %1").arg(n["value"].toInt());
    return EventAuthoring::actions().value(type,"Unknown action · "+type);}
QString eventConditionSummary(const QJsonObject &n,int row,const SceneEvents *flags){const auto type=n["type"].toString();if(type=="all"||type=="any"){QStringList children;for(const auto &v:n["conditions"].toArray())children<<"("+eventConditionSummary(v.toObject(),row,flags)+")";return children.join(type=="all"?" AND ":" OR ");}if(type=="not")return "NOT ("+eventConditionSummary(n["condition"].toObject(),row,flags)+")";return conditionText(n,row,flags);}
QString eventTargetSummary(int row,int group,int target){return targets(row,group).value(target,QString("Target ID %1").arg(target));}
QMap<int,QString> eventTargets(Project *p,int row,int group){
    if(group==0||group==5)return {{-1,"Any"}};
    auto out=targets(row,group);const auto keys=catalog()["rows"].toObject()[QString::number(row)].toObject()["scenarioKeys"].toArray();if(keys.size()!=1)return out;const int key=keys.first().toInt();
    if(group==3||group==4){p->loadScenarios();if(!p->scenarios()->entries().contains(key))return out;const auto &scenario=p->scenarios()->scenario(key);out={{-1,"Any"}};
        for(int i=0;i<scenario.squads.size()&&i<scenario.layout.deployments.size();++i){const int id=u8(scenario.layout.deployments[i],0)&127;const auto members=squadMembers(scenario.squads[i]);out[id]=members.isEmpty()?QString("Squad %1 (empty)").arg(id):p->record(Table::Classes,members.first().classId).name.simplified()+" squad";}
    }else {p->loadWorld();const auto &sites=p->world()->scenario(key).sites;out={{-1,"Any"}};for(int i=0;i<p->world()->siteCount(key);++i)out[i+1]=p->world()->townName(u16(sites,1+i*4));}
    return out;
}
std::optional<QJsonObject> editEventTree(Project *p,int row,QJsonObject tree,QWidget *parent,QJsonObject *flagState){if(tree["id"].toString().isEmpty())tree["id"]=QUuid::createUuid().toString(QUuid::WithoutBraces);TreeDialog dialog(p,row,tree,parent);if(dialog.exec()!=QDialog::Accepted)return {};if(flagState)*flagState=dialog.flagState.json();return dialog.result();}
std::optional<QJsonObject> editEventTrigger(Project *project,int row,QJsonObject value,QWidget *parent){
    QDialog dialog(parent);dialog.setWindowTitle("Change WHEN");auto form=new QFormLayout(&dialog);auto group=new QComboBox;for(auto [id,label]:EventAuthoring::triggers().asKeyValueRange())group->addItem(label,id);group->setCurrentIndex(group->findData(value["group"].toInt()));form->addRow("Event",group);auto target=new QComboBox;form->addRow("Target",target);
    auto refresh=[&](int wanted){target->clear();for(auto [id,label]:eventTargets(project,row,group->currentData().toInt()).asKeyValueRange())target->addItem(label+(id>=0?QString(" [%1]").arg(id):QString{}),id);if(target->findData(wanted)<0)target->addItem(QString("Target ID %1").arg(wanted),wanted);target->setCurrentIndex(target->findData(wanted));target->setEnabled(group->currentData().toInt()>=1&&group->currentData().toInt()<=4&&value["guard"].toInt(-1)>=0);};refresh(value["target"].toInt(-1));QObject::connect(group,&QComboBox::activated,&dialog,[&](int){refresh(-1);});
    auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form->addRow(buttons);QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return {};value["group"]=group->currentData().toInt();value["target"]=target->isEnabled()?target->currentData().toInt():-1;return value;
}
}
