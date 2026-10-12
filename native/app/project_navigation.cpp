#include "app/main_window.h"
#include "app/scenario_map_editor.h"
#include "app/text_workspace.h"
#include "app/project_review_dialog.h"
#include <QComboBox>
#include <QLineEdit>
#include <QJsonArray>
#include <QListWidget>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableView>

namespace lordly {
namespace {
void reveal(QWidget *widget) {
    if (!widget) return;
    for (auto parent = widget->parentWidget(); parent; parent = parent->parentWidget()) {
        if (auto tabs = qobject_cast<QTabWidget *>(parent))
            for (int i = 0; i < tabs->count(); ++i)
                if (tabs->widget(i) == widget || tabs->widget(i)->isAncestorOf(widget))
                    tabs->setCurrentIndex(i);
        if (auto stack = qobject_cast<QStackedWidget *>(parent))
            for (int i = 0; i < stack->count(); ++i)
                if (stack->widget(i) == widget || stack->widget(i)->isAncestorOf(widget))
                    stack->setCurrentIndex(i);
    }
    widget->setFocus(Qt::OtherFocusReason);
}
}
void MainWindow::navigateTo(const EditLocation &location) {
    if (!project_ || loading_) return;
    try {
        auto clear = [this](const QString &name) {
            if (auto search = findChild<QLineEdit *>(name)) search->clear();
        };
        auto list = [this](const QString &name, const QVariant &value, int role = Qt::UserRole) -> QWidget * {
            auto rows = findChild<QListWidget *>(name);
            if (!rows) return nullptr;
            for (int i = 0; i < rows->count(); ++i)
                if (rows->item(i)->data(role) == value) {
                    rows->item(i)->setHidden(false);
                    rows->setCurrentRow(i); rows->scrollToItem(rows->item(i)); break;
                }
            return rows;
        };
        auto combo = [this](const QString &name, const QVariant &value) -> QWidget * {
            auto box = findChild<QComboBox *>(name);
            if (box && box->findData(value) >= 0) box->setCurrentIndex(box->findData(value));
            return box;
        };
        QWidget *destination = nullptr;
        const auto &kind = location.editor;
        int id = location.id;
        if (kind == "classes" || kind == "items" || kind == "consumables") {
            clear(kind + "Search");
            auto table = findChild<QTableView *>(kind + "Table");
            if (table) {
                for (int row = 0; row < table->model()->rowCount(); ++row)
                    if (table->model()->index(row, 0).data(Qt::EditRole).toInt() == id) {
                        table->setCurrentIndex(table->model()->index(row, 1));
                        table->scrollTo(table->currentIndex()); break;
                    }
                destination = location.key == "promotionEditor" ? findChild<QWidget *>(location.key) : table;
            }
        } else if (kind == "family") {
            // Open a member of this family; this selects the family without changing assignment.
            project_->loadPromotion();
            const auto assignments = project_->promotion()->families["assignments"].toArray();
            for (int member = 1; member < assignments.size(); ++member)
                if (assignments[member].toInt() == id) { navigateTo({"classes", member, -1, "promotionEditor"}); return; }
            destination = findChild<QWidget *>("promotionEditor");
        } else if (kind == "training") {
            combo("trainingLocation",id); destination=combo("trainingOpponent",location.child);
        } else if (kind == "shops") {
            clear("shopSearch"); destination = list("shopList", id);
        } else if (kind == "encounters") {
            clear("encounterSearch"); destination = list("encounterPools", id);
        } else if (kind == "encounterReward") {
            clear("rewardSearch"); destination = list("rewardClasses", id);
        } else if (kind == "customEncounter") {
            clear("customSearch"); destination = list("customSquadList", CustomSquads::key(id, location.child));
        } else if (kind == "scenarios" || kind == "map" || kind == "town" || kind == "battlefield") {
            int siteRow = -1;
            if (kind == "battlefield") { project_->loadMapEdits(); id = project_->mapEdits()->scenarioForModel(id); }
            if (kind == "town") {
                id = -1;
                project_->loadWorld();
                for (int key = 1; key <= 64 && id < 0; ++key)
                    for (int row = 0; row < project_->world()->siteCount(key); ++row)
                        if (u16(project_->world()->scenario(key).sites, 1 + row * 4) == location.id) {
                            id = key; siteRow = row; break;
                        }
            }
            clear("scenarioSearch"); destination = list("scenarioList", id);
            if (auto pages = findChild<QStackedWidget *>("scenarioPages")) pages->setCurrentIndex(kind == "scenarios" ? 1 : 0);
            if (kind == "scenarios" && location.child >= 0) {
                if (auto squads = findChild<QListWidget *>("scenarioSquads")) {
                    squads->setCurrentRow(location.child); destination = squads;
                }
            }
            if (auto objects = findChild<QListWidget *>("mapObjects"))
                for (int row = 0; row < objects->count(); ++row) {
                    const auto item = objects->item(row);
                    const bool site = siteRow >= 0 && item->data(Qt::UserRole).toInt() == int(MapObject::Site) && item->data(Qt::UserRole + 1).toInt() == siteRow;
                    const bool treasure = location.key == "treasure" && item->data(Qt::UserRole).toInt() == int(MapObject::Treasure) && item->data(Qt::UserRole + 1).toInt() == location.child;
                    if (site || treasure) { objects->setCurrentRow(row); destination = objects; break; }
                }
        } else if (kind == "combatAnimation") {
            destination = combo("combatAnimationClass", id);
            if (location.child >= 0) {
                combo("combatAnimationSide", location.child & 1);
                combo("combatAnimationVariant", location.child >> 1);
            }
        } else if (kind == "animation") destination = combo("animationClass", id);
        else if (kind == "portrait") { clear("portraitSearch"); destination = list("portraitClasses", id); }
        else if (kind == "army" || kind == "armyClass") {
            clear("armySearch");
            if (kind == "army") { combo("armyAtlas", id); destination = list("armyModels", location.child); }
            else destination = list("armyModels", id, Qt::UserRole + 1);
        } else if (kind == "sprites") destination = list("spriteAssets", location.key);
        else if (kind == "combatSprite") {
            clear("combatSearch");
            project_->loadCombatArtwork();
            if (project_->combatArtwork()->bindings().contains(location.key)) {
                const auto &binding = project_->combatArtwork()->binding(location.key);
                combo("combatDescriptor", binding.descriptorKey);
                destination = list("combatMembers", location.key);
                combo("combatChild", location.child);
            }
        } else if (kind == "scene") { clear("cutsceneScenesSearch"); destination = location.key.isEmpty() ? list("cutsceneScenes", uint(id)) : list("cutsceneScenes", location.key, Qt::UserRole + 1); }
        else if (kind == "icon") {
            combo("artIconPack", (id & 0x200) ? 1 : 0); destination = list("artIcons", id);
        } else {
            const QMap<QString, QString> panels{{"Optional tools", "toolsEditor"},
                {"UI and Icons", "categoryIconEditor"}, {"Class families", "promotionEditor"},
                {"Neutral encounters", "encounterEditor"}, {"Consumable effects", "consumableEditor"},
                {"Animation assignments", "animationEditor"}, {"Cutscenes", "cutsceneScenes"},
                {"Cutscene events", "cutsceneScenes"}, {"Dialogue", "cutsceneScenes"}};
            if (panels.contains(kind)) destination = findChild<QWidget *>(panels.value(kind));
            else if (location.key == "description") {
                editDescription(this, project_.get(), undo_, kind, id, [this] { updateStatus(); });
                return;
            }
        }
        if (!destination) throw Error("This change has no editable record in the current workspace.");
        reveal(destination);
        if (location.key == "description")
            editDescription(this, project_.get(), undo_, kind,
                            kind == "classes" ? project_->classDescriptionIndex(id) : id,
                            [this] { updateStatus(); });
    } catch (const std::exception &e) { report(QString::fromUtf8(e.what())); }
}
void MainWindow::showUses() {
    if (!project_ || loading_) return;
    EditLocation query;
    for (const auto &kind : {QString("classes"), QString("items"), QString("consumables")}) {
        auto table = findChild<QTableView *>(kind + "Table");
        if (table && table->isVisible() && table->currentIndex().isValid()) {
            query = {kind, table->currentIndex().siblingAtColumn(0).data(Qt::EditRole).toInt()};
            break;
        }
    }
    for (const auto &pair : {qMakePair(QString("shopList"), QString("shops")),
                            qMakePair(QString("spriteAssets"), QString("sprites")),
                            qMakePair(QString("combatMembers"), QString("combatSprite")),
                            qMakePair(QString("portraitClasses"), QString("classes"))}) {
        auto rows = findChild<QListWidget *>(pair.first);
        if (rows && rows->isVisible() && rows->currentItem()) {
            const auto value = rows->currentItem()->data(Qt::UserRole);
            query = {pair.second, value.toInt(), -1, value.toString()}; break;
        }
    }
    if (query.editor.isEmpty()) {
        if (auto classes = findChild<QComboBox *>("combatAnimationClass"); classes && classes->isVisible())
            query = {"classes", classes->currentData().toInt()};
        if (auto icons = findChild<QListWidget *>("artIcons"); icons && icons->isVisible() && icons->currentItem()) {
            const int id = icons->currentItem()->data(Qt::UserRole).toInt();
            query = {id & 0x200 ? "consumables" : "items", id & 0x1ff};
        }
    }
    if (query.editor.isEmpty()) {
        report("Select a class, item, shop or sprite, then click ? to find where it is used.");
        return;
    }
    ProjectReviewDialog dialog(*project_, query, this);
    connect(&dialog, &ProjectReviewDialog::editRequested, this, &MainWindow::navigateTo);
    dialog.exec();
}
} // namespace lordly
