#pragma once
#include "engine/battlefield.h"
#include "engine/field_resources.h"
#include "engine/scenario_map.h"
#include <QJsonObject>
namespace lordly {
struct FieldMapSource {
    int key = 0, modelId = 0, geometryIndex = 0, terrainIndex = 0, pointIndex = 0;
    ScenarioMap map;
    Battlefield field;
    QVector<FieldMember> geometry, terrain;
    QByteArray points, visualTerrain;
    QVector<FieldTriangle> terrainFaces;
    QRectF boundary;
};
struct FieldChanges {
    QByteArray paint = QByteArray(65536, char(-1));
    QMap<int, QPointF> sites;
    std::optional<QRectF> boundary;
};
struct FieldPlan {
    int geometryIndex = 0, terrainIndex = 0, pointIndex = 0;
    QByteArray geometry, terrain, points;
};
class MapEdits {
  public:
    static MapEdits load(const Rom &);
    const FieldMapSource &source(int key) const;
    const FieldChanges &changes(int key) const;
    QVector<int> sharedScenarios(int key) const;
    QVector<QPointF> points(int key) const;
    bool canMoveSite(int key, int row) const;
    void moveSite(int key, int row, QPointF destination);
    void paint(int key, const QVector<QPointF> &stroke, int terrain, double radius);
    QRectF boundary(int key) const;
    void setBoundary(int key, QRectF boundary);
    void restore(int key);
    QByteArray model(int key) const;
    QByteArray fineTerrain(int key) const;
    QVector<FieldPlan> plan() const;
    QByteArray rebuild(const Rom &) const;
    QJsonObject json() const;
    void apply(const QJsonObject &);
    bool modified() const {
        return !edits_.isEmpty();
    }
    int changedMaps() const {
        return int(edits_.size());
    }

  private:
    Rom rom_;
    mutable QMap<int, FieldMapSource> sources_;
    mutable QMap<int, FieldBrush> textures_;
    QMap<int, FieldChanges> edits_;
    int modelId(int key) const;
    FieldBrush texture(int code) const;
    void prune(int model);
};
// Original packed model is retained; edits append native display lists and
// translate a town's verified culling group. No bitmap replaces the battlefield.
QByteArray editFieldModel(const FieldMapSource &, const FieldChanges &,
                          const QMap<int, FieldBrush> &);
QByteArray editFieldHeight(const FieldMapSource &, const FieldChanges &);
} // namespace lordly
