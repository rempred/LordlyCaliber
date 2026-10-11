#pragma once
#include "engine/rom.h"
#include <QMap>
#include <QPoint>
#include <QSet>
#include <QVector>
#include <optional>
namespace lordly {
struct StoryRoute {
    int node = 0, event = 0, clearFrom = 0, clearTo = 0;
    QMap<int, int> requirements, writes;
    bool operator==(const StoryRoute &) const = default;
};
struct StoryNode {
    QString name;
    QPoint position;
};
// Port of the standalone Scenario Seeder's bounded world-entry interpreter.
// These are ordinary world-map entries, not transplanted mission suspends.
class StoryRoutes {
  public:
    explicit StoryRoutes(const Rom &);
    QVector<StoryRoute> routes(int node, int chaos = 50, QMap<int, int> flags = {}) const;
    QMap<int, QByteArray> scripts;
    QMap<int, QVector<int>> keys;
    QMap<int, StoryNode> nodes;
    QSet<int> travelNodes;
    QVector<QPair<int, int>> edges;
};
class WorldProgress {
  public:
    WorldProgress(const StoryRoutes &, const QByteArray &memory);
    void advanceTo(int event);
    QByteArray forEntry(const StoryRoute &);
    QMap<int, int> flags;

  private:
    void complete(int event);
    void applyList(const QVector<int> &);
    const StoryRoutes &routing_;
    QByteArray memory_;
    QMap<int, int> roads_, nodes_, markers_;
    int key_ = 0;
};
} // namespace lordly
