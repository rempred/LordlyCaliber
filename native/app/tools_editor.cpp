#include "app/view_position.h"
#include "app/tools_editor.h"
#include "app/game_rules_editor.h"
#include "app/training_editor.h"
#include <QTabWidget>
#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
namespace lordly {
namespace {
class ExperienceCommand final : public QUndoCommand {
  public:
    ExperienceCommand(Project *project, int value)
        : QUndoCommand("Change experience size weight"), project_(project),
          before_(project->tools()->experiencePercent()), after_(value) {}
    void undo() override {
        apply(before_);
    }
    void redo() override {
        apply(after_);
    }

  private:
    void apply(int value) {
        auto settings = project_->tools()->json();
        settings["experienceScalePercent"] = value;
        project_->setTools(settings);
    }
    Project *project_;
    int before_, after_;
};
class ChaosCommand final : public QUndoCommand {
  public:
    ChaosCommand(Project *project, bool value)
        : QUndoCommand("Change Chaos Frame counter"), project_(project),
          before_(project->tools()->chaosCounter().enabled()), after_(value) {}
    void undo() override {
        apply(before_);
    }
    void redo() override {
        apply(after_);
    }

  private:
    void apply(bool value) {
        auto settings = project_->tools()->json();
        settings["chaosFrameCounter"] = value;
        project_->setTools(settings);
    }
    Project *project_;
    bool before_, after_;
};
class HighAttackCommand final : public QUndoCommand {
  public:
    HighAttackCommand(Project *project, bool value)
        : QUndoCommand("Change High Attack stream split"), project_(project),
          before_(project->tools()->highAttack().enabled()), after_(value) {}
    void undo() override {
        apply(before_);
    }
    void redo() override {
        apply(after_);
    }

  private:
    void apply(bool value) {
        auto settings = project_->tools()->json();
        settings["highAttackStreamSplit"] = value;
        project_->setTools(settings);
    }
    Project *project_;
    bool before_, after_;
};
class DisplayCommand final : public QUndoCommand {
  public:
    DisplayCommand(Project *project, ToolLabel kind, bool value)
        : QUndoCommand(kind == ToolLabel::Alignment ? "Change squad alignment display"
                                                    : "Change character luck display"),
          project_(project), key_(DisplayTool::definition(kind)["setting"].toString()),
          before_(project->tools()->displayTool(kind).enabled()), after_(value) {}
    void undo() override {
        apply(before_);
    }
    void redo() override {
        apply(after_);
    }

  private:
    void apply(bool value) {
        auto settings = project_->tools()->json();
        settings[key_] = value;
        project_->setTools(settings);
    }
    Project *project_;
    QString key_;
    bool before_, after_;
};
} // namespace
ToolsEditor::ToolsEditor(Project *project, QUndoStack *undo, std::function<void()> changed,
                         QWidget *parent)
    : QWidget(parent), project_(project), undo_(undo), changed_(std::move(changed)) {
    setObjectName("toolsEditor");
    project_->loadTools();
    auto outer = new QVBoxLayout(this);
    auto scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto content = new QWidget;
    scroll->setWidget(content);
    auto pages=new QTabWidget; pages->setObjectName("toolsPages");
    outer->addWidget(pages); pages->addTab(scroll,"Battle and squads");
    auto layout = new QVBoxLayout(content);
    layout->addWidget(new GameRulesEditor(project_, undo_, changed_, {"deployment", "tactics", "fatigue", "recovery", "elements", "weapons", "resistance"}));
    auto page=[pages](const QString &name) {
        auto scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);
        auto body=new QWidget;scroll->setWidget(body);pages->addTab(scroll,name);return new QVBoxLayout(body);
    };
    auto statuses=page("Status effects");
    auto statusScope=new QLabel("These settings apply throughout the game to status effects from spells, abilities and weapon hits. "
                               "Retail restores the original value for each control.");
    statusScope->setWordWrap(true);statuses->addWidget(statusScope);
    statuses->addWidget(new GameRulesEditor(project_,undo_,changed_,
        {"statusPoison","statusPowerUp","statusPowerDown","statusSleep","statusParalysis","statusPetrification"}));
    statuses->addStretch();
    pages->addTab(new TrainingEditor(project_,undo_,changed_),"Training");
    auto rewards=page("Mission rewards");
    rewards->addWidget(new GameRulesEditor(project_,undo_,changed_,{"goth","soldierPopulation","soldierMinimum","reserve"}));
    rewards->addStretch();
    auto extras=page("Display and options");
    auto box = new QGroupBox("Battle experience");
    auto fields = new QVBoxLayout(box);
    auto description = new QLabel("Scale the size weight applied to every reward source. "
                                  "The baseline award, Manual of Warfare, minimum and 100 XP "
                                  "cap retain their existing behavior.");
    description->setWordWrap(true);
    fields->addWidget(description);
    auto form = new QFormLayout;
    percent_ = new QSpinBox;
    percent_->setObjectName("experienceScale");
    percent_->setRange(0, 300);
    percent_->setSuffix(" %");
    percent_->setKeyboardTracking(false);
    form->addRow("Size-weight scale", percent_);
    weights_ = new QLabel;
    weights_->setObjectName("experienceWeights");
    form->addRow("Effective weights", weights_);
    fields->addLayout(form);
    auto buttons = new QHBoxLayout;
    restore_ = new QPushButton("Restore loaded value");
    restore_->setObjectName("experienceRestore");
    retail_ = new QPushButton("Reset to retail (100%)");
    retail_->setObjectName("experienceRetail");
    buttons->addWidget(restore_);
    buttons->addWidget(retail_);
    buttons->addStretch();
    fields->addLayout(buttons);
    notice_ = new QLabel;
    notice_->setObjectName("toolsNotice");
    notice_->setWordWrap(true);
    fields->addWidget(notice_);
    layout->addWidget(box);
    auto chaosBox = new QGroupBox("Army Management");
    auto chaosLayout = new QVBoxLayout(chaosBox);
    chaos_ = new QCheckBox("Show the Chaos Frame counter");
    chaos_->setObjectName("chaosCounter");
    chaosLayout->addWidget(chaos_);
    chaosNotice_ = new QLabel;
    chaosNotice_->setObjectName("chaosNotice");
    chaosNotice_->setWordWrap(true);
    chaosLayout->addWidget(chaosNotice_);
    chaosRestore_ = new QPushButton("Restore loaded counter setting");
    chaosRestore_->setObjectName("chaosRestore");
    chaosLayout->addWidget(chaosRestore_, 0, Qt::AlignLeft);
    extras->addWidget(chaosBox);
    auto displayBox = new QGroupBox("Additional values — Expansion Pak required (8 MiB RAM)");
    auto displayLayout = new QVBoxLayout(displayBox);
    for (int i = 0; i < 2; ++i) {
        const auto kind = i ? ToolLabel::Alignment : ToolLabel::Luck;
        const QString name = i ? "alignment" : "luck";
        displayBoxes_[i] = new QCheckBox(i ? "Show average Alignment for visible squads"
                                           : "Show Luck on character cards");
        displayBoxes_[i]->setObjectName(name + "Display");
        displayNotices_[i] = new QLabel;
        displayNotices_[i]->setObjectName(name + "Notice");
        displayNotices_[i]->setWordWrap(true);
        displayRestores_[i] = new QPushButton("Restore loaded setting");
        displayRestores_[i]->setObjectName(name + "Restore");
        displayLayout->addWidget(displayBoxes_[i]);
        displayLayout->addWidget(displayNotices_[i]);
        displayLayout->addWidget(displayRestores_[i], 0, Qt::AlignLeft);
        connect(displayBoxes_[i], &QCheckBox::toggled, this,
                [this, kind](bool value) { setDisplay(kind, value); });
        connect(displayRestores_[i], &QPushButton::clicked, this, [this, kind] {
            setDisplay(kind, project_->tools()->displayTool(kind).loadedEnabled());
        });
    }
    extras->addWidget(displayBox);
    auto highBox = new QGroupBox("High Attack — experimental");
    auto highLayout = new QVBoxLayout(highBox);
    highAttack_ = new QCheckBox("Expand battle action streams");
    highAttack_->setObjectName("highAttack");
    highAttackNotice_ = new QLabel;
    highAttackNotice_->setObjectName("highAttackNotice");
    highAttackNotice_->setWordWrap(true);
    highAttackRestore_ = new QPushButton("Restore loaded setting");
    highAttackRestore_->setObjectName("highAttackRestore");
    highLayout->addWidget(highAttack_);
    highLayout->addWidget(highAttackNotice_);
    highLayout->addWidget(highAttackRestore_, 0, Qt::AlignLeft);
    extras->addWidget(highBox);
    connect(highAttack_, &QCheckBox::toggled, this, &ToolsEditor::setHighAttack);
    connect(highAttackRestore_, &QPushButton::clicked, this,
            [this] { setHighAttack(project_->tools()->highAttack().loadedEnabled()); });
    layout->addStretch();
    extras->addStretch();
    connect(percent_, &QSpinBox::valueChanged, this, &ToolsEditor::setPercent);
    connect(restore_, &QPushButton::clicked, this,
            [this] { setPercent(project_->tools()->loadedExperiencePercent()); });
    connect(retail_, &QPushButton::clicked, this, [this] { setPercent(100); });
    connect(chaos_, &QCheckBox::toggled, this, &ToolsEditor::setChaos);
    connect(chaosRestore_, &QPushButton::clicked, this,
            [this] { setChaos(project_->tools()->chaosCounter().loadedEnabled()); });
    connect(undo_, &QUndoStack::indexChanged, this, [this] {
        refresh();
        changed_();
    });
    refresh();
}
void ToolsEditor::setPercent(int value) {
    if (!project_->tools()->available() || value == project_->tools()->experiencePercent())
        return;
    undo_->push(new ExperienceCommand(project_, value));
}
void ToolsEditor::refresh() {
    const ViewPosition keepView(this);
    const auto &settings = *project_->tools();
    const auto &high = settings.highAttack();
    const QSignalBlocker highBlock(highAttack_);
    highAttack_->setChecked(high.enabled());
    highAttack_->setEnabled(high.available());
    highAttackRestore_->setEnabled(high.available() && high.modified());
    highAttackNotice_->setText(
        high.available()
            ? "Requires an Expansion Pak (8 MiB RAM) and Project64's Interpreter CPU core. "
              "The default recompiler is unsupported. Full-battle validation remains pending. "
              "Edit per-class attack counts in Classes."
            : high.unavailableReason());
    const QSignalBlocker block(percent_);
    percent_->setValue(settings.experiencePercent());
    percent_->setEnabled(settings.available());
    restore_->setEnabled(settings.available() && settings.experienceModified());
    retail_->setEnabled(settings.available() && settings.experiencePercent() != 100);
    weights_->setText(settings.available()
                          ? QString("Size 0: %1% · Size 1/default: %2% · Size 2: %3%")
                                .arg(settings.experiencePercent() * .8, 0, 'f', 1)
                                .arg(settings.experiencePercent())
                                .arg(settings.experiencePercent() * 1.5, 0, 'f', 1)
                          : QString("Unavailable"));
    notice_->setText(settings.available() ? "100% restores the retail formula. At 0%, baseline and "
                                            "minimum awards can still give XP."
                                          : settings.unavailableReason());
    const auto &counter = settings.chaosCounter();
    const QSignalBlocker counterBlock(chaos_);
    chaos_->setChecked(counter.enabled());
    chaos_->setEnabled(counter.available());
    chaosRestore_->setEnabled(counter.available() && counter.modified());
    chaosNotice_->setText(counter.available()
                              ? "Display the current Chaos Frame on the Army roster. Requires an "
                                "Expansion Pak (8 MiB RAM). "
                                "Turning this off removes the counter."
                              : counter.unavailableReason());
    for (int i = 0; i < 2; ++i) {
        const auto &display = settings.displayTool(i ? ToolLabel::Alignment : ToolLabel::Luck);
        const QSignalBlocker guard(displayBoxes_[i]);
        displayBoxes_[i]->setChecked(display.enabled());
        displayBoxes_[i]->setEnabled(display.available());
        displayRestores_[i]->setEnabled(display.available() && display.modified());
        displayNotices_[i]->setText(
            display.available()
                ? (i ? "Displays the average of occupied squad members for up to six visible rows. "
                       "Empty squads display 50."
                     : "Displays the selected character's Luck beside the native card details.")
                : display.unavailableReason());
    }
}
void ToolsEditor::setChaos(bool enabled) {
    const auto &counter = project_->tools()->chaosCounter();
    if (!counter.available() || counter.enabled() == enabled)
        return;
    undo_->push(new ChaosCommand(project_, enabled));
}
void ToolsEditor::setHighAttack(bool enabled) {
    const auto &setting = project_->tools()->highAttack();
    if (!setting.available() || setting.enabled() == enabled)
        return;
    undo_->push(new HighAttackCommand(project_, enabled));
}
void ToolsEditor::setDisplay(ToolLabel kind, bool enabled) {
    const auto &setting = project_->tools()->displayTool(kind);
    if (!setting.available() || setting.enabled() == enabled)
        return;
    undo_->push(new DisplayCommand(project_, kind, enabled));
}
} // namespace lordly
