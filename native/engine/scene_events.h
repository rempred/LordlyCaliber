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
    static constexpr int retailRowCount = 115, firstCustomRow = retailRowCount,
                         customRowCapacity = 64, rowCapacity = firstCustomRow + customRowCapacity;
    static constexpr bool customMissionRow(int row) {
        return row == 2 || row == 3 || (row >= firstCustomRow && row < rowCapacity);
    }
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
    QByteArray program(int row,bool structural=true) const;
    QByteArray authoredBase(int row) const;
    QVector<int> scenarioRows(int key) const;
    void editFlag(int row, int offset, int flag, bool value);
    void editBranch(int row, int offset, int target);
    void editParameter(int row, int offset, const QString &key, int value, const ScenePrograms &);
    void setWorldRules(int node, const QJsonArray &rules);
    int createMission(int location,int key,const QString &name,int openingScene,int victoryScene,
                      const ScenePrograms &scenes,int prerequisiteFlag=-1);
    QString missionName(int row) const;
    void setRetailGate(int row,int comparison,const QJsonObject &,const ScenePrograms &);
    void setTrees(int row, const QJsonArray &, const ScenePrograms &);
    void setTrigger(int row, int entry, const QJsonObject &, const ScenePrograms &);
    static constexpr int firstCustomFlag = 256, customFlagCapacity = 512;
    QJsonArray customFlags() const;
    bool hasCustomFlags() const;
    int createFlag(const QString &name);
    void renameFlag(int id, const QString &name);
    void removeFlag(int id);
    QString flagName(int id, int row = -1, int offset = -1) const;
    QMap<int,QString> flagChoices(int row = -1) const;
    void unlockNodeAfter(int row, int node, bool enabled);
    void restoreFlags(int row);
    const QJsonObject &flagEdits() const { return flags_; }
    static QByteArray flagRow(const QByteArray &, int row, const QJsonObject &,bool structural=true);
    static QSet<int> flagRows(const QJsonObject &);
    static bool hasCampaignEdits(const QJsonObject &);
    void validate(const ScenePrograms &) const;
    QJsonObject json() const;
    void apply(const QJsonObject &, const ScenePrograms &);
    SceneEventBuild plan() const;
    const QMap<QString, int> &bindings() const {
        return bindings_;
    }
    bool modified() const {
        return bindings_ != original_ || flags_ != originalFlags_;
    }
    int changedSites() const;

  private:
    explicit SceneEvents(Rom rom) : rom_(std::move(rom)) {}
    Rom rom_;
    QMap<QString, int> bindings_, original_;
    QJsonObject flags_, originalFlags_;
};
} // namespace lordly
