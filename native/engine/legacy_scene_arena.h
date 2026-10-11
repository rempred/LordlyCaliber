#pragma once
#include "engine/scene_program_resources.h"
#include <QJsonObject>
namespace lordly {
// Read-only ownership of the browser editor's arena. Migration never clears it:
// retail resources and unknown manifest metadata may still depend on these bytes.
struct LegacySceneArena {
    static constexpr quint32 start = 0x2780000, descriptor = 0x27af000, end = 0x27b0000;
    QVector<AuthoredSceneBuild> authored;
    QMap<QString, int> eventBindings;
    QJsonObject manifest;
    static std::optional<LegacySceneArena> read(const Rom &);
};
} // namespace lordly
