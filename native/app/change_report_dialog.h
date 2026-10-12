#pragma once
#include "engine/project.h"
#include <QDialog>
class QPlainTextEdit;
class QTreeWidget;
namespace lordly {
class ChangeReportDialog : public QDialog {
    Q_OBJECT
  public:
    explicit ChangeReportDialog(Project snapshot, QWidget *parent = nullptr);
    void saveReport(const QString &path) const;
  signals:
    void editRequested(const lordly::EditLocation &);

  private:
    QPlainTextEdit *text_;
    QTreeWidget *entries_;
    ChangeReport report_;
    QString sourcePath_;
    bool ready_ = false;
};
} // namespace lordly
