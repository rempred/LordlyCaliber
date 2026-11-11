#include "app/movement_types_editor.h"
#include "engine/encounters.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
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
struct SwitchRule {const char *key,*label,*help;bool MovementRules::*member;};
const SwitchRule switchRules[] = {
    {"canMove","Can move","Allow squads with this movement type to travel.",&MovementRules::canMove},
    {"mapFlight","Fly above the map","Use the game's flying height and map presentation. Terrain passability remains editable below.",&MovementRules::mapFlight},
    {"crossRegions","Find supplies across ground-region barriers","Allow supply destinations across the mission's ground-region boundaries.",&MovementRules::crossRegions},
    {"airborne","Airborne in battle","Use flying battle movement and the game's airborne attack checks. Does not create new sprite artwork.",&MovementRules::airborne},
    {"forcedPlain","Use Plain terrain for forced retreats","Use the Plain type's terrain permissions during forced retreat movement, as original Sky and Immobile types do.",&MovementRules::forcedPlain}
};
struct RateRule {const char *key,*label,*help;int MovementRules::*member;};
const RateRule rateRules[] = {
    {"fatigue","Marching fatigue","Counter gain per movement update. Original ground 2; Sky 3. At the global cycle threshold, eligible members gain 1 fatigue. 0 disables this contribution.",&MovementRules::fatigue},
    {"groupFatigue","Legion marching fatigue","Counter gain while moving in a Legion. Original value 2. Marching Baton halves the base gain, rounded down.",&MovementRules::groupFatigue},
    {"nightFatigue","Extra nighttime fatigue","Additional counter gain from 18:00 to 05:59. Original value 1.",&MovementRules::nightFatigue},
    {"recovery","Rest recovery","Counter reduction per eligible rest update. Original value 3; Healing Censer doubles it. Each completed cycle restores fatigue and HP using the game rules.",&MovementRules::recovery},
    {"nightRecovery","Extra nighttime recovery","Additional rest counter reduction from 18:00 to 05:59. Original value 2.",&MovementRules::nightRecovery}
};
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
    Q_UNUSED(source);
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
    copy->addItem("Neutral settings",0);
    for (auto i = p.movementTypes().entries().begin(); i != p.movementTypes().entries().end(); ++i)
        copy->addItem(i->name, i.key());
    copy->setCurrentIndex(0);
    auto speed = new QSpinBox;
    speed->setObjectName("newMovementSpeed");
    speed->setRange(1, 255);
    speed->setValue(35);
    form->addRow("Name", name);
    form->addRow("Start with", copy);
    form->addRow("Base speed", speed);
    QObject::connect(copy, &QComboBox::activated, &d, [&] {
        const int id=copy->currentData().toInt();
        speed->setValue(id?p.movementTypes().entries()[id].baseSpeed:35);
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
    form->addRow("Name", name_);
    form->addRow("Base speed", speed_);
    auto rulesBox=new QGroupBox("Movement rules");
    auto rulesGrid=new QGridLayout(rulesBox);
    int ruleIndex=0;
    for (const auto &rule:switchRules) {
        auto check=new QCheckBox(QString::fromUtf8(rule.label));
        check->setObjectName("movementRule_"+QString::fromLatin1(rule.key));
        check->setToolTip(QString::fromUtf8(rule.help));
        switches_[rule.key]=check;
        rulesGrid->addWidget(check,ruleIndex/2,ruleIndex%2);++ruleIndex;
        connect(check,&QCheckBox::toggled,this,[this,rule](bool v){edit("Change movement rule",[=](MovementType &t){t.rules.*(rule.member)=v;});});
    }
    profile_=new QComboBox;profile_->setObjectName("movementFlightProfile");
    profile_->addItems({"Class flight path (safe default for new fliers)","Angel Knight / Hawkman / Wyrm","Sphinx","Cockatrice","Griffin / Opinincus"});
    profile_->setToolTip("Battle advance and retreat path. Unsupported class paths use the Angel Knight path instead of freezing the game.");
    rulesGrid->addWidget(new QLabel("Battle flight path"),3,0);rulesGrid->addWidget(profile_,3,1);
    connect(profile_,&QComboBox::activated,this,[this](int n){edit("Change battle flight path",[=](MovementType &t){t.rules.flightProfile=n;});});
    r->addWidget(rulesBox);
    auto rateBox=new QGroupBox("Fatigue and recovery");auto rateGrid=new QGridLayout(rateBox);
    int rateIndex=0;
    for (const auto &rule:rateRules) {
        auto spin=new QSpinBox;spin->setRange(0,30);spin->setKeyboardTracking(false);
        spin->setObjectName("movementRule_"+QString::fromLatin1(rule.key));spin->setToolTip(QString::fromUtf8(rule.help));
        rates_[rule.key]=spin;
        rateGrid->addWidget(new QLabel(QString::fromUtf8(rule.label)),rateIndex/2,(rateIndex%2)*2);
        rateGrid->addWidget(spin,rateIndex/2,(rateIndex%2)*2+1);++rateIndex;
        connect(spin,&QSpinBox::valueChanged,this,[this,rule](int v){edit("Change movement cycle rate",[=](MovementType &t){t.rules.*(rule.member)=v;});});
    }
    auto rateHelp=new QLabel("Rates are counter points per update. Higher fatigue values tire squads sooner; higher recovery values restore them sooner. Cycle lengths use Tools' global fatigue and recovery settings.");
    rateHelp->setWordWrap(true);rateGrid->addWidget(rateHelp,3,0,1,4);
    r->addWidget(rateBox);
    formula_=new QLabel;formula_->setObjectName("movementFormula");formula_->setWordWrap(true);r->addWidget(formula_);
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
    QSignalBlocker a(name_), s(speed_), profile(profile_);
    name_->setText(t.name);
    speed_->setValue(t.baseSpeed);
    for(const auto &rule:switchRules){auto check=switches_[rule.key];QSignalBlocker block(check);check->setChecked(t.rules.*(rule.member));}
    for(const auto &rule:rateRules){auto spin=rates_[rule.key];QSignalBlocker block(spin);spin->setValue(t.rules.*(rule.member));}
    profile_->setCurrentIndex(t.rules.flightProfile);profile_->setEnabled(t.rules.airborne);
    bool linear=true;for(int c=1;c<16;++c)linear &= m.curve()[c]==float(c);
    formula_->setText(linear
        ? QString("Ordinary speed factor = %1 − 2 × terrain cost. Cost 1 gives %2; cost 4 gives %3. For example, base 35/cost 4 gives 27; base 70/cost 8 gives 54 (double). Turning and Legion size also affect travel.").arg(t.baseSpeed).arg(qMax(1,t.baseSpeed-2)).arg(qMax(1,t.baseSpeed-8))
        : QString("Ordinary speed factor = %1 − 2 × the ROM's terrain-cost curve. The table shows the result for this ROM. Turning and Legion size also affect travel.").arg(t.baseSpeed));
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
