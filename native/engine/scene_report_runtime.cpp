#include "engine/scene_report_runtime.h"
#include "game/director/panel_animation.h"
#include <cstring>
namespace lordly {
namespace {
constexpr quint32 shared = 0x7ffa4030, report = 0x7ffa40c0, resident = 0x8012a100;
}
SceneReportRuntime::SceneReportRuntime(const Rom &rom, const QByteArray &ram,
                                       const QByteArray &script)
    : rom_(rom) {
    if ((ram.size() != 0x400000 && ram.size() != 0x800000) || script.isEmpty())
        throw Error("Hugo's Report requires a runtime capture");
    // A captured Expansion Pak can already use the upper four MiB. Place
    // the guest allocator and stack beyond all captured addresses.
    if (ram.size() == 0x800000)
        machine_.memory.append(QByteArray(0x400000, 0));
    next_ = 0x80010000u + quint32(ram.size());
    arenaEnd_ = next_ + 0x3a0000u;
    machine_.mount(0x80000400, ram.mid(0x400));
    const auto stream = machine_.word(0x8022a958);
    if (read(stream, script.size()) != script)
        throw Error("The captured scene is not the selected Hugo's Report script");
    for (const auto &[bias, offset] :
         {std::pair{shared, 0x283748}, std::pair{report, 0x297cc8}, std::pair{resident, 0x51eb0}})
        if (read(bias + offset, 24) != rom.bytes().mid(offset, 24))
            throw Error("The captured Hugo's Report uses different overlay code");
    auto code = [&](quint32 bias, int a, int b) {
        machine_.mount(bias + a, rom.bytes().mid(a, b - a), true);
    };
    // These independently relocated overlays coexist during the class-six report.
    code(shared, 0x283648, 0x286600);
    code(report, 0x28a7b0, 0x299d44);
    code(resident, 0x39810, 0x60000);
    // Resident libc text used by the report's variable-length text, formatting
    // and page buffers. Keep data pointers and the formatting tables from RAM.
    code(0x8006fc00, 0x23820, 0x2de50);
    code(0x8006fc00, 0x2def4,
         0x2e138); // resource size and raw loader, including caller-owned buffers
    code(0x8006fc00, 0xa510, 0xaf7c); // boot LZSS loader and decoded-size reader
    machine_.registers[29] = 0x80000000u + quint32(machine_.memory.size()) - 0x1000;
    const auto scene = machine_.word(0x8022a974), head = machine_.word(0x8018fdc0);
    machine_.physical(scene, 0x1cb8);
    machine_.physical(head, 0xe0);
    const auto parameters = machine_.word(0x8023de34);
    machine_.physical(parameters, 0x68);
    // Restart the original script with its captured report records, history and
    // character data. Its panels are constructed in game order from an empty list.
    for (int i = 0; i < 14; ++i)
        machine_.word(scene + 0x19b8 + 4 * i, 0);
    // 0028ADF4 initializes the page cursor, selection and text scratch
    // before the first command. Retain the six loaded artwork pointers.
    machine_.mount(parameters + 0x18, QByteArray(0x68 - 0x18, 0));
    machine_.mount(0x8023e5d0, QByteArray(0x2f, 0));
    machine_.word(head + 4, 0);
    machine_.half(head, 0);
    machine_.byte(0x8022a99a, 3);
}
QByteArray SceneReportRuntime::read(quint32 a, int size) const {
    if (size < 0 || size > 0x200000)
        throw Error("Report buffer exceeds its supported size");
    return machine_.memory.mid(machine_.physical(a, size), size);
}
QByteArray SceneReportRuntime::string(quint32 a, int limit) const {
    const int at = machine_.physical(a);
    const auto *start = machine_.memory.constData() + at;
    const auto size = std::min<qsizetype>(limit + 1, machine_.memory.size() - at);
    const auto *end = static_cast<const char *>(std::memchr(start, 0, size));
    if (!end)
        throw Error("Report text lacks a bounded terminator");
    return QByteArray(start, end - start);
}
quint32 SceneReportRuntime::allocate(int size) {
    if (size <= 0 || size > 0x200000)
        throw Error("Report allocation exceeds the preview arena");
    const int aligned = (size + 15) & ~15;
    for (auto it = free_.begin(); it != free_.end(); ++it)
        if (it.value() >= aligned) {
            const auto addr = it.key();
            blocks_[addr] = it.value();
            free_.erase(it);
            return addr;
        }
    if (next_ + aligned > arenaEnd_)
        throw Error("Report preview arena is full");
    const auto addr = next_;
    next_ += aligned;
    blocks_[addr] = aligned;
    return addr;
}
quint32 SceneReportRuntime::loadResource(quint32 key, bool compressed) {
    const quint64 at = 0x594280ULL + key;
    if (!key || at + 4 > quint64(rom_.bytes().size()))
        throw Error("Report references an invalid resource");
    const auto size = u32(rom_.bytes(), at);
    if (size > 0x200000 || at + 4 + size > quint64(rom_.bytes().size()))
        throw Error("Report resource exceeds its allocation");
    auto data = rom_.bytes().mid(at + 4, size);
    if (compressed) {
        const auto decoded = u32(data, 0);
        if (decoded > 0x200000)
            throw Error("Report resource exceeds its decode budget");
        data = decodeLzss(data.mid(4), decoded);
    }
    const auto address = allocate(data.size());
    machine_.mount(address, data);
    return address;
}
void SceneReportRuntime::attachServices() {
    // Rebind after a playback checkpoint is copied. Every callback accesses
    // only the copy currently being executed.
    auto &m = machine_;
    auto &r = m.registers;
    m.service(0x80070f30, [&] { r[2] = allocate(r[4]); });
    m.service(0x80071c04, [&] { r[2] = allocate(r[4]); });
    m.service(0x80071288, [&] { r[2] = allocate(r[4]); });
    m.service(0x80089f80, [&] {
        const auto offset = r[4], size = r[6];
        if (size > 0x200000 || quint64(offset) + size > quint64(rom_.bytes().size()))
            throw Error("Report resource DMA exceeds its ROM allocation");
        m.mount(r[5], rom_.bytes().mid(offset, size));
    });
    m.service(0x8009dd38, [&] { r[2] = loadResource(r[4] & 0x0fffffff, true); });
    m.service(0x800712c4, [&] {
        if (blocks_.contains(r[4]))
            free_[r[4]] = blocks_.take(r[4]);
        r[2] = 0;
    });
    m.service(0x80093380, [&] {
        if (r[5])
            std::memset(m.memory.data() + m.physical(r[4], r[5]), 0, r[5]);
    });
    m.service(0x80093460, [&] { r[2] = string(r[4], 0x10000).size(); });
    auto move = [&](quint32 d, quint32 s, quint32 n) {
        if (n)
            std::memmove(m.memory.data() + m.physical(d, n),
                         m.memory.constData() + m.physical(s, n), n);
    };
    m.service(0x80080998, [&, move] { move(r[4], r[5], r[6]); });
    m.service(0x80093060, [&, move] { move(r[5], r[4], r[6]); });
    m.service(0x8009c550, [&] {
        m.mount(r[4], string(r[5], 0x500) + char(0));
        r[2] = r[4];
    });
    m.service(0x8009c620, [&] {
        m.mount(r[4] + string(r[4], 0x500).size(), string(r[5], 0x500) + char(0));
        r[2] = r[4];
    });
    m.service(0x80227a18, [&] {
        const auto directory = loadResource(r[4], false);
        const auto size = u32(rom_.bytes(), 0x594280 + r[4]);
        if (r[5] >= size / 4)
            throw Error("Report entry exceeds its resource directory");
        const auto key = m.word(directory + 4 * r[5]);
        free_[directory] = blocks_.take(directory);
        r[2] = loadResource(key, true);
    });
    m.service(0x800ea604, [&] { sounds_ << int(r[5]); });
    m.service(resident + 0x5a0f4,
              [] {}); // Shared panel animation is stepped below, without submitting a display list.
}
int SceneReportRuntime::call(quint32 address, quint32 a0, quint32 a1) {
    auto &m = machine_;
    attachServices();
    m.registers[4] = a0;
    m.registers[5] = a1;
    // Building a detail page repeatedly measures its wrapped lines with the
    // resident font code. Ordinary input updates have a smaller budget.
    m.call(address, address == report + 0x297cc8 ? 4000000 : 400000);
    return qint32(m.registers[2]);
}
void SceneReportRuntime::create(int slot, int preset) {
    call(report + 0x297cc8, slot, preset);
}
void SceneReportRuntime::help(int slot, int section) {
    call(report + 0x298914, slot, section);
}
void SceneReportRuntime::release(int slot) {
    call(shared + 0x283748, slot);
}
void SceneReportRuntime::close(int slot, bool immediate) {
    call(report + 0x299ca8, slot, immediate);
}
void SceneReportRuntime::menuLabel() {
    const auto a = machine_.byte(0x8022a99a);
    if (a >= 3)
        machine_.byte(0x8022a99a, a - 1);
}
void SceneReportRuntime::advance(quint16 pressed, quint16 direction) {
    sounds_.clear();
    machine_.half(0x800e8100, pressed);
    machine_.half(0x800c4b28, pressed);
    machine_.half(0x800e8700, direction);
    call(report + 0x299740);
    const auto head = machine_.word(0x8018fdc0);
    machine_.half(head + 0xd0, quint16(machine_.half(head + 0xd0) + 1));
    int count = 0;
    for (auto a = machine_.word(head + 4); a;) {
        if (++count > 64)
            throw Error("Invalid Report panel list");
        const auto next = machine_.word(a + 4);
        auto record = read(a, 232);
        const bool removed = lordly_panel_animate(reinterpret_cast<uint8_t *>(record.data()));
        machine_.mount(a, record);
        if (removed)
            call(resident + 0x521fc, a);
        a = next;
    }
}
int SceneReportRuntime::pageIndicatorFrame() const {
    // The active panel takes one 16-pixel sprite from the atlas every four ticks.
    return (machine_.half(machine_.word(0x8018fdc0) + 0xd0) >> 2) & 3;
}
QVector<SceneReportRuntime::Slot> SceneReportRuntime::slotsInUse() const {
    QVector<Slot> result;
    const auto &m = machine_;
    const auto scene = m.word(0x8022a974);
    for (int i = 0; i < 14; ++i) {
        const auto addr = m.word(scene + 0x19b8 + i * 4);
        if (addr)
            result << Slot{i, qint32(m.word(addr)), qint16(m.half(addr + 0x14)), m.word(addr + 12)};
    }
    return result;
}
QVector<SceneReportRuntime::Panel> SceneReportRuntime::panels() const {
    QVector<Panel> result;
    const auto &m = machine_;
    for (auto a = m.word(m.word(0x8018fdc0) + 4); a; a = m.word(a + 4)) {
        if (result.size() >= 64)
            throw Error("Report panel list is cyclic or exceeds the preview budget");
        const auto payload = m.word(a + 0xd0);
        if (!payload)
            throw Error("Report panel has no text-state record");
        auto content = read(payload, 0x604);
        const auto preset = u32(content, 0x600) & 255;
        const bool paged = preset == 6 || preset == 11;
        int cursor = 0;
        const int count = std::min(48, int(u8(content, paged ? 0x5f4 : 0x5f5)));
        for (int i = 0; i < count; ++i) {
            const auto ptr = m.word(payload + 0x530 + i * 4);
            const auto line = ptr ? string(ptr, 0x4ff) : QByteArray{};
            if (cursor + line.size() + 1 > 0x500)
                throw Error("Report page exceeds its text budget");
            put32(content, 0x530 + i * 4, cursor);
            content.replace(cursor, line.size(), line);
            cursor += line.size();
            put8(content, cursor++, 0);
        }
        result << Panel{a, read(a, 232), content};
    }
    return result;
}
} // namespace lordly
