#pragma once
#include "engine/project.h"
#include <QDialog>
class QPlainTextEdit;
class QTreeWidget;
class QLabel;
class QPushButton;
class QComboBox;
namespace lordly {
class ChangeReportDialog : public QDialog {
    Q_OBJECT
  public:
    explicit ChangeReportDialog(Project snapshot, QWidget *parent = nullptr);
    void saveReport(const QString &path) const;
    void compareWithRom(const QString &path);
  signals:
    void editRequested(const lordly::EditLocation &);

  private:
    Project snapshot_;
    QLabel *status_;
    QPushButton *copy_, *save_, *compare_, *source_;
    QString comparisonPath_;
    QPlainTextEdit *text_;
    QTreeWidget *entries_;
    ChangeReport report_;
    QVector<ProjectChange> newWarnings_, existingWarnings_;
    QComboBox *view_;
    void refreshView();
    QString sourcePath_;
    bool ready_ = false;
};
} // namespace lordly
