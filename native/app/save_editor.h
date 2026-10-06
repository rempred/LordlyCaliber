#pragma once
#include "engine/project.h"
#include "engine/save_document.h"
#include <QUndoStack>
#include <QWidget>
#include <optional>
class QLabel;
class QComboBox;
class QTabWidget;
class QPushButton;
namespace lordly {
class SaveEditor : public QWidget {
    Q_OBJECT
  public:
    explicit SaveEditor(Project *, QWidget *parent = nullptr);
    ~SaveEditor() override;
    void loadBytes(const QByteArray &, const QString &sourcePath = {});
    void exportFile(const QString &);
    const std::optional<SaveDocument> &document() const {
        return document_;
    }
    Project *project() const {
        return project_;
    }
    QUndoStack *undoStack() const {
        return undo_;
    }
    bool modified() const {
        return !undo_->isClean() || pendingUnitEdits_;
    }
    bool hasPendingUnitEdits() const {
        return pendingUnitEdits_;
    }
    void setUnitDraftPending(bool);
    bool edit(SaveTable, int row, const QString &key, const QVariant &);
    bool editUnitSupplies(int index, const QVector<int> &);
    bool editUnit(int index, const QVector<int> &cells, int unity, int tactic);
    bool swapUnitMembers(int fromUnit, int fromMember, int toUnit, int toMember);
    bool transferUnitMember(int fromUnit, int fromMember, int toUnit, int cell);
    bool moveToReserve(int fromUnit, int fromMember);
    bool addFromReserve(int member, int toUnit, int cell);
    bool addSoldierGroup(int toUnit, int cell);
    void apply(const SaveChange &, bool forward);
  signals:
    void changed();
    void refreshed();

  private:
    void refresh();
    bool confirmDiscard();
    bool allowImmediateEdit();
    void updateActions();
    Project *project_;
    std::optional<SaveDocument> document_;
    QString sourcePath_;
    QUndoStack *undo_;
    QComboBox *slots_;
    QLabel *status_, *error_;
    QTabWidget *tables_;
    QPushButton *export_, *restore_, *undoButton_, *redoButton_;
    bool pendingUnitEdits_ = false;
};
} // namespace lordly
