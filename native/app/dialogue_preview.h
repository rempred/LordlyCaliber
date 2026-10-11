#pragma once
#include "engine/dialogue_session.h"
#include <QWidget>
#include <optional>
class QLabel;
class QPushButton;
class QSpinBox;
class QTimer;
class QLineEdit;
namespace lordly {
class DialoguePreview : public QWidget {
    Q_OBJECT
  public:
    explicit DialoguePreview(const Rom &, QWidget *parent = nullptr);
    ~DialoguePreview() override;
    void selectEntry(int archive, quint16 entry, std::optional<Dialogues> dialogues = {});
    void clear();
    void setPortraitProvider(DialogueDrawing::PortraitProvider provider);

  private:
    void restart();
    void advance(quint16 pressed = 0, quint16 repeat = 0);
    void refresh();
    void fail(const std::exception &);
    Rom rom_;
    DialogueDrawing::PortraitProvider portraits_;
    std::optional<Dialogues> dialogues_;
    int archive_ = -1;
    quint16 entry_ = 0;
    std::optional<DialogueSession> session_;
    std::optional<DialogueDrawing> drawing_;
    QLabel *canvas_, *status_;
    QPushButton *play_, *history_;
    QTimer *timer_;
    QSpinBox *rate_;
    std::array<QLineEdit *, 4> names_{};
    QLineEdit *suffix_;
};
} // namespace lordly
