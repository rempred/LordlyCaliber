#pragma once
#include "engine/cursor_boundary.h"
#include "rebuild/compiler.h"
namespace lordly {
CursorBoundary buildCursorBoundary(const QSet<int> &, quint32 address, const Compiler &, const QString &);
}
