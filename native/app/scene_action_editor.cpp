#include "app/scene_action_editor.h"
#include "app/scene_command_editor.h"
#include "engine/actor_art.h"
#include "engine/scene_ribbons.h"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QUndoStack>
#include <QVBoxLayout>
namespace lordly {
namespace {
class SceneTextCommand final : public QUndoCommand {
  public:
    SceneTextCommand(Project *project, const QString &label, const ScenePrograms &scenes,
                     const Dialogues &dialogues)
        : QUndoCommand(label), project_(project), beforeScenes_(project->scenePrograms()->json()),
          beforeText_(project->dialogues()->json()), afterScenes_(scenes.json()),
          afterText_(dialogues.json()) {}
    void undo() override {
        apply(beforeScenes_, beforeText_);
    }
    void redo() override {
        apply(afterScenes_, afterText_);
    }

  private:
    void apply(const QJsonObject &scenes, const QJsonObject &text) {
        project_->setDialogues(text);
        project_->setScenePrograms(scenes);
    }
    Project *project_;
    QJsonObject beforeScenes_, beforeText_, afterScenes_, afterText_;
};
} // namespace
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
            changeDialogue(false, actions_[row].name == "Choice");
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
bool SceneActionEditor::selectSprite(int slot) {
    if (editing_ || property("invalidEdits").toBool() || !key_ || slot < 0 || slot >= 30)
        return false;
    const auto program = project_->scenePrograms()->program(key_);
    int selected = -1;
    for (int row = 0; row < actions_.size(); ++row) {
        const auto &command = program.commands[actions_[row].command];
        if (command.opcode != 0x46 && command.opcode != 0x63 && command.opcode != 0x64 &&
            command.opcode != 0x66)
            continue;
        const auto value = command.operands[0];
        if ((value & 0xffffff00U) == 0x08880000U)
            continue;
        if ((command.opcode == 0x66 ? qint32(value) : int(quint8(value))) != slot)
            continue;
        if (selected < 0)
            selected = row;
        if (command.opcode == 0x46 ||
            (command.opcode == 0x63 && command.operands[9] == quint32(-1))) {
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
    for (int row = int(actions_.size()) - 1; row >= 0; --row)
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
        const auto choices = SceneActions::choices(program);
        for (const auto &action : actions_) {
            QString branch;
            for (const auto &choice : choices)
                if (choice.option(action.command))
                    branch = QString("Option %1 · ").arg(choice.option(action.command));
            auto item =
                new QListWidgetItem(branch + QString("%1 · word %2")
                                                 .arg(action.name)
                                                 .arg(program.commands[action.command].word),
                                    list_);
            item->setData(Qt::UserRole, action.command);
        }
        for (int row = int(actions_.size()) - 1; row >= 0; --row)
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
    text_->setText(action.name == "Choice" ? "Edit choice options" : "Edit dialogue text");
    remove_->setText(action.name == "Choice" ? "Remove choice and its branches" : "Remove action");
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
        if ((word & 0xffffff00U) == 0x08880000U) {
            input->setText(QString("Launch value %1").arg(word & 255));
            input->setToolTip(
                "The launch event supplies this value. Its symbolic source remains unchanged.");
        } else if (action.name == "Set Actor class and pose" ||
                   action.name == "Create class Actor") {
            if (f.canonical(word) && action.name == "Set Actor class and pose") {
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
        if ((action.name == "Create rotated scene sprite" ||
             action.name == "Rotate scene sprite") &&
            f.operand == 6 && f.canonical(word))
            input->setToolTip("Native rotation: 180 is unrotated; 270 turns a quarter turn. "
                              "Reusing a sprite retains its position, pose, scale and channel.");
        if ((action.name == "Create sprite with path heading" ||
             action.name == "Set sprite heading from path") &&
            f.operand >= 7 && f.canonical(word))
            input->setToolTip("A nonnegative group selects a resource path in the current ROM. "
                              "Group -1 selects a currently active path slot, from 0 through 19. "
                              "The sprite copies that path's starting heading.");
        if (action.name == "Set path ribbon endpoint" && i > 0 && f.canonical(word))
            input->setToolTip("A point other than -1 writes only the stored milestone. Otherwise "
                              "the percentage is updated. This never clears an earlier milestone, "
                              "rewinds revealed geometry or restarts the activity flag.");
        if (action.name == "Construct scene image" && f.canonical(word)) {
            if (i == 0)
                input->setToolTip("Image drawing channel 1 through 19. Channel 0 uses a separate "
                                  "game drawing owner and remains available in Commands.");
            else if (i == 3 || i == 4)
                input->setToolTip(
                    "Signed percentage. Negative values mirror this axis; zero "
                    "collapses it. The transform must fit native fixed-point limits.");
            else if (i == 5)
                input->setToolTip("Maximum alpha for the half-resolution vignette. The original "
                                  "image pixels remain intact.");
            else if (i == 6)
                input->setToolTip(
                    "0 applies the native -90-degree Y / +5-degree Z preset; "
                    "8 uses 0-degree Y / -5-degree Z. Other encoded flag words are preserved.");
        }
        if (action.name == "Image transition" && f.canonical(word)) {
            if (i == 0)
                input->setToolTip(
                    "Forward moves from the constructed image transform to the settled "
                    "view. Reverse captures the current pan/zoom and returns to the "
                    "constructed transform.");
            else if (i == 1)
                input->setToolTip("1 through 255 native updates before the echo drain phase.");
            else
                input->setToolTip("0 disables echo sampling. Otherwise the three-slot echo buffer "
                                  "drains against a tail budget of this value times five. Native "
                                  "drain updates can advance the counter twice.");
        }
        if (action.name == "Pan and zoom image" && f.canonical(word)) {
            if (i < 2)
                input->setToolTip("Set both start coordinates to -1 to use the current position. "
                                  "These values calculate movement rates; they do not reposition "
                                  "the image immediately.");
            else if (i == 4)
                input->setToolTip(
                    "After the initial image transition completes, normal requests scale 1.0 "
                    "and reduced requests 0.7. An in-progress zoom retains the game's reversal "
                    "timing.");
            else if (i == 5)
                input->setToolTip("Positive update count. Movement ends after this many updates; "
                                  "the job releases on the following update.");
        }
        const bool classField =
            (action.name == "Set Actor class and pose" || action.name == "Create class Actor") &&
            (i == 1 || i == 2);
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
            choice->setToolTip("Choose a class from the current project. The adjacent field keeps "
                               "its numeric ID.");
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
    notice_->setText(
        action.name == "Wait for image transition"
            ? "Continues only when the image transition and its echo tail have both finished. "
              "Uses the native completion query and leaves the timed-wait counter unchanged."
        : action.name == "Construct scene image"
            ? "Builds the vignette and starting transform from the launch image once. Keep this "
              "before Image transition. Removing it may leave later transitions without an image."
        : action.name == "Image transition"
            ? "Runs on the scene's constructed image. Echoes drain after the duration; this action "
              "does not wait for them. Add Wait for image transition before later pan/zoom."
        : action.name == "Pan and zoom image"
            ? "Pans the scene's image and requests its zoom state. Start -1/-1 uses the current "
              "position; differences of one unit or less do not pan. Zoom becomes available after "
              "the initial image transition. Add a Wait to delay later actions."
        : action.name == "Set path ribbon endpoint"
            ? "Changes the reveal limit. A stored point takes precedence over later percentage "
              "changes; this action does not rewind or restart the ribbon."
        : action.name == "Create path ribbon"
            ? "Creates or replaces one path in the scene's image presentation. A point overrides "
              "the percentage; point -1 and percentage -1 reveal the full path."
        : action.name == "Release path ribbon"
            ? "Fade releases the path after its native fade updates. Immediate release removes it "
              "at this command. Slot -1 applies to all paths."
        : action.name == "Create sprite with path heading" ||
                action.name == "Set sprite heading from path"
            ? "Copies the path's starting heading. Group -1 needs an active path when this action "
              "runs. "
              "Creation uses facing 0, channel 1 and scale 100%; reuse retains the sprite's other "
              "state."
        : action.name == "Create rotated scene sprite"
            ? "Creates with facing 0, drawing channel 1 and scale 100%. "
              "Complete values enter the project immediately and share Undo with Commands."
        : action.name == "Rotate scene sprite"
            ? "Reuses an existing sprite and changes only its rotation. "
              "Complete values enter the project immediately and share Undo with Commands."
            : "Complete values enter the project immediately and share Undo with Commands.");
}
void SceneActionEditor::refreshClassChoices() {
    const auto &classes = project_->records(Table::Classes);
    for (auto choice : findChildren<QComboBox *>()) {
        if (!choice->objectName().startsWith("sceneActionClassChoice"))
            continue;
        const auto input = findChild<QLineEdit *>(
            QString("sceneActionField%1").arg(choice->property("actionField").toInt()));
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
        const auto opcode = program.commands[action.command].opcode;
        const bool extendedCreate =
            opcode == 0x63 && program.commands[action.command].operands[9] == quint32(-1);
        if (opcode == 0x46 || extendedCreate || opcode == 0x64) {
            const auto &args = program.commands[action.command].operands;
            const auto &prior = original.commands[action.command].operands;
            const bool ordinary = opcode == 0x46;
            const bool create = ordinary || extendedCreate;
            const bool changedPose =
                args[1] != prior[1] || args[2] != prior[2] || (ordinary && args[3] != prior[3]);
            if (changedPose && qint32(args[1]) >= 0) {
                const auto art = ActorArt::load(project_->rom(), int(args[1]));
                if (qint32(args[2]) >= 0 &&
                    art.resolve(qint16(args[2]), ordinary ? int(args[3]) : 0) < 0)
                    throw Error("Choose an animation and facing present in this art bank.");
            }
            if (create && (args[4] != prior[4] || args[5] != prior[5]) && args[4] == 0xffffffffU &&
                args[5] == 0xffffffffU)
                throw Error("A new sprite cannot inherit both starting coordinates; set X or Y.");
        }
        if (opcode == 0x63) {
            const auto &args = program.commands[action.command].operands;
            const auto &prior = original.commands[action.command].operands;
            if ((args[7] != prior[7] || args[8] != prior[8]) && args[8] != quint32(-1) &&
                (args[7] & 0xffffff00U) != 0x08880000U && (args[8] & 0xffffff00U) != 0x08880000U)
                SceneRibbons(project_->rom())
                    .validateRotationSource(qint32(args[7]), qint32(args[8]));
        }
        if (opcode == 0x49 || opcode == 0x4a) {
            const auto &args = program.commands[action.command].operands;
            const auto &prior = original.commands[action.command].operands;
            const auto symbolic = [](quint32 value) {
                return (value & 0xffffff00U) == 0x08880000U;
            };
            if (opcode == 0x49 &&
                (args[1] != prior[1] || args[2] != prior[2] || args[4] != prior[4]) &&
                !symbolic(args[1]) && !symbolic(args[2])) {
                const auto count = SceneRibbons(project_->rom())
                                       .sourcePointCount(qint32(args[1]), qint32(args[2]));
                if (!symbolic(args[4]) && qint32(args[4]) >= count)
                    throw Error(
                        QString("This path has %1 points; choose -1 or a point from 0 through %2.")
                            .arg(count)
                            .arg(count - 1));
                for (int i = action.command + 1; i < program.commands.size(); ++i) {
                    const auto &endpoint = program.commands[i];
                    if (endpoint.opcode == 0x4a && !symbolic(endpoint.operands[2]) &&
                        qint32(endpoint.operands[2]) >= count &&
                        SceneActions::ribbonConstructor(program, i, endpoint.operands[0]) ==
                            action.command)
                        throw Error(
                            QString("The endpoint at word %1 uses point %2, beyond this path's "
                                    "last point %3. Change that endpoint first.")
                                .arg(endpoint.word)
                                .arg(endpoint.operands[2])
                                .arg(count - 1));
                }
            } else if (opcode == 0x4a && (args[0] != prior[0] || args[2] != prior[2]) &&
                       !symbolic(args[2]) && qint32(args[2]) >= 0) {
                const auto source = SceneActions::ribbonSource(program, action.command, args[0]);
                const int count = source ? SceneRibbons(project_->rom())
                                               .sourcePointCount(source->first, source->second)
                                         : 2;
                if (qint32(args[2]) >= count)
                    throw Error(
                        source
                            ? QString("This path has %1 points; the last point is %2.")
                                  .arg(count)
                                  .arg(count - 1)
                            : "The path is chosen outside this action section. Only points 0 and 1 "
                              "are guaranteed here; its full source remains available in "
                              "Commands.");
            }
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
    const bool wait =
        kind_->currentData().toUInt() == 0xe || kind_->currentData().toUInt() == 0xb5 ||
        kind_->currentData().toUInt() == 0xbf || kind_->currentData().toUInt() == 0x54 ||
        kind_->currentData().toUInt() == SceneActions::Choice;
    const bool body = kind_->currentData().toUInt() == 0x2a;
    const bool image =
        kind_->currentData().toUInt() == 0x76 || kind_->currentData().toUInt() == 0x50 ||
        kind_->currentData().toUInt() == 0x3a || kind_->currentData().toUInt() == 0x54;
    const int type = valid ? project_->scenePrograms()->program(key_).resourceClass() : -1;
    const bool allowed = (!wait || position_->currentData(Qt::UserRole + 1).toBool()) &&
                         (!body || type == 1 || type == 8) && (!image || type == 4 || type == 5);
    insert_->setEnabled(valid && position_->currentIndex() >= 0 && allowed);
    insert_->setToolTip(allowed ? "Insert here, then edit the action's fields."
                        : body  ? "Create class Actor requires a fresh environment scene."
                        : image ? "This action requires an image scene."
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
    if (operation == Change::Add && (kind_->currentData().toUInt() == SceneActions::Choice ||
                                     kind_->currentData().toUInt() == 0xbf)) {
        changeDialogue(true, kind_->currentData().toUInt() == SceneActions::Choice);
        return;
    }
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
void SceneActionEditor::changeDialogue(bool create, bool choice) {
    if (!key_ || property("invalidEdits").toBool())
        return;
    try {
        project_->loadDialogues();
        auto program = project_->scenePrograms()->program(key_);
        const int row = list_->currentRow();
        if (!create && (row < 0 || row >= actions_.size()))
            return;
        const int at = create ? position_->currentData().toInt() : actions_[row].command;
        int archive = 5, entry = -1;
        auto table = Dialogues::Table::Primary;
        QPair<QString, QString> options = choice
                                              ? QPair<QString, QString>{"Yes", "No"}
                                              : QPair<QString, QString>{"Speaker", "New dialogue."};
        if (!create) {
            const auto &c = program.commands[at];
            archive = quint16(c.operands[1]);
            entry = quint16(c.operands[2]);
            if ((entry & 0xc000) != 0 && (entry & 0xc000) != 0x8000) {
                emit editDialogue(archive, entry);
                return;
            }
            table = entry & 0x8000 ? Dialogues::Table::Alternate : Dialogues::Table::Primary;
            entry &= 0x3fff;
            const auto bytes = project_->dialogues()->entry(table, archive, entry);
            const auto parsed =
                choice ? SceneActions::choiceOptions(bytes) : Dialogues::spokenFields(bytes);
            if (!parsed) {
                emit editDialogue(archive, quint16(c.operands[2]));
                return;
            }
            options = *parsed;
        }
        QDialog dialog(this);
        dialog.setObjectName(choice ? "sceneChoiceDialog" : "sceneDialogueDialog");
        dialog.setWindowTitle(choice ? (create ? "Add choice" : "Edit choice options")
                                     : (create ? "Add dialogue" : "Edit dialogue"));
        auto layout = new QVBoxLayout(&dialog);
        auto help =
            new QLabel(choice ? "Write two options, up to 24 characters each. Add branch actions "
                                "using the After option 1 / After option 2 positions."
                              : "Write the speaker and dialogue. Lines wrap automatically, with up "
                                "to eight pages. The player advances pages with A. Portrait and "
                                "placement are editable in the action fields.");
        help->setWordWrap(true);
        layout->addWidget(help);
        if (!create) {
            auto shared = new QLabel(
                QString("Archive %1, entry %2. Changes also affect other windows using this text.")
                    .arg(archive)
                    .arg(entry));
            shared->setWordWrap(true);
            layout->addWidget(shared);
        }
        auto form = new QFormLayout;
        auto first = new QLineEdit(options.first);
        first->setObjectName(choice ? "sceneChoiceOption1" : "sceneDialogueSpeaker");
        first->setMaxLength(choice ? 24 : 28);
        form->addRow(choice ? "Option 1" : "Speaker", first);
        QLineEdit *second = nullptr;
        QPlainTextEdit *body = nullptr;
        if (choice) {
            second = new QLineEdit(options.second);
            second->setObjectName("sceneChoiceOption2");
            second->setMaxLength(24);
            form->addRow("Option 2", second);
        } else {
            body = new QPlainTextEdit(options.second);
            body->setObjectName("sceneDialogueText");
            body->setMinimumSize(420, 180);
            form->addRow("Text", body);
        }
        auto encoded = [&] {
            return choice ? SceneActions::choiceText(first->text(), second->text())
                          : Dialogues::spokenText(first->text(), body->toPlainText());
        };
        layout->addLayout(form);
        auto status = new QLabel;
        status->setWordWrap(true);
        layout->addWidget(status);
        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addWidget(buttons);
        auto validate = [&] {
            try {
                encoded();
                status->clear();
                buttons->button(QDialogButtonBox::Ok)->setEnabled(true);
            } catch (const std::exception &e) {
                status->setText(QString::fromUtf8(e.what()));
                buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
            }
        };
        connect(first, &QLineEdit::textChanged, &dialog, validate);
        if (second)
            connect(second, &QLineEdit::textChanged, &dialog, validate);
        if (body)
            connect(body, &QPlainTextEdit::textChanged, &dialog, validate);
        bool raw = false;
        if (!create) {
            auto button = buttons->addButton("Edit raw source", QDialogButtonBox::ActionRole);
            button->setObjectName("sceneDialogueRaw");
            connect(button, &QPushButton::clicked, &dialog, [&] {
                raw = true;
                dialog.reject();
            });
        }
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        const auto result = dialog.exec();
        if (raw) {
            emit editDialogue(archive, entry | (table == Dialogues::Table::Alternate ? 0x8000 : 0));
            return;
        }
        if (result != QDialog::Accepted)
            return;
        const auto text = encoded();
        auto dialogues = *project_->dialogues();
        if (create) {
            entry = dialogues.appendEntry(table, archive, text);
            if (entry > 0x3fff)
                throw Error("This dialogue archive has no remaining entry IDs.");
            program = SceneActions::insert(program, at, choice ? SceneActions::Choice : 0xbf);
            program.commands[at].operands[2] = quint32(entry);
            program = DirectorProgram::decode(program.encode());
        } else {
            if (dialogues.entry(table, archive, entry) == text)
                return;
            dialogues.setEntry(table, archive, entry, text);
        }
        auto scenes = *project_->scenePrograms();
        scenes.replace(key_, program.source);
        auto validation = *project_;
        validation.setDialogues(dialogues.json());
        validation.setScenePrograms(scenes.json());
        editing_ = true;
        undo_->push(new SceneTextCommand(project_, dialog.windowTitle(), scenes, dialogues));
        editing_ = false;
        refresh(at);
        emit commandSelected(at);
        if (changed_)
            changed_();
    } catch (const std::exception &e) {
        editing_ = false;
        notice_->setText(QString::fromUtf8(e.what()));
    }
}
} // namespace lordly
