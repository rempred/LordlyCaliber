#include "app/scene_action_editor.h"
#include "app/scene_command_editor.h"
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QUndoStack>
#include <QVBoxLayout>
namespace lordly {
SceneActionEditor::SceneActionEditor(Project *project, QUndoStack *undo,
                                     std::function<void()> changed, QWidget *parent)
    : QWidget(parent), project_(project), undo_(undo), changed_(std::move(changed)) {
    setObjectName("sceneActionEditor");
    auto layout = new QVBoxLayout(this);
    auto help =
        new QLabel("Actions are listed in script order. Conditions can change when they run. "
                   "Positions use game units; durations use update counts. Other commands remain "
                   "in the Commands tab.");
    help->setWordWrap(true);
    layout->addWidget(help);
    auto split = new QSplitter;
    list_ = new QListWidget;
    list_->setObjectName("sceneActionList");
    split->addWidget(list_);
    auto area = new QScrollArea;
    area->setWidgetResizable(true);
    auto form = new QWidget;
    fields_ = new QFormLayout(form);
    area->setWidget(form);
    split->addWidget(area);
    split->setSizes({350, 500});
    layout->addWidget(split, 1);
    notice_ = new QLabel("Select an action to edit.");
    notice_->setObjectName("sceneActionStatus");
    notice_->setWordWrap(true);
    layout->addWidget(notice_);
    discard_ = new QPushButton("Discard incomplete input");
    discard_->setObjectName("sceneActionDiscard");
    discard_->setEnabled(false);
    layout->addWidget(discard_);
    connect(list_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (editing_ || property("invalidEdits").toBool())
            return;
        showAction();
        if (row >= 0 && row < actions_.size())
            emit commandSelected(actions_[row].command);
    });
    connect(discard_, &QPushButton::clicked, this, [this] {
        editing_ = true;
        validity(true);
        showAction();
        editing_ = false;
        if (changed_)
            changed_();
    });
}
void SceneActionEditor::validity(bool valid) {
    const bool wasInvalid = property("invalidEdits").toBool();
    setProperty("invalidEdits", !valid);
    list_->setEnabled(valid);
    discard_->setEnabled(!valid);
    if (wasInvalid != !valid)
        emit validityChanged();
}
void SceneActionEditor::selectScene(SceneId key, int command) {
    if (editing_ || property("invalidEdits").toBool())
        return;
    key_ = key;
    refresh(command);
}
void SceneActionEditor::selectCommand(int command) {
    if (editing_ || property("invalidEdits").toBool())
        return;
    const QSignalBlocker block(list_);
    for (int row = 0; row < actions_.size(); ++row)
        if (command >= actions_[row].command &&
            command < actions_[row].command + actions_[row].commandCount) {
            if (list_->currentRow() != row) {
                list_->setCurrentRow(row);
                showAction();
            }
            return;
        }
    list_->setCurrentRow(-1);
    showAction();
}
void SceneActionEditor::refresh(int command) {
    const QSignalBlocker block(list_);
    list_->clear();
    actions_.clear();
    if (key_) {
        const auto program = project_->scenePrograms()->program(key_);
        actions_ = SceneActions::inspect(program);
        for (const auto &action : actions_) {
            auto item = new QListWidgetItem(
                QString("%1 · word %2").arg(action.name).arg(program.commands[action.command].word),
                list_);
            item->setData(Qt::UserRole, action.command);
        }
        for (int row = 0; row < actions_.size(); ++row)
            if (command >= actions_[row].command &&
                command < actions_[row].command + actions_[row].commandCount) {
                list_->setCurrentRow(row);
                break;
            }
    }
    showAction();
}
void SceneActionEditor::showAction() {
    while (fields_->rowCount())
        fields_->removeRow(0);
    const int row = list_->currentRow();
    if (!key_ || row < 0 || row >= actions_.size()) {
        notice_->setText(
            actions_.isEmpty()
                ? "This scene has no supported action fields. Its source commands remain available."
                : "Select a movement, placement, camera, fade, scale, opacity or recognized wait "
                  "action.");
        return;
    }
    const auto &action = actions_[row];
    const auto program = project_->scenePrograms()->program(key_);
    for (int i = 0; i < action.fields.size(); ++i) {
        const auto &f = action.fields[i];
        const auto word = program.commands[action.command + f.commandOffset].operands[f.operand];
        auto input = new QLineEdit(f.text(word));
        input->setObjectName(QString("sceneActionField%1").arg(i));
        input->setAccessibleName(f.name);
        input->setProperty("actionField", i);
        input->setReadOnly(!f.canonical(word));
        input->setToolTip(
            !f.canonical(word) ? "This value uses a native encoding; edit it in Commands."
            : f.scale == 1000
                ? "Game units with up to three decimal places."
                : QString("Whole numbers from %1 to %2.").arg(f.minimum).arg(f.maximum));
        if (action.name == "Move Actor" && f.canonical(word)) {
            if (i == 1 || i == 2)
                input->setToolTip(
                    "Set both start coordinates to -1 to use the Actor's current position.");
            if (i == 5 || i == 6)
                input->setToolTip("Duration 1 moves immediately. Otherwise, a nonzero speed "
                                  "determines the travel time; speed 0 uses the duration.");
        }
        fields_->addRow(f.name, input);
        connect(input, &QLineEdit::textChanged, this, [this] { publishFields(); });
    }
    notice_->setText("Complete values enter the project immediately and share Undo with Commands.");
}
void SceneActionEditor::publishFields() {
    const int row = list_->currentRow();
    if (editing_ || !key_ || row < 0 || row >= actions_.size())
        return;
    try {
        const auto action = actions_[row];
        auto program = project_->scenePrograms()->program(key_);
        for (auto input : findChildren<QLineEdit *>()) {
            if (!input->objectName().startsWith("sceneActionField") || input->isReadOnly())
                continue;
            program = SceneActions::edit(program, action.command,
                                         input->property("actionField").toInt(), input->text());
        }
        auto next = *project_->scenePrograms();
        next.replace(key_, program.source);
        editing_ = true;
        validity(true);
        pushSceneProgramChange(project_, undo_, "Edit " + action.name.toLower(), next,
                               QString("action/%1/%2").arg(key_.label()).arg(action.command));
        editing_ = false;
        notice_->setText("Action updated in the project.");
    } catch (const std::exception &e) {
        editing_ = false;
        validity(false);
        notice_->setText(
            QString::fromUtf8(e.what()) +
            " Complete or discard this input before changing scenes, saving or exporting.");
    }
    if (changed_)
        changed_();
}
} // namespace lordly
