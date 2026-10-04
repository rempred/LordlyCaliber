#include "engine/director_session.h"
#include "game/director/stream_words.h"
namespace lordly {
void DirectorSession::advanceCounter() {
    // ROM 283664: unsigned (counter - 1) <= 0x0FFFFFFD. Zero stays unarmed.
    if (state_.counter - 1U <= 0x0ffffffdU)
        ++state_.counter;
}
void DirectorSession::setMode(int mode) {
    if (mode < 0 || mode > 2)
        throw Error("Director mode is outside 0..2");
    state_.mode = mode;
}
void DirectorLaunchTranslations::set(int index, quint16 value) {
    if (index < 0 || index >= 256)
        throw Error("Launch translation index is outside 0..255");
    values[index] = value;
    known[index] = 1;
}
DirectorSession::DirectorSession(Directors resources, int selector, int mode,
                                 DirectorLaunchTranslations translations)
    : resources_(std::move(resources)), translations_(translations) {
    if (mode < 0 || mode > 2)
        throw Error("A Director session requires a known launch mode");
    current_ = load(selector, false);
    state_.mode = mode;
}
DirectorSession::Stream DirectorSession::load(int selector, bool continuation) const {
    const auto &indices =
        continuation ? resources_.continuationSelectors() : resources_.selectors();
    const auto &entries = continuation ? resources_.continuations() : resources_.entries();
    if (selector < 0 || selector >= indices.size() || !indices[selector] ||
        !entries.contains(indices[selector]))
        throw Error("Director transfer selects an absent resource");
    const auto &resource = entries[indices[selector]];
    const auto bytes = resource.program.encode();
    Stream result{{}, resource.key, selector, 0, continuation};
    result.words.reserve(bytes.size() / 4);
    for (int at = 0; at < bytes.size(); at += 4)
        result.words << u32(bytes, at);
    int missing = -1;
    const auto status = lordly_director_translate(result.words.data(), int(result.words.size()),
                                                  translations_.values.data(),
                                                  translations_.known.data(), &missing);
    if (status == 1)
        throw Error(QString("Scene requires launch translation value %1").arg(missing));
    if (status)
        throw Error("Invalid Director translation input");
    return result;
}
int DirectorSession::lookup(void *, LordlyDirectorWord word, LordlyDirectorDefinition *out) {
    const auto found = Directors::grammar().constFind(word);
    if (found == Directors::grammar().end())
        return 0;
    *out = {found->words, found->queryWords, found->requiredMode};
    return 1;
}
int DirectorSession::invoke(void *context, LordlyDirectorWord opcode,
                            const LordlyDirectorWord *words, int count, int *value) {
    auto &session = *static_cast<DirectorSession *>(context);
    if (!session.service_ || !*session.service_)
        return 0;
    try {
        const QVector<quint32> operands(words + 1, words + count);
        return (*session.service_)(opcode, operands, *value) ? 1 : 0;
    } catch (const std::exception &e) {
        session.reason_ = QString::fromUtf8(e.what());
        return 0;
    }
}
int DirectorSession::dispatch(const Service &service, int instructionBudget) {
    if (instructionBudget < 1)
        throw Error("Director instruction budget must be positive");
    if (terminalResult_ != LORDLY_DIRECTOR_YIELD)
        return terminalResult_;
    reason_.clear();
    service_ = &service;
    int remaining = instructionBudget;
    while (remaining > 0) {
        const int result =
            lordly_director_dispatch(&state_, current_.words.constData(),
                                     int(current_.words.size()), remaining, lookup, invoke, this);
        remaining -= state_.executed;
        if (result != LORDLY_DIRECTOR_TRANSFER) {
            if (result == LORDLY_DIRECTOR_END || result == LORDLY_DIRECTOR_RELEASE)
                terminalResult_ = result;
            else if (result == LORDLY_DIRECTOR_SERVICE && reason_.isEmpty())
                reason_ = QString("Game service for command %1 at word %2 is unavailable")
                              .arg(current_.words.value(state_.cursor), 8, 16, QChar('0'))
                              .arg(state_.cursor);
            else if (result == LORDLY_DIRECTOR_INVALID)
                reason_ = "Director execution reached an invalid word or branch";
            else if (result == LORDLY_DIRECTOR_BUDGET)
                reason_ = "Director instruction budget reached";
            service_ = nullptr;
            return result;
        }
        const auto opcode = current_.words[state_.cursor];
        try {
            if (opcode == 0x80000003 || opcode == 0xbb) {
                if (opcode == 0xbb && !requestedSelector_)
                    throw Error("Scene replacement requires the current gameplay Director request");
                auto next = load(opcode == 0xbb ? *requestedSelector_
                                                : qint32(current_.words[state_.cursor + 1]),
                                 false);
                current_ = std::move(next);
                previous_.reset();
                state_.resume_cursor = state_.cursor = state_.active = 0;
                service_ = nullptr;
                return LORDLY_DIRECTOR_YIELD;
            }
            if (state_.stream_selector == 254) {
                if (!previous_)
                    throw Error("Continuation return has no saved parent stream");
                int resume = previous_->resume;
                if (state_.barrier_state == 2 &&
                    lordly_director_find_label(current_.words.constData(),
                                               int(current_.words.size()), 12345, &resume))
                    throw Error("Barrier redirect label is absent from the returning stream");
                current_ = std::move(*previous_);
                previous_.reset();
                state_.resume_cursor = resume;
                state_.active = 0;
                if (state_.barrier_state == 2)
                    state_.barrier_state = 0;
                service_ = nullptr;
                return LORDLY_DIRECTOR_YIELD;
            }
            auto next = load(state_.stream_selector, true);
            if (!previous_) {
                current_.resume = state_.handoff ? state_.cursor + 1 : state_.resume_cursor;
                previous_ = std::move(current_);
            }
            current_ = std::move(next);
            state_.cursor = state_.resume_cursor = 0;
            state_.stream_selector = 255;
            // Same native call continues: nesting, scan mode and handoff remain local.
        } catch (const std::exception &e) {
            reason_ = QString::fromUtf8(e.what());
            service_ = nullptr;
            return LORDLY_DIRECTOR_TRANSFER;
        }
    }
    reason_ = "Director instruction budget reached";
    service_ = nullptr;
    return LORDLY_DIRECTOR_BUDGET;
}
} // namespace lordly
