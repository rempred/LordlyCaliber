#include "app/event_tree_dialog.h"
#include "engine/event_authoring.h"
#include "engine/new_campaign.h"
#include "engine/event_script.h"
#include "engine/squad_members.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QUuid>
#include <QVBoxLayout>
#include <algorithm>
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
QString chooseAction(QWidget *parent) {
    QDialog dialog(parent);
    dialog.setObjectName("eventActionPicker");
    dialog.setWindowTitle("Add action");
    dialog.resize(680, 480);
    auto layout = new QVBoxLayout(&dialog);
    auto filters = new QHBoxLayout;
    auto search = new QLineEdit;
    search->setObjectName("eventActionSearch");
    search->setPlaceholderText("Find an action…");
    auto groups = new QComboBox;
    groups->setObjectName("eventActionGroup");
    groups->addItem("All groups", QString{});
    filters->addWidget(search, 1);
    filters->addWidget(groups);
    layout->addLayout(filters);
    auto table = new QTableWidget;
    table->setObjectName("eventActionList");
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels({"Action", "Group"});
    table->verticalHeader()->hide();
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auto types = EventAuthoring::actionTypes();
    std::sort(types.begin(), types.end(), [](const auto &a, const auto &b) {
        return a.group == b.group ? a.name.localeAwareCompare(b.name) < 0
                                 : a.group.localeAwareCompare(b.group) < 0;
    });
    for (const auto &action : types) {
        const int row = table->rowCount();
        table->insertRow(row);
        auto item = new QTableWidgetItem(action.name);
        item->setData(Qt::UserRole, action.type);
        table->setItem(row, 0, item);
        table->setItem(row, 1, new QTableWidgetItem(action.group));
        if (groups->findData(action.group) < 0) groups->addItem(action.group, action.group);
    }
    table->setSortingEnabled(true);
    table->sortItems(1, Qt::AscendingOrder);
    layout->addWidget(table, 1);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText("Add action");
    layout->addWidget(buttons);
    auto updateSelection = [&] {
        buttons->button(QDialogButtonBox::Ok)->setEnabled(
            table->currentRow() >= 0 && !table->isRowHidden(table->currentRow()));
    };
    auto filter = [&] {
        const auto query = search->text().trimmed();
        const auto group = groups->currentData().toString();
        int first = -1;
        for (int row = 0; row < table->rowCount(); ++row) {
            const auto name = table->item(row, 0)->text();
            const auto category = table->item(row, 1)->text();
            const auto type = table->item(row, 0)->data(Qt::UserRole).toString();
            const bool visible = (group.isEmpty() || group == category) &&
                (name.contains(query, Qt::CaseInsensitive) || category.contains(query, Qt::CaseInsensitive) ||
                 type.contains(query, Qt::CaseInsensitive));
            table->setRowHidden(row, !visible);
            if (visible && first < 0) first = row;
        }
        if (table->currentRow() < 0 || table->isRowHidden(table->currentRow())) table->setCurrentCell(first, 0);
        updateSelection();
    };
    QObject::connect(search, &QLineEdit::textChanged, &dialog, filter);
    QObject::connect(groups, &QComboBox::currentIndexChanged, &dialog, filter);
    QObject::connect(table, &QTableWidget::itemSelectionChanged, &dialog, updateSelection);
    QObject::connect(table->horizontalHeader(), &QHeaderView::sortIndicatorChanged, &dialog, filter);
    QObject::connect(table, &QTableWidget::cellDoubleClicked, &dialog, [&](int row, int) {
        if (!table->isRowHidden(row)) dialog.accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    filter();
    if (dialog.exec() != QDialog::Accepted || table->currentRow() < 0) return {};
    return table->item(table->currentRow(), 0)->data(Qt::UserRole).toString();
}
QJsonObject conditionDefault(QString type="flag"){
    if(type=="all"||type=="any")return {{"type",type},{"conditions",QJsonArray{conditionDefault()}}};
    if(type=="not")return {{"type",type},{"condition",conditionDefault()}};
    return {{"type",type},{"flag",200},{"value",type=="answer"?255:type=="chaos"?50:1},{"comparison",10}};
}
QJsonObject actionDefault(QString type){
    if(type=="if")return {{"type",type},{"condition",conditionDefault()},{"then",QJsonArray{}},{"else",QJsonArray{}}};
    if(type=="startingArmy")return {{"type",type},{"army",0}};
    if(type=="prepareBattlefield")return {{"type",type},{"battlefield",1}};
    if(type=="completeMission"||type=="battleResults"||type=="returnToMap")return {{"type",type},{"location",6}};
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
    Project *project;int row;QJsonObject original;SceneEvents flagState;QByteArray recruitmentTemplates;QLineEdit *name;QComboBox *trigger,*target;QCheckBox *after;QTreeWidget *tree;QScrollArea *details;QLabel *error;
    TreeDialog(Project *p,int r,QJsonObject value,QWidget *parent):QDialog(parent),project(p),row(r),original(value),flagState(*p->sceneEvents()){
        setWindowTitle("Edit event logic");resize(1050,740);auto outer=new QVBoxLayout(this);auto when=new QFormLayout;outer->addLayout(when);
        name=new QLineEdit(value["name"].toString("New event"));name->setMaxLength(120);name->setObjectName("eventTreeName");when->addRow("Name",name);
        trigger=new QComboBox;trigger->setObjectName("eventTreeWhen");for(auto [id,label]:EventAuthoring::triggers().asKeyValueRange())if(row!=1||id==0)trigger->addItem(row==1?QString("New Game"):label,id);
        trigger->setCurrentIndex(trigger->findData(value["group"].toInt(3)));when->addRow("WHEN",trigger);target=new QComboBox;target->setObjectName("eventTreeTarget");when->addRow("Target",target);
        auto refreshTarget=[this](int wanted){target->clear();const int group=trigger->currentData().toInt();for(auto [id,label]:eventTargets(project,row,group).asKeyValueRange())target->addItem(label+(id<0?QString{}:QString(" [%1]").arg(id)),id);
            if(target->findData(wanted)<0)target->addItem(QString("Target ID %1").arg(wanted),wanted);target->setCurrentIndex(target->findData(wanted));target->setEnabled(group>=1&&group<=4);};
        refreshTarget(value["target"].toInt(-1));connect(trigger,&QComboBox::activated,this,[refreshTarget](int){refreshTarget(-1);});
        after=new QCheckBox("Run after existing events");after->setChecked(value["after"].toBool());when->addRow(after);
        auto bar=new QHBoxLayout;outer->addLayout(bar);
        auto addIf=new QPushButton("Add IF / ELSE");addIf->setObjectName("eventTreeAddIf");bar->addWidget(addIf);
        auto addAction=new QPushButton("Add action…");addAction->setObjectName("eventTreeAddAction");bar->addWidget(addAction);
        auto addCondition=new QPushButton("Add condition");bar->addWidget(addCondition);
        auto remove=new QPushButton("Remove");remove->setObjectName("eventTreeRemove");bar->addWidget(remove);auto up=new QPushButton("Move up"),down=new QPushButton("Move down");bar->addWidget(up);bar->addWidget(down);
        auto split=new QSplitter;outer->addWidget(split,1);tree=new QTreeWidget;tree->setObjectName("eventTreeSteps");tree->setHeaderHidden(true);tree->setWordWrap(true);tree->setMinimumWidth(300);split->addWidget(tree);
        details=new QScrollArea;details->setWidgetResizable(true);split->addWidget(details);split->setSizes({480,500});
        for(const auto &v:value["nodes"].toArray())append(tree->invisibleRootItem(),v.toObject(),"node");tree->expandAll();
        connect(tree,&QTreeWidget::currentItemChanged,this,[this]{showFields();});
        connect(addIf,&QPushButton::clicked,this,[this]{addNode(actionDefault("if"));});
        connect(addAction,&QPushButton::clicked,this,[this]{const auto type=chooseAction(this);if(!type.isEmpty()){auto v=actionDefault(type);if(type=="scene"&&!scenes(project).isEmpty())v["scene"]=scenes(project).firstKey();addNode(v);}});
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
    void title(QTreeWidgetItem *item){const auto n=item->data(0,Qt::UserRole).toJsonObject();item->setText(0,item->data(0,Qt::UserRole+1)=="condition"?conditionText(n,row,&flagState):eventTreeSummary(n,row,&flagState,project));}
    void addNode(QJsonObject n){
        const auto campaign=NewCampaign::settings(flagState);
        if(n["type"]=="startingArmy"&&!campaign.isEmpty())n["army"]=campaign["army"];
        auto previous=tree->currentItem();auto parent=previous;while(parent&&parent->data(0,Qt::UserRole+1)!="branch"){
        if(parent->data(0,Qt::UserRole).toJsonObject()["type"]=="if"){parent=parent->child(1);break;}parent=parent->parent();}
        if(!parent)parent=tree->invisibleRootItem();append(parent,n,"node");parent->setExpanded(true);auto item=parent->child(parent->childCount()-1);
        const int neighbor=previous?parent->indexOfChild(previous):-1;
        if(neighbor>=0&&previous->data(0,Qt::UserRole+1)=="node"){
            parent->takeChild(parent->indexOfChild(item));parent->insertChild(neighbor+1,item);
        }
        item->setExpanded(true);tree->setCurrentItem(n["type"]=="if"?item->child(0):item);}
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
            connect(combo,&QComboBox::activated,this,[set,key,combo](int at){set(key,combo->itemData(at).toInt());});return combo;};
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
        else if(type=="startingArmy") {
            QMap<int,QString> options;
            for(const auto &v:NewCampaign::settings(flagState)["armies"].toArray()) {
                const auto a=v.toObject();options[a["id"].toInt()]=a["name"].toString();
            }
            choice("army","Starting army",options,options.isEmpty()?0:options.firstKey(),false);
        }
        else if(type=="prepareBattlefield"){
            project->loadScenarios();QMap<int,QString> maps;for(const auto &s:project->scenarios()->entries())
                if(ScenarioResources::hasScenario(project->rom(),s.key))maps[s.key]=project->scenarioName(s.key);
            choice("battlefield","Battlefield",maps,1);
        }
        else if(type=="completeMission"||type=="returnToMap"||type=="battleResults"){
            project->loadCampaignMap();QMap<int,QString> places;for(int id=1;id<=40;++id)
                if(project->campaignMap()->position(id).x())places[id]=project->campaignMap()->name(id);
            choice("location","Return to",places,1);
            if(type=="completeMission"){
                auto following=new QCheckBox("Another mission follows at this location");
                following->setChecked(n["nextMission"].toBool());form->addRow(following);
                connect(following,&QCheckBox::toggled,this,[this,item](bool enabled){auto action=item->data(0,Qt::UserRole).toJsonObject();action["nextMission"]=enabled;item->setData(0,Qt::UserRole,action);title(item);});
            }
        }
        else if(type=="recruit") {
            if(recruitmentTemplates.isEmpty()) recruitmentTemplates=ScenarioResources::load(project->rom()).enemyData();
            const int selected=n["template"].toInt(527);
            const int selectedClass=selected>=0&&selected*35+35<=recruitmentTemplates.size()?u8(recruitmentTemplates,selected*35):-1;
            QMap<int,QString> classes,variants;
            QMap<int,int> firstTemplate;
            for(int id=0;id<556&&id*35+35<=recruitmentTemplates.size();++id) {
                const int classId=u8(recruitmentTemplates,id*35);
                if((classId<=0&&classId!=selectedClass)||classId>=project->records(Table::Classes).size()) continue;
                classes[classId]=project->record(Table::Classes,classId).name;
                if(!firstTemplate.contains(classId)) firstTemplate[classId]=id;
                if(classId!=selectedClass) continue;
                const auto record=recruitmentTemplates.mid(id*35,35);
                const int level=qint8(u8(record,1));
                auto gear=[&](int offset){const int itemId=u16(record,offset);return itemId==0?QString("Class default"):itemId<project->records(Table::Items).size()?project->record(Table::Items,itemId).name:QString("Item ID %1").arg(itemId);};
                variants[id]=QString("Level offset %1%2 · %3 / %4").arg(level>=0?"+":"").arg(level).arg(gear(2),gear(4));
            }
            auto character=new QComboBox;
            character->setObjectName("eventRecruitCharacter");
            character->setEditable(true);character->setInsertPolicy(QComboBox::NoInsert);
            character->completer()->setFilterMode(Qt::MatchContains);character->completer()->setCaseSensitivity(Qt::CaseInsensitive);
            for(auto [id,label]:classes.asKeyValueRange()) character->addItem(label,id);
            character->setCurrentIndex(character->findData(selectedClass));
            form->addRow("Character / class",character);
            connect(character,&QComboBox::activated,this,[this,item,character,firstTemplate,set](int at){
                const int classId=character->itemData(at).toInt();if(!firstTemplate.contains(classId))return;
                set("template",firstTemplate.value(classId));
                QTimer::singleShot(0,this,[this,item]{if(tree->currentItem()==item)showFields();});
            });
            auto preset=choice("template","Preset",variants,527);
            preset->setObjectName("eventRecruitPreset");
            choice("mode","Create",{{0,"Squad and its characters"},{1,"Characters in reserve"}},1,false);
        }
        else if(type=="goth")number("amount","Goth (negative = take)",-32767,32767,1);
        else if(type=="days")number("amount","Days (negative = earlier)",-32767,32767,1);
        else if(type=="program"){QMap<int,QString> programs;for(int r=1;r<SceneEvents::rowCapacity;++r){const auto name=flagState.missionName(r);if(!EventAuthoring::newMissionSlot(r)||!name.isEmpty())programs[r]=name.isEmpty()?EventScript::rowName(r):name;}choice("program","Event program",programs,4);number("clearFrom","Clear flags from",0,255,200);number("clearTo","Clear flags before",0,255,255);}
        if(condition){auto options=QMap<int,QString>{{10,"Equals"},{11,"Does not equal"},{12,"Greater than"},{13,"Less than"},{14,"At least"},{15,"At most"}};if(type=="flag"||type=="squadState")options={{10,"Is"},{11,"Is not"}};choice("comparison","Test",options,10,false);}
    }
};
}
QString eventTreeSummary(const QJsonObject &n,int row,const SceneEvents *flags,const Project *project){const auto type=n["type"].toString();if(type=="if")return "IF";if(type=="flag")return QString("%1 %2").arg(n["value"].toInt()?"Set":"Clear",flags?flags->flagName(n["flag"].toInt(),row):EventScript::flagName(n["flag"].toInt(),row));if(type=="scene")return QString("Play cutscene · %1").arg(named("scene",row).value(n["scene"].toInt(),QString::number(n["scene"].toInt())));
    if(type=="recruit")return "Add characters · "+named("recruitment-template",row).value(n["template"].toInt(),QString("Template ID %1").arg(n["template"].toInt()));
    if(type=="initializeCampaign")return "Initialize campaign settings";
    if(type=="startingArmy") {
        QString name=QString("Army %1").arg(n["army"].toInt()+1);
        if(flags)for(const auto &v:NewCampaign::settings(*flags)["armies"].toArray())
            if(v.toObject()["id"]==n["army"])name=v.toObject()["name"].toString();
        return "Use starting army · "+name;
    }
    if(type=="program"){const int id=n["program"].toInt();return "Load "+(flags&&!flags->missionName(id).isEmpty()?flags->missionName(id):EventScript::rowName(id));}
    if(type=="goth")return QString("%1 %2 Goth").arg(n["amount"].toInt()<0?"Take":"Give").arg(qAbs(n["amount"].toInt()));
    if(type=="days")return QString("Change calendar by %1 days").arg(n["amount"].toInt());
    if(type=="chaos")return QString("Set Chaos Frame to %1").arg(n["value"].toInt());
    if(type=="prepareBattlefield")return project?"Select battlefield · "+project->scenarioName(n["battlefield"].toInt()):QString("Select battlefield %1").arg(n["battlefield"].toInt());
    if(type=="completeMission"||type=="battleResults"||type=="returnToMap")return QString(type=="battleResults"?"Open post-mission save menu":type=="returnToMap"?"Return to world map":"Finish mission and return to world map")+" · "+(project&&project->campaignMap()?project->campaignMap()->name(n["location"].toInt()):named("world-location",row).value(n["location"].toInt(),QString("Map location %1").arg(n["location"].toInt())));
    return EventAuthoring::actions().value(type,"Unknown action · "+type);}
QString eventConditionSummary(const QJsonObject &n,int row,const SceneEvents *flags){const auto type=n["type"].toString();if(type=="all"||type=="any"){QStringList children;for(const auto &v:n["conditions"].toArray())children<<"("+eventConditionSummary(v.toObject(),row,flags)+")";return children.join(type=="all"?" AND ":" OR ");}if(type=="not")return "NOT ("+eventConditionSummary(n["condition"].toObject(),row,flags)+")";return conditionText(n,row,flags);}
QString eventTargetSummary(int row,int group,int target){return targets(row,group).value(target,QString("Target ID %1").arg(target));}
QMap<int,QString> eventTargets(Project *p,int row,int group){
    if(group==0||group==5)return {{-1,"Any"}};
    auto out=targets(row,group);auto keys=catalog()["rows"].toObject()[QString::number(row)].toObject()["scenarioKeys"].toArray();
    if(EventAuthoring::newMissionSlot(row)&&p->sceneEvents()){
        for(auto tree:p->sceneEvents()->flagEdits()[QString::number(row)].toObject()["trees"].toArray())
            for(auto node:tree.toObject()["nodes"].toArray())if(node.toObject()["type"]=="prepareBattlefield")keys.append(node.toObject()["battlefield"]);
    }
    if(keys.size()!=1)return out;const int key=keys.first().toInt();
    if(group==3||group==4){p->loadScenarios();if(!p->scenarios()->entries().contains(key))return out;const auto &scenario=p->scenarios()->scenario(key);out={{-1,"Any"}};
        for(int i=0;i<scenario.squads.size()&&i<scenario.layout.deployments.size();++i){const int id=u8(scenario.layout.deployments[i],0)&127;const auto members=squadMembers(scenario.squads[i]);out[id]=members.isEmpty()?QString("Squad %1 (empty)").arg(id):p->record(Table::Classes,members.first().classId).name.simplified()+" squad";}
    }else {p->loadWorld();const auto &sites=p->world()->scenario(key).sites;out={{-1,"Any"}};for(int i=0;i<p->world()->siteCount(key);++i)out[i+1]=p->world()->townName(u16(sites,1+i*4));}
    return out;
}
std::optional<QJsonObject> editEventTree(Project *p,int row,QJsonObject tree,QWidget *parent,QJsonObject *flagState,const QVector<int> &path,ChartTreeEdit edit,bool fromChart){
    if(tree["id"].toString().isEmpty())tree["id"]=QUuid::createUuid().toString(QUuid::WithoutBraces);
    TreeDialog dialog(p,row,tree,parent);
    auto item=dialog.tree->invisibleRootItem();
    for(int child:path){item=item->child(child);if(!item)throw Error("The selected step has changed");}
    if(item&&!path.isEmpty()){dialog.tree->setCurrentItem(item);dialog.tree->scrollToItem(item);}
    if(edit!=ChartTreeEdit::Edit){
        if(path.isEmpty())dialog.tree->setCurrentItem(nullptr);
        if(edit==ChartTreeEdit::AddDecision)dialog.addNode(actionDefault("if"));
        if(edit==ChartTreeEdit::AddAction){const auto type=chooseAction(parent);if(type.isEmpty())return {};
            auto node=actionDefault(type);if(type=="scene"){auto options=scenes(p);if(!options.isEmpty())node["scene"]=options.firstKey();}
            dialog.addNode(node);}
        if(edit==ChartTreeEdit::Remove){
            if(item==dialog.tree->invisibleRootItem())throw Error("Select a step to delete");
            // A decision card represents its IF, including both branches.
            if(item->data(0,Qt::UserRole+1)=="condition"){
                while(item&&item->data(0,Qt::UserRole+1)=="condition")item=item->parent();
                if(!item)throw Error("Could not locate the selected decision");
            }
            if(item->data(0,Qt::UserRole).toJsonObject()["type"]=="if"){
                QMessageBox confirm(QMessageBox::Question,"Remove IF condition","Which branch should run in its place?",QMessageBox::Cancel,parent);
                auto yes=confirm.addButton("Keep Yes",QMessageBox::ActionRole);
                auto no=confirm.addButton("Keep No",QMessageBox::ActionRole);
                auto both=confirm.addButton("Remove both branches",QMessageBox::DestructiveRole);
                confirm.exec();auto answer=confirm.clickedButton();
                if(answer!=yes&&answer!=no&&answer!=both)return {};
                if(answer!=both){
                    auto list=item->parent()?item->parent():dialog.tree->invisibleRootItem();
                    const int index=list->indexOfChild(item);
                    list->insertChildren(index,item->child(answer==yes?1:2)->takeChildren());
                }
            }
            delete item;if(flagState)*flagState=dialog.flagState.json();return dialog.result();
        }
    }
    if(fromChart && dialog.tree->currentItem() && (!path.isEmpty() || edit!=ChartTreeEdit::Edit)){
        // Present only the selected step's controls. The canvas remains the
        // surrounding navigation; the full event editor is available on its title.
        auto layout=static_cast<QVBoxLayout *>(dialog.layout());
        for(int i=0;i<2;++i)for(int j=0;j<layout->itemAt(i)->layout()->count();++j)
            if(auto w=layout->itemAt(i)->layout()->itemAt(j)->widget())w->hide();
        dialog.tree->hide();
        auto showCompound=[&dialog,layout]{
            const auto selected=dialog.tree->currentItem();if(!selected)return;
            bool compound=false;
            for(auto parent=selected;parent;parent=parent->parent()){
                const auto type=parent->data(0,Qt::UserRole).toJsonObject()["type"].toString();
                compound|=type=="all"||type=="any"||type=="not";
            }
            dialog.tree->setVisible(compound);
            for(int j=0;j<layout->itemAt(1)->layout()->count();++j)
                if(auto button=qobject_cast<QPushButton *>(layout->itemAt(1)->layout()->itemAt(j)->widget()))
                    button->setVisible(compound&&(button->text()=="Add condition"||button->text()=="Remove"));
        };
        QObject::connect(dialog.tree,&QTreeWidget::currentItemChanged,&dialog,showCompound);showCompound();
        dialog.setWindowTitle(dialog.tree->currentItem()->text(0));dialog.resize(610,340);
    }
    if(dialog.exec()!=QDialog::Accepted)return {};if(flagState)*flagState=dialog.flagState.json();return dialog.result();
}
std::optional<QJsonObject> editEventTrigger(Project *project,int row,QJsonObject value,QWidget *parent){
    QDialog dialog(parent);dialog.setWindowTitle("Change WHEN");auto form=new QFormLayout(&dialog);auto group=new QComboBox;for(auto [id,label]:EventAuthoring::triggers().asKeyValueRange())group->addItem(label,id);group->setCurrentIndex(group->findData(value["group"].toInt()));form->addRow("Event",group);auto target=new QComboBox;form->addRow("Target",target);
    auto refresh=[&](int wanted){target->clear();for(auto [id,label]:eventTargets(project,row,group->currentData().toInt()).asKeyValueRange())target->addItem(label+(id>=0?QString(" [%1]").arg(id):QString{}),id);if(target->findData(wanted)<0)target->addItem(QString("Target ID %1").arg(wanted),wanted);target->setCurrentIndex(target->findData(wanted));target->setEnabled(group->currentData().toInt()>=1&&group->currentData().toInt()<=4&&value["guard"].toInt(-1)>=0);};refresh(value["target"].toInt(-1));QObject::connect(group,&QComboBox::activated,&dialog,[&](int){refresh(-1);});
    auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form->addRow(buttons);QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return {};value["group"]=group->currentData().toInt();value["target"]=target->isEnabled()?target->currentData().toInt():-1;return value;
}
std::optional<QJsonObject> editRetailCondition(Project *project,int row,int group,QJsonObject gate,QWidget *parent,QJsonObject *flags){
    QJsonObject node{{"type","if"},{"condition",gate.contains("condition")?gate["condition"].toObject():conditionDefault()},
                     {"then",QJsonArray{}},{"else",QJsonArray{}}};
    QJsonObject tree{{"id","condition-editor-draft"},{"name","Additional condition"},{"group",group},{"nodes",QJsonArray{node}}};
    TreeDialog dialog(project,row,tree,parent);dialog.setWindowTitle("Add or edit condition");
    auto layout=static_cast<QVBoxLayout *>(dialog.layout());
    auto heading=layout->itemAt(0)->layout();
    for(int i=0;i<heading->count();++i)if(auto widget=heading->itemAt(i)->widget())widget->hide();
    auto tools=layout->itemAt(1)->layout();
    for(int i=0;i<tools->count();++i)if(auto widget=tools->itemAt(i)->widget()){
        const auto button=qobject_cast<QPushButton *>(widget);
        if(button&&button->text()!="Add condition"&&button->text()!="Remove")widget->hide();
    }
    auto root=dialog.tree->topLevelItem(0);root->setText(0,"IF");root->child(1)->setHidden(true);root->child(2)->setHidden(true);
    dialog.tree->setCurrentItem(root->child(0));
    auto outcomes=new QFormLayout;
    auto outcome=[&](const QString &key,const QString &label,const QString &fallback){
        auto box=new QComboBox;box->addItem("Check the original condition","original");box->addItem("Take the original Yes branch","yes");box->addItem("Take the original No branch","no");
        box->setCurrentIndex(box->findData(gate[key].toString(fallback)));outcomes->addRow(label,box);return box;
    };
    auto yes=outcome("yes","When this is true","original"),no=outcome("no","Otherwise","no");
    layout->insertLayout(layout->count()-2,outcomes);
    if(dialog.exec()!=QDialog::Accepted)return {};
    if(flags)*flags=dialog.flagState.json();
    return QJsonObject{{"mode","if"},{"condition",dialog.result()["nodes"].toArray().first().toObject()["condition"]},
                       {"yes",yes->currentData().toString()},{"no",no->currentData().toString()}};
}
}
