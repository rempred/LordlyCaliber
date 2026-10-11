#include "app/category_icon_editor.h"
#include "app/art_image_assignment.h"
#include "app/sprite_editor.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
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
namespace lordly {
namespace {
class CategoryCommand final : public QUndoCommand {
  public:
    CategoryCommand(Project *project, const QString &label, const QJsonObject &after)
        : QUndoCommand(label), project_(project), before_(project->categoryIcons()->json()),
          after_(after) {}
    void undo() override {
        project_->setCategoryIcons(before_);
    }
    void redo() override {
        project_->setCategoryIcons(after_);
    }

  private:
    Project *project_;
    QJsonObject before_, after_;
};
} // namespace
CategoryIconEditor::CategoryIconEditor(Project *project, QUndoStack *undo,
                                       std::function<void()> changed, QWidget *parent)
    : QWidget(parent), project_(project), undo_(undo), changed_(std::move(changed)) {
    setObjectName("categoryIconEditor");
    auto outer = new QVBoxLayout(this);
    auto title = new QLabel("<b>Class category icons</b> · Create, paint and assign the tabs at "
                            "the top of List Classes");
    title->setWordWrap(true);
    outer->addWidget(title);
    notice_ = new QLabel;
    notice_->setObjectName("categoryIconNotice");
    notice_->setWordWrap(true);
    try {
        project_->loadCategoryIcons();
    } catch (const std::exception &e) {
        notice_->setText(QString::fromUtf8(e.what()));
        outer->addWidget(notice_);
        return;
    }
    auto actions = new QHBoxLayout;
    outer->addLayout(actions);
    auto action = [&](const QString &text, const char *id, std::function<void()> run) {
        auto b = new QPushButton(text);
        b->setObjectName(id);
        actions->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, run] { guarded(run); });
    };
    action("New icon…", "categoryIconNew", [this] { createIcon(); });
    action("Copy icon…", "categoryIconCopy", [this] { createIcon(0, true); });
    action("Replace image…", "categoryIconImport", [this] {
        const auto path = QFileDialog::getOpenFileName(this, "Replace class category icon", {},
                                                       "Images (*.png *.jpg *.jpeg)");
        if (!path.isEmpty())
            prepareImage(loadArtworkImage(path));
    });
    action("Export PNG…", "categoryIconExport", [this] {
        const auto path = QFileDialog::getSaveFileName(
            this, "Export category icon", project_->categoryIcons()->label(selected_) + ".png",
            "PNG (*.png)");
        if (!path.isEmpty() && !project_->categoryIcons()->image(selected_).save(path))
            throw Error("Could not save the icon image");
    });
    action("Restore loaded icon", "categoryIconRestore", [this] {
        auto edited = *project_->categoryIcons();
        edited.restore(selected_);
        commit("Restore class category icon", edited);
    });
    actions->addStretch();
    auto split = new QSplitter;
    outer->addWidget(split, 1);
    icons_ = new QListWidget;
    icons_->setObjectName("categoryIcons");
    icons_->setMinimumWidth(150);
    split->addWidget(icons_);
    auto work = new QWidget;
    auto layout = new QVBoxLayout(work);
    layout->setContentsMargins(0, 0, 0, 0);
    split->addWidget(work);
    auto controls = new QHBoxLayout;
    layout->addLayout(controls);
    auto tool = new QComboBox;
    tool->setObjectName("categoryIconTool");
    tool->addItems({"Pencil", "Clear to background", "Fill", "Eyedropper", "Selection", "Move"});
    controls->addWidget(tool);
    controls->addWidget(new QLabel("Zoom"));
    auto zoom = new QSpinBox;
    zoom->setRange(1, 32);
    zoom->setValue(20);
    zoom->setSuffix("×");
    controls->addWidget(zoom);
    auto grid = new QCheckBox("Grid");
    grid->setChecked(true);
    controls->addWidget(grid);
    auto clear = new QPushButton("Deselect");
    controls->addWidget(clear);
    controls->addStretch();
    auto views = new QHBoxLayout;
    layout->addLayout(views, 1);
    auto scroll = new QScrollArea;
    scroll->setAlignment(Qt::AlignCenter);
    canvas_ = new SpriteCanvas;
    canvas_->setObjectName("categoryIconCanvas");
    canvas_->setZoom(20);
    scroll->setWidget(canvas_);
    views->addWidget(scroll, 1);
    auto colors = new QTableWidget(16, 16);
    colors->setObjectName("categoryIconPalette");
    colors->horizontalHeader()->hide();
    colors->verticalHeader()->hide();
    colors->setEditTriggers(QAbstractItemView::NoEditTriggers);
    colors->setSelectionMode(QAbstractItemView::SingleSelection);
    for (auto header : {colors->horizontalHeader(), colors->verticalHeader()}) {
        header->setMinimumSectionSize(16);
        header->setDefaultSectionSize(16);
        header->setSectionResizeMode(QHeaderView::Fixed);
    }
    colors->setFixedSize(258, 258);
    views->addWidget(colors, 0, Qt::AlignTop);
    for (int i = 0; i < 256; ++i) {
        auto item = new QTableWidgetItem;
        const auto c = project_->categoryIcons()->palette()[i];
        item->setBackground(c);
        item->setToolTip(QString("Color %1 · RGB %2, %3, %4")
                             .arg(i)
                             .arg(c.red())
                             .arg(c.green())
                             .arg(c.blue()));
        colors->setItem(i / 16, i % 16, item);
    }
    auto assignment = new QHBoxLayout;
    layout->addLayout(assignment);
    assignment->addWidget(new QLabel("Family"));
    family_ = new QComboBox;
    family_->setObjectName("iconAssignFamily");
    assignment->addWidget(family_, 1);
    auto assign = new QPushButton("Assign this icon");
    assign->setObjectName("categoryIconAssign");
    assignment->addWidget(assign);
    connect(assign, &QPushButton::clicked, this, [this] {
        guarded([this] {
            if (family_->currentIndex() < 0)
                throw Error("Create a class family first");
            auto edited = *project_->categoryIcons();
            edited.assignFamily(family_->currentData().toInt(), selected_);
            commit("Assign family tab icon", edited);
        });
    });
    usage_ = new QLabel;
    usage_->setObjectName("categoryIconUsage");
    usage_->setWordWrap(true);
    layout->addWidget(usage_);
    auto note = new QLabel(
        "14 × 14 pixels · Uses the existing menu colors. Replacements affect this category "
        "everywhere in Army management. The game highlights the selected tab automatically.");
    note->setWordWrap(true);
    layout->addWidget(note);
    connect(colors, &QTableWidget::cellClicked, this,
            [this](int row, int col) { canvas_->setIndexColor(quint8(row * 16 + col)); });
    connect(canvas_, &SpriteCanvas::indexPicked, this, [this, colors](int i) {
        canvas_->setIndexColor(quint8(i));
        colors->setCurrentCell(i / 16, i % 16);
    });
    connect(tool, &QComboBox::currentIndexChanged, this,
            [this](int i) { canvas_->setTool(SpriteCanvas::Tool(i)); });
    connect(zoom, &QSpinBox::valueChanged, canvas_, &SpriteCanvas::setZoom);
    connect(grid, &QCheckBox::toggled, canvas_, &SpriteCanvas::setGrid);
    connect(clear, &QPushButton::clicked, canvas_, &SpriteCanvas::clearSelection);
    connect(canvas_, &SpriteCanvas::indicesCommitted, this,
            [this](const QByteArray &pixels, QPoint position) {
                guarded([&] {
                    QImage blank(14, 14, QImage::Format_RGBA8888);
                    blank.fill(Qt::white);
                    auto shifted = project_->categoryIcons()->mapImage(blank);
                    for (int y = 0; y < 14; ++y)
                        for (int x = 0; x < 14; ++x) {
                            const auto point = QPoint(x, y) + position;
                            if (QRect(0, 0, 14, 14).contains(point))
                                shifted[point.y() * 14 + point.x()] = pixels[y * 14 + x];
                        }
                    auto edited = *project_->categoryIcons();
                    edited.setIndices(selected_, shifted);
                    commit("Paint class category icon", edited);
                });
            });
    connect(icons_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0) {
            selected_ = icons_->item(row)->data(Qt::UserRole).toInt();
            refresh();
        }
    });
    connect(undo_, &QUndoStack::indexChanged, this, [this] {
        refresh();
        changed_();
    });
    split->setSizes({190, 850});
    split->setStretchFactor(1, 1);
    outer->addWidget(notice_);
    refresh();
}
void CategoryIconEditor::guarded(const std::function<void()> &run) {
    try {
        notice_->clear();
        run();
    } catch (const std::exception &e) {
        refresh();
        notice_->setText(QString::fromUtf8(e.what()));
    }
}
void CategoryIconEditor::refresh() {
    if (!canvas_ || !project_->categoryIcons())
        return;
    QSignalBlocker block(icons_);
    icons_->clear();
    try {
        project_->loadPromotion();
    } catch (const std::exception &) {
    }
    if (!project_->categoryIcons()->iconIds().contains(selected_))
        selected_ = 1;
    const auto priorFamily = family_->currentData();
    family_->clear();
    QStringList assigned;
    if (project_->promotion())
        for (const auto &v : project_->promotion()->families["families"].toArray()) {
            const auto f = v.toObject();
            const int id = f["id"].toInt();
            if (!id)
                continue;
            family_->addItem(f["name"].toString(), id);
            if (project_->categoryIcons()->familyIcon(id) == selected_)
                assigned << f["name"].toString();
        }
    if (family_->findData(priorFamily) >= 0)
        family_->setCurrentIndex(family_->findData(priorFamily));
    usage_->setText(assigned.isEmpty()
                        ? "No family assignment. Choose a family above to give it this tab."
                        : "Used by: " + assigned.join(", "));
    for (int i : project_->categoryIcons()->iconIds()) {
        auto item = new QListWidgetItem(
            QString("%1 · %2%3")
                .arg(i)
                .arg(project_->categoryIcons()->label(i))
                .arg(project_->categoryIcons()->json()["icons"].toObject().contains(
                         QString::number(i))
                         ? " *"
                         : ""),
            icons_);
        QStringList users;
        if (project_->promotion())
            for (const auto &v : project_->promotion()->families["families"].toArray()) {
                const auto f = v.toObject();
                if (project_->categoryIcons()->familyIcon(f["id"].toInt()) == i)
                    users << f["name"].toString();
            }
        item->setToolTip(users.isEmpty() ? "Not assigned to a family"
                                         : "Used by: " + users.join(", "));
        item->setData(Qt::UserRole, i);
        if (i == selected_)
            icons_->setCurrentItem(item);
        item->setIcon(QPixmap::fromImage(project_->categoryIcons()->image(i).scaled(
            42, 42, Qt::IgnoreAspectRatio, Qt::FastTransformation)));
    }
    icons_->setIconSize({42, 42});
    QImage blank(14, 14, QImage::Format_RGBA8888);
    blank.fill(Qt::white);
    const auto background = quint8(project_->categoryIcons()->mapImage(blank)[0]);
    canvas_->setIndexedAsset(QString("category-%1").arg(selected_), {14, 14},
                             project_->categoryIcons()->indices(selected_),
                             project_->categoryIcons()->palette(), background);
}
void CategoryIconEditor::selectFamily(int id) {
    refresh();
    family_->setCurrentIndex(family_->findData(id));
}
void CategoryIconEditor::selectCategory(int id) {
    project_->categoryIcons()->label(id);
    selected_ = id;
    refresh();
}
void CategoryIconEditor::commit(const QString &label, const CategoryIcons &value) {
    if (value.json() != project_->categoryIcons()->json())
        undo_->push(new CategoryCommand(project_, label, value.json()));
}
void CategoryIconEditor::assignImage(const QImage &image) {
    auto edited = *project_->categoryIcons();
    edited.setIndices(selected_, edited.mapImage(image));
    commit("Replace class category icon", edited);
}
void CategoryIconEditor::createIcon(int family, bool copy) {
    guarded([&] {
        bool ok = false;
        const auto name = QInputDialog::getText(
            this, copy ? "Copy icon" : "Create icon", "Icon name", QLineEdit::Normal,
            copy ? project_->categoryIcons()->label(selected_) + " copy" : "New family icon", &ok);
        if (!ok)
            return;
        auto edited = *project_->categoryIcons();
        const auto id = edited.create(name, copy ? selected_ : 0);
        if (family)
            edited.assignFamily(family, id);
        selected_ = id;
        commit(family ? "Create and assign family icon" : "Create icon", edited);
        refresh();
    });
}
void CategoryIconEditor::prepareImage(const QImage &image) {
    const ArtImageAssignment config{{14, 14},
                                    "categoryIcon",
                                    "Replace category icon",
                                    "Fit the image into the 14 × 14 menu tab. Colors are "
                                    "matched to the current game palette.",
                                    true};
    const auto result =
        prepareArtworkImage(this, image, config, [this](const QImage &image, int, bool) {
            const auto data = project_->categoryIcons()->mapImage(image);
            return ArtImageMapping{data, project_->categoryIcons()->image(data)};
        });
    if (result) {
        auto edited = *project_->categoryIcons();
        edited.setIndices(selected_, *result);
        commit("Replace class category icon", edited);
    }
}
} // namespace lordly
