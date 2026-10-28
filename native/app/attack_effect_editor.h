#pragma once
#include "engine/project.h"
#include <QDialog>
#include <QElapsedTimer>
#include <atomic>
#include <memory>
class QLabel;
class QSpinBox;
class QListWidget;
class QPushButton;
class QTimer;
class QComboBox;
class QFormLayout;
namespace lordly {
class AttackEffectEditor : public QDialog {
    Q_OBJECT
  public:
    AttackEffectEditor(Project *, int attack, int classId, QWidget *parent = nullptr);
    ~AttackEffectEditor() override;
    AttackVisual visual() const {
        return visual_;
    }
    void setVisual(const AttackVisual &);
    void importFiles(const QStringList &);
    void importLibrary(const QString &);
    void copyFromAttack(int attack);
    void replaceArtwork(quint32 key, const QImage &);
    void exportRenderedFrame(const QString &) const;
    QImage renderedFrame() const;

  private:
    void refresh();
    void render();
    void showFrame();
    int currentTick() const;
    void chooseRom();
    void chooseLibrary();
    void chooseAttack();
    void refreshArtwork();
    void refreshArtworkColour();
    void editArtworkColour(const std::function<void(EffectArtwork &)> &);
    void chooseArtworkFile();
    void chooseArtworkLibrary();
    void paintFrame();
    void resizeFrames();
    void record(const AttackVisual &);
    void history(int);
    void guarded(const std::function<void()> &);
    Project *project_;
    int attack_, classId_, inherited_, generation_ = 0;
    AttackVisual visual_;
    QVector<AttackVisual> history_;
    int historyIndex_ = 0;
    bool refreshing_ = false, playing_ = false, enemyPreview_ = false;
    QLabel *preview_, *source_, *notice_, *comparison_;
    QLabel *framePosition_;
    QPushButton *step_, *exportFrame_;
    QListWidget *frames_;
    QSpinBox *delay_, *ticks_, *originX_, *originY_;
    QPushButton *play_, *undo_, *redo_, *paint_, *resize_;
    QWidget *frameControls_;
    QWidget *nativeControls_;
    QFormLayout *nativeForm_, *artworkColourForm_;
    QComboBox *colourMode_;
    QSpinBox *hue_, *effectSize_, *effectX_, *effectY_;
    QPushButton *effectColour_, *replaceArtwork_, *restoreArtwork_;
    QWidget *legacyColour_;
    QListWidget *artworkList_;
    QMap<quint32, EffectArtwork> sourceArtwork_;
    QTimer *timer_;
    QElapsedTimer clock_;
    QVector<QImage> clip_;
    QVector<int> holds_;
    int clipTicks_ = 0;
    qint64 pausedMs_ = 0;
    std::shared_ptr<std::atomic_bool> canceled_;
};
} // namespace lordly
