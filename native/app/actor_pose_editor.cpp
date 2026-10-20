#include "app/actor_pose_editor.h"
#include "engine/sprite_sources.h"
#include "engine/actor_materials.h"
#include "engine/class_data.h"
#include "engine/animation_transfer.h"
#include "engine/scene_transforms.h"
#include "engine/text.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QJsonDocument>
#include <QLabel>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QScrollBar>
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
    std::function<void(QPoint)> artworkMoved;
    bool groundGuides = false;
    void setArtwork(const QVector<QGraphicsPixmapItem *> &items) {
        cancelMove();
        artwork_ = items;
    }

  protected:
    void drawForeground(QPainter *painter, const QRectF &rect) override {
        if (!groundGuides) return;
        painter->save();
        QPen ground(QColor("#e5c775"), 1, Qt::DashLine);
        ground.setCosmetic(true);
        painter->setPen(ground);
        painter->drawLine(QPointF(rect.left(), 0), QPointF(rect.right(), 0));
        ground.setColor(QColor("#8b939f"));
        painter->setPen(ground);
        painter->drawLine(QPointF(0, rect.top()), QPointF(0, rect.bottom()));
        const auto origin = painter->worldTransform().map(QPointF(0, 0));
        painter->resetTransform();
        painter->setPen(QPen(QColor("#ffeab5"), 2));
        painter->drawLine(origin + QPointF(-6, 0), origin + QPointF(6, 0));
        painter->drawLine(origin + QPointF(0, -6), origin + QPointF(0, 6));
        painter->drawText(origin + QPointF(9, 17), "Ground / origin");
        painter->restore();
    }
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::MiddleButton) {
            cancelMove();
            panning_ = true;
            panPosition_ = event->pos();
            viewport()->setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
        if (event->button() == Qt::LeftButton) {
            for (auto item : artwork_) {
                if (!item->contains(item->mapFromScene(mapToScene(event->pos())))) continue;
                setFocus(Qt::MouseFocusReason);
                dragging_ = true;
                start_ = event->pos();
                startScene_ = mapToScene(start_);
                positions_.clear();
                for (auto layer : artwork_) positions_ << layer->pos();
                viewport()->setCursor(Qt::SizeAllCursor);
                event->accept();
                return;
            }
        }
        QGraphicsView::mousePressEvent(event);
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        if (panning_) {
            const auto delta = event->pos() - panPosition_;
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
            verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
            panPosition_ = event->pos();
        } else if (dragging_) updateMove(event->pos());
        else {
            QGraphicsView::mouseMoveEvent(event);
            return;
        }
        event->accept();
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::MiddleButton && panning_) {
            panning_ = false;
            viewport()->setCursor(Qt::OpenHandCursor);
            event->accept();
            return;
        }
        if (event->button() == Qt::LeftButton && dragging_) {
            updateMove(event->pos());
            const auto offset = offset_;
            // Restore transient positions before publishing; refresh can destroy these items.
            cancelMove();
            event->accept();
            if (!offset.isNull() && artworkMoved) artworkMoved(offset);
            return;
        }
        QGraphicsView::mouseReleaseEvent(event);
    }
    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Escape && dragging_) {
            cancelMove();
            event->accept();
            return;
        }
        QGraphicsView::keyPressEvent(event);
    }
    void focusOutEvent(QFocusEvent *event) override {
        cancelMove();
        panning_ = false;
        viewport()->setCursor(Qt::OpenHandCursor);
        QGraphicsView::focusOutEvent(event);
    }
    void wheelEvent(QWheelEvent *event) override {
        if (dragging_ || panning_) { event->accept(); return; }
        const double factor = event->angleDelta().y() > 0 ? 1.2 : 1 / 1.2;
        const double next = transform().m11() * factor;
        if (next >= .05 && next <= 64)
            scale(factor, factor);
        event->accept();
    }
  private:
    void updateMove(QPoint position) {
        if ((position - start_).manhattanLength() < 3 && offset_.isNull()) return;
        offset_ = (mapToScene(position) - startScene_).toPoint();
        for (int i = 0; i < artwork_.size(); ++i) {
            const auto transform = artwork_[i]->transform();
            const auto local = animationLayerTranslation(offset_, qRound(transform.m11() * 1024),
                                                        qRound(transform.m22() * 1024));
            artwork_[i]->setPos(positions_[i] + transform.map(QPointF(local)));
        }
    }
    void cancelMove() {
        if (!dragging_) return;
        for (int i = 0; i < artwork_.size(); ++i) artwork_[i]->setPos(positions_[i]);
        dragging_ = false;
        positions_.clear();
        offset_ = {};
        viewport()->setCursor(Qt::OpenHandCursor);
    }
    QVector<QGraphicsPixmapItem *> artwork_;
    QVector<QPointF> positions_;
    bool dragging_ = false, panning_ = false;
    QPoint start_, offset_, panPosition_;
    QPointF startScene_;
};
} // namespace
ActorPoseEditor::ActorPoseEditor(const Rom &rom, QWidget *parent)
    : QWidget(parent), rom_(rom), bodyAnimations_(BodyAnimationData::load(rom)), renderer_(rom) {
    auto layout = new QVBoxLayout(this);
    sourceControls_=new QWidget;
    auto sourceLayout=new QVBoxLayout(sourceControls_);
    sourceLayout->setContentsMargins(0,0,0,0);
    auto note = new QLabel(
        "Pose inspection with per-layer opacity and optional sepia. Playback speed is adjustable; "
        "full-scene timing, cameras and lighting are separate. Sound requests are recorded without "
        "audio output. Class bodies include their equipment appearance.");
    note->setWordWrap(true);
    sourceLayout->addWidget(note);
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
    sourceLayout->addLayout(selectors);
    auto poseSelectors = new QHBoxLayout;
    poseSelectors->addWidget(new QLabel("Pose"));
    poseSelectors->addWidget(poses_, 1);
    poseSelectors->addWidget(new QLabel("Image variant"));
    poseSelectors->addWidget(variant_);
    sourceLayout->addLayout(poseSelectors);
    layout->addWidget(sourceControls_);
    actionControls_ = new QWidget;
    auto actionRow = new QHBoxLayout(actionControls_);
    actionRow->setContentsMargins(0,0,0,0);
    actionRow->addWidget(new QLabel("Attack action"));
    actions_ = new QComboBox; actions_->setObjectName("actorAttackAction");
    actions_->addItem("Manual pose", -1);
    QMap<int,QString> actionNames;
    for (const auto &value : AnimationOverrideImage::definition()["actions"].toArray()) {
        const auto row=value.toObject(); actionNames[row["id"].toInt()]=row["name"].toString();
    }
    for (int id=0;id<255;++id) actions_->addItem(QString("0x%1 · %2").arg(id,2,16,QChar('0')).arg(actionNames.value(id,"Action")),id);
    actionRow->addWidget(actions_,1);
    actionModes_ = new QComboBox; actionModes_->setObjectName("actorAttackMode");
    actionModes_->addItems({"Normal · mode 0","Normal · mode 1","Attack blocked · mode 2"});
    actionRow->addWidget(actionModes_);
    actionControls_->setToolTip("Resolves current class defaults and attack overrides for the selected source, owner and body flags. Scene commands that directly select a pose keep that pose.");
    layout->addWidget(actionControls_);
    bodyControls_ = new QWidget;
    auto bodyLayout = new QHBoxLayout(bodyControls_);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    equipment_ = new QComboBox;
    equipment_->setObjectName("actorEquipment");
    equipment_->addItem("Class default", -1);
    equipment_->addItem("0 · None", 0);
    const auto itemData = bodyAnimations_->items(rom_);
    for (int id = 1; id < itemData.entries().size(); ++id)
        equipment_->addItem(QString("%1 · %2").arg(id).arg(decodeText(itemData.entries()[id].name)),
                            id);
    equipment_->setToolTip(
        "Preview equipment appearance. The game's class-specific substitutions still apply.");
    bodyPalette_ = new QSpinBox;
    bodyPalette_->setObjectName("actorEquipmentPalette");
    bodyPalette_->setRange(-1, 255);
    bodyPalette_->setValue(-1);
    bodyPalette_->setSpecialValueText("Unspecified");
    bodyPalette_->setMinimumWidth(bodyPalette_->fontMetrics().horizontalAdvance("Unspecified") + 45);
    bodyPalette_->setToolTip("Some equipment uses a palette supplied by the scene launch. Set that "
                             "value here when required.");
    bodyLayout->addWidget(new QLabel("Equipment"));
    bodyLayout->addWidget(equipment_, 1);
    bodyLayout->addWidget(new QLabel("Launch palette"));
    bodyLayout->addWidget(bodyPalette_);
    layout->addWidget(bodyControls_);
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
    auto stepLabel = new QLabel("Step");
    controls->addWidget(stepLabel);
    controls->addWidget(position_);
    controls->addWidget(speed_);
    controls->addWidget(sepia_);
    controls->addStretch();
    fit->setObjectName("actorFit");
    controls->addWidget(fit);
    controls->addWidget(actual);
    automaticFit_ = new QCheckBox("Automatic fit");
    automaticFit_->setObjectName("actorAutomaticFit");
    automaticFit_->setToolTip("Follow each frame's bounds. Leave unchecked to keep a stable sprite origin and scale.");
    controls->addWidget(automaticFit_);
    inspectionControls_ << stepLabel << position_ << speed_ << sepia_ << automaticFit_;
    layout->addLayout(controls);
    projected_ = new QCheckBox("Projected placement");
    projected_->setObjectName("actorProjected");
    projected_->setToolTip("Inspect native Actor matrices with a reference camera. Scene "
                           "commands and terrain are not executed here.");
    layout->addWidget(projected_);
    placementControls_ = new QWidget;
    auto placementLayout = new QVBoxLayout(placementControls_);
    placementLayout->setContentsMargins(0, 0, 0, 0);
    auto pathRow = new QHBoxLayout;
    pathRow->addWidget(new QLabel("Placement path"));
    placementMode_ = new QComboBox;
    placementMode_->setObjectName("actorPlacementMode");
    placementMode_->addItems({"Layered scene", "World scene"});
    pathRow->addWidget(placementMode_);
    pathRow->addStretch();
    placementLayout->addLayout(pathRow);
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
    auto colorRow = new QHBoxLayout;
    materialMode_ = new QComboBox;
    materialMode_->setObjectName("actorMaterialMode");
    materialMode_->addItems({"Color multiply", "Tint blend", "Material 0", "Material 1"});
    colorRow->addWidget(new QLabel("Preview material"));
    colorRow->addWidget(materialMode_);
    for (int group = 0; group < 2; ++group) {
        colorRow->addWidget(new QLabel(group ? "Environment RGB" : "Actor RGB"));
        for (int c = 0; c < 3; ++c) {
            auto value = new QSpinBox;
            value->setObjectName(QString(group ? "actorEnvironment%1" : "actorColor%1").arg(c));
            value->setRange(0, 255);
            value->setValue(group ? 0 : 255);
            (group ? environmentRgb_ : actorRgb_)[c] = value;
            colorRow->addWidget(value);
            connect(value, &QSpinBox::valueChanged, this, &ActorPoseEditor::draw);
        }
    }
    connect(materialMode_, &QComboBox::currentIndexChanged, this, &ActorPoseEditor::draw);
    placementLayout->addLayout(colorRow);
    auto placementNote = new QLabel(
        "Reference camera and preview position. World view uses two reference lights and flat "
        "terrain. Material previews use the game’s texture inputs and color formulas; Qt sampling "
        "and blending approximate the N64 output. Scene lighting remains separate.");
    placementNote->setWordWrap(true);
    placementLayout->addWidget(placementNote);
    layout->addWidget(placementControls_);
    placementControls_->hide();
    connect(projected_, &QCheckBox::toggled, this, [this](bool enabled) {
        placementControls_->setVisible(enabled);
        variant_->setEnabled(kinds_->currentIndex() != 1);
        fit_ = true;
        draw();
    });
    connect(facing_, &QSpinBox::valueChanged, this, &ActorPoseEditor::draw);
    connect(shadows_, &QCheckBox::toggled, this, &ActorPoseEditor::draw);
    connect(placementMode_, &QComboBox::currentIndexChanged, this, [this](int index) {
        previewCameras_[previousPlacementMode_] = {cameraYaw_->value(), cameraHeight_->value(),
                                                   cameraDistance_->value()};
        previousPlacementMode_ = index;
        const QSignalBlocker yawBlock(cameraYaw_), heightBlock(cameraHeight_),
            distanceBlock(cameraDistance_);
        cameraYaw_->setValue(previewCameras_[index][0]);
        cameraHeight_->setValue(previewCameras_[index][1]);
        cameraDistance_->setValue(previewCameras_[index][2]);
        draw();
    });
    positionControls_ = new QWidget;
    auto positioning = new QHBoxLayout(positionControls_);
    positioning->setContentsMargins(0, 0, 0, 0);
    positioning->addWidget(new QLabel("Move"));
    positionLayer_ = new QComboBox;
    positionLayer_->setObjectName("actorPositionLayer");
    positionLayer_->setIconSize(QSize(32, 32));
    positionLayer_->addItem("Everything", -1);
    positionLayer_->setToolTip("Choose the artwork to drag. A layer number applies to every selected frame; other layers stay in place.");
    positioning->addWidget(positionLayer_, 1);
    positionControls_->hide();
    layout->addWidget(positionControls_);
    connect(positionLayer_, &QComboBox::currentIndexChanged, this, [this] { draw(); });
    auto split = new QSplitter;
    canvas_ = new QGraphicsScene(this);
    view_ = new PoseView(canvas_);
    static_cast<PoseView *>(view_)->artworkMoved = [this](QPoint offset) {
        // Project refresh updates the bounds, but must not zoom/recenter a positioning gesture.
        committingArtworkMove_ = true;
        emit frameArtworkMoved(offset, positionLayer_->currentData().toInt());
        draw(); // Also restore the original artwork if validation rejected the move.
        fit_ = false;
        committingArtworkMove_ = false;
    };
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
    copyToLibrary_ = new QPushButton("Copy pose frames to library");
    copyToLibrary_->setObjectName("actorCopyToLibrary");
    copyToLibrary_->setToolTip("Copy authored frame artwork and timing with the selected image variant "
                             "and equipment appearance. Movement, loops, lighting, sepia and sound "
                             "remain in the game sequence.");
    copyToLibrary_->hide();
    layerLayout->addWidget(copyToLibrary_);
    connect(copyToLibrary_, &QPushButton::clicked, this, [this] {
        try {
            if (!librarySink_) return;
            stop();
            librarySink_(poseAsset());
            status_->setText("Copied pose frames to Art Library.");
        } catch (const std::exception &e) {
            status_->setText(QString::fromUtf8(e.what()));
        }
    });
    split->addWidget(layerPanel);
    inspectionControls_ << bodyControls_ << projected_ << placementControls_ << layerPanel;
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
            if (frameEditing_ && selectedFrame_) {
                restart();
                const QSignalBlocker block(play_);
                play_->setChecked(true);
            }
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
    connect(poses_, &QComboBox::currentIndexChanged, this, [this] {
        const QSignalBlocker block(actions_); actions_->setCurrentIndex(0); restart();
    });
    connect(actions_, &QComboBox::currentIndexChanged, this, &ActorPoseEditor::resolveAction);
    connect(actionModes_, &QComboBox::currentIndexChanged, this, &ActorPoseEditor::resolveAction);
    connect(variant_, &QSpinBox::valueChanged, this, &ActorPoseEditor::restart);
    connect(sepia_, &QCheckBox::toggled, this, &ActorPoseEditor::draw);
    connect(equipment_, &QComboBox::currentIndexChanged, this, &ActorPoseEditor::draw);
    connect(bodyPalette_, &QSpinBox::valueChanged, this, &ActorPoseEditor::draw);
    connect(fit, &QPushButton::clicked, this, [this] { fit_ = true; draw(); });
    connect(automaticFit_, &QCheckBox::toggled, this, [this] { fit_ = true; draw(); });
    connect(actual, &QPushButton::clicked, this, [this] { automaticFit_->setChecked(false); view_->resetTransform(); });
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
        placementMode_->setCurrentIndex(modeZero ? 0 : 1);
        projected_->setChecked(true);
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
                .arg("Command coordinates use the reference camera above; scene cameras, lighting "
                     "and terrain are separate."));
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
void ActorPoseEditor::setSpriteLibrary(std::function<void(SpriteAsset)> sink) {
    librarySink_ = std::move(sink);
    copyToLibrary_->setVisible(bool(librarySink_));
}
SpriteAsset ActorPoseEditor::poseAsset() const {
    if (!art_ || poses_->currentIndex() < 0)
        throw Error("Choose an available actor and pose");
    return spriteAssetFromPose(rom_, *art_, poses_->currentData().toInt(),
                                (banks_->currentText() + " · pose " + poses_->currentData().toString()).left(120),
                                variant_->value(), appearance(), artwork_.value(art_->descriptorKey()));
}
PosePlaybackSource ActorPoseEditor::playbackSource() const {
    if (!art_ || !art_->isBody() || poses_->currentIndex() < 0 || projected_->isChecked())
        throw Error("Choose a combat animation in the frame preview");
    return {rom_, *art_, poses_->currentData().toInt(), variant_->value(), speed_->value(),
            sepia_->isChecked(), appearance(), artwork_.value(art_->descriptorKey())};
}
void ActorPoseEditor::selectPrivateSequence(int classId,int flags,int state) {
    const auto classes = bodyAnimations_ ? bodyAnimations_->classes() : ClassData::load(rom_);
    if (classId < 0 || classId >= classes.entries().size()) throw Error("Invalid private preview class");
    const int field = u8(classes.entries()[classId].record, 69), owner = field ? field : -1;
    sourceControls_->hide();actionControls_->hide();
    kinds_->setEnabled(false);banks_->setEnabled(false);owners_->setEnabled(false);
    flags_->setEnabled(false);poses_->setEnabled(false);actions_->setEnabled(false);actionModes_->setEnabled(false);
    if(kinds_->currentIndex()==1&&banks_->currentData().toInt()==classId&&
       owners_->currentData().toInt()==owner&&flags_->currentIndex()==flags&&poses_->currentData().toInt()==state)return;
    if(kinds_->currentIndex()!=1)kinds_->setCurrentIndex(1);
    {
        const QSignalBlocker a(banks_),b(owners_),c(flags_),d(actions_);
        banks_->setCurrentIndex(banks_->findData(classId));owners_->setCurrentIndex(owners_->findData(owner));flags_->setCurrentIndex(flags);actions_->setCurrentIndex(0);
    }
    loadBank();
    if(poses_->findData(state)<0)throw Error("The selected pose is absent from this body route");
    {const QSignalBlocker block(poses_);poses_->setCurrentIndex(poses_->findData(state));}
    owners_->setEnabled(false);flags_->setEnabled(false);
    actionControls_->hide();
    restart();
}
void ActorPoseEditor::loadSources() {
    stop();
    const bool body = kinds_->currentIndex() == 1;
    projected_->setEnabled(kinds_->currentIndex() != 2);
    if (kinds_->currentIndex() == 2)
        projected_->setChecked(false);
    variant_->setEnabled(!body);
    bodyControls_->setVisible(body);
    actionControls_->setVisible(body);
    owners_->setEnabled(body);
    flags_->setEnabled(body);
    try {
        QSignalBlocker banksBlock(banks_), ownerBlock(owners_);
        banks_->clear();
        owners_->clear();
        owners_->addItem("Same as source", -1);
        if (body) {
            const auto &classes = bodyAnimations_->classes();
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
                                         flags_->currentIndex() >> 1, flags_->currentIndex() & 1, bodyAnimations_)
                    : ActorArt::load(rom_, source);
        if (body) {
            defaultAppearance_ = art_->defaultAppearance(rom_);
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
        draw();
        status_->setText(QString::fromUtf8(e.what()));
        return;
    }
    resolveAction();
}
void ActorPoseEditor::restart() {
    stop();
    selectedFrame_.reset();
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
    selectedFrame_.reset();
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
    selectedFrame_.reset();
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
void ActorPoseEditor::resolveAction() {
    if (kinds_->currentIndex()!=1 || actions_->currentData().toInt()<0) { restart(); return; }
    try {
        const int source=banks_->currentData().toInt(), owner=owners_->currentData().toInt();
        const int state=bodyAnimations_->selector(source,owner<0?source:owner,actions_->currentData().toInt(),actionModes_->currentIndex(),flags_->currentIndex());
        const int index=poses_->findData(state);
        if (index<0) throw Error("The selected action refers to an absent pose in this body");
        const QSignalBlocker block(poses_); poses_->setCurrentIndex(index); restart();
    } catch (const Error &e) {
        stop(); player_.reset(); draw(); status_->setText(QString::fromUtf8(e.what()));
    }
}
void ActorPoseEditor::setBodyAnimations(BodyAnimationData::Pointer animations) {
    if (!animations) throw Error("Animation preview requires class data");
    bodyAnimations_=std::move(animations);
    {
        const QSignalBlocker block(equipment_);
        const int selected = equipment_->currentData().toInt();
        equipment_->clear();
        equipment_->addItem("Class default", -1);
        equipment_->addItem("0 · None", 0);
        const auto items = bodyAnimations_->items(rom_);
        for (int id = 1; id < items.entries().size(); ++id)
            equipment_->addItem(
                QString("%1 · %2").arg(id).arg(decodeText(items.entries()[id].name)), id);
        equipment_->setCurrentIndex(qMax(0, equipment_->findData(selected)));
    }
    if (kinds_->currentIndex()!=1) return;
    const int source=banks_->currentData().toInt(),owner=owners_->currentData().toInt(),state=poses_->currentData().toInt(),target=step_;
    {
        const QSignalBlocker bankBlock(banks_),ownerBlock(owners_);
        banks_->clear(); owners_->clear(); owners_->addItem("Same as source",-1);
        const auto &entries=bodyAnimations_->classes().entries();
        for(int id=0;id<entries.size();++id) {
            const auto label=QString("%1 · %2").arg(id).arg(decodeText(entries[id].name));
            banks_->addItem(label,id); owners_->addItem(label,id);
        }
        banks_->setCurrentIndex((banks_->findData(source)>=0?banks_->findData(source):banks_->findData(1)));
        owners_->setCurrentIndex(std::max(0,owners_->findData(owner)));
    }
    loadBank();
    if (art_ && actions_->currentData().toInt()<0) {
        const QSignalBlocker block(poses_); poses_->setCurrentIndex(std::max(0,poses_->findData(state))); restart();
    }
    seek(target);
}
void ActorPoseEditor::setArtwork(const ActorArtworkEdits &edits) {
    if (artwork_ == edits)
        return;
    artwork_ = edits;
    renderer_.setArtwork(edits);
    draw();
}
void ActorPoseEditor::showFrame(int frame) {
    stop();
    selectedFrame_ = frame;
    draw();
}
void ActorPoseEditor::showReplacementPreview() {
    setFrameEditing(true);
    positionControls_->hide();
    bodyControls_->hide();
    status_->hide();
    fit_ = true;
    seek(1);
}
void ActorPoseEditor::setFrameEditing(bool enabled) {
    frameEditing_ = enabled;
    positionControls_->setVisible(enabled);
    static_cast<PoseView *>(view_)->groundGuides = enabled;
    view_->viewport()->update();
    if (!enabled) static_cast<PoseView *>(view_)->setArtwork({});
    for (auto widget : inspectionControls_) widget->setVisible(!enabled);
    if (enabled) {
        projected_->setChecked(false);
        placementControls_->hide();
        play_->setToolTip("Play the animation from the beginning after choosing a frame. Pause and Play resume playback.");
    } else placementControls_->setVisible(projected_->isChecked());
}
void ActorPoseEditor::setPreviousFrame(std::optional<int> frame) {
    if (previousFrame_ == frame) return;
    previousFrame_ = frame;
    draw();
}
void ActorPoseEditor::setFrameBounds(const QRectF &bounds) {
    if (editingBounds_ == bounds) return;
    editingBounds_ = bounds;
    fit_ = true;
    draw();
}
void ActorPoseEditor::setPlaybackRate(double rate) { speed_->setValue(qRound(30 * rate)); }
void ActorPoseEditor::draw() {
    // Drop observers before deleting selected scene items (selectionChanged is synchronous).
    static_cast<PoseView *>(view_)->setArtwork({});
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
        auto a = preview(*player_);
        if (selectedFrame_) a.frame = *selectedFrame_;
        if (frameEditing_ && selectedFrame_) { a.x = a.y = 0; a.material.fill(char(-1)); }
        const auto body = appearance();
        const auto frame = body ? art_->bodyFrame(rom_, a.frame, *body, sepia_->isChecked(),
                                                  artwork_.value(art_->descriptorKey()))
                                : art_->frame(rom_, a.frame, variant_->value(), sepia_->isChecked(),
                                              {}, artwork_.value(art_->descriptorKey()));
        QRectF bounds = frameEditing_ ? editingBounds_ : QRectF{};
        if (frameEditing_) bounds |= QRectF(-10, -4, 20, 12);
        const QSignalBlocker positionBlock(positionLayer_);
        const int positionLayer = positionLayer_->currentData().toInt();
        positionLayer_->clear();
        positionLayer_->addItem("Everything", -1);
        if (frameEditing_ && selectedFrame_ && previousFrame_ && body) {
            const auto previous = art_->bodyFrame(rom_, *previousFrame_, *body, false,
                                                  artwork_.value(art_->descriptorKey()));
            for (const auto &layer : previous) {
                const auto &pixels = layer.pixels.image;
                auto item = canvas_->addPixmap(QPixmap::fromImage(QImage(
                    reinterpret_cast<const uchar *>(pixels.rgba.constData()), pixels.width,
                    pixels.height, pixels.width * 4, QImage::Format_RGBA8888)));
                const auto &m = layer.metadata;
                item->setPos(m.origin.x() * m.scaleX / 1024., m.origin.y() * m.scaleY / 1024.);
                item->setTransform(QTransform::fromScale(m.scaleX / 1024., m.scaleY / 1024.));
                item->setOpacity(.25); item->setZValue(-1);
                bounds |= item->sceneBoundingRect();
            }
        }
        int fallbacks = 0;
        const auto rendered = posePreviewLayers(frame, a.x, a.y, a.material);
        for (int i = 0; i < frame.size(); ++i) {
            const auto &layer = frame[i];
            const auto &visual = rendered[i];
            auto pixmap = QPixmap::fromImage(visual.image);
            auto item = canvas_->addPixmap(pixmap);
            const auto &m = layer.metadata;
            item->setPos(visual.position);
            item->setTransform(QTransform::fromScale(visual.scaleX, visual.scaleY));
            item->setOpacity(visual.opacity);
            item->setZValue(i);
            item->setFlag(QGraphicsItem::ItemIsSelectable);
            items_ << item;
            const QString role = body ? (art_->imageKind(m.art) ? "Equipment" : "Body") : "Artwork";
            positionLayer_->addItem(QIcon(pixmap), QString("%1 · layer %2").arg(role).arg(i + 1), i);
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
        if (!committingArtworkMove_ && (fit_ || automaticFit_->isChecked())) {
            canvas_->setSceneRect(bounds.isEmpty() ? QRectF(-32, -32, 64, 64)
                                                   : bounds.adjusted(-8, -8, 8, 8));
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
        if (selectedFrame_) status_->setText(QString("Selected frame %1 · Play resumes sequence playback").arg(a.frame));
        if (frameEditing_)
            status_->setText(selectedFrame_ ? "Selected artwork · drag to position"
                                           : "Animation preview · select a frame to position its artwork");
        positionLayer_->setCurrentIndex(qMax(0, positionLayer_->findData(positionLayer)));
        positionLayer_->setEnabled(frameEditing_ && selectedFrame_.has_value());
        const int selectedLayer = positionLayer_->currentData().toInt();
        if (frameEditing_ && selectedFrame_) {
            if (selectedLayer < 0) static_cast<PoseView *>(view_)->setArtwork(items_);
            else if (selectedLayer < items_.size()) {
                static_cast<PoseView *>(view_)->setArtwork({items_[selectedLayer]});
                for (int i = 0; i < items_.size(); ++i)
                    if (i != selectedLayer) items_[i]->setOpacity(.35);
            }
        }
        setProperty("poseCursor", a.cursor);
        setProperty("poseDelay", a.delay);
        setProperty("poseX", a.x);
        setProperty("poseY", a.y);
        emit framePresented(a.state, a.cursor, a.frame, !selectedFrame_);
    } catch (const Error &e) {
        stop();
        status_->setText(QString::fromUtf8(e.what()));
    }
}
std::optional<BodyAppearance> ActorPoseEditor::appearance() const {
    if (!art_ || !art_->isBody())
        return {};
    auto result = defaultAppearance_;
    if (equipment_->currentData().toInt() >= 0)
        result.equipment = quint16(equipment_->currentData().toInt());
    if (bodyPalette_->value() >= 0)
        result.paletteReplacement = quint8(bodyPalette_->value());
    return result;
}
void ActorPoseEditor::drawProjected() {
    const auto &runtime = std::get<ActorPosePlayer>(*player_);
    auto pose = runtime.actor();
    if (selectedFrame_) pose.frame = *selectedFrame_;
    pose.position.x += float(placement_[0]->value());
    pose.position.y += float(placement_[1]->value());
    pose.position.z += float(placement_[2]->value());
    LordlyActorRecord raw{};
    lordly_actor_write_pose(&raw, &pose);
    QByteArray record(reinterpret_cast<const char *>(raw.bytes), 336);
    for (int i = 0; i < 3; ++i) {
        put32(record, 0x104 + i * 4, std::bit_cast<quint32>(float(size_->value())));
        put8(record, 0x142 + i, quint8(actorRgb_[i]->value()));
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
    const bool world = placementMode_->currentIndex() == 1;
    const auto actorCamera = world ? registered : camera(90, 0, 0, 120);
    if (world) {
        const LordlyActorLighting lighting{{0, 0, 0}, {{200, -200, 50}, {-200, 200, 75}}};
        put8(record, 0x145, 1); // Explicit flat reference terrain for this inspector.
        const auto matrices =
            ActorWorldMatrices::prepare(rom_, record, actorCamera, 0, 0, lighting);
        for (int i = 0; i < 2; ++i)
            record.replace(0x20 + i * 64, 64, matrices.shadows[i]);
        record.replace(0xa0, 64, matrices.matrix);
    } else {
        SceneTransforms channels;
        channels.prepareMatrices(rom_, actorCamera, 1);
        const auto matrices = ActorMatrices::prepare(rom_, record, channels.channel(0), registered);
        record.replace(0x20, 64, matrices.shadow);
        record.replace(0x60, 64, matrices.shadow);
        record.replace(0xa0, 64, matrices.matrix);
    }
    QVector<ActorRasterLayer> projected;
    if (shadows_->isChecked())
        for (int i = 0; i < (world ? 2 : 1); ++i)
            projected += renderer_.projectRecord(*art_, record, true, actorCamera, {255, 255, 255},
                                                 sepia_->isChecked(), i, world ? 96 : 255, nullptr,
                                                 nullptr, appearance());
    auto materialState = ActorRenderState::main({0, 0, 0, 0});
    ActorMaterials material;
    const int mode = materialMode_->currentIndex();
    if (mode == 1) {
        QByteArray tint(12, 0);
        for (int i = 0; i < 3; ++i)
            put8(tint, 3 + i, quint8(actorRgb_[i]->value()));
        put16(tint, 6, 1);
        material.supplyTint(0, tint);
    } else if (mode >= 2) {
        QByteArray entry(7, 0);
        put8(entry, 0, quint8(mode - 2));
        for (int i = 0; i < 3; ++i) {
            put8(entry, 1 + i, quint8(environmentRgb_[i]->value()));
            put8(entry, 4 + i, quint8(actorRgb_[i]->value()));
        }
        material.supplyMaterial(0, entry);
    }
    materialState.apply(material.render(record));
    projected +=
        renderer_.projectRecord(*art_, record, false, actorCamera, {255, 255, 255},
                                sepia_->isChecked(), 0, 255, &materialState, nullptr, appearance());
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
            QString("%1 %2")
                .arg(layer.shadow ? QString("Shadow %1 layer").arg(layer.shadowIndex + 1) : "Layer")
                .arg(layer.layer),
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
