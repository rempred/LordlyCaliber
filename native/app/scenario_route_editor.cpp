#include "app/scenario_route_editor.h"
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QVBoxLayout>
#include <algorithm>
namespace lordly {
namespace {
QComboBox *box(const char *name, const QStringList &labels = {}) {
    auto result = new QComboBox;
    result->setObjectName(name);
    for (int i = 0; i < labels.size(); ++i)
        result->addItem(labels[i], i);
    return result;
}
QSpinBox *byte(const QString &name) {
    auto result = new QSpinBox;
    result->setObjectName(name);
    result->setRange(0, 255);
    return result;
}
void select(QComboBox *box, int value) {
    int at = box->findData(value);
    if (at < 0) {
        box->addItem(QString("%1 · Preserved original value").arg(value), value);
        at = box->count() - 1;
    }
    box->setCurrentIndex(at);
}
QString kindName(int kind) {
    switch (kind) {
    case 1:
        return "Object rectangle and state";
    case 4:
        return "Target index and state";
    case 8:
        return "Watched object rectangle and state";
    case 9:
        return "Active source count";
    case 12:
        return "Site state flag";
    default:
        return QString("Original kind %1").arg(kind);
    }
}
} // namespace
ScenarioRouteEditor::ScenarioRouteEditor(
    Project *project, std::function<void(const Scenarios &, const QString &)> commit,
    QWidget *parent)
    : QWidget(parent), project_(project), commit_(std::move(commit)) {
    setObjectName("scenarioRoutesEditor");
    auto layout = new QVBoxLayout(this);
    auto tabs = new QTabWidget;
    tabs->setObjectName("scenarioRouteTabs");
    layout->addWidget(tabs, 1);
    auto nodePage = new QWidget;
    auto nodeLayout = new QVBoxLayout(nodePage);
    auto split = new QSplitter;
    nodeLayout->addWidget(split, 1);
    nodes_ = new QListWidget;
    nodes_->setObjectName("scenarioNodes");
    split->addWidget(nodes_);
    nodeDetails_ = new QWidget;
    auto nodeForm = new QFormLayout(nodeDetails_);
    nodeKind_ = box("routeKind", {"Direct order", "Waypoint", "Dormant gate"});
    nodeForm->addRow("Kind", nodeKind_);
    move_ = box("routeMove", {"Direct", "Hit and Run", "Evasion"});
    wait_ = box("routeWait", {"Guard", "Initiate", "Retreat"});
    nodeForm->addRow("Movement order", move_);
    nodeForm->addRow("Wait order", wait_);
    placement_ = box("routePlacement", {"Scenario point", "Map coordinates"});
    nodeForm->addRow("Waypoint destination", placement_);
    x_ = byte("routeX");
    z_ = byte("routeZ");
    z_->setMinimum(1);
    nodeForm->addRow("Point / X", x_);
    nodeForm->addRow("Z", z_);
    conditionA_ = box("routeConditionA");
    conditionB_ = box("routeConditionB");
    gate_ = box("routeGate", {"Single condition", "Branch choice", "AND", "OR"});
    nodeForm->addRow("Condition A", conditionA_);
    nodeForm->addRow("Gate", gate_);
    nodeForm->addRow("Condition B", conditionB_);
    next_ = box("routeNext");
    fork_ = box("routeFork");
    nodeForm->addRow("Next node / branch B", next_);
    nodeForm->addRow("Branch A node", fork_);
    extension_ = new QLabel;
    extension_->setWordWrap(true);
    nodeForm->addRow(extension_);
    auto nodeApply = new QPushButton("Apply node");
    nodeApply->setObjectName("routeNodeApply");
    nodeForm->addRow(nodeApply);
    split->addWidget(nodeDetails_);
    split->setSizes({330, 800});
    auto actions = new QHBoxLayout;
    auto addNode = new QPushButton("Add waypoint");
    addNode->setObjectName("routeNodeAdd");
    auto removeNode = new QPushButton("Remove node");
    removeNode->setObjectName("routeNodeRemove");
    actions->addWidget(addNode);
    actions->addWidget(removeNode);
    actions->addStretch();
    nodeLayout->addLayout(actions);
    tabs->addTab(nodePage, "Movement nodes");
    auto conditionPage = new QWidget;
    auto conditionLayout = new QVBoxLayout(conditionPage);
    auto conditionSplit = new QSplitter;
    conditionLayout->addWidget(conditionSplit, 1);
    conditions_ = new QListWidget;
    conditions_->setObjectName("scenarioConditions");
    conditionSplit->addWidget(conditions_);
    conditionDetails_ = new QWidget;
    auto conditionForm = new QFormLayout(conditionDetails_);
    conditionKind_ = box("routeConditionKind");
    for (const int kind : {1, 4, 8, 9, 12})
        conditionKind_->addItem(kindName(kind), kind);
    conditionForm->addRow("Predicate", conditionKind_);
    for (int at = 2; at < 10; ++at) {
        auto input = byte(QString("routePayload_%1").arg(at));
        auto label = new QLabel;
        payload_ << input;
        payloadLabels_ << label;
        conditionForm->addRow(label, input);
    }
    conditionNote_ =
        new QLabel("Unnamed payload fields retain their loaded values. Changing the "
                   "predicate resets its payload; review the new values before applying.");
    conditionNote_->setWordWrap(true);
    conditionForm->addRow(conditionNote_);
    auto conditionApply = new QPushButton("Apply condition");
    conditionApply->setObjectName("routeConditionApply");
    conditionForm->addRow(conditionApply);
    conditionSplit->addWidget(conditionDetails_);
    conditionSplit->setSizes({330, 800});
    auto conditionActions = new QHBoxLayout;
    auto addCondition = new QPushButton("Add count condition");
    addCondition->setObjectName("routeConditionAdd");
    deleteCondition_ = new QPushButton("Remove added condition");
    deleteCondition_->setObjectName("routeConditionRemove");
    conditionActions->addWidget(addCondition);
    conditionActions->addWidget(deleteCondition_);
    conditionActions->addStretch();
    conditionLayout->addLayout(conditionActions);
    tabs->addTab(conditionPage, "Conditions");
    auto groupPage = new QWidget;
    auto groupForm = new QFormLayout(groupPage);
    groupSquad_ = box("routeGroupSquad");
    group_ = byte("routeGroup");
    group_->setSpecialValueText("Independent");
    pattern_ = box("routePattern");
    for (const int p : {1, 5, 6, 7, 8})
        pattern_->addItem(QString("Pattern %1").arg(p), p);
    groupForm->addRow("Squad", groupSquad_);
    groupForm->addRow("Group label", group_);
    groupForm->addRow("Movement pattern", pattern_);
    auto help = new QLabel("Consecutive squads with the same nonzero label form one group. "
                           "The group can contain five squads; each scenario can have ten groups. "
                           "The first deployment does not use a preceding group record. Group "
                           "patterns affect followers after initial placement.");
    help->setWordWrap(true);
    groupForm->addRow(help);
    auto groupApply = new QPushButton("Apply movement group");
    groupApply->setObjectName("routeGroupApply");
    groupForm->addRow(groupApply);
    tabs->addTab(groupPage, "Squad groups");
    notice_ = new QLabel;
    notice_->setObjectName("routeNotice");
    notice_->setWordWrap(true);
    layout->addWidget(notice_);
    connect(nodes_, &QListWidget::currentRowChanged, this, [this] { showNode(); });
    connect(conditions_, &QListWidget::currentRowChanged, this, [this] { showCondition(); });
    connect(groupSquad_, &QComboBox::currentIndexChanged, this, [this] { showGroup(); });
    for (auto input : {nodeKind_, placement_, gate_})
        connect(input, &QComboBox::currentIndexChanged, this, [this] { nodeControls(); });
    connect(conditionKind_, &QComboBox::currentIndexChanged, this, [this] {
        if (loading_)
            return;
        for (auto input : payload_)
            input->setValue(0);
        if (conditionKind_->currentData().toInt() == 9)
            payload_[4]->setValue(1);
        conditionLabels();
    });
    connect(nodeApply, &QPushButton::clicked, this, [this] { applyNode(); });
    connect(conditionApply, &QPushButton::clicked, this, [this] { applyCondition(); });
    connect(addNode, &QPushButton::clicked, this, [this] {
        change("Add movement node",
               [this](Scenarios &s) { nodeRow_ = s.addNode(key_, 1, project_->classData()); });
    });
    connect(removeNode, &QPushButton::clicked, this, [this] {
        change("Remove movement node",
               [this](Scenarios &s) { s.removeNode(key_, nodeRow_, project_->classData()); });
    });
    connect(addCondition, &QPushButton::clicked, this, [this] {
        change("Add scenario condition", [this](Scenarios &s) {
            conditionRow_ = s.addCondition(key_, 9, project_->classData());
        });
    });
    connect(deleteCondition_, &QPushButton::clicked, this, [this] {
        change("Remove scenario condition", [this](Scenarios &s) {
            s.removeCondition(key_, conditionRow_, project_->classData());
        });
    });
    connect(groupApply, &QPushButton::clicked, this, [this] {
        change("Change squad movement group", [this](Scenarios &s) {
            s.setRouteGroup(key_, groupSquad_->currentData().toInt(), group_->value(),
                            pattern_->currentData().toInt(), project_->classData());
        });
    });
    setEnabled(false);
}
void ScenarioRouteEditor::refresh(int key) {
    setEnabled(key >= 0);
    if (key < 0) {
        key_ = key;
        return;
    }
    if (key != key_)
        nodeRow_ = conditionRow_ = 0;
    key_ = key;
    loading_ = true;
    const auto &s = project_->scenarios()->scenario(key_);
    nodes_->clear();
    for (const auto &n : s.layout.nodes)
        nodes_->addItem(QString("Node %1 · %2\nNext: %3 · Condition A: %4")
                            .arg(u8(n, 0))
                            .arg(QStringList{"Direct order", "Waypoint", "Dormant gate"}.value(
                                u8(n, 1), "Original kind"))
                            .arg(u8(n, 17))
                            .arg(u8(n, 10)));
    conditions_->clear();
    for (const auto &c : s.layout.conditions)
        conditions_->addItem(QString("%1 · %2").arg(u8(c, 0)).arg(kindName(u8(c, 1))));
    for (auto input : {conditionA_, conditionB_}) {
        input->clear();
        input->addItem("None", 0);
        for (const auto &c : s.layout.conditions)
            input->addItem(QString("%1 · %2").arg(u8(c, 0)).arg(kindName(u8(c, 1))), u8(c, 0));
    }
    for (auto input : {next_, fork_}) {
        input->clear();
        input->addItem("No transition", 0);
        input->addItem("Hold position", 1);
        input->addItem("Terminal (255)", 255);
        for (const auto &n : s.layout.nodes)
            input->addItem(QString("Node %1").arg(u8(n, 0)), u8(n, 0));
    }
    const int groupRow = groupSquad_->currentData().toInt();
    groupSquad_->clear();
    for (int i = 1; i < s.squads.size(); ++i)
        groupSquad_->addItem(QString("Source %1").arg(u8(s.layout.deployments[i], 0) & 127), i);
    groupSquad_->setCurrentIndex(std::max(0, groupSquad_->findData(groupRow)));
    nodes_->setCurrentRow(
        s.layout.nodes.isEmpty() ? -1 : std::clamp(nodeRow_, 0, int(s.layout.nodes.size()) - 1));
    conditions_->setCurrentRow(
        s.layout.conditions.isEmpty()
            ? -1
            : std::clamp(conditionRow_, 0, int(s.layout.conditions.size()) - 1));
    loading_ = false;
    notice_->clear();
    showNode();
    showCondition();
    showGroup();
}
void ScenarioRouteEditor::showNode() {
    if (loading_)
        return;
    nodeRow_ = nodes_->currentRow();
    nodeDetails_->setEnabled(nodeRow_ >= 0);
    if (nodeRow_ < 0)
        return;
    loading_ = true;
    const auto &n = project_->scenarios()->scenario(key_).layout.nodes[nodeRow_];
    select(nodeKind_, u8(n, 1));
    select(move_, u8(n, 2));
    select(wait_, u8(n, 3));
    select(placement_, u8(n, 5) ? 1 : 0);
    x_->setValue(u8(n, 4));
    z_->setValue(std::max(1, int(u8(n, 5))));
    select(conditionA_, u8(n, 10));
    select(gate_, u8(n, 11));
    select(conditionB_, u8(n, 12));
    select(fork_, u8(n, 16));
    select(next_, u8(n, 17));
    const bool extended = u8(n, 13) || u8(n, 14) || u8(n, 15);
    extension_->setText(
        extended
            ? "Extended gate payload is preserved: " + QString::fromLatin1(n.mid(13, 4).toHex(' '))
            : "A gate controls advancing from this node. Dormant squads wait for a valid next "
              "node. The waypoint subtype and unnamed fields retain their loaded values.");
    loading_ = false;
    nodeControls();
}
void ScenarioRouteEditor::nodeControls() {
    if (loading_ || key_ < 0 || nodeRow_ < 0)
        return;
    const int kind = nodeKind_->currentData().toInt();
    move_->setEnabled(kind == 0);
    wait_->setEnabled(kind == 0);
    placement_->setEnabled(kind == 1);
    x_->setEnabled(kind == 1);
    z_->setEnabled(kind == 1 && placement_->currentData().toInt() == 1);
    const auto &n = project_->scenarios()->scenario(key_).layout.nodes[nodeRow_];
    conditionB_->setEnabled(gate_->currentData().toInt() != 0);
    fork_->setEnabled(gate_->currentData().toInt() == 1 && !u8(n, 13) && !u8(n, 14) && !u8(n, 15));
}
void ScenarioRouteEditor::showCondition() {
    if (loading_)
        return;
    conditionRow_ = conditions_->currentRow();
    conditionDetails_->setEnabled(conditionRow_ >= 0);
    deleteCondition_->setEnabled(conditionRow_ >=
                                 project_->scenarios()->original(key_).layout.conditions.size());
    if (conditionRow_ < 0)
        return;
    loading_ = true;
    const auto &c = project_->scenarios()->scenario(key_).layout.conditions[conditionRow_];
    conditionKind_->clear();
    for (const int kind : {1, 4, 8, 9, 12})
        conditionKind_->addItem(kindName(kind), kind);
    select(conditionKind_, u8(c, 1));
    for (int i = 0; i < 8; ++i)
        payload_[i]->setValue(u8(c, i + 2));
    loading_ = false;
    conditionLabels();
}
void ScenarioRouteEditor::conditionLabels() {
    const int kind = conditionKind_->currentData().toInt();
    for (int i = 0; i < 8; ++i)
        payloadLabels_[i]->setText(QString("Payload byte %1").arg(i + 2));
    if (kind == 1 || kind == 8) {
        const QStringList labels{"Minimum X", "Minimum Z", "Maximum X", "Maximum Z"};
        for (int i = 0; i < 4; ++i)
            payloadLabels_[i]->setText(labels[i]);
    }
    if (kind == 8)
        payloadLabels_[4]->setText("Watched source ID");
    if (kind == 4)
        payloadLabels_[4]->setText("Target index");
    if (kind == 9)
        payloadLabels_[4]->setText("Active-source threshold");
    if (kind == 12)
        payloadLabels_[4]->setText("Scenario site record");
}
void ScenarioRouteEditor::showGroup() {
    if (loading_ || key_ < 0 || groupSquad_->currentIndex() < 0)
        return;
    const int row = groupSquad_->currentData().toInt();
    const auto &d = project_->scenarios()->scenario(key_).layout.deployments[row - 1];
    group_->setValue(u8(d, 16));
    select(pattern_, u8(d, 17));
}
void ScenarioRouteEditor::applyNode() {
    if (nodeRow_ < 0)
        return;
    change("Edit movement node", [this](Scenarios &s) {
        auto n = s.scenario(key_).layout.nodes[nodeRow_];
        const int kind = nodeKind_->currentData().toInt();
        if (kind != u8(n, 1))
            n.replace(2, 8, QByteArray(8, 0));
        put8(n, 1, quint8(kind));
        if (kind == 0) {
            put8(n, 2, quint8(move_->currentData().toInt()));
            put8(n, 3, quint8(wait_->currentData().toInt()));
        } else if (kind == 1) {
            put8(n, 4, quint8(x_->value()));
            put8(n, 5, quint8(placement_->currentData().toInt() ? z_->value() : 0));
        }
        const int gate = gate_->currentData().toInt();
        if (!gate && u8(n, 11))
            n[12] = 0;
        put8(n, 10, quint8(conditionA_->currentData().toInt()));
        put8(n, 11, quint8(gate));
        if (gate)
            put8(n, 12, quint8(conditionB_->currentData().toInt()));
        if (fork_->isEnabled())
            put8(n, 16, quint8(fork_->currentData().toInt()));
        put8(n, 17, quint8(next_->currentData().toInt()));
        s.setNode(key_, nodeRow_, n, project_->classData());
    });
}
void ScenarioRouteEditor::applyCondition() {
    if (conditionRow_ < 0)
        return;
    change("Edit scenario condition", [this](Scenarios &s) {
        auto c = s.scenario(key_).layout.conditions[conditionRow_];
        put8(c, 1, quint8(conditionKind_->currentData().toInt()));
        for (int i = 0; i < 8; ++i)
            put8(c, i + 2, quint8(payload_[i]->value()));
        s.setCondition(key_, conditionRow_, c, project_->classData());
    });
}
void ScenarioRouteEditor::change(const QString &label,
                                 const std::function<void(Scenarios &)> &edit) {
    if (key_ < 0)
        return;
    try {
        auto next = *project_->scenarios();
        edit(next);
        commit_(next, label);
        notice_->setText("Changes applied.");
    } catch (const Error &error) {
        notice_->setText(QString::fromUtf8(error.what()));
    }
}
} // namespace lordly
