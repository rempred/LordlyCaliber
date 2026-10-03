#pragma once
#include "app/record_model.h"
#include <QJsonObject>
#include <QMainWindow>
#include <memory>
class QTabWidget;
class QLabel;
namespace lordly {
class MainWindow : public QMainWindow {
    Q_OBJECT
  public:
    explicit MainWindow(QWidget *parent = nullptr);
    void loadRom(const QString &path);

  protected:
    void closeEvent(QCloseEvent *event) override;

  private:
    void installProject(Project project);
    void addRecordWorkspace(Table table, const QString &title);
    bool confirmDiscard();
    void openProject();
    void saveProject(bool archive);
    void buildRom();
    void updateStatus();
    void report(const QString &text);
    QJsonObject catalog_;
    std::unique_ptr<Project> project_;
    QUndoStack *undo_;
    QTabWidget *workspaces_;
    QLabel *summary_;
    bool loading_ = false;
};
} // namespace lordly
