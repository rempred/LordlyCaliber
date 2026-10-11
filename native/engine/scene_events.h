#pragma once
#include "engine/director_session.h"
#include "engine/scene_programs.h"
#include <QJsonObject>
namespace lordly {
struct SceneEventSite {
    QString id, name, role, mission, retailSceneId;
    int row = 0, offset = 0, selector = 0, entryCursor = 0;
    bool shortForm = false, shared = false;
};
struct SceneEventContext {
    QString id, site, parent;
    int invocation = -1, parentInvocation = -1, ticks = 1;
    DirectorLaunchTranslations translations;
};
class SceneEvents {
  public:
    static constexpr quint32 directory = 0x9398e8, base = 0x594280;
    static const QMap<QString, SceneEventSite> &sites();
    static const SceneEventSite &site(const QString &);
    static const QVector<SceneEventContext> &contexts();
    static quint32 retailKey(int row);
    static QByteArray retailRow(const Rom &, int row);
    static QByteArray readRow(const Rom &, int row);
    static QByteArray bindRow(const QByteArray &retail, int row, const QMap<QString, int> &bindings,
                              const QSet<QString> &forceStubs = {});
    static QMap<QString, int> discoverRow(const QByteArray &retail, const QByteArray &current,
                                          int row);
    static SceneEvents load(Rom);
    int target(const QString &) const;
    void bind(const QString &, int selector, const ScenePrograms &);
    void restore(const QString &);
    void validate(const ScenePrograms &) const;
    QJsonObject json() const;
    void apply(const QJsonObject &, const ScenePrograms &);
    SceneEventBuild plan() const;
    const QMap<QString, int> &bindings() const {
        return bindings_;
    }
    bool modified() const {
        return bindings_ != original_;
    }
    int changedSites() const;

  private:
    explicit SceneEvents(Rom rom) : rom_(std::move(rom)) {}
    Rom rom_;
    QMap<QString, int> bindings_, original_;
};
} // namespace lordly
