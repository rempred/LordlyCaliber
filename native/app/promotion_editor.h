#pragma once
#include "engine/project.h"
#include <QUndoStack>
#include <QWidget>
#include <functional>
class QSpinBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QCheckBox;
class QListWidget;
class QPushButton;
namespace lordly {
class PromotionEditor : public QWidget {
    Q_OBJECT
  public:
    PromotionEditor(Project *project, QUndoStack *undo, std::function<void()> changed,
                    QWidget *parent = nullptr);
    void selectClass(int id);
    void showOwnEntry();

  signals:
    void editFamilyIconRequested(int family, int icon, bool create);
    void promotionClassRequested(int id);

  private:
    void refresh();
    void edit(const QString &label, const std::function<void(Promotion &)> &operation);
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    int id_ = -1, sourceId_ = -1;
    QWidget *details_;
    QPushButton *goToSource_;
    bool refreshing_ = false, ownEntry_ = false;
    QVector<QSpinBox *> gates_;
    QComboBox *mask_;
    QCheckBox *equipmentSets_;
    QComboBox *families_, *rules_, *familyIcon_;
    QLabel *familyIconPreview_;
    QLineEdit *name_;
    QLabel *notice_, *linksTitle_;
    QListWidget *links_;
};
} // namespace lordly
