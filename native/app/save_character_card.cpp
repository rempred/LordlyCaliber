#include "app/save_character_card.h"
#include "app/save_editor.h"
#include "app/item_picker.h"
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QSignalBlocker>
#include <QSpinBox>

namespace lordly {
SaveCharacterCard::SaveCharacterCard(SaveEditor *editor) : QWidget(editor), editor_(editor) {
    setObjectName("saveCharacterCard");
    setMinimumWidth(360);
    try { floor_ = ArmyArtwork::floorImage(editor->project()->rom()); } catch (const Error &) {}
    setStyleSheet("QWidget#saveCharacterCard { color:#3b2914; }"
                  "QWidget#saveCharacterCard QLabel { color:#3b2914;background:transparent; }"
                  "QWidget#saveCharacterCard QLineEdit, QWidget#saveCharacterCard QSpinBox, "
                  "QWidget#saveCharacterCard QComboBox { background:#eee4c5;color:#292414; }");
    auto outer = new QVBoxLayout(this); outer->setContentsMargins(16,16,16,16);
    empty_ = new QLabel("Select a character"); outer->addWidget(empty_);
    content_ = new QWidget; outer->addWidget(content_);
    auto layout = new QVBoxLayout(content_); layout->setContentsMargins(0,0,0,0);
    auto identity = new QHBoxLayout;
    portrait_ = new QLabel; portrait_->setObjectName("saveCardPortrait");
    portrait_->setFixedSize(96,96); portrait_->setAlignment(Qt::AlignCenter);
    identity->addWidget(portrait_);
    auto heading = new QVBoxLayout;
    name_ = new QLineEdit; name_->setObjectName("saveCard_name"); heading->addWidget(name_);
    class_ = new QLabel; class_->setWordWrap(true); class_->setTextFormat(Qt::PlainText);
    heading->addWidget(class_); identity->addLayout(heading,1); layout->addLayout(identity);
    connect(name_, &QLineEdit::editingFinished, this, [this] {
        if (!loading_ && id_ > 0 && !editor_->edit(SaveTable::Characters,id_,"name",name_->text())) refresh();
    });
    auto stats = new QGridLayout;
    const QVector<QPair<QString,QString>> fields{
        {"level","Level"},{"experience","Experience"},{"hpCur","HP"},{"hpMax","Max HP"},
        {"strength","STR"},{"vitality","VIT"},{"intelligence","INT"},{"mentality","MEN"},
        {"agility","AGI"},{"dexterity","DEX"},{"alignment","Alignment"},{"luck","Luck"}};
    int index=0;
    for (const auto &[key,label] : fields) {
        auto number=new QSpinBox; number->setObjectName("saveCard_"+key);
        number->setKeyboardTracking(false); numbers_[key]=number;
        stats->addWidget(new QLabel(label),index/2,(index%2)*2);
        stats->addWidget(number,index/2,(index%2)*2+1); ++index;
        connect(number,&QSpinBox::valueChanged,this,[this,key](int value) {
            if (!loading_ && id_>0 && !editor_->edit(SaveTable::Characters,id_,key,value)) refresh();
        });
    }
    layout->addLayout(stats);
    auto form=new QFormLayout;
    element_=new QComboBox; element_->setObjectName("saveCard_element");
    for (const auto *label : {"None","Wind","Flame","Earth","Water"}) element_->addItem(label,element_->count());
    form->addRow("Element",element_);
    connect(element_,&QComboBox::activated,this,[this] {
        if (!loading_ && id_>0 && !editor_->edit(SaveTable::Characters,id_,"element",element_->currentData())) refresh();
    });
    const QVector<QPair<QString,QString>> gearSlots{{"weapon","Weapon"},{"body","Body armor"},{"offhand","Off-hand"},{"head","Head"}};
    for (const auto &[key,label] : gearSlots) {
        auto picker=new SaveItemPicker(editor->project(),true,"Choose "+label.toLower());
        picker->setObjectName("saveCard_"+key); picker->setIconSize({24,24});
        picker->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        picker->setMinimumContentsLength(15); gear_[key]=picker; form->addRow(label,picker);
        connect(picker,&QComboBox::currentIndexChanged,this,[this,key,picker] {
            if (!loading_ && id_>0 && !editor_->edit(SaveTable::Characters,id_,key,picker->currentData())) refresh();
        });
    }
    layout->addLayout(form);
    connect(editor_,&SaveEditor::refreshed,this,[this]{refresh();});
    refresh();
}
void SaveCharacterCard::paintEvent(QPaintEvent *) {
    QPainter p(this); p.fillRect(rect(),QColor("#273432"));
    if (!floor_.isNull()) p.drawTiledPixmap(rect(),QPixmap::fromImage(floor_));
    p.fillRect(rect(),QColor(0,0,0,90));
    p.fillRect(rect().adjusted(8,8,-8,-8),QColor("#ede2ba"));
    p.setPen(QPen(QColor("#b9a571"),3)); p.drawRect(rect().adjusted(2,2,-2,-2));
    p.setPen(QColor("#eee2b1")); p.drawRect(rect().adjusted(5,5,-5,-5));
}
void SaveCharacterCard::selectCharacter(int id) { id_=id; refresh(); }
void SaveCharacterCard::refresh() {
    loading_=true;
    const auto &document=editor_->document();
    const bool valid=document && document->rows(SaveTable::Characters).contains(id_);
    content_->setVisible(valid); empty_->setVisible(!valid);
    empty_->setText(id_>=100 && id_<220 ? "Soldier group" : "Select a character");
    if (!valid) {loading_=false;return;}
    const auto &doc=*document; auto project=editor_->project();
    for (const auto &field:doc.fields(SaveTable::Characters)) {
        const auto value=doc.value(SaveTable::Characters,id_,field.key);
        if (auto number=numbers_.value(field.key)) {
            number->setRange(qMin(int(field.minimum),value.toInt()),qMax(int(field.maximum),value.toInt()));
            number->setValue(value.toInt());
        } else if (field.key=="name") {
            name_->setMaxLength(qMax(field.bytes-1,int(value.toString().size()))); name_->setText(value.toString());
        }
    }
    element_->setCurrentIndex(element_->findData(doc.value(SaveTable::Characters,id_,"element")));
    const int primary=doc.value(SaveTable::Characters,id_,"class").toInt();
    const int secondary=doc.value(SaveTable::Characters,id_,"secondaryClass").toInt();
    const auto classes=project->classData();
    const int effective=primary>=0 && primary<classes.entries().size() && u8(classes.entries()[primary].record,69)==secondary ? primary : secondary;
    class_->setText(effective>=0 && effective<classes.entries().size() ? project->record(Table::Classes,effective).name.simplified() : QString("Class %1").arg(effective));
    portrait_->clear();
    try {
        project->loadAvatarArtwork(); project->loadIconArtwork();
        // The Army portrait loader receives character +0x11 (identity), not
        // the class selected for stats/name. See retail C77C0 -> C6BEC -> 45AE0.
        const auto image=project->avatarArtwork()->image(classes,primary,0);
        portrait_->setPixmap(QPixmap::fromImage(image).scaled(portrait_->size(),Qt::KeepAspectRatio,Qt::FastTransformation));
    } catch (const Error &) {}
    const QStringList keys{"weapon","body","offhand","head"};
    for (int slot=0;slot<keys.size();++slot) {
        auto box=gear_[keys[slot]]; box->clear();
        const int value=doc.value(SaveTable::Characters,id_,keys[slot]).toInt();
        const int defaultId=effective>=0 && effective<classes.entries().size() ? u16(classes.entries()[effective].record,46+slot*2) : 0;
        const auto &items=project->records(Table::Items);
        const bool known=defaultId>0 && defaultId<items.size();
        box->addItem(known ? "Class default · "+items[defaultId].name.simplified() : "Class default",0);
        for (const auto &item:items) {
            if (!item.id || item.id>510 || project->contentDeleted("items",item.id)) continue;
            if (item.id!=value && (!known || u8(item.bytes,4)!=u8(items[defaultId].bytes,4))) continue;
            QIcon icon;
            try {icon=QPixmap::fromImage(project->iconArtwork()->image(item.id));} catch (const Error &) {}
            box->addItem(icon,item.name.simplified(),item.id);
        }
        if (box->findData(value)<0) box->addItem(QString("Existing item %1").arg(value),value);
        box->setCurrentIndex(box->findData(value));
    }
    loading_=false;
}
}
