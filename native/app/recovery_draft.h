#pragma once
#include "engine/project.h"
#include <QJsonArray>
#include <QObject>
#include <functional>
class QDialog;
class QWidget;
class QUndoStack;
namespace lordly {
// A dialog publishes a copy of its current candidate, never an Apply action.
class RecoveryDraft final : public QObject {
    Q_OBJECT
  public:
    RecoveryDraft(QDialog *, QString label, std::function<QJsonObject()> candidate);
    QJsonObject capture() const;
  private:
    QString label_;
    std::function<QJsonObject()> candidate_;
    bool active_=true;
};
QJsonArray captureRecoveryDrafts(QWidget *root, Project &base);
void validateRecoveryDrafts(Project base, const QJsonArray &);
void restoreRecoveryDrafts(Project *, QUndoStack *, const QJsonArray &);
}
