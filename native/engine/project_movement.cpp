#include "engine/project.h"
namespace lordly {
void Project::setMovementTypes(const QJsonObject &j) {
    auto next = movementTypes_;
    next.apply(j);
    movementTypes_ = std::move(next);
    catalog_["MOVEMENT_TYPES"] = movementTypes_.labels();
}
int Project::createMovementType(const QString &name, int copyFrom) {
    auto next = movementTypes_;
    int id = next.create(name, copyFrom);
    setMovementTypes(next.json());
    return id;
}
void Project::deleteMovementType(int id) {
    for (const auto &r : records(Table::Classes))
        if (u8(r.bytes, 44) == id)
            throw Error("This movement type is assigned to " + r.name +
                        ". Change that class's movement type first.");
    auto next = movementTypes_;
    next.remove(id);
    setMovementTypes(next.json());
}
} // namespace lordly
