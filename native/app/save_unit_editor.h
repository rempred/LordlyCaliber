#pragma once
#include <QVector>
#include <QWidget>
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;
namespace lordly {
class SaveEditor;
class SaveUnitEditor final : public QWidget {
  public:
    explicit SaveUnitEditor(SaveEditor *editor);

  private:
    void refresh();
    void showUnit();
    void drawFormation();
    void refreshSwapTargets();
    QString memberName(int id) const;
    SaveEditor *editor_;
    QListWidget *units_, *members_;
    QVector<QPushButton *> grid_;
    QSpinBox *unity_;
    QComboBox *tactic_;
    QComboBox *targetUnit_, *targetMember_;
    QPushButton *swap_;
    QPushButton *apply_;
    QLabel *note_;
    QVector<int> cells_, ids_;
    bool loading_ = false;
};
} // namespace lordly
