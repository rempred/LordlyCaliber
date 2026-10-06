#include "engine/scene_playback_inputs.h"
#include <QFile>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <bit>
#include <cmath>
namespace lordly {
namespace {
QByteArray bytes(const QJsonValue &v, int size) {
    const auto hex = v.toString().toLatin1(), data = QByteArray::fromHex(hex);
    if (!v.isString() || data.size() != size || data.toHex() != hex.toLower())
        throw Error("Playback input has an incomplete byte record");
    return data;
}
float real(const QByteArray &data, int at) { return std::bit_cast<float>(u32(data, at)); }
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
    return result;
}
QJsonValue ScenePlaybackInputs::known(const QString &name) const {
    const auto group = json_[name].toObject();
    return group["status"] == "known" ? group["value"] : QJsonValue{};
}
QString ScenePlaybackInputs::description() const {
    return QString("Playback inputs: %1\nSource: %2 · declared %3")
        .arg(json_["invocationId"].toString(), json_["sourceIdentity"].toString(), json_["evidenceGrade"].toString());
}
int ScenePlaybackInputs::mode(int fallback) const {
    if (captured()) return known("capturedSnapshot").toObject()["sceneMode"].toInt(fallback);
    return known("externalProducers").toObject()["directorLaunch"].toObject()["sceneMode"].toInt(fallback);
}
void ScenePlaybackInputs::configure(ScenePlaybackStart &start, SceneCameras &cameras) const {
    start.mode = mode(start.mode);
    if (known("schedulerBranch").isString()) start.alternate = known("schedulerBranch") == "alternate";
    const auto launch = known("externalProducers").toObject()["directorLaunch"].toObject();
    if (!launch.isEmpty()) {
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
            const auto bank = bytes(launch["cameraBaseHex"], 144);
            const float scale = float(known("externalProducers").toObject()["sharedActor"].toObject()["worldScale"].toDouble(.1));
            // The input contains the screen bank at0 and independent world bank at88.
            cameras = SceneCameras(SceneCamera(bank.first(56), 1), SceneCamera(bank.mid(88,56), scale), SceneCamera(bank.mid(88,56), scale));
        }
    }
    const auto presentation = known("capturedPresentation").toObject();
    if (!presentation.isEmpty()) {
        const auto actor = camera(presentation["actorCamera"].toObject());
        const auto registered = camera(presentation["registeredCamera"].toObject());
        cameras = SceneCameras(actor, registered, start.mode == 0 ? registered : actor);
    }
}
void ScenePlaybackInputs::supply(ScenePlayback &playback) const {
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
    const auto color = known("externalProducers").toObject()["initialColor"].toObject();
    if (!color.isEmpty()) playback.fade() = SceneFade(bytes(color["recordHex"], 12), true);
}
std::optional<quint16> ScenePlaybackInputs::controller(int update) const {
    const auto controller = known("externalProducers").toObject()["resourceSchedule"].toObject()["controller"].toObject();
    if (controller.isEmpty() || update > controller["throughPass"].toInt(-1)) return {};
    quint16 result = 0;
    for (const auto &entry : controller["changes"].toArray()) {
        const auto row = entry.toObject();
        if (row["pass"].toInt() > update) break;
        result = quint16(row["actionMask"].toInt() | row["directionMask"].toInt());
    }
    return result;
}
QString ScenePlaybackInputs::boundary() const {
    if (captured()) return "Captured Actor snapshot: no elapsed updates. Parser cursor is an observation; resume inputs have not been applied.";
    QStringList unsupported;
    for (const auto name : {"poseRegistry", "bodyPoseSetups", "subordinateServices", "rosterConstruction", "rosterResets", "rosterStateServices", "directActorCreates"})
        if (!known(name).isNull() && !known(name).isUndefined()) unsupported << QString::fromLatin1(name);
    const auto external = known("externalProducers").toObject();
    for (const auto name : {"initialDialogue", "directorLaunch"})
        if (external.contains(name)) unsupported << "externalProducers." + QString::fromLatin1(name);
    for (const auto name : {"menuCreates", "colorCreates", "poseCalls", "dialogueCreates"})
        if (!external[name].toArray().isEmpty()) unsupported << "externalProducers." + QString::fromLatin1(name);
    const auto events = external["events"];
    if (!events.toArray().isEmpty() || !events.toObject()["sequence"].toArray().isEmpty())
        unsupported << "externalProducers.events";
    if (!unsupported.isEmpty()) return "Imported callback outcomes require native adapters: " + unsupported.join(", ");
    return {};
}
} // namespace lordly
