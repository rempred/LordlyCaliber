#pragma once
#include "engine/director_session.h"
namespace lordly {
class SaveDocument;
struct StoryCastMember {
    quint8 primary, secondary;
};
// Explicit preview inputs for the five-person event66/67 story-cast contract.
// This does not infer historical army membership or change authored operands.
class SceneStoryCast {
  public:
    static bool supports(const DirectorProgram &);
    static QVector<int> choices();
    static std::array<StoryCastMember, 5> sample();
    static std::array<StoryCastMember, 5> saved(const SaveDocument &, int classCount);
    static DirectorLaunchTranslations translations(const std::array<StoryCastMember, 5> &);
};
} // namespace lordly
