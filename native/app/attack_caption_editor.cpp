#include "app/attack_caption_editor.h"
#include "app/content_refresh.h"
#include "app/sprite_editor.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QUndoStack>
#include <QVBoxLayout>
namespace lordly {
namespace {
QVector<QColor> colors() {
    return {Qt::transparent, Qt::black, QColor("#789880"), QColor("#ccffcc")};
}
QImage picture(const QByteArray &pixels) {
    const auto palette = colors();
    QImage image(90, 15, QImage::Format_ARGB32);
    for (int y = 0; y < 15; ++y)
        for (int x = 0; x < 90; ++x)
            image.setPixelColor(x, y, palette.at(u8(pixels, y * 90 + x)));
    return image;
}
class CaptionChange : public QUndoCommand {
    Project *p_;
    QJsonObject before_, after_;

  public:
    CaptionChange(Project *p, const QJsonObject &after, const QString &name)
        : QUndoCommand(name), p_(p), before_(p->attacks()->json()), after_(after) {}
    void undo() override {
        p_->setAttacks(before_);
    }
    void redo() override {
        p_->setAttacks(after_);
    }
};
} // namespace
AttackCaptionEditor::AttackCaptionEditor(Project *p, QUndoStack *undo,
                                         std::function<void()> changed, QWidget *parent)
    : QWidget(parent), project_(p), undo_(undo), changed_(std::move(changed)) {
    setObjectName("attackCaptionEditor");
    project_->loadAttacks();
    auto outer = new QVBoxLayout(this);
    auto split = new QSplitter;
    outer->addWidget(split);
    auto browse = new QWidget;
    auto left = new QVBoxLayout(browse);
    search_ = new QLineEdit;
    search_->setPlaceholderText("Find an attack…");
    left->addWidget(search_);
    list_ = new QListWidget;
    list_->setObjectName("attackCaptionList");
    left->addWidget(list_);
    split->addWidget(browse);
    auto panel = new QWidget;
    auto right = new QVBoxLayout(panel);
    split->addWidget(panel);
    split->setStretchFactor(1, 1);
    auto actions = new QHBoxLayout;
    right->addLayout(actions);
    auto button = [&](const QString &title, const char *name, std::function<void()> call) {
        auto b = new QPushButton(title);
        b->setObjectName(name);
        actions->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, call] { guarded(call); });
    };
    button("Open attack", "captionOpenAttack", [this] { emit openAttack(selected_); });
    button("Generate from attack name", "captionGenerate", [this] {
        auto data = *project_->attacks();
        auto a = data.entry(selected_);
        a.caption = data.generateCaption(a.name);
        data.set(selected_, a);
        publish(data, "Generate attack caption");
    });
    findChild<QPushButton *>("captionGenerate")->setToolTip(
        "Uses lettering from the original battle artwork. Symbols absent from that artwork "
        "are adapted from the ROM font to the battle lettering's height and outline.");
    button("Import PNG…", "captionImport", [this] {
        auto path = QFileDialog::getOpenFileName(this, "Import 90×15 caption", {}, "PNG (*.png)");
        if (!path.isEmpty())
            importImage(QImage(path));
    });
    button("Export PNG…", "captionExport", [this] {
        auto path = QFileDialog::getSaveFileName(
            this, "Export attack caption",
            AttackData::displayName(project_->attacks()->entry(selected_)) + ".png", "PNG (*.png)");
        if (!path.isEmpty())
            exportImage(path);
    });
    button("Restore loaded artwork", "captionRestore", [this] {
        auto data = *project_->attacks();
        data.restoreCaption(selected_);
        publish(data, "Restore attack caption");
    });
    info_ = new QLabel;
    info_->setWordWrap(true);
    right->addWidget(info_);
    auto controls = new QHBoxLayout;
    right->addLayout(controls);
    auto tool = new QComboBox;
    tool->addItems({"Pencil", "Eraser", "Fill", "Eyedropper"});
    controls->addWidget(tool);
    auto color = new QComboBox;
    color->setObjectName("captionColor");
    color->addItems({"Transparent", "Outline", "Shade", "Lettering"});
    color->setCurrentIndex(3);
    controls->addWidget(color);
    auto zoom = new QSpinBox;
    zoom->setRange(1, 20);
    zoom->setValue(8);
    zoom->setSuffix("×");
    controls->addWidget(new QLabel("Zoom"));
    controls->addWidget(zoom);
    auto grid = new QCheckBox("Grid");
    controls->addWidget(grid);
    controls->addStretch();
    auto scroll = new QScrollArea;
    scroll->setAlignment(Qt::AlignCenter);
    right->addWidget(scroll, 1);
    canvas_ = new SpriteCanvas;
    canvas_->setObjectName("attackCaptionCanvas");
    canvas_->setZoom(8);
    canvas_->setGrid(false);
    canvas_->setIndexColor(3);
    scroll->setWidget(canvas_);
    preview_ = new QLabel;
    preview_->setObjectName("attackCaptionPreview");
    preview_->setAlignment(Qt::AlignCenter);
    right->addWidget(preview_);
    connect(tool, &QComboBox::currentIndexChanged, this,
            [this](int n) { canvas_->setTool(SpriteCanvas::Tool(n)); });
    connect(color, &QComboBox::currentIndexChanged, this,
            [this](int n) { canvas_->setIndexColor(quint8(n)); });
    connect(canvas_, &SpriteCanvas::indexPicked, color, &QComboBox::setCurrentIndex);
    connect(zoom, &QSpinBox::valueChanged, canvas_, &SpriteCanvas::setZoom);
    connect(grid, &QCheckBox::toggled, canvas_, &SpriteCanvas::setGrid);
    connect(canvas_, &SpriteCanvas::indicesCommitted, this,
            [this](const QByteArray &pixels, QPoint) {
                guarded([&] {
                    auto data = *project_->attacks();
                    auto a = data.entry(selected_);
                    a.caption = pixels;
                    data.set(selected_, a);
                    publish(data, "Paint attack caption");
                });
            });
    connect(list_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (refreshing_ || row < 0)
            return;
        selected_ = list_->item(row)->data(Qt::UserRole).toInt();
        guarded([&] { showCaption(); });
    });
    connect(search_, &QLineEdit::textChanged, this, [this] {
        for (int i = 0; i < list_->count(); ++i)
            list_->item(i)->setHidden(
                !list_->item(i)->text().contains(search_->text(), Qt::CaseInsensitive));
    });
    watchContentChanges(undo_, this, [this] {
        refresh();
        changed_();
    });
    refresh();
}
void AttackCaptionEditor::guarded(const std::function<void()> &call) {
    try {
        call();
    } catch (const std::exception &e) {
        QMessageBox::warning(this, "Attack names", e.what());
    }
}
void AttackCaptionEditor::publish(const AttackData &data, const QString &name) {
    if (data.json() != project_->attacks()->json())
        undo_->push(new CaptionChange(project_, data.json(), name));
}
void AttackCaptionEditor::refresh() {
    refreshing_ = true;
    list_->clear();
    const auto &data = *project_->attacks();
    selected_ = qBound(1, selected_, int(data.entries().size() - 1));
    for (int i = 1; i < data.entries().size(); ++i) {
        auto item = new QListWidgetItem(
            QString("%1 · %2").arg(i).arg(AttackData::displayName(data.entry(i))), list_);
        item->setData(Qt::UserRole, i);
        item->setHidden(!item->text().contains(search_->text(), Qt::CaseInsensitive));
    }
    list_->setCurrentRow(selected_ - 1);
    refreshing_ = false;
    showCaption();
}
void AttackCaptionEditor::selectAttack(int id) {
    selected_ = id;
    search_->clear();
    refresh();
}
void AttackCaptionEditor::showCaption() {
    const auto &data = *project_->attacks();
    const auto &a = data.entry(selected_);
    const auto pixels = data.captionPixels(selected_);
    auto canvasPalette = colors();
    canvasPalette.resize(256); // Shared indexed canvas uses a byte-sized lookup table.
    canvas_->setIndexedAsset(QString("attack-caption-%1").arg(selected_), {90, 15}, pixels,
                             canvasPalette, 0);
    preview_->setPixmap(QPixmap::fromImage(picture(pixels)).scaled(360, 60));
    info_->setText(AttackData::isTemplate(a.behavior)
                       ? "Spell arrays display the resolved spell's caption. Edit that spell's "
                         "artwork to change its battle name."
                       : QString("90 × 15 pixels · Four native shades · %1. Preview tint is green; battle tint follows "
                                 "the attack's element. PNG imports must be 90 × 15; opaque colors "
                                 "are reduced to the three native shades.")
                             .arg(a.caption.isEmpty() ? "Loaded artwork" : "Independent artwork"));
    if (u8(a.data,0)<2)
        info_->setText(info_->text()+" With battle animations enabled, this physical attack does not display a name caption. Its artwork appears in the action log when animations are disabled.");
}
void AttackCaptionEditor::importImage(const QImage &image) {
    if (image.isNull() || image.size() != QSize(90, 15))
        throw Error("Import a 90 × 15 PNG");
    auto data = *project_->attacks();
    auto a = data.entry(selected_);
    a.caption = QByteArray(1350, 0);
    const auto palette = colors();
    for (int y = 0; y < 15; ++y)
        for (int x = 0; x < 90; ++x) {
            const auto c = image.pixelColor(x, y);
            int best = 0, score = INT_MAX;
            if (c.alpha() >= 128)
                for (int i = 1; i < 4; ++i) {
                    const int d = qAbs(c.red() - palette[i].red()) +
                                  qAbs(c.green() - palette[i].green()) +
                                  qAbs(c.blue() - palette[i].blue());
                    if (d < score) {
                        score = d;
                        best = i;
                    }
                }
            a.caption[y * 90 + x] = char(best);
        }
    data.set(selected_, a);
    publish(data, "Import attack caption");
}
void AttackCaptionEditor::exportImage(const QString &path) const {
    if (!picture(project_->attacks()->captionPixels(selected_)).save(path, "PNG"))
        throw Error("Could not save the caption PNG");
}
} // namespace lordly
