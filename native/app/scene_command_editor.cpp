#include "app/scene_command_editor.h"
#include <QComboBox>
#include <QCompleter>
#include <QDateTime>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QUndoStack>
#include <QVBoxLayout>
namespace lordly {
namespace {
class SceneCommand final : public QUndoCommand {
  public:
    SceneCommand(Project *project, QString label, QJsonObject before, QJsonObject after,
                 QString merge)
        : QUndoCommand(label), project_(project), before_(std::move(before)),
          after_(std::move(after)), merge_(std::move(merge)),
          time_(QDateTime::currentMSecsSinceEpoch()) {}
    void undo() override {
        project_->setScenePrograms(before_);
    }
    void redo() override {
        project_->setScenePrograms(after_);
    }
    int id() const override {
        return merge_.isEmpty() ? -1 : 0x4c4316;
    }
    bool mergeWith(const QUndoCommand *command) override {
        const auto other = dynamic_cast<const SceneCommand *>(command);
        if (!other || other->project_ != project_ || other->merge_ != merge_ ||
            other->before_ != after_ || other->time_ - time_ > 1000)
            return false;
        after_ = other->after_;
        time_ = other->time_;
        return true;
    }

  private:
    Project *project_;
    QJsonObject before_, after_;
    QString merge_;
    qint64 time_;
};
} // namespace
void pushSceneProgramChange(Project *project, QUndoStack *undo, const QString &label,
                            const ScenePrograms &next, const QString &merge) {
    if (next.json() == project->scenePrograms()->json())
        return;
    auto validation = *project;
    validation.setScenePrograms(next.json());
    undo->push(
        new SceneCommand(project, label, project->scenePrograms()->json(), next.json(), merge));
}
SceneCommandEditor::SceneCommandEditor(Project *project, QUndoStack *undo,
                                       std::function<void()> changed, QWidget *parent)
    : QWidget(parent), project_(project), undo_(undo), changed_(std::move(changed)) {
    setObjectName("sceneCommandEditor");
    project_->loadScenePrograms();
    auto layout = new QVBoxLayout(this);
    description_ = new QLabel("Select a source command to edit its fields.");
    description_->setWordWrap(true);
    layout->addWidget(description_);
    auto area = new QScrollArea;
    area->setWidgetResizable(true);
    auto form = new QWidget;
    fields_ = new QFormLayout(form);
    area->setWidget(form);
    layout->addWidget(area, 1);
    notice_ = new QLabel("Values accept signed decimal or 0x-prefixed hexadecimal. Valid edits "
                         "enter the project immediately.");
    notice_->setObjectName("sceneCommandStatus");
    notice_->setWordWrap(true);
    layout->addWidget(notice_);
    auto add = new QHBoxLayout;
    kind_ = new QComboBox;
    kind_->setObjectName("sceneCommandKind");
    kind_->setEditable(true);
    kind_->setInsertPolicy(QComboBox::NoInsert);
    kind_->completer()->setFilterMode(Qt::MatchContains);
    for (const auto &definition : Directors::grammar()) {
        if (definition.opcode == 0x80000001)
            continue;
        kind_->addItem(
            QString("%1 · %2").arg(definition.name).arg(definition.opcode, 2, 16, QChar('0')),
            definition.opcode);
    }
    add->addWidget(kind_, 1);
    insert_ = new QPushButton("Insert before");
    insert_->setObjectName("sceneCommandInsert");
    add->addWidget(insert_);
    remove_ = new QPushButton("Remove command");
    remove_->setObjectName("sceneCommandRemove");
    add->addWidget(remove_);
    layout->addLayout(add);
    auto actions = new QHBoxLayout;
    restore_ = new QPushButton("Restore original scene");
    restore_->setObjectName("sceneCommandRestore");
    actions->addWidget(restore_);
    discard_ = new QPushButton("Discard invalid input");
    discard_->setObjectName("sceneCommandDiscard");
    actions->addWidget(discard_);
    actions->addStretch();
    layout->addLayout(actions);
    connect(undo_, &QUndoStack::indexChanged, this,
            [this, previous = project_->scenePrograms()->json()]() mutable {
        const auto next = project_->scenePrograms()->json();
        const bool changedProgram = next != previous;
        previous = next;
        if (!editing_ && changedProgram) {
            if (!property("invalidEdits").toBool()) refresh();
            emit programChanged();
        }
        if (changed_) changed_();
    });
    connect(insert_, &QPushButton::clicked, this, [this] {
        try {
            const auto choice = kind_->findText(kind_->currentText());
            if (choice < 0)
                throw Error("Choose a command from the list.");
            auto next = *project_->scenePrograms();
            next.insertCommand(key_, command_, kind_->itemData(choice).toUInt());
            publish("Insert scene command", next);
        } catch (const std::exception &e) {
            notice_->setText(QString::fromUtf8(e.what()));
        }
    });
    connect(remove_, &QPushButton::clicked, this, [this] {
        try {
            auto next = *project_->scenePrograms();
            next.removeCommand(key_, command_);
            publish("Remove scene command", next);
        } catch (const std::exception &e) {
            notice_->setText(QString::fromUtf8(e.what()));
        }
    });
    connect(restore_, &QPushButton::clicked, this, [this] {
        auto next = *project_->scenePrograms();
        next.restore(key_);
        publish("Restore scene", next);
        refresh();
    });
    connect(discard_, &QPushButton::clicked, this, [this] {
        refresh();
        if (changed_)
            changed_();
        emit programChanged();
    });
    refresh();
}
void SceneCommandEditor::selectCommand(SceneId key, int command) {
    if (editing_ || property("invalidEdits").toBool())
        return;
    key_ = key;
    command_ = command;
    refresh();
}
void SceneCommandEditor::validity(bool valid) {
    setProperty("invalidEdits", !valid);
    kind_->setEnabled(valid && key_);
    insert_->setEnabled(valid && command_ >= 0);
    remove_->setEnabled(valid && command_ >= 0 &&
                        command_ + 1 < project_->scenePrograms()->program(key_).commands.size());
    restore_->setEnabled(bool(key_));
    discard_->setEnabled(!valid);
}
void SceneCommandEditor::refresh() {
    if (editing_)
        return;
    while (fields_->rowCount())
        fields_->removeRow(0);
    if (!key_.authoredId.isEmpty() &&
        !project_->scenePrograms()->authoredScenes().contains(key_.authoredId))
        key_ = {};
    if (!key_) {
        command_ = -1;
        validity(true);
        return;
    }
    const auto program = project_->scenePrograms()->program(key_);
    command_ = qBound(0, command_, int(program.commands.size()) - 1);
    const auto &command = program.commands[command_];
    const auto definition = Directors::grammar()[command.opcode];
    description_->setText(definition.name + " — " + definition.description);
    for (int i = 0; i < command.operands.size(); ++i) {
        auto field = new QLineEdit(QString::number(qint32(command.operands[i])));
        field->setObjectName(QString("sceneOperand%1").arg(i));
        field->setAccessibleName(definition.operands.value(i));
        field->setToolTip(QString("Native word: 0x%1").arg(command.operands[i], 8, 16, QChar('0')));
        field->setReadOnly(command.opcode == 0x80000001);
        auto name = definition.operands.value(i);
        name.replace('_', ' ');
        fields_->addRow(name, field);
        connect(field, &QLineEdit::textChanged, this, [this, field](const QString &text) {
            try {
                auto next = *project_->scenePrograms();
                for (auto other : findChildren<QLineEdit *>()) {
                    if (!other->objectName().startsWith("sceneOperand"))
                        continue;
                    bool accepted = false;
                    const auto candidate = other->text().trimmed();
                    const auto number = candidate.toLongLong(
                        &accepted, candidate.startsWith("0x", Qt::CaseInsensitive) ? 16 : 10);
                    if (!accepted || number < -2147483648LL || number > 4294967295LL)
                        throw Error(
                            "Complete every operand as a signed 32-bit value or hexadecimal word.");
                    next.setOperand(key_, command_, other->objectName().mid(12).toInt(),
                                    quint32(number));
                }
                bool valid = false;
                const auto input = text.trimmed();
                const auto value =
                    input.toLongLong(&valid, input.startsWith("0x", Qt::CaseInsensitive) ? 16 : 10);
                if (!valid || value < -2147483648LL || value > 4294967295LL)
                    throw Error("Enter a signed 32-bit value or an unsigned hexadecimal word.");
                validity(true);
                editing_ = true;
                publish("Edit scene command", next, true);
                field->setToolTip(
                    QString("Native word: 0x%1").arg(quint32(value), 8, 16, QChar('0')));
                notice_->setText("Command updated in the project.");
                emit programChanged();
                editing_ = false;
            } catch (const std::exception &e) {
                editing_ = false;
                validity(false);
                notice_->setText(QString("Invalid edit: %1 Save and export are blocked.")
                                     .arg(QString::fromUtf8(e.what())));
                emit programChanged();
            }
            if (changed_)
                changed_();
        });
    }
    validity(true);
    notice_->setText("Values accept signed decimal or 0x-prefixed hexadecimal. Valid edits enter "
                     "the project immediately.");
}
void SceneCommandEditor::publish(const QString &label, const ScenePrograms &next, bool merge) {
    pushSceneProgramChange(project_, undo_, label, next,
                           merge ? QString("%1/%2").arg(key_.label()).arg(command_) : QString());
}
} // namespace lordly
