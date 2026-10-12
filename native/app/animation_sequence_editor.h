#pragma once
#include "engine/project.h"
#include <QWidget>
#include <QSet>
#include <functional>
class QComboBox;
class QLabel;
class QSpinBox;
class QTableWidget;
class QUndoStack;
class QPushButton;
class QListWidget;
class QCheckBox;
class QSplitter;
namespace lordly {
class ActorPoseEditor;
class AnimationSequenceEditor : public QWidget {
    Q_OBJECT
  public:
    AnimationSequenceEditor(Project *, QUndoStack *, std::function<void()> changed,
                            QWidget *parent = nullptr);
    void importImage(const QString &);
    void copyToLibrary();
    void importSequence(const SpriteAsset &);
    void copySequence(int sourceClass, int sourceRoute, int sourcePose, bool controls = true);
    void copyFrame(int sourceClass, int sourceRoute, int sourcePose, int sourceRecord, bool duration);
    void translateFrames(const QMap<int, QSet<int>> &, QPoint, int layer = -1, bool sceneSpace = false);
    void setContext(int classId, int flags, int pose);
    void setGuided(bool);
    void assignAnimation(int action, bool blocked, int pose);
    void moveVisibleFrame(int from, int to);

  private:
    void refresh();
    void selectRecord();
    void editLayers();
    void editSprite();
    void chooseSource(bool compare, int destinationRecord = -1, bool duration = false);
    void chooseTranslation();
    void prepareImage(const QImage &);
    void guarded(const std::function<void()> &);
    void apply(AnimationSequences, const QString &);
    void publish(const Project &, const QString &);
    void refreshAssignments();
    void edit(const QString &, const std::function<void(PoseBank &, int, int)> &);
    AnimationSequences editableSequences();
    void buildFrameWorkspace();
    void refreshFrames();
    void selectVisibleFrames();
    void updateFrameTools();
    void changeFrameDuration(int);
    void duplicateVisibleFrames();
    void removeVisibleFrames();
    void moveFrameArtwork(QPoint, int layer = -1);
    QSet<int> selectedFrameRecords() const;
    int selectedClass() const;
    int owner(int classId) const;
    AnimationDescriptor descriptor() const;
    Project *project_;
    QUndoStack *undo_;
    std::function<void()> changed_;
    bool refreshing_ = false;
    int preferredPose_ = 40;
    QComboBox *classes_ = nullptr, *routes_ = nullptr, *poses_ = nullptr;
    QComboBox *library_ = nullptr;
    QComboBox *actions_ = nullptr, *attackModes_ = nullptr;
    QLabel *assignmentSummary_ = nullptr;
    QSpinBox *libraryFrame_ = nullptr;
    QTableWidget *records_ = nullptr;
    QSpinBox *frame_ = nullptr, *ticks_ = nullptr;
    QLabel *notice_ = nullptr, *summary_ = nullptr;
    QWidget *edits_ = nullptr;
    QPushButton *create_ = nullptr, *remove_ = nullptr;
    ActorPoseEditor *preview_ = nullptr;
    QVector<QWidget *> advanced_;
    QWidget *assignmentControls_ = nullptr;
    bool guided_ = false, durationTyped_ = false;
    QWidget *rawWorkspace_ = nullptr, *frameWorkspace_ = nullptr, *frameTools_ = nullptr;
    QWidget *framePreviewHost_ = nullptr;
    QSplitter *rawSplit_ = nullptr;
    QListWidget *frames_ = nullptr;
    QSpinBox *duration_ = nullptr;
    QLabel *frameHeading_ = nullptr, *frameSelection_ = nullptr, *frameStatus_ = nullptr;
    QCheckBox *onion_ = nullptr;
    QPushButton *earlier_ = nullptr, *later_ = nullptr, *paint_ = nullptr, *layersButton_ = nullptr;
    QString frameContext_;
    int pendingRecord_ = -1;
};
} // namespace lordly
