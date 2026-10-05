#include "app/tools_editor.h"
#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
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
} // namespace
ToolsEditor::ToolsEditor(Project *project, QUndoStack *undo, std::function<void()> changed,
                         QWidget *parent)
    : QWidget(parent), project_(project), undo_(undo), changed_(std::move(changed)) {
    setObjectName("toolsEditor");
    project_->loadTools();
    auto layout = new QVBoxLayout(this);
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
    layout->addWidget(chaosBox);
    layout->addStretch();
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
    const auto &settings = *project_->tools();
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
                                "Turning this off restores the original task-submit call."
                              : counter.unavailableReason());
}
void ToolsEditor::setChaos(bool enabled) {
    const auto &counter = project_->tools()->chaosCounter();
    if (!counter.available() || counter.enabled() == enabled)
        return;
    undo_->push(new ChaosCommand(project_, enabled));
}
} // namespace lordly
