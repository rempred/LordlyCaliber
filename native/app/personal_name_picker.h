#pragma once
#include "engine/project.h"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <optional>
namespace lordly {
struct PersonalNameChoice {
    int id = -1;
    QString newName;
};
inline std::optional<PersonalNameChoice> pickPersonalName(QWidget *parent, Project &project, int classId) {
    QDialog dialog(parent);dialog.setObjectName("personalNamePicker");
    dialog.setWindowTitle("Choose personal name");dialog.resize(440,510);
    auto layout=new QVBoxLayout(&dialog);
    auto tableRow=new QHBoxLayout;tableRow->addWidget(new QLabel("Name table"));
    auto tables=new QComboBox;tables->setObjectName("personalNameTable");
    tables->addItem("Personal names",-1);tables->addItem("Male names",0);
    tables->addItem("Female names",1);tables->addItem("Creature names",2);
    tableRow->addWidget(tables,1);layout->addLayout(tableRow);
    auto search=new QLineEdit;search->setObjectName("personalNameSearch");
    search->setPlaceholderText("Find a name…");search->setClearButtonEnabled(true);layout->addWidget(search);
    auto names=new QListWidget;names->setObjectName("personalNameList");layout->addWidget(names,1);
    auto newRow=new QHBoxLayout;auto text=new QLineEdit;text->setObjectName("newPersonalNameText");
    text->setPlaceholderText("New personal name…");text->setMaxLength(16);newRow->addWidget(text,1);
    auto add=new QPushButton("Add name");add->setObjectName("createPersonalName");add->setEnabled(false);newRow->addWidget(add);
    layout->addLayout(newRow);
    auto error=new QLabel;error->setObjectName("personalNameError");error->setWordWrap(true);layout->addWidget(error);
    auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    auto use=buttons->button(QDialogButtonBox::Ok);use->setText("Use name");use->setObjectName("usePersonalName");
    auto random=buttons->addButton("Random name",QDialogButtonBox::ActionRole);random->setObjectName("useRandomName");
    layout->addWidget(buttons);
    std::optional<PersonalNameChoice> result;
    const auto personal=project.personalNames();const int current=personal.assignment(classId);
    auto filter=[&]{
        for(int row=0;row<names->count();++row)names->item(row)->setHidden(!names->item(row)->text().contains(search->text(),Qt::CaseInsensitive));
        use->setEnabled(names->currentItem()&&!names->currentItem()->isHidden());
    };
    auto refresh=[&]{
        names->clear();error->clear();
        try {
            const int table=tables->currentData().toInt();
            if(table<0) {
                for(int id=0;id<personal.names().size();++id) {
                    auto item=new QListWidgetItem(decodeText(personal.names()[id]),names);item->setData(Qt::UserRole,id);
                    if(id==current)names->setCurrentItem(item);
                }
            } else {
                project.loadNameTables();
                for(int id=0;id<project.nameTables()->randomCount(table);++id) {
                    auto item=new QListWidgetItem(project.nameTables()->randomName(table,id),names);
                    item->setData(Qt::UserRole,id);
                }
            }
        } catch(const Error &e){error->setText(QString::fromUtf8(e.what()));}
        filter();
        if(names->currentItem())names->scrollToItem(names->currentItem());
    };
    auto choose=[&]{
        auto item=names->currentItem();if(!item||item->isHidden())return;
        if(tables->currentData().toInt()<0)result=PersonalNameChoice{item->data(Qt::UserRole).toInt(),{}};
        else result=PersonalNameChoice{-1,item->text()};
        dialog.accept();
    };
    QObject::connect(tables,&QComboBox::currentIndexChanged,&dialog,[&]{search->clear();refresh();});
    QObject::connect(search,&QLineEdit::textChanged,&dialog,filter);
    QObject::connect(names,&QListWidget::currentRowChanged,&dialog,[&]{filter();});
    QObject::connect(names,&QListWidget::itemDoubleClicked,&dialog,choose);
    QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,choose);
    QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    QObject::connect(random,&QPushButton::clicked,&dialog,[&]{result=PersonalNameChoice{};dialog.accept();});
    QObject::connect(text,&QLineEdit::textChanged,&dialog,[&]{add->setEnabled(!text->text().trimmed().isEmpty());error->clear();});
    auto create=[&]{
        try {auto validation=personal;validation.create(text->text());result=PersonalNameChoice{-1,text->text().trimmed()};dialog.accept();}
        catch(const Error &e){error->setText(QString::fromUtf8(e.what()));}
    };
    QObject::connect(add,&QPushButton::clicked,&dialog,create);
    QObject::connect(text,&QLineEdit::returnPressed,&dialog,create);
    refresh();
    return dialog.exec()==QDialog::Accepted?result:std::nullopt;
}
}
