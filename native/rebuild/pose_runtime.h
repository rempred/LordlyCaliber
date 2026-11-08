#pragma once
#include "rebuild/compiler.h"
namespace lordly {
QByteArray rebuildPoseRuntime(const Rom &target, const Rom &source, const QByteArray &mappings,
                              bool wide, bool bodyPolicies, const Compiler &,
                              const QString &directory);
quint32 poseHeightRuntimeAddress(const Rom &, const std::optional<ClassPlacement> &);
QByteArray buildPoseHeightRuntime(quint32 address, const Compiler &, const QString &directory);
void installPoseHeightRuntime(QByteArray &image, const Rom &source, quint32 address,
                              const Compiler &, const QString &directory);
inline constexpr quint32 poseHeightHeaderSize = 16;
}
