#include "app/cutscene_editor.h"
#include "app/scene_command_editor.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QVBoxLayout>
namespace lordly {
QWidget *CutsceneEditor::createBackgroundControls() {
    auto panel = new QWidget;
    auto layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    auto row = new QHBoxLayout;
    row->addWidget(new QLabel("Scene background"));
    backgroundChoice_ = new QComboBox;
    backgroundChoice_->setObjectName("cutsceneBackgroundChoice");
    backgroundChoice_->setMinimumContentsLength(20);
    backgroundChoice_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    row->addWidget(backgroundChoice_, 1);
    backgroundApply_ = new QPushButton("Apply background");
    backgroundApply_->setObjectName("cutsceneBackgroundApply");
    backgroundRestore_ = new QPushButton("Restore original");
    backgroundRestore_->setObjectName("cutsceneBackgroundRestore");
    row->addWidget(backgroundApply_);
    row->addWidget(backgroundRestore_);
    layout->addLayout(row);
    backgroundHelp_ = new QLabel;
    backgroundHelp_->setObjectName("cutsceneBackgroundHelp");
    backgroundHelp_->setWordWrap(true);
    layout->addWidget(backgroundHelp_);
    backgroundChoice_->setEnabled(false);
    backgroundApply_->setEnabled(false);
    backgroundRestore_->setEnabled(false);
    connect(backgroundChoice_, &QComboBox::currentIndexChanged, this,
            &CutsceneEditor::previewBackgroundChoice);
    connect(backgroundApply_, &QPushButton::clicked, this,
            [this] { applyBackgroundChoice(false); });
    connect(backgroundRestore_, &QPushButton::clicked, this,
            [this] { applyBackgroundChoice(true); });
    panel->setVisible(project_ != nullptr);
    return panel;
}
void CutsceneEditor::refreshBackgroundEdit() {
    const QSignalBlocker blocker(backgroundChoice_);
    backgroundChoice_->clear();
    backgroundChoice_->setEnabled(false);
    backgroundApply_->setEnabled(false);
    backgroundRestore_->setEnabled(false);
    if (!project_ || !selectedScene() || catalogTabs_->currentIndex() != 0)
        return;
    try {
        const auto key = selectedScene();
        backgroundEdit_ = BackgroundEdit::inspect(rom_, programs().program(key));
        if (!backgroundEdit_.editable()) {
            backgroundHelp_->setText(backgroundEdit_.reason);
            return;
        }
        const int route = int(backgroundEdit_.route);
        if (!backgroundOptions_.contains(route))
            backgroundOptions_[route] = backgroundEdit_.choices(rom_);
        for (const auto &option : backgroundOptions_[route])
            backgroundChoice_->addItem(option.name, option.selector);
        int selected = backgroundChoice_->findData(backgroundEdit_.selector);
        if (selected < 0) {
            backgroundChoice_->insertItem(
                0,
                QString("Current selection %1 — preview unavailable").arg(backgroundEdit_.selector),
                -1);
            selected = 0;
        }
        backgroundChoice_->setCurrentIndex(selected);
        const bool valid = !invalidSceneInput();
        backgroundChoice_->setEnabled(valid && !backgroundOptions_[route].isEmpty());
        const auto original = BackgroundEdit::inspect(rom_, programs().resource(key).program);
        backgroundRestore_->setEnabled(valid && original.editable() &&
                                       original.route == backgroundEdit_.route &&
                                       original.selector != backgroundEdit_.selector);
        previewBackgroundChoice();
    } catch (const std::exception &e) {
        backgroundHelp_->setText(QString::fromUtf8(e.what()));
    }
}
void CutsceneEditor::previewBackgroundChoice() {
    if (!project_ || !backgroundEdit_.editable() || backgroundChoice_->currentIndex() < 0)
        return;
    const int selector = backgroundChoice_->currentData().toInt();
    const bool pending = selector >= 0 && selector != backgroundEdit_.selector;
    backgroundApply_->setEnabled(pending && !invalidSceneInput());
    backgroundHelp_->setText(
        pending
            ? "Previewing this choice. Apply to change the scene; Actor positions stay as authored."
            : "Current scene background. Choose another to preview before applying; Actor "
              "positions stay as authored.");
    const QSignalBlocker blocker(images_);
    images_->clear();
    for (const auto &option : backgroundOptions_.value(int(backgroundEdit_.route)))
        if (option.selector == selector)
            for (const auto &id : option.images)
                images_->addItem(SceneImages::catalog()[id].name + " · " + id, id);
    selectImage(images_->currentData().toString());
}
void CutsceneEditor::applyBackgroundChoice(bool restore) {
    if (!project_ || invalidSceneInput())
        return;
    try {
        const auto key = selectedScene();
        auto next = *project_->scenePrograms();
        auto program = next.program(key);
        const auto current = BackgroundEdit::inspect(rom_, program);
        if (!current.editable())
            throw Error(current.reason);
        if (restore) {
            const auto original = BackgroundEdit::inspect(rom_, next.resource(key).program);
            if (!original.editable() || original.route != current.route)
                throw Error("The original scene uses a different background route");
            program.commands[current.command].operands[0] =
                next.resource(key).program.commands[original.command].operands[0];
            if (SceneImages::launch(DirectorProgram::decode(program.encode())).environment !=
                original.selector)
                throw Error("Another launch marker overrides the original background");
        } else
            program = current.select(rom_, program, backgroundChoice_->currentData().toInt());
        next.replace(key, program.encode());
        pushSceneProgramChange(project_, undo_,
                               restore ? "Restore scene background" : "Change scene background",
                               next);
    } catch (const std::exception &e) {
        QMessageBox::warning(this, "Scene background", QString::fromUtf8(e.what()));
    }
}
} // namespace lordly
