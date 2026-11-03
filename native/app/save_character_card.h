#pragma once
#include <QImage>
#include <QMap>
#include <QWidget>
class QLabel;
class QLineEdit;
class QSpinBox;
class QComboBox;
namespace lordly {
class SaveEditor;
class SaveItemPicker;
class SaveCharacterCard final : public QWidget {
  public:
    explicit SaveCharacterCard(SaveEditor *editor);
    void selectCharacter(int id);
    int selectedCharacter() const { return id_; }
    void refresh();
  protected:
    void paintEvent(QPaintEvent *) override;
  private:
    SaveEditor *editor_;
    int id_ = -1;
    bool loading_ = false;
    QImage floor_;
    QWidget *content_;
    QLabel *portrait_, *class_, *empty_;
    QLineEdit *name_;
    QComboBox *element_;
    QMap<QString, QSpinBox *> numbers_;
    QMap<QString, SaveItemPicker *> gear_;
};
}
