#include "engine/dialogue_session.h"
namespace lordly {
DialogueSession::DialogueSession(Rom rom, int archive, quint16 entry,
                                 DialogueTextEnvironment environment)
    : rom_(std::move(rom)), environment_(std::move(environment)) {
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
    put16(record, 0x92, 0); // No override: retain the source portrait, initially absent.
    DialogueArchive::windowSource(rom_, record); // Reject absent targets before publishing.
    resources_.supplyRecord(slot, record);
}
void DialogueSession::advance(DialogueInput input) {
    auto next = *this;
    auto source = [&](const QByteArray &record) {
        return DialogueArchive::windowSource(rom_, record);
    };
    const auto shake = rom_.bytes().mid(0xeaf24, 7);
    auto &pass = next.dispatcher_.state();
    next.runtime_.clearSounds();
    if (pass.queue.count) {
        next.resources_.advanceOpening(pass.queue);
        for (int i = 0; i < pass.queue.count; ++i) {
            const int slot = pass.queue.entries[i];
            if (next.resources_.advanceClosing(slot)) {
                QByteArray scratch(168, 0);
                if (!next.runtime_.service(next.resources_, scratch, pass,
                                           {SceneResourceDispatcher::Service::Release, slot, 0},
                                           next.windows_, environment_, input, shake, source))
                    throw Error("Standalone dialogue encountered an unknown closing resource");
            }
        }
        next.resources_.rebuildPriority(pass.queue);
    }
    auto handler = [&](SceneResources &pool, QByteArray &record, SceneResourcePassState &state,
                       const SceneResourceDispatcher::Call &call) {
        return next.runtime_.service(pool, record, state, call, next.windows_, environment_, input,
                                     shake, source);
    };
    if (next.dispatcher_.advance(next.resources_, handler) != SceneResourceDispatcher::Complete)
        throw Error("Standalone dialogue resource pass could not complete");
    ++next.updates_;
    *this = std::move(next);
}
void DialogueSession::resume() {
    windows_.resume(0, resources_);
}
int DialogueSession::state() const {
    const auto window = runtime_.window(0, resources_);
    return window ? u8(window->frame.state, 0x3c) : -1;
}
int DialogueSession::activeWindows() const {
    int count = 0;
    for (int i = 0; i < 6; ++i)
        count += resources_.active(i) ? 1 : 0;
    return count;
}
RasterImage DialogueSession::image(DialogueDrawing &drawing) const {
    RasterImage image{320, 240, QByteArray(320 * 240 * 4, 0)};
    // Shared resources draw from lowest to highest priority.
    const auto &queue = dispatcher_.state().queue;
    for (int i = int(queue.count) - 1; i >= 0; --i) {
        const int slot = queue.entries[i];
        const auto window = runtime_.window(slot, resources_);
        if (window)
            DialogueDrawing::paint(image,
                                   drawing.compose(resources_.record(slot), window->frame.state));
    }
    return image;
}
} // namespace lordly
