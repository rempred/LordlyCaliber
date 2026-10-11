#include "engine/scene_dialogue_runtime.h"
namespace lordly {
namespace {
constexpr quint32 initialize = 0x80198be8, update = 0x8019981c, release = 0x801998a0;
}
const SceneDialogueRuntime::Window *SceneDialogueRuntime::window(int slot,
                                                                 const SceneResources &pool) const {
    const auto record = pool.record(slot);
    if (!(u16(record, 0) & 0x8000) || u32(record, 0x10) != initialize)
        return nullptr;
    const auto found = saved_.constFind(u32(record, 0x24));
    return found == saved_.cend() ? nullptr : &found.value();
}
bool SceneDialogueRuntime::service(SceneResources &pool, QByteArray &scratch,
                                   SceneResourcePassState &pass,
                                   const SceneResourceDispatcher::Call &call,
                                   SceneDialogue &dialogue,
                                   const DialogueTextEnvironment &environment, DialogueInput input,
                                   const QByteArray &shake, const Source &source) {
    auto next = *this;
    auto nextDialogue = dialogue;
    auto nextPool = pool;
    auto nextScratch = scratch;
    if (!next.apply(nextPool, nextScratch, pass, call, nextDialogue, environment, input, shake,
                    source))
        return false;
    pool = std::move(nextPool);
    scratch = std::move(nextScratch);
    dialogue = std::move(nextDialogue);
    *this = std::move(next);
    return true;
}
bool SceneDialogueRuntime::apply(SceneResources &pool, QByteArray &scratch,
                                 SceneResourcePassState &pass,
                                 const SceneResourceDispatcher::Call &call, SceneDialogue &dialogue,
                                 const DialogueTextEnvironment &environment, DialogueInput input,
                                 const QByteArray &shake, const Source &source) {
    using Service = SceneResourceDispatcher::Service;
    if (call.service == Service::Release) {
        if (u32(pool.record(call.slot), 0x10) != initialize)
            return false;
        // Release callbacks see their pool record, not the last callback scratch.
        const auto handles = pool.release(call.slot, [&](SceneResources &next, int slot,
                                                         quint32 address) {
            if (address != release)
                throw Error("Dialogue owner cannot execute an unknown resource release callback");
            dialogue.setSharedStatus(u8(next.record(slot), 0x91) & 3);
        });
        for (const auto handle : handles)
            saved_.remove(handle);
        return true;
    }
    if (u32(scratch, 0x10) != initialize)
        return false;
    switch (call.service) {
    case Service::Initialize: {
        if (call.address != initialize || !source)
            return false;
        auto text = source(scratch);
        if (text.isNull() || text.indexOf('\0') < 0)
            throw Error("Dialogue source requires an explicit terminated archive entry");
        const auto initialized = initializeDialogueWindow(scratch, call.slot, text, environment);
        current_ =
            Window{std::move(text), DialogueTextFrame{initialized.window, initialized.state}};
        currentSlot_ = call.slot;
        scratch = initialized.window;
        return true;
    }
    case Service::CaptureRender:
        // This initializer creates no framework render nodes. Its separate draw
        // callback remains on record +18; the renderer consumes that identity.
        if (u8(scratch, 3) & 2)
            throw Error("Dialogue resource unexpectedly owns a framework render node");
        put32(scratch, 0x98, 0);
        return true;
    case Service::RestorePayload: {
        const auto handle = u32(scratch, 0x24);
        const auto found = saved_.constFind(handle);
        if (found == saved_.cend())
            throw Error("Dialogue resource payload has no matching host owner");
        current_ = found.value();
        saved_.remove(handle); // Native restore consumes its temporary heap copy.
        currentSlot_ = call.slot;
        return true;
    }
    case Service::Update: {
        if (call.address != update)
            return false;
        if (!current_ || currentSlot_ != call.slot)
            throw Error("Dialogue update has no restored payload");
        auto &frame = current_->frame;
        frame.window = scratch;
        for (int i = 0; i < 8; ++i)
            frame.controls[i] = dialogue.control(i);
        if (!pass.selectedInput)
            input.pressed = input.repeat = input.history = 0;
        advanceDialogueWindow(frame, current_->source, environment, input, shake);
        for (const auto &child : frame.children) {
            const int slot = pool.registerWindow(initialize, child.x, child.y, 0, 0);
            auto record = pool.record(slot);
            put16(record, 0x88, child.entry);
            pool.supplyRecord(slot, record);
        }
        for (int i = 0; i < 8; ++i)
            dialogue.setControl(i, frame.controls[i]);
        sounds_ += frame.sounds;
        scratch = frame.window;
        return true;
    }
    case Service::SavePayload: {
        if (!current_ || currentSlot_ != call.slot || u16(scratch, 0x20) != 1144 ||
            (u8(scratch, 1) & 1))
            throw Error("Dialogue payload does not match its owned save representation");
        if (!nextHandle_)
            throw Error("Dialogue host payload identities exhausted");
        const auto handle = nextHandle_++;
        put32(scratch, 0x24, handle);
        current_->frame.window = scratch;
        saved_.insert(handle, *current_);
        current_.reset();
        currentSlot_ = -1;
        return true;
    }
    case Service::AdvanceRender:
        return false;
    case Service::Release:
        break;
    }
    return false;
}
} // namespace lordly
