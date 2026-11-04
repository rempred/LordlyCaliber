#include "app/event_flag_links.h"
#include "engine/event_flag_links.h"
#include "engine/event_script.h"
#include "engine/project.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
namespace lordly {
QWidget *eventFlagLinks(Project *project, int row, int flag,
                       std::function<void(int,int)> open, QWidget *parent) {
    auto widget=new QWidget(parent);auto layout=new QHBoxLayout(widget);
    layout->setContentsMargins(0,0,0,0);
    for(int state:{1,0}) {
        auto link=new QLabel(state?"<a href=\"open\">Where set ON…</a>":"<a href=\"open\">Where cleared…</a>");
        link->setObjectName(QString("flagSources%1_%2").arg(state?"On":"Off").arg(flag));layout->addWidget(link);
        QObject::connect(link,&QLabel::linkActivated,widget,[=]{
            try {
                const auto writers=EventFlagLinks::writers(project->rom(),*project->sceneEvents(),*project->scenePrograms(),row,flag,state==0);
                QVector<EventFlagWriter> matches;
                for(const auto &writer:writers)if(writer.state==state||writer.state<0)matches<<writer;
                if(matches.size()==1){open(matches.first().row,matches.first().offset);return;}
                QDialog dialog(widget);dialog.setWindowTitle(project->sceneEvents()->flagName(flag,row));dialog.resize(760,430);
                auto contents=new QVBoxLayout(&dialog);
                auto title=new QLabel(state?"Events that set this flag ON":"Events that clear this flag");contents->addWidget(title);
                auto list=new QListWidget;list->setObjectName("flagWriterChoices");list->setWordWrap(true);list->setTextElideMode(Qt::ElideNone);contents->addWidget(list,1);
                for(const auto &writer:matches) {
                    const auto action=writer.programReset?QString("Cleared when the event program changes"):writer.state<0?QString("Sets or clears according to a condition"):writer.state?QString("Sets ON"):QString("Clears OFF");
                    list->addItem(writer.title+"\n"+EventScript::rowName(writer.row)+" · "+action+(writer.detail.isEmpty()?QString{}:"\n"+writer.detail));
                }
                if(matches.isEmpty())contents->addWidget(new QLabel("No matching event writes were found for this flag’s scope."));
                auto buttons=new QDialogButtonBox(QDialogButtonBox::Cancel);auto jump=buttons->addButton("Open event",QDialogButtonBox::AcceptRole);jump->setEnabled(!matches.isEmpty());contents->addWidget(buttons);
                if(!matches.isEmpty())list->setCurrentRow(0);
                QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
                QObject::connect(list,&QListWidget::itemDoubleClicked,&dialog,[&]{dialog.accept();});
                if(dialog.exec()==QDialog::Accepted&&list->currentRow()>=0){const auto writer=matches[list->currentRow()];open(writer.row,writer.offset);}
            } catch(const std::exception &e) {QMessageBox::warning(widget,"Flag references",QString::fromUtf8(e.what()));}
        });
    }
    layout->addStretch();return widget;
}
}
