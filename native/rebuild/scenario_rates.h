#pragma once
#include "engine/scenario_rates.h"
#include "rebuild/compiler.h"
namespace lordly {
QByteArray rebuildScenarioRates(const Rom &rom, const ScenarioRates &rates,
                                const EncounterRate &global, const Compiler &compiler,
                                const QString &directory);
}
