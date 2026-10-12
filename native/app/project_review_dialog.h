#pragma once
#include "engine/project_references.h"
#include <QDialog>
namespace lordly {
// An empty query reviews an export; otherwise review uses of the selected entity.
class ProjectReviewDialog : public QDialog {
    Q_OBJECT
  public:
    ProjectReviewDialog(Project snapshot, const EditLocation &query = {}, QWidget *parent = nullptr);
  signals:
    void editRequested(const lordly::EditLocation &);
};
} // namespace lordly
