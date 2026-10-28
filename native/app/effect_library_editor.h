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
    EffectLibraryEditor(Project *, QUndoStack *, std::function<void()> changed, QWidget * = nullptr, bool picker = false);
    ~EffectLibraryEditor() override;
    void selectEffect(int);
    void selectAttack(int);
    void selectSavedEffect(const QString &);
    SpriteAsset selectedPiece() const;
    QString saveArtwork(const QString &name = {});
    void exportArtwork(const QString &) const;
    bool exportSequence(const QString &, EffectExport);
  signals:
    void editArtwork(const QString &id);
    void pieceChosen(const SpriteAsset &);
  protected:
    void showEvent(QShowEvent *) override;
  private:
    void refreshSources();
    void compose(bool fromPiece, bool editSaved = false);
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
    QPushButton *editEffect_, *assignEffect_, *moreEffect_;
    bool picker_ = false;
    int requested_ = -1, generation_ = 0;
    bool playing_ = false;
    qint64 pausedMs_ = 0;
    QElapsedTimer clock_;
    AttackVisual visual_;
    EffectClip clip_;
    std::optional<SpriteAsset> rendered_;
    QMap<quint32, QImage> images_;
    std::shared_ptr<std::atomic_bool> canceled_;
};
} // namespace lordly
