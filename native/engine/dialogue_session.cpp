#include "engine/dialogue_session.h"
namespace lordly {
DialogueSession::DialogueSession(Rom rom, int archive, quint16 entry,
                                 DialogueTextEnvironment environment,
                                 std::optional<Dialogues> dialogues)
    : environment_(std::move(environment)), playback_(std::move(rom), std::move(dialogues)) {
    if (archive < 0 || archive > 65535)
        throw Error("Dialogue archive selector exceeds its native field");
    // This is a standalone window centered on a 320x240 preview, without an Actor.
    const int slot = resources_.registerWindow(0x80198be8, 0, 0, 0, 0);
    auto record = resources_.record(slot);
    put16(record, 0x88, entry);
    put16(record, 0x96, quint16(archive));
    put8(record, 0x8a, 0x40);
    put16(record, 0x8c, 160);
    put16(record, 0x94, 120);
    put16(record, 0x92, 0);   // No override: retain the source portrait, initially absent.
    playback_.source(record); // Reject absent targets before publishing.
    resources_.supplyRecord(slot, record);
}
DialoguePlayback::DialoguePlayback(Rom rom, std::optional<Dialogues> dialogues)
    : rom_(std::move(rom)), dialogues_(std::move(dialogues)) {}
DialogueTextEnvironment DialoguePlayback::environment(const Rom &rom) {
    DialogueTextEnvironment result;
    constexpr quint32 at = 0x218c450 + 0x594280;
    const auto size = u32(rom.bytes(), at);
    if (size > 65536 || quint64(at) + 4 + size > quint64(rom.bytes().size()))
        throw Error("Dialogue metric resource exceeds its ROM envelope");
    result.metrics.normal = rom.bytes().mid(at + 4, size);
    result.longNameSuffix = QByteArray(1, 0);
    return result;
}
QByteArray DialoguePlayback::source(const QByteArray &record) const {
    if (!dialogues_)
        return DialogueArchive::windowSource(rom_, record);
    const auto group = u16(record, 0x88) & 0xc000;
    if (group != 0 && group != 0x8000)
        throw Error("Dialogue window selects an unresolved archive table group");
    return dialogues_->entry(group ? Dialogues::Table::Alternate : Dialogues::Table::Primary,
                             u16(record, 0x96), u16(record, 0x88) & 0x3fff) +
           char(0);
}
void DialogueSession::advance(DialogueInput input) {
    playback_.advance(resources_, windows_, environment_, input);
    ++updates_;
}
void DialoguePlayback::advance(SceneResources &resources, SceneDialogue &windows,
                               const DialogueTextEnvironment &environment, DialogueInput input) {
    auto next = *this;
    auto nextResources = resources;
    auto nextWindows = windows;
    auto source = [&](const QByteArray &record) { return next.source(record); };
    const auto shake = rom_.bytes().mid(0xeaf24, 7);
    auto &pass = next.dispatcher_.state();
    next.runtime_.clearSounds();
    if (pass.queue.count) {
        nextResources.advanceOpening(pass.queue);
        for (int i = 0; i < pass.queue.count; ++i) {
            const int slot = pass.queue.entries[i];
            if (nextResources.advanceClosing(slot)) {
                QByteArray scratch(168, 0);
                if (!next.runtime_.service(nextResources, scratch, pass,
                                           {SceneResourceDispatcher::Service::Release, slot, 0},
                                           nextWindows, environment, input, shake, source))
                    throw Error("Dialogue encountered an unknown closing resource");
            }
        }
        nextResources.rebuildPriority(pass.queue);
    }
    auto handler = [&](SceneResources &pool, QByteArray &record, SceneResourcePassState &state,
                       const SceneResourceDispatcher::Call &call) {
        return next.runtime_.service(pool, record, state, call, nextWindows, environment, input,
                                     shake, source);
    };
    if (next.dispatcher_.advance(nextResources, handler) != SceneResourceDispatcher::Complete)
        throw Error("Dialogue resource pass could not complete");
    resources = std::move(nextResources);
    windows = std::move(nextWindows);
    *this = std::move(next);
}
void DialogueSession::resume() {
    windows_.resume(0, resources_);
}
int DialogueSession::state() const {
    return playback_.state(0, resources_);
}
int DialoguePlayback::state(int slot, const SceneResources &resources) const {
    const auto window = runtime_.window(slot, resources);
    return window ? u8(window->frame.state, 0x3c) : -1;
}
int DialogueSession::activeWindows() const {
    int count = 0;
    for (int i = 0; i < 6; ++i)
        count += resources_.active(i) ? 1 : 0;
    return count;
}
RasterImage DialogueSession::image(DialogueDrawing &drawing) const {
    return playback_.image(resources_, drawing);
}
RasterImage DialoguePlayback::image(const SceneResources &resources,
                                    DialogueDrawing &drawing) const {
    RasterImage image{320, 240, QByteArray(320 * 240 * 4, 0)};
    // Shared resources draw from lowest to highest priority.
    const auto &queue = dispatcher_.state().queue;
    for (int i = int(queue.count) - 1; i >= 0; --i) {
        const int slot = queue.entries[i];
        const auto window = runtime_.window(slot, resources);
        if (window)
            DialogueDrawing::paint(image,
                                   drawing.compose(resources.record(slot), window->frame.state));
    }
    return image;
}
} // namespace lordly
