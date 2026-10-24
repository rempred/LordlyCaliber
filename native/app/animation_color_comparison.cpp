#include "app/animation_color_comparison.h"
#include "engine/sprite_colors.h"
#include "engine/sprite_object.h"
#include <QCheckBox>
#include <QLabel>
#include <QPainter>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
#include <algorithm>
namespace lordly {
namespace {
class ComparisonCanvas : public QWidget {
  public:
    QImage original, game;
    using QWidget::QWidget;

  protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        const int gap = 12, half = (width() - gap) / 2;
        for (int side = 0; side < 2; ++side) {
            const QRect box(side * (half + gap), 0, half, height());
            p.save();
            p.setClipRect(box);
            for (int y = 0; y < height(); y += 16)
                for (int x = 0; x < half; x += 16)
                    p.fillRect(QRect(box.x() + x, y, 16, 16),
                               ((x / 16 + y / 16) & 1) ? QColor("#272c34") : QColor("#313640"));
            const auto &image = side ? game : original;
            if (!image.isNull()) {
                const double scale = std::min(double(half - 16) / image.width(),
                                              double(height() - 16) / image.height());
                const QSizeF size(image.width() * scale, image.height() * scale);
                p.setRenderHint(QPainter::SmoothPixmapTransform, false);
                p.drawImage(
                    QRectF(QPointF(box.center()) - QPointF(size.width(), size.height()) / 2, size),
                    image);
            } else {
                p.setPen(Qt::white);
                p.drawText(box.adjusted(8, 8, -8, -8), Qt::AlignCenter | Qt::TextWordWrap,
                           "Choose source artwork to compare colors");
            }
            p.restore();
        }
    }
};
} // namespace
AnimationColorComparison::AnimationColorComparison(QWidget *parent) : QWidget(parent) {
    setObjectName("animationColorComparison");
    auto layout = new QVBoxLayout(this);
    auto profile = new QLabel(
        "Use sRGB for artwork. Embedded color profiles are converted automatically; images "
        "without a usable profile are treated as sRGB. The game does not read color profiles: "
        "it stores 32 levels each of red, green and blue, plus separate transparency.");
    profile->setWordWrap(true);
    profile->setObjectName("animationColorProfileHelp");
    layout->addWidget(profile);
    dither_ = new QCheckBox("Dither colors — soften gradient bands");
    layout->addWidget(dither_);
    auto help =
        new QLabel("Dithering mixes nearby shades in a small pixel pattern. It can smooth "
                   "gradients but make pixel art look grainy. Leave it off for crisp pixels; "
                   "compare both results. Transparency is unchanged.");
    help->setWordWrap(true);
    layout->addWidget(help);
    frameRow_ = new QWidget;
    auto controls = new QHBoxLayout(frameRow_);
    controls->setContentsMargins(0, 0, 0, 0);
    controls->addWidget(new QLabel("Compare frame"));
    frame_ = new QSpinBox;
    frame_->setObjectName("animationColorFrame");
    frame_->setRange(1, 1);
    frame_->setKeyboardTracking(false);
    controls->addWidget(frame_);
    controls->addStretch();
    layout->addWidget(frameRow_);
    auto labels = new QHBoxLayout;
    for (const auto &text :
         {"<b>Original</b> · displayed in sRGB", "<b>Game result</b> · reduced color precision"}) {
        auto label = new QLabel(text);
        label->setWordWrap(true);
        labels->addWidget(label, 1);
    }
    layout->addLayout(labels);
    canvas_ = new ComparisonCanvas;
    canvas_->setMinimumSize(260, 150);
    layout->addWidget(canvas_, 1);
    connect(frame_, &QSpinBox::valueChanged, this, [this] { refresh(); });
    connect(dither_, &QCheckBox::toggled, this, [this] { refresh(); });
}
void AnimationColorComparison::setAsset(const SpriteAsset &asset) {
    const bool same = asset_.id == asset.id;
    asset_ = spriteToSrgb(asset);
    const QSignalBlocker block(frame_);
    frame_->setRange(1, int(asset_.frames.size()));
    if (!same)
        frame_->setValue(1);
    refresh();
}
void AnimationColorComparison::clear() {
    asset_ = {};
    refresh();
}
void AnimationColorComparison::setFrame(int frame) {
    frame_->setValue(frame + 1);
}
void AnimationColorComparison::setFrameSelectorVisible(bool visible) {
    frameRow_->setVisible(visible);
}
void AnimationColorComparison::refresh() {
    original_ = {};
    game_ = {};
    if (!asset_.frames.isEmpty()) {
        original_ = asset_.composite(frame_->value() - 1);
        const auto converted =
            SpriteObject::fromImage(
                {original_.width(), original_.height(),
                 QByteArray(reinterpret_cast<const char *>(original_.constBits()),
                            original_.sizeInBytes())},
                dither_->isChecked())
                .frame(0)
                .image;
        game_ = QImage(reinterpret_cast<const uchar *>(converted.rgba.constData()), converted.width,
                       converted.height, QImage::Format_RGBA8888)
                    .copy();
    }
    auto canvas = static_cast<ComparisonCanvas *>(canvas_);
    canvas->original = original_;
    canvas->game = game_;
    canvas->update();
}
} // namespace lordly
