#include "app/content_refresh.h"
#include "app/training_editor.h"
#include "app/game_rules_editor.h"
#include "app/squad_members_editor.h"
#include <QComboBox>
#include <QFormLayout>
#include <QJsonArray>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
namespace lordly {
namespace {
class TrainingCommand final:public QUndoCommand {
  public:
    TrainingCommand(Project *p,QJsonObject after):QUndoCommand("Change training opponent"),p_(p),before_(p->training()->json()),after_(std::move(after)){}
    void undo() override {p_->setTraining(before_);}
    void redo() override {p_->setTraining(after_);}
  private:Project *p_;QJsonObject before_,after_;
};
}
TrainingEditor::TrainingEditor(Project *p,QUndoStack *undo,std::function<void()> changed,QWidget *parent)
    :QWidget(parent),project_(p),undo_(undo),changed_(std::move(changed)) {
    setObjectName("trainingEditor");project_->loadTraining();
    auto outer=new QVBoxLayout(this);auto scroll=new QScrollArea;scroll->setWidgetResizable(true);
    auto content=new QWidget;scroll->setWidget(content);outer->addWidget(scroll);auto layout=new QVBoxLayout(content);
    auto intro=new QLabel("Choose a world-map location and one of its five regular opponents. Changes apply automatically. "
        "Opponent levels use your selected squad's highest level plus the offsets below. "
        "Special story opponents and locations without training keep their existing rules.");
    intro->setWordWrap(true);layout->addWidget(intro);
    layout->addWidget(new GameRulesEditor(p,undo_,changed_,{"trainingXp"}));
    auto form=new QFormLayout;layout->addLayout(form);
    node_=new QComboBox;node_->setObjectName("trainingLocation");
    for(const auto &v:p->training()->entries()) if(node_->findData(v.node)<0) node_->addItem(QString("%1 · %2").arg(v.node).arg(v.name),v.node);
    slot_=new QComboBox;slot_->setObjectName("trainingOpponent");
    for(int i=0;i<5;++i) slot_->addItem(QString("Opponent %1").arg(i+1),i);
    form->addRow("Location",node_);form->addRow("Rotation",slot_);
    price_=new QSpinBox;price_->setRange(0,255);price_->setKeyboardTracking(false);price_->setObjectName("trainingPrice");
    form->addRow("Price factor (0 = free)",price_);
    level_=new QSpinBox;level_->setRange(1,99);level_->setValue(10);level_->setObjectName("trainingPreviewLevel");
    form->addRow("Preview: highest member level",level_);
    fee_=new QLabel;fee_->setObjectName("trainingFee");form->addRow("Training fee",fee_);
    auto formula=new QLabel("Fee: level × factor × 2 through level 20; (level + 20) × factor through 40; (level − 20) × factor × 3 thereafter.");
    formula->setWordWrap(true);layout->addWidget(formula);
    members_=new SquadMembersEditor(p,"training",1,1);layout->addWidget(members_);
    notice_=new QLabel;notice_->setObjectName("trainingNotice");notice_->setWordWrap(true);layout->addWidget(notice_);
    auto reset=new QPushButton("Restore this opponent from source ROM");layout->addWidget(reset,0,Qt::AlignLeft);
    layout->addStretch();
    connect(node_,&QComboBox::currentIndexChanged,this,[this]{refresh();});
    connect(slot_,&QComboBox::currentIndexChanged,this,[this]{refresh();});
    connect(price_,&QSpinBox::valueChanged,this,[this]{save();});
    connect(level_,&QSpinBox::valueChanged,this,[this]{previewFee();});
    connect(members_,&SquadMembersEditor::edited,this,[this]{save();});
    connect(reset,&QPushButton::clicked,this,[this]{
        if(row()<0)return;auto value=project_->training()->json();auto rows=value["opponents"].toArray();
        rows[row()]=project_->training()->original(row()).json();value["opponents"]=rows;
        if(value!=project_->training()->json())undo_->push(new TrainingCommand(project_,value));
    });
    watchContentChanges(undo_, this,[this]{refresh();if(changed_)changed_();});
    refresh();
}
int TrainingEditor::row() const {
    const auto &rows=project_->training()->entries();
    for(int i=0;i<rows.size();++i)if(rows[i].node==node_->currentData().toInt()&&rows[i].slot==slot_->currentData().toInt())return i;
    return -1;
}
void TrainingEditor::previewFee(){fee_->setText(QString("%1 Goth").arg(Training::fee(level_->value(),price_->value())));}
void TrainingEditor::refresh(){
    loading_=true;const int i=row();members_->setEnabled(i>=0);price_->setEnabled(i>=0);
    if(i>=0){const auto &value=project_->training()->entries()[i];price_->setValue(value.price);members_->setMembers(squadMembers(value.squad));}
    notice_->clear();previewFee();loading_=false;
}
void TrainingEditor::save(){
    if(loading_||row()<0)return;
    try{
        const auto &old=project_->training()->entries()[row()];auto next=old;
        next.price=price_->value();next.squad=editSquadMembers(old.squad,members_->members());
        auto value=project_->training()->json();auto rows=value["opponents"].toArray();rows[row()]=next.json();value["opponents"]=rows;
        auto check=*project_->training();check.apply(value,project_->classData(),int(project_->records(Table::Items).size()));
        if(value!=project_->training()->json())undo_->push(new TrainingCommand(project_,value));
        notice_->clear();previewFee();
    }catch(const Error &e){notice_->setText("Not applied: "+QString::fromUtf8(e.what()));}
}
}
