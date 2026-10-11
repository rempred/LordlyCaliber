#include "rebuild/shop_runtime.h"
#include "engine/shop_runtime.h"
#include <QFile>
#include <QJsonArray>

namespace lordly {
static QByteArray source(const QString &name) {
    QFile file(":/game/shops/" + name);
    if (!file.open(QIODevice::ReadOnly))
        throw Error("Missing maintained shop source");
    return file.readAll();
}
QByteArray rebuildShops(const Rom &rom, const Shops &shops, const Compiler &compiler,
                        const QString &directory) {
    const auto prior = ShopPlacement::read(rom);
    if (!prior) {
        verifyShopRuntime(rom, 0);
        if (rom.bytes().mid(ShopPlacement::descriptorOffset, ShopPlacement::descriptorSize) !=
            QByteArray(ShopPlacement::descriptorSize, char(-1)))
            throw Error("Shop descriptor allocation is occupied by another patch");
    }
    const auto offset = prior && shops.size() <= prior->count ? prior->offset : quint32((rom.bytes().size() + 15) & ~qsizetype(15));
    if (offset < 0x2800000 || quint64(offset) + (ShopPlacement::rowSize * shops.size()) > 0x4000000)
        throw Error("Shop data exceeds the cartridge address range");
    QByteArray rows((ShopPlacement::rowSize * shops.size()), 0);
    for (int id = 0; id < shops.size(); ++id) {
        auto at = id * ShopPlacement::rowSize;
        // Preserve the original consumer order: consumables, then equipment.
        for (auto item : shops.stock(id).consumables) {
            put16(rows, at, item);
            at += 2;
        }
        for (auto item : shops.stock(id).equipment) {
            put16(rows, at, item | 0x8000);
            at += 2;
        }
    }
    const auto data = compiler.compileData(rows, directory, "shop_rows");
    if (data != rows)
        throw Error("Compiled shop rows disagree with the authored inventory");
    const auto stock = compiler.compileModule(
        "#define LC_SHOP_COUNT " + QByteArray::number(shops.size()) + "\n" + source("stock.c"),
        directory, "shop_stock", ShopPlacement::stockOffset + ShopPlacement::slabDelta, {},
        {{"lc_shop_rows", offset}, {"lc_dma_read", 0x8009da50},
         {"resource_alloc", 0x80070f30}, {"memset_00023780", 0x80093380},
         {"boot_command_stream_dispatch", 0x8007938c},
         {"boot_command_stream_resource_node_dispatch", 0x80079618},
         {"D_8018F481", 0x8018f481}, {"D_801E7E15", 0x801e7e15},
         {"D_80219F20", 0x80219f20}});
    if (stock.entries.value("func_0019BE40") != stock.address)
        throw Error("Compiled shop entry moved from its original address");
    auto producer = stock.fitOwner(ShopPlacement::guardOffset - ShopPlacement::stockOffset);
    auto guard = compiler.assembleOwner(source("selection-guard.s"), directory, "shop_guard",
                                         ShopPlacement::guardOffset + ShopPlacement::slabDelta);
    const auto guardSpace = ShopPlacement::stockOffset + ShopPlacement::stockSize - ShopPlacement::guardOffset;
    if (guard.size() > guardSpace)
        throw Error("Shop selection guard exceeds its owner");
    guard.append(QByteArray(guardSpace - guard.size(), 0));
    producer += guard;
    QVector<CompiledAddressReference> rowReferences;
    for (const auto &reference : stock.addressReferences)
        if (reference.symbol == "lc_shop_rows")
            rowReferences << reference;
    if (rowReferences.size() != 1 || rowReferences[0].addend)
        throw Error("Compiled shop inventory reference is ambiguous");
    const auto consumers =
        compiler.assembleOwner(source("consumers.s"), directory, "shop_consumers", 0x80000000,
                               {{"lc_shop_selection_guard",
                                 ShopPlacement::producerOffset + ShopPlacement::slabDelta + 72}});
    const auto patches = shopRuntimeCatalog()["patches"].toArray();
    auto fit = [](QByteArray bytes, qsizetype length) {
        if (bytes.size() < length || bytes.mid(length) != QByteArray(bytes.size() - length, 0))
            throw Error("Assembled shop code exceeds its owner");
        return bytes.first(length);
    };
    fit(consumers, patches.size() * 4);
    auto image = rom.bytes();
    if (quint64(image.size()) < quint64(offset) + data.size())
        image.append(QByteArray(offset + data.size() - image.size(), char(-1)));
    image.replace(offset, data.size(), data);
    image.replace(ShopPlacement::stockOffset, producer.size(), producer);
    for (int i = 0; i < patches.size(); ++i) {
        const auto patch = patches[i].toObject();
        const auto word = u32(consumers, i * 4);
        if (word != rebuiltShopInstruction(patch))
            throw Error("Assembled shop reader disagrees with its operand contract");
        put32(image, patch["site"].toInt(), word);
    }
    QByteArray descriptor(ShopPlacement::descriptorSize, 0);
    descriptor.replace(0, 8, "LC05SHOP");
    put32(descriptor, 8, 4);
    put32(descriptor, 12, offset);
    put32(descriptor, 16, (ShopPlacement::rowSize * shops.size()));
    put32(descriptor, 20, shops.size());
    put32(descriptor, 24, ShopPlacement::rowSize);
    put32(descriptor, 28, crc32(data));
    put32(descriptor, 32, crc32(producer));
    put32(descriptor, 36, rowReferences[0].highOffset);
    put32(descriptor, 40, rowReferences[0].lowOffset);
    put32(descriptor, 60, crc32(descriptor.first(60)));
    image.replace(ShopPlacement::descriptorOffset, descriptor.size(), descriptor);
    if (!ShopPlacement::read(Rom::fromBytes(image)))
        throw Error("Shop rebuild readback failed");
    return image;
}
} // namespace lordly
