#include "app/view_position.h"
#include "app/text_workspace.h"
#include "app/animation_override_editor.h"
#include <QComboBox>
#include <QCompleter>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QTableWidget>
#include <QUndoStack>
#include <QVBoxLayout>
#include <algorithm>
namespace lordly {
namespace {
QString identity(const QJsonObject &row) {
    return QString("%1:%2:%3")
        .arg(row["classId"].toInt())
        .arg(row["actionId"].toInt())
        .arg(row.contains("bodyFlags") ? row["bodyFlags"].toInt() : -1);
}
QString hex(int value) {
    return QString("0x%1").arg(value, 2, 16, QChar('0'));
}
void searchable(QComboBox *box) {
    box->setEditable(true);
    box->setInsertPolicy(QComboBox::NoInsert);
    box->completer()->setFilterMode(Qt::MatchContains);
    box->setMinimumContentsLength(24);
    box->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
}
int selected(QComboBox *box) {
    if (box->currentIndex() < 0 || box->currentText() != box->itemText(box->currentIndex()))
        throw Error("Choose a class and action from the search results");
    return box->currentData().toInt();
}
class OverrideCommand final : public QUndoCommand {
  public:
    OverrideCommand(Project *project, QString label, QJsonObject after)
        : QUndoCommand(label), project_(project), before_(project->animationOverrides()->json()),
          after_(std::move(after)) {}
    void undo() override {
        project_->setAnimationOverrides(before_);
    }
    void redo() override {
        project_->setAnimationOverrides(after_);
    }

  private:
    Project *project_;
    QJsonObject before_, after_;
};
} // namespace
AnimationOverrideEditor::AnimationOverrideEditor(Project *project, QUndoStack *undo,
                                                 std::function<void()> changed, QWidget *parent)
    : QWidget(parent), project_(project), undo_(undo), changed_(std::move(changed)) {
    setObjectName("animationOverrideEditor");
    auto layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("<b>Attack animation overrides</b>"));
    auto description =
        new QLabel("Choose separate Normal and Attack blocked poses for an action. These overrides "
                   "use the existing Expansion Pak patch and require 8 MB RAM. Default class "
                   "animation sets are edited in Class animations.");
    description->setWordWrap(true);
    layout->addWidget(description);
    notice_ = new QLabel;
    notice_->setObjectName("overrideNotice");
    notice_->setWordWrap(true);
    try {
        project_->loadAnimationOverrides();
        project_->loadAnimationRoutes();
    } catch (const std::exception &e) {
        notice_->setText(QString::fromUtf8(e.what()));
        layout->addWidget(notice_);
        return;
    }
    table_ = new QTableWidget(0, 7);
    table_->setObjectName("overrideRows");
    table_->setHorizontalHeaderLabels({"Class", "Owner context", "Action", "Body route", "Normal",
                                       "Attack blocked", "Original match"});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->verticalHeader()->hide();
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(table_, 1);
    summary_ = new QLabel;
    summary_->setObjectName("overrideSummary");
    layout->addWidget(summary_);
    controls_ = new QWidget;
    auto fields = new QFormLayout(controls_);
    layout->addWidget(controls_);
    classes_ = new QComboBox;
    classes_->setObjectName("overrideClass");
    searchable(classes_);
    fields->addRow("Class", classes_);
    actions_ = new QComboBox;
    actions_->setObjectName("overrideAction");
    searchable(actions_);
    auto actionRow = new QHBoxLayout;
    actionRow->addWidget(actions_, 1);
    actionRow->addWidget(createActionDescriptionButton(actions_, project_, undo_, changed_));
    fields->addRow("Action", actionRow);
    routes_ = new QComboBox;
    routes_->setObjectName("overrideRoute");
    routes_->addItem("All four body routes", -1);
    for (int f = 0; f < 4; ++f)
        routes_->addItem(QString("Flags %1 / %2").arg(f / 2).arg(f % 2), f);
    fields->addRow("Body route", routes_);
    normal_ = new QComboBox;
    normal_->setObjectName("overrideNormal");
    fields->addRow("Normal", normal_);
    blocked_ = new QComboBox;
    blocked_->setObjectName("overrideBlocked");
    fields->addRow("Attack blocked", blocked_);
    auto buttons = new QHBoxLayout;
    fields->addRow(buttons);
    auto button = [&](const QString &text, const char *id, const std::function<void()> &action) {
        auto result = new QPushButton(text);
        result->setObjectName(id);
        buttons->addWidget(result);
        connect(result, &QPushButton::clicked, this, [this, action] { guarded(action); });
    };
    button("Add override", "overrideAdd", [this] {
        auto row = input();
        auto entries = project_->animationOverrides()->entries();
        entries << row;
        apply(entries, "Add attack animation override", identity(row));
    });
    button("Update selected", "overrideUpdate", [this] {
        auto row = input();
        auto entries = project_->animationOverrides()->entries();
        int found = -1;
        for (int i = 0; i < entries.size(); ++i)
            if (identity(entries[i].toObject()) == selection_)
                found = i;
        if (found < 0)
            throw Error("Select an override row to update");
        entries[found] = row;
        apply(entries, "Change attack animation override", identity(row));
    });
    button("Remove selected", "overrideRemove", [this] {
        auto entries = project_->animationOverrides()->entries();
        int found = -1;
        for (int i = 0; i < entries.size(); ++i)
            if (identity(entries[i].toObject()) == selection_)
                found = i;
        if (found < 0)
            throw Error("Select an override row to remove");
        entries.removeAt(found);
        apply(entries, "Remove attack animation override", {});
    });
    button("Remove all overrides", "overrideClear",
           [this] { apply({}, "Remove all attack animation overrides", {}); });
    buttons->addStretch();
    layout->addWidget(notice_);
    for (auto box : {classes_, actions_, routes_})
        connect(box, &QComboBox::currentIndexChanged, this, [this] {
            if (!refreshing_) {
                selection_.clear();
                table_->clearSelection();
                guarded([this] { choices(); });
            }
        });
    connect(table_, &QTableWidget::itemSelectionChanged, this, [this] {
        if (!refreshing_)
            guarded([this] { selectRow(); });
    });
    connect(undo_, &QUndoStack::indexChanged, this, [this] {
        guarded([this] { refresh(); });
        changed_();
    });
    refresh();
}
AnimationOverrideEditor::~AnimationOverrideEditor() {
    disconnect(undo_, nullptr, this, nullptr);
}
void AnimationOverrideEditor::guarded(const std::function<void()> &action) {
    try {
        notice_->clear();
        action();
    } catch (const std::exception &e) {
        notice_->setText(QString::fromUtf8(e.what()));
    }
}
QJsonObject AnimationOverrideEditor::input() const {
    if (normal_->currentIndex() < 0 || blocked_->currentIndex() < 0)
        throw Error("No compatible attack poses are available for this class and body route");
    QJsonObject row{{"classId", selected(classes_)},
                    {"actionId", selected(actions_)},
                    {"normalSelector", normal_->currentData().toInt()},
                    {"blockedSelector", blocked_->currentData().toInt()}};
    if (routes_->currentData().toInt() >= 0)
        row["bodyFlags"] = routes_->currentData().toInt();
    return row;
}
void AnimationOverrideEditor::apply(QJsonArray entries, const QString &label,
                                    const QString &selection) {
    auto next = *project_->animationOverrides();
    next.setEntries(entries, project_->rom(), project_->classData(), *project_->animationRoutes());
    selection_ = selection;
    if (next.json() != project_->animationOverrides()->json())
        undo_->push(new OverrideCommand(project_, label, next.json()));
    else
        refresh();
}
void AnimationOverrideEditor::refresh() {
    const ViewPosition keepView(this);
    if (!classes_)
        return;
    project_->loadAnimationOverrides();
    project_->loadAnimationRoutes();
    QScopedValueRollback<bool> guard(refreshing_, true);
    const int cls = classes_->currentData().toInt(), action = actions_->currentData().toInt();
    classes_->clear();
    actions_->clear();
    const auto &records = project_->records(Table::Classes);
    for (int id = 1; id < records.size(); ++id)
        classes_->addItem(QString("%1 · %2").arg(id).arg(records[id].name), id);
    for (const auto &value : AnimationOverrideImage::definition()["actions"].toArray()) {
        const auto row = value.toObject();
        const int id = row["id"].toInt();
        actions_->addItem(QString("%1 · %2").arg(hex(id), project_->catalog()["ACTION_NAMES"]
                                                              .toObject()[QString::number(id)]
                                                              .toString(row["name"].toString())),
                          id);
    }
    classes_->setCurrentIndex(std::max(0, classes_->findData(cls)));
    actions_->setCurrentIndex(std::max(0, actions_->findData(action)));
    const auto &model = *project_->animationOverrides();
    controls_->setEnabled(model.available());
    QJsonArray rows = model.entries();
    if (!model.available())
        for (const auto &raw : model.image().rows())
            rows << QJsonObject{{"classId", raw[0]},
                                {"owner", raw[1]},
                                {"actionId", raw[2]},
                                {"normalSelector", raw[5]},
                                {"blockedSelector", raw[6]},
                                {"bodyFlags", raw[3] & 2 ? int(raw[7]) : -1},
                                {"original", raw[3] & 1 ? int(raw[4]) : -1}};
    table_->setRowCount(rows.size());
    int selectedRow = -1;
    for (int i = 0; i < rows.size(); ++i) {
        const auto row = rows[i].toObject();
        const int id = row["classId"].toInt();
        const bool known = id >= 0 && id < records.size();
        const int field = known ? u8(records[id].bytes, 69) : 0;
        const int owner = row.contains("owner") ? row["owner"].toInt() : (field ? field : id);
        const int flags = row.contains("bodyFlags") ? row["bodyFlags"].toInt() : -1;
        const QStringList values{
            known ? QString("%1 · %2").arg(id).arg(records[id].name) : QString::number(id),
            QString::number(owner),
            (actions_->findData(row["actionId"].toInt()) < 0
                 ? hex(row["actionId"].toInt())
                 : actions_->itemText(actions_->findData(row["actionId"].toInt()))),
            flags < 0 ? "All" : QString("%1 / %2").arg(flags / 2).arg(flags % 2),
            hex(row["normalSelector"].toInt()),
            hex(row["blockedSelector"].toInt()),
            row.contains("original") && row["original"].toInt() >= 0 ? hex(row["original"].toInt())
                                                                     : "Any"};
        for (int column = 0; column < values.size(); ++column)
            table_->setItem(i, column, new QTableWidgetItem(values[column]));
        table_->item(i, 0)->setData(Qt::UserRole, row);
        if (identity(row) == selection_)
            selectedRow = i;
    }
    summary_->setText(QString("%1 / 128 overrides").arg(rows.size()));
    if (!model.available()) {
        notice_->setText(model.unavailableReason());
        return;
    }
    if (selectedRow >= 0) {
        table_->selectRow(selectedRow);
        selectRow();
    } else {
        selection_.clear();
        table_->clearSelection();
        table_->setCurrentCell(-1, -1);
        choices();
    }
}
void AnimationOverrideEditor::selectRow() {
    if (!project_->animationOverrides()->available() || table_->currentRow() < 0 ||
        table_->selectedItems().isEmpty() || !table_->item(table_->currentRow(), 0))
        return;
    QScopedValueRollback<bool> guard(refreshing_, true);
    const auto row = table_->item(table_->currentRow(), 0)->data(Qt::UserRole).toJsonObject();
    selection_ = identity(row);
    classes_->setCurrentIndex(classes_->findData(row["classId"].toInt()));
    actions_->setCurrentIndex(actions_->findData(row["actionId"].toInt()));
    routes_->setCurrentIndex(
        routes_->findData(row.contains("bodyFlags") ? row["bodyFlags"].toInt() : -1));
    normal_->clear();
    normal_->addItem(hex(row["normalSelector"].toInt()), row["normalSelector"].toInt());
    blocked_->clear();
    blocked_->addItem(hex(row["blockedSelector"].toInt()), row["blockedSelector"].toInt());
    choices(true);
}
void AnimationOverrideEditor::choices(bool stored) {
    if (classes_->currentIndex() < 0 || actions_->currentIndex() < 0)
        return;
    QScopedValueRollback<bool> guard(refreshing_, true);
    const int id = classes_->currentData().toInt(), action = actions_->currentData().toInt(),
              flags = routes_->currentData().toInt();
    int normal = stored ? normal_->currentData().toInt() : 40,
        blocked = stored ? blocked_->currentData().toInt() : 40;
    normal_->clear();
    blocked_->clear();
    if (!stored) {
        const auto entry = project_->animationRoutes()->entry(id);
        for (int at = 0; at + 1 < entry.selectors.size(); at += 4)
            if (u8(entry.selectors, at) == action) {
                normal = u8(entry.selectors, at + 1);
                blocked = u8(entry.selectors, at + 3);
                break;
            }
    }
    const auto options =
        AnimationOverrides::selectors(project_->rom(), *project_->animationRoutes(), id,
                                      flags < 0 ? std::nullopt : std::optional<int>(flags));
    normal_->clear();
    blocked_->clear();
    for (auto box : {normal_, blocked_})
        for (int value : options)
            box->addItem(hex(value), value);
    for (const auto &[box, value] :
         std::array<QPair<QComboBox *, int>, 2>{{{normal_, normal}, {blocked_, blocked}}}) {
        if (stored && box->findData(value) < 0)
            box->addItem(hex(value) + " (loaded)", value);
        box->setCurrentIndex(box->findData(value) >= 0 ? box->findData(value)
                                                       : (box->count() ? 0 : -1));
    }
}
} // namespace lordly
