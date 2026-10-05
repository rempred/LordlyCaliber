#include "app/tools_editor.h"
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
    layout->addStretch();
    connect(percent_, &QSpinBox::valueChanged, this, &ToolsEditor::setPercent);
    connect(restore_, &QPushButton::clicked, this,
            [this] { setPercent(project_->tools()->loadedExperiencePercent()); });
    connect(retail_, &QPushButton::clicked, this, [this] { setPercent(100); });
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
    restore_->setEnabled(settings.available() && settings.modified());
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
}
} // namespace lordly
