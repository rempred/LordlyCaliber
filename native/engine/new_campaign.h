#pragma once
#include "engine/project.h"
#include "engine/save_document.h"
namespace lordly {
// Campaign progress is never imported with an army. These spans are the native
// roster, formations, legion/order lists and inventory, in canonical RAM order.
class NewCampaign {
  public:
    static constexpr int armyCapacity = 16;
    static QVector<QPair<quint32, int>> armySpans();
    static QJsonObject settings(const SceneEvents &);
    static QByteArray armyBytes(const QJsonObject &, int id);
    static QByteArray armyMemory(const Project &, const QByteArray &);
    static QJsonObject army(const Project &, int id, const QString &, const QByteArray &);
    static void validate(const QJsonObject &);
    static QJsonObject defaults(const Project &, int location);
    static void initialize(Project &, const QJsonObject &);
    static void configure(Project &, const QJsonObject &);
    static void applyLayoutOptions(Project &);
    static bool includesBattlefield(const SceneEvents &, int key);
    static QJsonArray opening(const QJsonObject &);
};
}
