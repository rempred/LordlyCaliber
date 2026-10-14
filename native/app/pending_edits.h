#pragma once
#include "engine/change_report.h"
#include <QWidget>
#include <functional>
namespace lordly {
struct PendingEdit {
    QString label;
    EditLocation location;
};
// Form drafts belong to their editors, but participate in project lifecycle checks.
class PendingEdits final : public QObject {
    Q_OBJECT
  public:
    explicit PendingEdits(QWidget *owner) : QObject(owner) {}
    std::function<QVector<PendingEdit>()> entries;
    std::function<bool()> apply;
    std::function<void()> discard;
    void notify() { emit changed(); }
  signals:
    void changed();
};
QVector<PendingEdit> pendingEdits(QWidget *root);
bool resolvePendingEdits(QWidget *root, const QString &operation,
                         const std::function<void(const EditLocation &)> &navigate);
}
