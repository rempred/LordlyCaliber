#include "app/custom_content.h"
#include "app/content_refresh.h"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
namespace lordly {
namespace {
class ContentCommand final:public QUndoCommand {
public:
    ContentCommand(Project *p,Project after,const QString &label):QUndoCommand(label),p_(p),before_(*p),after_(std::move(after)){}
    void undo()override{*p_=before_;}
    void redo()override{*p_=after_;}
private:Project *p_;Project before_,after_;
};
}
void pushContentProject(Project *p,QUndoStack *undo,Project after,const QString &label) {
    undo->push(new ContentCommand(p,std::move(after),label));
}
void deleteCustomEntry(QWidget *parent,Project *p,QUndoStack *undo,const QString &kind,int id) {
    try {auto next=*p;next.deleteCustomContent(kind,id);pushContentProject(p,undo,std::move(next),"Delete custom entry");}
    catch(const std::exception &e){QMessageBox::information(parent,"Cannot delete",QString::fromUtf8(e.what()));}
}
void showCustomContent(QWidget *parent,Project *p,QUndoStack *undo,const QString &kind) {
    QDialog dialog(parent);dialog.setWindowTitle("Delete custom content");dialog.resize(560,520);
    auto layout=new QVBoxLayout(&dialog);auto types=new QComboBox;types->setObjectName("customContentType");
    for(const auto &pair:QVector<QPair<QString,QString>>{{"Classes","classes"},{"Items","items"},{"Attacks","attacks"},
        {"Weapon types","weaponTypes"},{"Class families","families"},{"Shops","shops"},{"Encounter pools","pools"}})
        types->addItem(pair.first,pair.second);
    if(!kind.isEmpty())types->setCurrentIndex(types->findData(kind));layout->addWidget(types);
    auto list=new QListWidget;list->setObjectName("customContentList");layout->addWidget(list,1);
    auto uses=new QLabel;uses->setObjectName("customContentUses");uses->setWordWrap(true);layout->addWidget(uses);
    auto remove=new QPushButton("Delete selected");remove->setObjectName("deleteCustomContent");layout->addWidget(remove);
    auto close=new QDialogButtonBox(QDialogButtonBox::Close);layout->addWidget(close);
    QObject::connect(close,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    auto select=[&]{
        remove->setEnabled(false);uses->clear();if(!list->currentItem())return;
        try {
            const auto refs=p->deletionUses(types->currentData().toString(),list->currentItem()->data(Qt::UserRole).toInt());
            remove->setEnabled(refs.isEmpty());
            uses->setText(refs.isEmpty()?QString():"Used by:\n"+refs.mid(0,8).join('\n')+(refs.size()>8?QString("\n…and %1 more").arg(refs.size()-8):QString()));
        }catch(const std::exception &e){uses->setText(QString::fromUtf8(e.what()));}
    };
    auto refresh=[&]{
        const int id=list->currentItem()?list->currentItem()->data(Qt::UserRole).toInt():-1;
        list->clear();
        try {const auto rows=p->customContent(types->currentData().toString());
            for(auto it=rows.begin();it!=rows.end();++it){auto item=new QListWidgetItem(QString("%1 · %2").arg(it.key()).arg(it.value()),list);item->setData(Qt::UserRole,it.key());if(it.key()==id)list->setCurrentItem(item);}
        }catch(const std::exception &e){uses->setText(QString::fromUtf8(e.what()));}
        if(!list->currentItem()&&list->count())list->setCurrentRow(0);select();
        if(!list->count())uses->setText("No custom entries to delete.");
    };
    QObject::connect(types,&QComboBox::currentIndexChanged,&dialog,[&]{refresh();});
    QObject::connect(list,&QListWidget::currentRowChanged,&dialog,[&]{select();});
    QObject::connect(remove,&QPushButton::clicked,&dialog,[&]{if(list->currentItem())deleteCustomEntry(&dialog,p,undo,types->currentData().toString(),list->currentItem()->data(Qt::UserRole).toInt());});
    watchContentChanges(undo,&dialog,refresh);refresh();dialog.exec();
}
}
