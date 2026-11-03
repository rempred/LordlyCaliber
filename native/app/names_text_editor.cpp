#include "app/names_text_editor.h"
#include "app/content_refresh.h"
#include "app/text_workspace.h"
#include "engine/project.h"
#include <QCheckBox>
#include <QComboBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTableWidget>
#include <QTabWidget>
#include <QUndoStack>
#include <QVBoxLayout>
namespace lordly {
namespace {
class NameCommand final : public QUndoCommand {
  public:
    NameCommand(Project *project,Project next,const QString &label="Edit name"):QUndoCommand(label),project_(project),before_(*project),after_(std::move(next)){}
    void undo() override {*project_=before_;}
    void redo() override {*project_=after_;}
  private:
    Project *project_;Project before_,after_;
};
}
NamesTextEditor::NamesTextEditor(Project *project,QUndoStack *undo,std::function<void()> changed,QWidget *parent)
    :QWidget(parent),project_(project),undo_(undo),changed_(std::move(changed)) {
    setObjectName("namesTextEditor");
    auto outer=new QVBoxLayout(this);auto pages=new QTabWidget;outer->addWidget(pages);
    auto names=new QWidget;auto layout=new QVBoxLayout(names);pages->addTab(names,"Names");
    auto toolbar=new QHBoxLayout;kind_=new QComboBox;kind_->setObjectName("nameTableKind");
    for(const auto &[label,key]:QVector<QPair<QString,QString>>{
        {"Classes","classes"},{"Equipment","items"},{"Consumables and treasures","consumables"},
        {"Attacks and spells","attacks"},{"Character personal names","personal"},
        {"Random names · Male","random0"},{"Random names · Female","random1"},{"Random names · Creatures","random2"},
        {"World map locations","locations"},{"Towns and other sites","towns"},
        {"Class families","families"},{"Custom weapon types","weaponTypes"}})kind_->addItem(label,key);
    toolbar->addWidget(kind_);search_=new QLineEdit;search_->setObjectName("nameTableSearch");
    search_->setPlaceholderText("Find a name or ID…");search_->setClearButtonEnabled(true);toolbar->addWidget(search_,1);
    add_=new QPushButton("Add name…");add_->setObjectName("nameTableAdd");toolbar->addWidget(add_);
    remove_=new QPushButton("Remove name");remove_->setObjectName("nameTableRemove");toolbar->addWidget(remove_);
    layout->addLayout(toolbar);
    table_=new QTableWidget;table_->setObjectName("nameTable");table_->setColumnCount(2);table_->setHorizontalHeaderLabels({"ID","Name"});
    table_->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);table_->horizontalHeader()->setStretchLastSection(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);table_->setAlternatingRowColors(true);layout->addWidget(table_,1);
    scope_=new QLabel;scope_->setWordWrap(true);layout->addWidget(scope_);
    auto edit=new QHBoxLayout;text_=new QLineEdit;text_->setObjectName("nameTableText");edit->addWidget(text_,1);
    auto rename=new QPushButton("Rename");rename->setObjectName("nameTableRename");edit->addWidget(rename);layout->addLayout(edit);
    shared_=new QCheckBox("Rename all attacks sharing this name");shared_->setObjectName("nameTableShared");shared_->setChecked(true);layout->addWidget(shared_);
    error_=new QLabel;error_->setWordWrap(true);error_->setObjectName("nameTableError");layout->addWidget(error_);
    pages->addTab(createTextWorkspace(project_,undo_,changed_),"Descriptions and help text");
    connect(kind_,&QComboBox::currentIndexChanged,this,[this]{refresh();});
    connect(table_,&QTableWidget::currentCellChanged,this,[this]{selection();});
    connect(search_,&QLineEdit::textChanged,this,[this]{filter();});
    connect(rename,&QPushButton::clicked,this,[this]{applyName();});
    connect(add_,&QPushButton::clicked,this,[this]{addName();});
    connect(remove_,&QPushButton::clicked,this,[this]{removeName();});
    connect(text_,&QLineEdit::returnPressed,this,[this]{applyName();});
    watchContentChanges(undo_,this,[this]{refresh();if(changed_)changed_();},true);
    refresh();
}
void NamesTextEditor::refresh() {
    if(loading_)return;loading_=true;
    const int selected=table_->currentRow()>=0?table_->item(table_->currentRow(),0)->text().toInt():-1;
    const QSignalBlocker block(table_);table_->setRowCount(0);error_->clear();
    auto add=[&](int id,const QString &text) {
        const int row=table_->rowCount();table_->insertRow(row);table_->setItem(row,0,new QTableWidgetItem(QString::number(id)));
        table_->setItem(row,1,new QTableWidgetItem(text));if(id==selected)table_->setCurrentCell(row,1);
    };
    try {
        const auto kind=kind_->currentData().toString();
        if(kind=="classes" || kind=="items") {
            const auto type=kind=="classes"?Table::Classes:Table::Items;
            for(const auto &record:project_->records(type))if((record.id || type==Table::Classes) && !project_->contentDeleted(kind,record.id))
                add(record.id,decodeText(type==Table::Classes?project_->classNameBytes(record.id):project_->itemNameBytes(record.id)));
        } else if(kind=="attacks") {
            project_->loadAttacks();for(int id=1;id<project_->attacks()->entries().size();++id)
                if(!project_->contentDeleted(kind,id))add(id,decodeText(project_->attacks()->entry(id).name));
        } else if(kind=="personal") {
            const auto names=project_->personalNames().names();for(int id=0;id<names.size();++id)add(id,QString::fromLatin1(names[id]));
        } else if(kind.startsWith("random")) {
            project_->loadNameTables();const int pool=kind.right(1).toInt();
            for(int id=0;id<project_->nameTables()->randomCount(pool);++id)add(id,project_->nameTables()->randomName(pool,id));
        } else if(kind=="consumables") {
            project_->loadNameTables();for(int id=1;id<45;++id)add(id,project_->record(Table::Consumables,id).name);
        } else if(kind=="locations") {
            project_->loadCampaignMap();for(int id=1;id<=CampaignMap::nodeCount;++id)add(id,project_->campaignMap()->name(id));
        } else if(kind=="towns") {
            project_->loadWorld();for(int id=1;id<=ScenarioWorld::townCount;++id)add(id,project_->world()->townName(id));
        } else if(kind=="families") {
            project_->loadPromotion();for(const auto &v:project_->promotion()->families["families"].toArray()) {
                const auto family=v.toObject();add(family["id"].toInt(),Promotion::familyName(family));
            }
        } else if(kind=="weaponTypes") {
            const auto values=project_->weaponTypes().names();for(auto it=values.begin();it!=values.end();++it)add(it.key().toInt(),it.value().toString());
        }
    } catch(const Error &e){error_->setText(QString::fromUtf8(e.what()));}
    if(table_->currentRow()<0 && table_->rowCount())table_->setCurrentCell(0,1);
    loading_=false;selection();filter();
}
void NamesTextEditor::filter() {
    for(int row=0;row<table_->rowCount();++row)
        table_->setRowHidden(row,!(table_->item(row,0)->text()+" "+table_->item(row,1)->text()).contains(search_->text(),Qt::CaseInsensitive));
}
void NamesTextEditor::selection() {
    if(loading_)return;
    const int row=table_->currentRow();text_->setEnabled(row>=0);
    text_->setMaxLength(1000);text_->setText(row>=0?table_->item(row,1)->text():QString{});
    const auto kind=kind_->currentData().toString();shared_->setVisible(kind=="attacks");scope_->clear();
    const bool random=kind.startsWith("random");add_->setVisible(random);remove_->setVisible(random);
    add_->setEnabled(random && project_->nameTables() && project_->nameTables()->randomCount(kind.right(1).toInt())<NameTables::randomCapacity);
    remove_->setEnabled(random && row>=0 && table_->item(row,0)->text().toInt()>=256);
    if(row<0)return;const int id=table_->item(row,0)->text().toInt();
    if(kind=="attacks")scope_->setText(QString("%1 attacks share this name.").arg(project_->attacks()->sharingName(id).size()));
    if(kind=="personal") {
        int users=0;const auto names=project_->personalNames();for(int cls=0;cls<project_->records(Table::Classes).size();++cls)users+=names.assignment(cls)==id;
        scope_->setText(QString("Used by %1 identity classes for newly created characters.").arg(users));text_->setMaxLength(16);
    }
    if(kind.startsWith("random"))text_->setMaxLength(12);
    if(kind=="locations")text_->setMaxLength(48);
    if(kind=="towns")text_->setMaxLength(20);
    if(kind=="consumables") {
        const auto &names=*project_->nameTables();text_->setMaxLength(names.consumableCapacity(id));
        scope_->setText(QString("Up to %1 characters · used by %2 entries").arg(names.consumableCapacity(id)).arg(names.sharingConsumableName(id).size()));
    }
    scope_->setVisible(!scope_->text().isEmpty());
}
void NamesTextEditor::applyName() {
    const int row=table_->currentRow();if(row<0)return;
    const int id=table_->item(row,0)->text().toInt();const auto text=text_->text(),kind=kind_->currentData().toString();
    if(text==table_->item(row,1)->text())return;
    try {
        auto next=*project_;
        if(kind=="classes")next.setClassNameBytes(id,encodeClassName(text));
        else if(kind=="items")next.setItemNameBytes(id,encodeItemName(text,itemNameFont(next.itemNameBytes(id))));
        else if(kind=="attacks") {auto model=*next.attacks();model.rename(id,text,shared_->isChecked());next.setAttacks(model.json());}
        else if(kind=="personal") {auto model=next.personalNames();model.rename(id,text);next.setPersonalNames(model.json());}
        else if(kind.startsWith("random") || kind=="consumables") {
            auto model=*next.nameTables();if(kind=="consumables")model.setConsumableName(id,text);else model.setRandomName(kind.right(1).toInt(),id,text);
            next.setNameTables(model.json());
        } else if(kind=="locations") {auto model=*next.campaignMap();model.setName(id,text);next.setCampaignMap(model.json());}
        else if(kind=="towns") {
            auto model=*next.world();auto record=model.town(id);auto bytes=text.toLatin1();
            if(QString::fromLatin1(bytes)!=text || bytes.size()>20)throw Error("Use up to 20 game characters");
            bytes.resize(20,char(0));record.replace(1,20,bytes);model.setTown(id,record);next.setWorld(model.json());
        } else if(kind=="families") {
            auto model=*next.promotion();auto rows=model.families["families"].toArray();
            for(int i=0;i<rows.size();++i){auto family=rows[i].toObject();if(family["id"].toInt()==id){family["name"]=text;rows[i]=family;}}
            model.families["families"]=rows;next.setPromotion(model.json());
        } else if(kind=="weaponTypes") {auto names=next.weaponTypes().names();names[QString::number(id)]=text;next.setWeaponTypes(names);}
        undo_->push(new NameCommand(project_,std::move(next)));error_->clear();
    } catch(const Error &e){error_->setText(QString::fromUtf8(e.what()));}
}
void NamesTextEditor::addName() {
    const auto kind=kind_->currentData().toString();if(!kind.startsWith("random"))return;
    QInputDialog dialog(this);dialog.setObjectName("addRandomNameDialog");
    dialog.setWindowTitle("Add random name");dialog.setLabelText("Name");dialog.setInputMode(QInputDialog::TextInput);
    dialog.setTextValue({});if(auto field=dialog.findChild<QLineEdit *>())field->setMaxLength(12);
    if(dialog.exec()!=QDialog::Accepted)return;
    try {
        auto next=*project_;auto model=*next.nameTables();
        const int id=model.addRandomName(kind.right(1).toInt(),dialog.textValue().trimmed());
        next.setNameTables(model.json());undo_->push(new NameCommand(project_,std::move(next),"Add random name"));
        search_->clear();table_->setCurrentCell(id,1);table_->scrollToItem(table_->item(id,1));
    } catch(const Error &e){error_->setText(QString::fromUtf8(e.what()));}
}
void NamesTextEditor::removeName() {
    const auto kind=kind_->currentData().toString();const int row=table_->currentRow();
    if(!kind.startsWith("random") || row<0)return;
    try {
        auto next=*project_;auto model=*next.nameTables();
        model.removeRandomName(kind.right(1).toInt(),table_->item(row,0)->text().toInt());
        next.setNameTables(model.json());undo_->push(new NameCommand(project_,std::move(next),"Remove random name"));
    } catch(const Error &e){error_->setText(QString::fromUtf8(e.what()));}
}
}
