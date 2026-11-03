#include "app/effect_library_editor.h"
#include <QCheckBox>
#include "app/content_refresh.h"
#include "app/effect_composer.h"
#include "app/sprite_library_command.h"
#include "engine/effect_composition.h"
#include "engine/image_colors.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSaveFile>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <algorithm>
#include <cmath>
namespace lordly {
namespace {
QLabel *note(const QString &text, QLayout *layout) {
    auto label = new QLabel(text);
    label->setWordWrap(true);
    layout->addWidget(label);
    return label;
}
}
EffectLibraryEditor::EffectLibraryEditor(Project *project, QUndoStack *undo,
    std::function<void()> changed, QWidget *parent, bool picker) : QWidget(parent), project_(project), undo_(undo), picker_(picker) {
    setObjectName("effectLibrary");
    project_->loadAttacks();
    auto root = new QVBoxLayout(this);
    auto commands = new QHBoxLayout; root->addLayout(commands);
    auto create = new QPushButton("Create effect…"); create->setObjectName("effectLibraryCreate"); commands->addWidget(create);
    editEffect_ = new QPushButton("Edit effect…"); editEffect_->setObjectName("effectLibraryEdit"); commands->addWidget(editEffect_);
    assignEffect_ = new QPushButton("Assign to attack…"); assignEffect_->setObjectName("effectLibraryAssign"); commands->addWidget(assignEffect_);
    moreEffect_=new QPushButton("More…");commands->addWidget(moreEffect_);auto more=new QMenu(moreEffect_);moreEffect_->setMenu(more);commands->addStretch();
    more->addAction("Duplicate effect",this,[this]{guarded([this]{
        auto model=EffectComposition::fromAsset(project_->sprites().asset(sources_->currentItem()->data(Qt::UserRole+1).toString()));
        model.id=SpriteLibrary::newId();model.name=model.name.left(110)+" copy";
        auto library=project_->sprites();library.replace(model.bake());undo_->push(new SpriteLibraryCommand(project_,"Duplicate composed effect",library));selectSavedEffect(model.id);
    });});
    more->addAction("Delete effect",this,[this]{guarded([this]{
        auto library=project_->sprites();library.remove(sources_->currentItem()->data(Qt::UserRole+1).toString());
        undo_->push(new SpriteLibraryCommand(project_,"Delete composed effect",library));
    });});
    for(auto button : {create,editEffect_,assignEffect_,moreEffect_}) button->setVisible(!picker_);
    connect(create,&QPushButton::clicked,this,[this]{guarded([this]{compose(false);});});
    connect(editEffect_,&QPushButton::clicked,this,[this]{guarded([this]{compose(false,true);});});
    connect(assignEffect_,&QPushButton::clicked,this,[this]{guarded([this]{
        const auto id=sources_->currentItem()->data(Qt::UserRole+1).toString();
        chooseEffectAttack(project_,undo_,project_->sprites().asset(id),this);
    });});
    note(picker_ ? "Choose an effect and an artwork piece, then click Add selected piece."
        : "Create an effect, add pieces from any source, and give each piece its own movement and timing. "
          "Your saved effects appear first in the list. Game effects below provide reusable artwork.", root);
    auto split = new QSplitter;
    root->addWidget(split, 1);
    auto left = new QWidget;
    auto listLayout = new QVBoxLayout(left);
    listLayout->setContentsMargins(0, 0, 0, 0);
    search_ = new QLineEdit;
    search_->setObjectName("effectLibrarySearch");
    search_->setPlaceholderText("Find an effect or attack…");
    search_->setClearButtonEnabled(true);
    listLayout->addWidget(search_);
    sources_ = new QListWidget;
    sources_->setObjectName("effectLibrarySources");
    sources_->setWordWrap(true);
    listLayout->addWidget(sources_, 1);
    split->addWidget(left);
    auto right = new QWidget;
    auto content = new QVBoxLayout(right);
    content->setContentsMargins(0, 0, 0, 0);
    split->addWidget(right);
    split->setSizes({260, 740});
    auto views = new QHBoxLayout;
    content->addLayout(views, 2);
    auto scene = new QVBoxLayout;
    views->addLayout(scene, 2);
    preview_ = new QLabel("Choose an effect");
    preview_->setObjectName("effectLibraryPreview");
    preview_->setAlignment(Qt::AlignCenter);
    preview_->setMinimumSize(320, 240);
    preview_->setStyleSheet("background:#242932;color:#f5e8cc;");
    scene->addWidget(preview_, 1);
    auto playback = new QHBoxLayout;
    scene->addLayout(playback);
    play_ = new QPushButton("Play");
    play_->setObjectName("effectLibraryPlay");
    loop_ = new QCheckBox("Loop");loop_->setObjectName("effectLibraryLoop");
    playback->addWidget(play_); playback->addWidget(loop_); playback->addStretch();
    auto exportSequenceButton = new QPushButton("Export sequence…");
    exportSequenceButton->setObjectName("effectLibraryExportSequence");
    exportSequenceButton->setEnabled(false);
    playback->addWidget(exportSequenceButton);
    auto exports = new QMenu(exportSequenceButton);
    exportSequenceButton->setMenu(exports);
    for (const auto format :
         {EffectExport::AnimatedPng, EffectExport::FramesZip, EffectExport::Webm}) {
        const QString title = format == EffectExport::AnimatedPng ? "Animated PNG…"
                              : format == EffectExport::FramesZip ? "PNG frame sequence ZIP…"
                                                                  : "WebM…";
        exports->addAction(title, this, [this, format] {
            guarded([this, format] {
                const QString filter = format == EffectExport::AnimatedPng ? "Animated PNG (*.png)"
                                       : format == EffectExport::FramesZip
                                           ? "PNG frame sequence (*.zip)"
                                           : "WebM video (*.webm)";
                const auto path = QFileDialog::getSaveFileName(this, "Export full rendered effect",
                                                               "effect-sequence", filter);
                if (!path.isEmpty() && exportSequence(path, format))
                    notice_->setText(
                        "Full effect sequence exported at its original preview size and timing.");
            });
        });
    }
    connect(play_, &QPushButton::clicked, this, [this] {
        if (playing_) pausedMs_ += clock_.elapsed();
        else {
            if (pausedMs_ * 30 / 1000 >= clip_.ticks - 1) pausedMs_ = 0;
            clock_.restart();
        }
        playing_ = !playing_; play_->setText(playing_ ? "Pause" : "Play");showFrame();
    });

    auto piece = new QVBoxLayout;
    views->addLayout(piece, 1);
    piecePreview_ = new QLabel;
    piecePreview_->setAlignment(Qt::AlignCenter);
    piecePreview_->setMinimumSize(160, 160);
    piecePreview_->setStyleSheet("background:#242932;");
    piece->addWidget(piecePreview_, 1);
    details_ = note("Select an artwork piece below", piece);
    save_ = new QPushButton(picker_ ? "Add selected piece" : "Use artwork…");
    save_->setObjectName("effectLibrarySave");
    export_ = new QPushButton("Export PNG…");
    export_->setObjectName("effectLibraryExport");
    edit_ = new QPushButton("Edit saved artwork");
    piece->addWidget(save_); piece->addWidget(export_); piece->addWidget(edit_);
    if(picker_) {
        connect(save_, &QPushButton::clicked, this, [this] { guarded([this] { emit pieceChosen(selectedPiece()); }); });
        export_->hide(); edit_->hide();
    } else {
        auto use = new QMenu(save_); save_->setMenu(use);
        use->addAction("Create effect from this piece…",this,[this]{guarded([this]{compose(true);});});
        use->addAction("Save piece to Artwork",this,[this]{guarded([this]{saveArtwork();});});
    }
    connect(export_, &QPushButton::clicked, this, [this] { guarded([this] {
        const auto path = QFileDialog::getSaveFileName(this, "Export selected artwork", "effect-artwork.png", "PNG image (*.png)");
        if (!path.isEmpty()) { exportArtwork(path); notice_->setText("Artwork exported to " + path); }
    }); });
    connect(edit_, &QPushButton::clicked, this, [this] { emit editArtwork(savedId_); });
    note("Textures and rendered animation", content);
    pieces_ = new QListWidget;
    pieces_->setObjectName("effectLibraryPieces");
    pieces_->setViewMode(QListView::IconMode);
    pieces_->setResizeMode(QListView::Adjust);
    pieces_->setMovement(QListView::Static);
    pieces_->setIconSize({64, 64});
    pieces_->setGridSize({150, 110});
    pieces_->setWordWrap(true);
    pieces_->setMinimumHeight(140);
    content->addWidget(pieces_, 1);
    note("Texture copies contain image pixels; their original movement stays with the effect. "
         "Full rendered animation keeps movement and timing in the copied frames.", content);
    notice_ = note({}, root);
    notice_->setObjectName("effectLibraryNotice");
    connect(search_, &QLineEdit::textChanged, this, &EffectLibraryEditor::filter);
    connect(sources_, &QListWidget::currentRowChanged, this, &EffectLibraryEditor::loadSelection);
    connect(pieces_, &QListWidget::currentRowChanged, this, [this] { savedId_.clear(); refreshPiece(); });
    watchContentChanges(undo_, this, [this, changed] { refreshSources(); refreshPiece(); changed(); });
    auto timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] { if (isVisible()) showFrame(); });
    timer->start(33);
    refreshSources(); refreshPiece();
}
EffectLibraryEditor::~EffectLibraryEditor() { if (canceled_) canceled_->store(true); }
void EffectLibraryEditor::showEvent(QShowEvent *event) { QWidget::showEvent(event); loadSelection(); }
void EffectLibraryEditor::refreshSources() {
    const auto savedSelection = sources_->currentItem() ? sources_->currentItem()->data(Qt::UserRole+1).toString() : QString();
    const int selected = sources_->currentItem() ? sources_->currentItem()->data(Qt::UserRole).toInt() : 8;
    {
        QSignalBlocker blocker(sources_);
        sources_->clear();
        int custom = -1;
        if(!picker_) for(const auto &asset : project_->sprites().assets()) if(EffectComposition::contains(asset)) {
            auto item = new QListWidgetItem("My effect · " + asset.name,sources_);
            item->setData(Qt::UserRole,custom--); item->setData(Qt::UserRole+1,asset.id);
        }
        const auto names = effectNames(project_->rom());
        auto add = [this](int id, const QString &name) {
            auto item = new QListWidgetItem(name, sources_); item->setData(Qt::UserRole, id);
        };
        for (auto it = names.begin(); it != names.end(); ++it)
            add(it.key(), QString("%1 · Effect %2").arg(it.value()).arg(it.key()));
        for (int id = 1; id < project_->attacks()->entries().size(); ++id) {
            const auto &a = project_->attacks()->entry(id);
            if (a.visual.modified()) add(1000 + id, QString("%1 · Edited attack %2").arg(AttackData::displayName(a)).arg(id));
        }
        sources_->setCurrentRow(8);
        for (int i = 0; i < sources_->count(); ++i)
            if ((!savedSelection.isEmpty() && sources_->item(i)->data(Qt::UserRole+1).toString() == savedSelection) ||
                (savedSelection.isEmpty() && sources_->item(i)->data(Qt::UserRole).toInt() == selected)) sources_->setCurrentRow(i);
    }
    filter();
    loadSelection();
}
void EffectLibraryEditor::filter() {
    for (int i = 0; i < sources_->count(); ++i)
        sources_->item(i)->setHidden(!sources_->item(i)->text().contains(search_->text(), Qt::CaseInsensitive));
}
void EffectLibraryEditor::selectEffect(int id) {
    search_->clear();
    for (int i = 0; i < sources_->count(); ++i)
        if (sources_->item(i)->data(Qt::UserRole).toInt() == id) { sources_->setCurrentRow(i); loadSelection(); return; }
    throw Error("Effect is not in the library catalog");
}
void EffectLibraryEditor::selectAttack(int id) { selectEffect(1000 + id); }
void EffectLibraryEditor::selectSavedEffect(const QString &id) {
    search_->clear();
    for(int i=0;i<sources_->count();++i)if(sources_->item(i)->data(Qt::UserRole+1).toString()==id){sources_->setCurrentRow(i);loadSelection();return;}
    throw Error("Saved effect is not in the library");
}
SpriteAsset EffectLibraryEditor::selectedPiece() const {
    if (pieces_->currentItem() &&
        pieces_->currentItem()->data(Qt::UserRole).toUInt() == 0xffffffffU && rendered_)
        return *rendered_;
    const auto image=selectedImage();
    auto asset=SpriteAsset::fromImage((sourceName_+" · "+pieces_->currentItem()->text().section('\n',0,0)).left(120),image);
    asset.kind="frame";asset.provenance={{"source","effect-artwork"},{"effectSource",requested_},
        {"resource",int(pieces_->currentItem()->data(Qt::UserRole).toUInt())}};
    return asset;
}
void EffectLibraryEditor::compose(bool fromPiece, bool editSaved) {
    EffectComposition composition;
    if(editSaved)composition=EffectComposition::fromAsset(project_->sprites().asset(sources_->currentItem()->data(Qt::UserRole+1).toString()));
    EffectComposer dialog(project_,composition,this,true);
    if(fromPiece)dialog.addPiece(selectedPiece());
    if(dialog.exec()!=QDialog::Accepted)return;
    const auto asset=dialog.asset();auto library=project_->sprites();library.replace(asset);
    undo_->push(new SpriteLibraryCommand(project_,"Save composed effect",std::move(library)));
    selectSavedEffect(asset.id);
    if(dialog.assignAfterSave())chooseEffectAttack(project_,undo_,asset,this);
}
void EffectLibraryEditor::loadSelection() {
    const bool saved = sources_->currentItem() && !sources_->currentItem()->data(Qt::UserRole+1).toString().isEmpty();
    editEffect_->setEnabled(saved); assignEffect_->setEnabled(saved); moreEffect_->setEnabled(saved);
    if (!isVisible() || !sources_->currentItem()) return;
    const auto item = sources_->currentItem();
    const int id = item->data(Qt::UserRole).toInt();
    AttackVisual visual;
    int inherited = id;
    if(id < 0) { visual.frames=project_->sprites().asset(item->data(Qt::UserRole+1).toString()); inherited=0; }
    else if (id >= 1000) {
        const auto &a = project_->attacks()->entry(id - 1000);
        visual = a.visual;
        inherited = EffectPlayer::retailEffect(a.animation);
    }
    if (requested_ == id && visual_ == visual) return;
    requested_ = id; visual_ = visual; sourceName_ = item->text();
    if (canceled_) canceled_->store(true);
    canceled_ = std::make_shared<std::atomic_bool>(false);
    const auto cancel = canceled_;
    const int generation = ++generation_;
    clip_ = {};
    images_.clear();
    pieces_->clear();
    savedId_.clear();
    rendered_.reset();
    findChild<QPushButton *>("effectLibraryExportSequence")->setEnabled(false);
    refreshPiece(); preview_->setText("Preparing effect and artwork…");
    play_->setEnabled(false); playing_ = false; pausedMs_ = 0;
    notice_->clear();
    const auto rom = project_->rom();
    auto watcher = new QFutureWatcher<EffectClip>(this);
    connect(watcher, &QFutureWatcher<EffectClip>::finished, this, [this, watcher, generation] {
        const auto result = watcher->result(); watcher->deleteLater();
        if (generation != generation_) return;
        if (!result.error.isEmpty()) { preview_->setText(result.error); requested_ = -1; return; }
        clip_ = result;
        findChild<QPushButton *>("effectLibraryExportSequence")
            ->setEnabled(!clip_.images.isEmpty());
        if (visual_.frames) {
            for (int i = 0; i < visual_.frames->frames.size(); ++i) images_[i] = visual_.frames->composite(i);
        } else for (auto it = clip_.artwork.begin(); it != clip_.artwork.end(); ++it)
            images_[it.key()] = effectArtworkImage(visual_.artwork.value(it.key(), it.value()), visual_);
        for (auto it = images_.begin(); it != images_.end(); ++it) {
            const QString label = visual_.frames ? QString("Frame %1").arg(it.key() + 1)
                : QString("Texture %1:%2").arg(it.key() >> 16).arg(it.key() & 65535);
            auto item = new QListWidgetItem(QIcon(QPixmap::fromImage(effectChecker(it.value()).scaled(
                64, 64, Qt::KeepAspectRatio, Qt::FastTransformation))),
                QString("%1\n%2 × %3").arg(label).arg(it->width()).arg(it->height()), pieces_);
            item->setData(Qt::UserRole, it.key());
        }
        if (!visual_.frames && !clip_.images.isEmpty()) guarded([&] {
            // Rendered models are a fixed-view sequence, not editable sprite textures.
            // Fit a reusable copy to the composer; export retains all original pixels.
            auto sequence = effectSequence(clip_.images, clip_.holds,
                                           (sourceName_ + " · rendered animation").left(120));
            const double scale =
                std::min({1.0, 256.0 / sequence.size.width(), 240.0 / sequence.size.height(),
                          std::sqrt(double(4 * 1024 * 1024) / sequence.pixels())});
            const QSize size(std::max(1, int(sequence.size.width() * scale)),
                             std::max(1, int(sequence.size.height() * scale)));
            if (size != sequence.size)
                for (auto &frame : sequence.frames)
                    frame.layers[0].image = frame.layers[0].image.scaled(
                        size, Qt::IgnoreAspectRatio, Qt::FastTransformation);
            sequence.size = size;
            sequence.anchor = {size.width() / 2, size.height() / 2};
            sequence.validate();
            rendered_ = sequence;
            images_[0xffffffffU] = sequence.composite(sequence.frames.size() / 2);
            auto item = new QListWidgetItem(
                QIcon(QPixmap::fromImage(effectChecker(images_[0xffffffffU]))
                          .scaled(64, 64, Qt::KeepAspectRatio)),
                QString("Full rendered animation\n%1 × %2 · %3 ticks · fixed view")
                    .arg(size.width())
                    .arg(size.height())
                    .arg(clip_.ticks),
                pieces_);
            item->setData(Qt::UserRole, 0xffffffffU);
            notice_->setText("Some pieces, including Fenrir's creature, are animated models rather "
                             "than sprite images. "
                             "Full rendered animation includes every visible piece and is fitted "
                             "to the composer's size budget. "
                             "Export sequence keeps the complete preview at its original size.");
        });
        if (!images_.isEmpty()) pieces_->setCurrentRow(0);
        else
            notice_->setText("No visible artwork was produced by this effect.");
        playing_ = !clip_.images.isEmpty(); play_->setEnabled(playing_); play_->setText("Pause");
        clock_.restart(); showFrame();
    });
    bool allies = (inherited >= 32 && inherited <= 35) || inherited == 103;
    int targetCount = 5;
    if (requested_ >= 1000) {
        const auto &attack = project_->attacks()->entry(requested_ - 1000);
        allies = effectTargetsAllies(attack);
        if (attack.targetPattern == 9 || attack.targetSide == 3 || attack.targetPattern == 1) targetCount = 1;
    }
    watcher->setFuture(QtConcurrent::run([rom, visual, inherited, cancel, allies, targetCount] {
        EffectPlayer::Scene scene; scene.enemyCaster = false;
        scene.allies = allies; scene.targetCount = targetCount;
        return renderEffectClip(rom, visual, inherited, scene, cancel);
    }));
}
void EffectLibraryEditor::showFrame() {
    if (clip_.ticks <= 0 || clip_.images.isEmpty()) return;
    const auto elapsed = (pausedMs_ + (playing_ ? clock_.elapsed() : 0)) * 30 / 1000;
    if (playing_ && !loop_->isChecked() && elapsed >= clip_.ticks) {
        pausedMs_ = (qint64(clip_.ticks) * 1000 + 29) / 30;
        playing_ = false;play_->setText("Play");
    }
    int tick = loop_->isChecked() ? int(elapsed % clip_.ticks) : int(qMin<qint64>(elapsed,clip_.ticks-1)), index = 0;
    while (index + 1 < clip_.holds.size() && tick >= clip_.holds[index]) tick -= clip_.holds[index++];
    preview_->setPixmap(QPixmap::fromImage(effectChecker(clip_.images[index])).scaled(
        preview_->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
}
QImage EffectLibraryEditor::selectedImage() const {
    const auto item = pieces_->currentItem();
    if (!item || !images_.contains(item->data(Qt::UserRole).toUInt())) throw Error("Select an artwork piece first");
    return images_.value(item->data(Qt::UserRole).toUInt());
}
void EffectLibraryEditor::refreshPiece() {
    const bool selected = pieces_->currentItem() != nullptr;
    save_->setEnabled(selected); export_->setEnabled(selected);
    bool saved = false;
    for (const auto &a : project_->sprites().assets()) if (a.id == savedId_) saved = true;
    edit_->setEnabled(saved);
    if (!selected) { piecePreview_->clear(); details_->setText("Select an artwork piece below"); return; }
    piecePreview_->setPixmap(QPixmap::fromImage(effectChecker(selectedImage())).scaled(
        192, 192, Qt::KeepAspectRatio, Qt::FastTransformation));
    details_->setText(pieces_->currentItem()->text());
}
QString EffectLibraryEditor::saveArtwork(const QString &name) {
    auto asset = selectedPiece();
    asset.id = SpriteLibrary::newId();
    if (!name.isEmpty())
        asset.name = name;
    auto library = project_->sprites(); library.replace(asset);
    undo_->push(new SpriteLibraryCommand(project_, "Save effect artwork to library", std::move(library)));
    savedId_ = asset.id; refreshPiece();
    notice_->setText("Saved “" + asset.name + "” in Artwork. This is an independent copy.");
    return asset.id;
}
bool EffectLibraryEditor::exportSequence(const QString &path, EffectExport format) {
    if (QFileInfo(path).exists() && QFileInfo(path).canonicalFilePath() ==
                                        QFileInfo(project_->rom().sourcePath()).canonicalFilePath())
        throw Error("Cannot overwrite the source ROM");
    return exportEffectSequence(effectSequence(clip_.images, clip_.holds, sourceName_), path,
                                format, this);
}
void EffectLibraryEditor::exportArtwork(const QString &path) const {
    const auto image = imageToSrgb(selectedImage());
    if (QFileInfo(path).exists() && QFileInfo(path).canonicalFilePath() == QFileInfo(project_->rom().sourcePath()).canonicalFilePath())
        throw Error("Cannot overwrite the source ROM");
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || !image.save(&file, "PNG") || !file.commit())
        throw Error("Cannot export artwork: " + file.errorString());
}
void EffectLibraryEditor::guarded(const std::function<void()> &fn) {
    try { fn(); } catch (const std::exception &e) { notice_->setText(QString::fromUtf8(e.what())); }
}
} // namespace lordly
