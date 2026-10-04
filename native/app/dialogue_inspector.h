#pragma once
#include "engine/dialogue_archive.h"
#include <QWidget>
#include <optional>
class QComboBox;
class QListWidget;
class QPlainTextEdit;
class QLabel;
namespace lordly {
class DialogueInspector : public QWidget {
    Q_OBJECT
  public:
    explicit DialogueInspector(const Rom &rom, QWidget *parent = nullptr);
    // entry includes the native archive-table bits in its upper two bits.
    void selectEntry(int selector, int entry);

  private:
    void loadArchive();
    void showEntry();
    Rom rom_;
    std::optional<DialogueArchive> archive_;
    QComboBox *archives_;
    QListWidget *entries_;
    QPlainTextEdit *source_;
    QLabel *status_;
};
} // namespace lordly
