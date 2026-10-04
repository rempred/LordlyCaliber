#include "app/cutscene_editor.h"
#include "app/actor_pose_editor.h"
#include "app/dialogue_inspector.h"
#include "app/scene_command_editor.h"
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
        removeScene_ = new QPushButton("Remove new scene");
        removeScene_->setObjectName("sceneRemove");
        actions->addWidget(renameScene_);
        actions->addWidget(removeScene_);
        static_cast<QVBoxLayout *>(scenes_->parentWidget()->layout())->addLayout(actions);
        for (auto button : {newScene_, duplicateScene_, renameScene_, removeScene_})
            button->setEnabled(false);
        connect(newScene_, &QPushButton::clicked, this, [this] { createScene(true); });
        connect(duplicateScene_, &QPushButton::clicked, this, [this] { createScene(false); });
        connect(renameScene_, &QPushButton::clicked, this, &CutsceneEditor::renameScene);
        connect(removeScene_, &QPushButton::clicked, this, &CutsceneEditor::removeScene);
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
            const bool valid = !commandEditor_->property("invalidEdits").toBool();
            commands_->setEnabled(valid);
            scenes_->setEnabled(valid);
            mode_->setEnabled(valid);
            if (valid)
                reloadScenes();
            else
                for (auto button : {newScene_, duplicateScene_, renameScene_, removeScene_})
                    button->setEnabled(false);
        });
    } else
        pages->addTab(commands_, "Source commands");
    pages->addTab(new ActorPoseEditor(rom_), "Pose inspector");
    auto dialogue =
        project ? new DialogueInspector(project, undo, changed) : new DialogueInspector(rom_);
    pages->addTab(dialogue, "Dialogue");
    commands_->setToolTip("Double-click a dialogue command to inspect its archive entry.");
    connect(commands_, &QTableWidget::cellDoubleClicked, this,
            [this, pages, dialogue](int row, int) {
                if (!directors_ || !scenes_->currentItem())
                    return;
                const auto program = programs().program(selectedScene());
                if (row < 0 || row >= program.commands.size())
                    return;
                const auto &command = program.commands[row];
                if (command.opcode != 0xbf || command.operands.size() != 13)
                    return;
                dialogue->selectEntry(quint16(command.operands[1]), quint16(command.operands[2]));
                pages->setCurrentWidget(dialogue);
            });
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
        mode_->setEnabled(tab == 0);
        requests_->setEnabled(tab == 0);
        images_->setEnabled(tab == 0);
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
SceneId CutsceneEditor::selectedScene() const {
    const auto item = scenes_->currentItem();
    if (!item)
        return {};
    const auto authored = item->data(Qt::UserRole + 1).toString();
    return authored.isEmpty() ? SceneId(item->data(Qt::UserRole).toUInt())
                              : SceneId::authored(authored);
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
                templates->addItem(label, id.nativeKey);
                templates->setItemData(templates->count() - 1, id.authoredId, Qt::UserRole + 1);
                if (id == source)
                    templates->setCurrentIndex(templates->count() - 1);
            }
            if (!templates->count())
                throw Error("This ROM has no intact class-five setup template for a blank scene");
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
void CutsceneEditor::removeScene() {
    if (!project_)
        return;
    try {
        auto next = *project_->scenePrograms();
        next.removeNewScene(selectedScene());
        pushSceneProgramChange(project_, undo_, "Remove new scene", next);
    } catch (const std::exception &e) {
        QMessageBox::warning(this, "Scene removal", QString::fromUtf8(e.what()));
    }
}
void CutsceneEditor::selectScene() {
    if (commandEditor_ && commandEditor_->property("invalidEdits").toBool())
        return;
    if (!directors_ || !scenes_->currentItem() || catalogTabs_->currentIndex() != 0)
        return;
    const auto key = selectedScene();
    auto scene = programs().resource(key);
    scene.program = programs().program(key);
    if (project_) {
        const bool authored = !key.authoredId.isEmpty();
        newScene_->setEnabled(true);
        duplicateScene_->setEnabled(true);
        renameScene_->setEnabled(authored);
        removeScene_->setEnabled(authored && scene.key == 0);
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
    const QSignalBlocker block(requests_);
    requests_->clear();
    for (const auto &r : backgrounds_.requests)
        requests_->addItem(QString("Word %1 · selector %2").arg(r.word).arg(r.selector));
    selectRequest();
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
