#include "app/combat_editor.h"
#include "app/art_image_assignment.h"
#include "app/sprite_editor.h"
#include <QBuffer>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QCompleter>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
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
class CombatCommand final : public QUndoCommand {
  public:
    CombatCommand(Project *p, QString label, QJsonObject after)
        : QUndoCommand(label), project_(p), before_(p->combatArtwork()->json()),
          after_(std::move(after)) {}
    void undo() override {
        project_->setCombatArtwork(before_);
    }
    void redo() override {
        project_->setCombatArtwork(after_);
    }

  private:
    Project *project_;
    QJsonObject before_, after_;
};
class LibraryCommand final : public QUndoCommand {
  public:
    LibraryCommand(Project *p, SpriteLibrary after)
        : QUndoCommand("Copy combat sprite to library"), project_(p), before_(p->sprites()),
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
QString role(int policy) {
    return QStringList{"Body", "Equipment", "Element effect"}.value(policy, "Unknown");
}
QColor color(quint16 word) {
    return {(255 * (word >> 11) + 15) / 31, (255 * ((word >> 6) & 31) + 15) / 31,
            (255 * ((word >> 1) & 31) + 15) / 31, 255};
}
} // namespace
CombatEditor::CombatEditor(Project *project, QUndoStack *undo, std::function<void()> changed,
                           QWidget *parent)
    : QWidget(parent), project_(project), undo_(undo), changed_(std::move(changed)) {
    setObjectName("combatEditor");
    auto outer = new QVBoxLayout(this);
    outer->addWidget(new QLabel("<b>Combat and actor artwork</b> · Edit color and opacity without "
                                "changing animation timing"));
    notice_ = new QLabel;
    notice_->setObjectName("combatNotice");
    notice_->setWordWrap(true);
    try {
        project_->loadCombatArtwork();
    } catch (const std::exception &e) {
        notice_->setText(QString::fromUtf8(e.what()));
        outer->addWidget(notice_);
        return;
    }
    auto actions = new QHBoxLayout;
    outer->addLayout(actions);
    auto action = [&](QString text, const char *id, std::function<void()> callback) {
        auto button = new QPushButton(text);
        button->setObjectName(id);
        actions->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, callback] { guarded(callback); });
        return button;
    };
    action("Import image…", "combatImport", [this] {
        const auto path = QFileDialog::getOpenFileName(this, "Prepare combat sprite", {},
                                                       "Images (*.png *.jpg *.jpeg)");
        if (!path.isEmpty())
            prepareImage(loadArtworkImage(path));
    });
    action("Export PNG…", "combatExport", [this] {
        const auto path = QFileDialog::getSaveFileName(this, "Export combat sprite",
                                                       "combat-sprite.png", "PNG (*.png)");
        if (!path.isEmpty())
            exportImage(path);
    });
    action("Copy to sprite library", "combatCopyToLibrary", [this] { copyToLibrary(); });
    action("Restore loaded child", "combatRestore", [this] {
        auto next = *project_->combatArtwork();
        next.restore(selected_, child_);
        if (next.json() != project_->combatArtwork()->json())
            undo_->push(new CombatCommand(project_, "Restore combat sprite child", next.json()));
    });
    actions->addStretch();
    auto split = new QSplitter;
    outer->addWidget(split, 1);
    auto browser = new QWidget;
    auto browse = new QVBoxLayout(browser);
    browse->setContentsMargins(0, 0, 0, 0);
    descriptors_ = new QComboBox;
    descriptors_->setObjectName("combatDescriptor");
    descriptors_->setEditable(true);
    descriptors_->setInsertPolicy(QComboBox::NoInsert);
    descriptors_->completer()->setFilterMode(Qt::MatchContains);
    descriptors_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    descriptors_->setMinimumContentsLength(24);
    browse->addWidget(descriptors_);
    search_ = new QLineEdit;
    search_->setObjectName("combatSearch");
    search_->setPlaceholderText("Find art number, role or resource");
    browse->addWidget(search_);
    members_ = new QListWidget;
    members_->setObjectName("combatMembers");
    browse->addWidget(members_);
    split->addWidget(browser);
    auto editing = new QWidget;
    auto edit = new QVBoxLayout(editing);
    edit->setContentsMargins(0, 0, 0, 0);
    info_ = new QLabel;
    info_->setObjectName("combatInfo");
    info_->setWordWrap(true);
    edit->addWidget(info_);
    auto tools = new QHBoxLayout;
    edit->addLayout(tools);
    childSelect_ = new QSpinBox;
    childSelect_->setObjectName("combatChild");
    childSelect_->setPrefix("Child ");
    tools->addWidget(childSelect_);
    channel_ = new QComboBox;
    channel_->setObjectName("combatChannel");
    channel_->addItems({"Color indices", "Opacity", "Combined image"});
    tools->addWidget(channel_);
    auto painting = new QHBoxLayout;
    edit->addLayout(painting);
    auto tool = new QComboBox;
    tool->setObjectName("combatTool");
    tool->addItems({"Pencil", "Eraser", "Fill", "Pick", "Select", "Move selection"});
    painting->addWidget(tool);
    ink_ = new QSpinBox;
    ink_->setObjectName("combatInk");
    ink_->setRange(0, 255);
    ink_->setValue(1);
    painting->addWidget(ink_);
    auto colorButton = new QPushButton("Paint color…");
    tools->addWidget(colorButton);
    connect(colorButton, &QPushButton::clicked, this, [this] {
        auto color = QColorDialog::getColor(Qt::white, this, "Combat sprite color",
                                            QColorDialog::ShowAlphaChannel);
        if (color.isValid()) {
            channel_->setCurrentIndex(2);
            canvas_->setColor(color);
        }
    });
    auto brush = new QSpinBox;
    brush->setRange(1, 16);
    brush->setPrefix("Brush ");
    painting->addWidget(brush);
    auto zoom = new QSpinBox;
    zoom->setRange(1, 24);
    zoom->setValue(8);
    zoom->setPrefix("Zoom ");
    painting->addWidget(zoom);
    auto grid = new QCheckBox("Grid");
    grid->setChecked(true);
    painting->addWidget(grid);
    auto deselect = new QPushButton("Deselect");
    painting->addWidget(deselect);
    tools->addStretch();
    auto row = new QHBoxLayout;
    edit->addLayout(row, 1);
    canvas_ = new SpriteCanvas;
    canvas_->setObjectName("combatCanvas");
    canvas_->setZoom(8);
    auto scroll = new QScrollArea;
    scroll->setWidget(canvas_);
    scroll->setAlignment(Qt::AlignCenter);
    row->addWidget(scroll, 1);
    auto sidebar = new QVBoxLayout;
    row->addLayout(sidebar);
    sidebar->addWidget(new QLabel("Combined preview"));
    preview_ = new QLabel;
    preview_->setObjectName("combatPreview");
    preview_->setAlignment(Qt::AlignCenter);
    preview_->setMinimumSize(200, 160);
    sidebar->addWidget(preview_);
    palette_ = new QTableWidget(16, 16);
    palette_->setObjectName("combatPalette");
    palette_->horizontalHeader()->hide();
    palette_->verticalHeader()->hide();
    palette_->horizontalHeader()->setDefaultSectionSize(14);
    palette_->verticalHeader()->setDefaultSectionSize(14);
    palette_->horizontalHeader()->setMinimumSectionSize(14);
    palette_->verticalHeader()->setMinimumSectionSize(14);
    palette_->setFixedSize(240, 240);
    palette_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    sidebar->addWidget(palette_);
    sidebar->addStretch();
    auto libraryRow = new QHBoxLayout;
    edit->addLayout(libraryRow);
    library_ = new QComboBox;
    library_->setObjectName("combatLibrary");
    libraryRow->addWidget(library_, 1);
    libraryFrame_ = new QSpinBox;
    libraryFrame_->setObjectName("combatLibraryFrame");
    libraryFrame_->setRange(1, 1);
    libraryFrame_->setPrefix("Frame ");
    libraryRow->addWidget(libraryFrame_);
    auto assign = new QPushButton("Assign library image…");
    assign->setObjectName("combatAssignLibrary");
    libraryRow->addWidget(assign);
    split->addWidget(editing);
    split->setStretchFactor(1, 1);
    split->setSizes({260, 1040});
    outer->addWidget(notice_);
    QMap<quint32, QVector<int>> descriptors;
    for (const auto &binding : project_->combatArtwork()->bindings())
        descriptors[binding.descriptorKey] = binding.handles;
    QFile vocabulary(":/game/art/combat-import.json");
    QJsonObject referenceClasses;
    if (vocabulary.open(QIODevice::ReadOnly))
        referenceClasses =
            QJsonDocument::fromJson(vocabulary.readAll()).object()["descriptors"].toObject();
    for (auto it = descriptors.cbegin(); it != descriptors.cend(); ++it) {
        QStringList handles, names;
        for (int h : it.value())
            handles << QString::number(h);
        for (const auto &value : referenceClasses[QString::number(it.key(), 16)].toArray()) {
            const int id = value.toInt(-1);
            if (id >= 0 && id < project_->records(Table::Classes).size())
                names << project_->record(Table::Classes, id).name;
        }
        descriptors_->addItem(
            QString("%1 · Handle %2")
                .arg(names.isEmpty()
                         ? "Other actors and effects"
                         : names.first() + (names.size() > 1 ? QString(" +%1").arg(names.size() - 1)
                                                             : QString()))
                .arg(handles.join('/')),
            it.key());
        descriptors_->setItemData(descriptors_->count() - 1,
                                  QString("Reference classes: %1\nDescriptor %2")
                                      .arg(names.join(", "))
                                      .arg(it.key(), 8, 16, QChar('0')),
                                  Qt::ToolTipRole);
    }
    connect(descriptors_, &QComboBox::currentIndexChanged, this, [this] {
        if (!refreshing_)
            guarded([this] {
                selected_.clear();
                child_ = 0;
                refreshMembers();
                refreshPixels();
            });
    });
    connect(search_, &QLineEdit::textChanged, this, [this] {
        for (int i = 0; i < members_->count(); ++i)
            members_->item(i)->setHidden(
                !members_->item(i)->text().contains(search_->text(), Qt::CaseInsensitive));
    });
    connect(members_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) {
        if (!refreshing_ && item)
            guarded([this, item] {
                selected_ = item->data(Qt::UserRole).toString();
                child_ = 0;
                refreshPixels();
            });
    });
    connect(childSelect_, &QSpinBox::valueChanged, this, [this](int value) {
        if (!refreshing_)
            guarded([this, value] {
                child_ = value;
                refreshPixels();
            });
    });
    connect(channel_, &QComboBox::currentIndexChanged, this, [this] {
        if (!refreshing_)
            guarded([this] { refreshPixels(); });
    });
    connect(tool, &QComboBox::currentIndexChanged, canvas_,
            [this](int value) { canvas_->setTool(SpriteCanvas::Tool(value)); });
    connect(ink_, &QSpinBox::valueChanged, canvas_,
            [this](int value) { canvas_->setIndexColor(quint8(value)); });
    connect(canvas_, &SpriteCanvas::indexPicked, ink_, &QSpinBox::setValue);
    connect(canvas_, &SpriteCanvas::colorPicked, canvas_, &SpriteCanvas::setColor);
    connect(brush, &QSpinBox::valueChanged, canvas_, &SpriteCanvas::setBrush);
    connect(zoom, &QSpinBox::valueChanged, canvas_, &SpriteCanvas::setZoom);
    connect(grid, &QCheckBox::toggled, canvas_, &SpriteCanvas::setGrid);
    connect(deselect, &QPushButton::clicked, canvas_, &SpriteCanvas::clearSelection);
    connect(palette_, &QTableWidget::cellClicked, this, [this](int row, int column) {
        if (channel_->currentIndex() != 2)
            ink_->setValue(row * 16 + column);
    });
    connect(
        canvas_, &SpriteCanvas::indicesCommitted, this,
        [this](const QByteArray &indices, QPoint offset) {
            guarded([this, indices, offset] {
                auto next = project_->combatArtwork()->pixels(selected_, child_);
                const auto sprite = project_->combatArtwork()->original(selected_);
                auto data = indices;
                if (!offset.isNull()) {
                    data.fill(0);
                    for (int y = 0; y < sprite.height(); ++y)
                        for (int x = 0; x < sprite.width(); ++x) {
                            const int sx = x - offset.x(), sy = y - offset.y();
                            if (sx >= 0 && sx < sprite.width() && sy >= 0 && sy < sprite.height())
                                data[y * sprite.width() + x] = indices[sy * sprite.width() + sx];
                        }
                }
                if (channel_->currentIndex() == 1)
                    next.alpha = data;
                else
                    next.color = data;
                apply(next, "Paint combat sprite channel");
            });
        });
    connect(canvas_, &SpriteCanvas::committed, this, [this](const QImage &image, QPoint offset) {
        if (channel_->currentIndex() != 2)
            return;
        guarded([this, image, offset] {
            auto shifted = image;
            if (!offset.isNull()) {
                shifted.fill(Qt::transparent);
                QPainter p(&shifted);
                p.drawImage(offset, image);
            }
            assignImage(shifted);
        });
    });
    connect(library_, &QComboBox::currentIndexChanged, this, [this] {
        if (!refreshing_)
            libraryFrame_->setMaximum(library_->currentIndex() < 0
                                          ? 1
                                          : int(project_->sprites()
                                                    .asset(library_->currentData().toString())
                                                    .frames.size()));
    });
    connect(assign, &QPushButton::clicked, this, [this] {
        guarded([this] {
            if (library_->currentIndex() < 0)
                throw Error("Choose a sprite library image");
            prepareImage(project_->sprites()
                             .asset(library_->currentData().toString())
                             .composite(libraryFrame_->value() - 1));
        });
    });
    connect(undo_, &QUndoStack::indexChanged, this, [this] {
        guarded([this] { refresh(); });
        changed_();
    });
    refresh();
}
CombatEditor::~CombatEditor() {
    disconnect(undo_, nullptr, this, nullptr);
}
void CombatEditor::guarded(const std::function<void()> &action) {
    try {
        action();
    } catch (const std::exception &e) {
        canvas_->cancelStroke();
        notice_->setText(QString::fromUtf8(e.what()));
    }
}
void CombatEditor::selectBinding(const QString &id, int child) {
    project_->combatArtwork()->original(id).materialize(child);
    selected_ = id;
    child_ = child;
    refresh();
}
void CombatEditor::refresh() {
    if (!project_->combatArtwork())
        return;
    refreshing_ = true;
    if (!selected_.isEmpty())
        descriptors_->setCurrentIndex(
            descriptors_->findData(project_->combatArtwork()->binding(selected_).descriptorKey));
    const auto selectedLibrary = library_->currentData().toString();
    library_->clear();
    for (const auto &asset : project_->sprites().assets())
        library_->addItem(asset.name, asset.id);
    const int index = library_->findData(selectedLibrary);
    if (index >= 0)
        library_->setCurrentIndex(index);
    libraryFrame_->setMaximum(
        library_->currentIndex() < 0
            ? 1
            : int(project_->sprites().asset(library_->currentData().toString()).frames.size()));
    refreshing_ = false;
    refreshMembers();
    refreshPixels();
}
void CombatEditor::refreshMembers() {
    QSignalBlocker block(members_);
    members_->clear();
    const auto descriptor = descriptors_->currentData().toUInt();
    QMap<int, CombatArtBinding> ordered;
    for (const auto &binding : project_->combatArtwork()->bindings())
        if (binding.descriptorKey == descriptor)
            ordered[binding.art] = binding;
    for (const auto &binding : ordered) {
        auto item = new QListWidgetItem(QString("Art %1 · %2 · %3")
                                            .arg(binding.art)
                                            .arg(role(binding.policy))
                                            .arg(binding.resourceKey, 8, 16, QChar('0')),
                                        members_);
        item->setData(Qt::UserRole, binding.id);
        item->setHidden(!item->text().contains(search_->text(), Qt::CaseInsensitive));
        if (binding.id == selected_)
            members_->setCurrentItem(item);
    }
    if (!members_->currentItem() && members_->count()) {
        members_->setCurrentRow(0);
        selected_ = members_->item(0)->data(Qt::UserRole).toString();
    }
}
void CombatEditor::refreshPixels() {
    canvas_->setEnabled(false);
    preview_->clear();
    if (selected_.isEmpty()) {
        canvas_->setAsset(nullptr, 0, 0);
        return;
    }
    const auto &model = *project_->combatArtwork();
    const auto &binding = model.binding(selected_);
    info_->setText(QString("Art %1 · %2").arg(binding.art).arg(role(binding.policy)));
    try {
        const auto sprite = model.original(selected_);
        {
            QSignalBlocker blocker(childSelect_);
            childSelect_->setRange(0, int(sprite.children().size() - 1));
            childSelect_->setValue(child_);
        }
        info_->setText(QString("Art %1 · %2 · %3 × %4 · %5 child variants%6")
                           .arg(binding.art)
                           .arg(role(binding.policy))
                           .arg(sprite.width())
                           .arg(sprite.height())
                           .arg(sprite.children().size())
                           .arg(model.modified(selected_, child_) ? " · edited" : ""));
        const auto pixels = model.pixels(selected_, child_);
        int mode = channel_->currentIndex();
        if ((mode == 0 && sprite.colorFormat() != 1) || (mode == 1 && pixels.alpha.isEmpty())) {
            QSignalBlocker block(channel_);
            channel_->setCurrentIndex(2);
            mode = 2;
        }
        QVector<QColor> colors(256, Qt::black);
        if (mode == 0) {
            const auto words = model.palette(selected_, child_);
            for (int i = 0; i < 256; ++i)
                colors[i] = color(u16(words, i * 2));
        } else if (mode == 1)
            for (int i = 0; i < 256; ++i) {
                const int value = sprite.alphaFormat() ? i : std::min(i, 15) * 17;
                colors[i] = QColor(value, value, value);
            }
        for (int i = 0; i < 256; ++i) {
            auto item = palette_->item(i / 16, i % 16);
            if (!item) {
                item = new QTableWidgetItem;
                palette_->setItem(i / 16, i % 16, item);
            }
            item->setBackground(colors[i]);
            item->setToolTip(QString("%1 · %2").arg(i).arg(colors[i].name()));
        }
        ink_->setEnabled(mode != 2);
        ink_->setRange(0, mode == 1 && !sprite.alphaFormat() ? 15 : 255);
        palette_->setVisible(mode != 2);
        const auto identity =
            selected_ + ":" + QString::number(child_) + ":" + QString::number(mode);
        const auto image = model.image(selected_, child_);
        if (mode == 2) {
            auto asset = SpriteAsset::fromImage("Combat sprite", image);
            asset.id = identity;
            canvas_->setAsset(&asset, 0, 0);
        } else {
            canvas_->setIndexedAsset(identity, {sprite.width(), sprite.height()},
                                     mode == 1 ? pixels.alpha : pixels.color, colors, 0);
            canvas_->setIndexColor(quint8(ink_->value()));
        }
        canvas_->setEnabled(true);
        const int scale = std::max(1, std::min(4, 240 / std::max(image.width(), image.height())));
        preview_->setPixmap(QPixmap::fromImage(
            image.scaled(image.size() * scale, Qt::KeepAspectRatio, Qt::FastTransformation)));
        notice_->setText(
            "Color indices and opacity edit separate planes; selection moves affect the chosen "
            "plane. Combined image tools edit both. Changes affect every sequence using this "
            "descriptor member; other descriptors keep their original shared resource.");
    } catch (const std::exception &e) {
        canvas_->setAsset(nullptr, 0, 0);
        notice_->setText(QString::fromUtf8(e.what()));
    }
}
void CombatEditor::apply(const SpritePixels &pixels, const QString &label) {
    auto next = *project_->combatArtwork();
    next.setPixels(selected_, child_, pixels);
    if (next.json() != project_->combatArtwork()->json())
        undo_->push(new CombatCommand(project_, label, next.json()));
}
void CombatEditor::assignImage(const QImage &image) {
    apply(project_->combatArtwork()->mapImage(selected_, child_, image),
          "Assign combat sprite image");
}
QString CombatEditor::copyToLibrary() {
    auto asset = SpriteAsset::fromImage(QString("Combat art %1 · child %2")
                                            .arg(project_->combatArtwork()->binding(selected_).art)
                                            .arg(child_),
                                        project_->combatArtwork()->image(selected_, child_));
    asset.provenance = {{"gameSource", "combat-sprite"}, {"binding", selected_}, {"child", child_}};
    const auto id = asset.id;
    auto library = project_->sprites();
    library.replace(asset);
    undo_->push(new LibraryCommand(project_, library));
    return id;
}
void CombatEditor::importImage(const QString &path) {
    assignImage(loadArtworkImage(path));
}
void CombatEditor::exportImage(const QString &path) const {
    const auto source = QFileInfo(project_->rom().sourcePath()).canonicalFilePath();
    if (!source.isEmpty() &&
        (QFileInfo(path).canonicalFilePath() == source ||
         QFileInfo(path).absoluteFilePath().compare(source, Qt::CaseInsensitive) == 0))
        throw Error("Choose a path different from the source ROM");
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!project_->combatArtwork()->image(selected_, child_).save(&buffer, "PNG"))
        throw Error("Cannot encode combat sprite");
    atomicWrite(path, bytes);
}
void CombatEditor::prepareImage(const QImage &image) {
    const auto id = selected_;
    const auto child = child_;
    const auto before = project_->combatArtwork()->json();
    const auto original = project_->combatArtwork()->pixels(id, child);
    const auto sprite = project_->combatArtwork()->original(id);
    const auto prepared = prepareArtworkImage(
        this, image,
        {{sprite.width(), sprite.height()},
         "combat",
         "Assign combat sprite image",
         "The current sprite palette and native opacity precision determine the exported pixels.",
         false,
         false,
         sprite.colorFormat() != 3},
        [&](const QImage &target, int, bool dither) {
            auto next = *project_->combatArtwork();
            const auto pixels = next.mapImage(id, child, target, dither);
            next.setPixels(id, child, pixels);
            return ArtImageMapping{pixels.color + pixels.alpha, next.image(id, child)};
        });
    if (prepared) {
        if (selected_ != id || child_ != child || project_->combatArtwork()->json() != before)
            throw Error("Combat artwork changed while assignment was open");
        apply({prepared->left(original.color.size()), prepared->mid(original.color.size())},
              "Assign combat sprite image");
    }
}
} // namespace lordly
