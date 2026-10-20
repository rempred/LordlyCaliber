#include "app/combat_animation_replace.h"
#include "app/actor_pose_editor.h"
#include "app/recovery_draft.h"
#include "engine/animation_file_import.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
namespace lordly {
namespace {
int artworkChoice(Project &p, int id, int flags) {
    const int value = u8(p.record(Table::Classes, id).bytes, 69), owner = value ? value : id;
    return (flags & 1) && (owner == 15 || owner == 30) ? flags ^ 2 : flags;
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
    source.addItems({"Files", "Another class", "Art Library"});
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
        library.addItem(QString("%1 · %2 %3").arg(assets[i].name).arg(assets[i].frames.size())
                            .arg(assets[i].frames.size() == 1 ? "frame" : "frames"),
                        i);
    libraryForm->addRow("Sequence", &library);
    QLabel libraryHint(assets.isEmpty()
                           ? "Art Library is empty. Choose Files or Another class instead."
                           : "Imports the sequence's artwork, placement and frame timing.");
    libraryHint.setWordWrap(true);
    libraryForm->addRow(&libraryHint);
    inputs.addWidget(libraryPage);
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
} // namespace lordly
