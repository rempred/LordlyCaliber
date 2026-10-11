#pragma once
#include <QString>
namespace lordly {
// Graceful close only: returns after the selected executable exits and flushes saves.
// Other Project64 installations and sessions that refuse to close are left intact.
void closeProject64(const QString &executable);
}
