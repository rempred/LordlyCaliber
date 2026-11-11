#pragma once
#include "engine/map_edits.h"
#include "engine/scenarios.h"
#include "engine/movement_types.h"
namespace lordly {
struct RoutePreview {
    QVector<QPointF> points; // Map grid coordinates, 0..256.
    QVector<int> cells;
    QString message, detail;
    int movementType = 7, leaderRow = -1;
    bool reachable = false;
    static RoutePreview squad(const Rom &, const Scenarios &, const ClassData &,
                              const MapEdits &, int key, int row, const MovementTypes *movement = nullptr);
};
}
