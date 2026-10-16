#pragma once
#include "engine/game_rules.h"

namespace lordly {
QVector<GameRule> statusRuleDefinitions();
void loadStatusRules(const Rom &, QMap<QString,int> &values, QMap<QString,QString> &unavailable,
                     QMap<QString,QByteArray> &owners);
QVector<RuleBuild> buildStatusRules(const GameRules &, const QMap<QString,QByteArray> &owners);
// Normalize only recognized wake-check edits, retaining all other owner bytes.
QByteArray canonicalWeaponRecovery(const QByteArray &, int *chance = nullptr);
QByteArray withWeaponRecovery(const QByteArray &, int chance);
QByteArray withStatusRecovery(const QByteArray &, const QString &owner, int chance);
}
