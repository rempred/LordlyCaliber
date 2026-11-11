#pragma once
#include "engine/animation_routes.h"
namespace lordly {
class BodyVisualPolicyImage {
  public:
    struct Block {
        int offset, size;
        const char *source;
    };
    static constexpr int descriptorOffset = 0x27c5100, descriptorSize = 96;
    static constexpr quint32 queryAddress = 0x801c45a0;
    static constexpr int settingsOffset = 0x207a6c, directoryOffset = 0x2060e8;
    static bool usesCDirectory(const Rom &);
    // The fragment lists are retained solely for recognizing older exports.
    static const std::array<Block, 5> &blocks();
    static const std::array<Block, 4> &constructionBlocks();
    static QByteArray constructionCode(const Rom &);
    static QByteArray code(const Rom &, bool construction = false);
    static QJsonObject catalog();
    static QJsonObject owner(const QString &id, const QString &variant = {});
    static QVector<QJsonObject> owners(bool construction);
    static QByteArray retailBytes(int offset, int size);
    static bool matches(const Rom &, const QJsonObject &);
    static bool legacyOwner(const Rom &, const QString &id, bool policies, bool construction);
    static QByteArray ownedCode(const Rom &, bool construction);
    static int count(const Rom &);
    static void validate(const Rom &, const AnimationRoutes &);
};
} // namespace lordly
