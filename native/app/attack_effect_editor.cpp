#include "app/attack_effect_editor.h"
#include <QCheckBox>
#include "app/effect_artwork.h"
#include "app/effect_composer.h"
#include "app/recovery_draft.h"
#include "app/sprite_editor.h"
#include "engine/animation_file_import.h"
#include "engine/effect_player.h"
#include "engine/image.h"
#include "engine/image_colors.h"
#include "engine/effect_resources.h"
#include <QColorDialog>
#include <QButtonGroup>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QImageReader>
#include <QInputDialog>
#include <QGroupBox>
#include <QTabWidget>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QSaveFile>
#include <QFileInfo>
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
    menu->addAction("Copy from another attack…", this, [this] { guarded([this] { chooseAttack(); }); });
    compose_=new QPushButton("Create effect…");compose_->setObjectName("composeAttackEffect");commands->addWidget(compose_);
    connect(compose_,&QPushButton::clicked,this,[this]{guarded([this]{
        EffectComposition model;
        const bool existing=visual_.frames && EffectComposition::contains(*visual_.frames);
        if(existing)model=EffectComposition::fromAsset(*visual_.frames);
        EffectComposer dialog(project_,model,this);
        if(!existing && visual_.frames)dialog.addPiece(*visual_.frames);
        if(dialog.exec()==QDialog::Accepted){AttackVisual v;v.delay=visual_.delay;v.frames=dialog.asset();record(v);}
    });});
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
    loop_ = new QCheckBox("Loop");
    loop_->setObjectName("effectLoop");
    step_ = new QPushButton("Step frame");
    step_->setObjectName("effectStep");
    step_->setToolTip("Pause and advance one game tick (1/30 second).");
    playback->addWidget(play_);
    playback->addWidget(step_);
    playback->addWidget(loop_);
    playback->addStretch();
    playback->addWidget(new QLabel("View"));
    auto sides = new QButtonGroup(this);
    for (int side = 0; side < 2; ++side) {
        auto button = new QPushButton(side ? "Enemy" : "Player");
        button->setObjectName(side ? "effectPreviewEnemy" : "effectPreviewPlayer");
        button->setCheckable(true);
        sides->addButton(button, side);
        playback->addWidget(button);
        button->setChecked(!side);
    }
    connect(sides, &QButtonGroup::idClicked, this, [this](int side) {
        if (enemyPreview_ == bool(side)) return;
        enemyPreview_ = bool(side);
        render();
    });
    connect(play_, &QPushButton::clicked, this, [this] {
        if (playing_)
            pausedMs_ += clock_.elapsed();
        else {
            if (pausedMs_ * 30 / 1000 >= clipTicks_ - 1) pausedMs_ = 0;
            clock_.restart();
        }
        playing_ = !playing_;
        play_->setText(playing_ ? "Pause" : "Play");
        showFrame();
    });

    connect(step_, &QPushButton::clicked, this, [this] {
        if (clipTicks_ <= 0) return;
        const int next = loop_->isChecked() ? (currentTick() + 1) % clipTicks_ : qMin(currentTick() + 1, clipTicks_ - 1);
        pausedMs_ = (qint64(next) * 1000 + 29) / 30;
        playing_ = false;
        play_->setText("Play");
        showFrame();
    });
    auto exportRow = new QHBoxLayout;
    scene->addLayout(exportRow);
    framePosition_ = new QLabel;
    framePosition_->setObjectName("effectFramePosition");
    exportRow->addWidget(framePosition_, 1);
    exportFrame_ = new QPushButton("Export…");
    exportFrame_->setObjectName("effectExportFrame");
    exportRow->addWidget(exportFrame_);
    auto exports = new QMenu(exportFrame_);
    exportFrame_->setMenu(exports);
    exports->addAction("Current frame · PNG…", this, [this] {
        if (playing_) {
            pausedMs_ += clock_.elapsed();
            playing_ = false;
            play_->setText("Play");
        }
        showFrame();
        guarded([this] {
            const auto path = QFileDialog::getSaveFileName(this, "Export displayed effect frame", "effect-frame.png", "PNG image (*.png)");
            if (!path.isEmpty()) exportRenderedFrame(path);
        });
    });
    for (const auto format :
         {EffectExport::AnimatedPng, EffectExport::FramesZip, EffectExport::Webm}) {
        const QString title = format == EffectExport::AnimatedPng ? "Full sequence · Animated PNG…"
                              : format == EffectExport::FramesZip ? "Full sequence · PNG frame ZIP…"
                                                                  : "Full sequence · WebM…";
        exports->addAction(title, this, [this, format] {
            guarded([this, format] {
                const QString filter = format == EffectExport::AnimatedPng ? "Animated PNG (*.png)"
                                       : format == EffectExport::FramesZip
                                           ? "PNG frame sequence (*.zip)"
                                           : "WebM video (*.webm)";
                const auto path = QFileDialog::getSaveFileName(this, "Export full effect sequence",
                                                               "effect-sequence", filter);
                if (!path.isEmpty() && exportRenderedSequence(path, format))
                    notice_->setText("Full effect sequence exported at 30 ticks per second.");
            });
        });
    }
    note("Neutral five-unit squads and a preview background. Character motion and sound are "
         "omitted. Desktop filtering and blending approximate the game.",
         scene);
    auto right = new QWidget;
    auto options = new QVBoxLayout(right);
    auto optionsScroll = new QScrollArea;
    optionsScroll->setWidgetResizable(true);
    optionsScroll->setWidget(right);
    optionsScroll->setMinimumWidth(330);
    split->addWidget(optionsScroll);
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
    nativeControls_ = new QGroupBox("Retail effect");
    auto nativeRoot = new QVBoxLayout(nativeControls_);
    auto nativeTabs = new QTabWidget;
    nativeTabs->setObjectName("effectRetailTabs");
    nativeRoot->addWidget(nativeTabs);
    auto artworkPage = new QWidget;
    auto nativeLayout = new QVBoxLayout(artworkPage);
    nativeTabs->addTab(artworkPage, "Artwork");
    auto placementPage = new QWidget;
    auto placementLayout = new QVBoxLayout(placementPage);
    nativeTabs->addTab(placementPage, "Position and movement");
    options->addWidget(nativeControls_, 1);
    nativeForm_ = new QFormLayout;
    placementLayout->addLayout(nativeForm_);
    effectSize_ = spin("effectSize", 25, 400); effectSize_->setSuffix("%");
    effectX_ = spin("effectOffsetX", -320, 320); effectX_->setSuffix(" px");
    effectY_ = spin("effectOffsetY", -240, 240); effectY_->setSuffix(" px");
    nativeForm_->addRow("Size", effectSize_);
    nativeForm_->addRow("Horizontal offset", effectX_);
    nativeForm_->addRow("Vertical offset", effectY_);
    note("Size scales the effect around the screen centre. Positive offsets move it right or down. "
         "These change the visual only; targeting and damage stay the same.", placementLayout);
    auto nativeValue = [this](QSpinBox *control, int AttackVisual::*member) {
        connect(control, &QSpinBox::valueChanged, this, [this, member](int value) {
            if (!refreshing_) { auto v = visual_; v.*member = value; record(v); }
        });
    };
    nativeValue(effectSize_, &AttackVisual::sizePercent);
    nativeValue(effectX_, &AttackVisual::offsetX); nativeValue(effectY_, &AttackVisual::offsetY);
    motionMode_ = new QComboBox;
    motionMode_->setObjectName("effectMotionMode");
    motionMode_->addItems({"Keep position", "Move to position"});
    nativeForm_->addRow("Movement", motionMode_);
    motionControls_ = new QWidget;
    auto motionForm = new QFormLayout(motionControls_);
    motionForm->setContentsMargins(0, 0, 0, 0);
    placementLayout->addWidget(motionControls_);
    motionX_ = spin("effectMotionX", -320, 320); motionX_->setSuffix(" px");
    motionY_ = spin("effectMotionY", -240, 240); motionY_->setSuffix(" px");
    motionStart_ = spin("effectMotionStart", 0, 300); motionStart_->setSuffix(" ticks");
    motionTicks_ = spin("effectMotionTicks", 1, 300); motionTicks_->setSuffix(" ticks");
    motionEasing_ = new QComboBox;
    motionEasing_->setObjectName("effectMotionEasing");
    motionEasing_->addItems({"Constant speed", "Start slowly", "Finish slowly", "Start and finish slowly"});
    motionForm->addRow("End horizontal offset", motionX_);
    motionForm->addRow("End vertical offset", motionY_);
    motionForm->addRow("Start moving after", motionStart_);
    motionForm->addRow("Move over", motionTicks_);
    motionForm->addRow("Motion curve", motionEasing_);
    nativeValue(motionX_, &AttackVisual::motionX); nativeValue(motionY_, &AttackVisual::motionY);
    nativeValue(motionStart_, &AttackVisual::motionStart); nativeValue(motionTicks_, &AttackVisual::motionTicks);
    connect(motionEasing_, &QComboBox::currentIndexChanged, this, [this](int n) {
        if (!refreshing_) { auto v=visual_; v.motionEasing=n; record(v); }
    });
    connect(motionMode_, &QComboBox::currentIndexChanged, this, [this](int n) {
        if (refreshing_) return;
        auto v=visual_;
        v.motionTicks=n ? 30 : 0; v.motionStart=v.motionEasing=0;
        v.motionX=n ? v.offsetX : 0; v.motionY=n ? v.offsetY : 0;
        record(v);
    });
    note("Move the whole effect from its current offsets to the end offsets. Timing starts after "
         "the effect delay; 30 ticks = 1 second. The original animation and its ending stay intact.", placementLayout);
    placementLayout->addStretch();
    note("Artwork used by this effect", nativeLayout);
    artworkList_ = new QListWidget;
    artworkList_->setObjectName("effectRetailArtwork");
    artworkList_->setIconSize({64, 64});
    artworkList_->setWordWrap(true);
    artworkList_->setMinimumHeight(120);
    nativeLayout->addWidget(artworkList_, 1);
    artworkColourForm_ = new QFormLayout;
    nativeLayout->addLayout(artworkColourForm_);
    colourMode_ = new QComboBox;
    colourMode_->setObjectName("effectColourMode");
    colourMode_->addItems({"Original colours", "Shift colours", "Single colour"});
    artworkColourForm_->addRow("Selected artwork", colourMode_);
    hue_ = spin("effectHue", -180, 180);
    hue_->setSuffix("°");
    artworkColourForm_->addRow("Colour shift", hue_);
    effectColour_ = new QPushButton("Choose colour…");
    effectColour_->setObjectName("effectColour");
    artworkColourForm_->addRow("Colour", effectColour_);
    note("Colour changes affect only the highlighted artwork. Each texture keeps its own settings.", nativeLayout);
    connect(colourMode_, &QComboBox::currentIndexChanged, this, [this](int mode) {
        editArtworkColour([mode](EffectArtwork &art) {
            art.hue = mode == 1 ? (art.hue ? art.hue : 120) : 0;
            art.color = mode == 2 ? (art.color >= 0 ? art.color : 0x4080ff) : -1;
        });
    });
    connect(hue_, &QSpinBox::valueChanged, this, [this](int hue) {
        editArtworkColour([hue](EffectArtwork &art) { art.hue = hue; art.color = -1; });
    });
    connect(effectColour_, &QPushButton::clicked, this, [this] {
        const auto item = artworkList_->currentItem();
        if (!item || !item->data(Qt::UserRole).isValid()) return;
        const auto art = visual_.artwork.value(item->data(Qt::UserRole).toUInt());
        const auto colour = QColorDialog::getColor(QColor::fromRgb(art.color < 0 ? 0x4080ff : art.color),
                                                     this, "Selected artwork colour");
        if (colour.isValid()) editArtworkColour([colour](EffectArtwork &a) {
            a.color = colour.rgb() & 0xffffff; a.hue = 0;
        });
    });
    legacyColour_ = new QWidget;
    auto legacyLayout = new QVBoxLayout(legacyColour_);
    legacyLayout->setContentsMargins(0, 0, 0, 0);
    note("This effect also has a whole-effect colour edit saved by an earlier version.", legacyLayout);
    auto clearLegacy = new QPushButton("Remove earlier whole-effect colour");
    clearLegacy->setObjectName("effectClearLegacyColour");
    legacyLayout->addWidget(clearLegacy);
    connect(clearLegacy, &QPushButton::clicked, this, [this] {
        auto v = visual_; v.color = -1; v.hue = 0; record(v);
    });
    nativeLayout->addWidget(legacyColour_);
    auto artworkButtons = new QHBoxLayout;
    nativeLayout->addLayout(artworkButtons);
    replaceArtwork_ = new QPushButton("Replace artwork…");
    replaceArtwork_->setObjectName("effectReplaceArtwork");
    auto artworkMenu = new QMenu(replaceArtwork_);
    artworkMenu->addAction("From file…", this, [this] { guarded([this] { chooseArtworkFile(); }); });
    artworkMenu->addAction("From Art Library…", this, [this] { guarded([this] { chooseArtworkLibrary(); }); });
    replaceArtwork_->setMenu(artworkMenu);
    restoreArtwork_ = new QPushButton("Restore artwork");
    restoreArtwork_->setObjectName("effectRestoreArtwork");
    artworkButtons->addWidget(replaceArtwork_); artworkButtons->addWidget(restoreArtwork_);
    connect(artworkList_, &QListWidget::currentRowChanged, this, [this] { refreshArtworkColour(); });
    connect(restoreArtwork_, &QPushButton::clicked, this, [this] {
        if (const auto item = artworkList_->currentItem()) {
            auto v = visual_; v.artwork.remove(item->data(Qt::UserRole).toUInt()); record(v);
        }
    });
    note("Replacements fit the selected texture's original dimensions and keep the retail movement "
         "and timing. Each texture may appear several times during the effect. Colours and alpha "
         "use the retail effect's RGBA32 format.", nativeLayout);
    compositionHelp_=note("This effect has independent pieces. Use Edit pieces… to change movement, timing, size or fades. "
        "Full colour and partial transparency are retained in the game.", options);
    frameControls_ = new QWidget;
    auto frameLayout = new QVBoxLayout(frameControls_);
    frameLayout->setContentsMargins(0, 0, 0, 0);
    options->addWidget(frameControls_, 1);
    note("Imported frames appear over the centre of the battle. The origin places the image "
         "relative to that centre. They keep the same orientation in Player and Enemy views; "
         "the game does not automatically mirror imported sequences.",
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
        const auto decoded = decodeN64Image(AttackVisual::gameImage(original, visual_.fullColorFrames()));
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
        painter.drawImage(QPoint(0, 26), effectChecker(original).scaled(150, 100, Qt::KeepAspectRatio,
                                                                  Qt::FastTransformation));
        painter.drawImage(QPoint(165, 26), effectChecker(game).scaled(150, 100, Qt::KeepAspectRatio,
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
        validateEffectArtwork(project_->rom(), v);
        if (v == visual_)
            return;
        auto candidate = *project_->attacks();
        auto attack = candidate.entry(attack_);
        attack.visual = v;
        candidate.set(attack_, attack); // Check capacity before accepting an unfinished draft.
        if (v.frames || v.nativeEffect != visual_.nativeEffect) sourceArtwork_.clear();
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
    if (visual_.nativeEffect != history_[at].nativeEffect || history_[at].frames) sourceArtwork_.clear();
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
    const bool composed=visual_.frames && EffectComposition::contains(*visual_.frames);
    frameControls_->setVisible(visual_.frames.has_value() && !composed);
    compositionHelp_->setVisible(composed);compose_->setText(composed?"Edit pieces…":"Create effect…");
    nativeControls_->setVisible(!visual_.frames && (inherited_ >= 0 || visual_.nativeEffect >= 0));
    effectSize_->setValue(visual_.sizePercent);
    effectX_->setValue(visual_.offsetX); effectY_->setValue(visual_.offsetY);
    motionMode_->setCurrentIndex(visual_.motionTicks ? 1 : 0);
    motionControls_->setVisible(visual_.motionTicks != 0);
    motionX_->setValue(visual_.motionX); motionY_->setValue(visual_.motionY);
    motionStart_->setValue(visual_.motionStart); motionTicks_->setValue(std::max(1, visual_.motionTicks));
    motionEasing_->setCurrentIndex(visual_.motionEasing);
    legacyColour_->setVisible(visual_.hue != 0 || visual_.color >= 0);
    refreshArtwork();
    if (visual_.frames) {
        const auto &asset = *visual_.frames;
        source_->setText(QString(composed ? "Composed effect · %1 frames · %2 × %3" : "Independent imported sequence · %1 frames · %2 × %3")
                             .arg(asset.frames.size())
                             .arg(asset.size.width())
                             .arg(asset.size.height()));
        originX_->setValue(asset.anchor.x());
        originY_->setValue(asset.anchor.y());
        for (int i = 0; i < asset.frames.size(); ++i) {
            auto item = new QListWidgetItem(
                QIcon(QPixmap::fromImage(
                    effectChecker(asset.composite(i))
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
    play_->setEnabled(false); step_->setEnabled(false); exportFrame_->setEnabled(false);
    framePosition_->clear();
    pausedMs_ = 0;
    playing_ = false;
    play_->setText("Play");
    preview_->setText("Preparing effect preview…");
    if (inherited_ < 0 && visual_.nativeEffect < 0 && !visual_.frames) {
        preview_->setText(
            "This attack uses its class animation.\nChoose Replace effect to add a visual effect.");
        return;
    }
    auto watcher = new QFutureWatcher<EffectClip>(this);
    const auto rom = project_->rom();
    const auto bodyAnimations = project_->bodyAnimationData();
    const auto visual = visual_;
    const bool enemyPreview = enemyPreview_;
    const int inherited = std::max(0, inherited_), classId = classId_;
    const auto attack = project_->attacks()->entry(attack_);
    const bool allies = effectTargetsAllies(attack);
    const int targetCount = attack.targetPattern == 9 || attack.targetSide == 3 || attack.targetPattern == 1 ? 1 : 5;
    const int strikes = attack.targetPattern == 11 ? attack.strikeCount : 0;
    connect(watcher, &QFutureWatcher<EffectClip>::finished, this, [this, watcher, generation] {
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
        play_->setEnabled(clipTicks_ > 0); step_->setEnabled(clipTicks_ > 0); exportFrame_->setEnabled(clipTicks_ > 0);
        sourceArtwork_ = result.artwork;
        refreshArtwork();
        playing_ = true;
        play_->setText("Pause");
        clock_.restart();
        showFrame();
    });
    watcher->setFuture(
        QtConcurrent::run([rom, bodyAnimations, visual, inherited, classId, cancel, enemyPreview, allies, targetCount, strikes] {
            EffectPlayer::Scene scene;
            scene.bodyAnimations = bodyAnimations;
            scene.sourceClass = classId;
            scene.enemyCaster = enemyPreview;
            scene.allies = allies;
            scene.targetCount = targetCount;
            scene.strikeCount = strikes;
            return renderEffectClip(rom, visual, inherited, scene, cancel);
        }));
}
int AttackEffectEditor::currentTick() const {
    if (clipTicks_ <= 0) return 0;
    const auto tick = (pausedMs_ + (playing_ ? clock_.elapsed() : 0)) * 30 / 1000;
    return loop_->isChecked() ? int(tick % clipTicks_) : int(qMin<qint64>(tick, clipTicks_ - 1));
}
SpriteAsset AttackEffectEditor::renderedSequence() const {
    return effectSequence(clip_, holds_, "Rendered attack effect");
}
bool AttackEffectEditor::exportRenderedSequence(const QString &path, EffectExport format) {
    if (QFileInfo(path).exists() && QFileInfo(path).canonicalFilePath() ==
                                        QFileInfo(project_->rom().sourcePath()).canonicalFilePath())
        throw Error("Cannot overwrite the source ROM");
    return exportEffectSequence(renderedSequence(), path, format, this);
}
QImage AttackEffectEditor::renderedFrame() const {
    if (clip_.isEmpty() || clipTicks_ <= 0)
        return {};
    int tick = currentTick(), index = 0;
    while (index + 1 < holds_.size() && tick >= holds_[index])
        tick -= holds_[index++];
    return clip_[index];
}
void AttackEffectEditor::showFrame() {
    if (playing_ && !loop_->isChecked() && clipTicks_ > 0 &&
        (pausedMs_ + clock_.elapsed()) * 30 / 1000 >= clipTicks_) {
        pausedMs_ = (qint64(clipTicks_) * 1000 + 29) / 30;
        playing_ = false; play_->setText("Play");
    }
    const auto image = renderedFrame();
    if (image.isNull()) return;
    framePosition_->setText(QString("Frame %1 / %2 · 30 fps").arg(currentTick() + 1).arg(clipTicks_));
    preview_->setPixmap(QPixmap::fromImage(effectChecker(image))
                            .scaled(preview_->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
}
void AttackEffectEditor::exportRenderedFrame(const QString &path) const {
    const auto image = renderedFrame();
    if (image.isNull()) throw Error("Wait for the effect preview to finish loading");
    if (QFileInfo(path).exists() && QFileInfo(path).canonicalFilePath() == QFileInfo(project_->rom().sourcePath()).canonicalFilePath())
        throw Error("Cannot overwrite the source ROM");
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || !image.save(&file, "PNG") || !file.commit())
        throw Error("Cannot export frame: " + file.errorString());
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
        AttackVisual v;
        v.delay = visual_.delay;
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
void AttackEffectEditor::refreshArtwork() {
    const auto selected = artworkList_->currentItem()
        ? artworkList_->currentItem()->data(Qt::UserRole).toUInt() : 0;
    QSignalBlocker blocker(artworkList_);
    artworkList_->clear();
    for (auto i = sourceArtwork_.begin(); i != sourceArtwork_.end(); ++i) {
        const auto art = visual_.artwork.value(i.key(), i.value());
        const bool localMode = art.color >= 0 || art.hue != 0;
        const auto image = effectArtworkImage(art, visual_);
        auto item = new QListWidgetItem(QIcon(QPixmap::fromImage(effectChecker(image).scaled(
            64, 64, Qt::KeepAspectRatio, Qt::FastTransformation))),
            QString("Texture %1:%2 · %3 × %4%5").arg(i.key() >> 16).arg(i.key() & 65535)
                .arg(art.width).arg(art.height).arg((art.rgba != i->rgba ? QString(" · replaced") : QString()) +
                    (localMode ? QString(" · recoloured") : QString())));
        item->setData(Qt::UserRole, i.key());
        artworkList_->addItem(item);
        if (i.key() == selected) artworkList_->setCurrentItem(item);
    }
    if (sourceArtwork_.isEmpty() && clipTicks_ > 0 && !visual_.frames) {
        auto item = new QListWidgetItem("No image textures in this effect. Use Replace effect… to supply new artwork.", artworkList_);
        item->setFlags(Qt::NoItemFlags);
    }
    if (!artworkList_->currentItem() && !sourceArtwork_.isEmpty()) artworkList_->setCurrentRow(0);
    refreshArtworkColour();
}
void AttackEffectEditor::refreshArtworkColour() {
    QScopedValueRollback guard(refreshing_, true);
    const auto item = artworkList_->currentItem();
    const bool selectedArtwork = item && item->data(Qt::UserRole).isValid();
    replaceArtwork_->setEnabled(selectedArtwork);
    restoreArtwork_->setEnabled(selectedArtwork && visual_.artwork.contains(item->data(Qt::UserRole).toUInt()));
    const auto art = selectedArtwork ? visual_.artwork.value(item->data(Qt::UserRole).toUInt()) : EffectArtwork{};
    colourMode_->setEnabled(selectedArtwork);
    colourMode_->setCurrentIndex(art.color >= 0 ? 2 : art.hue ? 1 : 0);
    hue_->setValue(art.hue);
    artworkColourForm_->setRowVisible(hue_, selectedArtwork && art.hue != 0 && art.color < 0);
    artworkColourForm_->setRowVisible(effectColour_, selectedArtwork && art.color >= 0);
    effectColour_->setText(art.color >= 0 ? QColor::fromRgb(art.color).name() + "…" : "Choose colour…");

}
void AttackEffectEditor::editArtworkColour(const std::function<void(EffectArtwork &)> &edit) {
    if (refreshing_) return;
    const auto item = artworkList_->currentItem();
    if (!item || !item->data(Qt::UserRole).isValid()) return;
    const auto key = item->data(Qt::UserRole).toUInt();
    if (!sourceArtwork_.contains(key)) return;
    const auto source = sourceArtwork_.value(key);
    auto art = visual_.artwork.value(key, source);
    edit(art);
    auto v = visual_;
    if (art == source) v.artwork.remove(key);
    else v.artwork[key] = std::move(art);
    record(v);
}
void AttackEffectEditor::replaceArtwork(quint32 key, const QImage &image) {
    if (!sourceArtwork_.contains(key)) throw Error("Choose artwork from the current effect preview");
    if (image.isNull() || qint64(image.width()) * image.height() > 16 * 1024 * 1024)
        throw Error("Choose an image with at most 16 million pixels");
    auto art = visual_.artwork.value(key, sourceArtwork_.value(key));
    const auto pixels = imageToSrgb(image).convertToFormat(QImage::Format_RGBA8888)
        .scaled(art.width, art.height, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    art.rgba = QByteArray(reinterpret_cast<const char *>(pixels.constBits()), art.width * art.height * 4);
    auto v = visual_;
    v.artwork[key] = std::move(art);
    record(v);
}
void AttackEffectEditor::chooseArtworkFile() {
    const auto item = artworkList_->currentItem();
    if (!item) return;
    const auto key = item->data(Qt::UserRole).toUInt();
    const auto path = QFileDialog::getOpenFileName(this, "Replace selected effect artwork", {}, "PNG image (*.png)");
    if (path.isEmpty()) return;
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const auto image = reader.read();
    if (image.isNull()) throw Error("Cannot read artwork: " + reader.errorString());
    replaceArtwork(key, image);
}
void AttackEffectEditor::chooseArtworkLibrary() {
    const auto item = artworkList_->currentItem();
    if (!item) return;
    const auto key = item->data(Qt::UserRole).toUInt();
    QStringList labels, ids;
    for (const auto &asset : project_->sprites().assets()) {
        labels << QString("%1 · %2 (%3 frames)").arg(labels.size() + 1).arg(asset.name).arg(asset.frames.size());
        ids << asset.id;
    }
    if (labels.isEmpty()) throw Error("The Art Library is empty. Add artwork there or choose From file.");
    bool ok = false;
    const auto selection = QInputDialog::getItem(this, "Choose replacement artwork", "Art Library", labels, 0, false, &ok);
    if (!ok) return;
    const auto asset = project_->sprites().asset(ids.at(labels.indexOf(selection)));
    int frame = 0;
    if (asset.frames.size() > 1) {
        QStringList frames;
        for (int i = 0; i < asset.frames.size(); ++i) frames << QString("Frame %1").arg(i + 1);
        const auto selected = QInputDialog::getItem(this, "Choose a frame", "Replacement artwork", frames, 0, false, &ok);
        if (!ok) return;
        frame = frames.indexOf(selected);
    }
    replaceArtwork(key, asset.composite(frame));
}
void AttackEffectEditor::copyFromAttack(int id) {
    const auto &source = project_->attacks()->entry(id);
    auto visual = source.visual;
    if (!visual.frames && visual.nativeEffect < 0) {
        if (u8(source.data, 0) < 2 || AttackData::isTemplate(id))
            throw Error("Choose an attack with a visual effect");
        visual.nativeEffect = EffectPlayer::retailEffect(source.animation);
    }
    if (visual.frames)
        visual.frames->id = SpriteLibrary::newId();
    record(visual);
}
void AttackEffectEditor::chooseAttack() {
    QDialog dialog(this);
    dialog.setWindowTitle("Copy an attack's edited effect");
    dialog.resize(460, 560);
    auto layout = new QVBoxLayout(&dialog);
    note("Copies the effect and its edits into this attack. Later edits to either copy stay independent.", layout);
    auto search = new QLineEdit;
    search->setPlaceholderText("Search attacks or IDs…");
    layout->addWidget(search);
    auto list = new QListWidget;
    layout->addWidget(list, 1);
    const auto &attacks = *project_->attacks();
    for (int id = 1; id < attacks.entries().size(); ++id) {
        const auto &a = attacks.entry(id);
        if (id == attack_ || AttackData::isTemplate(id) ||
            (!a.visual.frames && a.visual.nativeEffect < 0 && u8(a.data, 0) < 2))
            continue;
        auto item = new QListWidgetItem(QString("%1 · ID %2%3")
            .arg(AttackData::displayName(a)).arg(id).arg(a.visual.modified() ? " · edited effect" : ""));
        item->setData(Qt::UserRole, id);
        list->addItem(item);
    }
    if (list->count()) list->setCurrentRow(0);
    connect(search, &QLineEdit::textChanged, &dialog, [list](const QString &query) {
        for (int i = 0; i < list->count(); ++i)
            list->item(i)->setHidden(!list->item(i)->text().contains(query, Qt::CaseInsensitive));
    });
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setEnabled(list->count() > 0);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(list, &QListWidget::itemDoubleClicked, &dialog, &QDialog::accept);
    if (dialog.exec() == QDialog::Accepted && list->currentItem())
        copyFromAttack(list->currentItem()->data(Qt::UserRole).toInt());
}
void AttackEffectEditor::importLibrary(const QString &id) {
    AttackVisual v;
    v.delay = visual_.delay;
    v.frames = project_->sprites().asset(id);
    v.frames->id = SpriteLibrary::newId();
    v.validate();
    record(v);
}
void AttackEffectEditor::importFiles(const QStringList &paths) {
    AttackVisual v;
    v.delay = visual_.delay;
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
