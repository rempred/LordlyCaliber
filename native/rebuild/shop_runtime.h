#pragma once
#include "engine/shops.h"
#include "rebuild/compiler.h"
namespace lordly {
QByteArray rebuildShops(const Rom &rom, const Shops &shops, const Compiler &compiler,
                        const QString &directory);
}
