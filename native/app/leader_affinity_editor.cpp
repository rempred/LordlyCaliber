#include "app/leader_affinity_editor.h"
#include "app/content_refresh.h"
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QUndoStack>
#include <QVBoxLayout>

namespace lordly {
namespace {
class AffinityCommand final : public GameRuleUndoCommand {
  public:
    AffinityCommand(Project *p, QJsonObject after)
        : GameRuleUndoCommand("Change combat leader affinity"), project_(p),
          before_(p->tools()->json()), after_(std::move(after)) {}
    void undo() override { project_->setTools(before_); }
    void redo() override { project_->setTools(after_); }
  private:
    Project *project_;
    QJsonObject before_, after_;
};
QLabel *wrapped(const QString &text = {}) {
    auto label = new QLabel(text); label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText); return label;
}
}
LeaderAffinityEditor::LeaderAffinityEditor(Project *project, QUndoStack *undo,
                                           std::function<void()> changed)
    : project_(project), undo_(undo), changed_(std::move(changed)) {
    setObjectName("leaderAffinityEditor");
    project_->loadTools();
    auto outer = new QVBoxLayout(this); outer->setContentsMargins(0, 0, 0, 0);
    auto group = new QGroupBox("Leader affinity — combat bonuses");
    outer->addWidget(group);
    auto body = new QVBoxLayout(group);
    scope_ = wrapped(); scope_->setObjectName("leaderAffinityScope"); body->addWidget(scope_);
    body->addWidget(wrapped("Bonuses given to the listed squad members when this class leads. "
        "The same strength affects attack power, magic power, defense, accuracy and evasion. "
        "+15 makes a neutral 100% score multiplier 115%; it is not a flat +15 damage or hit chance. "
        "Persuasion has its own table under Encounters."));
    auto grid = new QGridLayout;
    grid->setColumnStretch(1, 1);
    const QStringList headers{"Group", "Member classes", "Strength"};
    for (int column = 0; column < 3; ++column) {
        auto title = wrapped(headers[column]); auto font = title->font(); font.setBold(true);
        title->setFont(font); grid->addWidget(title, 0, column);
    }
    for (int row = 0; row < 4; ++row) {
        grid->addWidget(wrapped(LeaderAffinity::groupName(row)), row + 1, 0, Qt::AlignTop);
        members_[row] = wrapped(); grid->addWidget(members_[row], row + 1, 1, Qt::AlignTop);
        auto values = new QVBoxLayout;
        strength_[row] = new QSpinBox;
        strength_[row]->setObjectName(QString("leaderAffinityStrength%1").arg(row));
        strength_[row]->setRange(-100, 100);
        strength_[row]->setSuffix(" points");
        strength_[row]->setKeyboardTracking(false);
        values->addWidget(strength_[row]);
        effects_[row] = wrapped(); values->addWidget(effects_[row]);
        grid->addLayout(values, row + 1, 2, Qt::AlignTop);
        connect(strength_[row], &QSpinBox::valueChanged, this, [this, row](int value) {
            if (leader_ < 1) return;
            try {
                auto next = project_->tools()->leaderAffinity();
                next.set(leader_, row, value); write(next);
            } catch (const Error &e) { refresh(); notice_->setText(QString::fromUtf8(e.what())); }
        });
    }
    body->addLayout(grid);
    retail_ = new QPushButton("Restore retail affinity");
    retail_->setObjectName("leaderAffinityRetail"); body->addWidget(retail_);
    notice_ = wrapped(); notice_->setObjectName("leaderAffinityNotice"); body->addWidget(notice_);
    connect(retail_, &QPushButton::clicked, this, [this] {
        if (leader_ < 1) return;
        try {
            auto next = project_->tools()->leaderAffinity();
            const auto values = LeaderAffinity::retail(leader_);
            for (int group = 0; group < 4; ++group) next.set(leader_, group, values[group]);
            write(next);
        } catch (const Error &e) { notice_->setText(QString::fromUtf8(e.what())); }
    });
    connect(undo_, &QUndoStack::indexChanged, this, [this] { refresh(); });
    refresh();
}
void LeaderAffinityEditor::selectClass(int id) { selected_ = id; refresh(); }
void LeaderAffinityEditor::refresh() {
    const auto &records = project_->records(Table::Classes);
    const bool valid = selected_ > 0 && selected_ < records.size();
    leader_ = valid ? selected_ : -1;
    if (valid) {
        const auto schema = fields(Table::Classes);
        for (int i = 0; i < schema.size(); ++i)
            if (schema[i].key == "classCopyMatch") {
                const int link = project_->value(Table::Classes, selected_, i);
                if (link) leader_ = link;
                break;
            }
    }
    const auto &affinity = project_->tools()->leaderAffinity();
    const bool available = valid && leader_ > 0 && leader_ < records.size() && affinity.unavailable().isEmpty();
    auto name = [&](int id) { return id < records.size() ? records[id].name.simplified() : QString("Class %1").arg(id); };
    scope_->setText(!valid ? "Select a class to see its leader bonuses." : leader_ != selected_
        ? QString("This class starts with %1 as its current class. These bonuses belong to %1 and are shared by every leader using that current class.").arg(name(leader_))
        : QString("Bonuses for %1 as the leader's current class.").arg(name(leader_)));
    notice_->setText(affinity.unavailable());
    const auto values = affinity.bonuses(leader_);
    for (int row = 0; row < 4; ++row) {
        QStringList names; for (int id : LeaderAffinity::members(row)) names << name(id);
        members_[row]->setText(names.join(", "));
        const QSignalBlocker blocked(strength_[row]);
        strength_[row]->setEnabled(available); strength_[row]->setValue(values[row]);
        effects_[row]->setText(available ? QString("100% → %1%").arg(100 + values[row]) : QString());
    }
    retail_->setEnabled(available && values != LeaderAffinity::retail(leader_));
}
void LeaderAffinityEditor::write(const LeaderAffinity &affinity) {
    auto next = project_->tools()->json(); next["leaderAffinity"] = affinity.json();
    if (next != project_->tools()->json()) {
        undo_->push(new AffinityCommand(project_, next)); changed_();
    }
    refresh();
}
}
