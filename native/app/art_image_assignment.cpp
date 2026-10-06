#include "app/art_image_assignment.h"
#include "engine/rom.h"
#include "engine/sprite_library.h"
#include "engine/sprite_object.h"
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QImageReader>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <algorithm>
namespace lordly {
QImage loadArtworkImage(const QString &path) {
    QImageReader reader(path);
    const auto size = reader.size();
    if (size.width() < 1 || size.height() < 1 || size.width() > 4096 || size.height() > 4096)
        throw Error("Image dimensions must be 1–4096 pixels");
    const auto image = reader.read();
    if (image.isNull())
        throw Error("Cannot read image: " + reader.errorString());
    return image;
}
std::optional<QByteArray>
prepareArtworkImage(QWidget *parent, const QImage &source, const ArtImageAssignment &options,
                    const std::function<ArtImageMapping(const QImage &, int, bool)> &convert) {
    if (source.isNull() || source.width() > 4096 || source.height() > 4096 ||
        options.target.isEmpty())
        throw Error("Invalid artwork image dimensions");
    QDialog dialog(parent);
    dialog.setObjectName(options.objectPrefix + "ImagePreparation");
    dialog.setWindowTitle(options.title);
    auto layout = new QVBoxLayout(&dialog);
    auto description = new QLabel(options.description);
    description->setWordWrap(true);
    layout->addWidget(description);
    auto previews = new QHBoxLayout;
    layout->addLayout(previews);
    auto input = new QLabel, output = new QLabel;
    for (auto label : {input, output}) {
        label->setMinimumSize(224, 256);
        label->setAlignment(Qt::AlignCenter);
        previews->addWidget(label);
    }
    input->setPixmap(
        QPixmap::fromImage(source.scaled(224, 256, Qt::KeepAspectRatio, Qt::FastTransformation)));
    auto form = new QFormLayout;
    layout->addLayout(form);
    QComboBox fit;
    fit.setObjectName(options.objectPrefix + "ImportFit");
    fit.addItems({"Fit inside", "Fill and crop", "Stretch", "Keep pixel size"});
    form->addRow("Placement", &fit);
    QComboBox sampling;
    sampling.setObjectName(options.objectPrefix + "ImportSampling");
    sampling.addItems({"Nearest pixel", "Smooth"});
    form->addRow("Sampling", &sampling);
    QDoubleSpinBox zoom;
    zoom.setObjectName(options.objectPrefix + "ImportZoom");
    zoom.setRange(.125, 8);
    zoom.setValue(1);
    zoom.setSingleStep(.125);
    form->addRow("Zoom", &zoom);
    QSpinBox x, y, alpha;
    x.setObjectName(options.objectPrefix + "ImportX");
    y.setObjectName(options.objectPrefix + "ImportY");
    for (auto spin : {&x, &y})
        spin->setRange(-4096, 4096);
    form->addRow("Offset X (output pixels)", &x);
    form->addRow("Offset Y (output pixels)", &y);
    alpha.setRange(1, 255);
    alpha.setValue(128);
    QColor background = Qt::black;
    QPushButton fill("Black");
    if (options.opaque)
        form->addRow("Opaque background", &fill);
    else if (options.binaryAlpha)
        form->addRow("Opaque from alpha", &alpha);
    QCheckBox dither("Ordered dithering");
    dither.setObjectName(options.objectPrefix + "ImportDither");
    dither.setToolTip("Use a repeating 4 × 4 color pattern to soften color-reduction bands. "
                      "Opacity is unchanged.");
    if (options.allowDither)
        form->addRow("Color reduction", &dither);
    auto status = new QLabel;
    status->setWordWrap(true);
    layout->addWidget(status);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons.button(QDialogButtonBox::Ok)->setText("Assign image");
    layout->addWidget(&buttons);
    QByteArray prepared;
    auto update = [&] {
        try {
            double sx = double(options.target.width()) / source.width(),
                   sy = double(options.target.height()) / source.height();
            if (fit.currentIndex() != 2)
                sx = sy = fit.currentIndex() == 0   ? std::min(sx, sy)
                          : fit.currentIndex() == 1 ? std::max(sx, sy)
                                                    : 1.;
            sx *= zoom.value();
            sy *= zoom.value();
            QImage target(options.target, QImage::Format_RGBA8888);
            target.fill(options.opaque ? background : QColor(Qt::transparent));
            const QSizeF size(source.width() * sx, source.height() * sy);
            const QRectF placement((target.width() - size.width()) / 2 + x.value(),
                                    (target.height() - size.height()) / 2 + y.value(),
                                    size.width(), size.height());
            if (!options.opaque && sampling.currentIndex() == 0)
                target = spritePlaceNearest(source, options.target, placement);
            else {
                QPainter painter(&target);
                if (!options.opaque) painter.setCompositionMode(QPainter::CompositionMode_Source);
                painter.setRenderHint(QPainter::SmoothPixmapTransform, sampling.currentIndex() == 1);
                painter.drawImage(placement, source);
            }
            const auto mapped =
                convert(target, alpha.value(), options.allowDither && dither.isChecked());
            if (mapped.image.size() != options.target || mapped.data.isEmpty())
                throw Error("Artwork conversion returned an invalid preview");
            prepared = mapped.data;
            const auto scale =
                std::max(1, std::min(224 / mapped.image.width(), 256 / mapped.image.height()));
            output->setPixmap(QPixmap::fromImage(mapped.image.scaled(
                mapped.image.size() * scale, Qt::IgnoreAspectRatio, Qt::FastTransformation)));
            status->setText(QString("Left: source · Right: prepared %1 × %2 image")
                                .arg(target.width())
                                .arg(target.height()));
            buttons.button(QDialogButtonBox::Ok)->setEnabled(true);
        } catch (const std::exception &e) {
            prepared.clear();
            status->setText(QString::fromUtf8(e.what()));
            buttons.button(QDialogButtonBox::Ok)->setEnabled(false);
        }
    };
    QObject::connect(&dither, &QCheckBox::toggled, &dialog, update);
    QObject::connect(&fit, &QComboBox::currentIndexChanged, &dialog, update);
    QObject::connect(&sampling, &QComboBox::currentIndexChanged, &dialog, update);
    QObject::connect(&zoom, &QDoubleSpinBox::valueChanged, &dialog, update);
    for (auto spin : {&x, &y, &alpha})
        QObject::connect(spin, &QSpinBox::valueChanged, &dialog, update);
    QObject::connect(&fill, &QPushButton::clicked, &dialog, [&] {
        const auto color = QColorDialog::getColor(background, &dialog, "Image background");
        if (color.isValid()) {
            background = color;
            fill.setText(color.name());
            update();
        }
    });
    QObject::connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    update();
    if (dialog.exec() != QDialog::Accepted)
        return {};
    return prepared;
}
std::optional<PreparedSpriteSequence> prepareSpriteSequence(QWidget *parent, const SpriteAsset &asset) {
    asset.validate();
    if (asset.frames.size() > 255)
        throw Error("A game pose supports at most 255 instructions, including its controls");
    QDialog dialog(parent);
    dialog.setObjectName("sequenceBatchPreparation");
    dialog.setWindowTitle("Prepare complete frame sequence");
    auto layout = new QVBoxLayout(&dialog);
    auto description = new QLabel(
        "Replace every frame and duration together. The destination's motion, sound and loop "
        "controls are retained. Inspect each converted frame before applying. Cancel leaves "
        "the target unchanged.");
    description->setWordWrap(true);
    layout->addWidget(description);
    auto images = new QHBoxLayout;
    auto input = new QLabel, output = new QLabel;
    for (auto label : {input, output}) {
        label->setMinimumSize(240, 240);
        label->setAlignment(Qt::AlignCenter);
        images->addWidget(label);
    }
    layout->addLayout(images);
    auto form = new QFormLayout;
    layout->addLayout(form);
    QSpinBox frame, width, height, originX, originY, offsetX, offsetY;
    frame.setObjectName("sequenceBatchFrame");
    frame.setRange(1, int(asset.frames.size()));
    frame.setSuffix(QString(" / %1").arg(asset.frames.size()));
    form->addRow("Inspect frame", &frame);
    for (auto spin : {&width, &height}) spin->setRange(1, 4096);
    width.setObjectName("sequenceBatchWidth");
    height.setObjectName("sequenceBatchHeight");
    width.setValue(asset.size.width()); height.setValue(asset.size.height());
    form->addRow("Output width", &width); form->addRow("Output height", &height);
    QComboBox fit;
    fit.setObjectName("sequenceBatchFit");
    fit.addItems({"Original size", "Fit inside", "Fill and crop", "Stretch"});
    form->addRow("Placement", &fit);
    for (auto spin : {&originX, &originY, &offsetX, &offsetY}) spin->setRange(-32768, 32767);
    originX.setObjectName("sequenceBatchOriginX"); originY.setObjectName("sequenceBatchOriginY");
    originX.setValue(-asset.anchor.x()); originY.setValue(-asset.anchor.y());
    offsetX.setObjectName("sequenceBatchOffsetX"); offsetY.setObjectName("sequenceBatchOffsetY");
    form->addRow("Image offset X", &offsetX); form->addRow("Image offset Y", &offsetY);
    form->addRow("Canvas origin X (sprite coordinates)", &originX);
    form->addRow("Canvas origin Y (sprite coordinates)", &originY);
    QCheckBox dither("Ordered color dithering");
    form->addRow(&dither);
    auto status = new QLabel;
    status->setObjectName("sequenceBatchStatus");
    status->setWordWrap(true);
    layout->addWidget(status);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons.setObjectName("sequenceBatchButtons");
    buttons.button(QDialogButtonBox::Ok)->setText("Replace complete sequence");
    layout->addWidget(&buttons);
    const auto convert = [&](int index) {
        const QSize canvas(width.value(), height.value());
        if (quint64(canvas.width()) * canvas.height() * asset.frames.size() > SpriteLibrary::pixelLimit)
            throw Error("Prepared sequence exceeds the sprite library pixel budget");
        double sx = double(canvas.width()) / asset.size.width(),
               sy = double(canvas.height()) / asset.size.height();
        if (fit.currentIndex() != 3)
            sx = sy = fit.currentIndex() == 0 ? 1. : fit.currentIndex() == 1 ? std::min(sx, sy) : std::max(sx, sy);
        const QSizeF size(asset.size.width() * sx, asset.size.height() * sy);
        const QRectF rect((canvas.width() - size.width()) / 2 + offsetX.value(),
                          (canvas.height() - size.height()) / 2 + offsetY.value(), size.width(), size.height());
        const auto rgba = spritePlaceNearest(asset.composite(index), canvas, rect).convertToFormat(QImage::Format_RGBA8888);
        const auto sprite = SpriteObject::fromImage(
            {rgba.width(), rgba.height(), QByteArray(reinterpret_cast<const char *>(rgba.constBits()), rgba.sizeInBytes())},
            dither.isChecked());
        const auto pixels = sprite.frame(0).image;
        return ArtImageMapping{sprite.source(), QImage(reinterpret_cast<const uchar *>(pixels.rgba.constData()),
                                                       pixels.width, pixels.height, pixels.width * 4, QImage::Format_RGBA8888).copy()};
    };
    auto update = [&] {
        try {
            const int index = frame.value() - 1;
            const auto prepared = convert(index);
            input->setPixmap(QPixmap::fromImage(asset.composite(index).scaled(240, 240, Qt::KeepAspectRatio, Qt::FastTransformation)));
            output->setPixmap(QPixmap::fromImage(prepared.image.scaled(240, 240, Qt::KeepAspectRatio, Qt::FastTransformation)));
            status->setText(QString("%1 · %2 ticks · Source %3 × %4 → Output %5 × %6\n"
                                    "Canvas origin (%7, %8). Every frame uses the same placement.")
                                .arg(asset.frames[index].name).arg(asset.frames[index].ticks)
                                .arg(asset.size.width()).arg(asset.size.height()).arg(width.value()).arg(height.value())
                                .arg(originX.value()).arg(originY.value()));
            buttons.button(QDialogButtonBox::Ok)->setEnabled(true);
        } catch (const std::exception &e) {
            status->setText(QString::fromUtf8(e.what()));
            buttons.button(QDialogButtonBox::Ok)->setEnabled(false);
        }
    };
    for (auto spin : {&frame, &width, &height, &originX, &originY, &offsetX, &offsetY})
        QObject::connect(spin, &QSpinBox::valueChanged, &dialog, update);
    QObject::connect(&fit, &QComboBox::currentIndexChanged, &dialog, update);
    QObject::connect(&dither, &QCheckBox::toggled, &dialog, update);
    PreparedSpriteSequence result;
    QObject::connect(&buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        try {
            PreparedSpriteSequence candidate;
            candidate.origin = {originX.value(), originY.value()};
            for (int i = 0; i < asset.frames.size(); ++i) candidate.sprites << convert(i).data;
            result = std::move(candidate);
            dialog.accept();
        } catch (const std::exception &e) { status->setText(QString::fromUtf8(e.what())); }
    });
    QObject::connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    update();
    if (dialog.exec() != QDialog::Accepted) return {};
    return result;
}
} // namespace lordly
