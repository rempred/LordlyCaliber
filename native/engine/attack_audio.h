#pragma once
#include "engine/rom.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
namespace lordly {
struct AttackSound {
    // -1: effect start; otherwise native spatial cue. Keep impact-relative timing.
    int anchor = -1, delay = 0, cue = -1, request = 1;
    QString name;
    QByteArray pcm; // Imported WAV: 32 kHz, mono, signed 16-bit little endian.
    // Private native note snapshots and edits. The original sound/sample banks
    // remain unchanged; nested replacement sources are copied by value in JSON.
    QByteArray state, enemyState, sequence;
    int enemyRequest = -1;
    QJsonArray parts;
    bool hasParts() const { return !state.isEmpty() || !parts.isEmpty(); }
    bool operator==(const AttackSound &) const = default;
    void validate() const;
    QJsonObject json() const;
    static AttackSound fromJson(const QJsonObject &);
    static AttackSound importWav(const QByteArray &, const QString &name);
    QByteArray bank(quint32 romOffset = 0) const;
};
struct AttackAudio {
    bool enabled = false;
    QVector<AttackSound> sounds;
    bool operator==(const AttackAudio &) const = default;
    void validate() const;
    QJsonObject json() const;
    static AttackAudio fromJson(const QJsonObject &);
    QByteArray build(quint32 romOffset = 0) const;
};
} // namespace lordly
