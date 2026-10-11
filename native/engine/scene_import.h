#pragma once
#include "engine/scene_document_compiler.h"
#include "engine/scene_events.h"
namespace lordly {
struct ImportedScenes {
    ScenePrograms programs;
    SceneEvents events;
};
ImportedScenes importBrowserScenes(const Rom &, ScenePrograms programs, SceneEvents events,
                                   const QJsonObject &);
DirectorProgram wipeSceneProgram(const DirectorProgram &);
} // namespace lordly
