#include "app/scene_action_editor.h"
#include "app/scene_command_editor.h"
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
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
    auto add = new QHBoxLayout;
    kind_ = new QComboBox;
    kind_->setObjectName("sceneActionKind");
    const auto kinds = SceneActions::kinds();
    for (auto [kind, label] : kinds.asKeyValueRange())
        kind_->addItem(label, kind);
    kind_->setCurrentIndex(kind_->findData(7));
    add->addWidget(kind_);
    position_ = new QComboBox;
    position_->setObjectName("sceneActionPosition");
    position_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    position_->setMinimumContentsLength(20);
    add->addWidget(position_, 1);
    insert_ = new QPushButton("Add action");
    insert_->setObjectName("sceneActionInsert");
    add->addWidget(insert_);
    remove_ = new QPushButton("Remove action");
    remove_->setObjectName("sceneActionRemove");
    add->addWidget(remove_);
    layout->addLayout(add);
    notice_ = new QLabel("Select an action to edit.");
    notice_->setObjectName("sceneActionStatus");
    notice_->setWordWrap(true);
    layout->addWidget(notice_);
    text_ = new QPushButton("Edit dialogue text");
    text_->setObjectName("sceneActionEditText");
    text_->setVisible(false);
    layout->addWidget(text_);
    connect(text_, &QPushButton::clicked, this, [this] {
        const int row = list_->currentRow();
        if (!key_ || row < 0 || row >= actions_.size() || property("invalidEdits").toBool())
            return;
        const auto program = project_->scenePrograms()->program(key_);
        const auto &c = program.commands[actions_[row].command];
        if (c.opcode == 0xbf)
            emit editDialogue(quint16(c.operands[1]), quint16(c.operands[2]));
    });
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
    connect(kind_, &QComboBox::currentIndexChanged, this, [this] { updateStructureControls(); });
    connect(position_, &QComboBox::currentIndexChanged, this,
            [this] { updateStructureControls(); });
    connect(insert_, &QPushButton::clicked, this, [this] { changeStructure(false); });
    connect(remove_, &QPushButton::clicked, this, [this] { changeStructure(true); });
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
    updateStructureControls();
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
    const QSignalBlocker positionBlock(position_);
    const auto priorPosition = position_->currentData().toInt();
    position_->clear();
    if (key_) {
        const auto program = project_->scenePrograms()->program(key_);
        actions_ = SceneActions::inspect(program);
        for (const auto &point : SceneActions::positions(program)) {
            position_->addItem(point.label, point.command);
            position_->setItemData(position_->count() - 1, point.canWait, Qt::UserRole + 1);
            position_->setItemData(position_->count() - 1, point.waitReason, Qt::ToolTipRole);
        }
        int insertion = position_->findData(command);
        if (insertion < 0)
            insertion = position_->findData(priorPosition);
        position_->setCurrentIndex(insertion < 0 ? position_->count() - 1 : insertion);
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
    if (row >= 0 && row < actions_.size()) {
        const int position = position_->findData(actions_[row].command);
        if (position >= 0)
            position_->setCurrentIndex(position);
    }
    updateStructureControls();
    text_->setVisible(
        key_ && row >= 0 && row < actions_.size() &&
        project_->scenePrograms()->program(key_).commands[actions_[row].command].opcode == 0xbf);
    if (!key_ || row < 0 || row >= actions_.size()) {
        notice_->setText(
            actions_.isEmpty()
                ? "This scene has no supported action fields. Its source commands remain available."
                : "Select a movement, placement, camera, fade, scale, opacity, dialogue or "
                  "recognized wait "
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
void SceneActionEditor::updateStructureControls() {
    const bool valid = key_ && !property("invalidEdits").toBool();
    kind_->setEnabled(valid);
    position_->setEnabled(valid);
    text_->setEnabled(valid);
    const bool wait = kind_->currentData().toUInt() == 0xe ||
                      kind_->currentData().toUInt() == 0xb5 ||
                      kind_->currentData().toUInt() == 0xbf;
    const bool allowed = !wait || position_->currentData(Qt::UserRole + 1).toBool();
    insert_->setEnabled(valid && position_->currentIndex() >= 0 && allowed);
    insert_->setToolTip(allowed ? "Insert here, then edit the action's fields."
                                : position_->currentData(Qt::ToolTipRole).toString());
    bool canRemove = false;
    QString reason;
    const int row = list_->currentRow();
    if (valid && row >= 0 && row < actions_.size()) {
        try {
            SceneActions::remove(project_->scenePrograms()->program(key_), actions_[row].command);
            canRemove = true;
        } catch (const std::exception &e) {
            reason = QString::fromUtf8(e.what());
        }
    }
    remove_->setEnabled(canRemove);
    remove_->setToolTip(reason);
}
void SceneActionEditor::changeStructure(bool remove) {
    if (!key_ || property("invalidEdits").toBool())
        return;
    try {
        const auto program = project_->scenePrograms()->program(key_);
        const int row = list_->currentRow();
        if (remove && (row < 0 || row >= actions_.size()))
            return;
        const int command = remove ? actions_[row].command : position_->currentData().toInt();
        auto next = *project_->scenePrograms();
        next.replace(
            key_, (remove ? SceneActions::remove(program, command)
                          : SceneActions::insert(program, command, kind_->currentData().toUInt()))
                      .source);
        editing_ = true;
        pushSceneProgramChange(project_, undo_, remove ? "Remove scene action" : "Add scene action",
                               next);
        editing_ = false;
        refresh(command);
        emit commandSelected(command);
        if (changed_)
            changed_();
    } catch (const std::exception &e) {
        editing_ = false;
        notice_->setText(QString::fromUtf8(e.what()));
    }
}
} // namespace lordly
