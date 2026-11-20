#include "app/code_patch_dialog.h"
#include "engine/project.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QUndoStack>
#include <QVBoxLayout>
namespace lordly {
namespace {
class PatchSelection final : public QUndoCommand {
  public:
    PatchSelection(Project &project, QJsonArray next)
        : QUndoCommand("Change enabled code patches"), project_(project),
          before_(project.codePatches()), after_(std::move(next)) {}
    void redo() override {
        project_.setCodePatches(after_);
    }
    void undo() override {
        project_.setCodePatches(before_);
    }

  private:
    Project &project_;
    QJsonArray before_, after_;
};
} // namespace
void showCodePatches(QWidget *parent, Project &project, QUndoStack &undo) {
    QDialog dialog(parent);
    dialog.setWindowTitle("Project code patches");
    dialog.resize(760, 540);
    auto layout = new QVBoxLayout(&dialog);
    auto patches = project.codePatches();
    auto list = new QListWidget;
    list->setObjectName("projectCodePatches");
    auto source = new QPlainTextEdit;
    source->setReadOnly(true);
    source->setObjectName("projectPatchSource");
    auto target = new QLabel;
    target->setTextFormat(Qt::PlainText);
    layout->addWidget(new QLabel(patches.isEmpty()
                                     ? "This project has no custom code patches."
                                     : "Checked patches are included when you build the ROM."));
    layout->addWidget(list);
    layout->addWidget(target);
    layout->addWidget(source, 2);
    for (const auto &v : patches) {
        const auto patch = v.toObject();
        auto item = new QListWidgetItem(patch["id"].toString(), list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(patch["enabled"].toBool(true) ? Qt::Checked : Qt::Unchecked);
    }
    QObject::connect(list, &QListWidget::currentRowChanged, &dialog, [&](int row) {
        if (row < 0)
            return;
        auto patch = patches.at(row).toObject();
        source->setPlainText(patch["source"].toString());
        target->setText(patch.contains("module") ? "Build module " + patch["module"].toString()
                                                 : QString("%1 · ROM 0x%2 · RAM 0x%3")
                                                       .arg(patch["language"].toString())
                                                       .arg(patch["offset"].toInteger(), 0, 16)
                                                       .arg(patch["address"].toInteger(), 0, 16));
    });
    if (list->count())
        list->setCurrentRow(0);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted)
        return;
    for (int i = 0; i < list->count(); ++i) {
        auto patch = patches[i].toObject();
        const bool enabled = list->item(i)->checkState() == Qt::Checked;
        if (patch["enabled"].toBool(true) != enabled) {
            patch["enabled"] = enabled;
            patches[i] = patch;
        }
    }
    if (patches != project.codePatches())
        undo.push(new PatchSelection(project, std::move(patches)));
}
} // namespace lordly
