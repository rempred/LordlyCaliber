#include "app/actor_pose_editor.h"
#include "engine/class_data.h"
#include "engine/scene_transforms.h"
#include "engine/text.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <bit>
#include <cmath>
#include <numbers>
namespace lordly {
namespace {
struct PreviewPose {
    int state, cursor, delay, frame;
    double x, y, z;
    QByteArray material;
    qsizetype audio;
};
PreviewPose preview(const std::variant<ActorPosePlayer, SpritePosePlayer> &player) {
    return std::visit(
        [](const auto &runtime) -> PreviewPose {
            if constexpr (std::is_same_v<std::decay_t<decltype(runtime)>, SpritePosePlayer>) {
                const auto &a = runtime.sprite();
                return {a.state,
                        a.cursor,
                        a.delay,
                        a.frame,
                        double(a.x),
                        double(a.y),
                        0,
                        QByteArray(reinterpret_cast<const char *>(a.color), 16),
                        runtime.audioRequests().size()};
            } else {
                const auto &a = runtime.actor();
                return {a.state,
                        a.cursor,
                        a.delay,
                        a.frame,
                        a.position.x,
                        a.position.y,
                        a.position.z,
                        QByteArray(reinterpret_cast<const char *>(a.material), 16),
                        runtime.audioRequests().size()};
            }
        },
        player);
}
class PoseView : public QGraphicsView {
  public:
    using QGraphicsView::QGraphicsView;

  protected:
    void wheelEvent(QWheelEvent *event) override {
        const double factor = event->angleDelta().y() > 0 ? 1.2 : 1 / 1.2;
        const double next = transform().m11() * factor;
        if (next >= .05 && next <= 64)
            scale(factor, factor);
        event->accept();
    }
};
} // namespace
ActorPoseEditor::ActorPoseEditor(const Rom &rom, QWidget *parent)
    : QWidget(parent), rom_(rom), renderer_(rom) {
    auto layout = new QVBoxLayout(this);
    auto note = new QLabel(
        "Pose inspection with per-layer opacity and optional sepia. Playback speed is adjustable; "
        "full-scene timing, cameras and lighting are separate. Sound requests are recorded without "
        "audio output. Class-body inspection uses no linked equipment actors.");
    note->setWordWrap(true);
    layout->addWidget(note);
    auto selectors = new QHBoxLayout;
    kinds_ = new QComboBox;
    kinds_->setObjectName("actorSourceKind");
    kinds_->addItems({"Cutscene actors", "Class bodies", "2D scene sprites"});
    owners_ = new QComboBox;
    owners_->setObjectName("actorOwner");
    flags_ = new QComboBox;
    flags_->setObjectName("actorBodyFlags");
    flags_->addItems({"0 / 0", "0 / 1", "1 / 0", "1 / 1"});
    banks_ = new QComboBox;
    banks_->setObjectName("actorBank");
    banks_->setMinimumContentsLength(20);
    banks_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    poses_ = new QComboBox;
    poses_->setObjectName("actorPose");
    poses_->setMinimumContentsLength(28);
    variant_ = new QSpinBox;
    variant_->setObjectName("actorVariant");
    variant_->setRange(0, 255);
    sepia_ = new QCheckBox("Sepia");
    sepia_->setObjectName("actorSepia");
    sepia_->setToolTip(
        "Inspect the game's sepia image conversion. Scene focus and cache history are separate.");
    selectors->addWidget(kinds_);
    selectors->addWidget(banks_);
    selectors->addWidget(new QLabel("Owner class"));
    selectors->addWidget(owners_, 1);
    selectors->addWidget(new QLabel("Body flags"));
    selectors->addWidget(flags_);
    layout->addLayout(selectors);
    auto poseSelectors = new QHBoxLayout;
    poseSelectors->addWidget(new QLabel("Pose"));
    poseSelectors->addWidget(poses_, 1);
    poseSelectors->addWidget(new QLabel("Image variant"));
    poseSelectors->addWidget(variant_);
    poseSelectors->addWidget(sepia_);
    layout->addLayout(poseSelectors);
    auto controls = new QHBoxLayout;
    play_ = new QPushButton("Play");
    play_->setObjectName("actorPlay");
    play_->setCheckable(true);
    auto next = new QPushButton("Step"), reset = new QPushButton("Restart"),
         fit = new QPushButton("Fit"), actual = new QPushButton("Actual pixels");
    next->setObjectName("actorStep");
    reset->setObjectName("actorRestart");
    position_ = new QSpinBox;
    position_->setObjectName("actorPosition");
    position_->setRange(0, 10000);
    speed_ = new QSpinBox;
    speed_->setRange(1, 120);
    speed_->setValue(30);
    speed_->setSuffix(" steps/s");
    controls->addWidget(play_);
    controls->addWidget(next);
    controls->addWidget(reset);
    controls->addWidget(new QLabel("Step"));
    controls->addWidget(position_);
    controls->addWidget(speed_);
    controls->addStretch();
    controls->addWidget(fit);
    controls->addWidget(actual);
    layout->addLayout(controls);
    projected_ = new QCheckBox("Projected placement");
    projected_->setObjectName("actorProjected");
    projected_->setToolTip("Inspect native mode-zero Actor matrices with a reference camera. Scene "
                           "commands and terrain are not executed here.");
    layout->addWidget(projected_);
    placementControls_ = new QWidget;
    auto placementLayout = new QVBoxLayout(placementControls_);
    placementLayout->setContentsMargins(0, 0, 0, 0);
    auto placementRow = new QHBoxLayout;
    auto number = [this](QHBoxLayout *row, const QString &label, const QString &name, double low,
                         double high, double value) {
        row->addWidget(new QLabel(label));
        auto spin = new QDoubleSpinBox;
        spin->setObjectName(name);
        spin->setRange(low, high);
        spin->setDecimals(3);
        spin->setValue(value);
        spin->setKeyboardTracking(false);
        row->addWidget(spin);
        connect(spin, &QDoubleSpinBox::valueChanged, this, &ActorPoseEditor::draw);
        return spin;
    };
    for (int i = 0; i < 3; ++i)
        placement_[i] = number(placementRow, QString("XYZ").mid(i, 1),
                               QString("actorPlacement%1").arg(i), -10000, 10000, 0);
    size_ = number(placementRow, "Scale", "actorPlacementScale", 0.01, 20, 1);
    placementLayout->addLayout(placementRow);
    auto cameraRow = new QHBoxLayout;
    cameraYaw_ = number(cameraRow, "Camera angle", "actorCameraYaw", -180, 180, 0);
    cameraHeight_ = number(cameraRow, "Height", "actorCameraHeight", -2000, 2000, 80);
    cameraDistance_ = number(cameraRow, "Distance", "actorCameraDistance", 1, 2000, 300);
    cameraRow->addWidget(new QLabel("Facing"));
    facing_ = new QSpinBox;
    facing_->setObjectName("actorPlacementFacing");
    facing_->setRange(0, 9);
    cameraRow->addWidget(facing_);
    shadows_ = new QCheckBox("Shadow");
    shadows_->setChecked(true);
    shadows_->setObjectName("actorPlacementShadow");
    cameraRow->addWidget(shadows_);
    placementLayout->addLayout(cameraRow);
    auto placementNote = new QLabel(
        "Reference camera and preview position only. Native main/shadow matrices; Qt image "
        "sampling. No scene lighting, custom material combiners or linked equipment.");
    placementNote->setWordWrap(true);
    placementLayout->addWidget(placementNote);
    layout->addWidget(placementControls_);
    placementControls_->hide();
    connect(projected_, &QCheckBox::toggled, this, [this](bool enabled) {
        placementControls_->setVisible(enabled);
        variant_->setEnabled(!(enabled && kinds_->currentIndex() == 1));
        fit_ = true;
        draw();
    });
    connect(facing_, &QSpinBox::valueChanged, this, &ActorPoseEditor::draw);
    connect(shadows_, &QCheckBox::toggled, this, &ActorPoseEditor::draw);
    auto split = new QSplitter;
    canvas_ = new QGraphicsScene(this);
    view_ = new PoseView(canvas_);
    view_->setObjectName("actorCanvas");
    view_->setDragMode(QGraphicsView::ScrollHandDrag);
    view_->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    QPixmap checker(24, 24);
    checker.fill(QColor("#252931"));
    {
        QPainter p(&checker);
        p.fillRect(0, 0, 12, 12, QColor("#2e333c"));
        p.fillRect(12, 12, 12, 12, QColor("#2e333c"));
    }
    view_->setBackgroundBrush(checker);
    split->addWidget(view_);
    auto layerPanel = new QWidget;
    auto layerLayout = new QVBoxLayout(layerPanel);
    layerLayout->addWidget(new QLabel("Frame layers"));
    layers_ = new QListWidget;
    layers_->setObjectName("actorLayers");
    layerLayout->addWidget(layers_, 1);
    selection_ = new QLabel;
    selection_->setWordWrap(true);
    layerLayout->addWidget(selection_);
    split->addWidget(layerPanel);
    split->setStretchFactor(0, 4);
    split->setStretchFactor(1, 1);
    layout->addWidget(split, 1);
    status_ = new QLabel;
    status_->setObjectName("actorStatus");
    status_->setWordWrap(true);
    layout->addWidget(status_);
    timer_ = new QTimer(this);
    timer_->setTimerType(Qt::PreciseTimer);
    connect(timer_, &QTimer::timeout, this, &ActorPoseEditor::step);
    connect(play_, &QPushButton::toggled, this, [this](bool playing) {
        if (playing && player_) {
            timer_->start(qMax(1, qRound(1000. / speed_->value())));
            play_->setText("Pause");
        } else {
            timer_->stop();
            play_->setText("Play");
        }
    });
    connect(speed_, &QSpinBox::valueChanged, this, [this] {
        if (timer_->isActive())
            timer_->setInterval(qMax(1, qRound(1000. / speed_->value())));
    });
    connect(next, &QPushButton::clicked, this, [this] {
        stop();
        step();
    });
    connect(reset, &QPushButton::clicked, this, &ActorPoseEditor::restart);
    connect(position_, &QSpinBox::valueChanged, this, &ActorPoseEditor::seek);
    connect(banks_, &QComboBox::currentIndexChanged, this, &ActorPoseEditor::loadBank);
    connect(kinds_, &QComboBox::currentIndexChanged, this, &ActorPoseEditor::loadSources);
    connect(owners_, &QComboBox::currentIndexChanged, this, &ActorPoseEditor::loadBank);
    connect(flags_, &QComboBox::currentIndexChanged, this, &ActorPoseEditor::loadBank);
    connect(poses_, &QComboBox::currentIndexChanged, this, &ActorPoseEditor::restart);
    connect(variant_, &QSpinBox::valueChanged, this, &ActorPoseEditor::restart);
    connect(sepia_, &QCheckBox::toggled, this, &ActorPoseEditor::draw);
    connect(fit, &QPushButton::clicked, this,
            [this] { view_->fitInView(canvas_->sceneRect(), Qt::KeepAspectRatio); });
    connect(actual, &QPushButton::clicked, this, [this] { view_->resetTransform(); });
    connect(layers_, &QListWidget::currentRowChanged, this, [this](int row) {
        for (int i = 0; i < items_.size(); ++i)
            items_[i]->setSelected(i == row);
        selection_->setText(row >= 0 && layers_->item(row) ? layers_->item(row)->toolTip()
                                                           : QString{});
    });
    connect(layers_, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
        const int row = layers_->row(item);
        if (row >= 0 && row < items_.size())
            items_[row]->setVisible(item->checkState() == Qt::Checked);
    });
    connect(canvas_, &QGraphicsScene::selectionChanged, this, [this] {
        for (int i = 0; i < items_.size(); ++i)
            if (items_[i]->isSelected()) {
                QSignalBlocker block(layers_);
                layers_->setCurrentRow(i);
                selection_->setText(layers_->item(i)->toolTip());
                break;
            }
    });
    loadSources();
}
void ActorPoseEditor::selectCommand(const DirectorProgram &program, int command) {
    stop();
    try {
        if (command < 0 || command >= program.commands.size() ||
            program.commands[command].opcode != 0x14 ||
            program.commands[command].operands.size() != 9)
            throw Error("Select an Actor creation command");
        const auto &args = program.commands[command].operands;
        const int bank = qint32(args[1]), animation = qint32(args[2]), facing = qint32(args[3]);
        if (facing < 0 || facing > 9)
            throw Error("Actor placement requires a known facing in 0..9");
        const auto art = ActorArt::load(rom_, bank);
        const int state = art.resolve(animation, facing);
        for (int i = 0; i < 3; ++i) {
            const double value = double(qint32(args[4 + i])) / 1000.;
            if (value < placement_[i]->minimum() || value > placement_[i]->maximum())
                throw Error("Actor command position exceeds the placement inspector range");
        }
        kinds_->setCurrentIndex(0);
        banks_->setCurrentIndex(banks_->findData(bank));
        poses_->setCurrentIndex(poses_->findData(state));
        variant_->setValue(quint8(args[8]));
        for (int i = 0; i < 3; ++i) {
            const QSignalBlocker block(placement_[i]);
            placement_[i]->setValue(double(qint32(args[4 + i])) / 1000.);
        }
        {
            const QSignalBlocker block(facing_);
            facing_->setValue(facing);
        }
        size_->setValue(1.);
        const bool modeZero = program.resourceClass() == 4 || program.resourceClass() == 5;
        projected_->setChecked(modeZero);
        restart();
        {
            const QSignalBlocker block(facing_);
            facing_->setValue(facing);
        }
        seek(1);
        status_->setText(
            status_->text() +
            QString(" Actor creation at word %1. %2")
                .arg(program.commands[command].word)
                .arg(modeZero ? "Command coordinates use the reference camera above; scene cameras "
                                "and terrain are separate."
                              : "Pose inspection only; this scene's placement path is separate."));
        setProperty("inspectedActorCommand", command);
    } catch (const std::exception &e) {
        player_.reset();
        items_.clear();
        canvas_->clear();
        layers_->clear();
        setProperty("placementFrame", QImage());
        setProperty("inspectedActorCommand", -1);
        status_->setText(QString::fromUtf8(e.what()));
    }
}
void ActorPoseEditor::stop() {
    play_->setChecked(false);
    timer_->stop();
}
void ActorPoseEditor::loadSources() {
    stop();
    const bool body = kinds_->currentIndex() == 1;
    projected_->setEnabled(kinds_->currentIndex() != 2);
    if (kinds_->currentIndex() == 2)
        projected_->setChecked(false);
    variant_->setEnabled(!(projected_->isChecked() && body));
    owners_->setEnabled(body);
    flags_->setEnabled(body);
    try {
        QSignalBlocker banksBlock(banks_), ownerBlock(owners_);
        banks_->clear();
        owners_->clear();
        owners_->addItem("Same as source", -1);
        if (body) {
            const auto classes = ClassData::load(rom_);
            for (int i = 0; i < classes.entries().size(); ++i) {
                const auto label =
                    QString("%1 · %2").arg(i).arg(decodeText(classes.entries()[i].name));
                banks_->addItem(label, i);
                owners_->addItem(label, i);
            }
            banks_->setCurrentIndex(banks_->findData(1));
        } else {
            for (int i = 1; i <= ActorArt::bankCount(rom_); ++i)
                banks_->addItem(QString("Bank %1").arg(i), i);
        }
    } catch (const Error &e) {
        status_->setText(QString::fromUtf8(e.what()));
        return;
    }
    loadBank();
}
void ActorPoseEditor::loadBank() {
    stop();
    art_.reset();
    player_.reset();
    try {
        const int source = banks_->currentData().toInt(), owner = owners_->currentData().toInt();
        const bool body = kinds_->currentIndex() == 1;
        art_ = body ? ActorArt::loadBody(rom_, source, owner < 0 ? source : owner,
                                         flags_->currentIndex() >> 1, flags_->currentIndex() & 1)
                    : ActorArt::load(rom_, source);
        if (body) {
            QSignalBlocker variantBlock(variant_);
            variant_->setValue(art_->bodyVariant());
        }
        QSignalBlocker block(poses_);
        poses_->clear();
        for (const auto &s : body ? QVector<ActorPoseSelection>{} : art_->selections())
            poses_->addItem(QString("Animation %1 · direction %2 · state %3")
                                .arg(s.animation)
                                .arg(s.facing)
                                .arg(s.state),
                            s.state);
        for (int state = 0; state < art_->poses().size(); ++state)
            if (poses_->findData(state) < 0)
                poses_->addItem(body ? QString("State %1").arg(state)
                                     : QString("State %1 · no animation key").arg(state),
                                state);
        fit_ = true;
    } catch (const Error &e) {
        status_->setText(QString::fromUtf8(e.what()));
        items_.clear();
        canvas_->clear();
        layers_->clear();
        return;
    }
    restart();
}
void ActorPoseEditor::restart() {
    stop();
    if (!art_ || poses_->currentIndex() < 0)
        return;
    if (kinds_->currentIndex() == 2)
        player_.emplace(std::in_place_type<SpritePosePlayer>, rom_, *art_,
                        poses_->currentData().toInt(), variant_->value());
    else {
        player_.emplace(std::in_place_type<ActorPosePlayer>, rom_, *art_,
                        poses_->currentData().toInt(), variant_->value());
        if (art_->isBody())
            std::get<ActorPosePlayer>(*player_).setLinkedObjectKnownAbsent(true);
    }
    if (!art_->isBody() && kinds_->currentIndex() != 2) {
        for (const auto &selection : art_->selections())
            if (selection.state == poses_->currentData().toInt()) {
                const QSignalBlocker block(facing_);
                facing_->setValue(selection.facing);
                break;
            }
    }
    step_ = 0;
    {
        QSignalBlocker block(position_);
        position_->setValue(0);
    }
    draw();
}
void ActorPoseEditor::seek(int target) {
    stop();
    if (!art_)
        return;
    if (!player_ || target < step_ || suspendedPose())
        restart();
    while (step_ < target) {
        const auto result = advancePose();
        if (result != LORDLY_POSE_OK) {
            draw();
            status_->setText(poseError());
            return;
        }
        ++step_;
    }
    {
        QSignalBlocker block(position_);
        position_->setValue(step_);
    }
    draw();
}
void ActorPoseEditor::step() {
    if (!player_ || step_ >= position_->maximum()) {
        stop();
        return;
    }
    if (advancePose() != LORDLY_POSE_OK) {
        stop();
        draw();
        status_->setText(poseError());
        return;
    }
    ++step_;
    {
        QSignalBlocker block(position_);
        position_->setValue(step_);
    }
    draw();
}
void ActorPoseEditor::draw() {
    // Drop observers before deleting selected scene items (selectionChanged is synchronous).
    items_.clear();
    canvas_->clear();
    layers_->clear();
    selection_->clear();
    setProperty("placementFrame", QImage());
    if (!player_)
        return;
    try {
        if (projected_->isChecked()) {
            drawProjected();
            return;
        }
        const auto a = preview(*player_);
        const auto frame = art_->frame(rom_, a.frame, variant_->value(), sepia_->isChecked());
        QRectF bounds;
        int fallbacks = 0;
        for (int i = 0; i < frame.size(); ++i) {
            const auto &layer = frame[i];
            const auto &r = layer.pixels.image;
            auto pixmap =
                QPixmap::fromImage(QImage(reinterpret_cast<const uchar *>(r.rgba.constData()),
                                          r.width, r.height, r.width * 4, QImage::Format_RGBA8888));
            auto item = canvas_->addPixmap(pixmap);
            const auto &m = layer.metadata;
            item->setPos(m.origin.x() * m.scaleX / 1024. + a.x,
                         m.origin.y() * m.scaleY / 1024. - a.y);
            item->setTransform(QTransform::fromScale(m.scaleX / 1024., m.scaleY / 1024.));
            if (i < 16)
                item->setOpacity(u8(a.material, i) / 255.);
            item->setZValue(i);
            item->setFlag(QGraphicsItem::ItemIsSelectable);
            items_ << item;
            bounds |= item->mapRectToScene(QRectF(0, 0, m.width, m.height));
            auto row = new QListWidgetItem(QString("Layer %1 · art %2").arg(i).arg(m.art), layers_);
            row->setFlags(row->flags() | Qt::ItemIsUserCheckable);
            row->setCheckState(Qt::Checked);
            const bool fallback = layer.pixels.requestedChild != layer.pixels.child;
            fallbacks += fallback;
            row->setToolTip(
                QString("Origin %1, %2 · size %3 × %4\nScale %5, %6 · flags %7\nVariant %8 → "
                        "%9%10\nMaterial %11")
                    .arg(m.origin.x())
                    .arg(m.origin.y())
                    .arg(m.width)
                    .arg(m.height)
                    .arg(m.scaleX / 1024.)
                    .arg(m.scaleY / 1024.)
                    .arg(m.flags)
                    .arg(layer.pixels.requestedChild)
                    .arg(layer.pixels.child)
                    .arg(fallback ? " (fallback)" : "")
                    .arg(i < 16 ? QString::number(u8(a.material, i)) : QString("unmapped")));
        }
        canvas_->setSceneRect(bounds.isEmpty() ? QRectF(-32, -32, 64, 64)
                                               : bounds.adjusted(-8, -8, 8, 8));
        if (fit_) {
            view_->fitInView(canvas_->sceneRect(), Qt::KeepAspectRatio);
            fit_ = false;
        }
        status_->setText(
            QString("Step %1 · state %2 · record %3 · delay %4 · frame %5 · position %6, %7, "
                    "%8\n%9 layers · %10 variant fallbacks · %11 audio requests this step")
                .arg(step_)
                .arg(a.state)
                .arg(a.cursor)
                .arg(a.delay)
                .arg(a.frame)
                .arg(a.x)
                .arg(a.y)
                .arg(a.z)
                .arg(frame.size())
                .arg(fallbacks)
                .arg(a.audio));
        setProperty("frameToken", a.frame);
        setProperty("poseCursor", a.cursor);
        setProperty("poseDelay", a.delay);
        setProperty("poseX", a.x);
        setProperty("poseY", a.y);
    } catch (const Error &e) {
        stop();
        status_->setText(QString::fromUtf8(e.what()));
    }
}
void ActorPoseEditor::drawProjected() {
    const auto &runtime = std::get<ActorPosePlayer>(*player_);
    auto pose = runtime.actor();
    pose.position.x += float(placement_[0]->value());
    pose.position.y += float(placement_[1]->value());
    pose.position.z += float(placement_[2]->value());
    LordlyActorRecord raw{};
    lordly_actor_write_pose(&raw, &pose);
    QByteArray record(reinterpret_cast<const char *>(raw.bytes), 336);
    for (int i = 0; i < 3; ++i) {
        put32(record, 0x104 + i * 4, std::bit_cast<quint32>(float(size_->value())));
        put8(record, 0x142 + i, 255);
    }
    put8(record, 0x13c, 255);
    put8(record, 0x13f, quint8(facing_->value()));
    put32(record, 0x130, std::bit_cast<quint32>(1.f));
    auto camera = [](float fov, float x, float y, float z) {
        const float values[]{fov, 4.f / 3, 1, 20000, 1, x, y, z, 0, 0, 0, 0, 1, 0};
        QByteArray bank(56, 0);
        for (int i = 0; i < 14; ++i)
            put32(bank, i * 4, std::bit_cast<quint32>(values[i]));
        return SceneCamera(bank, 1.f);
    };
    const auto angle = cameraYaw_->value() * std::numbers::pi / 180.;
    const auto registered =
        camera(45, float(std::sin(angle) * cameraDistance_->value()), float(cameraHeight_->value()),
               float(std::cos(angle) * cameraDistance_->value()));
    const auto actorCamera = camera(90, 0, 0, 120);
    SceneTransforms channels;
    channels.prepareMatrices(rom_, actorCamera, 1);
    const auto matrices = ActorMatrices::prepare(rom_, record, channels.channel(0), registered);
    record.replace(0x20, 64, matrices.shadow);
    record.replace(0x60, 64, matrices.shadow);
    record.replace(0xa0, 64, matrices.matrix);
    QVector<ActorRasterLayer> projected;
    if (shadows_->isChecked())
        projected += renderer_.projectRecord(*art_, record, true, actorCamera, {255, 255, 255},
                                             sepia_->isChecked());
    projected += renderer_.projectRecord(*art_, record, false, actorCamera, {255, 255, 255},
                                         sepia_->isChecked());
    QImage combined(320, 240, QImage::Format_RGBA8888);
    combined.fill(Qt::transparent);
    ActorRenderer::paint(combined, projected);
    for (const auto &layer : projected) {
        QImage image(320, 240, QImage::Format_RGBA8888);
        image.fill(Qt::transparent);
        ActorRenderer::paint(image, {layer});
        auto item = canvas_->addPixmap(QPixmap::fromImage(image));
        item->setFlag(QGraphicsItem::ItemIsSelectable);
        item->setZValue(items_.size());
        items_ << item;
        auto row = new QListWidgetItem(
            QString("%1 %2").arg(layer.shadow ? "Shadow layer" : "Layer").arg(layer.layer),
            layers_);
        row->setFlags(row->flags() | Qt::ItemIsUserCheckable);
        row->setCheckState(Qt::Checked);
        row->setToolTip(QString("Primitive RGBA %1, %2, %3, %4; %5 clipped corners")
                            .arg(layer.primitive[0])
                            .arg(layer.primitive[1])
                            .arg(layer.primitive[2])
                            .arg(layer.primitive[3])
                            .arg(layer.plane.clip.size()));
    }
    canvas_->setSceneRect(0, 0, 320, 240);
    if (fit_) {
        view_->fitInView(canvas_->sceneRect(), Qt::KeepAspectRatio);
        fit_ = false;
    }
    setProperty("placementFrame", combined);
    setProperty("frameToken", pose.frame);
    setProperty("poseCursor", pose.cursor);
    setProperty("poseDelay", pose.delay);
    status_->setText(QString("Step %1 - frame %2 - projected position %3, %4, %5 - %6 layers. "
                             "Reference camera; scene playback is separate.")
                         .arg(step_)
                         .arg(pose.frame)
                         .arg(pose.position.x)
                         .arg(pose.position.y)
                         .arg(pose.position.z)
                         .arg(projected.size()));
}
int ActorPoseEditor::advancePose() {
    return std::visit([](auto &runtime) { return runtime.advance(); }, *player_);
}
bool ActorPoseEditor::suspendedPose() const {
    return std::visit([](const auto &runtime) { return runtime.suspended(); }, *player_);
}
QString ActorPoseEditor::poseError() const {
    return std::visit([](const auto &runtime) { return runtime.error(); }, *player_);
}
} // namespace lordly
