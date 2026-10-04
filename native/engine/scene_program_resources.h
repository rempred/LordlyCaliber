#pragma once
#include "engine/director.h"
#include <optional>
namespace lordly {
struct SceneProgramMember {
    quint32 offset = 0, size = 0;
    QVector<quint32> references;
};
struct SceneProgramPlacement {
    quint32 offset = 0, allocation = 0;
    QMap<quint32, SceneProgramMember> members;
};
class SceneProgramResources {
  public:
    static constexpr int descriptorOffset = 0x27c4700, descriptorSize = 96;
    static std::optional<SceneProgramPlacement> placement(const Rom &);
    static QByteArray rebuild(const Rom &, const QMap<quint32, QByteArray> &compiled);
};
} // namespace lordly
