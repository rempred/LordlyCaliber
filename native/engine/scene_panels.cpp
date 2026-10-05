#include "engine/scene_panels.h"
#include "engine/text.h"
#include "game/director/panel_animation.h"
#include <QPainter>
#include <algorithm>
namespace lordly {
namespace {
uint8_t *bytes(QByteArray &b) {
    return reinterpret_cast<uint8_t *>(b.data());
}
void checkSlot(int slot) {
    if (slot < 0 || slot >= 14)
        throw Error("Panel slot is outside 0..13");
}
QString label(const QByteArray &b) {
    QString out;
    for (int i = 0; i < b.size(); ++i) {
        const auto c = u8(b, i);
        if (c < 0x80)
            out += QChar(c);
        else if (c == 0x81 && i + 1 < b.size()) {
            const auto d = u8(b, ++i);
            out += d == 0xaa ? QChar(0x2191) : d == 0xab ? QChar(0x2193) : QChar(0xfffd);
        } else
            out += QChar(0xfffd);
    }
    return out;
}
} // namespace
QString ScenePanels::rewardName(const Rom &rom, quint16 item) {
    rom.requireSupported();
    if (rom.revision())
        throw Error("Reward names currently require US revision zero");
    const bool consumable = (item & 0x200) != 0;
    const int index = item & 0x1ff;
    if ((item & ~0x3ff) || index >= (consumable ? 45 : 278))
        throw Error("Reward item is outside the current native table");
    const quint32 pointer =
        u32(rom.bytes(), (consumable ? 0x645cc : 0x6230c) + index * (consumable ? 12 : 32));
    // Qualified resident item-name storage, not the early-boot ROM/RAM mapping.
    const quint32 lower = consumable ? 0x80190380U : 0x8018b4b0U;
    const quint32 upper = consumable ? 0x801906c4U : 0x8018c40cU;
    if ((pointer < lower || pointer >= upper) && (consumable || pointer != 0x8019037cU))
        throw Error("Reward name uses an unsupported relocated string pool");
    const qsizetype at = 0x613b0 + (pointer - 0x8018b4b0U);
    const auto end = rom.bytes().indexOf('\0', at);
    if (end < at || end - at > 96)
        throw Error("Reward name has no bounded terminator");
    return decodeText(rom.bytes().mid(at, end - at));
}
const QByteArray &ScenePanels::ownerData(int slot) const {
    checkSlot(slot);
    if (!owners_[slot])
        throw Error("Panel owner is absent");
    return owners_[slot]->data;
}
QByteArray ScenePanels::resource(quint32 key) const {
    if (!key || (key & 1) || key >= 0x10000000)
        throw Error("Invalid panel resource key");
    const quint32 at = key + 0x594280, size = u32(rom_.bytes(), at);
    if (size > 1024 * 1024 || quint64(at) + 4 + size > quint64(rom_.bytes().size()))
        throw Error("Panel resource exceeds its envelope");
    return rom_.bytes().mid(at + 4, size);
}
QByteArray ScenePanels::textPayload(quint32 selector, QByteArray &entity) const {
    const auto directory = resource(0x18855fe), compressed = resource(u32(directory, 0));
    const quint32 size = u32(compressed, 0);
    if (size > 65536)
        throw Error("Panel text exceeds its decode budget");
    const auto text = decodeLzss(compressed.mid(4), size);
    const auto separator = QByteArray::fromHex("0d0a0d0a");
    int begin = 0, end = -1;
    for (int section = 1; section <= int(selector & 255); ++section) {
        end = text.indexOf(separator, begin);
        if (end < 0)
            throw Error("Panel text section is missing");
        if (section != int(selector & 255))
            begin = end + 4;
    }
    if (end - begin > 0x4fe)
        throw Error("Panel text exceeds its native storage");
    QByteArray p(0x604, 0);
    p.replace(0, end - begin, text.mid(begin, end - begin));
    put8(p, 0x5f3, 0x17);
    put8(p, 0x5f6, 15);
    put8(p, 0x5fe, 2);
    put32(p, 0x600, selector);
    if ((selector >> 16) == 2) {
        put8(p, 0x5f0, 6);
        put8(p, 0x5f6, 16);
    }
    int lines = 1;
    for (int at = 0; at < 0x500 && lines < 48;) {
        const auto c = u8(p, at), d = u8(p, at + 1);
        const bool single = c < 0x81 || (c >= 0xa0 && c < 0xe0) || c >= 0xfd;
        if ((c == 0x40 && d == 'n') || (c == 0x81 && d == 0x97)) {
            put8(p, at, 0);
            at += 2;
            put32(p, 0x530 + lines * 4, at);
            ++lines;
        } else if ((c == 0x40 && d == 0x5c) || (c == 0x81 && d == 0x8f)) {
            put8(p, at, 0);
            put8(p, 0x5f7, 1);
            put8(entity, 0x22, 0);
            at += single ? 2 : 4;
        } else
            at += single ? 1 : 2;
    }
    put8(entity, 0x25, lines);
    put8(p, 0x5f4, lines);
    put8(p, 0x5f5, lines);
    return p;
}
void ScenePanels::create(int slot, int preset, int width, int height) {
    checkSlot(slot);
    if (preset != 33 && preset != 3 && preset != 1 && preset != 34)
        throw Error("This panel preset is not implemented");
    if (preset == 34 && !reward_)
        throw Error("Reward panel requires the parent event's item selection");
    if (width < 1 || height < 1 || width > 2048 || height > 2048)
        throw Error("Invalid panel image dimensions");
    rom_.requireSupported();
    if (rom_.revision())
        throw Error("Map panels currently require US revision zero");
    if (preset == 34) {
        Entity e;
        e.identity = nextIdentity_;
        e.record = QByteArray(232, 0);
        const int32_t initial[4]{100, 155, 220, 155}, target[4]{100, 130, 220, 155};
        lordly_panel_initialize(bytes(e.record), 0x100, 0x802364cc, initial, target);
        e.reward = reward_;
        e.payload = QByteArray(0x604, 0);
        put8(e.payload, 0x5f3, 23);
        put8(e.payload, 0x5fe, 2);
        put32(e.payload, 0x600, 0x100);
        for (int at : {0x5f4, 0x5f5, 0x5f6})
            put8(e.payload, at, 1);
        put8(e.record, 0x25, 1);
        QByteArray data(56, 0);
        put16(data, 0, *reward_);
        put16(data, 2, 0xffff);
        rewardName(rom_, *reward_);
        release(slot);
        owners_[slot] = Owner{0x100, e.identity, 0, std::move(data)};
        entities_.push_back(std::move(e));
        ++nextIdentity_;
        return;
    }
    Entity e;
    e.identity = nextIdentity_;
    e.record = QByteArray(232, 0);
    const int32_t initial[4] = {preset == 33 ? 160 : 100, preset == 33 ? 120 : 100,
                                preset == 33 ? 160 : 110, preset == 33 ? 120 : 110};
    const bool small = width < 281 && height < 191;
    const int32_t target[4] = {preset == 33 ? 39 : 70, 80, preset == 33 ? 281 : 250,
                               preset == 33 ? (small ? 118 : 135) : 120};
    lordly_panel_initialize(bytes(e.record), preset == 3 ? 0x1000 : 0x100,
                            preset == 3 ? 0x8017ec84 : 0x8017c584, initial, target);
    const quint32 selector = preset == 33  ? (small ? 0x113 : 0x112)
                             : preset == 3 ? 0x21003
                                           : 0x10101;
    e.payload = textPayload(selector, e.record);
    if (preset == 33)
        put32(e.payload, 0x600, 0x121);
    if (preset == 3)
        put8(e.record, 0x24, 0xfe);
    // Validate/decode before replacing a live owner.
    if (preset == 3 && owners_[slot]) {
        const auto old = owners_[slot]->entity;
        entities_.removeIf([old](const Entity &e) { return e.identity == old; });
    }
    release(slot);
    owners_[slot] = Owner{u32(e.payload, 0x600), e.identity};
    entities_.push_back(std::move(e));
    ++nextIdentity_;
}
void ScenePanels::release(int slot) {
    if (slot == -1) {
        owners_ = {};
        return;
    }
    checkSlot(slot);
    if (!owners_[slot])
        return;
    // Native 283748 releases only the script owner/resources. The shared entity
    // list has an independent lifetime, including panels still on screen.
    owners_[slot].reset();
}
int ScenePanels::query(int slot) const {
    checkSlot(slot);
    if (!owners_[slot])
        return -5;
    const auto &owner = *owners_[slot];
    for (const auto &e : entities_)
        if (e.identity == owner.entity)
            return owner.status;
    return -6;
}
void ScenePanels::child(Entity &parent) {
    Entity e;
    e.identity = nextIdentity_++;
    e.parent = parent.identity;
    e.record = QByteArray(232, 0);
    const int right = qint32(u32(parent.record, 0x44)), bottom = qint32(u32(parent.record, 0x48));
    const int32_t initial[4] = {right - 12, bottom - 8, right - 4, bottom + 8},
                  target[4] = {right - 62, bottom - 16, right - 4, bottom + 16};
    lordly_panel_initialize(bytes(e.record), 4, 0x8017eae8, initial, target);
    put8(e.record, 0x22, u8(parent.record, 0x22));
    put8(e.record, 0x23, u8(parent.record, 0x22));
    put8(e.record, 0x24, 1);
    put8(e.record, 0x25, 2);
    entities_.push_back(std::move(e));
}
void ScenePanels::advance(quint16 action, quint16 direction) {
    audio_.clear();
    if (entities_.isEmpty())
        return;
    int parentIndex = entities_.size() - 1;
    if (entities_.last().parent)
        for (int i = entities_.size() - 1; i >= 0; --i)
            if (u16(entities_[i].record, 0x16) & 0x100) {
                parentIndex = i;
                break;
            }
    auto &parent = entities_[parentIndex];
    if (parent.payload.isEmpty())
        return;
    auto owner = std::find_if(owners_.begin(), owners_.end(), [&](const auto &o) {
        return o && o->selector == u32(parent.payload, 0x600);
    });
    if (owner == owners_.end())
        return;
    auto &status = (*owner)->status;
    status = -1;
    const int state = u8(parent.payload, 0x5f0);
    if (parent.reward) {
        if (!state && !u16(parent.record, 0x28) && (action & 0xc000)) {
            audio_ << 0x19;
            put8(parent.payload, 0x5f0, 3);
            status = -2;
        } else if (state == 3) {
            lordly_panel_close(bytes(parent.record));
            put8(parent.payload, 0x5f0, 4);
        }
    } else if ((u32(parent.payload, 0x600) & 255) == 33) {
        if (!state && u16(parent.record, 0x18) == 255 && (action & 0xff3f)) {
            audio_ << 3;
            put8(parent.payload, 0x5f0, 3);
            status = 0;
        } else if (state == 3) {
            lordly_panel_close(bytes(parent.record));
            put8(parent.payload, 0x5f0, 4);
        }
    } else if (!state && u8(parent.payload, 0x5f7))
        put8(parent.payload, 0x5f0, 5);
    else if (state == 5) {
        put8(parent.payload, 0x5f0, 6);
        child(parent);
    } else if (state == 6 && u16(parent.record, 0x18) == 255 && !u16(parent.record, 0x28)) {
        auto &select = entities_.last().parent ? entities_.last() : parent;
        const int before = qint8(u8(select.record, 0x22));
        if ((action & 0x8000) || ((action & 0x4000) && qint8(u8(select.record, 0x24)) != -1)) {
            if (action & 0x8000) {
                status = before;
                audio_ << (select.parent && before ? 1 : 6);
            } else {
                status = qint8(u8(select.record, 0x24));
                put8(select.record, 0x22, quint8(status));
                audio_ << 1;
            }
            lordly_panel_close(bytes(select.record));
            if (&select != &parent)
                lordly_panel_close(bytes(parent.record));
            put8(parent.payload, 0x5f0, 3);
        } else {
            int selected = before;
            if (direction & 0x800)
                selected = qMax(0, before - 1);
            else if (direction & 0x400)
                selected = qMin(int(qint8(u8(select.record, 0x25))) - 1, before + 1);
            put8(select.record, 0x22, quint8(selected));
            if (before != selected)
                audio_ << 2;
        }
    }
    for (auto it = entities_.begin(); it != entities_.end();)
        if (lordly_panel_animate(bytes(it->record)))
            it = entities_.erase(it);
        else
            ++it;
}
QImage ScenePanels::render() const {
    QImage result(320, 240, QImage::Format_RGBA8888);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    QFont font("Consolas");
    font.setPixelSize(11);
    painter.setFont(font);
    for (const auto &e : entities_) {
        const auto &r = e.record;
        const int alpha = u16(r, 0x18);
        if (!alpha)
            continue;
        const int left = qint32(u32(r, 0x2c)), top = qint32(u32(r, 0x30)),
                  right = qint32(u32(r, 0x34)), bottom = qint32(u32(r, 0x38));
        painter.save();
        painter.setOpacity(alpha / 255.);
        painter.setPen(QColor(213, 220, 244));
        painter.setBrush(QColor(27, 35, 62));
        painter.drawRect(QRect(left, top, right - left, bottom - top));
        painter.setClipRect(QRect(left + 1, top + 1, right - left - 2, bottom - top - 2));
        painter.setPen(Qt::white);
        const bool option = u32(r, 8) == 0x8017ec84;
        auto drawLine = [&](const QByteArray &text, int x, int y) {
            painter.drawText(QPoint(x, y + 10), label(text));
        };
        if (e.reward) {
            const auto icon = icons_.image(*e.reward);
            painter.drawImage(QPoint(left + 4, top + 4),
                              QImage(reinterpret_cast<const uchar *>(icon.rgba.constData()),
                                     16, 16, QImage::Format_RGBA8888));
            // Native content callback2A54A8 places the text22 pixels after the icon.
            painter.drawText(QRect(left + 26, top + 4, right - left - 30, 16),
                             Qt::AlignLeft | Qt::AlignVCenter, rewardName(rom_, *e.reward));
        } else if (e.parent) {
            for (int i = 0; i < 2; ++i) {
                const int at = 0x66710 + (i ? 4 : 0);
                int end = at;
                while (end < at + 16 && u8(rom_.bytes(), end))
                    ++end;
                drawLine(rom_.bytes().mid(at, end - at), left + 21, top + (i ? 14 : 2));
            }
        } else
            for (int i = 0; i < u8(e.payload, 0x5f4); ++i) {
                const int index = u8(e.payload, 0x5f1) + i;
                if (index >= u8(e.payload, 0x5f5))
                    continue;
                if (index >= 48)
                    throw Error("Panel line index exceeds its pointer table");
                const int at = u32(e.payload, 0x530 + index * 4);
                if (at < 0 || at >= 0x500)
                    throw Error("Panel line exceeds its text storage");
                int end = at;
                while (end < 0x500 && u8(e.payload, end))
                    ++end;
                if (end == 0x500)
                    throw Error("Panel line has no terminator");
                drawLine(e.payload.mid(at, end - at),
                         left + (option ? 24 : qint16(u16(e.payload, 0x5f8)) + 6),
                         top + i * u8(e.payload, 0x5f6) +
                             (option ? 4 : qint16(u16(e.payload, 0x5fa)) + 5));
            }
        if (e.parent || option) {
            const int selected = qint8(u8(r, 0x22)), x = left + (e.parent ? 1 : 3),
                      y = top + (e.parent ? (selected & 1) * 14 + 4
                                          : selected * u8(e.payload, 0x5f6) + 6);
            painter.setPen(Qt::NoPen);
            painter.setBrush(Qt::white);
            painter.drawPolygon(QPolygon{{x, y}, {x + 7, y + 4}, {x, y + 8}});
        }
        painter.restore();
    }
    return result;
}
} // namespace lordly
