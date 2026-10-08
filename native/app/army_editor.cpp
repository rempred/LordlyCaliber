#include "app/view_position.h"
#include "app/army_editor.h"
#include "app/art_image_assignment.h"
#include "app/sprite_editor.h"
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
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
class ArmyCommand final : public QUndoCommand {
  public:
    ArmyCommand(Project *project, QString label, QJsonObject after)
        : QUndoCommand(label), project_(project), before_(project->armyArtwork()->json()),
          after_(std::move(after)) {}
    void undo() override {
        project_->setArmyArtwork(before_);
    }
    void redo() override {
        project_->setArmyArtwork(after_);
    }

  private:
    Project *project_;
    QJsonObject before_, after_;
};
class LibraryCommand final : public QUndoCommand {
  public:
    LibraryCommand(Project *project, SpriteLibrary after)
        : QUndoCommand("Copy Army sprite to library"), project_(project),
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
QColor color(quint16 word) {
    return {(255 * (word >> 11) + 15) / 31, (255 * ((word >> 6) & 31) + 15) / 31,
            (255 * ((word >> 1) & 31) + 15) / 31, (word & 1) ? 255 : 0};
}
} // namespace
ArmyEditor::ArmyEditor(Project *project, QUndoStack *undo, std::function<void()> changed,
                       QWidget *parent)
    : QWidget(parent), project_(project), undo_(undo), changed_(std::move(changed)) {
    setObjectName("armyEditor");
    auto outer = new QVBoxLayout(this);
    outer->addWidget(
        new QLabel("<b>Army and formation sprites</b> · Select a class to edit its own artwork"));
    notice_ = new QLabel;
    notice_->setObjectName("armyNotice");
    notice_->setWordWrap(true);
    try {
        project_->loadArmyArtwork();
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
    action("Import image…", "armyImport", [this] {
        const auto path = QFileDialog::getOpenFileName(this, "Prepare Army sprite", {},
                                                       "Images (*.png *.jpg *.jpeg)");
        if (!path.isEmpty())
            prepareImage(loadArtworkImage(path));
    });
    action("Export PNG…", "armyExport", [this] {
        const auto path = QFileDialog::getSaveFileName(this, "Export Army sprite",
                                                       (class_ >= 0 ? project_->record(Table::Classes, class_).name : modelName(model_)) + ".png", "PNG (*.png)");
        if (!path.isEmpty())
            exportImage(path);
    });
    action("Copy to sprite library", "armyCopyToLibrary", [this] { copyToLibrary(); });
    restoreSprite_ = action("Restore loaded sprite", "armyRestore", [this] {
        auto next = *project_->armyArtwork();
        if (class_ >= 0 && next.routes().contains(class_)) next.restoreRoute(class_);
        else next.restore(atlas_, model_);
        if (next.json() != project_->armyArtwork()->json())
            undo_->push(new ArmyCommand(project_, "Restore Army sprite", next.json()));
    });
    create_ = action("Create missing plane", "armyCreate", [this] {
        const auto &spec = ArmyArtwork::specs()[atlas_];
        apply(QByteArray(spec.width * spec.height, char(spec.transparent.first())),
              "Create Army sprite plane");
    });
    actions->addStretch();
    auto split = new QSplitter;
    outer->addWidget(split, 1);
    auto browser = new QWidget;
    auto browse = new QVBoxLayout(browser);
    browse->setContentsMargins(0, 0, 0, 0);
    viewMode_ = new QComboBox;
    viewMode_->setObjectName("armyViewMode");
    viewMode_->addItems({"Classes — independent edits", "Shared models — advanced"});
    browse->addWidget(viewMode_);
    atlasSelect_ = new QComboBox;
    atlasSelect_->setObjectName("armyAtlas");
    for (const auto &spec : ArmyArtwork::specs())
        atlasSelect_->addItem(spec.label);
    browse->addWidget(atlasSelect_);
    search_ = new QLineEdit;
    search_->setPlaceholderText("Find model or class");
    search_->setObjectName("armySearch");
    browse->addWidget(search_);
    models_ = new QListWidget;
    models_->setObjectName("armyModels");
    models_->setIconSize({48, 48});
    browse->addWidget(models_);
    split->addWidget(browser);
    auto editing = new QWidget;
    auto edit = new QVBoxLayout(editing);
    edit->setContentsMargins(0, 0, 0, 0);
    title_ = new QLabel;
    edit->addWidget(title_);
    consumers_ = new QLabel;
    consumers_->setObjectName("armyConsumers");
    consumers_->setWordWrap(true);
    edit->addWidget(consumers_);
    auto routeRow = new QHBoxLayout;
    edit->addLayout(routeRow);
    routeClass_ = new QComboBox;
    routeClass_->setObjectName("armyRouteClass");
    routeRow->addWidget(routeClass_, 1);
    assignRoute_ = new QPushButton("Assign selected model");
    assignRoute_->setObjectName("armyAssignRoute");
    routeRow->addWidget(assignRoute_);
    auto showRoute = new QPushButton("Show assigned");
    showRoute->setObjectName("armyShowRoute");
    routeRow->addWidget(showRoute);
    restoreRoute_ = new QPushButton("Restore class route");
    restoreRoute_->setObjectName("armyRestoreRoute");
    routeRow->addWidget(restoreRoute_);
    routeInfo_ = new QLabel;
    routeInfo_->setObjectName("armyRouteInfo");
    routeInfo_->setWordWrap(true);
    edit->addWidget(routeInfo_);
    connect(routeClass_, &QComboBox::currentIndexChanged, this, [this] {
        if (!refreshing_)
            refreshRoute();
    });
    connect(assignRoute_, &QPushButton::clicked, this, [this] {
        guarded([this] {
            auto next = *project_->armyArtwork();
            next.setRoute(project_->classData(), routeClass_->currentData().toInt(), atlas_ / 2,
                          model_);
            if (next.json() != project_->armyArtwork()->json())
                undo_->push(new ArmyCommand(project_, "Assign Army model to class", next.json()));
        });
    });
    connect(restoreRoute_, &QPushButton::clicked, this, [this] {
        guarded([this] {
            auto next = *project_->armyArtwork();
            next.restoreRoute(routeClass_->currentData().toInt());
            if (next.json() != project_->armyArtwork()->json())
                undo_->push(new ArmyCommand(project_, "Restore Army class route", next.json()));
        });
    });
    connect(showRoute, &QPushButton::clicked, this, [this] {
        guarded([this] {
            const auto asset = project_->classData().entries()[routeClass_->currentData().toInt()];
            selectModel(ArmyArtwork::classLane(asset) * 2 + atlas_ % 2,
                        ArmyArtwork::classModel(asset), paletteOrdinal_);
        });
    });
    auto tools = new QHBoxLayout;
    edit->addLayout(tools);
    paletteSelect_ = new QComboBox;
    paletteSelect_->setObjectName("armyPalette");
    paletteSelect_->addItems({"Palette A", "Palette B"});
    tools->addWidget(paletteSelect_);
    auto tool = new QComboBox;
    tool->setObjectName("armyTool");
    tool->addItems({"Pencil", "Eraser", "Fill", "Pick index", "Select", "Move selection"});
    tools->addWidget(tool);
    ink_ = new QSpinBox;
    ink_->setObjectName("armyInk");
    ink_->setRange(0, 255);
    ink_->setValue(1);
    ink_->setPrefix("Index ");
    tools->addWidget(ink_);
    auto brush = new QSpinBox;
    brush->setRange(1, 16);
    brush->setPrefix("Brush ");
    tools->addWidget(brush);
    auto zoom = new QSpinBox;
    zoom->setRange(1, 32);
    zoom->setValue(12);
    zoom->setPrefix("Zoom ");
    tools->addWidget(zoom);
    auto grid = new QCheckBox("Grid");
    grid->setChecked(true);
    tools->addWidget(grid);
    auto deselect = new QPushButton("Deselect");
    tools->addWidget(deselect);
    tools->addStretch();
    auto row = new QHBoxLayout;
    edit->addLayout(row, 1);
    canvas_ = new SpriteCanvas;
    canvas_->setObjectName("armyCanvas");
    canvas_->setZoom(12);
    auto scroll = new QScrollArea;
    scroll->setWidget(canvas_);
    scroll->setAlignment(Qt::AlignCenter);
    row->addWidget(scroll, 1);
    auto paletteColumn = new QVBoxLayout;
    row->addLayout(paletteColumn);
    colors_ = new QTableWidget(16, 16);
    colors_->setObjectName("armyColors");
    colors_->horizontalHeader()->hide();
    colors_->verticalHeader()->hide();
    for (auto header : {colors_->horizontalHeader(), colors_->verticalHeader()}) {
        header->setMinimumSectionSize(18);
        header->setDefaultSectionSize(18);
        header->setSectionResizeMode(QHeaderView::Fixed);
    }
    colors_->setFixedSize(292, 292);
    colors_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    colors_->setSelectionMode(QAbstractItemView::NoSelection);
    paletteColumn->addWidget(colors_);
    paletteColumn->addWidget(new QLabel("Other palette"));
    alternate_ = new QLabel;
    alternate_->setObjectName("armyOtherPalette");
    alternate_->setAlignment(Qt::AlignCenter);
    paletteColumn->addWidget(alternate_);
    paletteColumn->addStretch();
    auto libraryRow = new QHBoxLayout;
    edit->addLayout(libraryRow);
    library_ = new QComboBox;
    library_->setObjectName("armyLibrary");
    libraryRow->addWidget(library_, 1);
    frame_ = new QSpinBox;
    frame_->setMinimum(1);
    frame_->setPrefix("Frame ");
    libraryRow->addWidget(frame_);
    auto assign = new QPushButton("Assign library image…");
    assign->setObjectName("armyAssignLibrary");
    libraryRow->addWidget(assign);
    split->addWidget(editing);
    split->setSizes({290, 960});
    split->setStretchFactor(1, 1);
    outer->addWidget(notice_);
    connect(atlasSelect_, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (!refreshing_) {
            if (atlas_ / 2 != i / 2) class_ = -1;
            atlas_ = i;
            model_ = 0;
            refresh();
        }
    });
    connect(viewMode_, &QComboBox::currentIndexChanged, this, [this] {
        if (!refreshing_) { class_ = -1; refresh(); }
    });
    connect(paletteSelect_, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (!refreshing_) {
            paletteOrdinal_ = i;
            refresh();
        }
    });
    connect(models_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (!refreshing_ && row >= 0) {
            model_ = models_->item(row)->data(Qt::UserRole).toInt();
            class_ = models_->item(row)->data(Qt::UserRole + 1).toInt();
            if (class_ >= 0) {
                const QSignalBlocker block(routeClass_);
                routeClass_->setCurrentIndex(routeClass_->findData(class_));
            }
            refreshPlane();
        }
    });
    connect(search_, &QLineEdit::textChanged, this, [this] {
        for (int i = 0; i < models_->count(); ++i)
            models_->item(i)->setHidden(
                !models_->item(i)->toolTip().contains(search_->text(), Qt::CaseInsensitive));
    });
    connect(tool, &QComboBox::currentIndexChanged, this,
            [this](int i) { canvas_->setTool(SpriteCanvas::Tool(i)); });
    connect(ink_, &QSpinBox::valueChanged, this,
            [this](int i) { canvas_->setIndexColor(quint8(i)); });
    connect(canvas_, &SpriteCanvas::indexPicked, ink_, &QSpinBox::setValue);
    connect(brush, &QSpinBox::valueChanged, canvas_, &SpriteCanvas::setBrush);
    connect(zoom, &QSpinBox::valueChanged, canvas_, &SpriteCanvas::setZoom);
    connect(grid, &QCheckBox::toggled, canvas_, &SpriteCanvas::setGrid);
    connect(deselect, &QPushButton::clicked, canvas_, &SpriteCanvas::clearSelection);
    connect(colors_, &QTableWidget::cellClicked, this,
            [this](int y, int x) { ink_->setValue(y * 16 + x); });
    connect(canvas_, &SpriteCanvas::indicesCommitted, this,
            [this](const QByteArray &pixels, QPoint position) {
                guarded([&] {
                    const auto &spec = ArmyArtwork::specs()[atlas_];
                    auto shifted = pixels;
                    if (!position.isNull()) {
                        shifted.fill(char(spec.transparent.first()));
                        for (int y = 0; y < spec.height; ++y)
                            for (int x = 0; x < spec.width; ++x) {
                                const auto source = QPoint(x, y) - position;
                                if (source.x() >= 0 && source.x() < spec.width && source.y() >= 0 &&
                                    source.y() < spec.height)
                                    shifted[y * spec.width + x] =
                                        pixels[source.y() * spec.width + source.x()];
                            }
                    }
                    apply(shifted, "Paint Army sprite");
                });
            });
    connect(library_, &QComboBox::currentIndexChanged, this, [this] {
        frame_->setMaximum(
            library_->currentIndex() < 0
                ? 1
                : int(project_->sprites().asset(library_->currentData().toString()).frames.size()));
    });
    connect(assign, &QPushButton::clicked, this, [this] {
        guarded([this] {
            if (library_->currentIndex() < 0)
                throw Error("Choose a sprite library image");
            prepareImage(project_->sprites()
                             .asset(library_->currentData().toString())
                             .composite(frame_->value() - 1));
        });
    });
    connect(undo_, &QUndoStack::indexChanged, this, [this] {
        refresh();
        changed_();
    });
    refresh();
}
ArmyEditor::~ArmyEditor() {
    disconnect(undo_, nullptr, this, nullptr);
}
void ArmyEditor::guarded(const std::function<void()> &action) {
    try {
        action();
        notice_->setText(class_ >= 0 ? "Changes affect only the selected class. Both fixed palettes use these pixel indices."
                                    : "Advanced shared-model editing affects every listed class. Both palettes use these pixel indices.");
    } catch (const std::exception &e) {
        refresh();
        notice_->setText(QString::fromUtf8(e.what()));
    }
}
QString ArmyEditor::modelName(int model) const {
    const auto users = project_->armyArtwork()->consumers(project_->classData(), atlas_, model);
    if (users.isEmpty())
        return "Unassigned model";
    return project_->record(Table::Classes, users.first()).name +
           (users.size() > 1 ? QString(" +%1").arg(users.size() - 1) : QString());
}
void ArmyEditor::selectModel(int atlas, int model, int palette) {
    if (!project_->armyArtwork())
        throw Error("Army artwork is unavailable");
    project_->armyArtwork()->words(atlas, model, palette);
    {const QSignalBlocker block(viewMode_); viewMode_->setCurrentIndex(1);}
    class_ = -1;
    atlas_ = atlas;
    model_ = model;
    paletteOrdinal_ = palette;
    refresh();
}
void ArmyEditor::selectClass(int id, int facing, int palette) {
    const auto classes = project_->classData();
    if (id < 0 || id >= classes.entries().size() || facing < 0 || facing > 1)
        throw Error("Invalid Army class or facing");
    {const QSignalBlocker block(viewMode_); viewMode_->setCurrentIndex(0);}
    class_ = id;
    atlas_ = ArmyArtwork::classLane(classes.entries()[id]) * 2 + facing;
    paletteOrdinal_ = palette;
    refresh();
}
void ArmyEditor::refresh() {
    const ViewPosition keepView(this);
    if (!project_->armyArtwork())
        return;
    refreshing_ = true;
    const auto classes = project_->classData();
    if (viewMode_->currentIndex() == 0 && class_ >= 0 && class_ < classes.entries().size()) {
        atlas_ = ArmyArtwork::classLane(classes.entries()[class_]) * 2 + atlas_ % 2;
        model_ = ArmyArtwork::classModel(classes.entries()[class_]);
    }
    atlasSelect_->setCurrentIndex(atlas_);
    paletteSelect_->setCurrentIndex(paletteOrdinal_);
    models_->clear();
    const auto append = [&](int model, int classId) {
        const bool present = project_->armyArtwork()->present(atlas_, model);
        const auto label = classId >= 0 ? QString("%1 · %2").arg(classId).arg(project_->record(Table::Classes, classId).name)
                                       : QString("0x%1 · %2").arg(model, 2, 16, QChar('0')).arg(modelName(model));
        auto item = new QListWidgetItem(
            QString("%1%2%3")
                .arg(label)
                .arg(present ? "" : " · missing")
                .arg(project_->armyArtwork()->modified(atlas_, model) ? " *" : ""),
            models_);
        item->setData(Qt::UserRole, model);
        item->setData(Qt::UserRole + 1, classId);
        item->setIcon(
            QPixmap::fromImage(project_->armyArtwork()
                                   ->image(atlas_, model, paletteOrdinal_)
                                   .scaled(48, 48, Qt::KeepAspectRatio, Qt::FastTransformation)));
        QStringList names;
        for (const auto id : project_->armyArtwork()->consumers(classes, atlas_, model))
            names << project_->record(Table::Classes, id).name;
        item->setToolTip(item->text() + "\n" + names.join(", "));
        item->setHidden(!item->toolTip().contains(search_->text(), Qt::CaseInsensitive));
        if (classId >= 0 ? classId == class_ : model == model_)
            models_->setCurrentItem(item);
    };
    if (viewMode_->currentIndex() == 0) {
        for (int id = 1; id < classes.entries().size(); ++id)
            if (ArmyArtwork::classLane(classes.entries()[id]) == atlas_ / 2)
                append(ArmyArtwork::classModel(classes.entries()[id]), id);
    } else {
        class_ = -1;
        for (int model = 0; model < ArmyArtwork::specs()[atlas_].capacity; ++model) append(model, -1);
    }
    if (!models_->currentItem() && models_->count()) models_->setCurrentRow(0);
    if (models_->currentItem()) {
        model_ = models_->currentItem()->data(Qt::UserRole).toInt();
        class_ = models_->currentItem()->data(Qt::UserRole + 1).toInt();
    }
    const auto selectedClass =
        class_ >= 0 ? class_ : routeClass_->currentData().isValid() ? routeClass_->currentData().toInt() : 1;
    for (auto widget : {static_cast<QWidget *>(routeClass_), static_cast<QWidget *>(assignRoute_),
                        static_cast<QWidget *>(restoreRoute_), static_cast<QWidget *>(findChild<QPushButton *>("armyShowRoute"))})
        widget->setVisible(viewMode_->currentIndex() == 1);
    {
        QSignalBlocker blocker(routeClass_);
        routeClass_->clear();
        for (const auto &row : project_->records(Table::Classes))
            routeClass_->addItem(QString("%1 · %2").arg(row.id).arg(row.name), row.id);
        routeClass_->setCurrentIndex(std::max(0, routeClass_->findData(selectedClass)));
    }
    const auto old = library_->currentData().toString();
    {
        QSignalBlocker blocker(library_);
        library_->clear();
        for (const auto &asset : project_->sprites().assets())
            library_->addItem(asset.name, asset.id);
        const auto at = library_->findData(old);
        if (at >= 0)
            library_->setCurrentIndex(at);
    }
    frame_->setMaximum(
        library_->currentIndex() < 0
            ? 1
            : int(project_->sprites().asset(library_->currentData().toString()).frames.size()));
    refreshing_ = false;
    refreshPlane();
}
void ArmyEditor::refreshPlane() {
    const auto &spec = ArmyArtwork::specs()[atlas_];
    const bool present = project_->armyArtwork()->present(atlas_, model_);
    const auto palette = project_->armyArtwork()->palette(atlas_, paletteOrdinal_);
    QVector<QColor> colors;
    for (int i = 0; i < 256; ++i) {
        const auto c = color(palette[i]);
        colors.append(c);
        auto item = colors_->item(i / 16, i % 16);
        if (!item) {
            item = new QTableWidgetItem;
            colors_->setItem(i / 16, i % 16, item);
        }
        item->setBackground(c.alpha() ? c : QColor(230, 230, 230));
        item->setText(c.alpha() ? "" : "×");
        item->setTextAlignment(Qt::AlignCenter);
        item->setToolTip(QString("Index %1 · %2").arg(i).arg(c.alpha() ? c.name() : "transparent"));
    }
    if (present)
        canvas_->setIndexedAsset(QString("army-%1-%2").arg(atlas_).arg(model_),
                                 {spec.width, spec.height},
                                 project_->armyArtwork()->indices(atlas_, model_), colors,
                                 quint8(spec.transparent.first()));
    else
        canvas_->setAsset(nullptr, 0, 0);
    canvas_->setEnabled(present);
    canvas_->setIndexColor(quint8(ink_->value()));
    create_->setEnabled(!present);
    alternate_->setPixmap(
        QPixmap::fromImage(project_->armyArtwork()
                               ->image(atlas_, model_, 1 - paletteOrdinal_)
                               .scaled(spec.width * 3, spec.height * 3, Qt::IgnoreAspectRatio,
                                       Qt::FastTransformation)));
    title_->setText(QString("%1 · Model 0x%2 · %3 × %4%5")
                        .arg(class_ >= 0 ? project_->record(Table::Classes, class_).name : modelName(model_))
                        .arg(model_, 2, 16, QChar('0'))
                        .arg(spec.width)
                        .arg(spec.height)
                        .arg(present ? "" : " · No loaded plane"));
    QStringList names;
    for (const auto id : project_->armyArtwork()->consumers(project_->classData(), atlas_, model_))
        names << QString("%1 (%2)").arg(project_->record(Table::Classes, id).name).arg(id);
    consumers_->setText(names.isEmpty() ? "No current class uses this model."
                       : (class_ >= 0 ? "Currently uses the same source artwork as: " : "Shared by: ") + names.join(", "));
    restoreSprite_->setText(class_ >= 0 ? "Restore class artwork" : "Restore loaded sprite");
    notice_->setText(
        present
            ? (class_ >= 0 ? "Editing automatically separates shared artwork. Other classes remain unchanged."
                           : "Paint indices to retain both palette appearances. Edits affect every listed class.")
            : "This plane is absent in the loaded ROM. Create a plane or import "
              "artwork to author it.");
    refreshRoute();
}
void ArmyEditor::refreshRoute() {
    if (routeClass_->currentIndex() < 0)
        return;
    const auto id = routeClass_->currentData().toInt();
    const auto asset = project_->classData().entries()[id];
    const auto lane = ArmyArtwork::classLane(asset), model = ArmyArtwork::classModel(asset);
    const bool compatible = (u8(asset.record, ClassData::unitSizeOffset) == 2) == (atlas_ / 2 == 2);
    assignRoute_->setEnabled(compatible);
    restoreRoute_->setEnabled(project_->armyArtwork()->routes().contains(id));
    QString info = QString("Assigned: %1 · Model 0x%2. ")
                       .arg(QStringList{"Ordinary", "Special", "Large"}[lane])
                       .arg(model, 2, 16, QChar('0'));
    if (!compatible)
        info += "Choose a model group matching this class's unit size.";
    else
        info += "An assignment selects both the front and player-facing model.";
    if (!project_->armyArtwork()->present(lane * 2, model) ||
        !project_->armyArtwork()->present(lane * 2 + 1, model))
        info += " A facing is missing; create or import it before exporting a new assignment.";
    routeInfo_->setText(info);
}
void ArmyEditor::apply(const QByteArray &pixels, const QString &label) {
    auto next = *project_->armyArtwork();
    if (pixels == next.indices(atlas_, model_)) return;
    const int destination = class_ >= 0 ? next.isolateClass(project_->classData(), class_) : model_;
    next.setIndices(atlas_, destination, pixels);
    if (next.json() != project_->armyArtwork()->json())
        undo_->push(new ArmyCommand(project_, label, next.json()));
}
void ArmyEditor::assignImage(const QImage &image, int threshold) {
    apply(project_->armyArtwork()->mapImage(atlas_, paletteOrdinal_, image, threshold,
                                            project_->armyArtwork()->indices(atlas_, model_)),
          "Assign Army sprite image");
}
QString ArmyEditor::copyToLibrary() {
    if (!project_->armyArtwork()->present(atlas_, model_))
        throw Error("Create or import this plane first");
    auto asset =
        SpriteAsset::fromImage((class_ >= 0 ? project_->record(Table::Classes, class_).name : modelName(model_)) + " · " + ArmyArtwork::specs()[atlas_].label,
                               project_->armyArtwork()->image(atlas_, model_, paletteOrdinal_));
    asset.provenance = {{"gameSource", "army-sprite"},
                        {"atlas", ArmyArtwork::specs()[atlas_].slug},
                        {"modelId", model_},
                        {"palette", paletteOrdinal_}};
    const auto id = asset.id;
    auto library = project_->sprites();
    library.replace(asset);
    undo_->push(new LibraryCommand(project_, library));
    return id;
}
void ArmyEditor::importImage(const QString &path) {
    assignImage(loadArtworkImage(path));
}
void ArmyEditor::exportImage(const QString &path) const {
    if (!project_->armyArtwork()->present(atlas_, model_))
        throw Error("Create or import this plane first");
    const auto source = QFileInfo(project_->rom().sourcePath()).canonicalFilePath();
    if (!source.isEmpty() &&
        (QFileInfo(path).canonicalFilePath() == source ||
         QFileInfo(path).absoluteFilePath().compare(source, Qt::CaseInsensitive) == 0))
        throw Error("Choose a path different from the source ROM");
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!project_->armyArtwork()->image(atlas_, model_, paletteOrdinal_).save(&buffer, "PNG"))
        throw Error("Cannot encode Army sprite");
    atomicWrite(path, bytes);
}
void ArmyEditor::prepareImage(const QImage &image) {
    const auto atlas = atlas_, model = model_, ordinal = paletteOrdinal_;
    const auto before = project_->armyArtwork()->json();
    const auto &spec = ArmyArtwork::specs()[atlas];
    const auto prepared =
        prepareArtworkImage(this, image,
                            {{spec.width, spec.height},
                             "army",
                             "Assign Army sprite image",
                             "The selected fixed palette determines the preview. Both palettes "
                             "share the resulting pixel indices.",
                             false,
                             true,
                             true},
                            [&](const QImage &target, int threshold, bool dither) {
                                auto next = *project_->armyArtwork();
                                const auto pixels =
                                    next.mapImage(atlas, ordinal, target, threshold,
                                                  next.indices(atlas, model), dither);
                                next.setIndices(atlas, model, pixels);
                                return ArtImageMapping{pixels, next.image(atlas, model, ordinal)};
                            });
    if (prepared) {
        if (atlas_ != atlas || model_ != model || paletteOrdinal_ != ordinal ||
            project_->armyArtwork()->json() != before)
            throw Error("Army artwork changed while assignment was open");
        apply(*prepared, "Assign Army sprite image");
    }
}
} // namespace lordly
