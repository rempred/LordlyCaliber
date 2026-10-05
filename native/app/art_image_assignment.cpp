#include "app/art_image_assignment.h"
#include "engine/rom.h"
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
                    const std::function<ArtImageMapping(const QImage &, int)> &convert) {
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
    sampling.addItems({"Nearest pixel", "Smooth"});
    form->addRow("Sampling", &sampling);
    QDoubleSpinBox zoom;
    zoom.setRange(.125, 8);
    zoom.setValue(1);
    zoom.setSingleStep(.125);
    form->addRow("Zoom", &zoom);
    QSpinBox x, y, alpha;
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
    else
        form->addRow("Opaque from alpha", &alpha);
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
            QPainter painter(&target);
            if (!options.opaque)
                painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.setRenderHint(QPainter::SmoothPixmapTransform, sampling.currentIndex() == 1);
            const QSizeF size(source.width() * sx, source.height() * sy);
            painter.drawImage(QRectF((target.width() - size.width()) / 2 + x.value(),
                                     (target.height() - size.height()) / 2 + y.value(),
                                     size.width(), size.height()),
                              source);
            painter.end();
            const auto mapped = convert(target, alpha.value());
            if (mapped.image.size() != options.target || mapped.data.isEmpty())
                throw Error("Artwork conversion returned an invalid preview");
            prepared = mapped.data;
            const auto scale =
                std::max(1, std::min(224 / mapped.image.width(), 256 / mapped.image.height()));
            output->setPixmap(QPixmap::fromImage(mapped.image.scaled(
                mapped.image.size() * scale, Qt::IgnoreAspectRatio, Qt::FastTransformation)));
            status->setText(QString("Left: source · Right: exact %1 × %2 game image")
                                .arg(target.width())
                                .arg(target.height()));
            buttons.button(QDialogButtonBox::Ok)->setEnabled(true);
        } catch (const std::exception &e) {
            prepared.clear();
            status->setText(QString::fromUtf8(e.what()));
            buttons.button(QDialogButtonBox::Ok)->setEnabled(false);
        }
    };
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
} // namespace lordly
