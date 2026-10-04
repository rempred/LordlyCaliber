#pragma once
#include "engine/director.h"
#include <optional>
namespace lordly {
struct SceneProgramMember {
    quint32 offset = 0, size = 0;
    QVector<quint32> references;
};
struct AuthoredSceneBuild {
    QString id, name;
    int selector = -1;
    QByteArray program;
};
struct SceneProgramPlacement {
    quint32 offset = 0, allocation = 0;
    QMap<quint32, SceneProgramMember> members;
    QVector<AuthoredSceneBuild> authored;
};
class SceneProgramResources {
  public:
    static constexpr int descriptorOffset = 0x27c4700, descriptorSize = 96;
    static std::optional<SceneProgramPlacement> placement(const Rom &);
    static QByteArray rebuild(const Rom &, const QMap<quint32, QByteArray> &compiled,
                              const std::optional<QVector<AuthoredSceneBuild>> &authored = {});
};
} // namespace lordly
