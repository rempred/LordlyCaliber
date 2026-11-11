#pragma once
#include "engine/attack_audio.h"
namespace lordly {
constexpr int soundPartStateSize = 0x14c;
void validateSoundParts(const AttackSound &);
int soundImportBytes(const AttackSound &);
QByteArray buildSoundSource(const AttackSound &, quint32 romOffset = 0);
QByteArray soundPartState(const QJsonObject &, const char *field);
AttackSound isolateSoundPart(const Rom &, const AttackSound &, int index);
void replaceSoundPart(const Rom &, AttackSound &, int index, const AttackSound &source);
void setSoundPartTime(const Rom &, AttackSound &, int index, int tick);
void muteSoundPart(const Rom &, AttackSound &, int index);
}
