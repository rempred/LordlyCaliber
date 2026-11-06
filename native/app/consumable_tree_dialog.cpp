#include "app/consumable_tree_dialog.h"
#include "engine/consumable_tree.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDrag>
#include <QDragMoveEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QTreeWidget>
#include <QVBoxLayout>
namespace lordly {
namespace {
// Qt's tree size hint normally measures the whole line without the column width.
// Give it the width left for text after the disclosure arrows and indentation.
class WrappedStepDelegate final : public QStyledItemDelegate {
  public:
    explicit WrappedStepDelegate(QTreeWidget *tree) : QStyledItemDelegate(tree), tree_(tree) {}

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        auto size = QStyledItemDelegate::sizeHint(option, index);
        int width = tree_->columnWidth(index.column()) - 10;
        for (auto ancestor = index; ancestor.isValid(); ancestor = ancestor.parent())
            width -= tree_->indentation();
        if (width > 0) {
            const auto text = index.data().toString();
            const QRect bounds = option.fontMetrics.boundingRect(
                QRect(0, 0, width, 32767), Qt::TextWordWrap, text);
            size.setHeight(std::max(size.height(), bounds.height() + 4));
        }
        return size;
    }
  private:
    QTreeWidget *tree_;
};

// A drag carries a live index in this view. A project refresh invalidates it.
// Move the original row explicitly so Qt never copies or removes it a second time.
class EffectDragData final : public QMimeData {
  public:
    explicit EffectDragData(const QModelIndex &index) : source(index) {
        setData("application/x-lordly-consumable-effect", {});
    }
    const QPersistentModelIndex source;
};

class EffectTreeView final : public QTreeWidget {
  public:
    explicit EffectTreeView(MoveConsumableEffect moved) : move_(std::move(moved)) {
        setDragDropMode(QAbstractItemView::DragDrop);
        setDefaultDropAction(Qt::MoveAction);
        setDropIndicatorShown(false);
        setAutoExpandDelay(500);
        setToolTip("Drag an effect onto THEN, ELSE or WHEN to move it.");
    }
  protected:
    QMimeData *mimeData(const QList<QTreeWidgetItem *> &items) const override {
        if (items.size() != 1) return nullptr;
        const auto node = items.first()->data(0, ConsumableNodeRole).toJsonObject();
        if (node.isEmpty() || node["type"] == "if") return nullptr;
        return new EffectDragData(indexFromItem(items.first()));
    }
    void startDrag(Qt::DropActions) override {
        const auto rows = selectedItems();
        auto data = mimeData(rows);
        if (!data) return;
        auto drag = new QDrag(this);
        drag->setMimeData(data);
        const auto row = rows.first();
        auto rect = visualItemRect(row);
        rect.setWidth(qMin(rect.width(), fontMetrics().horizontalAdvance(row->text(0)) + 12));
        drag->setPixmap(viewport()->grab(rect));
        drag->exec(Qt::MoveAction);
    }
    void dragEnterEvent(QDragEnterEvent *event) override {
        if (sourceItem(event) && (event->possibleActions() & Qt::MoveAction)) {
            setState(DraggingState);
            event->setDropAction(Qt::MoveAction);
            event->accept();
        } else {
            event->ignore();
        }
    }
    void dragMoveEvent(QDragMoveEvent *event) override {
        QTreeWidget::dragMoveEvent(event); // Keep the view's scrolling and auto-expand.
        auto target = dropTarget(event);
        highlight_ = indexFromItem(target);
        viewport()->update();
        if (target) {
            event->setDropAction(Qt::MoveAction);
            event->accept();
        } else {
            event->ignore();
        }
    }
    void dragLeaveEvent(QDragLeaveEvent *event) override {
        highlight_ = QModelIndex{};
        viewport()->update();
        QTreeWidget::dragLeaveEvent(event);
    }
    void dropEvent(QDropEvent *event) override {
        auto target = dropTarget(event);
        highlight_ = QModelIndex{};
        stopAutoScroll();
        setState(NoState);
        viewport()->update();
        if (target && move_(sourceItem(event), target)) {
            event->setDropAction(Qt::MoveAction);
            event->accept();
        } else {
            event->ignore();
        }
    }
    void paintEvent(QPaintEvent *event) override {
        QTreeWidget::paintEvent(event);
        if (!highlight_.isValid()) return;
        QPainter painter(viewport());
        painter.setPen(QPen(palette().highlight().color(), 2));
        painter.drawRect(visualRect(highlight_).adjusted(1, 1, -1, -1));
    }
  private:
    QTreeWidgetItem *sourceItem(const QDropEvent *event) const {
        const auto data = dynamic_cast<const EffectDragData *>(event->mimeData());
        return data && data->source.model() == model() ? itemFromIndex(data->source) : nullptr;
    }
    QTreeWidgetItem *dropTarget(const QDropEvent *event) const {
        auto target = itemAt(event->position().toPoint());
        if (!sourceItem(event) || !(event->possibleActions() & Qt::MoveAction) || !target ||
            !target->data(0, ConsumableBranchRole).isValid()) return nullptr;
        return target;
    }
    MoveConsumableEffect move_;
    QPersistentModelIndex highlight_;
};
}
QTreeWidget *createConsumableTreeView(MoveConsumableEffect moved) {
    auto tree = new EffectTreeView(std::move(moved));
    tree->setWordWrap(true);
    tree->setTextElideMode(Qt::ElideNone);
    tree->setItemDelegate(new WrappedStepDelegate(tree));
    QObject::connect(tree->header(), &QHeaderView::sectionResized, tree,
                     &QTreeWidget::doItemsLayout, Qt::QueuedConnection);
    return tree;
}
void moveConsumableRow(QTreeWidgetItem *effect, QTreeWidgetItem *branch) {
    auto parent = effect->parent();
    branch->addChild(parent->takeChild(parent->indexOfChild(effect)));
    branch->setExpanded(true);
    branch->treeWidget()->setCurrentItem(effect);
    branch->treeWidget()->scrollToItem(effect);
}
QJsonArray consumableSteps(const QTreeWidgetItem *branch) {
    QJsonArray steps;
    for (int i = 0; i < branch->childCount(); ++i) {
        auto row = branch->child(i);
        auto node = row->data(0, ConsumableNodeRole).toJsonObject();
        if (node.isEmpty()) continue;
        if (node["type"] == "if") {
            node["then"] = consumableSteps(row->child(0));
            node["else"] = consumableSteps(row->child(1));
        }
        steps.append(node);
    }
    return steps;
}
QString consumableStepLabel(const Project &project, const QJsonObject &node) {
    const auto condition = node["condition"].toObject();
    if (node["type"] == "if" && condition["type"] == "class") {
        const int id = condition["value"].toInt(1);
        if (id >= 0 && id < project.records(Table::Classes).size())
            return QString("IF Current class %1 %2")
                .arg(condition["not"].toBool() ? "differs from" : "is",
                     project.record(Table::Classes, id).name.simplified());
    }
    return ConsumableTree::label(node);
}
namespace {
class TreeDialog : public QDialog {
    const Project &project;
    QTreeWidget *tree;
    QTreeWidgetItem *when;
    QPushButton *addEffect, *addBranch;
    QComboBox *kind, *choice;
    QSpinBox *low, *high, *value;
    QCheckBox *invert, *percent, *randomRange;
    QLabel *help, *error, *lowLabel;
    bool busy = false;
    QJsonObject object(QTreeWidgetItem *i) const {
        return i ? i->data(0, ConsumableNodeRole).toJsonObject() : QJsonObject{};
    }
    void append(QTreeWidgetItem *parent, const QJsonArray &nodes) {
        for (const auto &v : nodes) {
            const auto n = v.toObject();
            auto row = new QTreeWidgetItem(parent, {consumableStepLabel(project, n)});
            row->setData(0, ConsumableNodeRole, n);
            if (n["type"] == "if") {
                auto yes = new QTreeWidgetItem(row, {"THEN"}),
                     no = new QTreeWidgetItem(row, {"ELSE"});
                yes->setData(0, ConsumableBranchRole, true);
                no->setData(0, ConsumableBranchRole, true);
                append(yes, n["then"].toArray());
                append(no, n["else"].toArray());
            }
        }
    }
    struct Insertion {
        QTreeWidgetItem *parent;
        int index;
        bool below;
    };
    Insertion insertion() const {
        auto row = tree->currentItem();
        if (!row)
            row = when;
        if (object(row)["type"] == "if")
            row = row->child(0); // The selected condition's THEN branch.
        if (object(row).isEmpty())
            return {row, row->childCount(), false};
        auto parent = row->parent();
        return {parent, parent->indexOfChild(row) + 1, true};
    }
    void select() {
        busy = true;
        const auto place = insertion();
        const QString where = place.parent == when
            ? (place.below ? " below at top level" : " at top level")
            : (place.below ? " below in " : " to ") + place.parent->text(0);
        addEffect->setText("Add effect" + where);
        addBranch->setText("Add IF / ELSE" + where);
        const auto n = object(tree->currentItem());
        const bool condition = n["type"] == "if", active = !n.isEmpty();
        const auto fields = condition ? n["condition"].toObject() : n;
        kind->clear();
        const auto options = condition ? ConsumableTree::conditions() : ConsumableTree::actions();
        for (auto it = options.begin(); it != options.end(); ++it)
            kind->addItem(it.value(), it.key());
        kind->setCurrentIndex(kind->findData(fields["type"].toString()));
        const auto type = fields["type"].toString();
        auto form = qobject_cast<QFormLayout *>(kind->parentWidget()->layout());
        form->setRowVisible(kind, active);
        auto kindLabel = qobject_cast<QLabel *>(form->labelForField(kind));
        kindLabel->setText(condition ? "Condition" : "Effect");
        const bool numeric = active && !condition && type != "cure";
        low->setValue(fields["minimum"].toInt(1));
        high->setValue(fields["maximum"].toInt(low->value()));
        randomRange->setChecked(low->value() != high->value());
        form->setRowVisible(randomRange, numeric);
        form->setRowVisible(low, numeric);
        form->setRowVisible(high, numeric && randomRange->isChecked());
        lowLabel->setText(randomRange->isChecked() ? "Minimum" : "Amount");
        invert->setVisible(active && condition);
        invert->setChecked(fields["not"].toBool());
        percent->setVisible(active && !condition && (type == "heal" || type == "revive"));
        percent->setChecked(fields["percent"].toBool());
        const bool pick = condition && (type == "class" || type == "retail");
        choice->clear();
        form->setRowVisible(choice, active && pick);
        if (pick) {
            const auto table = type == "class" ? Table::Classes : Table::Consumables;
            for (const auto &r : project.records(table))
                if (type == "class" || ConsumableTree::supported(r.id))
                    choice->addItem(r.name.simplified(), r.id);
            choice->setCurrentIndex(choice->findData(fields["value"].toInt()));
        }
        form->setRowVisible(
            value, active && condition && !pick &&
                       QStringList{"hpPercent", "fatigue", "alignment", "luck"}.contains(type));
        value->setValue(fields["value"].toInt());
        help->setText(type == "cure" ? "Removes petrification."
                      : type == "level"
                          ? "Uses the character's current class growth, up to level 99."
                          : QString());
        busy = false;
    }
    void update(bool changeKind = false) {
        if (busy)
            return;
        auto row = tree->currentItem();
        auto n = object(row);
        if (n.isEmpty())
            return;
        const bool cond = n["type"] == "if";
        const auto type = kind->currentData().toString();
        auto f = changeKind
                     ? (cond ? ConsumableTree::condition(type) : ConsumableTree::action(type))
                     : (cond ? n["condition"].toObject() : n);
        if (!changeKind) {
            if (cond) {
                f["not"] = invert->isChecked();
                if (type == "class" || type == "retail")
                    f["value"] = choice->currentData().toInt();
                else
                    f["value"] = value->value();
            } else {
                f["minimum"] = low->value();
                f["maximum"] = randomRange->isChecked() ? high->value() : low->value();
                if (type == "heal" || type == "revive")
                    f["percent"] = percent->isChecked();
            }
        }
        if (cond)
            n["condition"] = f;
        else
            n = f;
        row->setData(0, ConsumableNodeRole, n);
        row->setText(0, consumableStepLabel(project, n));
        if (changeKind)
            select();
        error->clear();
    }
    void add(bool branch) {
        const auto place = insertion();
        const auto n = branch ? QJsonObject{{"type", "if"},
                                            {"condition", ConsumableTree::condition("alive")},
                                            {"then", QJsonArray{}},
                                            {"else", QJsonArray{}}}
                              : ConsumableTree::action("heal");
        auto temp = new QTreeWidgetItem;
        append(temp, {n});
        auto added = temp->takeChild(0);
        delete temp;
        place.parent->insertChild(place.index, added);
        tree->expandAll();
        tree->setCurrentItem(added);
    }

  public:
    QJsonArray result;
    TreeDialog(QWidget *parent, const Project &p, int id, const QList<int> &path)
        : QDialog(parent), project(p) {
        setObjectName("consumableTreeDialog");
        setWindowTitle("Item effects · " + p.record(Table::Consumables, id).name.simplified());
        resize(900, 590);
        auto root = new QVBoxLayout(this);
        auto bar = new QHBoxLayout;
        root->addLayout(bar);
        auto split = new QSplitter;
        root->addWidget(split, 1);
        tree = createConsumableTreeView([](QTreeWidgetItem *effect, QTreeWidgetItem *branch) {
            moveConsumableRow(effect, branch);
            return true;
        });
        tree->setObjectName("itemEffectTree");
        tree->setHeaderHidden(true);
        when = new QTreeWidgetItem(tree, {"WHEN item used"});
        when->setData(0, ConsumableBranchRole, true);
        split->addWidget(tree);
        auto right = new QWidget;
        auto form = new QFormLayout(right);
        split->addWidget(right);
        kind = new QComboBox;
        kind->setObjectName("itemEffectKind");
        form->addRow("Effect / condition", kind);
        low = new QSpinBox;
        high = new QSpinBox;
        for (auto s : {low, high}) {
            s->setRange(-999, 999);
            s->setKeyboardTracking(false);
        }
        low->setObjectName("itemEffectMinimum");
        high->setObjectName("itemEffectMaximum");
        lowLabel = new QLabel("Amount");
        form->addRow(lowLabel, low);
        form->addRow("Maximum", high);
        randomRange = new QCheckBox("Random range");
        randomRange->setObjectName("itemEffectRandomRange");
        form->addRow(randomRange);
        connect(randomRange, &QCheckBox::toggled, this, [this, form](bool range) {
            if (busy)
                return;
            if (range)
                high->setValue(low->value());
            lowLabel->setText(range ? "Minimum" : "Amount");
            form->setRowVisible(high, range);
            update();
        });
        percent = new QCheckBox("Percentage of maximum HP");
        form->addRow(percent);
        choice = new QComboBox;
        form->addRow("Choose", choice);
        value = new QSpinBox;
        value->setRange(0, 100);
        value->setKeyboardTracking(false);
        form->addRow("Value", value);
        invert = new QCheckBox("NOT — invert this condition");
        form->addRow(invert);
        help = new QLabel;
        help->setWordWrap(true);
        form->addRow(help);
        auto button = [&](QString label, QString name, auto fn) {
            auto b = new QPushButton(label);
            b->setObjectName(name);
            bar->addWidget(b);
            connect(b, &QPushButton::clicked, this, fn);
            return b;
        };
        addEffect = button("Add effect", "itemAddEffect", [this] { add(false); });
        addBranch = button("Add IF / ELSE", "itemAddIf", [this] { add(true); });
        button("Remove", "itemRemove", [this] {
            auto i = tree->currentItem();
            if (!object(i).isEmpty())
                delete i;
        });
        for (int direction : {-1, 1})
            button(direction < 0 ? "Move up" : "Move down",
                   direction < 0 ? "itemMoveUp" : "itemMoveDown", [this, direction] {
                       auto i = tree->currentItem();
                       if (object(i).isEmpty())
                           return;
                       auto p = i->parent() ? i->parent() : tree->invisibleRootItem();
                       int at = p->indexOfChild(i), to = at + direction;
                       if (to >= 0 && to < p->childCount()) {
                           p->takeChild(at);
                           p->insertChild(to, i);
                           tree->setCurrentItem(i);
                       }
                   });
        error = new QLabel;
        error->setWordWrap(true);
        root->addWidget(error);
        root->addWidget(new QLabel(
            "Consumes one item after confirmation. Random amounts are rolled once per target."));
        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        root->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, this, [this] {
            try {
                result = consumableSteps(when);
                ConsumableTree::validate(result);
                accept();
            } catch (const std::exception &e) {
                error->setText(QString::fromUtf8(e.what()));
            }
        });
        append(when, p.consumableEffects()->tree(id));
        tree->expandAll();
        connect(tree, &QTreeWidget::currentItemChanged, this, [this] { select(); });
        connect(kind, &QComboBox::activated, this, [this] { update(true); });
        connect(choice, &QComboBox::activated, this, [this] { update(); });
        for (auto s : {low, high, value})
            connect(s, &QSpinBox::valueChanged, this, [this] { update(); });
        connect(invert, &QCheckBox::toggled, this, [this] { update(); });
        connect(percent, &QCheckBox::toggled, this, [this] { update(); });
        auto initial = when;
        for (int index : path) {
            initial = initial->child(index);
            if (!initial)
                break;
        }
        if (!initial || initial == when)
            initial = when->childCount() ? when->child(0) : when;
        tree->setCurrentItem(initial);
        select();
    }
};
} // namespace
std::optional<QJsonArray> editConsumableTree(QWidget *parent, const Project &p, int id,
                                             const QList<int> &path) {
    TreeDialog d(parent, p, id, path);
    if (d.exec() == QDialog::Accepted)
        return d.result;
    return {};
}
} // namespace lordly
