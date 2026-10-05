#include "engine/actor_art.h"
#include "engine/project.h"
#include <QJsonArray>
#include <algorithm>
#include <cmath>
namespace lordly {
namespace {
int integer(const QJsonValue &value, int low, int high) {
    const double n = value.toDouble(double(low) - 1);
    if (!value.isDouble() || !std::isfinite(n) || std::floor(n) != n || n < low || n > high)
        throw Error("Invalid imported animation integer");
    return int(n);
}
QByteArray data(const QJsonValue &value, int size = -1) {
    const auto text = value.toString().toLatin1();
    const auto decoded = QByteArray::fromBase64(text, QByteArray::AbortOnBase64DecodingErrors);
    if (!value.isString() || text.size() > 24 * 1024 * 1024 || decoded.toBase64() != text ||
        (size >= 0 && decoded.size() != size))
        throw Error("Invalid imported animation bytes");
    return decoded;
}
int ordinal(const QString &text) {
    bool ok = false;
    const int n = text.toInt(&ok);
    if (!ok || n < 0 || n > 65535 || QString::number(n) != text)
        throw Error("Invalid imported animation source identity");
    return n;
}
QJsonObject pair(const QJsonValue &value) {
    if (!value.isObject())
        throw Error("Missing imported animation selector pair");
    const auto row = value.toObject();
    return {{"normalSelector", integer(row["normalSelector"], 0, 255)},
            {"blockedSelector", integer(row["blockedSelector"], 0, 255)}};
}
struct Reference {
    int id, flags, selector;
    AnimationDescriptor descriptor;
    bool idle;
};
Reference reference(const Rom &rom, const QJsonValue &value, BodyAnimationData::Pointer body) {
    if (!value.isObject())
        throw Error("Missing imported animation reference");
    const auto row = value.toObject();
    const int id = integer(row["classId"], 1, int(body->classes().entries().size()) - 1),
              flags = integer(row["bodyFlags"], 0, 3),
              copy = u8(body->classes().entries()[id].record, 69), owner = copy ? copy : id;
    const auto art = ActorArt::loadBody(rom, id, owner, flags / 2, flags % 2, body);
    if (row["descriptorKey"].toDouble(-1) != double(art.descriptorKey()) ||
        integer(row["selectedBodyChild"], 0, 15) != art.bodyVariant())
        throw Error("Imported animation reference differs from the current ROM body route");
    const int selector = integer(row["selector"], 0, int(art.poses().size()) - 1);
    if (art.poses()[selector].records.isEmpty())
        throw Error("Imported animation reference selects an empty pose");
    integer(row["rawMode"], 0, 2);
    if (!row["idleSequence"].isBool())
        throw Error("Invalid imported idle reference");
    return {id, flags, selector, AnimationDescriptor::load(rom, art.descriptorKey()),
            row["idleSequence"].toBool()};
}
QVector<PoseRecord> donorRecords(const Reference &donor, bool idle) {
    auto records = donor.descriptor.poses().effective(donor.selector).records;
    if (idle && !donor.idle)
        throw Error("An imported idle needs a looping donor");
    if (donor.idle) {
        if (records.isEmpty() || records.last().opcode != 4)
            throw Error("Imported idle donor has no loop");
        const int start = u8(records.last().operands, 0);
        if (start >= records.size() || records[start].opcode != 1)
            throw Error("Imported idle donor has an invalid loop");
        records = records.mid(start);
        if (idle)
            records.last().operands[0] = 0;
        else
            records.removeLast();
    }
    return records;
}
QVector<PoseRecord> program(const QJsonObject &entry, const Reference &donor) {
    const bool idle = entry["laneKey"] == "idle";
    const auto expected = donorRecords(donor, idle);
    auto records = PoseBank::parseProgram(data(entry["poseProgramBase64"])).records;
    if (idle) {
        if (records.isEmpty() || records.last().opcode != 4)
            throw Error("Imported idle has no loop");
        const int start = u8(records.last().operands, 0);
        if (start >= records.size() || records[start].opcode != 1)
            throw Error("Imported idle has an invalid loop");
        for (int i = 0; i < start; ++i)
            if (records[i].opcode != 1)
                throw Error("Imported idle startup contains unsupported controls");
        records = records.mid(start);
        records.last().operands[0] = 0;
    }
    QVector<PoseRecord> controls;
    for (const auto &r : expected)
        if (r.opcode != 1)
            controls << r;
    int control = 0, frame = 0;
    const auto frames = entry["frames"].toArray();
    for (const auto &r : records) {
        if (r.opcode == 1) {
            if (frame >= frames.size())
                throw Error("Imported pose has more frames than its timeline");
            const auto row = frames[frame++].toObject();
            if (u8(r.operands, 0) != integer(row["token"], 0, 255) ||
                u8(r.operands, 1) != integer(row["ticks"], 0, 255))
                throw Error("Imported frame timing differs from its program");
        } else {
            if (control >= controls.size() || r.opcode != controls[control].opcode ||
                (r.opcode != 4 && r.operands != controls[control].operands))
                throw Error("Imported pose changed a preserved control instruction");
            if (r.opcode == 4) {
                const int target = u8(r.operands, 0);
                if (target >= records.size() || records[target].opcode != 1)
                    throw Error("Imported pose jump does not target a frame");
            }
            ++control;
        }
    }
    if (control != controls.size() || frame != frames.size() || !frame)
        throw Error("Imported pose does not match its frames and controls");
    return records;
}
struct Source {
    int art, width, height, children, format;
};
QMap<int, Source> sources(AnimationDescriptor &descriptor, const QJsonValue &value) {
    if (!value.isObject() || value.toObject().isEmpty())
        throw Error("Imported sequence has no sprite sources");
    QMap<int, Source> result;
    const auto rows = value.toObject();
    for (auto it = rows.begin(); it != rows.end(); ++it) {
        const int index = ordinal(it.key());
        const auto row = it.value().toObject();
        const auto original = SpriteObject::decode(data(row["decodedBase64"]));
        const int count = int(original.children().size()),
                  pixels = original.width() * original.height();
        const bool editable = original.colorFormat() == 1 && original.alphaFormat() == 0;
        const QString format = original.colorFormat() == 2 ? "direct-rgba5551"
                               : editable                  ? "indexed-ci8"
                                                           : "indexed-ci8-alpha8";
        if ((original.colorFormat() != 1 && original.colorFormat() != 2) ||
            integer(row["width"], 1, 65535) != original.width() ||
            integer(row["height"], 1, 65535) != original.height() ||
            integer(row["childCount"], 1, 255) != count || row["formatKind"] != format ||
            !row["editable"].isBool() || row["editable"].toBool() != editable ||
            !row["children"].isObject())
            throw Error("Imported sprite structure differs from its metadata");
        integer(row["childOrdinal"], 0, count - 1);
        const auto children = row["children"].toObject();
        if (children.size() != (editable ? count : 0))
            throw Error("Imported sprite is missing child pixels");
        QMap<int, SpritePixels> edits;
        for (int child = 0; editable && child < count; ++child) {
            const auto c = children[QString::number(child)].toObject();
            edits[child] = {data(c["ci8IndicesBase64"], pixels),
                            data(c["i4IntensityBase64"], pixels)};
        }
        const auto bytes = editable ? original.rebuild(edits) : original.source();
        const int art = descriptor.appendSprite(bytes, integer(row["selectorPolicy"], 0, 2),
                                                data(row["paletteRgba5551BeBase64"], 512));
        result[index] = {art, original.width(), original.height(), count, original.colorFormat()};
    }
    return result;
}
QVector<int> frames(AnimationDescriptor &descriptor, const QJsonValue &value,
                    const QMap<int, Source> &sources) {
    if (!value.isArray() || value.toArray().isEmpty())
        throw Error("Imported sequence has no frames");
    QVector<int> result;
    QMap<QByteArray, int> unique;
    QSet<int> identities, used;
    for (const auto &v : value.toArray()) {
        const auto row = v.toObject();
        const int identity = integer(row["sourceFrameIndex"], 0, 65535);
        if (integer(row["sequenceIndex"], 0, 65535) != result.size() ||
            identities.contains(identity) || !row["layers"].isArray())
            throw Error("Imported animation repeats or misorders a frame identity");
        identities.insert(identity);
        const auto layers = row["layers"].toArray();
        if (layers.isEmpty() || layers.size() > 4096)
            throw Error("Imported frame layer count exceeds its format");
        QByteArray bytes(2 + layers.size() * 16, 0);
        put16(bytes, 0, quint16(layers.size()));
        for (int i = 0; i < layers.size(); ++i) {
            const auto layer = layers[i].toObject();
            const int source = integer(layer["sourceOrdinal"], 0, 65535);
            if (!sources.contains(source))
                throw Error("Imported frame references an absent sprite");
            const auto &s = sources[source];
            used.insert(source);
            const int width = integer(layer["width"], 1, 65535),
                      height = integer(layer["height"], 1, 65535);
            if (height != s.height || width > s.width || s.width - width > (s.format == 2 ? 3 : 0))
                throw Error("Imported frame dimensions differ from its sprite");
            integer(layer["selectedChildOrdinal"], 0, s.children - 1);
            integer(layer["requestedChildOrdinal"], 0, 65535);
            const int at = 2 + i * 16;
            put16(bytes, at, quint16(s.art));
            put16(bytes, at + 2, quint16(integer(layer["drawOffsetX"], -32768, 32767)));
            put16(bytes, at + 4, quint16(integer(layer["drawOffsetY"], -32768, 32767)));
            put16(bytes, at + 6, quint16(width));
            put16(bytes, at + 8, quint16(height));
            put16(bytes, at + 10, quint16(integer(layer["flags"], 0, 65535)));
            put16(bytes, at + 12, quint16(integer(layer["scaleXRaw"], 0, 65535)));
            put16(bytes, at + 14, quint16(integer(layer["scaleYRaw"], 0, 65535)));
        }
        if (!unique.contains(bytes))
            unique[bytes] = descriptor.appendFrame(bytes);
        result << unique[bytes];
    }
    if (used.size() != sources.size())
        throw Error("Imported sequence contains unused sprite sources");
    return result;
}
struct Row {
    QString key, lane;
    int id, flags, action;
    QJsonObject entry;
    Reference target, donor;
};
QByteArray remapFrames(QVector<PoseRecord> records, const QVector<int> &tokens) {
    int index = 0;
    for (auto &r : records)
        if (r.opcode == 1) {
            const int token = tokens[index++], ticks = u8(r.operands, 1);
            r.opcode = token > 255 ? 21 : 1;
            r.operands = QByteArray(1, char(token));
            if (token > 255)
                r.operands += char(token >> 8);
            r.operands += char(ticks);
        }
    return PoseBank::encodeProgram(records);
}
} // namespace
void Project::importLegacyAnimationSequences(const QJsonObject &object) {
    const QJsonValue payload = object["patches"].toObject()["art"].toObject()["separations"];
    if (payload.isNull() || payload.isUndefined())
        return;
    if (!payload.isObject() || payload.toObject()["schemaVersion"] != 4 ||
        !payload.toObject()["entries"].isObject() ||
        !payload.toObject()["routeBaselines"].isObject())
        throw Error("Unsupported browser private-animation document");
    loadAnimationRoutes();
    loadAnimationOverrides();
    auto sequences = *animationSequences_;
    auto overrides = *animationOverrides_;
    const auto classes = classData();
    const auto body = std::make_shared<const BodyAnimationData>(classes, *animationRoutes_);
    const auto input = payload.toObject(), entries = input["entries"].toObject(),
               baselines = input["routeBaselines"].toObject();
    const QMap<QString, QPair<int, int>> fixed{
        {"idle", {-1, 0}}, {"advance", {-2, 5}}, {"return", {-3, 11}}, {"hit", {-4, 17}}};
    QMap<int, QVector<Row>> groups;
    QSet<QString> required;
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        const auto e = it.value().toObject();
        const auto lane = e["laneKey"].toString();
        const int id = integer(e["classId"], 1, int(classes.entries().size()) - 1),
                  flags = integer(e["bodyFlags"], 0, 3), action = integer(e["actionId"], -4, 255);
        if (it.key() != QString("%1:%2:%3:%4").arg(id).arg(action).arg(flags).arg(lane) ||
            (fixed.contains(lane) ? action != fixed[lane].first
                                  : (action < 0 || (lane != "normal" && lane != "blocked"))))
            throw Error("Imported sequence route identity is invalid");
        auto target = reference(rom_, e["targetRef"], body),
             donor = reference(rom_, e["donorRef"], body);
        if (target.id != id || target.flags != flags ||
            e["targetRef"].toObject()["actionId"] != action ||
            (fixed.contains(lane) && target.selector != fixed[lane].second) ||
            (!fixed.contains(lane) &&
             ((e["targetRef"].toObject()["rawMode"] == 2) != (lane == "blocked"))))
            throw Error("Imported sequence target differs from its route");
        groups[id * 4 + flags] << Row{
            it.key(), lane, id, flags, action, e, std::move(target), std::move(donor)};
        if (action >= 0)
            required.insert(QString("%1:%2:%3").arg(id).arg(action).arg(flags));
    }
    const auto baselineKeys = baselines.keys();
    if (required != QSet<QString>(baselineKeys.begin(), baselineKeys.end()))
        throw Error("Imported attack baselines do not match their routes");
    QMap<int, QMap<int, int>> selectorMaps;
    QMap<QString, int> assigned;
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        auto &rows = it.value();
        std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) {
            return a.action != b.action ? a.action < b.action
                                        : a.lane == "normal" && b.lane != "normal";
        });
        const int id = it.key() / 4, flags = it.key() % 4;
        sequences.remove(id, flags);
        sequences.create(id, flags, *animationRoutes_,
                         combat_ ? combat_->previewEdits() : ActorArtworkEdits{});
        auto descriptor = *sequences.find(id, flags);
        const auto original = descriptor.poses();
        auto bank = original;
        const int copy = u8(classes.entries()[id].record, 69), owner = copy ? copy : id;
        const bool linked = owner == 10 || owner == 25 || owner == 123;
        int appended = 0, start = 0;
        QMap<int, int> pairs;
        QByteArray idle;
        for (const auto &row : rows) {
            if (row.target.descriptor.key() != descriptor.key())
                throw Error("Imported group targets conflicting descriptors");
            const auto controls = program(row.entry, row.donor);
            const auto sprites = sources(descriptor, row.entry["sources"]);
            const auto tokens = frames(descriptor, row.entry["frames"], sprites);
            const auto bytes = remapFrames(controls, tokens);
            if (fixed.contains(row.lane)) {
                bank.setProgram(fixed[row.lane].second, bytes);
                if (row.lane == "idle")
                    idle = bytes;
                continue;
            }
            const int oldSelector = int(original.programs().size()) + appended++;
            int selected;
            if (start) {
                selected = start;
                while (selected < 256 && pairs.contains(selected))
                    ++selected;
                if (selected == 256)
                    throw Error("Imported attack selectors exceed their one-byte range");
                bank.setProgram(selected, bytes);
            } else {
                while (bank.programs().size() < 100)
                    bank.appendProgram(QByteArray(1, 0));
                if (bank.programs().size() >= 256)
                    throw Error("Imported attack selectors exceed their one-byte range");
                selected = bank.appendProgram(bytes);
            }
            if (linked) {
                const int previous = row.target.selector + 50;
                auto secondary = original.effective(previous).records;
                if (secondary.isEmpty())
                    throw Error("Imported linked attack has no secondary program");
                if (!start)
                    start = selected + 1;
                while (bank.programs().size() < 256)
                    bank.appendProgram(QByteArray(1, 0));
                const int next = int(bank.programs().size());
                for (auto &r : secondary)
                    if (r.opcode == 5 && u8(r.operands, 0) == previous) {
                        r.opcode = 22;
                        r.operands = QByteArray(1, char(next)) + QByteArray(1, char(next >> 8));
                    }
                pairs[selected] = bank.appendProgram(PoseBank::encodeProgram(secondary));
            } else
                pairs[selected] = -1;
            selectorMaps[it.key()][oldSelector] = selected;
            assigned[row.key] = selected;
        }
        if (!idle.isEmpty()) {
            for (int state : {1, 2, 3, 4, 8})
                bank.setProgram(state, idle);
            const auto idleRecords = PoseBank::parseProgram(idle).records;
            auto first = std::find_if(idleRecords.begin(), idleRecords.end(), [](const auto &r) {
                return r.opcode == 1 || r.opcode == 21;
            });
            auto recovery = bank.effective(18).records;
            for (auto &r : recovery)
                if (r.opcode == 1 || r.opcode == 21) {
                    const char ticks = r.operands.back();
                    r.opcode = first->opcode;
                    r.operands = first->operands;
                    r.operands.back() = ticks;
                }
            bank.setProgram(18, PoseBank::encodeProgram(recovery));
        }
        descriptor.setPoses(bank);
        sequences.setDescriptor(id, flags, descriptor);
        sequences.setPairedPoses(id, flags, pairs, start);
    }
    const auto remapPair = [&](QJsonObject p, int group) {
        for (const auto *key : {"normalSelector", "blockedSelector"}) {
            const int old = integer(p[key], 0, 255);
            p[key] = selectorMaps[group].value(old, old);
        }
        return p;
    };
    auto rows = overrides.entries();
    for (int i = 0; i < rows.size(); ++i) {
        auto row = rows[i].toObject();
        if (row.contains("bodyFlags"))
            row = remapPair(row, row["classId"].toInt() * 4 + row["bodyFlags"].toInt());
        rows[i] = row;
    }
    for (const auto &key : required) {
        const auto parts = key.split(':');
        const int id = parts[0].toInt(), action = parts[1].toInt(), flags = parts[2].toInt(),
                  group = id * 4 + flags;
        const auto baseline = baselines[key].toObject();
        auto prior = baseline["entry"];
        auto p = remapPair(pair(baseline["pair"]), group);
        if (!prior.isNull()) {
            const auto e = prior.toObject();
            if (e["classId"] != id || e["actionId"] != action || e["bodyFlags"] != flags)
                throw Error("Imported prior assignment targets a different route");
            prior = remapPair(pair(e), group);
            p = prior.toObject();
        }
        sequences.rememberAttackBaseline(id, flags, action, prior);
        for (const auto &lane : {QString("normal"), QString("blocked")}) {
            const auto route = key + ':' + lane;
            if (assigned.contains(route))
                p[lane == "normal" ? "normalSelector" : "blockedSelector"] = assigned[route];
        }
        QJsonArray retained;
        for (const auto &v : rows) {
            const auto r = v.toObject();
            if (r["classId"] != id || r["actionId"] != action || !r.contains("bodyFlags") ||
                r["bodyFlags"] != flags)
                retained << v;
        }
        p["classId"] = id;
        p["actionId"] = action;
        p["bodyFlags"] = flags;
        retained << p;
        rows = retained;
    }
    overrides.setEntries(rows, rom_, classes, *animationRoutes_);
    setPrivateAnimationState(sequences.json(), overrides.json());
    legacyAnimationSequencesImported_ = true;
}
} // namespace lordly
