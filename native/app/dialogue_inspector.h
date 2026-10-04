#pragma once
#include "engine/project.h"
#include <QWidget>
#include <functional>
#include <optional>
class QComboBox;
class QListWidget;
class QPlainTextEdit;
class QLabel;
class QPushButton;
class QUndoStack;
class QTimer;
namespace lordly {
class DialoguePreview;
class DialogueInspector : public QWidget {
    Q_OBJECT
  public:
    explicit DialogueInspector(const Rom &rom, QWidget *parent = nullptr);
    DialogueInspector(Project *, QUndoStack *, std::function<void()> changed,
                      QWidget *parent = nullptr);
    // entry includes the native archive-table bits in its upper two bits.
    void selectEntry(int selector, int entry);

  private:
    void loadArchive(int row = 0);
    void editSource();
    void updatePreview();
    void validity(bool valid);
    void publish(const QString &label, const Dialogues &next);
    DialogueArchive::Table table() const;
    int count() const;
    QByteArray bytes(int index) const;
    void showEntry();
    Rom rom_;
    Project *project_ = nullptr;
    QUndoStack *undo_ = nullptr;
    std::function<void()> changed_;
    bool editing_ = false;
    QPushButton *add_, *restore_, *discard_;
    QTimer *previewTimer_;
    std::optional<DialogueArchive> archive_;
    QComboBox *archives_;
    QListWidget *entries_;
    QPlainTextEdit *source_;
    QLabel *status_;
    DialoguePreview *preview_;
};
} // namespace lordly
