#include "app/squad_order_editor.h"
#include <QComboBox>
#include <QFormLayout>
#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
namespace lordly {
SquadOrderEditor::SquadOrderEditor(Project *project,
    std::function<void(const Scenarios &, const QString &)> commit)
    : project_(project), commit_(std::move(commit)) {
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    current_ = new QLabel;
    current_->setWordWrap(true);
    layout->addWidget(current_);
    auto form = new QFormLayout;
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    command_ = new QComboBox;
    command_->setObjectName("squadCommand");
    command_->addItem("Keep current scripted orders", -1);
    for (const auto &label : {"Guard this position", "Intercept nearby enemies", "Move",
                              "Move and attack", "Hit and run", "Evade", "Retreat"})
        command_->addItem(label, command_->count() - 1);
    form->addRow("Command", command_);
    destinations_ = new QWidget;
    auto destinations = new QFormLayout(destinations_);
    destinations->setContentsMargins(0, 0, 0, 0);
    destinations->setRowWrapPolicy(QFormLayout::WrapLongRows);
    destination_ = new QComboBox;
    destination_->setObjectName("squadCommandDestination");
    destinations->addRow("Destination", destination_);
    coordinates_ = new QWidget;
    auto coordinates = new QHBoxLayout(coordinates_);
    coordinates->setContentsMargins(0, 0, 0, 0);
    x_ = new QSpinBox;
    z_ = new QSpinBox;
    x_->setObjectName("squadCommandX"); z_->setObjectName("squadCommandZ");
    x_->setRange(1, 255); z_->setRange(1, 255);
    coordinates->addWidget(new QLabel("X")); coordinates->addWidget(x_);
    coordinates->addWidget(new QLabel("Z")); coordinates->addWidget(z_);
    coordinates->addStretch();
    destinations->addRow("Map position", coordinates_);
    pick_ = new QPushButton("Choose destination on map");
    pick_->setObjectName("squadCommandPick");
    pick_->setCheckable(true);
    pick_->hide();
    destinations->addRow(pick_);
    form->addRow(destinations_);
    layout->addLayout(form);
    help_ = new QLabel;
    help_->setObjectName("squadCommandHelp"); help_->setWordWrap(true);
    layout->addWidget(help_);
    apply_ = new QPushButton("Apply command to squad");
    apply_->setObjectName("squadCommandApply");
    layout->addWidget(apply_);
    notice_ = new QLabel;
    notice_->setWordWrap(true);
    notice_->setObjectName("squadCommandNotice");
    layout->addWidget(notice_);
    connect(command_, &QComboBox::currentIndexChanged, this, [this] { cancelPick(); controls(); });
    connect(destination_, &QComboBox::currentIndexChanged, this, [this] { cancelPick(); controls(); });
    connect(pick_, &QPushButton::toggled, this, [this](bool on) {
        pick_->setText(on ? "Cancel destination selection" : "Choose destination on map");
        if (picking_) picking_(on);
    });
    connect(apply_, &QPushButton::clicked, this, [this] {
        if (key_ < 0 || squad_ < 0 || command_->currentData().toInt() < 0) return;
        cancelPick();
        try {
            auto next = *project_->scenarios();
            const int point = destination_->currentData().toInt();
            next.setSquadCommand(key_, squad_, {SquadBehavior(command_->currentData().toInt()),
                point ? point : x_->value(), point ? 0 : z_->value()}, project_->classData());
            commit_(next, "Change squad command");
            notice_->setText("Command applied.");
        } catch (const Error &e) { notice_->setText(QString::fromUtf8(e.what())); }
    });
}
void SquadOrderEditor::enableMapPicking(std::function<void(bool)> callback) {
    picking_ = std::move(callback);
    pick_->setVisible(bool(picking_));
}
void SquadOrderEditor::cancelPick() { pick_->setChecked(false); }
void SquadOrderEditor::hideEvent(QHideEvent *event) {
    cancelPick();
    QWidget::hideEvent(event);
}
void SquadOrderEditor::setDestination(QPoint point) {
    destination_->setCurrentIndex(destination_->findData(0));
    x_->setValue(point.x()); z_->setValue(point.y());
    cancelPick();
    notice_->setText("Destination selected. Apply the command to save it.");
}
void SquadOrderEditor::refresh(int key, int squad) {
    cancelPick(); key_ = key; squad_ = squad;
    setEnabled(key >= 0 && squad >= 0);
    if (key < 0 || squad < 0) return;
    const QSignalBlocker a(command_), b(destination_);
    destination_->clear(); destination_->addItem("Map position", 0);
    if (project_->world()) {
        const auto &world = *project_->world();
        const auto &sites = world.scenario(key).sites;
        for (int i = 0; i < world.siteCount(key); ++i)
            destination_->addItem(world.townName(u16(sites, 1 + i * 4)), i + 1);
    }
    const auto command = project_->scenarios()->squadCommand(key, squad);
    command_->setCurrentIndex(command_->findData(command ? int(command->behavior) : -1));
    const auto &deployment = project_->scenarios()->scenario(key).layout.deployments[squad];
    const int x = command ? command->x : u8(deployment, 3);
    const int z = command ? command->z : u8(deployment, 4);
    if (!z && x) {
        if (destination_->findData(x) < 0) destination_->addItem(QString("Scenario point %1").arg(x), x);
        destination_->setCurrentIndex(destination_->findData(x));
    } else destination_->setCurrentIndex(0);
    x_->setValue(z ? x : 128); z_->setValue(z ? z : 128);
    current_->setText(command ? "Current order: " + command_->currentText() +
        (int(command->behavior) >= int(SquadBehavior::Move) ? " → " +
            (z ? QString("X %1, Z %2").arg(x).arg(z) : destination_->currentText()) : "")
        : "Current orders: scripted route. These stay unchanged until you apply a new command.");
    notice_->clear(); controls();
}
void SquadOrderEditor::controls() {
    const int action = command_->currentData().toInt();
    destinations_->setVisible(action >= int(SquadBehavior::Move));
    static_cast<QFormLayout *>(destinations_->layout())->setRowVisible(coordinates_, destination_->currentData().toInt() == 0);
    const QStringList help{
        "Stay at the starting position without seeking nearby enemies. Battles can still occur on contact.",
        "Wait here and pursue nearby enemies when the game's target checks allow it.",
        "Move to the destination without seeking nearby enemies. Battles can still occur on contact.",
        "Move to the destination and pursue nearby enemies when the game's target checks allow it.",
        "Move to the destination using Hit and Run movement and seek nearby enemies.",
        "Move to the destination using Evasion movement without seeking nearby enemies.",
        "Use the game's retreat movement toward the destination."};
    QString text = action < 0 ? "Choose a command to replace this squad's starting orders. Detailed scripts are available in Orders → Advanced." : help.value(action);
    if (action >= 0) text += " Applying replaces this squad's starting orders. Undo restores them.";
    bool follower = false;
    if (key_ >= 0 && squad_ >= 0) {
        const auto &d = project_->scenarios()->scenario(key_).layout.deployments;
        follower = squad_ > 1 && u8(d[squad_ - 1], 16) && d[squad_ - 1][16] == d[squad_ - 2][16];
    }
    if (follower) text = "This squad follows its movement group. Choose its group leader, or make it independent in Advanced.";
    help_->setText(text);
    apply_->setEnabled(action >= 0 && !follower && squad_ >= 0);
    pick_->setEnabled(action >= int(SquadBehavior::Move) && !follower && squad_ >= 0);
}
}
