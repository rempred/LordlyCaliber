#include "engine/attack_audio.h"
#include "engine/sound_parts.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QtEndian>
#include <cmath>
namespace lordly {
namespace {
int integer(const QJsonObject &o, const char *key, int fallback) {
    if (!o.contains(key))
        return fallback;
    auto v = o[key];
    if (!v.isDouble() || !std::isfinite(v.toDouble()) || v.toDouble() != v.toInt())
        throw Error("Invalid attack sound value");
    return v.toInt();
}
} // namespace
void AttackSound::validate() const {
    if (anchor < -1 || anchor > 181 || delay < 0 || delay > 900 || cue < -1 || cue > 181 ||
        request < 0 || request >= 740 || name.size() > 120 || pcm.size() > 128000 || pcm.size() % 2)
        throw Error("Sound cues support 0–900 ticks and WAV clips up to 2 seconds");
    validateSoundParts(*this);
}
QJsonObject AttackSound::json() const {
    validate();
    QJsonObject result{{"anchor", anchor},   {"delay", delay}, {"cue", cue},
            {"request", request}, {"name", name},   {"pcm", QString::fromLatin1(pcm.toBase64())}};
    if (!state.isEmpty()) result["state"] = QString::fromLatin1(state.toBase64());
    if (!enemyState.isEmpty()) result["enemyState"] = QString::fromLatin1(enemyState.toBase64());
    if (!sequence.isEmpty()) result["sequence"] = QString::fromLatin1(sequence.toBase64());
    if (enemyRequest >= 0) result["enemyRequest"] = enemyRequest;
    if (!parts.isEmpty()) result["parts"] = parts;
    return result;
}
AttackSound AttackSound::fromJson(const QJsonObject &o) {
    AttackSound s;
    s.anchor = integer(o, "anchor", -1);
    s.delay = integer(o, "delay", 0);
    s.cue = integer(o, "cue", -1);
    s.request = integer(o, "request", 1);
    s.name = o["name"].toString();
    if (!o["pcm"].isString())
        throw Error("Invalid imported sound data");
    const auto decoded = QByteArray::fromBase64Encoding(o["pcm"].toString().toLatin1(),
                                                        QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded)
        throw Error("Invalid imported sound encoding");
    s.pcm = decoded.decoded;
    for (const auto &field : {"state", "enemyState", "sequence"}) if (o.contains(field)) {
        if (!o[field].isString()) throw Error("Invalid native sound source");
        const auto data = QByteArray::fromBase64Encoding(o[field].toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
        if (!data) throw Error("Invalid native sound source encoding");
        if (QString(field) == "state") s.state = data.decoded;
        else if (QString(field) == "enemyState") s.enemyState = data.decoded;
        else s.sequence = data.decoded;
    }
    s.enemyRequest = integer(o, "enemyRequest", -1);
    if (o.contains("parts")) {
        if (!o["parts"].isArray()) throw Error("Invalid sound parts");
        s.parts = o["parts"].toArray();
    }
    s.validate();
    return s;
}
AttackSound AttackSound::importWav(const QByteArray &b, const QString &name) {
    const QString format = "Use a PCM WAV: 32,000 Hz, mono, signed 16-bit, at most 2 seconds.";
    auto le16 = [&](qsizetype p) { return qFromLittleEndian<quint16>(b.constData() + p); };
    auto le32 = [&](qsizetype p) { return qFromLittleEndian<quint32>(b.constData() + p); };
    if (b.size() < 12 || b.first(4) != "RIFF" || b.mid(8, 4) != "WAVE" ||
        quint64(le32(4)) + 8 != quint64(b.size()))
        throw Error(format);
    bool fmt = false, data = false;
    AttackSound s;
    s.name = name.left(120);
    for (qsizetype p = 12; p < b.size();) {
        if (p + 8 > b.size())
            throw Error(format);
        auto n = le32(p + 4);
        auto id = b.mid(p, 4);
        p += 8;
        if (quint64(p) + n + (n & 1) > quint64(b.size()))
            throw Error(format);
        if (id == "fmt ") {
            if (fmt || n < 16 || le16(p) != 1 || le16(p + 2) != 1 || le32(p + 4) != 32000 ||
                le32(p + 8) != 64000 || le16(p + 12) != 2 || le16(p + 14) != 16)
                throw Error(format);
            fmt = true;
        } else if (id == "data") {
            if (data || !n || n > 128000 || n % 2)
                throw Error(format);
            s.pcm = b.mid(p, n);
            data = true;
        }
        p += n + (n & 1);
    }
    if (!fmt || !data)
        throw Error(format);
    s.validate();
    return s;
}
QByteArray AttackSound::bank(quint32 romOffset) const {
    validate();
    if (hasParts()) return buildSoundSource(*this, romOffset);
    if (pcm.isEmpty())
        return {};
    // One independent PtrTablesV2/raw16 wave and one libmus component. The
    // native sample DMA reads PCM from ROM; metadata remains in permanent RAM.
    QByteArray b(256, 0);
    b.replace(0, 15, "N64 PtrTablesV2");
    put32(b, 32, 1);
    put32(b, 36, 0x34);
    put32(b, 40, 0x38);
    put32(b, 44, 0x30);
    put32(b, 0x30, 0x40); // Base-note 0 + note48 gives unity pitch in native libmus.
    put32(b, 0x40, romOffset + 256);
    put32(b, 0x44, pcm.size());
    put8(b, 0x48, 1);
    put32(b, 0x80, 1);
    put32(b, 0x84, 1);
    put32(b, 0x88, 1);
    put32(b, 0x94, 0x30);
    put32(b, 0x98, 0x40);
    put32(b, 0x9c, 100);
    QByteArray stream = QByteArray::fromHex("810084017f017f017fff a67f8578 30");
    const int length = (pcm.size() / 2 * 96 + 31999) / 32000 + 2;
    if (length >= 128)
        stream += char(0x80 | (length >> 8));
    stream += char(length);
    stream += char(0x80);
    b.replace(0xc0, stream.size(), stream);
    for (int i = 0; i < pcm.size(); i += 2) {
        b += pcm[i + 1];
        b += pcm[i];
    }
    b += QByteArray(32, 0); // DMA alignment/tail padding belongs to this allocation.
    b += QByteArray((16 - b.size() % 16) % 16, 0);
    return b;
}
void AttackAudio::validate() const {
    if (sounds.size() > 64 || (!enabled && !sounds.isEmpty()))
        throw Error("Invalid attack sound list");
    int bytes = 0;
    for (const auto &s : sounds) {
        s.validate();
        bytes += soundImportBytes(s);
    }
    if (bytes > 256000)
        throw Error("An attack may contain up to 256 KB of imported sound");
}
QJsonObject AttackAudio::json() const {
    validate();
    QJsonArray a;
    for (const auto &s : sounds)
        a.append(s.json());
    return {{"enabled", enabled}, {"sounds", a}};
}
AttackAudio AttackAudio::fromJson(const QJsonObject &o) {
    if (!o["enabled"].isBool() || !o["sounds"].isArray())
        throw Error("Invalid attack audio");
    AttackAudio a;
    a.enabled = o["enabled"].toBool();
    for (const auto &s : o["sounds"].toArray()) {
        if (!s.isObject())
            throw Error("Invalid attack sound");
        a.sounds.append(AttackSound::fromJson(s.toObject()));
    }
    a.validate();
    return a;
}
QByteArray AttackAudio::build(quint32 romOffset) const {
    validate();
    QByteArray b(32 + sounds.size() * 16, 0);
    b.replace(0, 8, "LCAUDIO1");
    put32(b, 8, sounds.size());
    put32(b, 12, romOffset);
    for (int i = 0; i < sounds.size(); ++i) {
        const auto &s = sounds[i];
        int r = 32 + i * 16;
        put16(b, r, quint16(s.anchor));
        put16(b, r + 2, s.delay);
        put16(b, r + 4, quint16(s.cue));
        put16(b, r + 6, s.request);
        if (!s.pcm.isEmpty() || s.hasParts()) {
            put32(b, r + 8, b.size());
            b += s.bank(romOffset + b.size());
        }
    }
    auto metadata = qCompress(QJsonDocument(json()).toJson(QJsonDocument::Compact), 9);
    put32(b, 16, b.size());
    put32(b, 20, metadata.size());
    b += metadata;
    b += QByteArray((16 - b.size() % 16) % 16, 0);
    put32(b, 24, b.size());
    return b;
}
} // namespace lordly
