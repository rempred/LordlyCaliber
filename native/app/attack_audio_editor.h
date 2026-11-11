#pragma once
#include "engine/audio_cues.h"
#include "engine/project.h"
#include <QDialog>
class QTreeWidget;
class QComboBox;
class QLabel;
class QPushButton;
namespace lordly {
class AudioCueView;
class AttackAudioEditor : public QDialog {
  public:
    AttackAudioEditor(Project *, int attack, const AttackVisual &, QWidget *parent = nullptr);
    AttackAudio audio() const {
        return audio_;
    }
    void setAudio(const AttackAudio &);
    void importWav(const QString &, bool replace = false);

  private:
    void refresh();
    void readOriginal();
    void chooseAttack(bool replace);
    AttackAudio editableAudio() const;
    void select();
    void populateParts();
    int selectedRow() const;
    int selectedPart() const;
    void guard(const std::function<void()> &);
    Project *project_;
    int attack_, generation_ = 0;
    AttackVisual visual_;
    AttackAudio audio_;
    QVector<AudioCue> anchors_;
    QVector<AttackSound> originalSounds_;
    QTreeWidget *table_;
    QPushButton *import_, *copy_, *replace_, *remove_, *restore_;
    QLabel *status_, *mode_;
    AudioCueView *audition_;
    bool ready_ = false;
    int desiredRow_ = -1, desiredPart_ = -1;
};
} // namespace lordly
