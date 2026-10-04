#include "engine/scene_dialogue_context.h"
namespace lordly {
namespace {
QByteArray stringAt(const QByteArray &bytes, qsizetype offset, qsizetype extent) {
    if (offset < 0 || extent < 1 || offset > bytes.size() || extent > bytes.size() - offset)
        throw Error("Dialogue name exceeds its current source record");
    const auto end = bytes.indexOf('\0', offset);
    if (end < offset || end >= offset + extent)
        throw Error("Dialogue name lacks a bounded terminator");
    return bytes.mid(offset, end - offset);
}
void storeName(QByteArray &workspace, const QByteArray &name) {
    if (name.size() >= 64)
        throw Error("Dialogue name exceeds the native 64-byte workspace");
    workspace.fill(0);
    workspace.replace(0, name.size(), name);
}
} // namespace
SceneDialogueContext::SceneDialogueContext(DialogueNameInputs inputs) : inputs_(std::move(inputs)) {
    if (inputs_.army.size() != 100 * 56 || inputs_.guests.size() != 100 * 52 ||
        inputs_.groups.size() % 25 || inputs_.currentGroup >= inputs_.groups.size() / 25)
        throw Error("Dialogue context requires current roster and group records");
    stringAt(inputs_.fixedName, 0, inputs_.fixedName.size());
    for (const auto &override : inputs_.overrides) {
        if (override.selector == 255)
            throw Error("Dialogue name override uses the table terminator");
        stringAt(override.text, 0, override.text.size());
    }
}
const SceneDialogueContext::Reference &SceneDialogueContext::reference(int index) const {
    if (index < 0 || index >= 4)
        throw Error("Dialogue name index is outside 0..3");
    return names_[index];
}
QByteArray SceneDialogueContext::name(int index) const {
    const auto ref = reference(index);
    switch (ref.kind) {
    case Kind::Absent:
        return {};
    case Kind::Army:
        return stringAt(inputs_.army, ref.index * 56, 56);
    case Kind::Guest:
        return stringAt(inputs_.guests, ref.index * 52, 52);
    case Kind::Fixed:
        return stringAt(inputs_.fixedName, 0, inputs_.fixedName.size());
    case Kind::OwnerWorkspace:
        return stringAt(owner_, 0, 64);
    case Kind::GroupWorkspace:
        return stringAt(group_, 0, 64);
    }
    throw Error("Unknown dialogue name reference");
}
SceneDialogueContext::Reference SceneDialogueContext::find(int source) const {
    for (int i = 0; i < 100; ++i)
        if (u8(inputs_.guests, i * 52 + 0x11) == source)
            return {Kind::Guest, i};
    for (int i = 0; i < 100; ++i)
        if (u8(inputs_.army, i * 56 + 0x11) == source)
            return {Kind::Army, i};
    return {};
}
SceneDialogueContext::Reference SceneDialogueContext::ownerName(const QByteArray &actor,
                                                                const QByteArray &row) {
    if (actor.isEmpty())
        return {};
    if (actor.size() != 336)
        throw Error("Dialogue owner requires its complete Actor record");
    if (u8(actor, 0x147) >= 20)
        return {};
    if (row.size() != 248)
        throw Error("Dialogue owner requires its current army row");
    const auto identity = u32(row, 0x48);
    if (!identity)
        return {};
    QByteArray name = stringAt(inputs_.army, u8(row, 0xf6) * 56, 56);
    for (const auto &override : inputs_.overrides)
        if (override.selector == identity)
            name += stringAt(override.text, 0, override.text.size());
    storeName(owner_, name);
    return {Kind::OwnerWorkspace};
}
SceneDialogueContext::Reference SceneDialogueContext::groupName() {
    const int member = u8(inputs_.groups, inputs_.currentGroup * 25 + 2);
    if (member >= 100)
        throw Error("Dialogue group member exceeds the current army roster");
    const auto identity = u8(inputs_.army, member * 56 + 0x11);
    if (!identity)
        return {};
    QByteArray name = stringAt(inputs_.army, member * 56, 56);
    for (const auto &override : inputs_.overrides) {
        if (override.selector != identity)
            continue;
        name += stringAt(override.text, 0, override.text.size());
        break;
    }
    storeName(group_, name);
    return {Kind::GroupWorkspace};
}
void SceneDialogueContext::prepare(int mode, const QByteArray &actor, const QByteArray &row) {
    if (mode < 0 || mode > 2)
        throw Error("Dialogue scene mode is outside 0..2");
    auto next = *this;
    // Accepted func_0004e434 (50aed758): U16 80190F30 -> U16 800E9C0C.
    next.textParameter_ = inputs_.defaultTextParameter;
    next.names_[0] = find(81);
    if (next.names_[0].kind == Kind::Absent)
        next.names_[0] = find(82);
    if (next.names_[0].kind == Kind::Absent)
        next.names_[0] = find(83);
    if (mode == 2) {
        next.names_[1] = next.ownerName(actor, row);
        if (next.names_[1].kind == Kind::Absent)
            next.names_[1] = next.names_[0];
    }
    next.names_[2] = {Kind::Fixed};
    next.names_[3] = next.groupName();
    *this = std::move(next);
}
} // namespace lordly
