#include "engine/scene_chapter_card.h"
#include "engine/image.h"
#include <QPainter>
#include <cstring>
#include <mutex>

// These accepted routines contain normal field and byte accesses. Host pointers
// replace target addresses. Image dimensions are converted at the call boundary.
namespace chapter_retail {
#include "game/chapter_card.h"
ChapterCard *D_8023DE20;
u16 D_8018F1A0;
static quint32 randomState;
static std::mutex mutex;
void bzero(void *p, s32 n) {
    std::memset(p, 0, n);
}
void func_80080998(void *d, void *s, s32 n) {
    std::memcpy(d, s, n);
}
// The director can seek backwards. Use a repeatable preview sequence for the
// sparkles, whose actual random seed belongs to the game's launch.
s32 func_0002CBCC() {
    randomState = 1664525U * randomState + 1013904223U;
    return int(randomState >> 16);
}
#include "game/accepted/library/src/lib/func_00289D24.c"
#include "game/accepted/library/src/lib/func_00289E88.c"
#include "game/accepted/library/src/lib/func_00289ECC.c"
#include "game/accepted/library/src/lib/func_0028ac88.c"
} // namespace chapter_retail
namespace lordly {
namespace {
QByteArray resource(const Rom &rom, quint32 key) {
    const quint64 at = 0x594280ULL + key;
    if (!key || at + 4 > quint64(rom.bytes().size()))
        throw Error("Title image resource lies outside the ROM");
    const auto size = u32(rom.bytes(), at);
    if (size > 0x200000 || at + 4 + size > quint64(rom.bytes().size()))
        throw Error("Title image resource exceeds its allocation");
    return rom.bytes().mid(at + 4, size);
}
QImage nativeImage(const QByteArray &bytes) {
    const auto raster = decodeN64Image(bytes);
    QImage image(reinterpret_cast<const uchar *>(raster.rgba.constData()), raster.width,
                 raster.height, raster.width * 4, QImage::Format_RGBA8888);
    return image.copy();
}
QImage coverageImage(const QByteArray &b, QColor ink) {
    const int w = u16(b, 4), h = u16(b, 6), stride = chapter_retail::func_0028ac88(u8(b, 3), w);
    if (u8(b, 3) != 1 || b.size() < 8 + stride * h)
        throw Error("Title lettering requires an eight-bit coverage texture");
    QImage result(w, h, QImage::Format_RGBA8888);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            ink.setAlpha(u8(b, 8 + y * stride + x));
            result.setPixelColor(x, y, ink);
        }
    return result;
}
} // namespace
SceneChapterCard::SceneChapterCard(const Rom &rom, int selector, int scenario, int sequenceBefore)
    : selector_(selector) {
    const auto directory = resource(rom, 0x1890c90);
    auto load = [&](int child) {
        if (child < 0 || child >= directory.size() / 4)
            throw Error("Mission title has no image for the selected scenario");
        const auto packed = resource(rom, u32(directory, child * 4));
        const auto size = u32(packed, 0);
        if (size < 8 || size > 0x200000)
            throw Error("Title image exceeds the preview budget");
        auto result = decodeLzss(packed.mid(4), size);
        if (result.first(2) != "64")
            throw Error("Invalid title image header");
        const int w = u16(result, 4), h = u16(result, 6);
        const auto stride = chapter_retail::func_0028ac88(u8(result, 3), w);
        if (w < 1 || h < 1 || w > 640 || h > 480 || stride * h + 8 > result.size())
            throw Error("Invalid title texture dimensions");
        return result;
    };
    const int variant = selector - 1073;
    if (variant == 5) {
        const int sequence = quint8(sequenceBefore + 1);
        if (sequence >= 100 || scenario < 0)
            throw Error("Mission title requires a scenario and a two-digit mission number");
        mission_[0] = coverageImage(load(32 + scenario), Qt::white);
        mission_[1] = coverageImage(load(30), Qt::white);
        load(31); // The retail owner also loads this image and releases it with the title.
        if (sequence >= 10)
            mission_[3] = coverageImage(load(20 + sequence / 10), Qt::white);
        mission_[4] = coverageImage(load(20 + sequence % 10), Qt::white);
        companionBase_ = nativeImage(load(19));
        companionPattern_ = nativeImage(load(18));
        companionOverlay_ = coverageImage(load(17), Qt::black);
        if (companionPattern_.size() != QSize(160, 120))
            throw Error("Mission companion requires a 160 by 120 scrolling texture");
        return;
    }
    if (variant < 0 || variant > 4)
        throw Error("Invalid chapter card selector");
    count_ = variant == 4 ? 4 : 3;
    source_[0] = load(0);
    for (int i = 1; i <= count_; ++i) {
        source_[i] = load(variant * 3 + i);
        // The original reveal loop samples the edge pixel before its destination check.
        source_[i].append(8, char(0));
        pixels_[i] = source_[i];
    }
    x_[1] = 160 - u16(source_[1], 4) / 2;
    y_[1] = 120 - u16(source_[1], 6) / 2 - 16 * count_;
    for (int i = 2; i <= count_; ++i) {
        if (u8(source_[i], 3) != 1 || u8(source_[0], 3) != 1 ||
            u16(source_[i], 6) > u16(source_[0], 6))
            throw Error("Chapter reveal needs compatible byte textures and mask rows");
    }
}
void SceneChapterCard::startFade() {
    ticks_[1] = 85;
    alpha_[1] = 0;
    step_[1] = 3;
}
void SceneChapterCard::startReveal() {
    if (selector_ == 1078)
        throw Error("Mission entry has no chapter reveal buffers");
    run(true);
}
bool SceneChapterCard::revealing() const {
    const std::lock_guard lock(chapter_retail::mutex);
    chapter_retail::ChapterCard card{};
    std::copy(ticks_.begin(), ticks_.end(), card.revealTicks);
    chapter_retail::D_8023DE20 = &card;
    const bool active = chapter_retail::func_00289E88() != 0;
    chapter_retail::D_8023DE20 = nullptr;
    return active;
}
void SceneChapterCard::run(bool start) {
    using chapter_retail::ChapterCard;
    using chapter_retail::TitleImageHeader;
    const std::lock_guard lock(chapter_retail::mutex);
    ChapterCard card{};
    // The accepted target accesses bytes immediately before the first pixel.
    // Preserve its complete 8-byte header and any palette in both buffers.
    auto hostImage = [](QByteArray &bytes) -> TitleImageHeader * {
        if (bytes.isEmpty())
            return nullptr;
        const int w = u16(bytes, 4), h = u16(bytes, 6);
        auto p = reinterpret_cast<TitleImageHeader *>(bytes.data());
        p->width = w;
        p->height = h;
        return p;
    };
    auto sources = source_;
    for (int i = 0; i < 5; ++i) {
        card.images[i] = hostImage(sources[i]);
        card.secondary[i] = hostImage(pixels_[i]);
        card.x[i] = x_[i];
        card.y[i] = y_[i];
        card.revealTicks[i] = ticks_[i];
    }
    card.secondaryCount = count_;
    card.revealSlot = revealSlot_;
    chapter_retail::D_8023DE20 = &card;
    chapter_retail::D_8018F1A0 = selector_;
    chapter_retail::randomState = random_;
    if (start)
        chapter_retail::func_00289D24();
    else
        chapter_retail::func_00289ECC();
    random_ = chapter_retail::randomState;
    revealSlot_ = card.revealSlot;
    chapter_retail::D_8023DE20 = nullptr;
    for (int i = 0; i < 5; ++i) {
        x_[i] = card.x[i];
        y_[i] = card.y[i];
        ticks_[i] = card.revealTicks[i];
        if (!pixels_[i].isEmpty()) {
            put16(pixels_[i], 4, u16(source_[i], 4));
            put16(pixels_[i], 6, u16(source_[i], 6));
        }
    }
}
void SceneChapterCard::advance() {
    if (ticks_[1]) {
        --ticks_[1];
        alpha_[1] += step_[1];
    }
    if (revealing())
        run(false);
    if (selector_ == 1078) {
        scrollA_ = (scrollA_ + 322) % 19200;
        scrollB_ = (scrollB_ + 318) % 19200;
    }
}
QImage SceneChapterCard::render() const {
    QImage frame(320, 240, QImage::Format_RGBA8888);
    frame.fill(Qt::transparent);
    QPainter p(&frame);
    p.setClipRect(0, 0, 319, 239);
    if (selector_ != 1078) {
        p.setOpacity(quint8(alpha_[1]) / 255.);
        for (int i = 1; i <= count_; ++i) {
            if (!x_[i] && !y_[i])
                continue;
            p.drawImage(x_[i], y_[i], coverageImage(pixels_[i], Qt::white));
        }
        return frame;
    }
    p.drawImage(0, 0, companionBase_);
    p.setOpacity(96. / 255);
    for (int offset : {scrollA_, scrollB_}) {
        QImage tile(160, 120, QImage::Format_RGBA8888);
        for (int y = 0; y < 120; ++y) {
            const int first = (offset + 160 * y) % 19200, count = std::min(160, 19200 - first);
            std::memcpy(tile.scanLine(y), companionPattern_.constBits() + 4 * first, 4 * count);
            if (count < 160)
                std::memcpy(tile.scanLine(y) + 4 * count, companionPattern_.constBits(),
                            4 * (160 - count));
        }
        for (int y : {0, 120})
            for (int x : {0, 160})
                p.drawImage(x, y, tile);
    }
    p.setOpacity(1);
    // The final intensity layer uses its texture as coverage.
    p.drawImage(0, 0, companionOverlay_);
    int x = (320 - mission_[1].width() - mission_[3].width() - mission_[4].width()) / 2;
    p.drawImage(x, 60, mission_[1]);
    x += mission_[1].width();
    if (!mission_[3].isNull()) {
        p.drawImage(x, 61, mission_[3]);
        x += mission_[3].width();
    }
    p.drawImage(x, 61, mission_[4]);
    p.drawImage((320 - mission_[0].width()) / 2, 110, mission_[0]);
    return frame;
}
} // namespace lordly
