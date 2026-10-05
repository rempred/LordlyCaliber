#pragma once
#include "engine/project.h"
#include "engine/sprite_video.h"
#include <QElapsedTimer>
#include <QWidget>
#include <functional>
class QUndoStack;
class QListWidget;
class QSpinBox;
class QLabel;
class QLineEdit;
class QCheckBox;
class QPushButton;
class QTimer;
namespace lordly {
class SpriteCanvas : public QWidget {
    Q_OBJECT
  public:
    enum class Tool { Pencil, Eraser, Fill, Pick, Select, Move };
    explicit SpriteCanvas(QWidget *parent = nullptr);
    void setAsset(const SpriteAsset *, int frame, int layer, bool resetSelection = false);
    void setIndexedAsset(const QString &identity,QSize,const QByteArray &indices,
                         const QVector<QColor> &palette,quint8 transparentIndex);
    void setIndexColor(quint8 index);
    void setZoom(int);
    void setGrid(bool value) {
        grid_ = value;
        update();
    }
    void setTool(Tool);
    void setColor(QColor color) {
        color_ = color;
    }
    void setBrush(int brush) {
        brush_ = brush;
    }
    QRect selection() const {
        return selection_;
    }
    void clearSelection() {
        selection_ = {};
        update();
    }
    void cancelStroke();
    int zoom() const {
        return zoom_;
    }
  signals:
    void committed(const QImage &, QPoint);
    void colorPicked(QColor);
    void indexPicked(int);
    void indicesCommitted(const QByteArray &,QPoint);
    void cursorChanged(QPoint);

  protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;

  private:
    QPoint pixel(QPointF) const;
    void applyStroke(QPoint);
    void updatePreview();
    std::optional<SpriteAsset> asset_;
    QImage preview_, before_;
    QPoint beforePosition_, start_, last_;
    QRect selection_, beforeSelection_;
    int frame_ = 0, layer_ = 0, zoom_ = 8, brush_ = 1;
    bool grid_ = true, drawing_ = false;
    Tool tool_ = Tool::Pencil;
    QColor color_ = Qt::black;
    QVector<QColor> indexedPalette_;
    quint8 transparentIndex_=0,indexColor_=0;
};
class SpriteEditor : public QWidget {
    Q_OBJECT
  public:
    SpriteEditor(Project *, QUndoStack *, std::function<void()> changed, QWidget *parent = nullptr);
    ~SpriteEditor() override;
    void addAsset(SpriteAsset);
    void selectAsset(const QString &);
    void importFile(const QString &);
    void importLayerImage(const QString &);
    void exportAsset(const QString &, bool browserCompatible) const;
    void exportImage(const QString &, const QByteArray &format) const;
    bool exportVideo(const QString &, const SpriteVideoOptions &);
    QString selectedAsset() const {
        return selected_;
    }

  private:
    const SpriteAsset *current() const;
    void refresh();
    void refreshFrame(bool resetSelection = false);
    void change(const QString &, const std::function<void(SpriteAsset &)> &);
    void commit(const QString &, SpriteLibrary);
    void guarded(const std::function<void()> &);
    void createAsset();
    void chooseVideoExport();
    void resizeAsset();
    void copyPixels(bool cut);
    void pastePixels();
    void play(bool);
    void tick();
    void updateColor(QColor);
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    QString selected_;
    QListWidget *assets_, *frames_, *layers_;
    QLineEdit *search_, *name_, *frameName_, *layerName_;
    QSpinBox *ticks_, *anchorX_, *anchorY_, *layerX_, *layerY_;
    QCheckBox *visible_;
    QLabel *notice_, *info_, *cursor_;
    QPushButton *colorButton_, *play_;
    SpriteCanvas *canvas_;
    QWidget *editing_;
    QTimer *timer_;
    QColor color_ = Qt::black;
    bool refreshing_ = false;
    int playTicks_ = 0;
    QElapsedTimer playbackClock_;
    qint64 playedTicks_ = 0;
};
} // namespace lordly
