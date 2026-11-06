#include "app/campaign_setup_dialog.h"
#include "app/event_tree_dialog.h"
#include "app/save_editor.h"
#include "engine/new_campaign.h"
#include <QComboBox>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QJsonArray>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
namespace lordly {
std::optional<Project> editCampaignSetup(const Project &source,int location,bool create,QWidget *parent) {
    auto project=source;project.loadCampaignMap();project.loadSceneEvents();
    auto settings=create?NewCampaign::defaults(project,location):NewCampaign::settings(*project.sceneEvents());
    if(create)NewCampaign::initialize(project,settings);
    QDialog dialog(parent);dialog.setObjectName("campaignSetupDialog");
    dialog.setWindowTitle(create?"New Campaign":"Campaign setup");dialog.resize(700,650);
    auto layout=new QVBoxLayout(&dialog);
    QCheckBox *keepWorld=nullptr,*keepScenarios=nullptr;
    if(create) {
        keepWorld=new QCheckBox("Keep world map, locations and roads");keepWorld->setObjectName("campaignKeepWorldMap");keepWorld->setChecked(true);layout->addWidget(keepWorld);
        keepScenarios=new QCheckBox("Keep scenario maps and towns");keepScenarios->setObjectName("campaignKeepScenarioMaps");keepScenarios->setChecked(true);layout->addWidget(keepScenarios);
        keepWorld->setToolTip("Unchecked: an empty world map with one starting location.");
        keepScenarios->setToolTip("Unchecked: start without scenarios. Original maps remain available to copy.");
        auto text=new QLabel("Storyline logic and deployed squads are cleared.");
        text->setWordWrap(true);layout->addWidget(text);
    }
    auto form=new QFormLayout;layout->addLayout(form);
    auto start=new QComboBox;start->setObjectName("campaignStartLocation");
    for(int id=1;id<=40;++id)if(project.campaignMap()->position(id).x())start->addItem(project.campaignMap()->name(id),id);
    start->setCurrentIndex(start->findData(settings["location"].toInt()));form->addRow("Starting location",start);
    auto goth=new QSpinBox;goth->setObjectName("campaignStartGoth");goth->setRange(0,99999999);goth->setValue(settings["goth"].toInt());form->addRow("Starting Goth",goth);
    auto chaos=new QSpinBox;chaos->setObjectName("campaignStartChaos");chaos->setRange(0,100);chaos->setValue(settings["chaos"].toInt());form->addRow("Chaos Frame",chaos);
    layout->addWidget(new QLabel("Initially accessible locations"));
    auto locations=new QListWidget;locations->setObjectName("campaignStartingLocations");locations->setMaximumHeight(140);
    for(int id=1;id<=40;++id) {
        if(!project.campaignMap()->position(id).x())continue;
        auto item=new QListWidgetItem(project.campaignMap()->name(id),locations);item->setData(Qt::UserRole,id);
        item->setCheckState(settings["locations"].toArray().contains(id)?Qt::Checked:Qt::Unchecked);
    }
    layout->addWidget(locations);
    if(keepWorld)QObject::connect(keepWorld,&QCheckBox::toggled,locations,&QWidget::setEnabled);
    layout->addWidget(new QLabel("Starting armies"));
    auto armies=new QListWidget;armies->setObjectName("campaignStartingArmies");layout->addWidget(armies,1);
    auto defaultArmy=new QComboBox;defaultArmy->setObjectName("campaignDefaultArmy");
    form->addRow("Default army",defaultArmy);
    auto buttons=new QHBoxLayout;layout->addLayout(buttons);
    auto add=new QPushButton("Add army…"),edit=new QPushButton("Edit army…"),rename=new QPushButton("Rename…"),remove=new QPushButton("Delete");
    for(auto button:{add,edit,rename,remove})buttons->addWidget(button);
    add->setObjectName("campaignAddArmy");edit->setObjectName("campaignEditArmy");
    auto opening=new QPushButton("Edit opening and army choices…");opening->setObjectName("campaignEditOpening");layout->addWidget(opening);
    auto error=new QLabel;error->setWordWrap(true);layout->addWidget(error);
    auto ok=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(ok);
    ok->button(QDialogButtonBox::Ok)->setText(create?"Create campaign":"Apply");
    const auto refresh=[&] {
        const int selected=armies->currentItem()?armies->currentItem()->data(Qt::UserRole).toInt():settings["army"].toInt();
        armies->clear();defaultArmy->clear();
        for(const auto &v:settings["armies"].toArray()) {
            const auto a=v.toObject();auto item=new QListWidgetItem(a["name"].toString(),armies);item->setData(Qt::UserRole,a["id"].toInt());
            defaultArmy->addItem(a["name"].toString(),a["id"].toInt());
            if(a["id"].toInt()==selected)armies->setCurrentItem(item);
        }
        defaultArmy->setCurrentIndex(defaultArmy->findData(settings["army"].toInt()));
        add->setEnabled(armies->count()<NewCampaign::armyCapacity);
    };
    const auto collect=[&] {
        if(create){settings["keepWorldMap"]=keepWorld->isChecked();settings["keepScenarioMaps"]=keepScenarios->isChecked();}
        settings["location"]=start->currentData().toInt();settings["goth"]=goth->value();settings["chaos"]=chaos->value();
        settings["army"]=defaultArmy->currentData().toInt();
        QJsonArray visible;
        for(int i=0;i<locations->count();++i)if(locations->item(i)->checkState()==Qt::Checked)visible.append(locations->item(i)->data(Qt::UserRole).toInt());
        if(!visible.contains(settings["location"]))visible.append(settings["location"]);
        settings["locations"]=visible;
        NewCampaign::configure(project,settings);
    };
    QObject::connect(add,&QPushButton::clicked,&dialog,[&] {
        bool accepted=false;const auto name=QInputDialog::getText(&dialog,"Add starting army","Name",QLineEdit::Normal,{},&accepted).trimmed();
        if(!accepted||name.isEmpty())return;
        try {
            int id=0;QSet<int> used;auto rows=settings["armies"].toArray();for(const auto &v:rows)used.insert(v.toObject()["id"].toInt());while(used.contains(id))++id;
            rows.append(NewCampaign::army(project,id,name,SaveDocument::newArmy(project).serialize()));settings["armies"]=rows;refresh();
        }catch(const std::exception &e){error->setText(QString::fromUtf8(e.what()));}
    });
    QObject::connect(edit,&QPushButton::clicked,&dialog,[&] {
        if(!armies->currentItem())return;
        const int id=armies->currentItem()->data(Qt::UserRole).toInt();
        try {
            QDialog armyDialog(&dialog);armyDialog.setWindowTitle(armies->currentItem()->text());armyDialog.resize(1250,850);
            auto box=new QVBoxLayout(&armyDialog);auto editor=new SaveEditor(&project);box->addWidget(editor);
            editor->editStartingArmy(NewCampaign::armyBytes(settings,id),[&](const QByteArray &bytes) {
                auto rows=settings["armies"].toArray();
                for(int i=0;i<rows.size();++i)if(rows[i].toObject()["id"].toInt()==id)
                    rows[i]=NewCampaign::army(project,id,rows[i].toObject()["name"].toString(),bytes);
                settings["armies"]=rows;armyDialog.accept();
            });
            armyDialog.exec();
        }catch(const std::exception &e){error->setText(QString::fromUtf8(e.what()));}
    });
    QObject::connect(rename,&QPushButton::clicked,&dialog,[&] {
        if(!armies->currentItem())return;bool accepted=false;const int id=armies->currentItem()->data(Qt::UserRole).toInt();
        const auto name=QInputDialog::getText(&dialog,"Rename starting army","Name",QLineEdit::Normal,armies->currentItem()->text(),&accepted).trimmed();
        if(!accepted||name.isEmpty())return;
        auto rows=settings["armies"].toArray();for(int i=0;i<rows.size();++i)if(rows[i].toObject()["id"].toInt()==id){auto a=rows[i].toObject();a["name"]=name;rows[i]=a;}
        settings["armies"]=rows;refresh();
    });
    QObject::connect(remove,&QPushButton::clicked,&dialog,[&] {
        if(!armies->currentItem())return;
        const int id=armies->currentItem()->data(Qt::UserRole).toInt();
        auto candidate=settings;auto rows=settings["armies"].toArray();for(int i=rows.size()-1;i>=0;--i)if(rows[i].toObject()["id"].toInt()==id)rows.removeAt(i);
        candidate["armies"]=rows;
        try{NewCampaign::configure(project,candidate);settings=candidate;refresh();}catch(const std::exception &e){error->setText(QString::fromUtf8(e.what()));}
    });
    QObject::connect(opening,&QPushButton::clicked,&dialog,[&] {
        try {
            collect();auto events=*project.sceneEvents();auto trees=events.flagEdits()["1"].toObject()["trees"].toArray();
            int index=0;for(int i=0;i<trees.size();++i)if(trees[i].toObject()["id"]=="campaign-opening")index=i;
            if(trees.isEmpty())throw Error("Create an opening event in Storyline first");
            auto flagState=events.json();const auto tree=editEventTree(&project,1,trees[index].toObject(),&dialog,&flagState);
            if(tree){events.apply(flagState,*project.scenePrograms());trees[index]=*tree;events.setTrees(1,trees,*project.scenePrograms());project.setSceneEvents(events.json());
                settings["army"]=NewCampaign::settings(events)["army"];refresh();}
        }catch(const std::exception &e){error->setText(QString::fromUtf8(e.what()));}
    });
    QObject::connect(ok,&QDialogButtonBox::accepted,&dialog,[&]{try{collect();if(create)NewCampaign::applyLayoutOptions(project);dialog.accept();}catch(const std::exception &e){error->setText(QString::fromUtf8(e.what()));}});
    QObject::connect(ok,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    refresh();
    if(dialog.exec()!=QDialog::Accepted)return {};
    return project;
}
}
