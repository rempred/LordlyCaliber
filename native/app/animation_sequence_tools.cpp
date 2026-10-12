#include "app/animation_sequence_editor.h"
#include "app/actor_pose_editor.h"
#include "engine/animation_transfer.h"
#include "engine/sprite_sources.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLabel>
#include <QPen>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
namespace lordly {
namespace {
ActorArt body(Project &project, int id, int flags) {
    const int copy = u8(project.record(Table::Classes, id).bytes, 69);
    return ActorArt::loadBody(project.rom(), id, copy ? copy : id, flags >> 1, flags & 1, project.bodyAnimationData());
}
AnimationDescriptor sourceDescriptor(Project &project, int id, int flags, const ActorArt &art) {
    if (const auto original = project.animationSequences()->find(id, flags)) return *original;
    return AnimationDescriptor::load(project.rom(), art.descriptorKey(), project.combatArtwork()->previewEdits().value(art.descriptorKey()));
}
SpriteAsset frames(Project &project, int id, int flags, int pose) {
    const auto art = body(project, id, flags);
    return spriteAssetFromPose(project.rom(), art, pose,
                               QString("%1 · pose %2").arg(project.record(Table::Classes, id).name).arg(pose),
                               0, {}, project.combatArtwork()->previewEdits().value(art.descriptorKey()));
}
void compareFrames(QWidget *parent, const SpriteAsset &left, const SpriteAsset &right) {
    QDialog dialog(parent);
    dialog.setObjectName("sequenceComparison");
    dialog.setWindowTitle("Compare frame sequences"); dialog.resize(950, 640);
    auto layout = new QVBoxLayout(&dialog);
    auto note = new QLabel("Synchronized frames and timing in common sprite coordinates. Crosshairs mark the "
                           "same origin. Movement, sound and other game control instructions are not played here.");
    note->setWordWrap(true); layout->addWidget(note);
    const std::array<SpriteAsset, 2> assets{left, right};
    std::array<QGraphicsScene *, 2> scenes;
    std::array<QGraphicsView *, 2> views;
    std::array<QGraphicsPixmapItem *, 2> items;
    std::array<QLabel *, 2> labels;
    std::array<int, 2> durations{};
    QRectF bounds;
    auto panes = new QHBoxLayout; layout->addLayout(panes, 1);
    for (int i = 0; i < 2; ++i) {
        bounds |= QRectF(-assets[i].anchor, assets[i].size);
        for (const auto &frame : assets[i].frames) durations[i] += frame.ticks;
        durations[i] = std::max(1, durations[i]);
        auto column = new QVBoxLayout; panes->addLayout(column, 1);
        labels[i] = new QLabel; labels[i]->setWordWrap(true); column->addWidget(labels[i]);
        scenes[i] = new QGraphicsScene(&dialog);
        views[i] = new QGraphicsView(scenes[i]);
        views[i]->setObjectName(i ? "sequenceCompareRight" : "sequenceCompareLeft");
        views[i]->setBackgroundBrush(QColor("#272c35"));
        column->addWidget(views[i], 1);
        items[i] = scenes[i]->addPixmap(QPixmap());
        items[i]->setPos(-assets[i].anchor);
    }
    bounds |= QRectF(-1, -1, 2, 2); bounds = bounds.adjusted(-8, -8, 8, 8);
    for (auto scene : scenes) {
        scene->setSceneRect(bounds);
        auto horizontal = scene->addLine(bounds.left(), 0, bounds.right(), 0, QPen(QColor("#809090")));
        auto vertical = scene->addLine(0, bounds.top(), 0, bounds.bottom(), QPen(QColor("#809090")));
        horizontal->setZValue(1); vertical->setZValue(1);
    }
    auto controls = new QHBoxLayout; layout->addLayout(controls);
    QPushButton play("Play"); play.setObjectName("sequenceComparePlay"); play.setCheckable(true);
    QPushButton reset("Restart"), fit("Fit both"), actual("Actual pixels");
    QSlider tick(Qt::Horizontal); tick.setObjectName("sequenceCompareTick");
    tick.setRange(0, std::max(durations[0], durations[1]) - 1);
    controls->addWidget(&play); controls->addWidget(&reset); controls->addWidget(&tick, 1);
    controls->addWidget(&fit); controls->addWidget(&actual);
    QTimer timer;
    qint64 elapsed = 0;
    auto draw = [&] {
        for (int i = 0; i < 2; ++i) {
            int remaining = int(elapsed % durations[i]), index = 0;
            while (index + 1 < assets[i].frames.size() && remaining >= assets[i].frames[index].ticks)
                remaining -= assets[i].frames[index++].ticks;
            items[i]->setPixmap(QPixmap::fromImage(assets[i].composite(index)));
            labels[i]->setText(QString("%1 · frame %2 / %3 · tick %4")
                                  .arg(assets[i].name).arg(index + 1).arg(assets[i].frames.size()).arg(elapsed));
            views[i]->setProperty("frameIndex", index);
        }
    };
    auto fitBoth = [&] {
        const double scale = std::min({views[0]->viewport()->width() / bounds.width(),
                                      views[1]->viewport()->width() / bounds.width(),
                                      views[0]->viewport()->height() / bounds.height(),
                                      views[1]->viewport()->height() / bounds.height()}) * .95;
        for (auto view : views) { view->setTransform(QTransform::fromScale(scale, scale)); view->centerOn(bounds.center()); }
    };
    QObject::connect(&fit, &QPushButton::clicked, &dialog, fitBoth);
    QObject::connect(&actual, &QPushButton::clicked, &dialog, [&] {
        for (auto view : views) { view->resetTransform(); view->centerOn(bounds.center()); }
    });
    QObject::connect(&tick, &QSlider::valueChanged, &dialog, [&](int value) { elapsed = value; draw(); });
    QObject::connect(&reset, &QPushButton::clicked, &dialog, [&] { elapsed = 0; tick.setValue(0); draw(); });
    QObject::connect(&play, &QPushButton::toggled, &dialog, [&](bool on) {
        play.setText(on ? "Pause" : "Play"); if (on) timer.start(16); else timer.stop();
    });
    QObject::connect(&timer, &QTimer::timeout, &dialog, [&] {
        elapsed += 2;
        const QSignalBlocker blocker(&tick); tick.setValue(int(elapsed % (tick.maximum() + 1))); draw();
    });
    QDialogButtonBox close(QDialogButtonBox::Close); layout->addWidget(&close);
    QObject::connect(&close, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    draw(); QTimer::singleShot(0, &dialog, fitBoth); dialog.exec();
}
} // namespace
void AnimationSequenceEditor::copySequence(int sourceClass, int sourceRoute, int sourcePose, bool controls) {
    project_->loadCombatArtwork();
    const int id = selectedClass(), flags = routes_->currentIndex(), state = poses_->currentData().toInt();
    const auto art = body(*project_, sourceClass, sourceRoute);
    const auto source = sourceDescriptor(*project_, sourceClass, sourceRoute, art);
    auto next = *project_->animationSequences();
    if (!next.find(id, flags)) next.create(id, flags, *project_->animationRoutes(), project_->combatArtwork()->previewEdits());
    next.setDescriptor(id, flags, copyAnimationPose(project_->rom(), *next.find(id, flags), state, source, art, sourcePose, controls));
    apply(std::move(next), "Copy animation sequence from class");
}
void AnimationSequenceEditor::copyFrame(int sourceClass, int sourceRoute, int sourcePose, int sourceRecord, bool duration) {
    project_->loadCombatArtwork();
    const int id = selectedClass(), flags = routes_->currentIndex(), state = poses_->currentData().toInt();
    const auto art = body(*project_, sourceClass, sourceRoute);
    const auto source = sourceDescriptor(*project_, sourceClass, sourceRoute, art);
    auto next = editableSequences();
    if (!next.find(id, flags)) throw Error("Make a private copy before replacing frame art");
    next.setDescriptor(id, flags, copyAnimationFrame(project_->rom(), *next.find(id, flags), state, records_->currentRow(),
                                                    source, art, sourcePose, sourceRecord, duration));
    apply(std::move(next), duration ? "Copy frame art and duration" : "Copy frame art");
}
void AnimationSequenceEditor::translateFrames(const QMap<int, QSet<int>> &records, QPoint offset, int layer, bool sceneSpace) {
    if (offset.isNull() || records.isEmpty()) return;
    const int id = selectedClass(), flags = routes_->currentIndex();
    auto next = editableSequences();
    const auto original = next.find(id, flags);
    if (!original) throw Error("Make a private copy before translating its frames");
    next.setDescriptor(id, flags, translateAnimationFrames(*original, records, offset, layer, sceneSpace));
    apply(std::move(next), "Translate animation frame layers");
}
void AnimationSequenceEditor::chooseSource(bool compare, int destinationRecord, bool duration) {
    if (destinationRecord >= 0) {
        const auto records = descriptor().poses().effective(poses_->currentData().toInt()).records;
        if (destinationRecord >= records.size() || (records[destinationRecord].opcode != 1 && records[destinationRecord].opcode != 21))
            throw Error("Choose a destination frame instruction");
    }
    project_->loadCombatArtwork();
    QDialog dialog(this); dialog.setObjectName("sequenceSourceDialog");
    dialog.setWindowTitle(compare ? "Choose comparison sequence" : "Copy complete sequence from class");
    dialog.resize(900, 800);
    auto layout = new QVBoxLayout(&dialog);
    auto choices = new QHBoxLayout; layout->addLayout(choices);
    QComboBox classes, routes, poses;
    classes.setObjectName("sequenceSourceClass"); routes.setObjectName("sequenceSourceRoute"); poses.setObjectName("sequenceSourcePose");
    for (const auto &row : project_->records(Table::Classes)) if (row.id > 0)
        classes.addItem(QString("%1 · %2").arg(row.id).arg(row.name), row.id);
    classes.setCurrentIndex(classes.findData(selectedClass()));
    routes.addItems({"Body 0 / 0", "Body 0 / 1", "Body 1 / 0", "Body 1 / 1"});
    routes.setCurrentIndex(routes_->currentIndex());
    choices->addWidget(&classes, 2); choices->addWidget(&routes); choices->addWidget(&poses);
    ActorPoseEditor preview(project_->rom()); preview.setBodyAnimations(project_->bodyAnimationData());
    preview.setArtwork(project_->combatArtwork()->previewEdits()); layout->addWidget(&preview, 1);
    QComboBox sourceFrame; sourceFrame.setObjectName("sequenceSourceFrame");
    QLabel framePreview; framePreview.setAlignment(Qt::AlignCenter); framePreview.setMinimumSize(256, 256);
    if (destinationRecord >= 0) {
        preview.hide(); layout->addWidget(&sourceFrame); layout->addWidget(&framePreview);
        dialog.setWindowTitle(duration ? "Copy frame art and duration" : "Copy frame art");
        dialog.resize(650, 500);
    }
    QCheckBox controls("Copy source motion, sound, loops and dependent poses");
    controls.setObjectName("sequenceCopyControls"); controls.setChecked(true);
    controls.setToolTip("Unchecked: copy frames and timing while keeping the destination's controls.");
    if (!compare && destinationRecord < 0) layout->addWidget(&controls);
    QLabel status; status.setWordWrap(true); layout->addWidget(&status);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons.setObjectName("sequenceSourceButtons");
    buttons.button(QDialogButtonBox::Ok)->setText(compare ? "Compare" : "Copy and replace"); layout->addWidget(&buttons);
    auto showFrame = [&] {
        try {
            const auto asset = frames(*project_, classes.currentData().toInt(), routes.currentIndex(), poses.currentData().toInt());
            framePreview.setPixmap(QPixmap::fromImage(asset.composite(sourceFrame.currentIndex()).scaled(320, 320, Qt::KeepAspectRatio, Qt::FastTransformation)));
        } catch (const std::exception &e) { status.setText(QString::fromUtf8(e.what())); }
    };
    auto select = [&] {
        try {
            const auto art = body(*project_, classes.currentData().toInt(), routes.currentIndex());
            const auto bank = sourceDescriptor(*project_, classes.currentData().toInt(), routes.currentIndex(), art).poses();
            const int pose = poses.currentData().toInt();
            bool hasFrames = false;
            for (const auto &record : bank.effective(pose).records) hasFrames |= record.opcode == 1 || record.opcode == 21;
            if (!hasFrames) throw Error("Choose a pose containing frames");
            preview.selectPrivateSequence(classes.currentData().toInt(), routes.currentIndex(), pose);
            status.clear(); buttons.button(QDialogButtonBox::Ok)->setEnabled(true);
            if (destinationRecord >= 0) {
                const QSignalBlocker blocker(&sourceFrame); sourceFrame.clear();
                const auto records = bank.effective(pose).records;
                for (int i = 0; i < records.size(); ++i) if (records[i].opcode == 1 || records[i].opcode == 21)
                    sourceFrame.addItem(QString("Frame %1 · %2 ticks").arg(sourceFrame.count() + 1).arg(u8(records[i].operands, records[i].operands.size() - 1)), i);
                showFrame();
            }
        } catch (const std::exception &e) { status.setText(QString::fromUtf8(e.what())); buttons.button(QDialogButtonBox::Ok)->setEnabled(false); }
    };
    auto reload = [&] {
        try {
            const int previous = poses.currentIndex() < 0 ? poses_->currentData().toInt() : poses.currentData().toInt();
            const QSignalBlocker blocker(&poses); poses.clear();
            const auto art = body(*project_, classes.currentData().toInt(), routes.currentIndex());
            for (int pose = 0; pose < art.poses().size(); ++pose) poses.addItem(QString("Pose %1").arg(pose), pose);
            poses.setCurrentIndex(std::max(0, poses.findData(previous)));
            select();
        } catch (const std::exception &e) { status.setText(QString::fromUtf8(e.what())); buttons.button(QDialogButtonBox::Ok)->setEnabled(false); }
    };
    connect(&classes, &QComboBox::currentIndexChanged, &dialog, reload);
    connect(&routes, &QComboBox::currentIndexChanged, &dialog, reload);
    connect(&poses, &QComboBox::currentIndexChanged, &dialog, select);
    connect(&sourceFrame, &QComboBox::currentIndexChanged, &dialog, showFrame);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    reload(); if (dialog.exec() != QDialog::Accepted) return;
    if (compare) compareFrames(this, frames(*project_, selectedClass(), routes_->currentIndex(), poses_->currentData().toInt()),
                                frames(*project_, classes.currentData().toInt(), routes.currentIndex(), poses.currentData().toInt()));
    else if (destinationRecord >= 0)
        copyFrame(classes.currentData().toInt(), routes.currentIndex(), poses.currentData().toInt(), sourceFrame.currentData().toInt(), duration);
    else copySequence(classes.currentData().toInt(), routes.currentIndex(), poses.currentData().toInt(), controls.isChecked());
}
void AnimationSequenceEditor::chooseTranslation() {
    const int state = poses_->currentData().toInt();
    if (!project_->animationSequences()->find(selectedClass(), routes_->currentIndex()))
        throw Error("Make a private copy before translating its frames");
    const auto original = descriptor();
    const auto bank = original.poses();
    QSet<int> selected;
    for (const auto &row : records_->selectionModel()->selectedRows()) {
        const auto record = bank.effective(state).records[row.row()];
        if (record.opcode == 1 || record.opcode == 21) selected.insert(row.row());
    }
    QDialog dialog(this); dialog.setObjectName("sequenceTranslateDialog"); dialog.setWindowTitle("Translate animation frames");
    auto layout = new QVBoxLayout(&dialog);
    QComboBox scope; scope.addItems({"Selected frame rows in current pose", "All frames in checked poses"});
    scope.setObjectName("sequenceTranslateScope"); layout->addWidget(&scope);
    QListWidget poses; poses.setObjectName("sequenceTranslatePoses"); layout->addWidget(&poses);
    for (int pose = 0; pose < bank.programs().size(); ++pose) {
        bool visible = false;
        try { for (const auto &r : bank.effective(pose).records) visible |= r.opcode == 1 || r.opcode == 21; }
        catch (const Error &) { continue; }
        if (!visible) continue;
        auto item = new QListWidgetItem(QString("Pose %1").arg(pose), &poses);
        item->setData(Qt::UserRole, pose); item->setCheckState(pose == state ? Qt::Checked : Qt::Unchecked);
    }
    poses.setEnabled(false);
    connect(&scope, &QComboBox::currentIndexChanged, &dialog, [&](int index) { poses.setEnabled(index == 1); });
    if (selected.isEmpty()) scope.setCurrentIndex(1);
    auto form = new QFormLayout; layout->addLayout(form);
    QSpinBox x, y, layer;
    x.setObjectName("sequenceTranslateX"); y.setObjectName("sequenceTranslateY"); layer.setObjectName("sequenceTranslateLayer");
    x.setRange(-32768, 32767); y.setRange(-32768, 32767); layer.setRange(-1, 4095); layer.setValue(-1); layer.setSpecialValueText("All layers");
    form->addRow("Move X", &x); form->addRow("Move Y", &y); form->addRow("Layer index", &layer);
    QLabel status("Offsets use local sprite coordinates. Unselected frames keep their artwork. "
                  "Substitution poses 38/88 can also affect artwork displayed by other poses.");
    status.setWordWrap(true); layout->addWidget(&status);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons.setObjectName("sequenceTranslateButtons"); buttons.button(QDialogButtonBox::Ok)->setText("Translate frames");
    auto preview = buttons.addButton("Preview movement", QDialogButtonBox::ActionRole); layout->addWidget(&buttons);
    const auto selection = [&] {
        QMap<int, QSet<int>> result;
        if (!scope.currentIndex()) result[state] = selected;
        else for (int i = 0; i < poses.count(); ++i) if (poses.item(i)->checkState() == Qt::Checked) {
            const int pose = poses.item(i)->data(Qt::UserRole).toInt();
            const auto records = bank.effective(pose).records;
            for (int j = 0; j < records.size(); ++j) if (records[j].opcode == 1 || records[j].opcode == 21) result[pose].insert(j);
        }
        if (result.isEmpty() || (result.size() == 1 && result.first().isEmpty())) throw Error("Choose frames or poses to translate");
        return result;
    };
    connect(preview, &QPushButton::clicked, &dialog, [&] {
        try {
            const auto selected = selection(); const int pose = selected.firstKey();
            const auto art = body(*project_, selectedClass(), routes_->currentIndex());
            const auto changed = art.withDescriptor(translateAnimationFrames(original, selected, {x.value(), y.value()}, layer.value()));
            compareFrames(&dialog, frames(*project_, selectedClass(), routes_->currentIndex(), pose),
                          spriteAssetFromPose(project_->rom(), changed, pose, "After translation"));
        } catch (const std::exception &e) { status.setText(QString::fromUtf8(e.what())); }
    });
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        try { translateFrames(selection(), {x.value(), y.value()}, layer.value()); dialog.accept(); }
        catch (const std::exception &e) { status.setText(QString::fromUtf8(e.what())); }
    });
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
}
} // namespace lordly
