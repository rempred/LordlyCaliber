#pragma once
#include "engine/audio_cues.h"
#include "engine/rom.h"
#include "engine/sound_preview.h"
#include <QHash>
#include <QWidget>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
class QLabel;
class QTreeWidget;
class QPushButton;
class QSlider;

namespace lordly {
class SoundOutput;
struct SoundPreview;
class AudioCueView : public QWidget {
  public:
    AudioCueView(std::function<void(int)> seek, QWidget *parent = nullptr);
    ~AudioCueView();
    void setRom(const Rom &);
    void setCues(const QVector<AudioCue> &, const QString &usage, bool effect,
                 const QString &boundary = {});
    void setMessage(const QString &);
    void setTick(int tick);
    void setCompact(bool);
    int previewVolume() const;
    void stopAudition();
    std::function<void()> auditionStarted;
    std::function<void()> soundsReady;
    void setSelectedSound(int cue, int part = -1);
    void playSelectedSound();
    std::shared_ptr<const SoundPreview> sound(int cue) const;

  private:
    QLabel *summary_, *position_;
    QTreeWidget *table_;
    QLabel *playback_;
    QPushButton *play_, *stop_;
    QSlider *volume_;
    SoundOutput *output_;
    QVector<AudioCue> cues_;
    QVector<std::shared_ptr<SoundPreview>> decoded_;
    QHash<QByteArray, std::shared_ptr<SoundPreview>> cache_;
    QString usage_, boundary_;
    bool effect_ = true;
    int selectedCue_ = 0, selectedPart_ = -1;
    std::shared_ptr<std::atomic_bool> partsCancel_;
    int partsGeneration_ = 0;
    void rebuildRows();
    std::optional<Rom> rom_;
    void stopSound();
    void playSound();
    void updateSelection();
    void loadParts();
    void clearParts();
};
} // namespace lordly
