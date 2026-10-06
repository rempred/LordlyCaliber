#include "app/art_editor.h"
#include "app/art_image_assignment.h"
#include "app/sprite_editor.h"
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QUndoStack>
#include <QVBoxLayout>
#include <algorithm>
namespace lordly {
namespace {
class IconCommand final : public QUndoCommand {
  public:
    IconCommand(Project *project, const QString &label, QJsonObject after)
        : QUndoCommand(label), project_(project), before_(project->iconArtwork()->json()),
          after_(std::move(after)) {}
    void undo() override {
        project_->setIconArtwork(before_);
    }
    void redo() override {
        project_->setIconArtwork(after_);
    }

  private:
    Project *project_;
    QJsonObject before_, after_;
};
class LibraryCopyCommand final : public QUndoCommand {
  public:
    LibraryCopyCommand(Project *project, SpriteLibrary after)
        : QUndoCommand("Copy game artwork to sprite library"), project_(project),
          before_(project->sprites()), after_(std::move(after)) {}
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
QColor wordColor(quint16 word) {
    return {((word >> 11) * 255 + 15) / 31, (((word >> 6) & 31) * 255 + 15) / 31,
            (((word >> 1) & 31) * 255 + 15) / 31, (word & 1) ? 255 : 0};
}

QString iconName(const Project &project, quint16 item) {
    const auto &rows = project.records(item & 0x200 ? Table::Consumables : Table::Items);
    const int id = item & 0x1ff;
    return id < rows.size() ? rows[id].name : QString("Item %1").arg(id);
}
} // namespace
ArtEditor::ArtEditor(Project *project, QUndoStack *undo, std::function<void()> changed,
                     QWidget *parent)
    : QWidget(parent), project_(project), undo_(undo), changed_(std::move(changed)) {
    setObjectName("artEditor");
    auto outer = new QVBoxLayout(this);
    outer->addWidget(
        new QLabel("<b>Item artwork</b> · Edit icons used by menus, shops and cutscene rewards"));
    notice_ = new QLabel;
    notice_->setObjectName("artNotice");
    notice_->setWordWrap(true);
    try {
        project_->loadIconArtwork();
    } catch (const std::exception &e) {
        notice_->setText(QString::fromUtf8(e.what()));
        outer->addWidget(notice_);
        return;
    }
    auto actions = new QHBoxLayout;
    outer->addLayout(actions);
    auto action = [&](const QString &text, const char *id, const std::function<void()> &callback) {
        auto button = new QPushButton(text);
        button->setObjectName(id);
        actions->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, callback] { guarded(callback); });
        return button;
    };
    action("Import image…", "artImport", [this] {
        const auto path = QFileDialog::getOpenFileName(this, "Prepare item icon", {},
                                                       "Images (*.png *.jpg *.jpeg)");
        if (!path.isEmpty())
            prepareImage(loadArtworkImage(path));
    });
    action("Export PNG…", "artExport", [this] {
        const auto path = QFileDialog::getSaveFileName(
            this, "Export item icon", iconName(*project_, selected_) + ".png", "PNG (*.png)");
        if (!path.isEmpty())
            exportImage(path);
    });
    action("Copy to sprite library", "artCopyToLibrary", [this] { copyToLibrary(); });
    action("Restore loaded icon", "artRestore", [this] {
        auto candidate = *project_->iconArtwork();
        candidate.restore(selected_);
        if (candidate.json() != project_->iconArtwork()->json())
            undo_->push(new IconCommand(project_, "Restore item icon", candidate.json()));
    });
    actions->addStretch();
    auto split = new QSplitter;
    outer->addWidget(split, 1);
    auto browser = new QWidget;
    auto browse = new QVBoxLayout(browser);
    browse->setContentsMargins(0, 0, 0, 0);
    pack_ = new QComboBox;
    pack_->setObjectName("artIconPack");
    for (const auto &spec : IconArtwork::specs())
        pack_->addItem(spec.label);
    browse->addWidget(pack_);
    search_ = new QLineEdit;
    search_->setPlaceholderText("Search icon names or IDs…");
    browse->addWidget(search_);
    icons_ = new QListWidget;
    icons_->setObjectName("artIcons");
    icons_->setIconSize({32, 32});
    browse->addWidget(icons_);
    split->addWidget(browser);
    auto workspace = new QWidget;
    auto work = new QVBoxLayout(workspace);
    work->setContentsMargins(0, 0, 0, 0);
    split->addWidget(workspace);
    title_ = new QLabel;
    work->addWidget(title_);
    auto controls = new QHBoxLayout;
    work->addLayout(controls);
    auto tool = new QComboBox;
    tool->setObjectName("artPixelTool");
    tool->addItems({"Pencil", "Eraser", "Fill", "Eyedropper", "Selection", "Move"});
    controls->addWidget(tool);
    controls->addWidget(new QLabel("Brush"));
    auto brush = new QSpinBox;
    brush->setRange(1, 16);
    controls->addWidget(brush);
    controls->addWidget(new QLabel("Zoom"));
    auto zoom = new QSpinBox;
    zoom->setRange(1, 32);
    zoom->setValue(20);
    zoom->setSuffix("×");
    controls->addWidget(zoom);
    auto grid = new QCheckBox("Grid");
    grid->setChecked(true);
    controls->addWidget(grid);
    auto deselect = new QPushButton("Deselect");
    controls->addWidget(deselect);
    controls->addStretch();
    auto views = new QHBoxLayout;
    work->addLayout(views, 1);
    auto scroll = new QScrollArea;
    scroll->setAlignment(Qt::AlignCenter);
    scroll->setWidgetResizable(false);
    canvas_ = new SpriteCanvas;
    canvas_->setObjectName("artIconCanvas");
    canvas_->setZoom(20);
    scroll->setWidget(canvas_);
    views->addWidget(scroll, 1);
    auto palettePanel = new QWidget;
    auto colors = new QVBoxLayout(palettePanel);
    colors->setContentsMargins(0, 0, 0, 0);
    views->addWidget(palettePanel);
    colors->addWidget(new QLabel("Shared pack palette"));
    palette_ = new QTableWidget(16, 16);
    palette_->setObjectName("artIconPalette");
    palette_->horizontalHeader()->hide();
    palette_->verticalHeader()->hide();
    palette_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    palette_->setSelectionMode(QAbstractItemView::SingleSelection);
    palette_->setFocusPolicy(Qt::NoFocus);
    palette_->horizontalHeader()->setMinimumSectionSize(19);
    palette_->horizontalHeader()->setDefaultSectionSize(19);
    palette_->verticalHeader()->setMinimumSectionSize(19);
    palette_->verticalHeader()->setDefaultSectionSize(19);
    palette_->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    palette_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    palette_->setFixedSize(16 * 19 + 2, 16 * 19 + 2);
    colors->addWidget(palette_);
    auto paletteNotice =
        new QLabel("255 opaque colors + transparency.\nPainting keeps the other icons intact.");
    paletteNotice->setWordWrap(true);
    colors->addWidget(paletteNotice);
    colors->addStretch();
    auto libraryRow = new QHBoxLayout;
    work->addLayout(libraryRow);
    libraryRow->addWidget(new QLabel("Sprite library"));
    library_ = new QComboBox;
    library_->setObjectName("artLibraryAsset");
    library_->setMinimumContentsLength(20);
    library_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    libraryRow->addWidget(library_, 1);
    frame_ = new QSpinBox;
    frame_->setObjectName("artLibraryFrame");
    frame_->setPrefix("Frame ");
    frame_->setMinimum(1);
    libraryRow->addWidget(frame_);
    auto assign = new QPushButton("Assign to icon…");
    assign->setObjectName("artAssignLibrary");
    libraryRow->addWidget(assign);
    connect(assign, &QPushButton::clicked, this, [this] {
        guarded([&] {
            if (library_->currentIndex() < 0)
                throw Error("Create or import a sprite asset first");
            const auto &asset = project_->sprites().asset(library_->currentData().toString());
            prepareImage(asset.composite(frame_->value() - 1));
        });
    });
    connect(library_, &QComboBox::currentIndexChanged, this, [this] {
        if (library_->currentIndex() >= 0)
            frame_->setMaximum(
                int(project_->sprites().asset(library_->currentData().toString()).frames.size()));
        else
            frame_->setMaximum(1);
    });
    connect(tool, &QComboBox::currentIndexChanged, this,
            [this](int index) { canvas_->setTool(SpriteCanvas::Tool(index)); });
    connect(brush, &QSpinBox::valueChanged, canvas_, &SpriteCanvas::setBrush);
    connect(zoom, &QSpinBox::valueChanged, canvas_, &SpriteCanvas::setZoom);
    connect(grid, &QCheckBox::toggled, canvas_, &SpriteCanvas::setGrid);
    connect(deselect, &QPushButton::clicked, canvas_, &SpriteCanvas::clearSelection);
    connect(palette_, &QTableWidget::itemClicked, this, [this](QTableWidgetItem *item) {
        canvas_->setColor(item->data(Qt::UserRole).value<QColor>());
    });
    connect(canvas_, &SpriteCanvas::colorPicked, canvas_, &SpriteCanvas::setColor);
    connect(canvas_, &SpriteCanvas::committed, this, [this](const QImage &image, QPoint position) {
        guarded([&] {
            auto asset = SpriteAsset::fromImage("Icon", image);
            asset.frames[0].layers[0].position = position;
            assignImage(position.isNull() ? image : asset.composite(0));
        });
    });
    connect(pack_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (refreshing_)
            return;
        selected_ = quint16((index ? 0x200 : 0) | 1);
        refresh();
    });
    connect(icons_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (refreshing_ || row < 0)
            return;
        selected_ = quint16(icons_->item(row)->data(Qt::UserRole).toUInt());
        refreshIcon();
    });
    connect(search_, &QLineEdit::textChanged, this, [this] {
        for (int i = 0; i < icons_->count(); ++i)
            icons_->item(i)->setHidden(
                !icons_->item(i)->text().contains(search_->text(), Qt::CaseInsensitive));
    });
    connect(undo_, &QUndoStack::indexChanged, this, [this] {
        refresh();
        changed_();
    });
    split->setSizes({260, 950});
    split->setStretchFactor(1, 1);
    outer->addWidget(notice_);
    refresh();
}
ArtEditor::~ArtEditor() {
    disconnect(undo_, nullptr, this, nullptr);
}
void ArtEditor::guarded(const std::function<void()> &action) {
    try {
        action();
        notice_->setText("Edits rebuild the shared icon packs during ROM export. Undo restores "
                         "earlier artwork.");
    } catch (const std::exception &e) {
        refresh();
        notice_->setText(QString::fromUtf8(e.what()));
    }
}
void ArtEditor::selectIcon(quint16 item) {
    IconArtwork::packIndex(item);
    if (!project_->iconArtwork())
        throw Error("Icon artwork is unavailable");
    selected_ = item;
    refresh();
}
void ArtEditor::refresh() {
    if (!project_->iconArtwork())
        return;
    refreshing_ = true;
    const int index = IconArtwork::packIndex(selected_);
    pack_->setCurrentIndex(index);
    icons_->clear();
    for (int id = 1; id <= IconArtwork::specs()[index].count; ++id) {
        const auto itemId = quint16((index ? 0x200 : 0) | id);
        auto item =
            new QListWidgetItem(QString("%1 · %2%3")
                                    .arg(id)
                                    .arg(iconName(*project_, itemId))
                                    .arg(project_->iconArtwork()->modified(itemId) ? " *" : ""),
                                icons_);
        item->setData(Qt::UserRole, itemId);
        item->setIcon(QPixmap::fromImage(project_->iconArtwork()->image(itemId).scaled(
            32, 32, Qt::IgnoreAspectRatio, Qt::FastTransformation)));
        item->setHidden(!item->text().contains(search_->text(), Qt::CaseInsensitive));
        if (itemId == selected_)
            icons_->setCurrentItem(item);
    }
    const auto old = library_->currentData().toString();
    {
        const QSignalBlocker blocker(library_);
        library_->clear();
        for (const auto &asset : project_->sprites().assets())
            library_->addItem(asset.name, asset.id);
        const int at = library_->findData(old);
        if (at >= 0)
            library_->setCurrentIndex(at);
    }
    frame_->setMaximum(
        library_->currentIndex() < 0
            ? 1
            : int(project_->sprites().asset(library_->currentData().toString()).frames.size()));
    refreshing_ = false;
    refreshIcon();
}
void ArtEditor::refreshIcon() {
    auto asset = SpriteAsset::fromImage(iconName(*project_, selected_),
                                        project_->iconArtwork()->image(selected_));
    asset.id = "icon-" + QString::number(selected_);
    canvas_->setAsset(&asset, 0, 0);
    title_->setText(QString("%1 · ID %2 · 16 × 16 pixels")
                        .arg(iconName(*project_, selected_))
                        .arg(selected_ & 0x1ff));
    const auto palette = project_->iconArtwork()->palette(IconArtwork::packIndex(selected_));
    for (int i = 0; i < 256; ++i) {
        auto item = palette_->item(i / 16, i % 16);
        if (!item) {
            item = new QTableWidgetItem;
            palette_->setItem(i / 16, i % 16, item);
        }
        const auto color = i < palette.size() ? wordColor(palette[i]) : QColor(Qt::transparent);
        item->setBackground(color.alpha() ? color : QColor(230, 230, 230));
        item->setText(color.alpha() ? "" : "×");
        item->setTextAlignment(Qt::AlignCenter);
        item->setData(Qt::UserRole, color);
        item->setToolTip(
            color.alpha()
                ? QString("RGB %1, %2, %3").arg(color.red()).arg(color.green()).arg(color.blue())
                : "Transparent");
    }
}
void ArtEditor::apply(const QByteArray &words, const QString &label) {
    auto candidate = *project_->iconArtwork();
    if (words == candidate.words(selected_))
        return;
    candidate.setWords(selected_, words);
    undo_->push(new IconCommand(project_, label, candidate.json()));
}
void ArtEditor::assignImage(const QImage &image, bool matchPalette, int threshold) {
    if (!project_->iconArtwork())
        throw Error("Icon artwork is unavailable");
    apply(project_->iconArtwork()->mapImage(selected_, image, matchPalette, threshold),
          "Edit item icon");
}
QString ArtEditor::copyToLibrary() {
    if (!project_->iconArtwork())
        throw Error("Icon artwork is unavailable");
    auto asset = SpriteAsset::fromImage(iconName(*project_, selected_) + " icon",
                                        project_->iconArtwork()->image(selected_));
    asset.provenance = {{"gameSource", "item-icon"}, {"itemId", selected_}};
    auto library = project_->sprites();
    library.replace(asset);
    const auto id = asset.id;
    undo_->push(new LibraryCopyCommand(project_, std::move(library)));
    return id;
}
void ArtEditor::exportImage(const QString &path) const {
    if (!project_->iconArtwork())
        throw Error("Icon artwork is unavailable");
    if (QFileInfo(path).exists() && QFileInfo(path).canonicalFilePath() ==
                                        QFileInfo(project_->rom().sourcePath()).canonicalFilePath())
        throw Error("Cannot overwrite the source ROM");
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!project_->iconArtwork()->image(selected_).save(&buffer, "PNG"))
        throw Error("Cannot encode item icon");
    atomicWrite(path, bytes);
}
void ArtEditor::importImage(const QString &path) {
    assignImage(loadArtworkImage(path));
}
void ArtEditor::prepareImage(const QImage &image) {
    const auto item = selected_;
    const auto before = project_->iconArtwork()->words(item);
    const auto prepared = prepareArtworkImage(
        this, image,
        {{16, 16},
         "art",
         "Assign image to item icon",
         "The preview uses this pack's shared palette and one-bit transparency.",
         false,
         true,
         true},
        [&](const QImage &target, int alpha, bool dither) {
            const auto words = project_->iconArtwork()->mapImage(item, target, true, alpha, dither);
            auto next = *project_->iconArtwork();
            next.setWords(item, words);
            return ArtImageMapping{words, next.image(item)};
        });
    if (prepared) {
        if (selected_ != item || project_->iconArtwork()->words(item) != before)
            throw Error("This icon changed while the assignment preview was open");
        apply(*prepared, "Assign image to item icon");
    }
}

} // namespace lordly
