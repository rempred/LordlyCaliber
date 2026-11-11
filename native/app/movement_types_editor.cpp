#include "app/movement_types_editor.h"
#include "engine/encounters.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>
namespace lordly {
namespace {
class MovementCommand final : public QUndoCommand {
  public:
    MovementCommand(Project *p, QString label, QJsonObject after)
        : QUndoCommand(label), p_(p), before_(p->movementTypes().json()), after_(std::move(after)) {
    }
    void undo() override {
        p_->setMovementTypes(before_);
    }
    void redo() override {
        p_->setMovementTypes(after_);
    }

  private:
    Project *p_;
    QJsonObject before_, after_;
};
} // namespace
std::optional<MovementCreation> requestMovementType(QWidget *parent, const Project &p, int source) {
    QDialog d(parent);
    d.setWindowTitle("New movement type");
    d.setObjectName("newMovementTypeDialog");
    auto v = new QVBoxLayout(&d);
    auto form = new QFormLayout;
    v->addLayout(form);
    auto name = new QLineEdit;
    name->setObjectName("newMovementName");
    name->setMaxLength(80);
    auto copy = new QComboBox;
    copy->setObjectName("newMovementCopy");
    for (auto i = p.movementTypes().entries().begin(); i != p.movementTypes().entries().end(); ++i)
        copy->addItem(i->name, i.key());
    copy->setCurrentIndex(qMax(0, copy->findData(source)));
    auto speed = new QSpinBox;
    speed->setObjectName("newMovementSpeed");
    speed->setRange(1, 255);
    speed->setValue(p.movementTypes().entries()[copy->currentData().toInt()].baseSpeed);
    form->addRow("Name", name);
    form->addRow("Copy terrain costs from", copy);
    form->addRow("Base speed", speed);
    QObject::connect(copy, &QComboBox::activated, &d, [&] {
        speed->setValue(p.movementTypes().entries()[copy->currentData().toInt()].baseSpeed);
    });
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    v->addWidget(buttons);
    buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    QObject::connect(name, &QLineEdit::textChanged, &d, [&] {
        buttons->button(QDialogButtonBox::Ok)->setEnabled(!name->text().trimmed().isEmpty());
    });
    QObject::connect(buttons, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    name->setFocus();
    if (d.exec() != QDialog::Accepted)
        return {};
    return MovementCreation{name->text().trimmed(), copy->currentData().toInt(), speed->value()};
}
MovementTypesEditor::MovementTypesEditor(Project *p, QUndoStack *u, std::function<void()> c,
                                         QWidget *parent)
    : QWidget(parent), project_(p), undo_(u), changed_(std::move(c)) {
    setObjectName("movementTypesEditor");
    auto outer = new QVBoxLayout(this);
    auto split = new QSplitter;
    outer->addWidget(split);
    auto left = new QWidget;
    auto l = new QVBoxLayout(left);
    l->addWidget(new QLabel("Squad priority — first matching type wins"));
    list_ = new QListWidget;
    list_->setObjectName("movementTypesList");
    l->addWidget(list_);
    auto buttons = new QHBoxLayout;
    l->addLayout(buttons);
    auto add = new QPushButton("New movement type…"), remove = new QPushButton("Delete"),
         up = new QPushButton("Up"), down = new QPushButton("Down");
    add->setObjectName("addMovementType");
    remove->setObjectName("deleteMovementType");
    up->setObjectName("movementPriorityUp");
    down->setObjectName("movementPriorityDown");
    for (auto b : {add, remove, up, down})
        buttons->addWidget(b);
    split->addWidget(left);
    auto right = new QWidget;
    auto r = new QVBoxLayout(right);
    auto form = new QFormLayout;
    r->addLayout(form);
    name_ = new QLineEdit;
    name_->setObjectName("movementTypeName");
    name_->setMaxLength(80);
    speed_ = new QSpinBox;
    speed_->setObjectName("movementBaseSpeed");
    speed_->setRange(1, 255);
    speed_->setKeyboardTracking(false);
    speed_->setToolTip("Base movement speed before the terrain-cost curve. Higher values move faster; the terrain table shows the resulting speed factors.");
    behavior_ = new QComboBox;
    behavior_->setObjectName("movementBehavior");
    behavior_->addItem("Ground", 2);
    behavior_->addItem("Flying", 1);
    behavior_->addItem("Immobile", 7);
    form->addRow("Name", name_);
    form->addRow("Base speed", speed_);
    form->addRow("Travel behavior", behavior_);
    terrain_ = new QTableWidget(32, 4);
    terrain_->setObjectName("movementTerrainCosts");
    terrain_->setHorizontalHeaderLabels({"Terrain", "Passable", "Cost", "Speed factor"});
    terrain_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    terrain_->verticalHeader()->hide();
    r->addWidget(terrain_);
    for (int i = 0; i < 32; ++i) {
        auto item = new QTableWidgetItem(Encounters::mapTerrainName(i) + QString(" [%1]").arg(i));
        item->setFlags(Qt::ItemIsEnabled);
        terrain_->setItem(i, 0, item);
        auto pass = new QCheckBox;
        pass->setObjectName(QString("movementPassable%1").arg(i));
        terrain_->setCellWidget(i, 1, pass);
        auto cost = new QSpinBox;
        cost->setRange(1, 15);
        cost->setKeyboardTracking(false);
        cost->setObjectName(QString("movementCost%1").arg(i));
        cost->setToolTip("Terrain costs 1–15 use the game's terrain curve. Higher costs reduce "
                         "speed. Passability is controlled separately.");
        terrain_->setCellWidget(i, 2, cost);
        auto value = new QTableWidgetItem;
        value->setFlags(Qt::ItemIsEnabled);
        terrain_->setItem(i, 3, value);
        connect(pass, &QCheckBox::toggled, this, [this, i, cost](bool yes) {
            edit("Change movement passability",
                 [=](MovementType &t) { t.costs[i] = yes ? qMax(1, cost->value()) : 65535; });
        });
        connect(cost, &QSpinBox::valueChanged, this, [this, i](int value) {
            edit("Change terrain cost", [=](MovementType &t) { t.costs[i] = value; });
        });
    }
    split->addWidget(right);
    split->setStretchFactor(1, 1);
    connect(list_, &QListWidget::currentRowChanged, this, [this] { refresh(); });
    connect(name_, &QLineEdit::editingFinished, this, [this] {
        edit("Rename movement type", [this](MovementType &t) { t.name = name_->text().trimmed(); });
    });
    connect(speed_, &QSpinBox::valueChanged, this, [this](int n) {
        edit("Change movement speed", [=](MovementType &t) { t.baseSpeed = n; });
    });
    connect(behavior_, &QComboBox::activated, this, [this] {
        edit("Change travel behavior",
             [this](MovementType &t) { t.behavior = behavior_->currentData().toInt(); });
    });
    connect(add, &QPushButton::clicked, this, [this] {
        try {
            if (auto request = requestMovementType(this, *project_, selected())) {
                auto next = project_->movementTypes();
                int id = next.create(request->name, request->copyFrom);
                auto t = next.entries()[id];
                t.baseSpeed = request->baseSpeed;
                next.set(id, t);
                apply("Create movement type", next);
                for (int i = 0; i < list_->count(); ++i)
                    if (list_->item(i)->data(Qt::UserRole).toInt() == id)
                        list_->setCurrentRow(i);
            }
        } catch (const Error &e) {
            QMessageBox::warning(this, "Movement type", e.what());
        }
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        try {
            auto next = *project_;
            next.deleteMovementType(selected());
            apply("Delete movement type", next.movementTypes());
        } catch (const Error &e) {
            QMessageBox::warning(this, "Movement type", e.what());
        }
    });
    for (auto pair : {qMakePair(up, -1), qMakePair(down, 1)})
        connect(pair.first, &QPushButton::clicked, this, [this, delta = pair.second] {
            auto next = project_->movementTypes();
            auto order = next.priority();
            int i = order.indexOf(selected()), j = i + delta;
            if (i < 0 || j < 0 || j >= order.size())
                return;
            order.swapItemsAt(i, j);
            next.setPriority(order);
            apply("Change movement priority", next);
        });
    connect(undo_, &QUndoStack::indexChanged, this, [this] { refresh(); });
    refresh();
    setEnabled(!project_->rom().revision());
}
int MovementTypesEditor::selected() const {
    return list_->currentItem() ? list_->currentItem()->data(Qt::UserRole).toInt() : 2;
}
void MovementTypesEditor::apply(const QString &label, const MovementTypes &m) {
    if (m.json() == project_->movementTypes().json())
        return;
    undo_->push(new MovementCommand(project_, label, m.json()));
    refresh();
    changed_();
}
void MovementTypesEditor::edit(const QString &label,
                               const std::function<void(MovementType &)> &fn) {
    try {
        auto m = project_->movementTypes();
        int id = selected();
        auto t = m.entries()[id];
        fn(t);
        m.set(id, t);
        apply(label, m);
    } catch (const Error &e) {
        QMessageBox::warning(this, "Movement type", e.what());
        refresh();
    }
}
void MovementTypesEditor::refresh() {
    int id = selected();
    QSignalBlocker b(list_);
    list_->clear();
    const auto &m = project_->movementTypes();
    for (int n : m.priority()) {
        auto item = new QListWidgetItem(QString("%1 · speed %2").arg(m.entries()[n].name).arg(m.entries()[n].baseSpeed), list_);
        item->setData(Qt::UserRole, n);
        if (n == id)
            list_->setCurrentItem(item);
    }
    if (!list_->currentItem())
        list_->setCurrentRow(0);
    id = selected();
    if (!m.entries().contains(id))
        return;
    const auto &t = m.entries()[id];
    QSignalBlocker a(name_), s(speed_), behavior(behavior_);
    name_->setText(t.name);
    speed_->setValue(t.baseSpeed);
    behavior_->setCurrentIndex(behavior_->findData(t.behavior == 1 ? 1 : t.behavior == 7 ? 7 : 2));
    for (int i = 0; i < 32; i++) {
        auto pass = static_cast<QCheckBox *>(terrain_->cellWidget(i, 1));
        auto cost = static_cast<QSpinBox *>(terrain_->cellWidget(i, 2));
        QSignalBlocker p(pass), c(cost);
        pass->setChecked(t.costs[i] != 65535);
        cost->setEnabled(pass->isChecked());
        const int value = pass->isChecked() ? t.costs[i] : 1;
        cost->setRange(qMin(1, value), qMax(15, value));
        cost->setValue(value);
        terrain_->item(i, 3)->setText(m.speed(id, i) > 0 ? QString::number(m.speed(id, i), 'g', 5)
                                                         : "—");
    }
    findChild<QPushButton *>("deleteMovementType")->setEnabled(id > 7);
}
} // namespace lordly
