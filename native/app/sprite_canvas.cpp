#include "app/sprite_editor.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <cmath>
namespace lordly {
SpriteCanvas::SpriteCanvas(QWidget *parent) : QWidget(parent) {
    setObjectName("spriteCanvas");
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setZoom(8);
}
void SpriteCanvas::setAsset(const SpriteAsset *asset, int frame, int layer, bool reset) {
    cancelStroke();
    if (!indexedPalette_.isEmpty())
        color_ = indexedPalette_[indexColor_];
    indexedPalette_.clear();
    reset =
        reset || !asset || !asset_ || asset_->id != asset->id || frame_ != frame || layer_ != layer;
    asset_ = asset ? std::optional<SpriteAsset>(*asset) : std::nullopt;
    frame_ = frame;
    layer_ = layer;
    if (reset)
        selection_ = {};
    if (asset_)
        selection_ = selection_.intersected(QRect({}, asset_->size));
    updatePreview();
    setZoom(zoom_);
}
void SpriteCanvas::setIndexedAsset(const QString &identity, QSize size, const QByteArray &indices,
                                   const QVector<QColor> &palette, quint8 transparent) {
    if (size.width() < 1 || size.height() < 1 || size.width() > 4096 || size.height() > 4096 ||
        indices.size() != qsizetype(size.width()) * size.height() || palette.size() != 256 ||
        palette[transparent].alpha() != 0)
        throw Error("Invalid indexed canvas dimensions or palette");
    // Keep indices as opaque channel values while reusing the exact pixel tools.
    // Color resolution happens only at presentation; equal palette colors stay distinct.
    QImage encoded(size, QImage::Format_RGBA8888);
    for (int y = 0; y < size.height(); ++y)
        for (int x = 0; x < size.width(); ++x)
            encoded.setPixelColor(x, y, {quint8(indices[y * size.width() + x]), 0, 0, 255});
    auto asset = SpriteAsset::fromImage("Indexed artwork", encoded);
    asset.id = identity;
    setAsset(&asset, 0, 0);
    indexedPalette_ = palette;
    transparentIndex_ = transparent;
    setIndexColor(indexColor_);
    update();
}
void SpriteCanvas::setIndexColor(quint8 index) {
    indexColor_ = index;
    color_ = {index, 0, 0, 255};
}
void SpriteCanvas::setZoom(int value) {
    zoom_ = std::clamp(value, 1, 32);
    setFixedSize(asset_ ? asset_->size * zoom_ : QSize(320, 240));
    update();
}
void SpriteCanvas::setTool(Tool tool) {
    cancelStroke();
    tool_ = tool;
}
QPoint SpriteCanvas::pixel(QPointF point) const {
    return {int(std::floor(point.x() / zoom_)), int(std::floor(point.y() / zoom_))};
}
void SpriteCanvas::updatePreview() {
    preview_ = asset_ ? asset_->composite(frame_) : QImage();
    update();
}
void SpriteCanvas::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), QColor(48, 51, 56));
    if (!asset_)
        return;
    // A small repeatable texture avoids per-pixel checker drawing on large canvases.
    QPixmap checker(16, 16);
    checker.fill(QColor(214, 218, 224));
    {
        QPainter p(&checker);
        p.fillRect(0, 0, 8, 8, QColor(241, 243, 246));
        p.fillRect(8, 8, 8, 8, QColor(241, 243, 246));
    }
    painter.fillRect(rect(), QBrush(checker));
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    if (indexedPalette_.isEmpty())
        painter.drawImage(rect(), preview_);
    else {
        QImage visible(preview_.size(), QImage::Format_RGBA8888);
        for (int y = 0; y < visible.height(); ++y)
            for (int x = 0; x < visible.width(); ++x) {
                const auto pixel = preview_.pixelColor(x, y);
                visible.setPixelColor(
                    x, y, indexedPalette_[pixel.alpha() ? pixel.red() : transparentIndex_]);
            }
        painter.drawImage(rect(), visible);
    }
    if (grid_ && zoom_ >= 6) {
        painter.setPen(QColor(50, 55, 65, 65));
        const auto visible = visibleRegion().boundingRect();
        for (int x = std::max(0, visible.left() / zoom_) * zoom_; x <= visible.right(); x += zoom_)
            painter.drawLine(x, visible.top(), x, visible.bottom());
        for (int y = std::max(0, visible.top() / zoom_) * zoom_; y <= visible.bottom(); y += zoom_)
            painter.drawLine(visible.left(), y, visible.right(), y);
    }
    painter.setPen(QPen(QColor(30, 150, 220), 1, Qt::DashLine));
    const auto &layer = asset_->frames[frame_].layers[layer_];
    painter.drawRect(
        QRect(layer.position * zoom_, layer.image.size() * zoom_).adjusted(0, 0, -1, -1));
    if (!selection_.isEmpty()) {
        const auto r =
            QRect(selection_.topLeft() * zoom_, selection_.size() * zoom_).adjusted(0, 0, -1, -1);
        painter.setPen(QPen(Qt::black, 1));
        painter.drawRect(r);
        painter.setPen(QPen(Qt::white, 1, Qt::DashLine));
        painter.drawRect(r);
    }
    const auto anchor = asset_->anchor * zoom_;
    painter.setPen(QPen(QColor(218, 50, 113), 2));
    painter.drawLine(anchor - QPoint(5, 0), anchor + QPoint(5, 0));
    painter.drawLine(anchor - QPoint(0, 5), anchor + QPoint(0, 5));
}
void SpriteCanvas::cancelStroke() {
    if (!drawing_)
        return;
    if (asset_) {
        auto &layer = asset_->frames[frame_].layers[layer_];
        layer.image = before_;
        layer.position = beforePosition_;
    }
    selection_ = beforeSelection_;
    drawing_ = false;
    updatePreview();
}
void SpriteCanvas::mousePressEvent(QMouseEvent *event) {
    if (!asset_ || event->button() != Qt::LeftButton)
        return;
    setFocus();
    const auto p = pixel(event->position());
    if (!QRect({}, asset_->size).contains(p))
        return;
    if (tool_ == Tool::Pick) {
        if (indexedPalette_.isEmpty())
            emit colorPicked(preview_.pixelColor(p));
        else {
            const auto pixel = preview_.pixelColor(p);
            emit indexPicked(pixel.alpha() ? pixel.red() : transparentIndex_);
        }
        return;
    }
    auto &layer = asset_->frames[frame_].layers[layer_];
    before_ = layer.image;
    beforePosition_ = layer.position;
    beforeSelection_ = selection_;
    start_ = last_ = p;
    drawing_ = true;
    applyStroke(p);
}
void SpriteCanvas::applyStroke(QPoint p) {
    auto &layer = asset_->frames[frame_].layers[layer_];
    if (tool_ == Tool::Select) {
        selection_ = QRect(start_, p).normalized().intersected(QRect({}, asset_->size));
    } else if (tool_ == Tool::Move) {
        const QPoint delta = p - start_;
        if (beforeSelection_.isEmpty()) {
            layer.position = {std::clamp(beforePosition_.x() + delta.x(), -32768, 32767),
                              std::clamp(beforePosition_.y() + delta.y(), -32768, 32767)};
        } else {
            const auto source =
                beforeSelection_.translated(-beforePosition_).intersected(before_.rect());
            layer.image = spriteMoveSelection(before_, source, delta);
            selection_ = beforeSelection_.translated(delta).intersected(QRect({}, asset_->size));
        }
    } else {
        const auto local = p - layer.position;
        auto selected =
            selection_.isEmpty()
                ? QRect()
                : selection_.translated(-layer.position).intersected(layer.image.rect());
        // A selection outside this layer must not turn into an unrestricted brush.
        if (!selection_.isEmpty() && selected.isEmpty())
            return;
        if (tool_ == Tool::Fill) {
            if (p == start_)
                layer.image = spriteFill(std::move(layer.image), local, color_, selected);
        } else {
            auto bounded = [](QPoint pt) {
                return QPoint(std::clamp(pt.x(), -4096, 8192), std::clamp(pt.y(), -4096, 8192));
            };
            layer.image = spriteLine(
                std::move(layer.image), bounded(last_ - layer.position), bounded(local),
                tool_ == Tool::Eraser ? QColor(Qt::transparent) : color_, brush_, selected);
        }
    }
    last_ = p;
    updatePreview();
}
void SpriteCanvas::mouseMoveEvent(QMouseEvent *event) {
    const auto p = pixel(event->position());
    emit cursorChanged(p);
    if (drawing_ && tool_ != Tool::Fill)
        applyStroke(p);
}
void SpriteCanvas::mouseReleaseEvent(QMouseEvent *event) {
    if (!drawing_ || event->button() != Qt::LeftButton)
        return;
    if (tool_ != Tool::Fill)
        applyStroke(pixel(event->position()));
    drawing_ = false;
    const auto &layer = asset_->frames[frame_].layers[layer_];
    if (layer.image != before_ || layer.position != beforePosition_) {
        // The receiver may replace asset_ synchronously while refreshing the workspace.
        const auto image = layer.image;
        const auto position = layer.position;
        if (indexedPalette_.isEmpty())
            emit committed(image, position);
        else {
            QByteArray indices(image.width() * image.height(), char(transparentIndex_));
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x) {
                    const auto pixel = image.pixelColor(x, y);
                    indices[y * image.width() + x] =
                        char(pixel.alpha() ? pixel.red() : transparentIndex_);
                }
            emit indicesCommitted(indices, position);
        }
    }
    before_ = {};
}
void SpriteCanvas::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        if (drawing_)
            cancelStroke();
        else
            clearSelection();
        event->accept();
    } else
        QWidget::keyPressEvent(event);
}
} // namespace lordly
