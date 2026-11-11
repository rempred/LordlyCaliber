#pragma once
#include <QByteArray>
namespace lordly {
// Authentication only: restore supported threshold operands before comparing
// the complete owner's baseline hash. Never install this canonical copy.
QByteArray canonicalAutomaticRequirementOwner(const QByteArray &bytes, qsizetype romStart);
}
