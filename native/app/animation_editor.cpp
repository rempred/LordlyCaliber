#include "app/animation_editor.h"
#include <QComboBox>
#include <QCheckBox>
#include <QCompleter>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QUndoStack>
#include <QVBoxLayout>
#include <algorithm>
namespace lordly {
namespace {
class AnimationCommand final : public QUndoCommand {
  public:
    AnimationCommand(Project *project, QString text, QJsonObject after)
        : QUndoCommand(text), project_(project), before_(project->animationRoutes()->json()),
          after_(std::move(after)) {}
    void undo() override {
        project_->setAnimationRoutes(before_);
    }
    void redo() override {
        project_->setAnimationRoutes(after_);
    }

  private:
    Project *project_;
    QJsonObject before_, after_;
};
void searchable(QComboBox *box) {
    box->setEditable(true);
    box->setInsertPolicy(QComboBox::NoInsert);
    box->completer()->setFilterMode(Qt::MatchContains);
    box->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    box->setMinimumContentsLength(22);
}
QString code(int n) {
    return QString("0x%1").arg(n, 2, 16, QChar('0')).toUpper();
}
int selectedId(QComboBox *box, const QString &label) {
    if (box->currentIndex() < 0 || box->currentText() != box->itemText(box->currentIndex()))
        throw Error("Choose " + label + " from the search results");
    return box->currentData().toInt();
}
} // namespace
AnimationEditor::AnimationEditor(Project *project, QUndoStack *undo, std::function<void()> changed,
                                 QWidget *parent)
    : QWidget(parent), project_(project), undo_(undo), changed_(std::move(changed)) {
    setObjectName("animationEditor");
    auto layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("<b>Class animation sets</b>"));
    auto explanation =
        new QLabel("Copy an animation set from another class, or edit its body routes, movement "
                   "speeds, equipment artwork and default action poses. Each class keeps its own assignment.");
    explanation->setWordWrap(true);
    layout->addWidget(explanation);
    notice_ = new QLabel;
    notice_->setObjectName("animationNotice");
    notice_->setWordWrap(true);
    try {
        project_->loadAnimationRoutes();
    } catch (const std::exception &e) {
        notice_->setText(QString::fromUtf8(e.what()));
        layout->addWidget(notice_);
        return;
    }
    auto choices = new QFormLayout;
    layout->addLayout(choices);
    classes_ = new QComboBox;
    classes_->setObjectName("animationClass");
    searchable(classes_);
    choices->addRow("Class", classes_);
    auto sourceRow = new QHBoxLayout;
    sources_ = new QComboBox;
    sources_->setObjectName("animationSource");
    searchable(sources_);
    sourceRow->addWidget(sources_, 1);
    auto copy = new QPushButton("Copy animation set");
    copy->setObjectName("animationCopySource");
    sourceRow->addWidget(copy);
    choices->addRow("Source class", sourceRow);
    controls_ = new QWidget;
    auto editing = new QVBoxLayout(controls_);
    editing->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(controls_, 1);
    auto top = new QHBoxLayout;
    editing->addLayout(top);
    auto body = new QGroupBox("Body routes");
    auto bodyForm = new QFormLayout(body);
    top->addWidget(body);
    for (int flag = 0; flag < 4; ++flag) {
        auto input = new QSpinBox;
        handles_[flag] = input;
        input->setObjectName(QString("animationHandle%1").arg(flag));
        input->setRange(1, 65535);
        input->setDisplayIntegerBase(16);
        input->setPrefix("0x");
        input->setKeyboardTracking(false);
        input->setToolTip(
            "Low 12 bits select the body descriptor; high 4 bits select its sprite variant.");
        bodyForm->addRow(QString("Flags %1 / %2").arg(flag / 2).arg(flag % 2), input);
        connect(input, &QSpinBox::valueChanged, this, [this, flag](int value) {
            if (!refreshing_)
                guarded([&] {
                    changeEntry("Change class body route",
                                [&](auto &entry) { entry.handles[flag] = quint16(value); });
                });
        });
    }
    equipmentArt_ = new QComboBox;
    equipmentArt_->setObjectName("animationEquipmentArt");
    searchable(equipmentArt_);
    equipmentArt_->addItem("Use equipped item", -1);
    for (const auto &record : project_->records(Table::Items))
        equipmentArt_->addItem(QString("%1 · %2").arg(record.id).arg(record.name), record.id);
    bodyForm->addRow("Equipment artwork", equipmentArt_);
    equipmentChildren_ = new QCheckBox("Use equipment variants for item-colored layers");
    equipmentChildren_->setObjectName("animationEquipmentChildren");
    equipmentChildren_->setToolTip("Select equipment sprite variants for layers that normally use the item's palette.");
    bodyForm->addRow(equipmentChildren_);
    connect(equipmentArt_, &QComboBox::currentIndexChanged, this, [this] {
        if (!refreshing_) guarded([&] {
            const int item = selectedId(equipmentArt_, "equipment artwork");
            changeEntry("Change equipment artwork source", [&](auto &entry) { entry.equipmentArt = qint16(item); });
        });
    });
    connect(equipmentChildren_, &QCheckBox::toggled, this, [this](bool value) {
        if (!refreshing_) guarded([&] {
            changeEntry("Change equipment image selection", [&](auto &entry) { entry.equipmentChildArt = value; });
        });
    });
    construction_ = new QComboBox;
    construction_->setObjectName("animationConstruction");
    construction_->addItems({"Ordinary", "Danika body", "Danika tendril"});
    bodyForm->addRow("Body construction", construction_);
    fixedPose_ = new QCheckBox("Use fixed base poses (Danika)");
    fixedPose_->setObjectName("animationFixedPose");
    fixedPose_->setToolTip("Use the base primary or secondary pose for ordinary requests. Assigned private attack copies keep their own poses.");
    bodyForm->addRow(fixedPose_);
    connect(construction_, &QComboBox::currentIndexChanged, this, [this](int value) {
        if (!refreshing_) guarded([&] {
            changeEntry("Change body construction", [&](auto &entry) { entry.construction = quint8(value); });
        });
    });
    connect(fixedPose_, &QCheckBox::toggled, this, [this](bool value) {
        if (!refreshing_) guarded([&] {
            changeEntry("Change fixed base poses", [&](auto &entry) { entry.fixedPose = value; });
        });
    });
    auto movement = new QGroupBox("Movement");
    auto speedForm = new QFormLayout(movement);
    top->addWidget(movement);
    for (int direction = 0; direction < 2; ++direction) {
        auto input = new QSpinBox;
        speeds_[direction] = input;
        input->setObjectName(QString("animationSpeed%1").arg(direction));
        input->setRange(0, 255);
        input->setKeyboardTracking(false);
        input->setToolTip("Stored speed divided by 16. Value 255 uses the owner's speed when the "
                          "source matches that owner.");
        speedForm->addRow(direction ? "Return speed (×16)" : "Advance speed (×16)", input);
        connect(input, &QSpinBox::valueChanged, this, [this, direction](int value) {
            if (!refreshing_)
                guarded([&] {
                    changeEntry("Change class animation speed",
                                [&](auto &entry) { entry.speeds[direction] = quint8(value); });
                });
        });
    }
    details_ = new QLabel;
    details_->setObjectName("animationDetails");
    details_->setWordWrap(true);
    speedForm->addRow(details_);
    auto defaults = new QGroupBox("Default action selectors");
    auto defaultsLayout = new QVBoxLayout(defaults);
    editing->addWidget(defaults, 1);
    auto lookup = new QLabel(
        "The game chooses this class's defaults using its source/owner rules. Actions absent from "
        "the list use selector 0x28. Selectors refer to poses in the assigned body.");
    lookup->setWordWrap(true);
    defaultsLayout->addWidget(lookup);
    actions_ = new QTableWidget(0, 4);
    actions_->setObjectName("animationActions");
    actions_->setHorizontalHeaderLabels(
        {"Action", "Normal · mode 0", "Normal · mode 1", "Attack blocked · mode 2"});
    actions_->setSelectionBehavior(QAbstractItemView::SelectRows);
    actions_->setSelectionMode(QAbstractItemView::SingleSelection);
    actions_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    actions_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    actions_->verticalHeader()->hide();
    defaultsLayout->addWidget(actions_, 1);
    auto actionRow = new QHBoxLayout;
    defaultsLayout->addLayout(actionRow);
    action_ = new QComboBox;
    action_->setObjectName("animationAction");
    searchable(action_);
    actionRow->addWidget(action_, 1);
    const auto names = project_->catalog()["ACTION_NAMES"].toObject();
    for (int id = 0; id < 255; ++id)
        action_->addItem(QString("%1 · %2").arg(code(id), names[QString::number(id)].toString(
                                                              QString("Action %1").arg(id))),
                         id);
    for (int mode = 0; mode < 3; ++mode) {
        auto input = new QSpinBox;
        selectors_[mode] = input;
        input->setObjectName(QString("animationSelector%1").arg(mode));
        input->setRange(0, 255);
        input->setDisplayIntegerBase(16);
        input->setPrefix("0x");
        input->setValue(0x28);
        input->setKeyboardTracking(false);
        input->setToolTip(QString("Selector for raw mode %1").arg(mode));
        actionRow->addWidget(new QLabel(QString("Mode %1").arg(mode)));
        actionRow->addWidget(input);
    }
    auto buttons = new QHBoxLayout;
    defaultsLayout->addLayout(buttons);
    auto add = new QPushButton("Add action");
    add->setObjectName("animationAddAction");
    buttons->addWidget(add);
    replace_ = new QPushButton("Update selected action");
    replace_->setObjectName("animationUpdateAction");
    buttons->addWidget(replace_);
    remove_ = new QPushButton("Remove selected action");
    remove_->setObjectName("animationRemoveAction");
    buttons->addWidget(remove_);
    buttons->addStretch();
    layout->addWidget(notice_);
    connect(classes_, &QComboBox::currentIndexChanged, this, [this] {
        if (!refreshing_ && classes_->currentIndex() >= 0) {
            selected_ = classes_->currentData().toInt();
            actionRow_ = -1;
            guarded([this] { refreshEntry(); });
        }
    });
    connect(copy, &QPushButton::clicked, this, [this] {
        guarded([this] {
            auto next = *project_->animationRoutes();
            next.cloneClass(selectedId(sources_, "a source class"),
                            selectedId(classes_, "a class"));
            apply(next, "Copy class animation set");
        });
    });
    connect(actions_, &QTableWidget::itemSelectionChanged, this, [this] {
        if (!refreshing_) {
            actionRow_ = actions_->currentRow();
            refreshAction();
        }
    });
    auto commitAction = [this](bool adding) {
        guarded([this, adding] {
            const int action = selectedId(action_, "an action");
            if (action < 0 || action > 254)
                throw Error("Choose an action");
            QByteArray row;
            row += char(action);
            for (auto input : selectors_)
                row += char(input->value());
            const int selected = actionRow_;
            changeEntry(adding ? "Add class animation action" : "Change class animation action",
                        [&](auto &entry) {
                            for (int at = 0; at + 1 < entry.selectors.size(); at += 4)
                                if (u8(entry.selectors, at) == action &&
                                    (adding || at / 4 != selected))
                                    throw Error("This action already has a default selector row");
                            if (adding) {
                                if (entry.selectors.isEmpty())
                                    entry.selectors += char(-1);
                                actionRow_ = int(entry.selectors.size() / 4);
                                entry.selectors.insert(entry.selectors.size() - 1, row);
                            } else {
                                if (selected < 0 || selected * 4 + 4 >= entry.selectors.size())
                                    throw Error("Select an action row to update");
                                entry.selectors.replace(selected * 4, 4, row);
                            }
                        });
        });
    };
    connect(add, &QPushButton::clicked, this, [commitAction] { commitAction(true); });
    connect(replace_, &QPushButton::clicked, this, [commitAction] { commitAction(false); });
    connect(remove_, &QPushButton::clicked, this, [this] {
        guarded([this] {
            const int selected = actionRow_;
            changeEntry("Remove class animation action", [&](auto &entry) {
                if (selected < 0 || selected * 4 + 4 >= entry.selectors.size())
                    throw Error("Select an action row to remove");
                entry.selectors.remove(selected * 4, 4);
                actionRow_ = std::min(selected, int(entry.selectors.size() / 4) - 1);
            });
        });
    });
    connect(undo_, &QUndoStack::indexChanged, this, [this] {
        guarded([this] { refresh(); });
        changed_();
    });
    refresh();
}
AnimationEditor::~AnimationEditor() {
    disconnect(undo_, nullptr, this, nullptr);
}
void AnimationEditor::guarded(const std::function<void()> &action) {
    try {
        notice_->clear();
        action();
    } catch (const std::exception &e) {
        const auto message = QString::fromUtf8(e.what());
        try {
            refreshEntry();
        } catch (...) {
        }
        notice_->setText(message);
    }
}
void AnimationEditor::apply(const AnimationRoutes &next, const QString &label) {
    if (next.json() != project_->animationRoutes()->json())
        undo_->push(new AnimationCommand(project_, label, next.json()));
}
void AnimationEditor::changeEntry(const QString &label,
                                  const std::function<void(AnimationRoutes::Entry &)> &change) {
    selectedId(classes_, "a class");
    auto next = *project_->animationRoutes();
    auto entry = next.entry(selected_);
    change(entry);
    next.setEntry(selected_, entry);
    apply(next, label);
}
void AnimationEditor::selectClass(int id) {
    if (!classes_ || id < 1 || id >= project_->records(Table::Classes).size())
        throw Error("Choose an existing class");
    selected_ = id;
    actionRow_ = -1;
    refresh();
}
void AnimationEditor::refresh() {
    if (!classes_)
        return;
    project_->loadAnimationRoutes();
    QScopedValueRollback<bool> guard(refreshing_, true);
    const auto source = sources_->currentData().toInt();
    classes_->clear();
    sources_->clear();
    const auto &records = project_->records(Table::Classes);
    selected_ = std::clamp(selected_, 1, int(records.size()) - 1);
    for (int id = 1; id < records.size(); ++id) {
        const auto label = QString("%1 · %2").arg(id).arg(records[id].name);
        classes_->addItem(label, id);
        if (!project_->animationRoutes()->pending().contains(id)) {
            try {
                project_->animationRoutes()->entry(id);
                sources_->addItem(label, id);
            } catch (const Error &) {
            }
        }
    }
    classes_->setCurrentIndex(classes_->findData(selected_));
    sources_->setCurrentIndex(std::max(0, sources_->findData(source)));
    refreshEntry();
}
void AnimationEditor::refreshEntry() {
    if (!controls_)
        return;
    QScopedValueRollback<bool> guard(refreshing_, true);
    notice_->clear();
    const bool pending = project_->animationRoutes()->pending().contains(selected_);
    controls_->setEnabled(!pending);
    if (pending) {
        actions_->setRowCount(0);
        details_->clear();
        notice_->setText("This imported class needs an animation source. Choose a class above and "
                         "copy its animation set before exporting.");
        return;
    }
    const auto entry = project_->animationRoutes()->entry(selected_);
    equipmentArt_->setCurrentIndex(equipmentArt_->findData(entry.equipmentArt));
    equipmentChildren_->setChecked(entry.equipmentChildArt);
    construction_->setCurrentIndex(entry.construction);
    fixedPose_->setChecked(entry.fixedPose);
    for (int flag = 0; flag < 4; ++flag)
        handles_[flag]->setValue(entry.handles[flag]);
    for (int dir = 0; dir < 2; ++dir)
        speeds_[dir]->setValue(entry.speeds[dir]);
    auto speed = [](int value) { return QString::number(value / 16.0); };
    details_->setText(QString("Game speed: %1 advance · %2 return")
                          .arg(speed(entry.speeds[0]), speed(entry.speeds[1])));
    const int count = int(entry.selectors.size() / 4);
    actions_->setRowCount(count);
    for (int row = 0; row < count; ++row) {
        const int action = u8(entry.selectors, row * 4);
        actions_->setItem(row, 0,
                          new QTableWidgetItem(action_->itemText(action_->findData(action))));
        for (int mode = 0; mode < 3; ++mode)
            actions_->setItem(row, mode + 1,
                              new QTableWidgetItem(code(u8(entry.selectors, row * 4 + 1 + mode))));
    }
    actionRow_ = count ? std::clamp(actionRow_, 0, count - 1) : -1;
    if (actionRow_ >= 0)
        actions_->selectRow(actionRow_);
    refreshAction();
}
void AnimationEditor::refreshAction() {
    const bool selected = actionRow_ >= 0 && actionRow_ < actions_->rowCount();
    replace_->setEnabled(selected);
    remove_->setEnabled(selected);
    if (!selected)
        return;
    const auto entry = project_->animationRoutes()->entry(selected_);
    const auto at = actionRow_ * 4;
    action_->setCurrentIndex(action_->findData(u8(entry.selectors, at)));
    for (int mode = 0; mode < 3; ++mode)
        selectors_[mode]->setValue(u8(entry.selectors, at + 1 + mode));
}
} // namespace lordly
