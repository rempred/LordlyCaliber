#include "engine/scene_resource_dispatch.h"
namespace lordly {
void SceneResourceDispatcher::current(SceneResources &pool, int slot) {
    auto context = pool.context();
    context.context = slot;
    pool.setContext(context);
}
void SceneResourceDispatcher::clearHead() {
    if (slot_ == state_.queue.entries[0])
        put8(scratch_, 2, u8(scratch_, 2) & 0xfb);
}
bool SceneResourceDispatcher::invoke(SceneResources &pool, const Handler &handler, Service service,
                                     quint32 address) {
    pending_ = Call{service, slot_, address};
    if (!handler)
        return false;
    auto nextPool = pool;
    auto nextScratch = scratch_;
    auto nextState = state_;
    if (!handler(nextPool, nextScratch, nextState, *pending_))
        return false;
    if (nextScratch.size() != 168)
        throw Error("Resource callback must preserve its complete scratch record");
    pool = std::move(nextPool);
    scratch_ = std::move(nextScratch);
    state_ = nextState;
    pending_.reset();
    return true;
}
SceneResourceDispatcher::Result
SceneResourceDispatcher::advance(SceneResources &pool, const Handler &handler, int stageBudget) {
    if (stageBudget <= 0)
        return Budget;
    if (!running_) {
        phase_ = Phase::InitScan;
        slot_ = lastInitialized_ = 0;
        running_ = true;
        pending_.reset();
    }
    while (stageBudget-- > 0) {
        switch (phase_) {
        case Phase::InitScan: {
            current(pool, slot_);
            const auto record = pool.record(slot_);
            if ((u16(record, 0) & 0xa000) == 0x8000 && u32(record, 0x10)) {
                scratch_ = record;
                if (slot_ == state_.queue.entries[0])
                    put8(scratch_, 2, u8(scratch_, 2) | 4);
                put8(scratch_, 4, 6);
                put32(scratch_, 0x98, state_.renderContext);
                phase_ = Phase::Initialize;
            } else
                phase_ = Phase::InitNext;
            break;
        }
        case Phase::Initialize:
            if (!invoke(pool, handler, Service::Initialize, u32(scratch_, 0x10)))
                return Suspended;
            phase_ = Phase::CaptureRender;
            break;
        case Phase::CaptureRender:
            if (!invoke(pool, handler, Service::CaptureRender))
                return Suspended;
            put16(scratch_, 0, u16(scratch_, 0) | 0x2000);
            clearHead();
            phase_ = Phase::InitSave;
            break;
        case Phase::InitSave:
            if (!invoke(pool, handler, Service::SavePayload))
                return Suspended;
            phase_ = Phase::InitCommit;
            break;
        case Phase::InitCommit:
            pool.supplyRecord(slot_, scratch_);
            lastInitialized_ = slot_;
            phase_ = (u8(scratch_, 2) & 2) ? Phase::InitRelease : Phase::InitNext;
            break;
        case Phase::InitRelease:
            if (!invoke(pool, handler, Service::Release))
                return Suspended;
            phase_ = Phase::InitNext;
            break;
        case Phase::InitNext:
            slot_ = (slot_ + 1) % 6;
            phase_ = slot_ == lastInitialized_ ? Phase::Priority : Phase::InitScan;
            break;
        case Phase::Priority:
            pool.rebuildPriority(state_.queue);
            slot_ = 0;
            phase_ = Phase::UpdateScan;
            break;
        case Phase::UpdateScan: {
            current(pool, slot_);
            const auto record = pool.record(slot_);
            if ((u16(record, 0) & 0xa000) == 0xa000 && u32(record, 0x14)) {
                scratch_ = record;
                if (slot_ == state_.queue.entries[0])
                    put8(scratch_, 2, u8(scratch_, 2) | 4);
                phase_ = Phase::Restore;
            } else if (++slot_ == 6)
                phase_ = Phase::FinalPriority;
            break;
        }
        case Phase::Restore:
            if (!invoke(pool, handler, Service::RestorePayload))
                return Suspended;
            state_.selectedInput = slot_ == state_.queue.entries[0];
            phase_ = Phase::Update;
            break;
        case Phase::Update:
            if (!invoke(pool, handler, Service::Update, u32(scratch_, 0x14)))
                return Suspended;
            phase_ = Phase::Render;
            break;
        case Phase::Render:
            if ((u8(scratch_, 3) & 2) && !invoke(pool, handler, Service::AdvanceRender))
                return Suspended;
            clearHead();
            phase_ = Phase::UpdateSave;
            break;
        case Phase::UpdateSave:
            if (!invoke(pool, handler, Service::SavePayload))
                return Suspended;
            phase_ = Phase::UpdateCommit;
            break;
        case Phase::UpdateCommit:
            pool.supplyRecord(slot_, scratch_);
            phase_ = (u8(scratch_, 2) & 2) ? Phase::UpdateRelease : Phase::UpdateNext;
            break;
        case Phase::UpdateRelease:
            if (!invoke(pool, handler, Service::Release))
                return Suspended;
            phase_ = Phase::UpdateNext;
            break;
        case Phase::UpdateNext:
            state_.randomSeed = state_.randomSeed * 0x4e6dU + 0x3039U;
            --state_.queue.count;
            if (!state_.queue.count || ++slot_ == 6)
                phase_ = Phase::FinalPriority;
            else
                phase_ = Phase::UpdateScan;
            break;
        case Phase::FinalPriority:
            pool.rebuildPriority(state_.queue);
            current(pool, -1);
            slot_ = 0;
            phase_ = state_.cleanupStatus == 0xffff ? Phase::Finish : Phase::CleanupScan;
            break;
        case Phase::CleanupScan:
            if (pool.active(slot_) && (u8(pool.record(slot_), 3) & 4)) {
                // Release uses the pool; it does not replace the last scratch record.
                if (scratch_.isEmpty())
                    scratch_ = QByteArray(168, 0);
                phase_ = Phase::CleanupRelease;
            } else if (++slot_ == 6)
                phase_ = Phase::Finish;
            break;
        case Phase::CleanupRelease:
            if (!invoke(pool, handler, Service::Release))
                return Suspended;
            phase_ = ++slot_ == 6 ? Phase::Finish : Phase::CleanupScan;
            break;
        case Phase::Finish:
            running_ = false;
            pending_.reset();
            return Complete;
        }
    }
    return Budget;
}
} // namespace lordly
