#include "app/combat_animation_replace.h"
#include "app/actor_pose_editor.h"
#include "app/recovery_draft.h"
#include "engine/actor_art.h"
#include "engine/animation_file_import.h"
#include "engine/combat_animation_frames.h"
#include "engine/sprite_sources.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
namespace lordly {
namespace {
int artworkChoice(Project &p, int id, int flags) {
    const int value = u8(p.record(Table::Classes, id).bytes, 69), owner = value ? value : id;
    return (flags & 1) && (owner == 15 || owner == 30) ? flags ^ 2 : flags;
}
void addAnimationSources(QComboBox &source) {
    // Retain the established source indexes for existing saved UI checks.
    source.addItems({"Files", "Game animations", "Art Library"});
}
void fitSourcePage(QStackedWidget &inputs) {
    inputs.setFixedHeight(inputs.currentWidget()->sizeHint().height());
}
bool frameRecord(const PoseRecord &record) {
    return record.opcode == 1 || record.opcode == 21;
}
int frameToken(const PoseRecord &record) {
    return u8(record.operands, 0) + (record.opcode == 21 ? 256 * u8(record.operands, 1) : 0);
}
ActorArt sourceArt(Project &project, int id, int flags) {
    const int shared = u8(project.record(Table::Classes, id).bytes, 69);
    return ActorArt::loadBody(project.rom(), id, shared ? shared : id, flags >> 1, flags & 1,
                              project.bodyAnimationData());
}
AnimationDescriptor sourceDescriptor(Project &project, int id, int flags) {
    if (const auto data = project.animationSequences()->find(id, flags))
        return *data;
    const auto art = sourceArt(project, id, flags);
    return AnimationDescriptor::load(
        project.rom(), art.descriptorKey(),
        project.combatArtwork()->previewEdits().value(art.descriptorKey()));
}
} // namespace
std::optional<Project> chooseCombatAnimationReplacement(Project base, CombatAnimationTarget target,
                                                        bool create, QWidget *parent) {
    QDialog dialog(parent);
    dialog.setObjectName("combatAnimationReplaceDialog");
    dialog.setWindowTitle(create ? "Create animation" : "Replace animation");
    dialog.resize(940, 820);
    auto layout = new QVBoxLayout(&dialog);
    const auto destination = base.record(Table::Classes, target.classId).name + " · " +
                             combatAnimationName(base, target);
    auto heading = new QLabel("<b>" + destination.toHtmlEscaped() + "</b>");
    layout->addWidget(heading);
    const int destinationArtwork = artworkChoice(base, target.classId, target.flags);
    auto scope = new QLabel(QString("Applies to %1 · %2 artwork. Other classes and attack "
                                    "assignments keep their animations.")
                                .arg(destinationArtwork & 1 ? "Enemy" : "Player",
                                     destinationArtwork & 2 ? "Alternate" : "Base"));
    scope->setWordWrap(true);
    layout->addWidget(scope);
    QComboBox source;
    source.setObjectName("combatReplacementSource");
    addAnimationSources(source);
    auto sourceForm = new QFormLayout;
    sourceForm->addRow("Import from", &source);
    layout->addLayout(sourceForm);
    QStackedWidget inputs;
    layout->addWidget(&inputs);
    auto filesPage = new QWidget;
    auto fileLayout = new QVBoxLayout(filesPage);
    fileLayout->setContentsMargins(0, 0, 0, 0);
    auto fileRow = new QHBoxLayout;
    fileLayout->addLayout(fileRow);
    QPushButton choose("Select files…");
    choose.setObjectName("combatReplacementFiles");
    fileRow->addWidget(&choose);
    QLabel fileName("PNG frames, a frame-sequence ZIP, or a sprite asset");
    fileName.setWordWrap(true);
    fileRow->addWidget(&fileName, 1);
    QSpinBox fps;
    fps.setObjectName("combatReplacementFps");
    fps.setRange(1, 30);
    fps.setValue(15);
    fps.setSuffix(" frames / second");
    fps.setKeyboardTracking(false);
    auto fileForm = new QFormLayout;
    fileForm->addRow("PNG playback speed", &fps);
    fileLayout->addLayout(fileForm);
    inputs.addWidget(filesPage);
    auto classPage = new QWidget;
    auto classForm = new QFormLayout(classPage);
    classForm->setContentsMargins(0, 0, 0, 0);
    QComboBox classes, variants, animations;
    classes.setObjectName("combatReplacementClass");
    variants.setObjectName("combatReplacementVariant");
    animations.setObjectName("combatReplacementAnimation");
    for (const auto &row : base.records(Table::Classes))
        if (row.id)
            classes.addItem(row.name, row.id);
    classes.setEditable(true);
    classes.setInsertPolicy(QComboBox::NoInsert);
    classes.completer()->setFilterMode(Qt::MatchContains);
    classes.completer()->setCompletionMode(QCompleter::PopupCompletion);
    classes.setCurrentIndex(classes.findData(target.classId));
    variants.addItems({"Player · Base", "Enemy · Base", "Player · Alternate", "Enemy · Alternate"});
    variants.setCurrentIndex(destinationArtwork);
    classForm->addRow("Source class", &classes);
    classForm->addRow("Artwork", &variants);
    classForm->addRow("Animation", &animations);
    inputs.addWidget(classPage);
    auto libraryPage = new QWidget;
    auto libraryForm = new QFormLayout(libraryPage);
    libraryForm->setContentsMargins(0, 0, 0, 0);
    QComboBox library;
    library.setObjectName("combatReplacementLibrary");
    const auto assets = base.sprites().assets();
    for (int i = 0; i < assets.size(); ++i)
        library.addItem(QString("%1 · %2 %3")
                            .arg(assets[i].name)
                            .arg(assets[i].frames.size())
                            .arg(assets[i].frames.size() == 1 ? "frame" : "frames"),
                        i);
    libraryForm->addRow("Sequence", &library);
    QLabel libraryHint(assets.isEmpty()
                           ? "Art Library is empty. Choose Files or Game animations instead."
                           : "Imports the sequence's artwork, placement and frame timing.");
    libraryHint.setWordWrap(true);
    libraryForm->addRow(&libraryHint);
    inputs.addWidget(libraryPage);
    fitSourcePage(inputs);
    QCheckBox loop("Loop animation");
    loop.setObjectName("combatReplacementLoop");
    loop.setChecked(target.action == -1 || target.action == -2 || target.action == -3);
    layout->addWidget(&loop);
    QCheckBox imageSettings("Image settings");
    imageSettings.setObjectName("combatReplacementImageSettings");
    layout->addWidget(&imageSettings);
    QWidget settings;
    auto settingsForm = new QFormLayout(&settings);
    QSpinBox originX, originY;
    originX.setObjectName("combatReplacementOriginX");
    originY.setObjectName("combatReplacementOriginY");
    for (auto v : {&originX, &originY}) {
        v->setRange(-32768, 32767);
        v->setKeyboardTracking(false);
    }
    settingsForm->addRow("Anchor X", &originX);
    settingsForm->addRow("Anchor Y", &originY);
    QCheckBox dither("Dither colors");
    dither.setObjectName("combatReplacementDither");
    settingsForm->addRow(&dither);
    layout->addWidget(&settings);
    settings.hide();
    QLabel note("Preview shows the converted game artwork. File and library imports replace the "
                "sequence, including its timing and loop; game colors may be quantized.");
    note.setWordWrap(true);
    layout->addWidget(&note);
    ActorPoseEditor preview(base.rom());
    preview.setObjectName("combatReplacementPreview");
    preview.setFrameEditing(true);
    preview.hide();
    layout->addWidget(&preview, 1);
    QLabel status;
    status.setObjectName("combatReplacementStatus");
    status.setWordWrap(true);
    layout->addWidget(&status);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons.setObjectName("combatReplacementButtons");
    buttons.button(QDialogButtonBox::Ok)
        ->setText(create ? "Create animation" : "Replace animation");
    buttons.button(QDialogButtonBox::Ok)->setEnabled(false);
    layout->addWidget(&buttons);
    QStringList paths;
    std::optional<SpriteAsset> fileAsset;
    std::optional<Project> candidate;
    QVector<NamedCombatAnimation> choices;
    QTimer debounce;
    debounce.setSingleShot(true);
    debounce.setInterval(100);
    const auto update = [&] {
        candidate.reset();
        buttons.button(QDialogButtonBox::Ok)->setEnabled(false);
        try {
            if (source.currentIndex() == 1) {
                if (classes.currentIndex() < 0 ||
                    classes.currentText() != classes.itemText(classes.currentIndex()) ||
                    animations.currentIndex() < 0)
                    throw Error("Choose a source class and named animation");
                const auto item = choices[animations.currentIndex()];
                candidate = copyCombatAnimation(
                    base, target,
                    {classes.currentData().toInt(),
                     artworkChoice(base, classes.currentData().toInt(), variants.currentIndex()),
                     item.action, item.blocked});
            } else {
                if (source.currentIndex() == 0 && !fileAsset)
                    throw Error("Select files to preview a replacement");
                if (source.currentIndex() == 2 && library.currentIndex() < 0)
                    throw Error("Choose a sequence from Art Library");
                auto asset =
                    source.currentIndex() == 0 ? *fileAsset : assets[library.currentData().toInt()];
                asset.anchor = {originX.value(), originY.value()};
                candidate = replaceCombatAnimation(base, target, asset, loop.isChecked(),
                                                   dither.isChecked());
            }
            preview.setBodyAnimations(candidate->bodyAnimationData());
            preview.setArtwork(candidate->combatArtwork()->previewEdits());
            preview.selectPrivateSequence(target.classId, target.flags,
                                          combatAnimationPose(*candidate, target));
            preview.show();
            preview.showReplacementPreview();
            status.setText(
                "Preview the replacement, then apply. Cancel keeps the current animation.");
            buttons.button(QDialogButtonBox::Ok)->setEnabled(true);
        } catch (const std::exception &e) {
            candidate.reset();
            preview.hide();
            status.setText(QString::fromUtf8(e.what()));
        }
    };
    const auto schedule = [&] {
        candidate.reset();
        buttons.button(QDialogButtonBox::Ok)->setEnabled(false);
        debounce.start();
    };
    QObject::connect(&debounce, &QTimer::timeout, &dialog, update);
    const auto origin = [&](const SpriteAsset &asset) {
        const QSignalBlocker a(&originX), b(&originY), c(&loop);
        originX.setValue(asset.anchor.x());
        originY.setValue(asset.anchor.y());
        if (asset.provenance.contains("stoppedAt"))
            loop.setChecked(asset.provenance["stoppedAt"] == "cycle");
    };
    const auto loadFiles = [&] {
        fileAsset.reset();
        candidate.reset();
        try {
            fileAsset = importAnimationFiles(paths, fps.value());
            origin(*fileAsset);
            schedule();
        } catch (const std::exception &e) {
            debounce.stop();
            preview.hide();
            buttons.button(QDialogButtonBox::Ok)->setEnabled(false);
            status.setText(QString::fromUtf8(e.what()));
        }
    };
    QObject::connect(&choose, &QPushButton::clicked, &dialog, [&] {
        const auto selected = QFileDialog::getOpenFileNames(
            &dialog, "Import animation", {},
            "Animations (*.zip *.png *.lcsprite *.json);;PNG frames (*.png);;Frame sequence ZIP "
            "(*.zip);;Sprite asset (*.lcsprite *.json)");
        if (selected.isEmpty())
            return;
        paths = selected;
        fileName.setText(paths.size() == 1
                             ? QFileInfo(paths.first()).fileName()
                             : QString("%1 PNG files · sorted by filename").arg(paths.size()));
        fitSourcePage(inputs);
        fps.setEnabled(QFileInfo(paths.first()).suffix().compare("png", Qt::CaseInsensitive) == 0);
        loadFiles();
    });
    QObject::connect(&fps, &QSpinBox::valueChanged, &dialog, [&] {
        if (!paths.isEmpty())
            loadFiles();
    });
    const auto reloadClasses = [&] {
        const QSignalBlocker b(&animations);
        animations.clear();
        choices.clear();
        try {
            if (classes.currentIndex() < 0 ||
                classes.currentText() != classes.itemText(classes.currentIndex()))
                throw Error("Choose a source class from the list");
            choices = namedCombatAnimations(
                base, classes.currentData().toInt(),
                artworkChoice(base, classes.currentData().toInt(), variants.currentIndex()));
            int selected = 0;
            for (int i = 0; i < choices.size(); ++i) {
                animations.addItem(choices[i].name);
                if (choices[i].action == target.action && choices[i].blocked == target.blocked)
                    selected = i;
            }
            animations.setCurrentIndex(selected);
            schedule();
        } catch (const std::exception &e) {
            debounce.stop();
            candidate.reset();
            buttons.button(QDialogButtonBox::Ok)->setEnabled(false);
            status.setText(QString::fromUtf8(e.what()));
        }
    };
    QObject::connect(&classes, &QComboBox::currentIndexChanged, &dialog, reloadClasses);
    QObject::connect(&classes, &QComboBox::editTextChanged, &dialog, schedule);
    QObject::connect(&variants, &QComboBox::currentIndexChanged, &dialog, reloadClasses);
    QObject::connect(&animations, &QComboBox::currentIndexChanged, &dialog, schedule);
    QObject::connect(&source, &QComboBox::currentIndexChanged, &dialog, [&] {
        inputs.setCurrentIndex(source.currentIndex());
        fitSourcePage(inputs);
        loop.setVisible(source.currentIndex() != 1);
        imageSettings.setVisible(source.currentIndex() != 1);
        settings.setVisible(source.currentIndex() != 1 && imageSettings.isChecked());
        note.setText(
            source.currentIndex() == 1
                ? "Copies artwork, timing, movement, sound and loops. Equipment follows the "
                  "destination class; preview the result before applying."
                : "File and library imports use a fresh frame sequence. Movement already drawn "
                  "into a ZIP is not applied twice. Preview shows converted game colors.");
        if (source.currentIndex() == 1)
            reloadClasses();
        else {
            if (source.currentIndex() == 0 && fileAsset)
                origin(*fileAsset);
            if (source.currentIndex() == 2 && library.currentIndex() >= 0)
                origin(assets[library.currentData().toInt()]);
            schedule();
        }
    });
    QObject::connect(&library, &QComboBox::currentIndexChanged, &dialog, [&] {
        if (library.currentIndex() >= 0)
            origin(assets[library.currentData().toInt()]);
        schedule();
    });
    QObject::connect(&imageSettings, &QCheckBox::toggled, &settings, &QWidget::setVisible);
    for (auto v : {&originX, &originY})
        QObject::connect(v, &QSpinBox::valueChanged, &dialog, schedule);
    for (auto v : {&loop, &dither})
        QObject::connect(v, &QCheckBox::toggled, &dialog, schedule);
    QObject::connect(&buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        if (candidate)
            dialog.accept();
    });
    QObject::connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    RecoveryDraft recovery(&dialog, "Replace " + destination, [&] {
        return candidate
                   ? QJsonObject{{"privateAnimations",
                                  QJsonObject{
                                      {"sequences", candidate->animationSequences()->json()},
                                      {"overrides", candidate->animationOverrides()->json()}}}}
                   : QJsonObject{};
    });
    if (dialog.exec() != QDialog::Accepted)
        return {};
    return candidate;
}
std::optional<Project> chooseCombatAnimationFrames(Project base, int classId, int flags, int pose,
                                                   int destinationRecord, bool replace,
                                                   QWidget *parent) {
    base.loadAnimationRoutes();
    base.loadAnimationOverrides();
    base.loadCombatArtwork();
    const CombatAnimationFrameTarget target{classId, flags, pose, destinationRecord, replace};
    const auto destinationDescriptor = sourceDescriptor(base, classId, flags);
    const auto destinationRecords = destinationDescriptor.poses().effective(pose).records;
    int destinationFrame = 0, resultRecord = 0;
    for (int i = 0; i < destinationRecords.size(); ++i) {
        if (!frameRecord(destinationRecords[i]))
            continue;
        if (i <= destinationRecord)
            ++destinationFrame;
        if (destinationRecord < 0)
            resultRecord = i + 1;
    }
    if (destinationRecord >= 0) {
        if (destinationRecord >= destinationRecords.size() ||
            !frameRecord(destinationRecords[destinationRecord]))
            throw Error("Choose a destination animation frame");
        resultRecord = destinationRecord + (replace ? 0 : 1);
    } else if (replace)
        throw Error("Choose a frame to replace");
    QDialog dialog(parent);
    dialog.setObjectName("combatAnimationFramesDialog");
    dialog.setWindowTitle(replace ? "Replace frame" : "Add frames");
    dialog.resize(1020, 780);
    auto layout = new QVBoxLayout(&dialog);
    const auto destinationName = base.record(Table::Classes, classId).name;
    const auto destinationArtwork = artworkChoice(base, classId, flags);
    auto heading = new QLabel(
        QString("<b>%1 · %2</b>")
            .arg(destinationName.toHtmlEscaped(),
                 replace ? QString("Replace frame %1").arg(destinationFrame) : "Add frames"));
    layout->addWidget(heading);
    auto effect = new QLabel(
        replace
            ? QString("Replace artwork in frame %1 and keep its duration.").arg(destinationFrame)
        : destinationRecord >= 0
            ? QString("Add selected source frames after frame %1, including their timing.")
                  .arg(destinationFrame)
            : "Add selected source frames at the end of this animation, including their timing.");
    effect->setWordWrap(true);
    layout->addWidget(effect);
    const int shared = u8(base.record(Table::Classes, classId).bytes, 69);
    const int owner = shared ? shared : classId;
    auto scope = new QLabel(
        QString("%1 · %2 artwork. Animation movement, sound and loop controls are retained.%3")
            .arg(destinationArtwork & 1 ? "Enemy" : "Player",
                 destinationArtwork & 2 ? "Alternate" : "Base",
                 owner == 10 || owner == 25 || owner == 123
                     ? " Edits the selected body part; its companion animation remains."
                     : ""));
    scope->setWordWrap(true);
    layout->addWidget(scope);
    QComboBox source;
    source.setObjectName("combatFrameSource");
    addAnimationSources(source);
    auto sourceForm = new QFormLayout;
    sourceForm->addRow("Import from", &source);
    layout->addLayout(sourceForm);
    QStackedWidget inputs;
    layout->addWidget(&inputs);
    auto filesPage = new QWidget;
    auto fileLayout = new QVBoxLayout(filesPage);
    fileLayout->setContentsMargins(0, 0, 0, 0);
    auto fileRow = new QHBoxLayout;
    fileLayout->addLayout(fileRow);
    QPushButton choose("Select files…");
    choose.setObjectName("combatFrameFiles");
    fileRow->addWidget(&choose);
    QLabel fileName("PNG frames, a frame-sequence ZIP, or a sprite asset");
    fileName.setWordWrap(true);
    fileRow->addWidget(&fileName, 1);
    QSpinBox fps;
    fps.setObjectName("combatFrameFps");
    fps.setRange(1, 30);
    fps.setValue(15);
    fps.setSuffix(" frames / second");
    fps.setKeyboardTracking(false);
    auto fileForm = new QFormLayout;
    fileForm->addRow("PNG playback speed", &fps);
    fileLayout->addLayout(fileForm);
    inputs.addWidget(filesPage);
    auto gamePage = new QWidget;
    auto gameForm = new QFormLayout(gamePage);
    gameForm->setContentsMargins(0, 0, 0, 0);
    QComboBox classes, variants, animations;
    classes.setObjectName("combatFrameClass");
    variants.setObjectName("combatFrameVariant");
    animations.setObjectName("combatFrameAnimation");
    for (const auto &row : base.records(Table::Classes))
        if (row.id)
            classes.addItem(QString("%1 · %2").arg(row.id).arg(row.name), row.id);
    classes.setEditable(true);
    classes.setInsertPolicy(QComboBox::NoInsert);
    classes.completer()->setFilterMode(Qt::MatchContains);
    classes.completer()->setCompletionMode(QCompleter::PopupCompletion);
    classes.completer()->setCaseSensitivity(Qt::CaseInsensitive);
    classes.lineEdit()->setPlaceholderText("Search class name or ID…");
    classes.setCurrentIndex(classes.findData(classId));
    variants.addItems({"Player · Base", "Enemy · Base", "Player · Alternate", "Enemy · Alternate"});
    variants.setCurrentIndex(destinationArtwork);
    gameForm->addRow("Source class", &classes);
    gameForm->addRow("Artwork", &variants);
    gameForm->addRow("Animation", &animations);
    inputs.addWidget(gamePage);
    auto libraryPage = new QWidget;
    auto libraryForm = new QFormLayout(libraryPage);
    libraryForm->setContentsMargins(0, 0, 0, 0);
    QComboBox library;
    library.setObjectName("combatFrameLibrary");
    const auto assets = base.sprites().assets();
    for (int i = 0; i < assets.size(); ++i)
        library.addItem(QString("%1 · %2 %3")
                            .arg(assets[i].name)
                            .arg(assets[i].frames.size())
                            .arg(assets[i].frames.size() == 1 ? "frame" : "frames"),
                        i);
    libraryForm->addRow("Artwork / sequence", &library);
    if (assets.isEmpty()) {
        auto hint = new QLabel("Art Library is empty. Choose Files or Game animations instead.");
        hint->setWordWrap(true);
        libraryForm->addRow(hint);
    }
    inputs.addWidget(libraryPage);
    auto split = new QSplitter;
    split->setChildrenCollapsible(false);
    layout->addWidget(split, 1);
    auto framesPage = new QWidget;
    framesPage->setMinimumWidth(300);
    auto framesLayout = new QVBoxLayout(framesPage);
    framesLayout->setContentsMargins(0, 0, 0, 0);
    auto framesHeading = new QHBoxLayout;
    framesHeading->addWidget(
        new QLabel(replace ? "Choose one source frame" : "Choose source frames"), 1);
    QPushButton selectAll("Select all");
    selectAll.setObjectName("combatFrameSelectAll");
    selectAll.setVisible(!replace);
    framesHeading->addWidget(&selectAll);
    framesLayout->addLayout(framesHeading);
    QListWidget frames;
    frames.setObjectName("combatFrameChoices");
    frames.setViewMode(QListView::IconMode);
    frames.setResizeMode(QListView::Adjust);
    frames.setMovement(QListView::Static);
    frames.setIconSize({96, 96});
    frames.setGridSize({120, 150});
    frames.setSpacing(4);
    frames.setUniformItemSizes(true);
    frames.setWordWrap(true);
    frames.setTextElideMode(Qt::ElideNone);
    frames.setSelectionMode(replace ? QAbstractItemView::SingleSelection
                                    : QAbstractItemView::ExtendedSelection);
    framesLayout->addWidget(&frames, 1);
    QLabel selection;
    selection.setObjectName("combatFrameSelection");
    selection.setWordWrap(true);
    framesLayout->addWidget(&selection);
    split->addWidget(framesPage);
    auto previewPage = new QWidget;
    previewPage->setMinimumWidth(340);
    auto previewLayout = new QVBoxLayout(previewPage);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->addWidget(new QLabel("Result in " + destinationName));
    ActorPoseEditor preview(base.rom());
    preview.setObjectName("combatFramePreview");
    preview.setFrameEditing(true);
    preview.hide();
    previewLayout->addWidget(&preview, 1);
    split->addWidget(previewPage);
    split->setSizes({460, 540});
    QCheckBox imageSettings("Image settings");
    imageSettings.setObjectName("combatFrameImageSettings");
    layout->addWidget(&imageSettings);
    QWidget settings;
    auto settingsForm = new QFormLayout(&settings);
    QSpinBox anchorX, anchorY;
    anchorX.setObjectName("combatFrameOriginX");
    anchorY.setObjectName("combatFrameOriginY");
    for (auto value : {&anchorX, &anchorY}) {
        value->setRange(-32768, 32767);
        value->setKeyboardTracking(false);
    }
    settingsForm->addRow("Anchor X", &anchorX);
    settingsForm->addRow("Anchor Y", &anchorY);
    QCheckBox dither("Dither colors");
    dither.setObjectName("combatFrameDither");
    settingsForm->addRow(&dither);
    layout->addWidget(&settings);
    settings.hide();
    QLabel status;
    status.setObjectName("combatFrameStatus");
    status.setWordWrap(true);
    layout->addWidget(&status);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons.setObjectName("combatFrameButtons");
    buttons.button(QDialogButtonBox::Ok)->setText(replace ? "Replace frame" : "Add frames");
    buttons.button(QDialogButtonBox::Ok)->setEnabled(false);
    layout->addWidget(&buttons);
    QStringList paths;
    std::optional<SpriteAsset> fileAsset, displayedAsset;
    std::optional<Project> candidate;
    QVector<NamedCombatAnimation> choices;
    QTimer debounce;
    debounce.setSingleShot(true);
    debounce.setInterval(100);
    const auto selectedFrames = [&] {
        QVector<int> selected;
        for (int i = 0; i < frames.count(); ++i)
            if (frames.item(i)->isSelected())
                selected << frames.item(i)->data(Qt::UserRole).toInt();
        return selected;
    };
    const auto update = [&] {
        candidate.reset();
        buttons.button(QDialogButtonBox::Ok)->setEnabled(false);
        try {
            const auto selected = selectedFrames();
            selection.setText(replace ? "Replacement keeps the destination frame's duration."
                                      : QString("%1 frames selected · Ctrl / Shift selects several")
                                            .arg(selected.size()));
            if (source.currentIndex() == 1) {
                if (classes.currentIndex() < 0 ||
                    classes.currentText() != classes.itemText(classes.currentIndex()) ||
                    animations.currentIndex() < 0 || animations.currentIndex() >= choices.size())
                    throw Error("Choose a source class and animation");
                candidate = copyCombatAnimationFrames(
                    base, target, classes.currentData().toInt(),
                    artworkChoice(base, classes.currentData().toInt(), variants.currentIndex()),
                    choices[animations.currentIndex()].pose, selected);
            } else {
                if (!displayedAsset)
                    throw Error("Choose source artwork to preview frames");
                auto asset = *displayedAsset;
                asset.anchor = {anchorX.value(), anchorY.value()};
                candidate =
                    importCombatAnimationFrames(base, target, asset, selected, dither.isChecked());
            }
            preview.setBodyAnimations(candidate->bodyAnimationData());
            preview.setArtwork(candidate->combatArtwork()->previewEdits());
            preview.selectPrivateSequence(classId, flags, pose);
            preview.showReplacementPreview();
            const auto descriptor = candidate->animationSequences()->find(classId, flags);
            const auto records = descriptor->poses().effective(pose).records;
            preview.showFrame(frameToken(records[resultRecord]));
            preview.show();
            status.setText(
                source.currentIndex() == 1
                    ? "Native layers and equipment choices are copied. Preview the result, then "
                      "apply."
                    : "Preview shows converted game colors. Preview the result, then apply.");
            buttons.button(QDialogButtonBox::Ok)->setEnabled(true);
        } catch (const std::exception &e) {
            candidate.reset();
            preview.hide();
            status.setText(QString::fromUtf8(e.what()));
        }
    };
    const auto schedule = [&] {
        candidate.reset();
        buttons.button(QDialogButtonBox::Ok)->setEnabled(false);
        debounce.start();
    };
    QObject::connect(&debounce, &QTimer::timeout, &dialog, update);
    const auto reloadFrames = [&] {
        candidate.reset();
        displayedAsset.reset();
        buttons.button(QDialogButtonBox::Ok)->setEnabled(false);
        const QSignalBlocker blocker(&frames);
        frames.clear();
        try {
            QVector<int> records;
            if (source.currentIndex() == 1) {
                if (classes.currentIndex() < 0 ||
                    classes.currentText() != classes.itemText(classes.currentIndex()) ||
                    animations.currentIndex() < 0 || animations.currentIndex() >= choices.size())
                    throw Error("Choose a source class and animation");
                const int id = classes.currentData().toInt();
                const int route = artworkChoice(base, id, variants.currentIndex());
                const auto descriptor = sourceDescriptor(base, id, route);
                const int sourcePose = choices[animations.currentIndex()].pose;
                const auto art = sourceArt(base, id, route).withDescriptor(descriptor);
                displayedAsset = spriteAssetFromPose(
                    base.rom(), art, sourcePose, animations.currentText(), 0, {},
                    base.combatArtwork()->previewEdits().value(art.descriptorKey()));
                const auto program = descriptor.poses().effective(sourcePose).records;
                for (int i = 0; i < program.size(); ++i)
                    if (frameRecord(program[i]))
                        records << i;
            } else if (source.currentIndex() == 0) {
                if (!fileAsset)
                    throw Error("Select files to choose frames");
                displayedAsset = *fileAsset;
            } else {
                if (library.currentIndex() < 0)
                    throw Error("Choose artwork from Art Library");
                displayedAsset = assets[library.currentData().toInt()];
            }
            for (int i = 0; i < displayedAsset->frames.size(); ++i) {
                const auto image = displayedAsset->composite(i);
                const int ticks = source.currentIndex() == 1
                                      ? displayedAsset->frames[i].ticks
                                      : std::max(1, displayedAsset->frames[i].ticks) * 2;
                auto item = new QListWidgetItem(
                    QIcon(QPixmap::fromImage(
                        image.scaled(96, 96, Qt::KeepAspectRatio, Qt::FastTransformation))),
                    QString("Frame %1\n%2 ticks").arg(i + 1).arg(ticks), &frames);
                item->setData(Qt::UserRole, source.currentIndex() == 1 ? records[i] : i);
                item->setSizeHint({116, 146});
            }
            if (frames.count())
                frames.setCurrentRow(0, QItemSelectionModel::ClearAndSelect);
            if (source.currentIndex() != 1) {
                const QSignalBlocker x(&anchorX), y(&anchorY);
                anchorX.setValue(displayedAsset->anchor.x());
                anchorY.setValue(displayedAsset->anchor.y());
            }
            schedule();
        } catch (const std::exception &e) {
            debounce.stop();
            preview.hide();
            status.setText(QString::fromUtf8(e.what()));
        }
    };
    const auto reloadGame = [&] {
        const QSignalBlocker blocker(&animations);
        animations.clear();
        choices.clear();
        try {
            if (classes.currentIndex() < 0 ||
                classes.currentText() != classes.itemText(classes.currentIndex()))
                throw Error("Choose a source class from the search results");
            choices = namedCombatAnimations(
                base, classes.currentData().toInt(),
                artworkChoice(base, classes.currentData().toInt(), variants.currentIndex()));
            int selected = 0;
            for (int i = 0; i < choices.size(); ++i) {
                animations.addItem(choices[i].name);
                if (choices[i].pose == pose)
                    selected = i;
            }
            animations.setCurrentIndex(selected);
            reloadFrames();
        } catch (const std::exception &e) {
            candidate.reset();
            debounce.stop();
            preview.hide();
            buttons.button(QDialogButtonBox::Ok)->setEnabled(false);
            status.setText(QString::fromUtf8(e.what()));
        }
    };
    const auto loadFiles = [&] {
        fileAsset.reset();
        try {
            fileAsset = importAnimationFiles(paths, fps.value());
            reloadFrames();
        } catch (const std::exception &e) {
            candidate.reset();
            debounce.stop();
            frames.clear();
            preview.hide();
            buttons.button(QDialogButtonBox::Ok)->setEnabled(false);
            status.setText(QString::fromUtf8(e.what()));
        }
    };
    QObject::connect(&choose, &QPushButton::clicked, &dialog, [&] {
        const auto selected = QFileDialog::getOpenFileNames(
            &dialog, "Import frames", {},
            "Animations (*.zip *.png *.lcsprite *.json);;PNG frames (*.png);;Frame sequence ZIP "
            "(*.zip);;Sprite asset (*.lcsprite *.json)");
        if (selected.isEmpty())
            return;
        paths = selected;
        fileName.setText(paths.size() == 1
                             ? QFileInfo(paths.first()).fileName()
                             : QString("%1 PNG files · sorted by filename").arg(paths.size()));
        fitSourcePage(inputs);
        fps.setEnabled(QFileInfo(paths.first()).suffix().compare("png", Qt::CaseInsensitive) == 0);
        loadFiles();
    });
    QObject::connect(&fps, &QSpinBox::valueChanged, &dialog, [&] {
        if (!paths.isEmpty())
            loadFiles();
    });
    QObject::connect(&source, &QComboBox::currentIndexChanged, &dialog, [&] {
        inputs.setCurrentIndex(source.currentIndex());
        fitSourcePage(inputs);
        imageSettings.setVisible(source.currentIndex() != 1);
        settings.setVisible(source.currentIndex() != 1 && imageSettings.isChecked());
        if (source.currentIndex() == 1)
            reloadGame();
        else
            reloadFrames();
    });
    QObject::connect(&classes, &QComboBox::currentIndexChanged, &dialog, reloadGame);
    QObject::connect(&classes, &QComboBox::editTextChanged, &dialog, reloadFrames);
    QObject::connect(&variants, &QComboBox::currentIndexChanged, &dialog, reloadGame);
    QObject::connect(&animations, &QComboBox::currentIndexChanged, &dialog, reloadFrames);
    QObject::connect(&library, &QComboBox::currentIndexChanged, &dialog, reloadFrames);
    QObject::connect(&frames, &QListWidget::itemSelectionChanged, &dialog, schedule);
    QObject::connect(&selectAll, &QPushButton::clicked, &frames, &QListWidget::selectAll);
    QObject::connect(&imageSettings, &QCheckBox::toggled, &settings, &QWidget::setVisible);
    for (auto value : {&anchorX, &anchorY})
        QObject::connect(value, &QSpinBox::valueChanged, &dialog, schedule);
    QObject::connect(&dither, &QCheckBox::toggled, &dialog, schedule);
    QObject::connect(&buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        if (candidate)
            dialog.accept();
    });
    QObject::connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    RecoveryDraft recovery(
        &dialog,
        (replace ? "Replace animation frame · " : "Add animation frames · ") + destinationName,
        [&] {
            return candidate
                       ? QJsonObject{{"privateAnimations",
                                      QJsonObject{
                                          {"sequences", candidate->animationSequences()->json()},
                                          {"overrides", candidate->animationOverrides()->json()}}}}
                       : QJsonObject{};
        });
    source.setCurrentIndex(1);
    if (dialog.exec() != QDialog::Accepted)
        return {};
    return candidate;
}
} // namespace lordly
