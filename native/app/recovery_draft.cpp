#include "app/recovery_draft.h"
#include <QDialog>
#include <QTimer>
#include <QUndoStack>
namespace lordly {
namespace {
QJsonObject section(Project &p,const QString &key) {
    if(key=="animationSequences"){p.loadAnimationSequences();return p.animationSequences()->json();}
    if(key=="spriteLibrary")return p.sprites().json();
    if(key=="combatArtwork"){p.loadCombatArtwork();return p.combatArtwork()->json();}
    if(key=="armyArtwork"){p.loadArmyArtwork();return p.armyArtwork()->json();}
    if(key=="avatarArtwork"){p.loadAvatarArtwork();return p.avatarArtwork()->json();}
    if(key=="iconArtwork"){p.loadIconArtwork();return p.iconArtwork()->json();}
    if(key=="categoryIcons"){p.loadCategoryIcons();return p.categoryIcons()->json();}
    throw Error("Unknown unfinished artwork section: "+key);
}
void apply(Project &p,const QJsonObject &sections) {
    // Validate the entire change on a value copy before touching the live model.
    auto next=p;
    for(auto it=sections.begin();it!=sections.end();++it) {
        section(next,it.key());
        if(!it->isObject())throw Error("Invalid unfinished artwork data");
        const auto value=it->toObject();
        if(it.key()=="animationSequences")next.setAnimationSequences(value);
        else if(it.key()=="spriteLibrary")next.setSprites(SpriteLibrary::fromJson(value));
        else if(it.key()=="combatArtwork")next.setCombatArtwork(value);
        else if(it.key()=="armyArtwork")next.setArmyArtwork(value);
        else if(it.key()=="avatarArtwork")next.setAvatarArtwork(value);
        else if(it.key()=="iconArtwork")next.setIconArtwork(value);
        else if(it.key()=="categoryIcons")next.setCategoryIcons(value);
    }
    p=std::move(next);
}
class RecoveredDraft final:public QUndoCommand {
  public:
    RecoveredDraft(Project *p,const QJsonObject &draft):QUndoCommand("Recover unfinished: "+draft["label"].toString()),project_(p),after_(draft["sections"].toObject()) {
        for(auto it=after_.begin();it!=after_.end();++it)before_[it.key()]=section(*p,it.key());
    }
    void undo() override {apply(*project_,before_);}
    void redo() override {apply(*project_,after_);}
  private: Project *project_;QJsonObject before_,after_;
};
}
RecoveryDraft::RecoveryDraft(QDialog *dialog,QString label,std::function<QJsonObject()> candidate)
    :QObject(dialog),label_(std::move(label)),candidate_(std::move(candidate)) {
    // Dialogs may be nested (for example a color picker over painting).
    auto root=dialog->parentWidget();while(root&&root->parentWidget())root=root->parentWidget();
    connect(dialog,&QDialog::finished,this,[this,root]{
        active_=false;
        // Run after the caller's Apply/Cancel handling, so canceled drafts do
        // not remain the latest snapshot simply because Undo stayed clean.
        if(root)QTimer::singleShot(0,root,[root]{
            if(auto timer=root->findChild<QTimer *>("recoveryTimer"))QMetaObject::invokeMethod(timer,"timeout",Qt::DirectConnection);
        });
    });
}
QJsonObject RecoveryDraft::capture() const {
    if(!active_||!candidate_)return {};
    const auto sections=candidate_();
    return sections.isEmpty()?QJsonObject{}:QJsonObject{{"label",label_},{"sections",sections}};
}
QJsonArray captureRecoveryDrafts(QWidget *root,Project &base) {
    QJsonArray result;
    for(auto provider:root->findChildren<RecoveryDraft *>()) {
        auto draft=provider->capture();auto sections=draft["sections"].toObject();
        for(const auto &key:sections.keys())if(sections[key]==section(base,key))sections.remove(key);
        if(sections.isEmpty())continue;
        draft["sections"]=sections;result.append(draft);
    }
    return result;
}
void validateRecoveryDrafts(Project base,const QJsonArray &drafts) {
    if(drafts.size()>16)throw Error("Too many unfinished artwork dialogs in recovery");
    for(const auto &value:drafts) {
        const auto draft=value.toObject();
        if(!value.isObject()||!draft["label"].isString()||!draft["sections"].isObject())throw Error("Invalid unfinished artwork snapshot");
        apply(base,draft["sections"].toObject());
    }
}
void restoreRecoveryDrafts(Project *project,QUndoStack *undo,const QJsonArray &drafts) {
    validateRecoveryDrafts(*project,drafts);
    for(const auto &value:drafts)undo->push(new RecoveredDraft(project,value.toObject()));
}
}
