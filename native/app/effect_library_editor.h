#pragma once
#include "app/effect_artwork.h"
#include "engine/project.h"
#include <QElapsedTimer>
#include <QWidget>
class QListWidget;
class QLineEdit;
class QLabel;
class QPushButton;
class QUndoStack;
namespace lordly {
class EffectLibraryEditor : public QWidget {
    Q_OBJECT
  public:
    EffectLibraryEditor(Project *, QUndoStack *, std::function<void()> changed, QWidget * = nullptr);
    ~EffectLibraryEditor() override;
    void selectEffect(int);
    void selectAttack(int);
    QString saveArtwork(const QString &name = {});
    void exportArtwork(const QString &) const;
  signals:
    void editArtwork(const QString &id);
  protected:
    void showEvent(QShowEvent *) override;
  private:
    void refreshSources();
    void loadSelection();
    void filter();
    void refreshPiece();
    void showFrame();
    QImage selectedImage() const;
    void guarded(const std::function<void()> &);
    Project *project_;
    QUndoStack *undo_;
    QLineEdit *search_;
    QListWidget *sources_, *pieces_;
    QLabel *preview_, *piecePreview_, *details_, *notice_;
    QPushButton *save_, *export_, *edit_, *play_;
    QString sourceName_, savedId_;
    int requested_ = -1, generation_ = 0;
    bool playing_ = false;
    qint64 pausedMs_ = 0;
    QElapsedTimer clock_;
    AttackVisual visual_;
    EffectClip clip_;
    QMap<quint32, QImage> images_;
    std::shared_ptr<std::atomic_bool> canceled_;
};
} // namespace lordly
