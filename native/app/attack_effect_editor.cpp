#include "app/attack_effect_editor.h"
#include "app/recovery_draft.h"
#include "app/sprite_editor.h"
#include "engine/animation_file_import.h"
#include "engine/effect_player.h"
#include "engine/image.h"
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <algorithm>
namespace lordly {
namespace {
QLabel *note(const QString &text, QLayout *layout) {
    auto l = new QLabel(text);
    l->setWordWrap(true);
    layout->addWidget(l);
    return l;
}
QSpinBox *spin(const char *name, int lo, int hi) {
    auto s = new QSpinBox;
    s->setObjectName(name);
    s->setRange(lo, hi);
    s->setKeyboardTracking(false);
    return s;
}
QMap<int, QString> effectNames(const Rom &rom) {
    QMap<int, QString> names;
    const auto data = AttackData::loadRetail(rom);
    for (int i = 0; i <= 132; ++i)
        names[i] = QString("Effect ID %1").arg(i);
    for (int i = 1; i < 159; ++i) {
        const auto &a = data.entry(i);
        if (u8(a.data, 0) < 2 || AttackData::isTemplate(i))
            continue;
        names[EffectPlayer::retailEffect(i)] = AttackData::displayName(a);
    }
    return names;
}
QImage checker(QImage image) {
    QImage composed(image.size(), QImage::Format_RGBA8888);
    QPainter p(&composed);
    for (int y = 0; y < image.height(); y += 8)
        for (int x = 0; x < image.width(); x += 8)
            p.fillRect(x, y, 8, 8, ((x / 8 + y / 8) & 1) ? QColor("#303641") : QColor("#242932"));
    p.drawImage(0, 0, image);
    return composed;
}
struct Clip {
    QVector<QImage> images;
    QVector<int> holds;
    int ticks = 0;
    QString error;
};
} // namespace
AttackEffectEditor::AttackEffectEditor(Project *p, int attack, int classId, QWidget *parent)
    : QDialog(parent), project_(p), attack_(attack), classId_(classId) {
    setObjectName("attackEffectEditor");
    setWindowTitle("Edit attack effect");
    resize(1050, 760);
    const auto &a = p->attacks()->entry(attack);
    visual_ = a.visual;
    history_ << visual_;
    inherited_ = u8(a.data, 0) < 2 ? -1 : EffectPlayer::retailEffect(a.animation);
    auto root = new QVBoxLayout(this);
    note(AttackData::displayName(a) + " — changes apply to every class using this attack. Other "
                                      "attack IDs keep their effects.",
         root);
    auto commands = new QHBoxLayout;
    root->addLayout(commands);
    auto replace = new QPushButton("Replace effect…");
    replace->setObjectName("replaceAttackEffect");
    auto menu = new QMenu(replace);
    menu->addAction("From ROM effects…", this, [this] { guarded([this] { chooseRom(); }); });
    menu->addAction("From Art Library…", this, [this] { guarded([this] { chooseLibrary(); }); });
    menu->addAction("From files…", this, [this] {
        guarded([this] {
            const auto paths = QFileDialog::getOpenFileNames(
                this, "Import effect frames", {}, "Animation frames (*.png *.zip *.json)");
            if (!paths.isEmpty())
                importFiles(paths);
        });
    });
    replace->setMenu(menu);
    commands->addWidget(replace);
    auto restore = new QPushButton("Restore original effect");
    restore->setObjectName("restoreAttackEffect");
    commands->addWidget(restore);
    connect(restore, &QPushButton::clicked, this, [this] { record({}); });
    commands->addStretch();
    undo_ = new QPushButton("Undo");
    redo_ = new QPushButton("Redo");
    undo_->setObjectName("effectUndo");
    redo_->setObjectName("effectRedo");
    commands->addWidget(undo_);
    commands->addWidget(redo_);
    connect(undo_, &QPushButton::clicked, this, [this] { history(-1); });
    connect(redo_, &QPushButton::clicked, this, [this] { history(1); });
    source_ = note({}, root);
    source_->setObjectName("effectSource");
    note("119 independent edited effects are supported. Imported sequences: up to 300 frames, 256 "
         "× 256 pixels, and 30 seconds; the export also checks available game memory.",
         root);
    auto split = new QSplitter;
    root->addWidget(split, 1);
    auto left = new QWidget;
    auto scene = new QVBoxLayout(left);
    split->addWidget(left);
    preview_ = new QLabel;
    preview_->setObjectName("attackEffectPreview");
    preview_->setAlignment(Qt::AlignCenter);
    preview_->setMinimumSize(400, 300);
    preview_->setStyleSheet("background:#242932;color:#f5e8cc;");
    scene->addWidget(preview_, 1);
    auto playback = new QHBoxLayout;
    scene->addLayout(playback);
    play_ = new QPushButton("Play");
    play_->setObjectName("effectPlay");
    auto restart = new QPushButton("Restart");
    playback->addWidget(play_);
    playback->addWidget(restart);
    playback->addStretch();
    connect(play_, &QPushButton::clicked, this, [this] {
        if (playing_)
            pausedMs_ += clock_.elapsed();
        else
            clock_.restart();
        playing_ = !playing_;
        play_->setText(playing_ ? "Pause" : "Play");
    });
    connect(restart, &QPushButton::clicked, this, [this] {
        pausedMs_ = 0;
        clock_.restart();
        showFrame();
    });
    note("Neutral five-unit squads and a preview background. Character motion and sound are "
         "omitted. Desktop filtering and blending approximate the game.",
         scene);
    auto right = new QWidget;
    auto options = new QVBoxLayout(right);
    split->addWidget(right);
    split->setSizes({570, 400});
    auto timing = new QFormLayout;
    options->addLayout(timing);
    delay_ = spin("effectDelay", 0, 300);
    delay_->setSuffix(" ticks");
    timing->addRow("Delay before effect", delay_);
    note("30 ticks = 1 second. This delays the visual effect; it does not change damage, healing, "
         "targets or attack counts.",
         options);
    connect(delay_, &QSpinBox::valueChanged, this, [this](int n) {
        if (!refreshing_) {
            auto v = visual_;
            v.delay = n;
            record(v);
        }
    });
    frameControls_ = new QWidget;
    auto frameLayout = new QVBoxLayout(frameControls_);
    frameLayout->setContentsMargins(0, 0, 0, 0);
    options->addWidget(frameControls_, 1);
    note("Imported frames appear over the centre of the battle. The origin places the image "
         "relative to that centre.",
         frameLayout);
    frames_ = new QListWidget;
    frames_->setObjectName("effectFrames");
    frames_->setIconSize({48, 48});
    frameLayout->addWidget(frames_, 1);
    auto form = new QFormLayout;
    frameLayout->addLayout(form);
    ticks_ = spin("effectFrameTicks", 1, 255);
    originX_ = spin("effectOriginX", -1024, 1024);
    originY_ = spin("effectOriginY", -1024, 1024);
    form->addRow("Selected frame duration", ticks_);
    form->addRow("Origin X", originX_);
    form->addRow("Origin Y", originY_);
    connect(frames_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (!visual_.frames || row < 0)
            return;
        QScopedValueRollback guard(refreshing_, true);
        ticks_->setValue(visual_.frames->frames[row].ticks);
        const auto original = visual_.frames->composite(row);
        const auto decoded = decodeN64Image(AttackVisual::gameImage(original));
        const auto game =
            QImage(reinterpret_cast<const uchar *>(decoded.rgba.constData()), decoded.width,
                   decoded.height, decoded.width * 4, QImage::Format_RGBA8888)
                .copy();
        QImage comparison(320, 130, QImage::Format_RGBA8888);
        comparison.fill(Qt::transparent);
        QPainter painter(&comparison);
        painter.setPen(palette().color(QPalette::WindowText));
        painter.drawText(0, 0, 155, 22, Qt::AlignCenter, "Original");
        painter.drawText(165, 0, 155, 22, Qt::AlignCenter, "Game result");
        painter.drawImage(QPoint(0, 26), checker(original).scaled(150, 100, Qt::KeepAspectRatio,
                                                                  Qt::FastTransformation));
        painter.drawImage(QPoint(165, 26), checker(game).scaled(150, 100, Qt::KeepAspectRatio,
                                                                Qt::FastTransformation));
        painter.end();
        comparison_->setPixmap(QPixmap::fromImage(comparison));
    });
    connect(ticks_, &QSpinBox::valueChanged, this, [this](int n) {
        if (!refreshing_ && visual_.frames && frames_->currentRow() >= 0) {
            auto v = visual_;
            v.frames->frames[frames_->currentRow()].ticks = n;
            record(v);
        }
    });
    for (auto s : {originX_, originY_})
        connect(s, &QSpinBox::valueChanged, this, [this] {
            if (!refreshing_ && visual_.frames) {
                auto v = visual_;
                v.frames->anchor = {originX_->value(), originY_->value()};
                record(v);
            }
        });
    auto editRow = new QHBoxLayout;
    frameLayout->addLayout(editRow);
    paint_ = new QPushButton("Paint frame…");
    resize_ = new QPushButton("Resize frames…");
    paint_->setObjectName("effectPaintFrame");
    editRow->addWidget(paint_);
    editRow->addWidget(resize_);
    connect(paint_, &QPushButton::clicked, this, [this] { guarded([this] { paintFrame(); }); });
    connect(resize_, &QPushButton::clicked, this, [this] { guarded([this] { resizeFrames(); }); });
    comparison_ = note({}, frameLayout);
    note("Imports convert to the game's 5-bit RGB channels and on/off transparency. Tagged images "
         "convert to sRGB first; untagged images assume sRGB. Original artwork is retained for "
         "further editing.",
         frameLayout);
    notice_ = note({}, root);
    notice_->setObjectName("effectNotice");
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->setObjectName("effectButtons");
    buttons->button(QDialogButtonBox::Ok)->setText("Apply to attack");
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        guarded([this] {
            visual_.validate();
            accept();
        });
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    timer_ = new QTimer(this);
    timer_->setInterval(33);
    connect(timer_, &QTimer::timeout, this, [this] {
        if (playing_)
            showFrame();
    });
    timer_->start();
    new RecoveryDraft(this, "Unconfirmed attack effect edit", [this] {
        auto data = *project_->attacks();
        auto a = data.entry(attack_);
        a.visual = visual_;
        data.set(attack_, a);
        return QJsonObject{{"attacks", data.json()}};
    });
    refresh();
    render();
}
AttackEffectEditor::~AttackEffectEditor() {
    if (canceled_)
        canceled_->store(true);
}
void AttackEffectEditor::guarded(const std::function<void()> &fn) {
    notice_->clear();
    try {
        fn();
    } catch (const std::exception &e) {
        notice_->setText(QString::fromUtf8(e.what()));
    }
}
void AttackEffectEditor::setVisual(const AttackVisual &v) {
    record(v);
}
void AttackEffectEditor::record(const AttackVisual &v) {
    notice_->clear();
    try {
        v.validate();
        if (v == visual_)
            return;
        auto candidate = *project_->attacks();
        auto attack = candidate.entry(attack_);
        attack.visual = v;
        candidate.set(attack_, attack); // Check capacity before accepting an unfinished draft.
        visual_ = v;
        history_.resize(historyIndex_ + 1);
        history_ << v;
        ++historyIndex_;
        refresh();
        render();
    } catch (const std::exception &e) {
        refresh();
        notice_->setText(QString::fromUtf8(e.what()));
    }
}
void AttackEffectEditor::history(int n) {
    const int at = historyIndex_ + n;
    if (at < 0 || at >= history_.size())
        return;
    historyIndex_ = at;
    visual_ = history_[at];
    refresh();
    render();
}
void AttackEffectEditor::refresh() {
    QScopedValueRollback guard(refreshing_, true);
    delay_->setValue(visual_.delay);
    delay_->setEnabled(inherited_ >= 0 || visual_.nativeEffect >= 0 || visual_.frames.has_value());
    const int selected = frames_->currentRow();
    frames_->clear();
    frameControls_->setVisible(visual_.frames.has_value());
    if (visual_.frames) {
        const auto &asset = *visual_.frames;
        source_->setText(QString("Independent imported sequence · %1 frames · %2 × %3")
                             .arg(asset.frames.size())
                             .arg(asset.size.width())
                             .arg(asset.size.height()));
        originX_->setValue(asset.anchor.x());
        originY_->setValue(asset.anchor.y());
        for (int i = 0; i < asset.frames.size(); ++i) {
            auto item = new QListWidgetItem(
                QIcon(QPixmap::fromImage(
                    checker(asset.composite(i))
                        .scaled(48, 48, Qt::KeepAspectRatio, Qt::FastTransformation))),
                QString("Frame %1 · %2 ticks").arg(i + 1).arg(asset.frames[i].ticks));
            frames_->addItem(item);
        }
        frames_->setCurrentRow(std::clamp(selected, 0, int(asset.frames.size()) - 1));
    } else {
        const int id = visual_.nativeEffect < 0 ? inherited_ : visual_.nativeEffect;
        source_->setText(id < 0 ? "Body animation only — choose a ROM effect or import frames to "
                                  "add a visual effect."
                                : QString("ROM effect: %1 · ID %2%3")
                                      .arg(effectNames(project_->rom()).value(id))
                                      .arg(id)
                                      .arg(visual_.modified() ? " · independent settings" : ""));
    }
    undo_->setEnabled(historyIndex_ > 0);
    redo_->setEnabled(historyIndex_ + 1 < history_.size());
}
void AttackEffectEditor::render() {
    if (canceled_)
        canceled_->store(true);
    canceled_ = std::make_shared<std::atomic_bool>(false);
    const auto cancel = canceled_;
    const int generation = ++generation_;
    clip_.clear();
    holds_.clear();
    clipTicks_ = 0;
    pausedMs_ = 0;
    playing_ = false;
    play_->setText("Play");
    preview_->setText("Preparing effect preview…");
    if (inherited_ < 0 && visual_.nativeEffect < 0 && !visual_.frames) {
        preview_->setText(
            "This attack uses its class animation.\nChoose Replace effect to add a visual effect.");
        return;
    }
    auto watcher = new QFutureWatcher<Clip>(this);
    const auto rom = project_->rom();
    const auto visual = visual_;
    const int inherited = std::max(0, inherited_), classId = classId_;
    connect(watcher, &QFutureWatcher<Clip>::finished, this, [this, watcher, generation] {
        const auto result = watcher->result();
        watcher->deleteLater();
        if (generation != generation_)
            return;
        if (!result.error.isEmpty()) {
            preview_->setText(result.error);
            notice_->setText(
                "The preview could not finish. Your existing effect is preserved until Apply.");
            return;
        }
        clip_ = result.images;
        holds_ = result.holds;
        clipTicks_ = result.ticks;
        playing_ = true;
        play_->setText("Pause");
        clock_.restart();
        showFrame();
    });
    watcher->setFuture(QtConcurrent::run([rom, visual, inherited, classId, cancel] {
        Clip clip;
        try {
            EffectPlayer player(rom);
            EffectPlayer::Scene scene;
            scene.sourceClass = classId;
            player.startVisual(visual, inherited, scene);
            quint64 pixels = 0;
            while (player.advance()) {
                if (cancel->load())
                    return Clip{};
                auto image = player.draw({320, 240}, .75).image;
                if (!clip.images.isEmpty() && clip.images.last() == image)
                    ++clip.holds.last();
                else {
                    pixels += 320 * 240;
                    if (pixels > 64 * 1024 * 1024)
                        throw Error("Effect preview exceeds its image budget");
                    clip.images << image;
                    clip.holds << 1;
                }
                ++clip.ticks;
            }
        } catch (const std::exception &e) {
            clip.error = QString::fromUtf8(e.what());
        }
        return clip;
    }));
}
void AttackEffectEditor::showFrame() {
    if (clip_.isEmpty() || clipTicks_ <= 0)
        return;
    int tick = int((pausedMs_ + (playing_ ? clock_.elapsed() : 0)) * 30 / 1000) % clipTicks_,
        index = 0;
    while (index + 1 < holds_.size() && tick >= holds_[index])
        tick -= holds_[index++];
    preview_->setPixmap(QPixmap::fromImage(checker(clip_[index]))
                            .scaled(preview_->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
}
void AttackEffectEditor::chooseRom() {
    QDialog dialog(this);
    dialog.setWindowTitle("Choose effect from this ROM");
    dialog.resize(460, 620);
    auto layout = new QVBoxLayout(&dialog);
    auto search = new QLineEdit;
    search->setPlaceholderText("Search effects or IDs…");
    layout->addWidget(search);
    auto list = new QListWidget;
    layout->addWidget(list, 1);
    const auto names = effectNames(project_->rom());
    for (auto i = names.begin(); i != names.end(); ++i) {
        auto item = new QListWidgetItem(QString("%1 · ID %2").arg(i.value()).arg(i.key()));
        item->setData(Qt::UserRole, i.key());
        list->addItem(item);
    }
    list->setCurrentRow(visual_.nativeEffect < 0 ? std::max(0, inherited_) : visual_.nativeEffect);
    connect(search, &QLineEdit::textChanged, &dialog, [list](const QString &query) {
        for (int i = 0; i < list->count(); ++i)
            list->item(i)->setHidden(!list->item(i)->text().contains(query, Qt::CaseInsensitive));
    });
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(list, &QListWidget::itemDoubleClicked, &dialog, &QDialog::accept);
    if (dialog.exec() == QDialog::Accepted && list->currentItem()) {
        auto v = visual_;
        v.frames.reset();
        v.nativeEffect = list->currentItem()->data(Qt::UserRole).toInt();
        record(v);
    }
}
void AttackEffectEditor::chooseLibrary() {
    QDialog dialog(this);
    dialog.setWindowTitle("Choose from Art Library");
    dialog.resize(460, 560);
    auto layout = new QVBoxLayout(&dialog);
    auto list = new QListWidget;
    layout->addWidget(list, 1);
    for (const auto &asset : project_->sprites().assets()) {
        auto item = new QListWidgetItem(QString("%1 · %2 frames · %3 × %4")
                                            .arg(asset.name)
                                            .arg(asset.frames.size())
                                            .arg(asset.size.width())
                                            .arg(asset.size.height()));
        item->setData(Qt::UserRole, asset.id);
        list->addItem(item);
    }
    if (!list->count())
        note("The Art Library is empty. Add artwork there or choose From files.", layout);
    else
        list->setCurrentRow(0);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    buttons->button(QDialogButtonBox::Ok)->setEnabled(list->count() > 0);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted && list->currentItem())
        importLibrary(list->currentItem()->data(Qt::UserRole).toString());
}
void AttackEffectEditor::importLibrary(const QString &id) {
    auto v = visual_;
    v.frames = project_->sprites().asset(id);
    v.frames->id = SpriteLibrary::newId();
    v.validate();
    record(v);
}
void AttackEffectEditor::importFiles(const QStringList &paths) {
    auto v = visual_;
    v.frames = importAnimationFiles(paths);
    v.validate();
    record(v);
}
void AttackEffectEditor::resizeFrames() {
    if (!visual_.frames)
        return;
    QDialog dialog(this);
    dialog.setWindowTitle("Resize all effect frames");
    auto form = new QFormLayout(&dialog);
    auto w = spin("effectResizeWidth", 1, 256), h = spin("effectResizeHeight", 1, 256);
    w->setValue(visual_.frames->size.width());
    h->setValue(visual_.frames->size.height());
    form->addRow("Width", w);
    form->addRow("Height", h);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted) {
        auto v = visual_;
        v.frames = v.frames->resized({w->value(), h->value()}, true);
        record(v);
    }
}
void AttackEffectEditor::paintFrame() {
    if (!visual_.frames || frames_->currentRow() < 0)
        return;
    const int row = frames_->currentRow();
    auto image = visual_.frames->composite(row);
    auto asset = SpriteAsset::fromImage("Effect frame", image);
    QDialog dialog(this);
    dialog.setWindowTitle("Paint effect frame");
    dialog.setObjectName("attackEffectFramePaint");
    dialog.resize(780, 650);
    auto layout = new QVBoxLayout(&dialog);
    auto tools = new QHBoxLayout;
    layout->addLayout(tools);
    auto tool = new QComboBox;
    tool->addItems({"Pencil", "Eraser", "Fill", "Pick color"});
    tools->addWidget(tool);
    auto color = new QPushButton("Color…");
    tools->addWidget(color);
    auto zoom = spin("effectPaintZoom", 1, 24);
    zoom->setValue(4);
    tools->addWidget(zoom);
    tools->addStretch();
    auto canvas = new SpriteCanvas;
    canvas->setAsset(&asset, 0, 0);
    canvas->setZoom(4);
    auto scroll = new QScrollArea;
    scroll->setWidget(canvas);
    scroll->setAlignment(Qt::AlignCenter);
    layout->addWidget(scroll, 1);
    QColor ink = Qt::white;
    canvas->setColor(ink);
    connect(tool, &QComboBox::currentIndexChanged, canvas,
            [canvas](int i) { canvas->setTool(SpriteCanvas::Tool(i)); });
    connect(zoom, &QSpinBox::valueChanged, canvas, &SpriteCanvas::setZoom);
    connect(color, &QPushButton::clicked, &dialog, [&] {
        const auto c = QColorDialog::getColor(ink, &dialog, "Paint color");
        if (c.isValid()) {
            ink = c;
            canvas->setColor(c);
        }
    });
    connect(canvas, &SpriteCanvas::colorPicked, &dialog, [&](QColor c) {
        ink = c;
        canvas->setColor(c);
    });
    connect(canvas, &SpriteCanvas::committed, &dialog, [&](const QImage &pixels, QPoint) {
        asset.frames[0].layers[0].image = pixels;
        canvas->setAsset(&asset, 0, 0);
    });
    note("This paints a flattened copy of the selected frame. Other frames and library artwork are "
         "unchanged.",
         layout);
    RecoveryDraft draft(&dialog, "Unconfirmed effect frame painting", [&] {
        auto v = visual_;
        v.frames->frames[row].layers = asset.frames[0].layers;
        auto data = *project_->attacks();
        auto attack = data.entry(attack_);
        attack.visual = v;
        data.set(attack_, attack);
        return QJsonObject{{"attacks", data.json()}};
    });
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted) {
        auto v = visual_;
        v.frames->frames[row].layers = asset.frames[0].layers;
        record(v);
    }
}
} // namespace lordly
