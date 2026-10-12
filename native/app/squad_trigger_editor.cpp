#include "app/squad_trigger_editor.h"
#include "engine/squad_route.h"
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSpinBox>
#include <QVBoxLayout>

namespace lordly {
SquadTriggerEditor::SquadTriggerEditor(const QString &suffix, QWidget *parent) : QWidget(parent) {
    setObjectName("squadTrigger" + suffix);
    auto layout = new QVBoxLayout(this); layout->setContentsMargins(0, 0, 0, 0);
    auto box = [&](const QString &name) {
        auto w = new QComboBox; w->setObjectName("squadTrigger" + suffix + name);
        w->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        w->setMinimumContentsLength(12); return w;
    };
    auto text = [&](const QString &name) {
        auto w = new QLabel; w->setObjectName("squadTrigger" + suffix + name);
        w->setWordWrap(true); w->setTextFormat(Qt::PlainText); layout->addWidget(w); return w;
    };
    existing_ = box("Existing");
    auto choices = new QHBoxLayout; choices->addWidget(existing_,1);
    auto add = new QPushButton("New"); add->setObjectName("squadTrigger" + suffix + "New");
    add->setToolTip("Create a new trigger for this step, then choose its type and settings below.");
    choices->addWidget(add); layout->addLayout(choices);
    existing_->setToolTip("Choose a mission trigger or create a new one. Editing a reused trigger makes a separate copy when needed.");
    type_ = box("Type"); layout->addWidget(type_);
    type_->addItem("This squad is inside an area", 16);
    type_->addItem("Scenario squad count is at most…", 9);
    type_->addItem("Preserve mission trigger", -1);
    area_ = new QWidget; auto grid = new QGridLayout(area_); grid->setContentsMargins(0,0,0,0);
    const QStringList labels{"Left (X)", "Top (Z)", "Right (X)", "Bottom (Z)"};
    for (int i = 0; i < 4; ++i) {
        bounds_[i] = new QSpinBox; bounds_[i]->setRange(0,255); bounds_[i]->setKeyboardTracking(false);
        bounds_[i]->setObjectName("squadTrigger" + suffix + "Bound" + QString::number(i));
        grid->addWidget(new QLabel(labels[i]), i / 2 * 2, i % 2);
        grid->addWidget(bounds_[i], i / 2 * 2 + 1, i % 2);
        connect(bounds_[i], &QSpinBox::valueChanged, this, [this,i]{editArea(i);});
    }
    pick_ = new QPushButton("Choose area on map"); pick_->setCheckable(true);
    pick_->setObjectName("squadTrigger" + suffix + "Pick"); grid->addWidget(pick_,4,0,1,2);
    layout->addWidget(area_);
    threshold_ = new QWidget; auto form = new QFormLayout(threshold_); form->setContentsMargins(0,0,0,0);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    count_ = new QSpinBox; count_->setRange(1,30); count_->setKeyboardTracking(false);
    count_->setObjectName("squadTrigger" + suffix + "Count"); count_->setSuffix(" squads");
    form->addRow("At most", count_); layout->addWidget(threshold_);
    help_ = text("Help"); scope_ = text("Scope");
    connect(existing_, &QComboBox::activated, this, [this] {
        const int id = existing_->currentData().toInt();
        if (id < 0) { if(create)create(-id,true); }
        else if (id && selected) selected(id);
    });
    connect(add, &QPushButton::clicked, this, [this] {
        if(create)create(record_.size()==10 && u8(record_,1)==9?9:16,true);
    });
    connect(type_, &QComboBox::activated, this, [this] {
        const int kind = type_->currentData().toInt();
        if (kind > 0 && (record_.size()!=10 || kind != u8(record_,1)) && create) create(kind,record_.size()!=10);
    });
    connect(count_, &QSpinBox::valueChanged, this, [this] {
        if(loading_ || record_.size()!=10)return;
        auto c=record_; put8(c,6,quint8(count_->value())); if(changed)changed(c);
    });
    connect(pick_, &QPushButton::clicked, this, [this](bool on){if(picking)picking(on);});
}
void SquadTriggerEditor::refresh(const Eset &e, int id, bool privateTrigger) {
    const QScopedValueRollback<bool> guard(loading_,true);
    record_ = id>0 && id<=e.conditions.size()?e.conditions[id-1]:QByteArray();
    existing_->clear();
    for(int i=0;i<e.conditions.size();++i) {
        const int kind=u8(e.conditions[i],1);
        if(kind!=14 && kind!=20)existing_->addItem(SquadRoute::conditionName(e,i+1),i+1);
    }
    if(existing_->findData(id)<0)existing_->addItem(SquadRoute::conditionName(e,id),id);
    existing_->insertSeparator(existing_->count());
    existing_->addItem("New area trigger…",-16);existing_->addItem("New squad-count trigger…",-9);
    existing_->setCurrentIndex(existing_->findData(id));
    const int kind=record_.size()==10?u8(record_,1):-1;
    const bool editable=kind==9 || kind==16;
    type_->clear();type_->addItem("This squad is inside an area",16);type_->addItem("Scenario squad count is at most…",9);
    if(!editable)type_->addItem("Preserve mission trigger",-1);
    type_->setCurrentIndex(type_->findData(editable?kind:-1));
    area_->setVisible(kind==16);threshold_->setVisible(kind==9);pick_->setVisible(bool(picking));
    if(kind==16)for(int i=0;i<4;++i)bounds_[i]->setValue(u8(record_,i+2));
    if(kind==9)count_->setValue(u8(record_,6));
    help_->setText(kind==16?
        "Continues while this squad is inside the highlighted area, including its edges. Choose two corners on the map, or adjust the bounds here. This checks position, not a remembered arrival event.":kind==9?
        "Counts active squads from the scenario's normal squad list, including allies; excludes your army. True from 1 up to the limit, never at zero. Checked periodically; each success is consumed, so shared count triggers may fire for only one step per check.":
        "This existing mission event is preserved. Its custom rules cannot yet be edited with simple controls. Choose an area or squad-count trigger above to replace it for this step.");
    scope_->setText(privateTrigger?"Edits apply automatically to this trigger. Undo restores the previous setting.":
        "Editing creates a separate trigger for this step, protecting other steps and original mission events. Squads sharing this step still share its trigger.");
}
void SquadTriggerEditor::setPicking(bool on, bool second) {
    pick_->setChecked(on);pick_->setText(on?(second?"Cancel · choose second corner":"Cancel · choose first corner"):"Choose area on map");
}
void SquadTriggerEditor::editArea(int index) {
    if(loading_ || record_.size()!=10)return;
    auto c=record_; const int value=bounds_[index]->value(); put8(c,index+2,quint8(value));
    if(index<2 && value>u8(c,index+4))put8(c,index+4,quint8(value));
    if(index>=2 && value<u8(c,index))put8(c,index,quint8(value));
    if(changed)changed(c);
}
}
