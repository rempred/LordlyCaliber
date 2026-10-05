#pragma once
#include "engine/animation_descriptor.h"
#include "engine/animation_routes.h"
#include "engine/direct_descriptors.h"
namespace lordly {
// One private descriptor per class/body route. The source ROM remains immutable.
class AnimationSequences {
  public:
    struct Group {
        quint16 baselineHandle = 0, privateHandle = 0;
        AnimationDescriptor descriptor;
    };
    struct Plan {
        quint32 offset = 0;
        QByteArray payload;
        DirectDescriptors directory;
        QMap<int, quint16> handles;
    };
    static constexpr int descriptorOffset = 0x27c4e00, descriptorSize = 96;
    static AnimationSequences load(const Rom &);
    static QSet<quint32> ownedDescriptors(const Rom &);
    const QMap<int, Group> &groups() const {
        return groups_;
    }
    const AnimationDescriptor *find(int classId, int flags) const;
    void create(int classId, int flags, const AnimationRoutes &, const ActorArtworkEdits & = {});
    void setDescriptor(int classId, int flags, const AnimationDescriptor &);
    void remove(int classId, int flags);
    void cloneClass(int source, int target);
    AnimationRoutes baseRoutes(AnimationRoutes) const;
    void validate(const AnimationRoutes &, int classCount) const;
    bool requiresMarkerRuntime() const;
    bool modified() const {
        return json() != original_;
    }
    QJsonObject json() const;
    void apply(const QJsonObject &);
    Plan plan(const Rom &) const;
    QByteArray materialize(const Rom &, const Plan &, const QByteArray &compiled) const;
    AnimationRoutes exportRoutes(AnimationRoutes, const Plan &) const;

  private:
    Rom rom_;
    QByteArray header_;
    quint32 offset_ = 0, size_ = 0;
    QMap<int, Group> groups_, loaded_;
    QJsonObject original_;
};
} // namespace lordly
