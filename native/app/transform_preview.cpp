#include "app/transform_preview.h"
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHideEvent>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <bit>
#include <limits>
namespace lordly {
TransformPreview::TransformPreview(const Rom &rom, QWidget *parent)
    : QWidget(parent), rom_(rom), camera_(rom.bytes().mid(0x2866f0, 56), 1.f) {
    auto layout = new QVBoxLayout(this);
    context_ = new QLabel("Isolated transform preview. Other scene actions are not played here. "
                          "Double-click a transform command to use its settings.");
    context_->setWordWrap(true);
    layout->addWidget(context_);
    auto controls = new QHBoxLayout;
    auto number = [&](const QString &label, const QString &name, int low, int high, int value) {
        controls->addWidget(new QLabel(label));
        auto box = new QSpinBox;
        box->setObjectName(name);
        box->setRange(low, high);
        box->setValue(value);
        box->setKeyboardTracking(false);
        controls->addWidget(box);
        return box;
    };
    resource_ =
        number("Resource", "transformResource", 0, SceneTransformAsset::selectorCount(rom) - 1, 0);
    group_ = number("Sequence", "transformGroup", 0, 0, 0);
    duration_ = number("Ticks / segment", "transformDuration", std::numeric_limits<int>::min(),
                       std::numeric_limits<int>::max(), 30);
    channel_ = number("Channel", "transformChannel", 0, 19, 0);
    controls->addStretch();
    layout->addLayout(controls);
    auto transport = new QHBoxLayout;
    play_ = new QPushButton("Play");
    play_->setObjectName("transformPlay");
    step_ = new QPushButton("Step");
    step_->setObjectName("transformStep");
    auto rewind = new QPushButton("Restart");
    rewind->setObjectName("transformRestart");
    position_ = new QSpinBox;
    position_->setObjectName("transformPosition");
    position_->setPrefix("Tick ");
    position_->setKeyboardTracking(false);
    rate_ = new QSpinBox;
    rate_->setObjectName("transformRate");
    rate_->setRange(1, 120);
    rate_->setValue(30);
    rate_->setSuffix(" updates/s");
    for (auto button : {play_, step_, rewind})
        transport->addWidget(button);
    transport->addWidget(position_);
    transport->addWidget(rate_);
    transport->addStretch();
    layout->addLayout(transport);
    canvas_ = new QGraphicsScene(this);
    view_ = new QGraphicsView(canvas_);
    view_->setObjectName("transformCanvas");
    view_->setMinimumSize(400, 300);
    layout->addWidget(view_, 1);
    status_ = new QLabel;
    status_->setObjectName("transformStatus");
    status_->setWordWrap(true);
    status_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(status_);
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, [this] { seek(tick_ + 1); });
    connect(play_, &QPushButton::clicked, this, [this] {
        if (timer_->isActive())
            stop();
        else if (state_ && !state_->done()) {
            timer_->start(qMax(1, 1000 / rate_->value()));
            play_->setText("Pause");
        }
    });
    connect(rate_, &QSpinBox::valueChanged, this, [this] {
        if (timer_->isActive())
            timer_->start(qMax(1, 1000 / rate_->value()));
    });
    connect(step_, &QPushButton::clicked, this, [this] {
        stop();
        seek(tick_ + 1);
    });
    connect(rewind, &QPushButton::clicked, this, [this] {
        stop();
        if (state_)
            seek(0);
        else
            reload(false);
    });
    connect(position_, &QSpinBox::valueChanged, this, [this](int tick) {
        stop();
        seek(tick);
    });
    connect(resource_, &QSpinBox::valueChanged, this, [this] { reload(true); });
    connect(group_, &QSpinBox::valueChanged, this, [this] { reload(false); });
    connect(duration_, &QSpinBox::valueChanged, this, [this] { reload(false); });
    connect(channel_, &QSpinBox::valueChanged, this, [this] { draw(); });
    clear();
    reload(true);
}
void TransformPreview::stop() {
    timer_->stop();
    play_->setText("Play");
}
void TransformPreview::hideEvent(QHideEvent *event) {
    stop();
    QWidget::hideEvent(event);
}
void TransformPreview::clear() {
    stop();
    state_.reset();
    asset_.reset();
    checkpoints_.clear();
    canvas_->clear();
    frame_ = {};
    position_->setEnabled(false);
    play_->setEnabled(false);
    step_->setEnabled(false);
    setProperty("channelRecord", QByteArray());
    status_->setText("Choose a transform resource or double-click a scene transform command.");
}
void TransformPreview::fail(const std::exception &e) {
    clear();
    status_->setText(QString::fromUtf8(e.what()));
}
void TransformPreview::setImage(std::optional<DecodedImage> image) {
    image_ = std::move(image);
    draw();
}
void TransformPreview::selectSequence(int resource, int group, qint32 duration,
                                      SceneCamera camera) {
    stop();
    try {
        // Validate before QSpinBox can clamp a supplied command silently.
        const auto asset = SceneTransformAsset::load(rom_, resource);
        SceneTransforms checked;
        checked.start(asset, group, duration);
        const QSignalBlocker r(resource_), g(group_), d(duration_);
        resource_->setValue(resource);
        group_->setRange(0, qMax(0, int(asset.groups.size()) - 1));
        group_->setValue(group);
        duration_->setValue(duration);
        camera_ = std::move(camera);
        asset_ = asset;
        reload(false);
    } catch (const std::exception &e) {
        fail(e);
    }
}
void TransformPreview::selectCommand(const DirectorProgram &program, int command) {
    try {
        if (command < 0 || command >= program.commands.size())
            throw Error("No transform command selected");
        const auto &selected = program.commands[command];
        if (selected.opcode != 8 || selected.operands.size() != 3)
            throw Error("Select a transform command");
        SceneCamera camera(rom_.bytes().mid(0x2866f0, 56), 1.f);
        for (int i = 0; i < command; ++i)
            if (program.commands[i].opcode == 0x36)
                camera.select(program.commands[i].operands);
        context_->setText("Isolated transform preview using the preceding actor-camera command "
                          "in source order. Branches and other scene actions are not played. "
                          "The selected background is a reference image for this channel.");
        selectSequence(qint32(selected.operands[0]), qint32(selected.operands[1]),
                       qint32(selected.operands[2]), std::move(camera));
    } catch (const std::exception &e) {
        fail(e);
    }
}
void TransformPreview::reload(bool newResource) {
    stop();
    try {
        if (newResource || !asset_) {
            asset_ = SceneTransformAsset::load(rom_, resource_->value());
            const QSignalBlocker block(group_);
            group_->setRange(0, qMax(0, int(asset_->groups.size()) - 1));
            if (newResource)
                group_->setValue(0);
        }
        SceneTransforms state;
        state.start(*asset_, group_->value(), duration_->value());
        state_ = std::move(state);
        checkpoints_.clear();
        checkpoints_.insert(0, *state_);
        tick_ = 0;
        const qint64 total =
            duration_->value() < 0
                ? 10000
                : qint64(asset_->groups[group_->value()].size() - 1) * duration_->value();
        const QSignalBlocker block(position_);
        position_->setRange(0, int(qMin<qint64>(10000, total)));
        position_->setValue(0);
        position_->setEnabled(true);
        seek(0);
    } catch (const std::exception &e) {
        fail(e);
    }
}
void TransformPreview::seek(int tick) {
    if (!state_)
        return;
    try {
        tick = qBound(0, tick, position_->maximum());
        if (tick < tick_) {
            auto checkpoint = checkpoints_.upperBound(tick);
            --checkpoint;
            state_ = *checkpoint;
            tick_ = checkpoint.key();
        }
        while (tick_ < tick) {
            state_->advance();
            ++tick_;
            if (tick_ % 120 == 0)
                checkpoints_.insert(tick_, *state_);
        }
        const QSignalBlocker block(position_);
        position_->setValue(tick_);
        const bool canAdvance = !state_->done() && tick_ < position_->maximum();
        if (!canAdvance)
            stop();
        play_->setEnabled(canAdvance);
        step_->setEnabled(canAdvance);
        draw();
    } catch (const std::exception &e) {
        fail(e);
    }
}
void TransformPreview::draw() {
    if (!state_)
        return;
    try {
        state_->prepareMatrices(rom_, camera_, channel_->value() + 1);
        const auto matrix = state_->layerMatrix(channel_->value());
        QImage image;
        QPoint origin;
        if (image_) {
            const auto &raster = image_->composite;
            image = QImage(reinterpret_cast<const uchar *>(raster.rgba.constData()), raster.width,
                           raster.height, raster.width * 4, QImage::Format_RGBA8888);
            origin = image_->origin;
        } else {
            image = QImage(80, 80, QImage::Format_RGBA8888);
            image.fill(QColor("#355467"));
            QPainter p(&image);
            p.setPen(QColor("#a6c8db"));
            for (int at = 0; at < 80; at += 10) {
                p.drawLine(at, 0, at, 79);
                p.drawLine(0, at, 79, at);
            }
            origin = {-40, -40};
        }
        frame_ = QImage(320, 240, QImage::Format_RGBA8888);
        frame_.fill(QColor("#171c24"));
        const auto plane =
            camera_.projectPackedPlane(matrix, {double(origin.x()), -double(origin.y()), 0},
                                       {1, 0, 0}, {0, -1, 0}, image.size());
        const auto &m = plane.mapping;
        const QTransform mapping(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
        const bool drawable = plane.clip.size() >= 3 && mapping.isInvertible();
        if (drawable) {
            QPainter painter(&frame_);
            QPainterPath clip;
            clip.addPolygon(QPolygonF(plane.clip));
            painter.setClipPath(clip);
            painter.setWorldTransform(mapping);
            painter.drawImage(QPointF(0, 0), image);
        }
        canvas_->clear();
        canvas_->addPixmap(QPixmap::fromImage(frame_));
        canvas_->setSceneRect(0, 0, 320, 240);
        view_->fitInView(canvas_->sceneRect(), Qt::KeepAspectRatio);
        const auto record = state_->channel(channel_->value());
        setProperty("channelRecord", record);
        auto real = [&](int at) { return std::bit_cast<float>(u32(record, at)); };
        status_->setText(
            QString("Tick %1 Â· %2\nPosition %3, %4, %5 Â· rotation %6, %7 Â· scale %8\n%9%10%11")
                .arg(tick_)
                .arg(state_->done()
                         ? "Sequence complete"
                         : QString("%1 ticks remaining in segment").arg(state_->remaining()))
                .arg(real(0x40))
                .arg(real(0x44))
                .arg(real(0x50))
                .arg(real(0x48))
                .arg(real(0x4c))
                .arg(real(0x54))
                .arg(image_ ? "Selected background reference."
                            : "Reference grid; no background image selected.")
                .arg(drawable ? "" : " Plane is outside the view or has zero projected area.")
                .arg(position_->maximum() == 10000 ? " Preview is limited to 10,000 updates."
                                                   : ""));
    } catch (const std::exception &e) {
        fail(e);
    }
}
} // namespace lordly
