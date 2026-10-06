#include "engine/scene_playback_inputs.h"
#include <QFile>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <bit>
#include <cmath>
#include <algorithm>
namespace lordly {
namespace {
QByteArray bytes(const QJsonValue &v, int size) {
    const auto hex = v.toString().toLatin1(), data = QByteArray::fromHex(hex);
    if (!v.isString() || data.size() != size || data.toHex() != hex.toLower())
        throw Error("Playback input has an incomplete byte record");
    return data;
}
float real(const QByteArray &data, int at) { return std::bit_cast<float>(u32(data, at)); }
QByteArray memory(const QJsonArray &regions, quint32 address, int size) {
    for (const auto &value : regions) {
        const auto region = value.toObject();
        const auto base = region["address"].toInteger(-1);
        const auto hex = region["hex"].toString().toLatin1();
        if (base < 0x80000000LL || base > 0x80400000LL || hex.size() % 2) throw Error("Invalid captured memory region");
        const auto data = QByteArray::fromHex(hex);
        if (data.toHex() != hex.toLower() || base + data.size() > 0x80400000LL)
            throw Error("Invalid captured memory bytes");
        if (address >= base && quint64(address) + size <= quint64(base + data.size()))
            return data.mid(address - base, size);
    }
    throw Error(QString("Captured input is missing RAM %1 (%2 bytes)").arg(address, 8, 16, QChar('0')).arg(size));
}
SceneCamera camera(const QJsonObject &object) {
    const auto values = object["values"].toArray();
    QByteArray bank(56, 0);
    if (values.size() != 14) throw Error("Playback camera requires fourteen values");
    for (int i = 0; i < 14; ++i) put32(bank, i * 4, std::bit_cast<quint32>(float(values[i].toDouble())));
    return SceneCamera(bank, float(object["modelScale"].toDouble()));
}
}
ScenePlaybackInputs ScenePlaybackInputs::parse(const QByteArray &data, const QString &assetId) {
    if (data.size() > 128 * 1024) throw Error("Playback inputs exceed 128 KiB");
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        throw Error("Invalid playback-input JSON: " + error.errorString());
    // Keep the existing file-format validator as a compatibility boundary.
    // Only the extracted validator runs; playback and rendering use native code.
    QJSEngine engine;
    auto check = [&](QJSValue value) {
        if (value.isError() || engine.hasError()) {
            if (engine.hasError()) value = engine.catchError();
            throw Error("Playback inputs: " + value.property("message").toString());
        }
        return value;
    };
    QFile source(":/legacy/playback-input-schema.js");
    if (!source.open(QIODevice::ReadOnly)) throw Error("Playback-input compatibility schema is missing");
    check(engine.evaluate(QString::fromUtf8(source.readAll()), "playback-input-schema.js"));
    auto validate = check(engine.evaluate(R"JS((function(text,id) {
        try { validateLaunchInputs(JSON.parse(text),id); return ""; }
        catch(error) { return String(error.message || error); }
    }))JS"));
    const auto problem = check(validate.call({QString::fromUtf8(data), assetId})).toString();
    if (!problem.isEmpty()) throw Error(problem);
    ScenePlaybackInputs result; result.json_ = document.object();
    const auto schedule = result.services()["resourceSchedule"].toObject();
    if (!schedule.isEmpty()) {
        const auto controller = schedule["controller"].toObject();
        const auto limit = controller["throughPass"].toInt(-1);
        if (schedule["kind"] != "resident-resource-pass-v1" || limit < 0 || limit > 29999)
            throw Error("Playback input requires a bounded resource controller");
        int previous = -1;
        for (const auto &value : controller["changes"].toArray()) {
            const auto row = value.toObject();
            const int at = row["pass"].toInt(-1);
            if (at <= previous || at > limit || (previous < 0 && at != 0))
                throw Error("Playback controller changes must start at zero and increase");
            for (const auto field : {"actionMask", "directionMask", "historyMask", "dummyMask"}) {
                const double mask = row[field].toDouble(-1);
                if (mask < 0 || mask > 65535 || mask != std::floor(mask))
                    throw Error("Playback controller masks require unsigned halfwords");
            }
            result.controls_.append({at, quint16(row["actionMask"].toInt()),
                quint16(row["directionMask"].toInt()), quint16(row["historyMask"].toInt())});
            previous = at;
        }
        if (result.controls_.isEmpty()) throw Error("Playback controller history is empty");
    }
    return result;
}
QJsonValue ScenePlaybackInputs::known(const QString &name) const {
    const auto group = json_[name].toObject();
    return group["status"] == "known" ? group["value"] : QJsonValue{};
}
QJsonObject ScenePlaybackInputs::services() const {
    const auto external = known("externalProducers");
    return external.isObject() ? external.toObject() : known("capturedResume").toObject()["resourceServices"].toObject();
}
quint16 ScenePlaybackInputs::textSpeed(quint16 fallback) const {
    const auto initial = services()["initialDialogue"].toObject();
    return initial.isEmpty() ? fallback : u16(memory(initial["memory"].toArray(), 0x800e9c0c, 2), 0);
}
bool ScenePlaybackInputs::automaticPages() const {
    return services()["resourceSchedule"].toObject()["pageAdvancePolicy"] == "automatic";
}
int ScenePlaybackInputs::choice(int index) const {
    const auto schedule = services()["resourceSchedule"].toObject();
    const auto policy = schedule["dialogueChoicePolicy"].toString();
    if (policy == "first") return 1;
    if (policy == "second") return 2;
    if (policy == "sequence") {
        const auto choices = schedule["dialogueChoiceSequence"].toArray();
        return index < choices.size() ? choices[index].toInt(1) : 1;
    }
    return 0;
}
std::array<qint16, 3> ScenePlaybackInputs::globalColor() const {
    const auto world = services()["directorLaunch"].toObject()["world"].toObject();
    return {qint16(world["red"].toInt(255)), qint16(world["green"].toInt(255)), qint16(world["blue"].toInt(255))};
}
bool ScenePlaybackInputs::omitBackground() const {
    return services()["framebuffer"].toObject()["backgroundPolicy"] == "omit";
}
int ScenePlaybackInputs::updateLimit() const {
    if (resumeLimit() >= 0) return resumeLimit();
    const auto controller = services()["resourceSchedule"].toObject()["controller"].toObject();
    return controller.isEmpty() ? -1 : controller["throughPass"].toInt() + 1;
}
QString ScenePlaybackInputs::description() const {
    auto result = QString("Playback inputs: %1\nSource: %2 · declared %3")
        .arg(json_["invocationId"].toString(), json_["sourceIdentity"].toString(), json_["evidenceGrade"].toString());
    if (captured() && !known("actorInputRows").isArray()) result += "\nCaptured body artwork uses class-default equipment when army rows are absent.";
    if (captured() && !known("capturedPresentation").isObject()) result += "\nCamera and lighting use editor defaults; they were not captured.";
    return result;
}
int ScenePlaybackInputs::mode(int fallback) const {
    if (captured()) return known("capturedSnapshot").toObject()["sceneMode"].toInt(fallback);
    return known("externalProducers").toObject()["directorLaunch"].toObject()["sceneMode"].toInt(fallback);
}
void ScenePlaybackInputs::configure(ScenePlaybackStart &start, SceneCameras &cameras) const {
    start.mode = mode(start.mode);
    if (known("schedulerBranch").isString()) start.alternate = known("schedulerBranch") == "alternate";
    const auto resume = known("capturedResume").toObject();
    if (!resume.isEmpty()) start.delayedCounter = quint8(resume["tailTimer"].toInt());
    if (!resume.isEmpty() && start.mode == 2) {
        const auto scheduler = resume["nativeScheduler"].toObject();
        const auto expected = resume["entry"] == "normal-mode-two-continuous" ? "native-captured-continuous-v1" : "native-held-movement-v1";
        if (scheduler["kind"] != expected) throw Error("Captured mode-two resume requires native scheduler memory");
        const auto regions = scheduler["memory"].toArray();
        start.sceneFlags = u32(memory(regions, 0x80197b14, 4), 0);
        if (*start.sceneFlags & 8) throw Error("Captured movement requires its proximity helper inputs");
        const auto bank = memory(regions, 0x8022a720, 144);
        const auto scale = cameras.actor().modelScale();
        const auto actor = SceneCamera(bank.first(56), scale);
        cameras = SceneCameras(actor, SceneCamera(bank.mid(88,56), scale), actor);
    }
    const auto launch = known("externalProducers").toObject()["directorLaunch"].toObject();
    if (!launch.isEmpty()) {
        const auto kind = launch["kind"].toString();
        if (!((start.mode == 0 && (kind == "mode-zero-director-v1" || kind == "rom-mode-zero-director-v1")) ||
              (start.mode == 2 && kind == "rom-mode-two-director-v1")))
            throw Error("Playback launch requires a supported Director mode");
        const auto world = launch["world"].toObject();
        for (const auto field : {"mapKind", "scenarioByte", "red", "green", "blue"}) {
            const double value = world[field].toDouble(-1);
            if (value < 0 || value > 255 || value != std::floor(value))
                throw Error("Playback world inputs require unsigned bytes");
        }
        start.presentation = quint8(launch["actorPresentationWord"].toInteger());
        const auto translations = launch["operandTranslations"].toObject();
        for (auto it = translations.begin(); it != translations.end(); ++it) {
            bool ok = false; const int index = it.key().toInt(&ok);
            const auto value = it.value().toDouble(-1);
            if (!ok || index < 0 || index > 255 || value < 0 || value > 65535 || value != std::floor(value))
                throw Error("Invalid playback operand translation");
            start.translations.set(index, quint16(value));
        }
        if (launch.contains("cameraBaseHex")) {
            auto bank = bytes(launch["cameraBaseHex"], 144);
            const float scale = start.mode == 2 ? cameras.actor().modelScale() :
                float(known("externalProducers").toObject()["sharedActor"].toObject()["worldScale"].toDouble(.1));
            // Mode-zero initializer ROM2819EC resets these seven fields in each
            // bank, preserving the caller's aspect, clipping and up vector.
            if (start.mode == 0) {
                for (int base : {0, 88}) {
                    put32(bank, base, std::bit_cast<quint32>(38.f));
                    for (int field = 20; field <= 40; field += 4) put32(bank, base + field, 0);
                    put32(bank, base + (base ? 28 : 20), std::bit_cast<quint32>(360.f));
                }
            } else {
                // The caller's clipping/up fields survive the mode-two reset.
                for (int field : {0, 20, 24, 28, 32, 36, 40})
                    put32(bank, field, u32(cameras.actor().bank(), field));
            }
            // The input contains the screen bank at0 and independent world bank at88.
            const auto actor = SceneCamera(bank.first(56), start.mode == 0 ? 1 : scale);
            const auto registered = SceneCamera(bank.mid(88,56), scale);
            cameras = SceneCameras(actor, registered, start.mode == 0 ? registered : actor);
        }
    }
    const auto presentation = known("capturedPresentation").toObject();
    if (!presentation.isEmpty()) {
        const auto actor = camera(presentation["actorCamera"].toObject());
        const auto registered = camera(presentation["registeredCamera"].toObject());
        cameras = SceneCameras(actor, registered, start.mode == 0 ? registered : actor);
    }
}
void ScenePlaybackInputs::validatePoseRegistry(const Rom &rom, BodyAnimationData::Pointer bodyData) const {
    const auto registry = known("poseRegistry").toObject();
    for (const auto kind : {"ordinary", "alternate"})
        for (const auto &entry : registry[kind].toArray()) {
            const auto row = entry.toObject();
            const auto art = QString::fromLatin1(kind) == "ordinary"
                ? ActorArt::load(rom, row["sourceArt"].toInt())
                : ActorArt::loadBody(rom, row["sourceArt"].toInt(), row["ownerContext"].toInt(),
                                     row["flagA"].toInt(), row["flagB"].toInt(), bodyData);
            for (const auto &value : row["programs"].toArray()) {
                const auto program = value.toObject();
                int state = program["state"].toInt(-1);
                while (state >= 0 && state < art.poses().size() && art.poses()[state].source.isEmpty()) ++state;
                const auto hex = program["programHex"].toString().toLatin1();
                if (state < 0 || state >= art.poses().size() || art.poses()[state].source != QByteArray::fromHex(hex))
                    throw Error("Imported pose registration differs from the current ROM/project animation");
            }
        }
}
void ScenePlaybackInputs::supply(ScenePlayback &playback) const {
    playback.actors().allowCapturedBodyPreview(captured());
    const auto rows = known("actorInputRows").toArray();
    const auto members = known("currentUnitMembers").toArray();
    if (!rows.isEmpty()) {
        ActorBindingInputs binding;
        for (int i = 0; i < 20; ++i) binding.rows[i] = bytes(rows[i], 248);
        binding.unitMembersKnown = !members.isEmpty();
        if (binding.unitMembersKnown) for (int i = 0; i < 5; ++i) binding.unitMembers[i] = quint8(members[i].toInt());
        playback.actors().supplyBindingInputs(std::move(binding));
    }
    const auto actors = (captured() ? known("capturedSnapshot") : known("existingActors")).toObject();
    if (!actors.isEmpty()) {
        const auto entries = actors["slots"].toArray();
        for (int slot = 0; slot < 28; ++slot) {
            playback.actors().vacate(slot);
            playback.actors().supplyMotion(slot, {});
            if (entries[slot].isNull()) continue;
            const auto entry = entries[slot].toObject();
            playback.actors().supplyActor(slot, bytes(entry["recordHex"], 336));
            if (!entry["movementHex"].isNull()) {
                const auto m = bytes(entry["movementHex"], 16);
                if (!std::isfinite(real(m,4))) throw Error("Playback movement velocity must be finite");
                playback.actors().supplyMotion(slot, {real(m,0), real(m,4), real(m,8), u16(m,12), u8(m,14), u8(m,15), 1});
            }
        }
    }
    const auto channels = known("capturedPresentation").toObject()["channels"].toArray();
    const char *fields[]{"rotationX", "rotationY", "translateX", "translateY", "translateZ", "uniformScale"};
    for (int i = 0; i < channels.size(); ++i) if (channels[i].isObject()) {
        auto record = playback.transforms().channel(i);
        for (int f = 0; f < 6; ++f) put32(record, 64 + f * 4, std::bit_cast<quint32>(float(channels[i].toObject()[fields[f]].toDouble())));
        playback.transforms().supplyChannel(i, record);
    }
    const auto external = services();
    const auto initial = external["initialDialogue"].toObject();
    if (!initial.isEmpty()) {
        const int director = external["resourceSchedule"].toObject()["directorSlot"].toInt(-1);
        const auto pool = memory(initial["memory"].toArray(), 0x800e82c8, 6 * 168);
        for (int i = 0; i < 6; ++i)
            if (i != director && (u16(pool, i * 168) & 0x8000))
                throw Error("Imported initial dialogue contains an active inherited owner requiring its payload");
        if (playback.dialogue())
            playback.dialogue()->resources.setContext({1,
                qint32(u32(memory(initial["memory"].toArray(), 0x800c4c20, 4), 0)),
                u16(memory(initial["memory"].toArray(), 0x800c4c26, 2), 0)});
    }
    const auto color = external["initialColor"].toObject();
    if (!color.isEmpty()) playback.fade() = SceneFade(bytes(color["recordHex"], 12), true);
    const auto resume = known("capturedResume").toObject();
    if (!resume.isEmpty()) {
        if (mode(2) == 2) {
            const auto regions = resume["nativeScheduler"].toObject()["memory"].toArray();
            const auto primaryAddress = quint32(resume["primaryOwnerAddress"].toInteger());
            const auto primary = bytes(resume["primaryOwnerHex"], 0x1cb2);
            if (memory(regions, primaryAddress, primary.size()) != primary ||
                u32(memory(regions, 0x8022a974, 4), 0) != primaryAddress ||
                u8(memory(regions, 0x8018fc19, 1), 0) != 2 ||
                u32(memory(regions, 0x8022a950, 4), 0) != actors["observedParserCursor"].toInteger())
                throw Error("Captured scheduler memory disagrees with the declared scene");
            const auto entries = actors["slots"].toArray();
            for (int i = 0; i < 28; ++i) if (!entries[i].isNull()) {
                const auto entry = entries[i].toObject();
                if (memory(regions, u32(primary, 24 + 4*i), 336) != bytes(entry["recordHex"], 336))
                    throw Error("Captured scheduler Actor disagrees with its imported record");
                if (!entry["movementHex"].isNull() && memory(regions, u32(primary, 0xf8 + 4*i), 16) != bytes(entry["movementHex"], 16))
                    throw Error("Captured scheduler movement disagrees with its imported record");
            }
            for (int i = 0; i < 30; ++i) if (u32(primary, 0x1ac0 + 4*i))
                throw Error("Captured scheduler requires its active secondary sprite records");
            const auto projection = u32(primary, 0x19f0);
            if (projection) playback.projection() = SceneProjection(memory(regions, projection, 76));
            if (resume["entry"] == "normal-mode-two-continuous") {
                std::array<quint32, 4> gains{};
                for (int i = 0; i < 4; ++i) gains[i] = u32(memory(regions, 0x800eb2e0 + 8*i, 4), 0);
                playback.supplyAudio(SceneAudio({0, {}, 0, 1}, gains));
            }
        }
        const auto query = playback.director().restoreMovementQuery(actors["observedParserCursor"].toInt(-1));
        const int slot = qint32(query.operands[2]);
        if (slot < 0 || slot >= 28 || !playback.actors().actor(slot))
            throw Error("Captured movement query requires its Actor");
        const auto &motion = playback.actors().motion(slot);
        if (!motion.present || motion.pause || qint16(motion.remaining) <= 1)
            throw Error("Captured movement query requires an advancing movement owner");
        const bool continuous = resume["entry"].toString().endsWith("continuous");
        if (!continuous) {
            const int remaining = qint16(motion.remaining) - resumeLimit();
            const int target = qint32(query.operands[1]), comparison = qint32(query.operands[0]);
            const bool passes = comparison == 0 ? remaining == target : comparison == 1 ? remaining != target :
                                comparison == 2 ? remaining >= target : comparison == 3 ? remaining <= target :
                                comparison == 4 ? remaining > target : comparison == 5 && remaining < target;
            if (remaining <= 0 || passes)
                throw Error("Captured movement must remain blocked throughout its declared update window");
        }
        // Captured profiles do not provide a terrain service. Preserve the
        // original browser boundary instead of using the editor's flat terrain.
        for (int i = 0; i < 28; ++i)
            if (playback.actors().actor(i) && playback.actors().motion(i).present &&
                (u8(playback.actors().actor(i)->record(), 0x145) & 1))
                throw Error("Captured movement requires its terrain input");
    }
}
std::optional<quint16> ScenePlaybackInputs::controller(int update) const {
    const auto found = control(update);
    return found ? std::optional<quint16>(found->action) : std::nullopt;
}
const ScenePlaybackInputs::Control *ScenePlaybackInputs::control(int update) const {
    const auto found = std::upper_bound(controls_.cbegin(), controls_.cend(), update,
        [](int at, const Control &row) { return at < row.update; });
    return found == controls_.cbegin() ? nullptr : &*std::prev(found);
}
quint16 ScenePlaybackInputs::direction(int update) const {
    const auto found = control(update); return found ? found->direction : 0;
}
quint16 ScenePlaybackInputs::history(int update) const {
    const auto found = control(update); return found ? found->history : 0;
}
QString ScenePlaybackInputs::boundary() const {
    if (captured() && resumeLimit() < 0) return "Captured Actor snapshot: no elapsed updates. Parser cursor is an observation; resume state is unknown.";
    QStringList unsupported;
    for (const auto name : {"bodyPoseSetups", "subordinateServices", "rosterConstruction", "rosterResets", "rosterStateServices", "directActorCreates"})
        if (!known(name).isNull() && !known(name).isUndefined()) unsupported << QString::fromLatin1(name);
    const auto external = services();
    for (const auto name : {"menuCreates", "colorCreates", "poseCalls", "dialogueCreates"})
        if (!external[name].toArray().isEmpty()) unsupported << "externalProducers." + QString::fromLatin1(name);
    const auto events = external["events"];
    if (!events.toArray().isEmpty() || !events.toObject()["sequence"].toArray().isEmpty())
        unsupported << "externalProducers.events";
    if (!unsupported.isEmpty()) return "Imported callback outcomes require native adapters: " + unsupported.join(", ");
    return {};
}
} // namespace lordly
