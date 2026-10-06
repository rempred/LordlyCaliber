#include "app/scene_action_editor.h"
#include "app/scene_command_editor.h"
#include "engine/actor_art.h"
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
    move_ = new QPushButton("Move action");
    move_->setObjectName("sceneActionMove");
    add->addWidget(move_);
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
    connect(insert_, &QPushButton::clicked, this, [this] { changeStructure(Change::Add); });
    connect(remove_, &QPushButton::clicked, this, [this] { changeStructure(Change::Remove); });
    connect(move_, &QPushButton::clicked, this, [this] { changeStructure(Change::Move); });
    connect(discard_, &QPushButton::clicked, this, [this] {
        editing_ = true;
        validity(true);
        showAction();
        editing_ = false;
        if (changed_)
            changed_();
    });
    connect(undo_, &QUndoStack::indexChanged, this, [this] { refreshClassChoices(); });
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
bool SceneActionEditor::selectActor(int slot) {
    if (editing_ || property("invalidEdits").toBool() || !key_ || slot < 0 || slot > 27)
        return false;
    const auto program = project_->scenePrograms()->program(key_);
    int selected = -1;
    for (int row = 0; row < actions_.size(); ++row) {
        const auto &action = actions_[row];
        if (action.actorOperand < 0)
            continue;
        const auto &command = program.commands[action.command];
        if (qint32(command.operands[action.actorOperand]) != slot)
            continue;
        if (selected < 0)
            selected = row;
        if (command.opcode == 0x14) {
            selected = row;
            break;
        }
    }
    if (selected < 0)
        return false;
    const QSignalBlocker guard(list_);
    list_->setCurrentRow(selected);
    showAction();
    emit commandSelected(actions_[selected].command);
    return true;
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
        if (!f.choices.isEmpty() && f.canonical(word)) {
            QStringList labels;
            for (auto choice : f.choices)
                labels << QString::number(choice);
            input->setToolTip("Choose " + labels.join(" or ") +
                              ". Change the sign to turn the other way.");
        }
        if (action.name == "Set Actor pose and position" && i > 0)
            input->setToolTip(i >= 4 && i <= 6 ? "X and Y both -1 keep all coordinates. Otherwise "
                                                 "X, Y and Z replace the position together."
                                               : "Use -1 to keep the Actor's current value.");
        if (action.name == "Set Actor class and pose" || action.name == "Create class Actor") {
            if ((word & 0xffffff00U) == 0x08880000U) {
                input->setText(QString("Launch value %1").arg(word & 255));
                input->setToolTip(
                    "The launch event supplies this value. Its symbolic source remains unchanged.");
            } else if (f.canonical(word) && action.name == "Set Actor class and pose") {
                input->setToolTip(
                    "World scenes apply this to an existing Actor. -1 retains the current "
                    "value (or skips the command for the slot). Positions are retained; "
                    "these fields preserve the command's body flags.");
            }
        }
        if (action.name == "Room Actor channel")
            input->setToolTip("Room scenes use drawing channels for transforms and layer order. An "
                              "active automatic divider can reassign the channel.");
        if (action.name == "Turn Actor" && i == 4)
            input->setToolTip("The game rounds this budget to its turning steps. Start and target "
                              "facings must differ; very short budgets can wrap the native timer.");
        const bool classField = (action.name == "Set Actor class and pose" ||
                                 action.name == "Create class Actor") && (i == 1 || i == 2);
        if (classField && !input->isReadOnly()) {
            auto classRow = new QWidget;
            auto layout = new QHBoxLayout(classRow);
            layout->setContentsMargins(0, 0, 0, 0);
            input->setMaximumWidth(100);
            layout->addWidget(input);
            auto choice = new QComboBox;
            choice->setObjectName(QString("sceneActionClassChoice%1").arg(i));
            choice->setProperty("actionField", i);
            choice->setProperty("minimumClass", f.minimum);
            choice->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
            choice->setMinimumContentsLength(18);
            choice->setToolTip("Choose a class from the current project. The adjacent field keeps its numeric ID.");
            layout->addWidget(choice, 1);
            fields_->addRow(f.name, classRow);
            connect(choice, &QComboBox::activated, input, [choice, input] {
                if (choice->currentIndex() >= 0)
                    input->setText(QString::number(choice->currentData().toInt()));
            });
        } else
            fields_->addRow(f.name, input);
        connect(input, &QLineEdit::textChanged, this, [this] { publishFields(); });
        if (classField)
            connect(input, &QLineEdit::textChanged, this, [this] { refreshClassChoices(); });
    }
    refreshClassChoices();
    notice_->setText("Complete values enter the project immediately and share Undo with Commands.");
}
void SceneActionEditor::refreshClassChoices() {
    const auto &classes = project_->records(Table::Classes);
    for (auto choice : findChildren<QComboBox *>()) {
        if (!choice->objectName().startsWith("sceneActionClassChoice"))
            continue;
        const auto input = findChild<QLineEdit *>(QString("sceneActionField%1").arg(choice->property("actionField").toInt()));
        if (!input)
            continue;
        QStringList labels;
        QVector<int> ids;
        const int minimum = choice->property("minimumClass").toInt();
        if (minimum < 0) {
            labels << "Keep current class";
            ids << -1;
        }
        for (int i = qMax(0, minimum); i < classes.size(); ++i) {
            labels << QString("%1 · %2").arg(i).arg(classes[i].name);
            ids << i;
        }
        bool changed = choice->count() != labels.size();
        for (int i = 0; !changed && i < labels.size(); ++i)
            changed = choice->itemText(i) != labels[i] || choice->itemData(i).toInt() != ids[i];
        const QSignalBlocker block(choice);
        if (changed) {
            choice->clear();
            for (int i = 0; i < labels.size(); ++i)
                choice->addItem(labels[i], ids[i]);
        }
        bool valid = false;
        const auto text = input->text().trimmed();
        const int value = text.toInt(&valid, text.startsWith("0x", Qt::CaseInsensitive) ? 16 : 10);
        const int index = valid ? choice->findData(value) : -1;
        if (choice->currentIndex() != index)
            choice->setCurrentIndex(index);
    }
}
void SceneActionEditor::publishFields() {
    const int row = list_->currentRow();
    if (editing_ || !key_ || row < 0 || row >= actions_.size())
        return;
    try {
        const auto action = actions_[row];
        auto program = project_->scenePrograms()->program(key_);
        const auto original = program;
        for (auto input : findChildren<QLineEdit *>()) {
            if (!input->objectName().startsWith("sceneActionField") || input->isReadOnly())
                continue;
            program = SceneActions::edit(program, action.command,
                                         input->property("actionField").toInt(), input->text());
        }
        if (action.name == "Create class Actor") {
            const auto &body = program.commands[action.command + 1].operands;
            for (int operand : {1, 5})
                if (body[operand] != original.commands[action.command + 1].operands[operand] &&
                    body[operand] >= quint32(project_->records(Table::Classes).size()))
                    throw Error("Choose a class that exists in the current project.");
        }
        if (action.name == "Create scene sprite" || action.name == "Set scene sprite pose") {
            const auto &args = program.commands[action.command].operands;
            const auto &prior = original.commands[action.command].operands;
            const bool create = action.name == "Create scene sprite";
            const bool changedPose =
                args[1] != prior[1] || args[2] != prior[2] || (create && args[3] != prior[3]);
            if (changedPose && qint32(args[1]) >= 0) {
                const auto art = ActorArt::load(project_->rom(), int(args[1]));
                if (qint32(args[2]) >= 0 &&
                    art.resolve(qint16(args[2]), create ? int(args[3]) : 0) < 0)
                    throw Error("Choose an animation and facing present in this art bank.");
            }
            if (create && (args[4] != prior[4] || args[5] != prior[5]) && args[4] == 0xffffffffU &&
                args[5] == 0xffffffffU)
                throw Error("A new sprite cannot inherit both starting coordinates; set X or Y.");
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
    const bool body = kind_->currentData().toUInt() == 0x2a;
    const int type = valid ? project_->scenePrograms()->program(key_).resourceClass() : -1;
    const bool allowed = (!wait || position_->currentData(Qt::UserRole + 1).toBool()) &&
                         (!body || type == 1 || type == 8);
    insert_->setEnabled(valid && position_->currentIndex() >= 0 && allowed);
    insert_->setToolTip(allowed ? "Insert here, then edit the action's fields."
                        : body ? "Create class Actor requires a fresh environment scene."
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
    bool canMove = false;
    reason.clear();
    if (valid && row >= 0 && row < actions_.size() && position_->currentIndex() >= 0) {
        try {
            const auto source = project_->scenePrograms()->program(key_);
            canMove =
                SceneActions::move(source, actions_[row].command, position_->currentData().toInt())
                    .source != source.source;
        } catch (const std::exception &e) {
            reason = QString::fromUtf8(e.what());
        }
    }
    move_->setEnabled(canMove);
    move_->setToolTip(reason.isEmpty() ? "Move the selected action before the chosen destination."
                                       : reason);
}
void SceneActionEditor::changeStructure(Change operation) {
    if (!key_ || property("invalidEdits").toBool())
        return;
    try {
        const auto program = project_->scenePrograms()->program(key_);
        const int row = list_->currentRow();
        if (operation != Change::Add && (row < 0 || row >= actions_.size()))
            return;
        const int destination = position_->currentData().toInt();
        int command = operation == Change::Add ? destination : actions_[row].command;
        DirectorProgram replacement;
        QString label;
        if (operation == Change::Add) {
            replacement = SceneActions::insert(program, destination, kind_->currentData().toUInt());
            label = "Add scene action";
        } else if (operation == Change::Remove) {
            replacement = SceneActions::remove(program, command);
            label = "Remove scene action";
        } else {
            replacement = SceneActions::move(program, command, destination);
            if (replacement.source == program.source)
                return;
            command =
                destination > command ? destination - actions_[row].commandCount : destination;
            label = "Move scene action";
        }
        auto next = *project_->scenePrograms();
        next.replace(key_, replacement.source);
        editing_ = true;
        pushSceneProgramChange(project_, undo_, label, next);
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
