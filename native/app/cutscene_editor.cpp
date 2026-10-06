#include "app/cutscene_editor.h"
#include "app/scene_action_editor.h"
#include <QUndoStack>
#include "app/actor_pose_editor.h"
#include "app/sprite_library_command.h"
#include "app/dialogue_inspector.h"
#include "app/scene_command_editor.h"
#include "app/scene_event_editor.h"
#include "app/scene_preview.h"
#include "app/transform_preview.h"
#include "engine/scene_import.h"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtConcurrent>
namespace lordly {
namespace {
QString hex(quint32 value) {
    return QString::number(value, 16).toUpper().rightJustified(8, '0');
}
class ImageView : public QGraphicsView {
  public:
    using QGraphicsView::QGraphicsView;

  protected:
    void wheelEvent(QWheelEvent *event) override {
        const double factor = event->angleDelta().y() > 0 ? 1.2 : 1 / 1.2;
        const auto next = transform().m11() * factor;
        if (next >= .03 && next <= 64)
            scale(factor, factor);
        event->accept();
    }
};
QPixmap pixmap(const RasterImage &r) {
    return QPixmap::fromImage(QImage(reinterpret_cast<const uchar *>(r.rgba.constData()), r.width,
                                     r.height, r.width * 4, QImage::Format_RGBA8888));
}
} // namespace
CutsceneEditor::CutsceneEditor(const Rom &rom, QWidget *parent, Project *project, QUndoStack *undo,
                               std::function<void()> changed)
    : QWidget(parent), rom_(rom), project_(project), undo_(undo) {
    auto layout = new QVBoxLayout(this);
    auto note = new QLabel(
        "Edit scene commands and dialogue, inspect background resources and actor poses. "
        "Full-scene playback is being integrated.");
    note->setWordWrap(true);
    layout->addWidget(note);
    auto split = new QSplitter;
    layout->addWidget(split, 1);
    catalogTabs_ = new QTabWidget;
    catalogTabs_->setObjectName("cutsceneCatalogTabs");
    auto addList = [&](const QString &title, const QString &name) {
        auto panel = new QWidget;
        auto column = new QVBoxLayout(panel);
        auto search = new QLineEdit;
        search->setPlaceholderText("Search names, IDs and addresses…");
        search->setClearButtonEnabled(true);
        search->setObjectName(name + "Search");
        auto list = new QListWidget;
        list->setObjectName(name);
        column->addWidget(search);
        column->addWidget(list, 1);
        connect(search, &QLineEdit::textChanged, list, [list](const QString &text) {
            for (int i = 0; i < list->count(); ++i)
                list->item(i)->setHidden(
                    !list->item(i)->text().contains(text, Qt::CaseInsensitive));
        });
        catalogTabs_->addTab(panel, title);
        return list;
    };
    scenes_ = addList("Scenes", "cutsceneScenes");
    if (project_) {
        auto buttons = new QHBoxLayout;
        newScene_ = new QPushButton("New scene");
        newScene_->setObjectName("sceneCreate");
        duplicateScene_ = new QPushButton("Duplicate");
        duplicateScene_->setObjectName("sceneDuplicate");
        buttons->addWidget(newScene_);
        buttons->addWidget(duplicateScene_);
        static_cast<QVBoxLayout *>(scenes_->parentWidget()->layout())->addLayout(buttons);
        auto actions = new QHBoxLayout;
        renameScene_ = new QPushButton("Rename");
        renameScene_->setObjectName("sceneRename");
        removeScene_ = new QPushButton("Remove authored scene");
        removeScene_->setObjectName("sceneRemove");
        actions->addWidget(renameScene_);
        actions->addWidget(removeScene_);
        static_cast<QVBoxLayout *>(scenes_->parentWidget()->layout())->addLayout(actions);
        wipeScene_ = new QPushButton("Clear scene content");
        wipeScene_->setObjectName("sceneWipe");
        static_cast<QVBoxLayout *>(scenes_->parentWidget()->layout())->addWidget(wipeScene_);
        for (auto button : {newScene_, duplicateScene_, renameScene_, removeScene_, wipeScene_})
            button->setEnabled(false);
        connect(newScene_, &QPushButton::clicked, this, [this] { createScene(true); });
        connect(duplicateScene_, &QPushButton::clicked, this, [this] { createScene(false); });
        connect(renameScene_, &QPushButton::clicked, this, &CutsceneEditor::renameScene);
        connect(removeScene_, &QPushButton::clicked, this, &CutsceneEditor::removeScene);
        connect(wipeScene_, &QPushButton::clicked, this, &CutsceneEditor::wipeScene);
    }
    assets_ = addList("Images", "cutsceneAssets");
    split->addWidget(catalogTabs_);
    auto details = new QWidget;
    auto detailLayout = new QVBoxLayout(details);
    auto controls = new QHBoxLayout;
    mode_ = new QComboBox;
    mode_->setObjectName("cutsceneMode");
    mode_->addItem("Resolve launch mode", -1);
    mode_->addItem("Preview mode 2", 2);
    mode_->addItem("Preview scene group", 0);
    requests_ = new QComboBox;
    requests_->setObjectName("cutsceneRequests");
    images_ = new QComboBox;
    images_->setObjectName("cutsceneImages");
    images_->setMinimumContentsLength(15);
    images_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    controls->addWidget(mode_);
    controls->addWidget(requests_, 1);
    controls->addWidget(images_, 1);
    detailLayout->addLayout(controls);
    sceneStatus_ = new QLabel("Reading the ROM's Director catalog…");
    sceneStatus_->setObjectName("cutsceneStatus");
    sceneStatus_->setWordWrap(true);
    detailLayout->addWidget(sceneStatus_);
    auto pages = new QTabWidget;
    pages->setObjectName("cutsceneDetailPages");
    auto preview = new QWidget;
    auto previewLayout = new QVBoxLayout(preview);
    previewLayout->addWidget(createBackgroundControls());
    auto imageTools = new QHBoxLayout;
    auto fit = new QPushButton("Fit image"), actual = new QPushButton("Actual pixels");
    frames_ = new QComboBox;
    frames_->setObjectName("cutsceneFrames");
    frames_->setMinimumWidth(165);
    frames_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    imageTools->addWidget(fit);
    imageTools->addWidget(actual);
    imageTools->addWidget(frames_);
    imageTools->addStretch();
    previewLayout->addLayout(imageTools);
    auto previewSplit = new QSplitter;
    canvas_ = new QGraphicsScene(this);
    view_ = new ImageView(canvas_);
    view_->setObjectName("cutsceneCanvas");
    view_->setDragMode(QGraphicsView::ScrollHandDrag);
    view_->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    QPixmap checker(24, 24);
    checker.fill(QColor("#252931"));
    {
        QPainter p(&checker);
        p.fillRect(0, 0, 12, 12, QColor("#2e333c"));
        p.fillRect(12, 12, 12, 12, QColor("#2e333c"));
    }
    view_->setBackgroundBrush(checker);
    previewSplit->addWidget(view_);
    layers_ = new QListWidget;
    layers_->setObjectName("cutsceneLayers");
    layers_->setMinimumWidth(150);
    previewSplit->addWidget(layers_);
    previewSplit->setSizes({750, 200});
    previewLayout->addWidget(previewSplit, 1);
    imageStatus_ = new QLabel;
    imageStatus_->setObjectName("cutsceneImageStatus");
    imageStatus_->setWordWrap(true);
    imageStatus_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    previewLayout->addWidget(imageStatus_);
    pages->addTab(preview, "Background preview");
    commands_ = new QTableWidget(0, 4);
    commands_->setObjectName("cutsceneCommands");
    commands_->setHorizontalHeaderLabels({"Word", "Command", "Operands", "Meaning"});
    commands_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    commands_->setSelectionBehavior(QAbstractItemView::SelectRows);
    commands_->setAlternatingRowColors(true);
    commands_->verticalHeader()->hide();
    commands_->horizontalHeader()->setStretchLastSection(true);
    commands_->setColumnWidth(0, 60);
    commands_->setColumnWidth(1, 210);
    commands_->setColumnWidth(2, 270);
    if (project_) {
        auto programPage = new QSplitter(Qt::Vertical);
        programPage->addWidget(commands_);
        commandEditor_ = new SceneCommandEditor(project_, undo, changed);
        programPage->addWidget(commandEditor_);
        programPage->setSizes({350, 350});
        pages->addTab(programPage, "Commands");
        connect(commands_, &QTableWidget::currentCellChanged, this, [this](int row, int, int, int) {
            if (scenes_->currentItem())
                commandEditor_->selectCommand(selectedScene(), row);
        });
        connect(commandEditor_, &SceneCommandEditor::programChanged, this, [this] {
            refreshSceneInputState();
        });
    } else
        pages->addTab(commands_, "Source commands");
    auto actorInspector = new ActorPoseEditor(rom_);
    scenePreview_ = new ScenePreview(rom_);
    if (project_) {
        scenePreview_->setPortraitProvider([project](int id, bool variant) {
            project->loadAvatarArtwork();
            return project->avatarArtwork()->words(project->classData(), id, variant ? 1 : 0);
        });
        actorInspector->setSpriteLibrary([project, undo, changed](SpriteAsset asset) {
            auto library = project->sprites();
            library.replace(asset);
            undo->push(new SpriteLibraryCommand(project, "Copy actor pose to sprite library", std::move(library)));
            if (changed) changed();
        });
        auto artwork = [project] {
            return project->combatArtwork() ? project->combatArtwork()->previewEdits()
                                            : ActorArtworkEdits{};
        };
        const auto initialAnimations = project->bodyAnimationData();
        scenePreview_->setBodyAnimations(initialAnimations);
        actorInspector->setBodyAnimations(initialAnimations);
        const auto initialArtwork = artwork();
        scenePreview_->setArtwork(initialArtwork);
        actorInspector->setArtwork(initialArtwork);
        auto signature = [project] {
            QJsonArray classes;
            for (const auto &record : project->records(Table::Classes))
                classes << QJsonArray{record.name,QString::fromLatin1(record.bytes.toHex())};
            return QJsonObject{{"classes", classes},
                               {"routes", project->animationRoutes() ? project->animationRoutes()->json() : QJsonObject{}},
                               {"overrides", project->animationOverrides() ? project->animationOverrides()->json() : QJsonObject{}},
                               {"sequences", project->animationSequences() ? project->animationSequences()->json() : QJsonObject{}},
                               {"art", project->avatarArtwork() ? project->avatarArtwork()->json()
                                                                : QJsonObject{}},
                               {"combat", project->combatArtwork()
                                              ? project->combatArtwork()->json()
                                              : QJsonObject{}}};
        };
        connect(undo_, &QUndoStack::indexChanged, this,
                [this, signature, artwork, actorInspector, previous = signature()]() mutable {
                    const auto next = signature();
                    if (next != previous) {
                        if (next["classes"] != previous["classes"] || next["routes"] != previous["routes"] || next["overrides"] != previous["overrides"] || next["sequences"] != previous["sequences"]) {
                            const auto animations = project_->bodyAnimationData();
                            scenePreview_->setBodyAnimations(animations);
                            actorInspector->setBodyAnimations(animations);
                        }
                        previous = next;
                        const auto edits = artwork();
                        scenePreview_->setArtwork(edits);
                        actorInspector->setArtwork(edits);
                    }
                });
    }
    pages->addTab(scenePreview_, "Scene playback");
    pages->addTab(actorInspector, "Pose inspector");
    transformPreview_ = new TransformPreview(rom_);
    pages->addTab(transformPreview_, "Transform preview");
    auto dialogue =
        project ? new DialogueInspector(project, undo, changed) : new DialogueInspector(rom_);
    pages->addTab(dialogue, "Dialogue");
    connect(pages, &QTabWidget::currentChanged, this, [this, pages] {
        if (project_ && pages->currentWidget() == scenePreview_)
            scenePreview_->setDialogues(project_->dialogues());
    });
    commands_->setToolTip(
        "Double-click an Actor creation, dialogue or transform command to preview it.");
    connect(commands_, &QTableWidget::cellDoubleClicked, this,
            [this, pages, dialogue, actorInspector](int row, int) {
                if (!directors_ || !scenes_->currentItem())
                    return;
                const auto program = programs().program(selectedScene());
                if (row < 0 || row >= program.commands.size())
                    return;
                const auto &command = program.commands[row];
                if (command.opcode == 0x14) {
                    actorInspector->selectCommand(program, row);
                    pages->setCurrentWidget(actorInspector);
                    return;
                }
                if (command.opcode == 8) {
                    transformPreview_->selectCommand(program, row);
                    pages->setCurrentWidget(transformPreview_);
                    return;
                }
                if (command.opcode != 0xbf || command.operands.size() != 13)
                    return;
                dialogue->selectEntry(quint16(command.operands[1]), quint16(command.operands[2]));
                pages->setCurrentWidget(dialogue);
            });
    if (project_)
        pages->addTab(new SceneEventEditor(project_, undo_, changed), "Event launches");
    if (project_) {
        actionEditor_ = new SceneActionEditor(project_, undo_, changed);
        pages->addTab(actionEditor_, "Actions");
        scenePreview_->setObjectNavigationEnabled(true);
        auto findObject = [this, pages](int selector, SceneSelectionTarget object) {
            const int slot = object.slot;
            if (invalidSceneInput())
                return;
            SceneId target;
            for (const auto &id : programs().scenes())
                if (programs().resource(id).selectors.contains(selector)) {
                    target = id;
                    break;
                }
            if (!target) {
                sceneStatus_->setText("The playback script has no editable scene in this project.");
                return;
            }
            if (selectedScene() != target) {
                for (int row = 0; row < scenes_->count(); ++row) {
                    const auto item = scenes_->item(row);
                    if (item->data(Qt::UserRole).toUInt() == target.nativeKey &&
                        item->data(Qt::UserRole + 1).toString() == target.authoredId) {
                        scenes_->setCurrentRow(row);
                        break;
                    }
                }
            }
            const bool actor = object.kind == SceneObjectKind::Actor;
            if (actor ? actionEditor_->selectActor(slot) : actionEditor_->selectSprite(slot)) {
                pages->setCurrentWidget(actionEditor_);
                return;
            }
            if (!actor) {
                const auto program = programs().program(target);
                for (int i = 0; i < program.commands.size(); ++i) {
                    const auto &c = program.commands[i];
                    if (c.opcode == 0x63 && (c.operands[0] & 0xffffff00U) != 0x08880000U &&
                        quint8(c.operands[0]) == slot) {
                        commands_->setCurrentCell(i, 0);
                        commandEditor_->selectCommand(target, i);
                        pages->setCurrentWidget(commandEditor_->parentWidget());
                        return;
                    }
                }
            }
            sceneStatus_->setText(QString("%1 %2 has no directly editable action in this script; "
                                          "it may come from launch context.")
                                      .arg(actor ? "Actor" : "Sprite")
                                      .arg(slot));
        };
        connect(scenePreview_, &ScenePreview::findActorAction, this,
                [findObject](int selector, int slot) {
                    findObject(selector, {SceneObjectKind::Actor, slot});
                });
        connect(scenePreview_, &ScenePreview::findSpriteAction, this,
                [findObject](int selector, int slot) {
                    findObject(selector, {SceneObjectKind::Sprite, slot});
                });
        connect(actionEditor_, &SceneActionEditor::editDialogue, this,
                [dialogue, pages](int selector, int entry) {
                    dialogue->selectEntry(selector, entry);
                    pages->setCurrentWidget(dialogue);
                });
        connect(actionEditor_, &SceneActionEditor::commandSelected, this,
                [this](int command) { commands_->setCurrentCell(command, 0); });
        connect(actionEditor_, &SceneActionEditor::validityChanged, this,
                &CutsceneEditor::refreshSceneInputState);
        connect(commands_, &QTableWidget::currentCellChanged, this,
                [this](int command, int, int, int) { actionEditor_->selectCommand(command); });
    }
    detailLayout->addWidget(pages, 1);
    split->addWidget(details);
    split->setSizes({310, 1000});
    connect(fit, &QPushButton::clicked, this, [this] {
        if (image_)
            view_->fitInView(canvas_->sceneRect(), Qt::KeepAspectRatio);
    });
    connect(actual, &QPushButton::clicked, this, [this] { view_->resetTransform(); });
    connect(frames_, &QComboBox::currentIndexChanged, this, [this] { showImage(); });
    connect(layers_, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
        const int row = layers_->row(item);
        if (row >= 0 && row < layerItems_.size())
            layerItems_[row]->setVisible(item->checkState() == Qt::Checked);
    });
    connect(layers_, &QListWidget::currentRowChanged, this, [this](int row) {
        canvas_->clearSelection();
        if (row >= 0 && row < layerItems_.size())
            layerItems_[row]->setSelected(true);
    });
    connect(scenes_, &QListWidget::currentRowChanged, this, [this] { selectScene(); });
    connect(mode_, &QComboBox::currentIndexChanged, this, [this] { selectScene(); });
    connect(requests_, &QComboBox::currentIndexChanged, this, [this] { selectRequest(); });
    connect(images_, &QComboBox::currentIndexChanged, this,
            [this] { selectImage(images_->currentData().toString()); });
    connect(assets_, &QListWidget::currentRowChanged, this, [this] {
        if (catalogTabs_->currentIndex() == 1 && assets_->currentItem())
            selectImage(assets_->currentItem()->data(Qt::UserRole).toString());
    });
    connect(catalogTabs_, &QTabWidget::currentChanged, this, [this, pages](int tab) {
        pages->setTabEnabled(1, tab == 0);
        if (actionEditor_) pages->setTabEnabled(pages->indexOf(actionEditor_), tab == 0);
        mode_->setEnabled(tab == 0);
        requests_->setEnabled(tab == 0);
        images_->setEnabled(tab == 0);
        if (tab) {
            backgroundChoice_->setEnabled(false);
            backgroundApply_->setEnabled(false);
            backgroundRestore_->setEnabled(false);
        }
        if (!tab)
            selectScene();
        else if (assets_->currentItem())
            selectImage(assets_->currentItem()->data(Qt::UserRole).toString());
        else
            assets_->setCurrentRow(0);
    });
    for (const auto &asset : SceneImages::catalog()) {
        auto item = new QListWidgetItem(asset.name + " · " + asset.id, assets_);
        item->setData(Qt::UserRole, asset.id);
        item->setToolTip(asset.container + " · resource " + hex(asset.key));
    }
    struct Result {
        std::optional<ScenePrograms> value;
        QString error;
    };
    auto watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcher<Result>::finished, this, [this, watcher] {
        auto result = watcher->result();
        watcher->deleteLater();
        if (!result.value) {
            sceneStatus_->setText(result.error);
            return;
        }
        readOnlyPrograms_ = std::move(result.value);
        directors_ = readOnlyPrograms_->original();
        reloadScenes();
    });
    const auto snapshot = project_ ? project_->scenePrograms() : std::optional<ScenePrograms>{};
    watcher->setFuture(QtConcurrent::run([rom = rom_, snapshot] {
        try {
            return Result{snapshot ? *snapshot : ScenePrograms::load(rom), {}};
        } catch (const std::exception &e) {
            return Result{{}, QString::fromUtf8(e.what())};
        }
    }));
}
void CutsceneEditor::setSaveProvider(std::function<const SaveDocument *()> provider) {
    scenePreview_->setSaveProvider(std::move(provider));
}
SceneId CutsceneEditor::selectedScene() const {
    const auto item = scenes_->currentItem();
    if (!item)
        return {};
    const auto authored = item->data(Qt::UserRole + 1).toString();
    return authored.isEmpty() ? SceneId(item->data(Qt::UserRole).toUInt())
                              : SceneId::authored(authored);
}
bool CutsceneEditor::invalidSceneInput() const {
    return (commandEditor_ && commandEditor_->property("invalidEdits").toBool()) ||
           (actionEditor_ && actionEditor_->property("invalidEdits").toBool());
}
void CutsceneEditor::refreshSceneInputState() {
    const bool valid = !invalidSceneInput();
    commands_->setEnabled(valid);
    scenes_->setEnabled(valid);
    mode_->setEnabled(valid);
    if (commandEditor_)
        commandEditor_->setEnabled(!actionEditor_ || !actionEditor_->property("invalidEdits").toBool());
    if (actionEditor_)
        actionEditor_->setEnabled(!commandEditor_ || !commandEditor_->property("invalidEdits").toBool());
    if (valid) reloadScenes();
    else {
        backgroundChoice_->setEnabled(false);
        backgroundApply_->setEnabled(false);
        backgroundRestore_->setEnabled(false);
        for (auto button : {newScene_, duplicateScene_, renameScene_, removeScene_, wipeScene_})
            button->setEnabled(false);
    }
}
const ScenePrograms &CutsceneEditor::programs() const {
    return project_ ? *project_->scenePrograms() : *readOnlyPrograms_;
}
void CutsceneEditor::reloadScenes(SceneId preferred) {
    if (!readOnlyPrograms_ && (!project_ || !project_->scenePrograms()))
        return;
    if (!preferred)
        preferred = selectedScene();
    QJsonArray identities;
    for (const auto &scene : programs().authoredScenes())
        identities << QJsonObject{
            {"id", scene.id}, {"name", scene.name}, {"selector", scene.selector}};
    const auto signature =
        QString::fromUtf8(QJsonDocument(identities).toJson(QJsonDocument::Compact));
    const QSignalBlocker block(scenes_);
    if (!scenes_->count() || sceneCatalogSignature_ != signature) {
        scenes_->clear();
        const auto search = findChild<QLineEdit *>("cutsceneScenesSearch");
        for (const auto &id : programs().scenes()) {
            const auto &resource = programs().resource(id);
            QStringList selectors;
            for (const auto row : resource.selectors)
                selectors << QString::number(row);
            const auto name = id.authoredId.isEmpty()
                                  ? resource.name
                                  : programs().authoredScenes()[id.authoredId].name;
            auto item =
                new QListWidgetItem(name + "\n" +
                                        (id.authoredId.isEmpty() ? hex(resource.envelope + 4)
                                                                 : QString("Authored scene")) +
                                        " · selectors " + selectors.join(", "),
                                    scenes_);
            item->setData(Qt::UserRole, id.nativeKey);
            item->setData(Qt::UserRole + 1, id.authoredId);
            item->setHidden(search && !item->text().contains(search->text(), Qt::CaseInsensitive));
        }
        sceneCatalogSignature_ = signature;
    }
    int selected = 0;
    for (int i = 0; i < scenes_->count(); ++i) {
        const auto item = scenes_->item(i);
        if (item->data(Qt::UserRole).toUInt() == preferred.nativeKey &&
            item->data(Qt::UserRole + 1).toString() == preferred.authoredId) {
            selected = i;
            break;
        }
    }
    scenes_->setCurrentRow(selected);
    selectScene();
}
void CutsceneEditor::createScene(bool blank) {
    if (!project_)
        return;
    try {
        QDialog dialog(this);
        dialog.setObjectName("sceneCreateDialog");
        dialog.setWindowTitle(blank ? "New scene" : "Duplicate scene");
        auto layout = new QFormLayout(&dialog);
        auto name = new QLineEdit(blank ? "New scene" : "Copy of scene");
        name->setObjectName("newSceneName");
        name->setMaxLength(120);
        layout->addRow("Name", name);
        auto source = selectedScene();
        QComboBox *templates = nullptr;
        if (blank) {
            templates = new QComboBox;
            templates->setObjectName("newSceneTemplate");
            for (const auto &id : programs().scenes()) {
                if (!programs().canCreateBlank(id))
                    continue;
                const auto &scene = programs().resource(id);
                const auto label = id.authoredId.isEmpty()
                                       ? scene.name
                                       : programs().authoredScenes()[id.authoredId].name;
                const auto route =
                    programs().program(id).resourceClass() == 5 ? "Room" : "Environment";
                templates->addItem(QString("%1 · %2").arg(route, label), id.nativeKey);
                templates->setItemData(templates->count() - 1, id.authoredId, Qt::UserRole + 1);
                if (id == source)
                    templates->setCurrentIndex(templates->count() - 1);
            }
            if (!templates->count())
                throw Error(
                    "This ROM has no supported room or environment setup for a blank scene");
            layout->addRow("Setup template", templates);
        } else if (source) {
            const auto &scene = programs().resource(source);
            name->setText("Copy of " + (source.authoredId.isEmpty()
                                            ? scene.name
                                            : programs().authoredScenes()[source.authoredId].name));
        } else
            return;
        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addRow(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted)
            return;
        if (templates) {
            const auto uuid = templates->currentData(Qt::UserRole + 1).toString();
            source = uuid.isEmpty() ? SceneId(templates->currentData().toUInt())
                                    : SceneId::authored(uuid);
        }
        auto next = *project_->scenePrograms();
        const auto created = next.createScene(name->text(), source, blank);
        pushSceneProgramChange(project_, undo_, blank ? "Create scene" : "Duplicate scene", next);
        findChild<QLineEdit *>("cutsceneScenesSearch")->clear();
        reloadScenes(created);
        scenes_->scrollToItem(scenes_->currentItem());
    } catch (const std::exception &e) {
        QMessageBox::warning(this, "Scene creation", QString::fromUtf8(e.what()));
    }
}
void CutsceneEditor::renameScene() {
    const auto id = selectedScene();
    if (!project_ || id.authoredId.isEmpty())
        return;
    bool accepted = false;
    const auto name =
        QInputDialog::getText(this, "Rename scene", "Name", QLineEdit::Normal,
                              programs().authoredScenes()[id.authoredId].name, &accepted);
    if (!accepted)
        return;
    try {
        auto next = *project_->scenePrograms();
        next.renameScene(id, name);
        pushSceneProgramChange(project_, undo_, "Rename scene", next);
    } catch (const std::exception &e) {
        QMessageBox::warning(this, "Scene name", QString::fromUtf8(e.what()));
    }
}
void CutsceneEditor::wipeScene() {
    if (!project_)
        return;
    try {
        auto next = *project_->scenePrograms();
        next.replace(selectedScene(), wipeSceneProgram(next.program(selectedScene())).source);
        pushSceneProgramChange(project_, undo_, "Clear scene content", next);
    } catch (const std::exception &e) {
        QMessageBox::warning(this, "Scene content", QString::fromUtf8(e.what()));
    }
}
void CutsceneEditor::removeScene() {
    if (!project_)
        return;
    try {
        auto next = *project_->scenePrograms();
        next.removeScene(selectedScene());
        pushSceneProgramChange(project_, undo_, "Remove authored scene", next);
    } catch (const std::exception &e) {
        QMessageBox::warning(this, "Scene removal", QString::fromUtf8(e.what()));
    }
}
void CutsceneEditor::selectScene() {
    if (invalidSceneInput())
        return;
    if (!directors_ || !scenes_->currentItem() || catalogTabs_->currentIndex() != 0)
        return;
    const auto key = selectedScene();
    transformPreview_->clear();
    auto scene = programs().resource(key);
    scene.program = programs().program(key);
    if (project_) {
        const bool authored = !key.authoredId.isEmpty();
        newScene_->setEnabled(true);
        duplicateScene_->setEnabled(true);
        renameScene_->setEnabled(authored);
        removeScene_->setEnabled(authored);
        try {
            const auto wiped = wipeSceneProgram(scene.program);
            wipeScene_->setEnabled(wiped.source != scene.program.source);
            wipeScene_->setToolTip("Remove scene actions while retaining its background setup, "
                                   "initial cameras and exit. This can be undone.");
        } catch (const std::exception &e) {
            wipeScene_->setEnabled(false);
            wipeScene_->setToolTip(QString::fromUtf8(e.what()));
        }
    }
    const int selectedCommand = commands_->currentRow();
    const QSignalBlocker commandsBlock(commands_);
    try {
        backgrounds_ = SceneImages::backgrounds(rom_, scene, mode_->currentData().toInt());
    } catch (const std::exception &e) {
        backgrounds_ = {};
        backgrounds_.modeEvidence =
            QString("Preview unavailable: %1").arg(QString::fromUtf8(e.what()));
    }
    scenePreview_->selectScene(programs(), key, backgrounds_.mode,
                               project_ ? project_->dialogues() : std::optional<Dialogues>{});
    commands_->setRowCount(scene.program.commands.size());
    for (int i = 0; i < scene.program.commands.size(); ++i) {
        const auto &cmd = scene.program.commands[i];
        const auto &def = Directors::grammar()[cmd.opcode];
        QStringList words, meanings;
        for (int j = 0; j < cmd.operands.size(); ++j) {
            words << hex(cmd.operands[j]);
            if (j < def.operands.size())
                meanings << def.operands[j] + " = " + QString::number(qint32(cmd.operands[j]));
        }
        commands_->setItem(i, 0, new QTableWidgetItem(QString::number(cmd.word)));
        commands_->setItem(i, 1, new QTableWidgetItem(def.name));
        commands_->setItem(i, 2, new QTableWidgetItem(words.join(" ")));
        auto meaning = new QTableWidgetItem(meanings.join("; "));
        meaning->setToolTip(def.description);
        commands_->setItem(i, 3, meaning);
    }
    commands_->setCurrentCell(qBound(0, selectedCommand, int(scene.program.commands.size()) - 1),
                              0);
    if (commandEditor_)
        commandEditor_->selectCommand(key, commands_->currentRow());
    if (actionEditor_)
        actionEditor_->selectScene(key, commands_->currentRow());
    const QSignalBlocker block(requests_);
    requests_->clear();
    for (const auto &r : backgrounds_.requests)
        requests_->addItem(QString("Word %1 · selector %2").arg(r.word).arg(r.selector));
    selectRequest();
    refreshBackgroundEdit();
}
void CutsceneEditor::selectRequest() {
    const QSignalBlocker block(images_);
    images_->clear();
    QString status = backgrounds_.modeEvidence;
    const int index = requests_->currentIndex();
    if (index >= 0 && index < backgrounds_.requests.size()) {
        const auto &r = backgrounds_.requests[index];
        status += "\n" + r.status;
        for (const auto &id : r.images) {
            const auto &asset = SceneImages::catalog()[id];
            images_->addItem(asset.name + " · " + id, id);
        }
    } else
        status += "\nNo background request in this stream. Launch context may provide scenery.";
    sceneStatus_->setText(status);
    selectImage(images_->currentData().toString());
}
void CutsceneEditor::selectImage(const QString &id) {
    const auto generation = ++generation_;
    imageId_ = id;
    image_.reset();
    transformPreview_->setImage({});
    layerItems_.clear();
    canvas_->clear();
    layers_->clear();
    frames_->clear();
    imageStatus_->setText(id.isEmpty() ? "No image selected." : "Decoding " + id + "…");
    setProperty("displayedImage", QString());
    if (catalogTabs_->currentIndex() == 1)
        sceneStatus_->setText(
            "Image catalog. Layer visibility and frame selection affect the preview only.");
    // Keep at most one decoder active. Rapid selection replaces the pending request.
    if (id.isEmpty() || decoding_)
        return;
    decoding_ = true;
    struct Result {
        std::optional<DecodedImage> value;
        QString error;
    };
    auto watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcher<Result>::finished, this, [this, watcher, generation, id] {
        auto result = watcher->result();
        watcher->deleteLater();
        decoding_ = false;
        if (generation != generation_) {
            selectImage(imageId_);
            return;
        }
        if (!result.value) {
            imageStatus_->setText(result.error);
            return;
        }
        image_ = std::move(result.value);
        transformPreview_->setImage(image_);
        {
            const QSignalBlocker block(frames_);
            frames_->addItem("Complete image", -1);
            if (image_->frames.size() > 1)
                for (int i = 0; i < image_->frames.size(); ++i)
                    frames_->addItem(QString("Frame / palette %1").arg(i + 1), i);
        }
        setProperty("displayedImage", id);
        showImage();
    });
    watcher->setFuture(QtConcurrent::run([rom = rom_, id] {
        try {
            return Result{SceneImages::decode(rom, id), {}};
        } catch (const std::exception &e) {
            return Result{{}, QString::fromUtf8(e.what())};
        }
    }));
}
void CutsceneEditor::showImage() {
    if (!image_)
        return;
    const QSignalBlocker block(layers_);
    layerItems_.clear();
    canvas_->clear();
    layers_->clear();
    const int frame = frames_->currentData().toInt();
    auto add = [this](const RasterImage &r, QPoint origin, const QString &label) {
        auto item = canvas_->addPixmap(pixmap(r));
        item->setPos(origin);
        item->setZValue(layerItems_.size());
        item->setFlag(QGraphicsItem::ItemIsSelectable);
        layerItems_ << item;
        auto row = new QListWidgetItem(label, layers_);
        row->setFlags(row->flags() | Qt::ItemIsUserCheckable);
        row->setCheckState(Qt::Checked);
    };
    if (frame >= 0 && frame < image_->frames.size())
        add(image_->frames[frame], {}, "Selected frame");
    else if (image_->layers.size() > 1)
        for (int i = 0; i < image_->layers.size(); ++i) {
            const auto &l = image_->layers[i];
            add(l.image, l.origin,
                QString("Layer %1 · %2, %3\n%4 × %5")
                    .arg(i + 1)
                    .arg(l.origin.x())
                    .arg(l.origin.y())
                    .arg(l.image.width)
                    .arg(l.image.height));
        }
    else
        add(image_->composite, image_->origin, "Complete image");
    const bool singleFrame = frame >= 0 && frame < image_->frames.size();
    const auto &raster = singleFrame ? image_->frames[frame] : image_->composite;
    canvas_->setSceneRect(
        QRectF(singleFrame ? QPoint() : image_->origin, QSize(raster.width, raster.height)));
    view_->fitInView(canvas_->sceneRect(), Qt::KeepAspectRatio);
    imageStatus_->setText(QString("%1 · %2 × %3 · origin %4, %5\n%6")
                              .arg(imageId_)
                              .arg(image_->composite.width)
                              .arg(image_->composite.height)
                              .arg(image_->origin.x())
                              .arg(image_->origin.y())
                              .arg(image_->limitations.join("\n")));
}
} // namespace lordly
