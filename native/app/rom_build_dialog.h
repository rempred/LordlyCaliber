#pragma once
#include "rebuild/compiler.h"
#include <QDialog>
#include <QElapsedTimer>
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QTimer;
namespace lordly {
class RomBuildDialog final : public QDialog {
    Q_OBJECT
  public:
    explicit RomBuildDialog(const QString &path, QWidget *parent = nullptr);
    BuildProgressCallback progressCallback();
    void updateProgress(const BuildProgress &progress);
    void finish(const QString &error = {}, bool canTest = false, bool canReview = false);
    void done(int result) override;
  signals:
    void testRequested();
    void reviewRequested();
  protected:
    void closeEvent(QCloseEvent *event) override;
  private:
    void updateElapsed();
    void appendMessage(const QString &message);
    bool running_ = true;
    QString path_;
    QLabel *heading_, *stage_, *activity_, *elapsed_;
    QProgressBar *overall_, *current_;
    QPlainTextEdit *log_;
    QPushButton *close_, *test_, *review_;
    QTimer *timer_;
    QElapsedTimer clock_;
};
}
