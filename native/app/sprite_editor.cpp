#include "app/sprite_editor.h"
#include "app/art_image_assignment.h"
#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QClipboard>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QImageReader>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QToolBar>
#include <QUndoStack>
#include <QVBoxLayout>
namespace lordly {
namespace {
class SpriteCommand final : public QUndoCommand {
  public:
    SpriteCommand(Project *project, const QString &label, SpriteLibrary after)
        : QUndoCommand(label), project_(project), before_(project->sprites()),
          after_(std::move(after)) {}
    void undo() override {
        project_->setSprites(before_);
    }
    void redo() override {
        project_->setSprites(after_);
    }

  private:
    Project *project_;
    SpriteLibrary before_, after_;
};
QSpinBox *spin(const char *id, int low, int high) {
    auto widget = new QSpinBox;
    widget->setObjectName(id);
    widget->setRange(low, high);
    widget->setKeyboardTracking(false);
    return widget;
}
QPushButton *button(QHBoxLayout *layout, const QString &text, const char *id) {
    auto widget = new QPushButton(text);
    widget->setObjectName(id);
    layout->addWidget(widget);
    return widget;
}
QRect opaqueBounds(const QImage &image) {
    QRect bounds;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (image.constScanLine(y)[x * 4 + 3])
                bounds = bounds.united(QRect(x, y, 1, 1));
    return bounds;
}
SpriteAssetFrame newFrame(QSize size) {
    return SpriteAsset::blank("Frame", size).frames.first();
}
SpriteLayer newLayer(QSize size) {
    return newFrame(size).layers.first();
}
} // namespace
SpriteEditor::SpriteEditor(Project *project, QUndoStack *undo, std::function<void()> changed,
                           QWidget *parent)
    : QWidget(parent), project_(project), undo_(undo), changed_(std::move(changed)) {
    setObjectName("spriteEditor");
    auto outer = new QVBoxLayout(this);
    auto heading = new QLabel(
        "<b>Sprite studio</b> · Create reusable artwork, frames and animation sequences");
    outer->addWidget(heading);
    auto actions = new QHBoxLayout;
    outer->addLayout(actions);
    connect(button(actions, "New…", "spriteNew"), &QPushButton::clicked, this,
            &SpriteEditor::createAsset);
    connect(button(actions, "Import…", "spriteImport"), &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getOpenFileName(
            this, "Import sprite asset or image", {},
            "Sprite assets and images (*.json *.lcsprite *.png *.jpg *.jpeg)");
        if (!path.isEmpty())
            guarded([&] { importFile(path); });
    });
    auto exportButton = button(actions, "Export", "spriteExport");
    auto exports = new QMenu(exportButton);
    exportButton->setMenu(exports);
    for (const auto &format : {QByteArray("PNG"), QByteArray("JPEG")})
        exports->addAction(QString::fromLatin1(format) + " image…", this, [this, format] {
            if (!current())
                return;
            const auto path = QFileDialog::getSaveFileName(
                this, "Export selected frame",
                current()->name + (format == "PNG" ? ".png" : ".jpg"),
                format == "PNG" ? "PNG (*.png)" : "JPEG (*.jpg *.jpeg)");
            if (!path.isEmpty())
                guarded([&] { exportImage(path, format); });
        });
    for (bool browser : {false, true})
        exports->addAction(
            browser ? "Browser-compatible asset…" : "Native sprite asset…", this, [this, browser] {
                if (!current())
                    return;
                const auto path = QFileDialog::getSaveFileName(this, "Export all frames and layers",
                                                               current()->name + ".json",
                                                               "Sprite asset (*.json *.lcsprite)");
                if (!path.isEmpty())
                    guarded([&] { exportAsset(path, browser); });
            });
    connect(button(actions, "Duplicate asset", "spriteDuplicate"), &QPushButton::clicked, this,
            [this] {
                if (!current())
                    return;
                auto asset = *current();
                asset.id = SpriteLibrary::newId();
                asset.name = (asset.name.left(110) + " copy");
                addAsset(std::move(asset));
            });
    connect(button(actions, "Delete asset", "spriteDelete"), &QPushButton::clicked, this, [this] {
        if (!current())
            return;
        auto library = project_->sprites();
        library.remove(selected_);
        commit("Delete sprite asset", std::move(library));
    });
    actions->addStretch();
    auto split = new QSplitter;
    outer->addWidget(split, 1);
    auto libraryPanel = new QWidget;
    auto libraryLayout = new QVBoxLayout(libraryPanel);
    libraryLayout->setContentsMargins(0, 0, 0, 0);
    search_ = new QLineEdit;
    search_->setPlaceholderText("Search assets…");
    libraryLayout->addWidget(search_);
    assets_ = new QListWidget;
    assets_->setObjectName("spriteAssets");
    libraryLayout->addWidget(assets_);
    split->addWidget(libraryPanel);
    editing_ = new QWidget;
    auto work = new QVBoxLayout(editing_);
    work->setContentsMargins(0, 0, 0, 0);
    split->addWidget(editing_);
    auto metadata = new QHBoxLayout;
    work->addLayout(metadata);
    name_ = new QLineEdit;
    name_->setObjectName("spriteName");
    name_->setMaxLength(120);
    metadata->addWidget(name_, 1);
    info_ = new QLabel;
    metadata->addWidget(info_);
    connect(button(metadata, "Resize…", "spriteResize"), &QPushButton::clicked, this,
            &SpriteEditor::resizeAsset);
    for (bool clockwise : {false, true})
        connect(button(metadata, clockwise ? "Rotate right" : "Rotate left",
                       clockwise ? "spriteRotateRight" : "spriteRotateLeft"),
                &QPushButton::clicked, this, [this, clockwise] {
                    change("Rotate sprite", [&](SpriteAsset &a) { a = a.rotated(clockwise); });
                });
    auto tools = new QHBoxLayout;
    work->addLayout(tools);
    auto tool = new QComboBox;
    tool->setObjectName("spriteTool");
    tool->addItems({"Pencil", "Eraser", "Fill", "Eyedropper", "Selection", "Move"});
    tools->addWidget(tool);
    colorButton_ = button(tools, "Color", "spriteColor");
    connect(colorButton_, &QPushButton::clicked, this, [this] {
        const auto color =
            QColorDialog::getColor(color_, this, "Paint color", QColorDialog::ShowAlphaChannel);
        if (color.isValid())
            updateColor(color);
    });
    tools->addWidget(new QLabel("Brush"));
    auto brush = spin("spriteBrush", 1, 64);
    tools->addWidget(brush);
    tools->addWidget(new QLabel("Zoom"));
    auto zoom = spin("spriteZoom", 1, 32);
    zoom->setSuffix("×");
    zoom->setValue(8);
    tools->addWidget(zoom);
    auto grid = new QCheckBox("Grid");
    grid->setChecked(true);
    tools->addWidget(grid);
    connect(button(tools, "Copy", "spriteCopyPixels"), &QPushButton::clicked, this,
            [this] { copyPixels(false); });
    connect(button(tools, "Cut", "spriteCutPixels"), &QPushButton::clicked, this,
            [this] { copyPixels(true); });
    connect(button(tools, "Paste layer", "spritePastePixels"), &QPushButton::clicked, this,
            &SpriteEditor::pastePixels);
    auto clearSelection = button(tools, "Deselect", "spriteDeselect");
    tools->addStretch();
    auto center = new QSplitter;
    work->addWidget(center, 1);
    auto scroll = new QScrollArea;
    scroll->setAlignment(Qt::AlignCenter);
    scroll->setWidgetResizable(false);
    canvas_ = new SpriteCanvas;
    scroll->setWidget(canvas_);
    center->addWidget(scroll);
    connect(clearSelection, &QPushButton::clicked, canvas_, &SpriteCanvas::clearSelection);
    connect(tool, &QComboBox::currentIndexChanged, this,
            [this](int index) { canvas_->setTool(SpriteCanvas::Tool(index)); });
    connect(brush, &QSpinBox::valueChanged, canvas_, &SpriteCanvas::setBrush);
    connect(zoom, &QSpinBox::valueChanged, canvas_, &SpriteCanvas::setZoom);
    connect(grid, &QCheckBox::toggled, canvas_, &SpriteCanvas::setGrid);
    connect(canvas_, &SpriteCanvas::colorPicked, this, &SpriteEditor::updateColor);
    connect(canvas_, &SpriteCanvas::committed, this, [this](const QImage &image, QPoint position) {
        change("Paint sprite layer", [&](SpriteAsset &a) {
            auto &list = a.frames[frames_->currentRow()].layers;
            auto &layer = list[list.size() - 1 - layers_->currentRow()];
            layer.image = image;
            layer.position = position;
        });
    });
    auto side = new QWidget;
    auto sideLayout = new QVBoxLayout(side);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    center->addWidget(side);
    auto anchorForm = new QFormLayout;
    sideLayout->addLayout(anchorForm);
    anchorX_ = spin("spriteAnchorX", -32768, 32767);
    anchorY_ = spin("spriteAnchorY", -32768, 32767);
    anchorForm->addRow("Anchor X", anchorX_);
    anchorForm->addRow("Anchor Y", anchorY_);
    sideLayout->addWidget(new QLabel("Layers · higher rows draw last"));
    layers_ = new QListWidget;
    layers_->setObjectName("spriteLayers");
    sideLayout->addWidget(layers_, 1);
    auto layerActions = new QHBoxLayout;
    sideLayout->addLayout(layerActions);
    connect(button(layerActions, "+", "spriteAddLayer"), &QPushButton::clicked, this, [this] {
        change("Add sprite layer", [&](SpriteAsset &a) {
            auto &list = a.frames[frames_->currentRow()].layers;
            auto layer = newLayer(a.size);
            layer.name = QString("Layer %1").arg(list.size() + 1);
            list.append(layer);
        });
        if (current())
            layers_->setCurrentRow(0);
    });
    connect(button(layerActions, "Copy", "spriteDuplicateLayer"), &QPushButton::clicked, this,
            [this] {
                change("Duplicate sprite layer", [&](SpriteAsset &a) {
                    auto &list = a.frames[frames_->currentRow()].layers;
                    auto layer = list[list.size() - 1 - layers_->currentRow()];
                    layer.id = SpriteLibrary::newId();
                    layer.name = layer.name.left(110) + " copy";
                    list.append(layer);
                });
                if (current())
                    layers_->setCurrentRow(0);
            });
    connect(button(layerActions, "−", "spriteRemoveLayer"), &QPushButton::clicked, this, [this] {
        change("Remove sprite layer", [&](SpriteAsset &a) {
            auto &list = a.frames[frames_->currentRow()].layers;
            if (list.size() == 1)
                throw Error("A frame needs at least one layer");
            list.removeAt(list.size() - 1 - layers_->currentRow());
        });
    });
    for (int direction : {-1, 1})
        connect(button(layerActions, direction < 0 ? "↑" : "↓",
                       direction < 0 ? "spriteLayerUp" : "spriteLayerDown"),
                &QPushButton::clicked, this, [this, direction] {
                    const int row = layers_->currentRow();
                    change("Reorder sprite layers", [&](SpriteAsset &a) {
                        auto &list = a.frames[frames_->currentRow()].layers;
                        const int at = int(list.size()) - 1 - row, to = at - direction;
                        if (to < 0 || to >= list.size())
                            return;
                        list.move(at, to);
                    });
                    layers_->setCurrentRow(std::clamp(row + direction, 0, layers_->count() - 1));
                });
    auto layerForm = new QFormLayout;
    sideLayout->addLayout(layerForm);
    layerName_ = new QLineEdit;
    layerName_->setObjectName("spriteLayerName");
    layerName_->setMaxLength(120);
    layerForm->addRow("Name", layerName_);
    visible_ = new QCheckBox("Visible");
    visible_->setObjectName("spriteLayerVisible");
    layerForm->addRow(visible_);
    layerX_ = spin("spriteLayerX", -32768, 32767);
    layerY_ = spin("spriteLayerY", -32768, 32767);
    layerForm->addRow("Offset X", layerX_);
    layerForm->addRow("Offset Y", layerY_);
    auto importLayer = new QPushButton("Import layer image…");
    importLayer->setObjectName("spriteImportLayer");
    sideLayout->addWidget(importLayer);
    connect(importLayer, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getOpenFileName(this, "Import selected layer image", {},
                                                       "Images (*.png *.jpg *.jpeg)");
        if (!path.isEmpty()) guarded([&] { importLayerImage(path); });
    });
    auto transforms = new QHBoxLayout;
    sideLayout->addLayout(transforms);
    for (bool horizontal : {true, false})
        connect(button(transforms, horizontal ? "Flip H" : "Flip V",
                       horizontal ? "spriteFlipH" : "spriteFlipV"),
                &QPushButton::clicked, this, [this, horizontal] {
                    change("Flip sprite layer", [&](SpriteAsset &a) {
                        auto &list = a.frames[frames_->currentRow()].layers;
                        auto &layer = list[list.size() - 1 - layers_->currentRow()];
                        layer.image =
                            layer.image.flipped(horizontal ? Qt::Horizontal : Qt::Vertical);
                    });
                });
    connect(button(transforms, "Trim", "spriteTrim"), &QPushButton::clicked, this, [this] {
        change("Trim transparent layer edges", [&](SpriteAsset &a) {
            auto &list = a.frames[frames_->currentRow()].layers;
            auto &layer = list[list.size() - 1 - layers_->currentRow()];
            const auto rect = opaqueBounds(layer.image);
            if (rect.isEmpty())
                throw Error("This layer is fully transparent");
            layer.image = layer.image.copy(rect);
            layer.position += rect.topLeft();
        });
    });
    center->setStretchFactor(0, 1);
    center->setSizes({700, 250});
    auto frameBar = new QHBoxLayout;
    work->addLayout(frameBar);
    play_ = button(frameBar, "Play", "spritePlay");
    play_->setCheckable(true);
    connect(button(frameBar, "+ Frame", "spriteAddFrame"), &QPushButton::clicked, this, [this] {
        change("Add sprite frame", [&](SpriteAsset &a) {
            auto frame = newFrame(a.size);
            frame.name = QString("Frame %1").arg(a.frames.size() + 1);
            a.frames.append(frame);
        });
        if (current())
            frames_->setCurrentRow(int(current()->frames.size()) - 1);
    });
    connect(button(frameBar, "Copy frame", "spriteDuplicateFrame"), &QPushButton::clicked, this,
            [this] {
                change("Duplicate sprite frame", [&](SpriteAsset &a) {
                    auto frame = a.frames[frames_->currentRow()];
                    frame.id = SpriteLibrary::newId();
                    frame.name = frame.name.left(110) + " copy";
                    a.frames.append(frame);
                });
                if (current())
                    frames_->setCurrentRow(int(current()->frames.size()) - 1);
            });
    connect(button(frameBar, "− Frame", "spriteRemoveFrame"), &QPushButton::clicked, this, [this] {
        change("Remove sprite frame", [&](SpriteAsset &a) {
            if (a.frames.size() == 1)
                throw Error("An asset needs at least one frame");
            a.frames.removeAt(frames_->currentRow());
        });
    });
    for (int direction : {-1, 1})
        connect(button(frameBar, direction < 0 ? "←" : "→",
                       direction < 0 ? "spriteFrameLeft" : "spriteFrameRight"),
                &QPushButton::clicked, this, [this, direction] {
                    const int row = frames_->currentRow();
                    change("Reorder sprite frames", [&](SpriteAsset &a) {
                        const int to = row + direction;
                        if (to >= 0 && to < a.frames.size())
                            a.frames.move(row, to);
                    });
                    frames_->setCurrentRow(std::clamp(row + direction, 0, frames_->count() - 1));
                });
    frameName_ = new QLineEdit;
    frameName_->setObjectName("spriteFrameName");
    frameName_->setMaxLength(120);
    frameBar->addWidget(frameName_, 1);
    frameBar->addWidget(new QLabel("Ticks (30/s)"));
    ticks_ = spin("spriteFrameTicks", 0, 255);
    frameBar->addWidget(ticks_);
    frames_ = new QListWidget;
    frames_->setObjectName("spriteFrames");
    frames_->setFlow(QListView::LeftToRight);
    frames_->setMaximumHeight(85);
    work->addWidget(frames_);
    cursor_ = new QLabel(
        "Paint edits the selected layer. Escape cancels a stroke or clears the selection.");
    work->addWidget(cursor_);
    notice_ = new QLabel("Assets are saved in this project. Assign artwork to a game resource to "
                         "include it in a ROM.");
    notice_->setWordWrap(true);
    outer->addWidget(notice_);
    split->setStretchFactor(1, 1);
    split->setSizes({210, 1100});
    timer_ = new QTimer(this);
    timer_->setTimerType(Qt::PreciseTimer);
    timer_->setInterval(16);
    connect(timer_, &QTimer::timeout, this, &SpriteEditor::tick);
    connect(play_, &QPushButton::toggled, this, &SpriteEditor::play);
    connect(canvas_, &SpriteCanvas::cursorChanged, this, [this](QPoint p) {
        cursor_->setText(QString("X %1 · Y %2 · Selected layer pixels · Escape cancels / deselects")
                             .arg(p.x())
                             .arg(p.y()));
    });
    connect(search_, &QLineEdit::textChanged, this, [this] {
        for (int i = 0; i < assets_->count(); ++i)
            assets_->item(i)->setHidden(
                !assets_->item(i)->text().contains(search_->text(), Qt::CaseInsensitive));
    });
    connect(assets_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (refreshing_)
            return;
        selected_ = row < 0 ? QString() : assets_->item(row)->data(Qt::UserRole).toString();
        refresh();
    });
    connect(frames_, &QListWidget::currentRowChanged, this, [this] {
        if (!refreshing_)
            refreshFrame(true);
    });
    connect(layers_, &QListWidget::currentRowChanged, this, [this] {
        if (!refreshing_)
            refreshFrame(true);
    });
    connect(name_, &QLineEdit::editingFinished, this, [this] {
        if (!refreshing_ && current() && name_->text() != current()->name)
            change("Rename sprite", [&](SpriteAsset &a) { a.name = name_->text().simplified(); });
    });
    connect(frameName_, &QLineEdit::editingFinished, this, [this] {
        if (!refreshing_)
            change("Rename sprite frame", [&](SpriteAsset &a) {
                a.frames[frames_->currentRow()].name = frameName_->text().simplified();
            });
    });
    connect(layerName_, &QLineEdit::editingFinished, this, [this] {
        if (!refreshing_)
            change("Rename sprite layer", [&](SpriteAsset &a) {
                auto &list = a.frames[frames_->currentRow()].layers;
                list[list.size() - 1 - layers_->currentRow()].name =
                    layerName_->text().simplified();
            });
    });
    connect(ticks_, &QSpinBox::valueChanged, this, [this](int n) {
        if (!refreshing_)
            change("Change sprite timing",
                   [&](SpriteAsset &a) { a.frames[frames_->currentRow()].ticks = n; });
    });
    for (auto widget : {anchorX_, anchorY_})
        connect(widget, &QSpinBox::valueChanged, this, [this] {
            if (!refreshing_)
                change("Move sprite anchor",
                       [&](SpriteAsset &a) { a.anchor = {anchorX_->value(), anchorY_->value()}; });
        });
    for (auto widget : {layerX_, layerY_})
        connect(widget, &QSpinBox::valueChanged, this, [this] {
            if (!refreshing_)
                change("Move sprite layer", [&](SpriteAsset &a) {
                    auto &list = a.frames[frames_->currentRow()].layers;
                    list[list.size() - 1 - layers_->currentRow()].position = {layerX_->value(),
                                                                              layerY_->value()};
                });
        });
    connect(visible_, &QCheckBox::toggled, this, [this](bool show) {
        if (!refreshing_)
            change("Change sprite layer visibility", [&](SpriteAsset &a) {
                auto &list = a.frames[frames_->currentRow()].layers;
                list[list.size() - 1 - layers_->currentRow()].visible = show;
            });
    });
    connect(undo_, &QUndoStack::indexChanged, this, [this] {
        refresh();
        changed_();
    });
    updateColor(color_);
    refresh();
}
SpriteEditor::~SpriteEditor() {
    disconnect(undo_, nullptr, this, nullptr);
}
const SpriteAsset *SpriteEditor::current() const {
    for (const auto &asset : project_->sprites().assets())
        if (asset.id == selected_)
            return &asset;
    return nullptr;
}
void SpriteEditor::guarded(const std::function<void()> &action) {
    try {
        action();
        notice_->setText("Assets are saved in this project. Assign artwork to a game resource to "
                         "include it in a ROM.");
    } catch (const std::exception &e) {
        notice_->setText(QString::fromUtf8(e.what()));
        refresh();
    }
}
void SpriteEditor::commit(const QString &label, SpriteLibrary library) {
    undo_->push(new SpriteCommand(project_, label, std::move(library)));
}
void SpriteEditor::change(const QString &label, const std::function<void(SpriteAsset &)> &action) {
    if (refreshing_ || !current())
        return;
    guarded([&] {
        auto asset = *current();
        action(asset);
        if (asset == *current())
            return;
        auto library = project_->sprites();
        library.replace(asset);
        commit(label, std::move(library));
    });
}
void SpriteEditor::addAsset(SpriteAsset asset) {
    guarded([&] {
        auto library = project_->sprites();
        library.replace(asset);
        selected_ = asset.id;
        commit("Add sprite asset", std::move(library));
    });
}
void SpriteEditor::selectAsset(const QString &id) {
    project_->sprites().asset(id);
    selected_ = id;
    refresh();
}
void SpriteEditor::refresh() {
    const QSignalBlocker blocker(play_);
    play_->setChecked(false);
    play_->setText("Play");
    timer_->stop();
    canvas_->setEnabled(true);
    refreshing_ = true;
    const int oldFrame = frames_->currentRow(), oldLayer = layers_->currentRow();
    assets_->clear();
    for (const auto &asset : project_->sprites().assets()) {
        auto item = new QListWidgetItem(asset.name, assets_);
        item->setData(Qt::UserRole, asset.id);
        item->setHidden(!asset.name.contains(search_->text(), Qt::CaseInsensitive));
        if (asset.id == selected_)
            assets_->setCurrentItem(item);
    }
    if (!current() && !project_->sprites().assets().isEmpty()) {
        selected_ = project_->sprites().assets().first().id;
        assets_->setCurrentRow(0);
    }
    const auto asset = current();
    editing_->setEnabled(asset != nullptr);
    frames_->clear();
    layers_->clear();
    if (asset) {
        name_->setText(asset->name);
        info_->setText(QString("%1 × %2").arg(asset->size.width()).arg(asset->size.height()));
        anchorX_->setValue(asset->anchor.x());
        anchorY_->setValue(asset->anchor.y());
        for (int i = 0; i < asset->frames.size(); ++i) {
            const auto &frame = asset->frames[i];
            frames_->addItem(
                QString("%1 · %2\n%3 ticks").arg(i + 1).arg(frame.name).arg(frame.ticks));
        }
        frames_->setCurrentRow(std::clamp(oldFrame, 0, int(asset->frames.size()) - 1));
        const auto &list = asset->frames[frames_->currentRow()].layers;
        for (auto it = list.crbegin(); it != list.crend(); ++it)
            layers_->addItem(it->name);
        layers_->setCurrentRow(std::clamp(oldLayer, 0, int(list.size()) - 1));
    } else {
        selected_.clear();
        name_->clear();
        info_->clear();
    }
    refreshing_ = false;
    refreshFrame();
}
void SpriteEditor::refreshFrame(bool reset) {
    const auto asset = current();
    if (!asset || frames_->currentRow() < 0) {
        canvas_->setAsset(nullptr, 0, 0, true);
        return;
    }
    refreshing_ = true;
    const auto &frame = asset->frames[frames_->currentRow()];
    const int row = std::clamp(layers_->currentRow(), 0, int(frame.layers.size()) - 1);
    layers_->clear();
    for (auto it = frame.layers.crbegin(); it != frame.layers.crend(); ++it)
        layers_->addItem((it->visible ? "" : "○ ") + it->name);
    layers_->setCurrentRow(row);
    const int index = int(frame.layers.size()) - 1 - row;
    const auto &layer = frame.layers[index];
    frameName_->setText(frame.name);
    ticks_->setValue(frame.ticks);
    layerName_->setText(layer.name);
    visible_->setChecked(layer.visible);
    layerX_->setValue(layer.position.x());
    layerY_->setValue(layer.position.y());
    canvas_->setAsset(asset, frames_->currentRow(), index, reset);
    refreshing_ = false;
}
void SpriteEditor::updateColor(QColor color) {
    color_ = color;
    canvas_->setColor(color);
    colorButton_->setText(color.name(QColor::HexArgb));
    colorButton_->setStyleSheet(QString("QPushButton { border: 3px solid %1; }").arg(color.name()));
}
void SpriteEditor::play(bool enabled) {
    canvas_->cancelStroke();
    canvas_->setEnabled(!enabled);
    play_->setText(enabled ? "Pause" : "Play");
    playTicks_ = 0;
    playedTicks_ = 0;
    playbackClock_.start();
    if (enabled && current())
        timer_->start();
    else
        timer_->stop();
}
void SpriteEditor::tick() {
    const auto asset = current();
    if (!asset) {
        play_->setChecked(false);
        return;
    }
    const auto now = playbackClock_.elapsed() * 30 / 1000;
    int total = 0;
    for (const auto &frame : asset->frames)
        total += std::max(1, frame.ticks);
    playTicks_ += int((now - playedTicks_) % total);
    playedTicks_ = now;
    int row = frames_->currentRow();
    while (playTicks_ >= std::max(1, asset->frames[row].ticks)) {
        playTicks_ -= std::max(1, asset->frames[row].ticks);
        row = (row + 1) % int(asset->frames.size());
    }
    frames_->setCurrentRow(row);
}
void SpriteEditor::createAsset() {
    QDialog dialog(this);
    dialog.setWindowTitle("New sprite asset");
    auto form = new QFormLayout(&dialog);
    QLineEdit name("Untitled sprite");
    name.setMaxLength(120);
    form->addRow("Name", &name);
    QComboBox preset;
    preset.addItems({"Custom", "Class avatar · 40 × 48", "Item icon · 16 × 16",
                     "Army sprite · 16 × 24", "Large army sprite · 32 × 28"});
    form->addRow("Canvas", &preset);
    QSpinBox width, height;
    for (auto box : {&width, &height}) {
        box->setRange(1, 4096);
        box->setValue(32);
    }
    form->addRow("Width", &width);
    form->addRow("Height", &height);
    connect(&preset, &QComboBox::currentIndexChanged, &dialog, [&](int index) {
        const QSize sizes[]{{32, 32}, {40, 48}, {16, 16}, {16, 24}, {32, 28}};
        width.setValue(sizes[index].width());
        height.setValue(sizes[index].height());
    });
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted)
        guarded(
            [&] { addAsset(SpriteAsset::blank(name.text(), {width.value(), height.value()})); });
}
void SpriteEditor::resizeAsset() {
    if (!current())
        return;
    QDialog dialog(this);
    dialog.setWindowTitle("Resize sprite");
    auto form = new QFormLayout(&dialog);
    QSpinBox width, height, x, y;
    for (auto box : {&width, &height})
        box->setRange(1, 4096);
    for (auto box : {&x, &y})
        box->setRange(-32768, 32767);
    width.setValue(current()->size.width());
    height.setValue(current()->size.height());
    form->addRow("Width", &width);
    form->addRow("Height", &height);
    QCheckBox scale("Scale all layers (nearest pixel)"), crop("Crop layers to canvas");
    form->addRow(&scale);
    form->addRow(&crop);
    form->addRow("Canvas offset X", &x);
    form->addRow("Canvas offset Y", &y);
    connect(&scale, &QCheckBox::toggled, &dialog, [&](bool value) {
        crop.setEnabled(!value);
        x.setEnabled(!value);
        y.setEnabled(!value);
    });
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted)
        change("Resize sprite canvas", [&](SpriteAsset &a) {
            a = a.resized({width.value(), height.value()}, scale.isChecked(),
                          {x.value(), y.value()}, crop.isChecked());
        });
}
void SpriteEditor::copyPixels(bool cut) {
    if (!current())
        return;
    const auto &list = current()->frames[frames_->currentRow()].layers;
    const auto &layer = list[list.size() - 1 - layers_->currentRow()];
    const auto rect =
        canvas_->selection().isEmpty()
            ? layer.image.rect()
            : canvas_->selection().translated(-layer.position).intersected(layer.image.rect());
    if (rect.isEmpty())
        return;
    QApplication::clipboard()->setImage(layer.image.copy(rect));
    if (cut)
        change("Cut sprite pixels", [&](SpriteAsset &a) {
            auto &list = a.frames[frames_->currentRow()].layers;
            auto &image = list[list.size() - 1 - layers_->currentRow()].image;
            QPainter p(&image);
            p.setCompositionMode(QPainter::CompositionMode_Source);
            p.fillRect(rect, Qt::transparent);
        });
}
void SpriteEditor::pastePixels() {
    const auto image = QApplication::clipboard()->image();
    if (image.isNull())
        return;
    change("Paste sprite layer", [&](SpriteAsset &a) {
        auto layer = SpriteAsset::fromImage("Clipboard", image).frames.first().layers.first();
        layer.name = "Pasted layer";
        layer.position = canvas_->selection().topLeft();
        a.frames[frames_->currentRow()].layers.append(layer);
    });
    if (current())
        layers_->setCurrentRow(0);
}
void SpriteEditor::importFile(const QString &path) {
    const auto extension = QFileInfo(path).suffix().toLower();
    SpriteAsset asset;
    if (extension == "json" || extension == "lcsprite") {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly) || file.size() > 512 * 1024 * 1024)
            throw Error("Cannot read sprite asset or file exceeds 512 MiB");
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject())
            throw Error("Invalid sprite asset JSON");
        asset = SpriteAsset::fromAssetFile(doc.object());
    } else {
        QImageReader reader(path);
        const auto size = reader.size();
        if (size.width() < 1 || size.height() < 1 || size.width() > 4096 || size.height() > 4096)
            throw Error("Image dimensions must be 1–4096 pixels");
        const auto image = reader.read();
        if (image.isNull())
            throw Error("Cannot decode image: " + reader.errorString());
        asset = SpriteAsset::fromImage(QFileInfo(path).completeBaseName().left(120), image);
    }
    // Imports are new library assets, never an implicit overwrite of another asset's identity.
    asset.id = SpriteLibrary::newId();
    auto library = project_->sprites();
    library.replace(asset);
    selected_ = asset.id;
    commit("Import sprite asset", std::move(library));
}
void SpriteEditor::importLayerImage(const QString &path) {
    if (!current() || frames_->currentRow() < 0 || layers_->currentRow() < 0)
        throw Error("Select a sprite layer before importing an image");
    play_->setChecked(false);
    canvas_->cancelStroke();
    const auto before = *current();
    const int frame = frames_->currentRow();
    const int layer = int(before.frames[frame].layers.size()) - 1 - layers_->currentRow();
    const auto size = before.frames[frame].layers[layer].image.size();
    const auto prepared = prepareArtworkImage(
        this, loadArtworkImage(path),
        {size, "spriteLayer", "Prepare layer image",
         "Replace the selected layer's pixels. Its size and position stay the same; fit, crop or "
         "place the image below. Color and partial transparency are retained.", false, false},
        [](const QImage &image, int) {
            const auto rgba = image.convertToFormat(QImage::Format_RGBA8888);
            return ArtImageMapping{QByteArray(reinterpret_cast<const char *>(rgba.constBits()),
                                               rgba.sizeInBytes()), rgba};
        });
    if (!prepared) return;
    if (!current() || *current() != before)
        throw Error("The sprite changed while image preparation was open; import again");
    const auto image = QImage(reinterpret_cast<const uchar *>(prepared->constData()),
                               size.width(), size.height(), size.width() * 4,
                               QImage::Format_RGBA8888).copy();
    change("Import sprite layer image", [&](SpriteAsset &asset) {
        asset.frames[frame].layers[layer].image = image;
    });
}
void SpriteEditor::exportAsset(const QString &path, bool browser) const {
    if (!current())
        throw Error("Select a sprite asset first");
    if (QFileInfo(path).canonicalFilePath() ==
            QFileInfo(project_->rom().sourcePath()).canonicalFilePath() &&
        QFileInfo(path).exists())
        throw Error("Cannot overwrite the source ROM");
    atomicWrite(path, QJsonDocument(current()->assetFile(browser)).toJson());
}
void SpriteEditor::exportImage(const QString &path, const QByteArray &format) const {
    if (!current())
        throw Error("Select a sprite asset first");
    if (QFileInfo(path).canonicalFilePath() ==
            QFileInfo(project_->rom().sourcePath()).canonicalFilePath() &&
        QFileInfo(path).exists())
        throw Error("Cannot overwrite the source ROM");
    auto image = current()->composite(frames_->currentRow());
    if (format == "JPEG") {
        QImage opaque(image.size(), QImage::Format_RGB888);
        opaque.fill(Qt::white);
        QPainter p(&opaque);
        p.drawImage(0, 0, image);
        p.end();
        image = std::move(opaque);
    }
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, format.constData(), 100))
        throw Error("Cannot encode sprite image");
    atomicWrite(path, bytes);
}
} // namespace lordly
