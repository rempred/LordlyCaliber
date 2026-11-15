#pragma once
#include "engine/scene_actions.h"
#include "engine/scene_programs.h"
#include <QGraphicsView>
namespace lordly {
// A projection of the native program and observed execution times. It never
// persists a second schedule that can disagree with the exported commands.
class SceneTimeline : public QGraphicsView {
    Q_OBJECT
  public:
    explicit SceneTimeline(QWidget *parent = nullptr);
    void setProgram(SceneId, const DirectorProgram &);
    void updatePlayback(SceneId, const QMap<int, int> &, int activeWord, int tick);
    void selectCommand(int);
  signals:
    void commandSelected(int);
    void commandActivated(int);
    void timeSelected(int);
  protected:
    void mousePressEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
  private:
    void rebuild();
    SceneId id_;
    DirectorProgram program_;
    QMap<int, int> times_;
    QGraphicsLineItem *playhead_ = nullptr;
    int command_ = -1, tick_ = 0;
};
}
