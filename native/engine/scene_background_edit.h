#pragma once
#include "engine/scene_images.h"
namespace lordly {
struct BackgroundChoice {
    int selector = -1;
    QString name;
    QStringList images;
};
struct BackgroundEdit {
    enum Route { Unavailable, SceneGroup, Environment } route = Unavailable;
    int command = -1, selector = -1;
    QString reason;
    bool editable() const {
        return route != Unavailable;
    }
    static BackgroundEdit inspect(const Rom &, const DirectorProgram &);
    QVector<BackgroundChoice> choices(const Rom &) const;
    DirectorProgram select(const Rom &, const DirectorProgram &, int selector) const;
};
} // namespace lordly
