#include "app/portrait_editor.h"
#include "app/sprite_editor.h"
#include <QBuffer>
#include <QCheckBox>
#include <QColorDialog>
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
#include <QSet>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QUndoStack>
#include <QVBoxLayout>
#include <algorithm>
namespace lordly {
namespace {
class PortraitCommand final : public QUndoCommand {
  public:
    PortraitCommand(Project *project, QString label, QJsonObject after)
        : QUndoCommand(label), project_(project), before_(project->avatarArtwork()->json()),
          after_(std::move(after)) {}
    void undo() override {
        project_->setAvatarArtwork(before_);
    }
    void redo() override {
        project_->setAvatarArtwork(after_);
    }

  private:
    Project *project_;
    QJsonObject before_, after_;
};
class LibraryCommand final : public QUndoCommand {
  public:
    LibraryCommand(Project *project, SpriteLibrary after)
        : QUndoCommand("Copy portrait to sprite library"), project_(project),
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
QImage readImage(const QString &path) {
    QImageReader reader(path);
    const auto size = reader.size();
    if (size.width() < 1 || size.height() < 1 || size.width() > 4096 || size.height() > 4096)
        throw Error("Image dimensions must be 1–4096 pixels");
    const auto image = reader.read();
    if (image.isNull())
        throw Error("Cannot read image: " + reader.errorString());
    return image;
}
} // namespace
PortraitEditor::PortraitEditor(Project *project, QUndoStack *undo, std::function<void()> changed,
                               QWidget *parent)
    : QWidget(parent), project_(project), undo_(undo), changed_(std::move(changed)) {
    setObjectName("portraitEditor");
    auto outer = new QVBoxLayout(this);
    outer->addWidget(
        new QLabel("<b>Class portraits</b> · Character cards and dialogue · 40 × 48 pixels"));
    notice_ = new QLabel;
    notice_->setObjectName("portraitNotice");
    notice_->setWordWrap(true);
    try {
        project_->loadAvatarArtwork();
    } catch (const std::exception &e) {
        notice_->setText(QString::fromUtf8(e.what()));
        outer->addWidget(notice_);
        return;
    }
    auto actions = new QHBoxLayout;
    outer->addLayout(actions);
    auto action = [&](QString text, const char *id, std::function<void()> callback) {
        auto b = new QPushButton(text);
        b->setObjectName(id);
        actions->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, callback] { guarded(callback); });
        return b;
    };
    action("Import image…", "portraitImport", [this] {
        const auto path = QFileDialog::getOpenFileName(this, "Prepare portrait", {},
                                                       "Images (*.png *.jpg *.jpeg)");
        if (!path.isEmpty())
            prepareImage(readImage(path));
    });
    action("Export PNG…", "portraitExport", [this] {
        const auto path = QFileDialog::getSaveFileName(
            this, "Export portrait", project_->record(Table::Classes, selected_).name + ".png",
            "PNG (*.png)");
        if (!path.isEmpty())
            exportImage(path);
    });
    action("Copy to sprite library", "portraitCopyToLibrary", [this] { copyToLibrary(); });
    action("Restore loaded portrait", "portraitRestore", [this] {
        auto next = *project_->avatarArtwork();
        next.restore(selected_, selector_);
        if (both_->isChecked())
            next.restore(selected_, 1 - selector_);
        if (next.json() != project_->avatarArtwork()->json())
            undo_->push(new PortraitCommand(project_, "Restore portrait", next.json()));
    });
    actions->addStretch();
    auto split = new QSplitter;
    outer->addWidget(split, 1);
    auto browser = new QWidget;
    auto browse = new QVBoxLayout(browser);
    browse->setContentsMargins(0, 0, 0, 0);
    search_ = new QLineEdit;
    search_->setPlaceholderText("Find class or ID");
    search_->setObjectName("portraitSearch");
    browse->addWidget(search_);
    classes_ = new QListWidget;
    classes_->setObjectName("portraitClasses");
    classes_->setIconSize({40, 48});
    browse->addWidget(classes_);
    split->addWidget(browser);
    auto editing = new QWidget;
    auto edit = new QVBoxLayout(editing);
    edit->setContentsMargins(0, 0, 0, 0);
    title_ = new QLabel;
    edit->addWidget(title_);
    auto selectors = new QHBoxLayout;
    edit->addLayout(selectors);
    variant_ = new QComboBox;
    variant_->setObjectName("portraitVariant");
    variant_->addItems({"Portrait A", "Portrait B"});
    selectors->addWidget(variant_);
    both_ = new QCheckBox("Apply edits to both portraits");
    both_->setObjectName("portraitBoth");
    selectors->addWidget(both_);
    selectors->addStretch();
    auto tools = new QHBoxLayout;
    edit->addLayout(tools);
    auto tool = new QComboBox;
    tool->setObjectName("portraitTool");
    for (auto [label, value] : {std::pair{"Pencil", SpriteCanvas::Tool::Pencil},
                                {"Fill", SpriteCanvas::Tool::Fill},
                                {"Pick color", SpriteCanvas::Tool::Pick},
                                {"Select", SpriteCanvas::Tool::Select},
                                {"Move selection", SpriteCanvas::Tool::Move}})
        tool->addItem(label, int(value));
    tools->addWidget(tool);
    color_ = new QPushButton;
    color_->setObjectName("portraitColor");
    tools->addWidget(color_);
    auto brush = new QSpinBox;
    brush->setRange(1, 16);
    brush->setPrefix("Brush ");
    tools->addWidget(brush);
    auto zoom = new QSpinBox;
    zoom->setObjectName("portraitZoom");
    zoom->setRange(1, 24);
    zoom->setValue(8);
    zoom->setPrefix("Zoom ");
    tools->addWidget(zoom);
    auto grid = new QCheckBox("Grid");
    grid->setChecked(true);
    tools->addWidget(grid);
    auto clear = new QPushButton("Deselect");
    tools->addWidget(clear);
    tools->addStretch();
    auto workspace = new QHBoxLayout;
    edit->addLayout(workspace, 1);
    canvas_ = new SpriteCanvas;
    canvas_->setObjectName("portraitCanvas");
    canvas_->setZoom(8);
    auto scroll = new QScrollArea;
    scroll->setWidget(canvas_);
    scroll->setAlignment(Qt::AlignCenter);
    workspace->addWidget(scroll, 1);
    auto colors = new QVBoxLayout;
    workspace->addLayout(colors);
    colors->addWidget(new QLabel("Colors used (maximum 80)"));
    palette_ = new QTableWidget(10, 8);
    palette_->setObjectName("portraitPalette");
    palette_->horizontalHeader()->hide();
    palette_->verticalHeader()->hide();
    for (auto header : {palette_->horizontalHeader(), palette_->verticalHeader()}) {
        header->setMinimumSectionSize(22);
        header->setDefaultSectionSize(22);
        header->setSectionResizeMode(QHeaderView::Fixed);
    }
    palette_->setFixedSize(8 * 22 + 4, 10 * 22 + 4);
    palette_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    palette_->setSelectionMode(QAbstractItemView::NoSelection);
    colors->addWidget(palette_);
    colors->addStretch();
    auto libraryRow = new QHBoxLayout;
    edit->addLayout(libraryRow);
    library_ = new QComboBox;
    library_->setObjectName("portraitLibrary");
    libraryRow->addWidget(library_, 1);
    frame_ = new QSpinBox;
    frame_->setObjectName("portraitLibraryFrame");
    frame_->setMinimum(1);
    frame_->setPrefix("Frame ");
    libraryRow->addWidget(frame_);
    auto assign = new QPushButton("Assign library image…");
    assign->setObjectName("portraitAssignLibrary");
    libraryRow->addWidget(assign);
    split->addWidget(editing);
    split->setSizes({260, 950});
    split->setStretchFactor(1, 1);
    outer->addWidget(notice_);
    connect(tool, &QComboBox::currentIndexChanged, this,
            [=] { canvas_->setTool(SpriteCanvas::Tool(tool->currentData().toInt())); });
    connect(brush, &QSpinBox::valueChanged, canvas_, &SpriteCanvas::setBrush);
    connect(zoom, &QSpinBox::valueChanged, canvas_, &SpriteCanvas::setZoom);
    connect(grid, &QCheckBox::toggled, canvas_, &SpriteCanvas::setGrid);
    connect(clear, &QPushButton::clicked, canvas_, &SpriteCanvas::clearSelection);
    connect(color_, &QPushButton::clicked, this, [this] {
        const auto color = QColorDialog::getColor(paint_, this, "Portrait paint color");
        if (color.isValid())
            setColor(color);
    });
    connect(canvas_, &SpriteCanvas::colorPicked, this, &PortraitEditor::setColor);
    connect(palette_, &QTableWidget::cellClicked, this, [this](int row, int col) {
        const auto cell = palette_->item(row, col);
        if (cell && cell->data(Qt::UserRole).isValid())
            setColor(cell->data(Qt::UserRole).value<QColor>());
    });
    connect(canvas_, &SpriteCanvas::committed, this, [this](const QImage &image, QPoint position) {
        guarded([&] {
            auto asset = SpriteAsset::fromImage("Portrait", image);
            asset.frames[0].layers[0].position = position;
            assignImage(position.isNull() ? image : asset.composite(0));
        });
    });
    connect(variant_, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (!refreshing_) {
            selector_ = i;
            refresh();
        }
    });
    connect(classes_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (!refreshing_ && row >= 0) {
            selected_ = classes_->item(row)->data(Qt::UserRole).toInt();
            refreshPortrait();
        }
    });
    connect(search_, &QLineEdit::textChanged, this, [this] {
        for (int i = 0; i < classes_->count(); ++i)
            classes_->item(i)->setHidden(
                !classes_->item(i)->text().contains(search_->text(), Qt::CaseInsensitive));
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
                throw Error("Choose an image from the sprite library");
            prepareImage(project_->sprites()
                             .asset(library_->currentData().toString())
                             .composite(frame_->value() - 1));
        });
    });
    connect(undo_, &QUndoStack::indexChanged, this, [this] {
        refresh();
        changed_();
    });
    setColor(Qt::black);
    refresh();
}
PortraitEditor::~PortraitEditor() {
    disconnect(undo_, nullptr, this, nullptr);
}
void PortraitEditor::guarded(const std::function<void()> &action) {
    try {
        action();
        notice_->setText(
            "Only the selected class portraits change. Images use up to 80 opaque game colors.");
    } catch (const std::exception &e) {
        refresh();
        notice_->setText(QString::fromUtf8(e.what()));
    }
}
void PortraitEditor::selectPortrait(int id, int selector) {
    if (!project_->avatarArtwork())
        throw Error("Portrait artwork is unavailable");
    project_->avatarArtwork()->words(project_->classData(), id, selector);
    selected_ = id;
    selector_ = selector;
    refresh();
}
void PortraitEditor::setColor(QColor color) {
    color.setAlpha(255);
    paint_ = color;
    canvas_->setColor(color);
    color_->setText(color.name());
    color_->setStyleSheet("background-color:" + color.name() +
                          ";color:" + (color.lightness() < 128 ? "white" : "black"));
}
void PortraitEditor::refresh() {
    if (!project_->avatarArtwork())
        return;
    refreshing_ = true;
    const auto classData = project_->classData();
    selected_ = std::clamp(selected_, 0, int(classData.entries().size()) - 1);
    variant_->setCurrentIndex(selector_);
    classes_->clear();
    for (int id = 0; id < classData.entries().size(); ++id) {
        auto item = new QListWidgetItem(QString("%1 · %2%3")
                                            .arg(id)
                                            .arg(project_->record(Table::Classes, id).name)
                                            .arg(project_->avatarArtwork()->modified(id, 0) ||
                                                         project_->avatarArtwork()->modified(id, 1)
                                                     ? " *"
                                                     : ""),
                                        classes_);
        item->setData(Qt::UserRole, id);
        item->setIcon(
            QPixmap::fromImage(project_->avatarArtwork()->image(classData, id, selector_)));
        item->setHidden(!item->text().contains(search_->text(), Qt::CaseInsensitive));
        if (id == selected_)
            classes_->setCurrentItem(item);
    }
    const auto old = library_->currentData().toString();
    {
        QSignalBlocker blocker(library_);
        library_->clear();
        for (const auto &asset : project_->sprites().assets())
            library_->addItem(asset.name, asset.id);
        const auto index = library_->findData(old);
        if (index >= 0)
            library_->setCurrentIndex(index);
    }
    frame_->setMaximum(
        library_->currentIndex() < 0
            ? 1
            : int(project_->sprites().asset(library_->currentData().toString()).frames.size()));
    refreshing_ = false;
    refreshPortrait();
}
void PortraitEditor::refreshPortrait() {
    const auto classData = project_->classData();
    const auto words = project_->avatarArtwork()->words(classData, selected_, selector_);
    auto asset = SpriteAsset::fromImage("Portrait", AvatarArtwork::render(words));
    asset.id = QString("portrait-%1-%2").arg(selected_).arg(selector_);
    canvas_->setAsset(&asset, 0, 0);
    QSet<quint16> used;
    for (int i = 0; i < words.size(); i += 2)
        used.insert(u16(words, i));
    auto colors = used.values();
    std::sort(colors.begin(), colors.end());
    palette_->clearContents();
    for (int i = 0; i < colors.size(); ++i) {
        const auto word = colors[i];
        QColor color((255 * (word >> 11) + 15) / 31, (255 * ((word >> 6) & 31) + 15) / 31,
                     (255 * ((word >> 1) & 31) + 15) / 31);
        auto item = new QTableWidgetItem;
        item->setBackground(color);
        item->setData(Qt::UserRole, color);
        item->setToolTip(color.name());
        palette_->setItem(i / 8, i % 8, item);
    }
    title_->setText(QString("%1 · Class %2 · %3 / 80 colors%4")
                        .arg(project_->record(Table::Classes, selected_).name)
                        .arg(selected_)
                        .arg(colors.size())
                        .arg(words.isEmpty() ? " · No loaded portrait" : ""));
}
void PortraitEditor::apply(const QByteArray &words, const QString &label) {
    auto next = *project_->avatarArtwork();
    const auto classData = project_->classData();
    next.setWords(classData, selected_, selector_, words);
    if (both_->isChecked())
        next.setWords(classData, selected_, 1 - selector_, words);
    if (next.json() != project_->avatarArtwork()->json())
        undo_->push(new PortraitCommand(project_, label, next.json()));
}
void PortraitEditor::assignImage(const QImage &image, bool reduce) {
    apply(AvatarArtwork::mapImage(image, reduce), "Edit class portrait");
}
QString PortraitEditor::copyToLibrary() {
    auto library = project_->sprites();
    auto asset = SpriteAsset::fromImage(
        project_->record(Table::Classes, selected_).name +
            QString(" · Portrait %1").arg(selector_ ? "B" : "A"),
        project_->avatarArtwork()->image(project_->classData(), selected_, selector_));
    asset.provenance = {
        {"gameSource", "portrait"}, {"classId", selected_}, {"selector", selector_}};
    const auto id = asset.id;
    library.replace(asset);
    undo_->push(new LibraryCommand(project_, library));
    return id;
}
void PortraitEditor::exportImage(const QString &path) const {
    const auto source = QFileInfo(project_->rom().sourcePath()).canonicalFilePath();
    if (!source.isEmpty() &&
        (QFileInfo(path).canonicalFilePath() == source ||
         QFileInfo(path).absoluteFilePath().compare(source, Qt::CaseInsensitive) == 0))
        throw Error("Choose a path different from the source ROM");
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!project_->avatarArtwork()
             ->image(project_->classData(), selected_, selector_)
             .save(&buffer, "PNG"))
        throw Error("Cannot encode portrait");
    atomicWrite(path, bytes);
}
void PortraitEditor::importImage(const QString &path) {
    assignImage(readImage(path), true);
}
void PortraitEditor::prepareImage(const QImage &image) {
    if (image.isNull() || image.width() > 4096 || image.height() > 4096)
        throw Error("Invalid source image");
    const auto id = selected_, selector = selector_;
    const auto before = project_->avatarArtwork()->json();
    QByteArray prepared;
    QColor background = Qt::black;
    QDialog dialog(this);
    dialog.setObjectName("portraitImagePreparation");
    dialog.setWindowTitle("Assign portrait image");
    auto layout = new QVBoxLayout(&dialog);
    layout->addWidget(
        new QLabel("Preview the crop and final 80-color game image before assigning."));
    auto previews = new QHBoxLayout;
    layout->addLayout(previews);
    auto input = new QLabel, output = new QLabel;
    for (auto label : {input, output}) {
        label->setMinimumSize(200, 240);
        label->setAlignment(Qt::AlignCenter);
        previews->addWidget(label);
    }
    input->setPixmap(
        QPixmap::fromImage(image.scaled(200, 240, Qt::KeepAspectRatio, Qt::FastTransformation)));
    auto form = new QFormLayout;
    layout->addLayout(form);
    QComboBox fit;
    fit.setObjectName("portraitImportFit");
    fit.addItems({"Fit inside", "Fill and crop", "Stretch", "Keep pixel size"});
    form->addRow("Placement", &fit);
    QComboBox sampling;
    sampling.addItems({"Nearest pixel", "Smooth"});
    form->addRow("Sampling", &sampling);
    QDoubleSpinBox zoom;
    zoom.setRange(.125, 8);
    zoom.setValue(1);
    zoom.setSingleStep(.125);
    form->addRow("Zoom", &zoom);
    QSpinBox x, y;
    for (auto spin : {&x, &y})
        spin->setRange(-4096, 4096);
    form->addRow("Offset X (output pixels)", &x);
    form->addRow("Offset Y (output pixels)", &y);
    QPushButton fill("Black");
    form->addRow("Opaque background", &fill);
    auto status = new QLabel;
    status->setWordWrap(true);
    layout->addWidget(status);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons.button(QDialogButtonBox::Ok)->setText("Assign portrait");
    layout->addWidget(&buttons);
    auto update = [&] {
        try {
            double sx = 40.0 / image.width(), sy = 48.0 / image.height();
            if (fit.currentIndex() != 2)
                sx = sy = fit.currentIndex() == 0   ? std::min(sx, sy)
                          : fit.currentIndex() == 1 ? std::max(sx, sy)
                                                    : 1.;
            sx *= zoom.value();
            sy *= zoom.value();
            QImage target(40, 48, QImage::Format_RGBA8888);
            target.fill(background);
            QPainter painter(&target);
            painter.setRenderHint(QPainter::SmoothPixmapTransform, sampling.currentIndex() == 1);
            const QSizeF size(image.width() * sx, image.height() * sy);
            painter.drawImage(QRectF((40 - size.width()) / 2 + x.value(),
                                     (48 - size.height()) / 2 + y.value(), size.width(),
                                     size.height()),
                              image);
            painter.end();
            prepared = AvatarArtwork::mapImage(target, true, background);
            output->setPixmap(QPixmap::fromImage(AvatarArtwork::render(prepared).scaled(
                200, 240, Qt::IgnoreAspectRatio, Qt::FastTransformation)));
            status->setText("Left: source · Right: exact game colors. Transparent areas use the "
                            "background color.");
            buttons.button(QDialogButtonBox::Ok)->setEnabled(true);
        } catch (const std::exception &e) {
            prepared.clear();
            status->setText(QString::fromUtf8(e.what()));
            buttons.button(QDialogButtonBox::Ok)->setEnabled(false);
        }
    };
    connect(&fit, &QComboBox::currentIndexChanged, &dialog, update);
    connect(&sampling, &QComboBox::currentIndexChanged, &dialog, update);
    connect(&zoom, &QDoubleSpinBox::valueChanged, &dialog, update);
    connect(&x, &QSpinBox::valueChanged, &dialog, update);
    connect(&y, &QSpinBox::valueChanged, &dialog, update);
    connect(&fill, &QPushButton::clicked, &dialog, [&] {
        const auto color = QColorDialog::getColor(background, &dialog, "Portrait background");
        if (color.isValid()) {
            background = color;
            fill.setText(color.name());
            update();
        }
    });
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    update();
    if (dialog.exec() == QDialog::Accepted) {
        if (selected_ != id || selector_ != selector || project_->avatarArtwork()->json() != before)
            throw Error("Portrait selection changed while assignment was open");
        apply(prepared, "Assign portrait image");
    }
}
} // namespace lordly
