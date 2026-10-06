#pragma once
#include "engine/project.h"
#include <QDialog>
class QPlainTextEdit;
namespace lordly {
class ChangeReportDialog : public QDialog {
    Q_OBJECT
  public:
    explicit ChangeReportDialog(Project snapshot, QWidget *parent = nullptr);
    void saveReport(const QString &path) const;

  private:
    QPlainTextEdit *text_;
    QString sourcePath_;
    bool ready_ = false;
};
} // namespace lordly
